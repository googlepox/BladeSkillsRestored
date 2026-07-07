#pragma once

namespace BladeSeparationShared
{
	static constexpr unsigned int kLongSkillId = 1101;
	static constexpr unsigned int kShortSkillId = 1102;
	static constexpr unsigned int kAxeSkillId = 1103;

	static constexpr const char* kLongSkillName = "Long";
	static constexpr const char* kShortSkillName = "Short";
	static constexpr const char* kAxeSkillName = "Axe";

	enum NativeWeaponType
	{
		kWeaponType_BladeOneHand = 0,
		kWeaponType_BladeTwoHand = 1,
		kWeaponType_BluntOneHand = 2,
		kWeaponType_BluntTwoHand = 3,
		kWeaponType_Staff = 4,
		kWeaponType_Bow = 5
	};

	enum WeaponSkillKind
	{
		kWeaponSkill_None = 0,
		kWeaponSkill_Long,
		kWeaponSkill_Short,
		kWeaponSkill_Axe
	};

	struct WeaponTypeEntry
	{
		unsigned int skillId;
		const char* name;
	};

	static constexpr WeaponTypeEntry kSeparatedWeaponTypes[] =
	{
		{ kLongSkillId, kLongSkillName },
		{ kShortSkillId, kShortSkillName },
		{ kAxeSkillId, kAxeSkillName },
	};

	inline char AsciiLower(char value)
	{
		return value >= 'A' && value <= 'Z' ?
			static_cast<char>(value - 'A' + 'a') :
			value;
	}

	inline bool ContainsTokenInsensitive(const char* text, const char* token)
	{
		if (!text || !token || !token[0])
			return false;

		for (const char* start = text; *start; ++start)
		{
			const char* current = start;
			const char* expected = token;
			while (*current && *expected && AsciiLower(*current) == AsciiLower(*expected))
			{
				++current;
				++expected;
			}

			if (!*expected)
				return true;
		}

		return false;
	}

	inline bool HasShortBladeToken(const char* text)
	{
		return ContainsTokenInsensitive(text, "dagger") ||
			ContainsTokenInsensitive(text, "shortsword") ||
			ContainsTokenInsensitive(text, "short sword") ||
			ContainsTokenInsensitive(text, "knife") ||
			ContainsTokenInsensitive(text, "tanto") ||
			ContainsTokenInsensitive(text, "wakizashi");
	}

	inline bool HasAxeToken(const char* text)
	{
		return ContainsTokenInsensitive(text, "axe") ||
			ContainsTokenInsensitive(text, "hatchet");
	}

	inline WeaponSkillKind ClassifyWeapon(unsigned int nativeWeaponType, const char* editorId, const char* displayName)
	{
		// Oblivion's decoded WEAP type has one-handed/two-handed blade and blunt buckets.
		// Short blades and axes are sidecar splits derived from that bucket plus stable name tokens.
		switch (nativeWeaponType)
		{
			case kWeaponType_BladeTwoHand:
				return kWeaponSkill_Long;

			case kWeaponType_BladeOneHand:
				if (HasShortBladeToken(editorId) || HasShortBladeToken(displayName))
					return kWeaponSkill_Short;
				return kWeaponSkill_Long;

			case kWeaponType_BluntOneHand:
			case kWeaponType_BluntTwoHand:
				if (HasAxeToken(editorId) || HasAxeToken(displayName))
					return kWeaponSkill_Axe;
				return kWeaponSkill_None;

			default:
				return kWeaponSkill_None;
		}
	}

	inline unsigned int SkillIdForKind(WeaponSkillKind kind)
	{
		switch (kind)
		{
			case kWeaponSkill_Long:
				return kLongSkillId;
			case kWeaponSkill_Short:
				return kShortSkillId;
			case kWeaponSkill_Axe:
				return kAxeSkillId;
			default:
				return 0xFFFFFFFF;
		}
	}

	inline const char* NameForKind(WeaponSkillKind kind)
	{
		switch (kind)
		{
			case kWeaponSkill_Long:
				return kLongSkillName;
			case kWeaponSkill_Short:
				return kShortSkillName;
			case kWeaponSkill_Axe:
				return kAxeSkillName;
			default:
				return "";
		}
	}
}
