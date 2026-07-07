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
	struct NpcSkillSidecarEntry
	{
		unsigned int formId;
		unsigned int skillId;
		unsigned int level;
		float progress;
		unsigned int levelUps;
	};

	class NpcSkillSidecarStore
	{
	public:
		NpcSkillSidecarStore() :
			m_count(0),
			m_dirty(false)
		{
			std::memset(m_entries, 0, sizeof(m_entries));
		}

		bool TryGet(unsigned int formId, unsigned int skillId, NpcSkillSidecarEntry* outEntry) const
		{
			const int index = FindIndex(formId, skillId);
			if (index < 0)
				return false;

			if (outEntry)
				*outEntry = m_entries[index];
			return true;
		}

		bool Set(unsigned int formId, const NpcSkillSidecarEntry& entry)
		{
			return SetInternal(formId, entry.skillId, entry.level, entry.progress, entry.levelUps, true);
		}

		bool SetLevel(unsigned int formId, unsigned int skillId, unsigned int level)
		{
			NpcSkillSidecarEntry existing = {};
			if (TryGet(formId, skillId, &existing))
				return SetInternal(formId, skillId, level, existing.progress, existing.levelUps, true);

			return SetInternal(formId, skillId, level, 0.0f, 0, true);
		}

		bool SetLoaded(unsigned int formId, unsigned int skillId, unsigned int level, float progress, unsigned int levelUps)
		{
			return SetInternal(formId, skillId, level, progress, levelUps, false);
		}

		bool Remove(unsigned int formId, unsigned int skillId)
		{
			const int index = FindIndex(formId, skillId);
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

		const NpcSkillSidecarEntry& EntryAt(unsigned int index) const
		{
			return m_entries[index];
		}

		static unsigned int ClampLevel(unsigned int level)
		{
			return level > 100 ? 100 : level;
		}

	private:
		static constexpr unsigned int kMaxEntries = 4096;

		static bool IsSupportedSkill(unsigned int skillId)
		{
			return skillId == kLongSkillId || skillId == kShortSkillId || skillId == kAxeSkillId;
		}

		int FindIndex(unsigned int formId, unsigned int skillId) const
		{
			if (!formId || !IsSupportedSkill(skillId))
				return -1;

			for (unsigned int i = 0; i < m_count; ++i)
			{
				if (m_entries[i].formId == formId && m_entries[i].skillId == skillId)
					return static_cast<int>(i);
			}
			return -1;
		}

		bool SetInternal(unsigned int formId, unsigned int skillId, unsigned int level,
			float progress, unsigned int levelUps, bool markDirty)
		{
			if (!formId || !IsSupportedSkill(skillId))
				return false;

			NpcSkillSidecarEntry entry = {};
			entry.formId = formId;
			entry.skillId = skillId;
			entry.level = ClampLevel(level);
			entry.progress = progress < 0.0f ? 0.0f : progress;
			entry.levelUps = levelUps;

			const int index = FindIndex(formId, skillId);
			if (index >= 0)
			{
				m_entries[index] = entry;
				if (markDirty)
					m_dirty = true;
				return true;
			}

			if (m_count >= kMaxEntries)
				return false;

			m_entries[m_count++] = entry;
			if (markDirty)
				m_dirty = true;
			return true;
		}

		unsigned int m_count;
		bool m_dirty;
		NpcSkillSidecarEntry m_entries[kMaxEntries];
	};

	struct NpcSkillSidecarKey
	{
		char sourceMod[260];
		unsigned int objectId;
		char editorId[128];
	};

	struct NpcSkillSidecarPayloadRow
	{
		NpcSkillSidecarKey key;
		unsigned int skillId;
		unsigned int level;
		float progress;
		unsigned int levelUps;
	};

	struct NpcSkillSidecarPayloadStats
	{
		unsigned int carrierRecords;
		unsigned int parsedEntries;
		unsigned int resolvedEntries;
		unsigned int unresolvedEntries;
		unsigned int skippedRows;
	};

	class NpcSkillSidecarPayloadCodec
	{
	public:
		typedef void (*LogCallback)(void* context, const char* message);

		static const char* Header()
		{
			return "BSEP_NPC_SKILLS_V1";
		}

		static bool LooksLikePayload(const char* payload)
		{
			if (!payload)
				return false;

			while (*payload == '\r' || *payload == '\n' || *payload == ' ' || *payload == '\t')
				++payload;

			return std::strncmp(payload, Header(), std::strlen(Header())) == 0;
		}

		static bool Parse(const char* payload, std::vector<NpcSkillSidecarPayloadRow>& rows, NpcSkillSidecarPayloadStats* stats, LogCallback log, void* logContext)
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

					Log(log, logContext, "embedded NPC skill sidecar payload malformed header at line %u", lineNumber);
					if (stats)
						++stats->skippedRows;
					return false;
				}

				NpcSkillSidecarPayloadRow row = {};
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
					Log(log, logContext, "embedded NPC skill sidecar payload skipped malformed row at line %u", lineNumber);
				}
			}

			return sawHeader;
		}

		static std::string Write(const std::vector<NpcSkillSidecarPayloadRow>& inputRows)
		{
			std::vector<NpcSkillSidecarPayloadRow> rows;
			rows.reserve(inputRows.size());
			for (size_t i = 0; i < inputRows.size(); ++i)
			{
				if (IsSupportedSkill(inputRows[i].skillId) && inputRows[i].key.objectId != 0)
					rows.push_back(inputRows[i]);
			}

			std::sort(rows.begin(), rows.end(), CompareRows);

			std::string output;
			output += Header();
			output += "\r\n\r\n# sourceMod|objectIdHex|skill|level|progress|levelUps|editorIdEscaped\r\n";
			output += "# skill values: Long, Short, Axe. NPC native skill arrays remain vanilla.\r\n\r\n";
			for (size_t i = 0; i < rows.size(); ++i)
			{
				const std::string escapedEditorId = Escape(rows[i].key.editorId);
				char line[512] = {};
				_snprintf_s(line, sizeof(line), _TRUNCATE, "%s|%06X|%s|%u|%.6g|%u|%s\r\n",
					rows[i].key.sourceMod[0] ? rows[i].key.sourceMod : "$SELF",
					rows[i].key.objectId & 0x00FFFFFF,
					PayloadNameForSkillId(rows[i].skillId),
					NpcSkillSidecarStore::ClampLevel(rows[i].level),
					rows[i].progress < 0.0f ? 0.0f : rows[i].progress,
					rows[i].levelUps,
					escapedEditorId.c_str());
				output += line;
			}

			return output;
		}

		static const char* PayloadNameForSkillId(unsigned int skillId)
		{
			switch (skillId)
			{
				case kLongSkillId:
					return "Long";
				case kShortSkillId:
					return "Short";
				case kAxeSkillId:
					return "Axe";
				default:
					return "";
			}
		}

		static unsigned int SkillIdFromPayloadName(const char* value)
		{
			if (!value || !value[0])
				return 0;
			if (!CompareInsensitive(value, "Long") || !CompareInsensitive(value, "Long Blade") ||
				!CompareInsensitive(value, "BSLong") || !CompareInsensitive(value, "1101"))
				return kLongSkillId;
			if (!CompareInsensitive(value, "Short") || !CompareInsensitive(value, "Short Blade") ||
				!CompareInsensitive(value, "BSShort") || !CompareInsensitive(value, "1102"))
				return kShortSkillId;
			if (!CompareInsensitive(value, "Axe") || !CompareInsensitive(value, "BSAxe") ||
				!CompareInsensitive(value, "1103"))
				return kAxeSkillId;
			return 0;
		}

	private:
		static bool IsSupportedSkill(unsigned int skillId)
		{
			return skillId == kLongSkillId || skillId == kShortSkillId || skillId == kAxeSkillId;
		}

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

		static bool ParseUnsigned(const std::string& value, unsigned int radix, unsigned int* result)
		{
			if (!result || value.empty())
				return false;

			char* end = nullptr;
			const unsigned long parsed = std::strtoul(value.c_str(), &end, radix);
			if (!end || *end)
				return false;

			*result = static_cast<unsigned int>(parsed);
			return true;
		}

		static bool ParseFloat(const std::string& value, float* result)
		{
			if (!result || value.empty())
				return false;

			char* end = nullptr;
			const double parsed = std::strtod(value.c_str(), &end);
			if (!end || *end)
				return false;

			*result = parsed < 0.0 ? 0.0f : static_cast<float>(parsed);
			return true;
		}

		static bool ParseRow(const std::string& line, NpcSkillSidecarPayloadRow& row)
		{
			std::vector<std::string> parts;
			SplitFields(line, parts);
			if (parts.size() != 7)
				return false;

			if (parts[0].empty() || parts[1].empty() || parts[2].empty() || parts[3].empty() ||
				parts[4].empty() || parts[5].empty())
				return false;

			unsigned int objectId = 0;
			if (!ParseUnsigned(parts[1], 16, &objectId) || objectId > 0x00FFFFFF)
				return false;

			const unsigned int skillId = SkillIdFromPayloadName(parts[2].c_str());
			if (!IsSupportedSkill(skillId))
				return false;

			unsigned int level = 0;
			unsigned int levelUps = 0;
			float progress = 0.0f;
			if (!ParseUnsigned(parts[3], 10, &level) ||
				!ParseFloat(parts[4], &progress) ||
				!ParseUnsigned(parts[5], 10, &levelUps))
			{
				return false;
			}

			const std::string editorId = Unescape(parts[6]);
			std::memset(&row, 0, sizeof(row));
			_snprintf_s(row.key.sourceMod, sizeof(row.key.sourceMod), _TRUNCATE, "%s", parts[0].c_str());
			row.key.objectId = objectId & 0x00FFFFFF;
			row.skillId = skillId;
			row.level = NpcSkillSidecarStore::ClampLevel(level);
			row.progress = progress;
			row.levelUps = levelUps;
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

		static bool CompareRows(const NpcSkillSidecarPayloadRow& left, const NpcSkillSidecarPayloadRow& right)
		{
			const int sourceCompare = CompareInsensitive(left.key.sourceMod, right.key.sourceMod);
			if (sourceCompare != 0)
				return sourceCompare < 0;
			if (left.key.objectId != right.key.objectId)
				return left.key.objectId < right.key.objectId;
			if (left.skillId != right.skillId)
				return left.skillId < right.skillId;
			return std::strcmp(left.key.editorId, right.key.editorId) < 0;
		}
	};

	struct NpcTrainingSidecarEntry
	{
		unsigned int formId;
		unsigned int skillId;
	};

	class NpcTrainingSidecarStore
	{
	public:
		NpcTrainingSidecarStore() :
			m_count(0),
			m_dirty(false)
		{
			std::memset(m_entries, 0, sizeof(m_entries));
		}

		bool Has(unsigned int formId) const
		{
			return FindIndex(formId) >= 0;
		}

		bool TryGet(unsigned int formId, NpcTrainingSidecarEntry* outEntry) const
		{
			const int index = FindIndex(formId);
			if (index < 0)
				return false;

			if (outEntry)
				*outEntry = m_entries[index];
			return true;
		}

		bool Set(unsigned int formId, unsigned int skillId)
		{
			return SetInternal(formId, skillId, true);
		}

		bool SetLoaded(unsigned int formId, unsigned int skillId)
		{
			return SetInternal(formId, skillId, false);
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

		const NpcTrainingSidecarEntry& EntryAt(unsigned int index) const
		{
			return m_entries[index];
		}

	private:
		static constexpr unsigned int kMaxEntries = 4096;

		static bool IsSupportedSkill(unsigned int skillId)
		{
			return skillId == kLongSkillId || skillId == kShortSkillId || skillId == kAxeSkillId;
		}

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

		bool SetInternal(unsigned int formId, unsigned int skillId, bool markDirty)
		{
			if (!formId || !IsSupportedSkill(skillId))
				return false;

			const int index = FindIndex(formId);
			if (index >= 0)
			{
				m_entries[index].skillId = skillId;
				if (markDirty)
					m_dirty = true;
				return true;
			}

			if (m_count >= kMaxEntries)
				return false;

			m_entries[m_count].formId = formId;
			m_entries[m_count].skillId = skillId;
			++m_count;
			if (markDirty)
				m_dirty = true;
			return true;
		}

		unsigned int m_count;
		bool m_dirty;
		NpcTrainingSidecarEntry m_entries[kMaxEntries];
	};

	struct NpcTrainingSidecarPayloadRow
	{
		NpcSkillSidecarKey key;
		unsigned int skillId;
	};

	class NpcTrainingSidecarPayloadCodec
	{
	public:
		typedef void (*LogCallback)(void* context, const char* message);

		static const char* Header()
		{
			return "BSEP_NPC_TRAINING_V1";
		}

		static const char* Signature()
		{
			return "BSEP_NPC_TRAINING";
		}

		static unsigned int Version()
		{
			return 1;
		}

		static bool LooksLikePayload(const char* payload)
		{
			if (!payload)
				return false;

			while (*payload == '\r' || *payload == '\n' || *payload == ' ' || *payload == '\t')
				++payload;

			return std::strncmp(payload, Header(), std::strlen(Header())) == 0;
		}

		static bool Parse(const char* payload, std::vector<NpcTrainingSidecarPayloadRow>& rows, NpcSkillSidecarPayloadStats* stats, LogCallback log, void* logContext)
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

					Log(log, logContext, "embedded NPC training sidecar payload malformed header at line %u", lineNumber);
					if (stats)
						++stats->skippedRows;
					return false;
				}

				NpcTrainingSidecarPayloadRow row = {};
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
					Log(log, logContext, "embedded NPC training sidecar payload skipped malformed row at line %u", lineNumber);
				}
			}

			return sawHeader;
		}

		static std::string Write(const std::vector<NpcTrainingSidecarPayloadRow>& inputRows)
		{
			std::vector<NpcTrainingSidecarPayloadRow> rows;
			rows.reserve(inputRows.size());
			for (size_t i = 0; i < inputRows.size(); ++i)
			{
				if (IsSupportedSkill(inputRows[i].skillId) && inputRows[i].key.objectId != 0)
					rows.push_back(inputRows[i]);
			}

			std::sort(rows.begin(), rows.end(), CompareRows);

			std::string output;
			output += Header();
			output += "\r\n\r\n# Format: sourceMod|objectIdHex|trainingSkill|editorIdEscaped\r\n";
			output += "# trainingSkill values: Long Blade, Short Blade, Axe. Native TESAIForm::trainingSkill remains a vanilla fallback.\r\n\r\n";
			for (size_t i = 0; i < rows.size(); ++i)
			{
				const std::string escapedEditorId = Escape(rows[i].key.editorId);
				char line[512] = {};
				_snprintf_s(line, sizeof(line), _TRUNCATE, "%s|%06X|%s|%s\r\n",
					rows[i].key.sourceMod[0] ? rows[i].key.sourceMod : "$SELF",
					rows[i].key.objectId & 0x00FFFFFF,
					TrainingNameForSkillId(rows[i].skillId),
					escapedEditorId.c_str());
				output += line;
			}

			return output;
		}

		static const char* TrainingNameForSkillId(unsigned int skillId)
		{
			switch (skillId)
			{
				case kLongSkillId:
					return "Long Blade";
				case kShortSkillId:
					return "Short Blade";
				case kAxeSkillId:
					return "Axe";
				default:
					return "";
			}
		}

	private:
		static bool IsSupportedSkill(unsigned int skillId)
		{
			return skillId == kLongSkillId || skillId == kShortSkillId || skillId == kAxeSkillId;
		}

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

		static bool ParseRow(const std::string& line, NpcTrainingSidecarPayloadRow& row)
		{
			std::vector<std::string> parts;
			SplitFields(line, parts);
			if (parts.size() != 4)
				return false;

			if (parts[0].empty() || parts[1].empty() || parts[2].empty())
				return false;

			char* objectEnd = nullptr;
			const unsigned long parsedObjectId = std::strtoul(parts[1].c_str(), &objectEnd, 16);
			if (!objectEnd || *objectEnd || parsedObjectId > 0x00FFFFFF)
				return false;

			const unsigned int skillId = NpcSkillSidecarPayloadCodec::SkillIdFromPayloadName(parts[2].c_str());
			if (!IsSupportedSkill(skillId))
				return false;

			const std::string editorId = Unescape(parts[3]);
			std::memset(&row, 0, sizeof(row));
			_snprintf_s(row.key.sourceMod, sizeof(row.key.sourceMod), _TRUNCATE, "%s", parts[0].c_str());
			row.key.objectId = static_cast<unsigned int>(parsedObjectId) & 0x00FFFFFF;
			row.skillId = skillId;
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

		static bool CompareRows(const NpcTrainingSidecarPayloadRow& left, const NpcTrainingSidecarPayloadRow& right)
		{
			const int sourceCompare = CompareInsensitive(left.key.sourceMod, right.key.sourceMod);
			if (sourceCompare != 0)
				return sourceCompare < 0;
			if (left.key.objectId != right.key.objectId)
				return left.key.objectId < right.key.objectId;
			if (left.skillId != right.skillId)
				return left.skillId < right.skillId;
			return std::strcmp(left.key.editorId, right.key.editorId) < 0;
		}
	};
}
