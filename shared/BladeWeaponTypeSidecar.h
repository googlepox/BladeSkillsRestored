#pragma once

#include "BladeSeparation.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace BladeSeparationShared
{
	struct WeaponTypeSidecarEntry
	{
		unsigned int formId;
		WeaponSkillKind kind;
	};

	class WeaponTypeSidecarStore
	{
	public:
		WeaponTypeSidecarStore() :
			m_count(0),
			m_dirty(false)
		{
			std::memset(m_entries, 0, sizeof(m_entries));
		}

		bool TryGet(unsigned int formId, WeaponSkillKind* outKind) const
		{
			const int index = FindIndex(formId);
			if (index < 0)
				return false;

			if (outKind)
				*outKind = m_entries[index].kind;
			return true;
		}

		bool Set(unsigned int formId, WeaponSkillKind kind)
		{
			return SetInternal(formId, kind, true);
		}

		bool SetLoaded(unsigned int formId, WeaponSkillKind kind)
		{
			return SetInternal(formId, kind, false);
		}

		bool Remove(unsigned int formId)
		{
			const int index = FindIndex(formId);
			if (index < 0)
				return false;

			const unsigned int last = m_count - 1;
			if (static_cast<unsigned int>(index) != last)
				m_entries[index] = m_entries[last];

			std::memset(&m_entries[last], 0, sizeof(m_entries[last]));
			--m_count;
			m_dirty = true;
			return true;
		}

		void Clear()
		{
			std::memset(m_entries, 0, sizeof(m_entries));
			m_count = 0;
			m_dirty = false;
		}

		void ClearDirty()
		{
			m_dirty = false;
		}

		bool IsDirty() const
		{
			return m_dirty;
		}

		unsigned int Count() const
		{
			return m_count;
		}

		const WeaponTypeSidecarEntry& EntryAt(unsigned int index) const
		{
			return m_entries[index];
		}

	private:
		static constexpr unsigned int kMaxEntries = 4096;

		int FindIndex(unsigned int formId) const
		{
			if (!formId)
				return -1;

			for (unsigned int i = 0; i < m_count; ++i)
			{
				if (m_entries[i].formId == formId)
					return static_cast<int>(i);
			}
			return -1;
		}

		bool SetInternal(unsigned int formId, WeaponSkillKind kind, bool markDirty)
		{
			if (!formId || kind == kWeaponSkill_None)
				return false;

			const int index = FindIndex(formId);
			if (index >= 0)
			{
				if (m_entries[index].kind == kind)
					return true;

				m_entries[index].kind = kind;
				if (markDirty)
					m_dirty = true;
				return true;
			}

			if (m_count >= kMaxEntries)
				return false;

			m_entries[m_count].formId = formId;
			m_entries[m_count].kind = kind;
			++m_count;
			if (markDirty)
				m_dirty = true;
			return true;
		}

		unsigned int m_count;
		bool m_dirty;
		WeaponTypeSidecarEntry m_entries[kMaxEntries];
	};

	struct WeaponTypeSidecarKey
	{
		char sourceMod[260];
		unsigned int objectId;
		char editorId[128];
	};

	struct WeaponTypeSidecarPayloadRow
	{
		WeaponTypeSidecarKey key;
		WeaponSkillKind kind;
	};

	struct WeaponTypeSidecarPayloadStats
	{
		unsigned int carrierRecords;
		unsigned int parsedEntries;
		unsigned int resolvedEntries;
		unsigned int unresolvedEntries;
		unsigned int skippedRows;
	};

	class WeaponTypeSidecarPayloadCodec
	{
	public:
		typedef void (*LogCallback)(void* context, const char* message);

		static const char* Header()
		{
			return "BSEP_WEAPON_TYPES_V1";
		}

		static bool LooksLikePayload(const char* payload)
		{
			if (!payload)
				return false;

			while (*payload == '\r' || *payload == '\n' || *payload == ' ' || *payload == '\t')
				++payload;

			return std::strncmp(payload, Header(), std::strlen(Header())) == 0;
		}

		static bool Parse(const char* payload, std::vector<WeaponTypeSidecarPayloadRow>& rows, WeaponTypeSidecarPayloadStats* stats, LogCallback log, void* logContext)
		{
			rows.clear();
			if (stats)
				std::memset(stats, 0, sizeof(*stats));
			if (!payload || !LooksLikePayload(payload))
				return false;

			const std::string text(payload);
			size_t offset = 0;
			unsigned int lineNumber = 0;
			bool sawHeader = false;
			while (offset <= text.size())
			{
				const size_t end = text.find_first_of("\r\n", offset);
				std::string line = end == std::string::npos ? text.substr(offset) : text.substr(offset, end - offset);
				offset = end == std::string::npos ? text.size() + 1 : end + 1;
				if (end != std::string::npos && offset < text.size() && text[end] == '\r' && text[offset] == '\n')
					++offset;

				++lineNumber;
				Trim(line);
				if (line.empty() || line[0] == '#')
					continue;

				if (!sawHeader)
				{
					if (line == Header())
					{
						sawHeader = true;
						continue;
					}

					Log(log, logContext, "embedded weapon Type sidecar payload malformed header at line %u", lineNumber);
					if (stats)
						++stats->skippedRows;
					return false;
				}

				WeaponTypeSidecarPayloadRow row = {};
				if (ParseRow(line, row))
				{
					rows.push_back(row);
					if (stats)
						++stats->parsedEntries;
				}
				else
				{
					if (stats)
						++stats->skippedRows;
					Log(log, logContext, "embedded weapon Type sidecar payload skipped malformed row at line %u", lineNumber);
				}
			}

			return sawHeader;
		}

		static std::string Write(const std::vector<WeaponTypeSidecarPayloadRow>& inputRows)
		{
			std::vector<WeaponTypeSidecarPayloadRow> rows;
			rows.reserve(inputRows.size());
			for (size_t i = 0; i < inputRows.size(); ++i)
			{
				if (inputRows[i].kind != kWeaponSkill_None && inputRows[i].key.objectId != 0)
					rows.push_back(inputRows[i]);
			}

			std::sort(rows.begin(), rows.end(), CompareRows);

			std::string output;
			output += Header();
			output += "\r\n\r\n# sourceMod|objectIdHex|weaponType|editorIdEscaped\r\n";
			output += "# weaponType values: Long, Short, Axe. Native WEAP type remains a vanilla fallback.\r\n\r\n";
			for (size_t i = 0; i < rows.size(); ++i)
			{
				const std::string escapedEditorId = Escape(rows[i].key.editorId);
				char line[512] = {};
				_snprintf_s(line, sizeof(line), _TRUNCATE, "%s|%06X|%s|%s\r\n",
					rows[i].key.sourceMod[0] ? rows[i].key.sourceMod : "$SELF",
					rows[i].key.objectId & 0x00FFFFFF,
					PayloadNameForKind(rows[i].kind),
					escapedEditorId.c_str());
				output += line;
			}

			return output;
		}

		static const char* PayloadNameForKind(WeaponSkillKind kind)
		{
			switch (kind)
			{
				case kWeaponSkill_Long:
					return "Long";
				case kWeaponSkill_Short:
					return "Short";
				case kWeaponSkill_Axe:
					return "Axe";
				default:
					return "";
			}
		}

		static WeaponSkillKind KindFromPayloadName(const char* value)
		{
			if (!value || !value[0])
				return kWeaponSkill_None;
			if (!CompareInsensitive(value, "1") || !CompareInsensitive(value, "Long") || !CompareInsensitive(value, "Long Blade"))
				return kWeaponSkill_Long;
			if (!CompareInsensitive(value, "2") || !CompareInsensitive(value, "Short") || !CompareInsensitive(value, "Short Blade"))
				return kWeaponSkill_Short;
			if (!CompareInsensitive(value, "3") || !CompareInsensitive(value, "Axe"))
				return kWeaponSkill_Axe;
			return kWeaponSkill_None;
		}

	private:
		static void Log(LogCallback log, void* context, const char* format, ...)
		{
			if (!log || !format)
				return;

			char buffer[512] = {};
			va_list args;
			va_start(args, format);
			_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
			va_end(args);
			log(context, buffer);
		}

		static char Lower(char value)
		{
			return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
		}

		static int CompareInsensitive(const char* left, const char* right)
		{
			if (!left)
				left = "";
			if (!right)
				right = "";

			while (*left && *right)
			{
				const char l = Lower(*left);
				const char r = Lower(*right);
				if (l != r)
					return static_cast<unsigned char>(l) - static_cast<unsigned char>(r);
				++left;
				++right;
			}
			return static_cast<unsigned char>(Lower(*left)) - static_cast<unsigned char>(Lower(*right));
		}

		static void Trim(std::string& value)
		{
			const char* whitespace = " \t\r\n";
			const size_t first = value.find_first_not_of(whitespace);
			if (first == std::string::npos)
			{
				value.clear();
				return;
			}

			const size_t last = value.find_last_not_of(whitespace);
			value = value.substr(first, last - first + 1);
		}

		static bool ParseRow(const std::string& line, WeaponTypeSidecarPayloadRow& row)
		{
			std::vector<std::string> parts;
			SplitFields(line, parts);
			if (parts.size() != 4)
				return false;

			if (parts[0].empty() || parts[1].empty() || parts[2].empty())
				return false;

			char* objectEnd = nullptr;
			const unsigned int objectId = std::strtoul(parts[1].c_str(), &objectEnd, 16);
			if (!objectEnd || *objectEnd || objectId > 0x00FFFFFF)
				return false;

			const WeaponSkillKind kind = KindFromPayloadName(parts[2].c_str());
			if (kind == kWeaponSkill_None)
				return false;

			const std::string editorId = Unescape(parts[3]);
			std::memset(&row, 0, sizeof(row));
			_snprintf_s(row.key.sourceMod, sizeof(row.key.sourceMod), _TRUNCATE, "%s", parts[0].c_str());
			row.key.objectId = objectId & 0x00FFFFFF;
			row.kind = kind;
			_snprintf_s(row.key.editorId, sizeof(row.key.editorId), _TRUNCATE, "%s", editorId.c_str());
			return true;
		}

		static void SplitFields(const std::string& line, std::vector<std::string>& parts)
		{
			parts.clear();
			size_t start = 0;
			while (start <= line.size())
			{
				const size_t end = line.find('|', start);
				parts.push_back(end == std::string::npos ? line.substr(start) : line.substr(start, end - start));
				if (end == std::string::npos)
					break;
				start = end + 1;
			}
		}

		static std::string Escape(const char* input)
		{
			std::string output;
			if (!input)
				return output;

			for (const char* cursor = input; *cursor; ++cursor)
			{
				switch (*cursor)
				{
					case '\\': output += "\\\\"; break;
					case '|': output += "\\p"; break;
					case '\r': output += "\\r"; break;
					case '\n': output += "\\n"; break;
					default: output += *cursor; break;
				}
			}
			return output;
		}

		static std::string Unescape(const std::string& input)
		{
			std::string output;
			for (size_t i = 0; i < input.size(); ++i)
			{
				if (input[i] != '\\' || i + 1 >= input.size())
				{
					output += input[i];
					continue;
				}

				const char escaped = input[++i];
				switch (escaped)
				{
					case '\\': output += '\\'; break;
					case 'p': output += '|'; break;
					case 'r': output += '\r'; break;
					case 'n': output += '\n'; break;
					default: output += escaped; break;
				}
			}
			return output;
		}

		static bool CompareRows(const WeaponTypeSidecarPayloadRow& left, const WeaponTypeSidecarPayloadRow& right)
		{
			const int sourceCompare = CompareInsensitive(left.key.sourceMod, right.key.sourceMod);
			if (sourceCompare != 0)
				return sourceCompare < 0;
			if (left.key.objectId != right.key.objectId)
				return left.key.objectId < right.key.objectId;
			if (left.kind != right.kind)
				return left.kind < right.kind;
			return std::strcmp(left.key.editorId, right.key.editorId) < 0;
		}
	};
}
