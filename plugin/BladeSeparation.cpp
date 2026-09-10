#include "obse/PluginAPI.h"
#include "obse/GameAPI.h"
#include "obse/GameActorValues.h"
#include "obse/GameData.h"
#include "obse/GameForms.h"
#include "obse/GameObjects.h"
#include "obse/GameProcess.h"
#include "obse/GameTiles.h"
#include "obse_common/SafeWrite.h"
#include "..\shared\BladeSeparation.h"
#include "..\shared\BladeWeaponTypeSidecar.h"
#include "TrueCustomSkills/TrueCustomSkillsInterface.h"

#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <intrin.h>
#include <vector>
#include <windows.h>

#pragma intrinsic(_ReturnAddress)

IDebugLog gLog("BladeSkillsRestored.log");

PluginHandle g_pluginHandle = kPluginHandle_Invalid;
OBSESerializationInterface* g_serialization = nullptr;
static OBSEMessagingInterface* g_messaging = nullptr;
static TrueCustomSkillsInterface* g_tcs = nullptr;

namespace BladeSeparation
{
	static constexpr UInt32 kPluginVersion = 1;
	static constexpr UInt32 kRecordWeaponTypes = ('B') | ('S' << 8) | ('W' << 16) | ('T' << 24);
	static constexpr UInt32 kWeaponTypesRecordVersion = 1;
	static constexpr UInt32 kMaxSavedWeaponTypeEntries = 4096;
	static constexpr UInt32 kMaxSkillLevel = 100;
	static constexpr UInt32 kSkillCount = 2;
	static constexpr UInt32 kShortSkillIndex = 0;
	static constexpr UInt32 kAxeSkillIndex = 1;
	static constexpr UInt32 kWeaponSuccessfulHitUseType = 0;
	static constexpr float kProgressEpsilon = 0.0001f;
	static constexpr const char* kLongSkillDisplayName = "Long Blade";
	static constexpr const char* kShortSkillDisplayName = "Short Blade";
	static constexpr const char* kAxeSkillDisplayName = "Axe";

	static constexpr UInt32 kPlayerModExperience = 0x668C30;
	static constexpr UInt32 kPlayerModExperiencePatchLength = 7;
	static constexpr UInt32 kTileSetFloat = 0x0058CEB0;
	static constexpr UInt32 kTileSetString = 0x0058CED0;
	static constexpr UInt32 kTileGetFloat = 0x00588BD0;
	static constexpr UInt32 kMenuGetOpenMenuTile = 0x00589B70;
	static constexpr UInt32 kTileGetParentMenu = 0x005898F0;
	static constexpr UInt32 kActorValueGetName = 0x00565CC0;
	static constexpr UInt32 kActorValueGetNamePatchLength = 7;
	static constexpr UInt32 kCalcMasteryFromSkill = 0x0056A300;
	static constexpr UInt32 kActorGetBaseCalcAVi = 0x005F1910;
	static constexpr UInt32 kActorGetSkillMasteryLevel = 0x005F23B0;
	static constexpr UInt32 kOpenSkillPerkMenu = 0x0057B370;
	static constexpr UInt32 kPlayerMaybeStartNextAttributeBonusBucket = 0x0065FB30;
	static constexpr UInt32 kPlayerIncrementAttributeBonusBucket = 0x006648D0;
	static constexpr UInt32 kTESObjectREFRGetAnimData = 0x004D8370;
	static constexpr UInt32 kActorAnimDataGetAnimGroupFromField8Value = 0x00470720;
	static constexpr UInt32 kActorAnimDataRemovePowerAttackGroups = 0x00471990;
	static constexpr UInt32 kObservedActorAnimDataBuildPowerAttackKFList = 0x00476410;
	static constexpr UInt32 kAnimKeyGetGroupID = 0x0051AA00;
	static constexpr UInt32 kActorPlayStaggerAnimGroup = 0x005F4F00;
	static constexpr UInt32 kActorAttemptAttackDisarmPerkOnHit = 0x005FC090;
	static constexpr UInt32 kGameRandomLargeInteger = 0x0047DF80;
	static constexpr UInt32 kTESObjectWEAPGetWeaponSkillAV = 0x004BB060;
	static constexpr UInt32 kCombatControllerWeaponSkillCall = 0x006137C9;
	static constexpr UInt32 kCombatControllerWeaponSkillContinue = 0x006137D5;
	static constexpr UInt32 kCombatSelectionHandToHandComparePatch = 0x00621F33;
	static constexpr UInt32 kCombatSelectionHandToHandPreferred = 0x00621F54;
	static constexpr UInt32 kCombatSelectionHandToHandCompareContinue = 0x00621F5E;
	static constexpr UInt32 kCalcWeaponDamage = 0x00547070;
	static constexpr UInt32 kCalcWeaponDamageContinue = kCalcWeaponDamage + 0x12;
	static constexpr UInt32 kCalcPowerAttackBonus = 0x00546BA0;
	static constexpr UInt32 kCalcPowerAttackBonusContinue = kCalcPowerAttackBonus + 0x15;
	static constexpr UInt32 kCalcLuckModifiedSkill = 0x00547B90;
	static constexpr UInt32 kMagicPopupWeaponLabelTrait = 0x00000FAE;
	static constexpr UInt32 kMagicPopupEnchantedWeaponLabelSetStringCall = 0x005B43D0;
	static constexpr UInt32 kMagicPopupEnchantedWeaponNullLabelSetStringCall = 0x005B43E5;
	static constexpr UInt32 kMagicPopupSimpleWeaponLabelSetStringCall = 0x005B55CA;
	static constexpr UInt32 kPowerAttackBuilderMasteryCall = 0x00476508;
	static constexpr UInt32 kLongBladeDisarmBaseCalcCall = 0x005FC10D;
	static constexpr UInt32 kPostHitExpertMasteryCall = 0x006002F1;
	static constexpr UInt32 kPostHitMasterMasteryCall = 0x00600343;
	static constexpr UInt32 kAttackDisarmPostHitCall = 0x0060043E;
	static constexpr UInt32 kPostHitExpertMasteryReturn = kPostHitExpertMasteryCall + 5;
	static constexpr UInt32 kAnimGroupBackwardPowerAttack = 0x18;

	static constexpr UInt32 kRetWeaponSkillEquippedDamage = 0x00484FED;
	static constexpr UInt32 kRetWeaponSkillPlayerInventoryRating = 0x00489295;
	static constexpr UInt32 kRetWeaponSkillEquippableRatingA = 0x0048C115;
	static constexpr UInt32 kRetWeaponSkillEquippableRatingB = 0x0048C58A;
	static constexpr UInt32 kRetWeaponSkillActorBaseEquippableRating = 0x0051A1D6;
	static constexpr UInt32 kRetWeaponSkillPowerAttackBonus = 0x005FF4A2;
	static constexpr UInt32 kRetWeaponSkillProjectileDamage = 0x00612598;
	static constexpr UInt32 kRetEquippedWeaponDamage = 0x00485080;
	static constexpr UInt32 kRetPlayerInventoryWeaponRating = 0x00489382;
	static constexpr UInt32 kRetEquippableWeaponRating = 0x0048C7C0;
	static constexpr UInt32 kRetActorBaseEquippableWeaponRating = 0x0051A271;
	static constexpr UInt32 kRetPowerAttackBonus = 0x005FF4E6;
	static constexpr UInt32 kRetWeaponDamageWrapper = 0x005471CC;

	static constexpr UInt32 kStatsMenuMasteryRankCount = 5;
	static constexpr UInt32 kGenericMenuArgInt = 0;
	static constexpr UInt32 kGenericMenuArgFloat = 1;
	static constexpr UInt32 kGenericMenuArgString = 2;
	static constexpr UInt32 kGenericMenuArgEnd = 3;
	static constexpr const char* kSkillPerkMenuXml = "skill_perk.xml";
	static constexpr const char* kSkillPerkOkText = "OK";

	static const UInt8 kPlayerModExperienceExpected[kPlayerModExperiencePatchLength] =
	{
		0x53, 0x8B, 0x5C, 0x24, 0x08, 0x56, 0x57
	};
	static const UInt8 kActorValueGetNameExpected[kActorValueGetNamePatchLength] =
	{
		0x8B, 0x44, 0x24, 0x04,
		0x83, 0xF8, 0x27
	};
	static const UInt8 kTESObjectWEAPGetWeaponSkillAVExpected[] =
	{
		0x0F, 0xBE, 0x81, 0x90, 0x00, 0x00, 0x00,
		0x8B, 0x04, 0x85, 0xA0, 0x86, 0xB0, 0x00,
		0xC3
	};
	static const UInt8 kCombatControllerWeaponSkillExpected[] =
	{
		0xE8, 0x92, 0x78, 0xEA, 0xFF,
		0x50,
		0x8B, 0x07,
		0x8B, 0xCB,
		0xFF, 0xD0
	};
	static const UInt8 kCombatSelectionHandToHandCompareExpected[] =
	{
		0x8B, 0x16,
		0x8B, 0x82, 0x84, 0x02, 0x00, 0x00,
		0x53,
		0x8B, 0xCE,
		0xFF, 0xD0,
		0x8B, 0x16,
		0x8B, 0xD8,
		0x8B, 0x82, 0x84, 0x02, 0x00, 0x00,
		0x6A, 0x11,
		0x8B, 0xCE,
		0xFF, 0xD0,
		0x3B, 0xC3,
		0x7E, 0x0A
	};
	static const UInt8 kCalcWeaponDamageExpected[] =
	{
		0x83, 0xEC, 0x08,
		0x8B, 0x44, 0x24, 0x10,
		0x8B, 0x4C, 0x24, 0x0C,
		0x50,
		0x51,
		0xE8, 0x0E, 0x0B, 0x00, 0x00
	};
	static const UInt8 kCalcPowerAttackBonusExpected[] =
	{
		0x51,
		0x8B, 0x44, 0x24, 0x08,
		0xD9, 0x05, 0x20, 0x6E, 0xB3, 0x00,
		0x50,
		0xD9, 0x5C, 0x24, 0x04,
		0xE8, 0x4B, 0x37, 0x02, 0x00
	};

	struct SkillDefinition
	{
		UInt32 skillId;
		const char* editorId;
		const char* name;
		UInt32 governingAttributeAV;
		UInt32 fallbackActorValue;
	};

	struct SkillUseCondition
	{
		BladeSeparationShared::WeaponSkillKind kind;
		UInt32 nativeActorValue;
		UInt32 useType;
	};

	struct SavedWeaponTypeEntry
	{
		UInt32 formId;
		UInt32 kind;
	};

	struct PendingWeaponSkillConsumer
	{
		bool active;
		bool playerFacing;
		UInt32 sourceReturnAddress;
		TESObjectWEAP* weapon;
		BladeSeparationShared::WeaponSkillKind kind;
	};

	static const SkillDefinition kSkills[kSkillCount] =
	{
		{ BladeSeparationShared::kShortSkillId, "BSShort", kShortSkillDisplayName, kActorVal_Speed, kActorVal_Blade },
		{ BladeSeparationShared::kAxeSkillId, "BSAxe", kAxeSkillDisplayName, kActorVal_Strength, kActorVal_Blunt },
	};

	static const SkillUseCondition kSkillUseConditions[kSkillCount] =
	{
		{ BladeSeparationShared::kWeaponSkill_Short, kActorVal_Blade, kWeaponSuccessfulHitUseType },
		{ BladeSeparationShared::kWeaponSkill_Axe, kActorVal_Blunt, kWeaponSuccessfulHitUseType },
	};

	static BladeSeparationShared::WeaponTypeSidecarStore g_weaponTypeStore;
	static bool g_loggedLegacySelfWeaponTypeFallback = false;
	static bool g_loggedWeaponTypeCarrierStoreFull = false;
	static UInt32 g_appliedPatches = 0;
	static UInt32 g_failedPatches = 0;
	static PendingWeaponSkillConsumer g_pendingWeaponSkillConsumer = {};
	static void* g_playerModExperienceOriginal = nullptr;
	static void* g_actorValueGetNameOriginal = nullptr;
	static void* g_getWeaponSkillAVOriginal = nullptr;
	static void* g_calcWeaponDamageOriginal = nullptr;
	static void* g_calcPowerAttackBonusOriginal = nullptr;
	static UInt32 g_combatControllerWeaponSkillChainTarget = 0;
	static UInt32 g_combatSelectionHandToHandChainTarget = 0;
	static bool g_hooksInstalled = false;
	static bool g_hookInstallAttempted = false;

	using PlayerModExperienceFn = void(__thiscall*)(PlayerCharacter* player, UInt32 actorValue, UInt32 useType, float baseDelta);
	using TileSetFloatFn = void(__thiscall*)(Tile* tile, UInt32 trait, float value);
	using TileSetStringFn = void(__thiscall*)(Tile* tile, UInt32 trait, const char* value);
	using TileGetFloatFn = double(__thiscall*)(Tile* tile, UInt32 trait);
	using TileGetParentMenuFn = void* (__thiscall*)(Tile* tile);
	using MenuGetOpenMenuTileFn = Tile * (__cdecl*)(UInt32 menuType);
	using ActorValueGetNameFn = const char* (__cdecl*)(UInt32 actorValue);
	using CalcMasteryFromSkillFn = UInt32(__cdecl*)(SInt32 skillLevel);
	using ActorGetBaseCalcAViFn = UInt32(__thiscall*)(Actor* actor, UInt32 actorValue);
	using ActorGetSkillMasteryLevelFn = UInt32(__thiscall*)(Actor* actor, UInt32 actorValue);
	using OpenSkillPerkMenuFn = char(__cdecl*)(const char* xml, UInt32 unk1, UInt32 unk2, UInt32 unk3, UInt32 firstArgType, ...);
	using TESObjectREFRGetAnimDataFn = ActorAnimData * (__thiscall*)(TESObjectREFR* refr);
	using ActorAnimDataGetAnimGroupFromField8ValueFn = UInt16(__thiscall*)(ActorAnimData* animData, UInt32 field);
	using ActorAnimDataRemovePowerAttackGroupsFn = void(__thiscall*)(ActorAnimData* animData);
	using ObservedActorAnimDataBuildPowerAttackKFListFn = void(__thiscall*)(ActorAnimData* animData, TESObjectREFR* refr, UInt32 unk);
	using AnimKeyGetGroupIDFn = UInt32(__cdecl*)(UInt32 animKey);
	using ActorPlayStaggerAnimGroupFn = void(__thiscall*)(Actor* actor);
	using ActorAttemptAttackDisarmPerkOnHitFn = UInt8(__thiscall*)(Actor* attacker, Actor* target, UInt32 actorValue);
	using GameRandomLargeIntegerFn = UInt32(__cdecl*)(UInt32 seed);
	using GetWeaponSkillAVFn = UInt32(__thiscall*)(TESObjectWEAP* weapon);
	using CalcWeaponDamageFn = double(__cdecl*)(int weaponSkill, int luck, int strengthOrAgility, float fatigue, int weaponDamage, float condition, float multiplier, float ignoreFatigue);
	using CalcPowerAttackBonusFn = double(__cdecl*)(int skillLevel, int attackType);
	using PlayerMaybeStartNextAttributeBonusBucketFn = void(__thiscall*)(PlayerCharacter* player);
	using PlayerIncrementAttributeBonusBucketFn = UInt32(__thiscall*)(PlayerCharacter* player, UInt32 attributeAV);

	static PlayerCharacter* GetPlayer()
	{
		return g_thePlayer ? *g_thePlayer : nullptr;
	}

	static PlayerModExperienceFn PlayerModExperienceOriginal()
	{
		return reinterpret_cast<PlayerModExperienceFn>(g_playerModExperienceOriginal);
	}

	static TileSetFloatFn TileSetFloat()
	{
		return reinterpret_cast<TileSetFloatFn>(kTileSetFloat);
	}

	static TileSetStringFn TileSetString()
	{
		return reinterpret_cast<TileSetStringFn>(kTileSetString);
	}

	static TileGetFloatFn TileGetFloat()
	{
		return reinterpret_cast<TileGetFloatFn>(kTileGetFloat);
	}

	static TileGetParentMenuFn TileGetParentMenu()
	{
		return reinterpret_cast<TileGetParentMenuFn>(kTileGetParentMenu);
	}

	static MenuGetOpenMenuTileFn MenuGetOpenMenuTile()
	{
		return reinterpret_cast<MenuGetOpenMenuTileFn>(kMenuGetOpenMenuTile);
	}

	static ActorValueGetNameFn ActorValueGetName()
	{
		if (g_actorValueGetNameOriginal)
			return reinterpret_cast<ActorValueGetNameFn>(g_actorValueGetNameOriginal);

		return reinterpret_cast<ActorValueGetNameFn>(kActorValueGetName);
	}

	static CalcMasteryFromSkillFn CalcMasteryFromSkill()
	{
		return reinterpret_cast<CalcMasteryFromSkillFn>(kCalcMasteryFromSkill);
	}

	static ActorGetBaseCalcAViFn ActorGetBaseCalcAVi()
	{
		return reinterpret_cast<ActorGetBaseCalcAViFn>(kActorGetBaseCalcAVi);
	}

	static ActorGetSkillMasteryLevelFn ActorGetSkillMasteryLevel()
	{
		return reinterpret_cast<ActorGetSkillMasteryLevelFn>(kActorGetSkillMasteryLevel);
	}

	static OpenSkillPerkMenuFn OpenSkillPerkMenu()
	{
		return reinterpret_cast<OpenSkillPerkMenuFn>(kOpenSkillPerkMenu);
	}

	static TESObjectREFRGetAnimDataFn TESObjectREFRGetAnimData()
	{
		return reinterpret_cast<TESObjectREFRGetAnimDataFn>(kTESObjectREFRGetAnimData);
	}

	static ActorAnimDataGetAnimGroupFromField8ValueFn ActorAnimDataGetAnimGroupFromField8Value()
	{
		return reinterpret_cast<ActorAnimDataGetAnimGroupFromField8ValueFn>(kActorAnimDataGetAnimGroupFromField8Value);
	}

	static ActorAnimDataRemovePowerAttackGroupsFn ActorAnimDataRemovePowerAttackGroups()
	{
		return reinterpret_cast<ActorAnimDataRemovePowerAttackGroupsFn>(kActorAnimDataRemovePowerAttackGroups);
	}

	static ObservedActorAnimDataBuildPowerAttackKFListFn ObservedActorAnimDataBuildPowerAttackKFList()
	{
		return reinterpret_cast<ObservedActorAnimDataBuildPowerAttackKFListFn>(kObservedActorAnimDataBuildPowerAttackKFList);
	}

	static AnimKeyGetGroupIDFn AnimKeyGetGroupID()
	{
		return reinterpret_cast<AnimKeyGetGroupIDFn>(kAnimKeyGetGroupID);
	}

	static ActorPlayStaggerAnimGroupFn ActorPlayStaggerAnimGroup()
	{
		return reinterpret_cast<ActorPlayStaggerAnimGroupFn>(kActorPlayStaggerAnimGroup);
	}

	static ActorAttemptAttackDisarmPerkOnHitFn ActorAttemptAttackDisarmPerkOnHit()
	{
		return reinterpret_cast<ActorAttemptAttackDisarmPerkOnHitFn>(kActorAttemptAttackDisarmPerkOnHit);
	}

	static GameRandomLargeIntegerFn GameRandomLargeInteger()
	{
		return reinterpret_cast<GameRandomLargeIntegerFn>(kGameRandomLargeInteger);
	}

	static GetWeaponSkillAVFn GetWeaponSkillAVOriginal()
	{
		return reinterpret_cast<GetWeaponSkillAVFn>(g_getWeaponSkillAVOriginal);
	}

	static CalcWeaponDamageFn CalcWeaponDamageOriginal()
	{
		return reinterpret_cast<CalcWeaponDamageFn>(g_calcWeaponDamageOriginal);
	}

	static CalcPowerAttackBonusFn CalcPowerAttackBonusOriginal()
	{
		return reinterpret_cast<CalcPowerAttackBonusFn>(g_calcPowerAttackBonusOriginal);
	}

	static PlayerMaybeStartNextAttributeBonusBucketFn PlayerMaybeStartNextAttributeBonusBucket()
	{
		return reinterpret_cast<PlayerMaybeStartNextAttributeBonusBucketFn>(kPlayerMaybeStartNextAttributeBonusBucket);
	}

	static PlayerIncrementAttributeBonusBucketFn PlayerIncrementAttributeBonusBucket()
	{
		return reinterpret_cast<PlayerIncrementAttributeBonusBucketFn>(kPlayerIncrementAttributeBonusBucket);
	}

	static UInt32 GetGameSettingUIntOrDefault(const char* name, UInt32 fallback)
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>(name), &setting) && setting && setting->i >= 0)
			return static_cast<UInt32>(setting->i);

		return fallback;
	}

	static UInt32 GetSkillIndexById(UInt32 skillId)
	{
		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (kSkills[i].skillId == skillId)
				return i;
		}

		return 0xFFFFFFFF;
	}

	static UInt32 GetSkillIndexForKind(BladeSeparationShared::WeaponSkillKind kind)
	{
		return GetSkillIndexById(BladeSeparationShared::SkillIdForKind(kind));
	}

	static const char* GetWeaponSkillDisplayName(BladeSeparationShared::WeaponSkillKind kind)
	{
		switch (kind)
		{
		case BladeSeparationShared::kWeaponSkill_Long:
			return kLongSkillDisplayName;
		case BladeSeparationShared::kWeaponSkill_Short:
			return kShortSkillDisplayName;
		case BladeSeparationShared::kWeaponSkill_Axe:
			return kAxeSkillDisplayName;
		default:
			return nullptr;
		}
	}

	static Tile* GetMenuTileAtOffset(void* menu, UInt32 offset)
	{
		return menu ? *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(menu) + offset) : nullptr;
	}

	static float GetTileFloat(Tile* tile, UInt32 trait)
	{
		return tile ? static_cast<float>(TileGetFloat()(tile, trait)) : 0.0f;
	}

	static void SetTileFloat(Tile* tile, UInt32 trait, float value)
	{
		if (tile)
			TileSetFloat()(tile, trait, value);
	}

	static void SetTileString(Tile* tile, UInt32 trait, const char* value)
	{
		if (tile)
			TileSetString()(tile, trait, value ? value : "");
	}

	static const char* GetSafeActorValueName(UInt32 actorValue)
	{
		if (const char* name = ActorValueGetName()(actorValue))
			return name;

		return "";
	}

	static const char* __cdecl HookActorValueGetName(UInt32 actorValue)
	{
		if (actorValue == kActorVal_Blade)
			return kLongSkillDisplayName;

		if (ActorValueGetNameFn original = ActorValueGetName())
			return original(actorValue);

		return "";
	}

	static bool IsWeaponForm(const TESForm* form)
	{
		return form && form->typeID == kFormType_Weapon;
	}

	static TESObjectWEAP* GetPlayerEquippedWeapon()
	{
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return nullptr;

		EquippedItemsList equippedItems = player->GetEquippedItems();
		for (EquippedItemsList::const_iterator it = equippedItems.begin(); it != equippedItems.end(); ++it)
		{
			TESForm* form = *it;
			if (IsWeaponForm(form))
				return static_cast<TESObjectWEAP*>(form);
		}

		return nullptr;
	}

	static const char* GetWeaponDisplayName(TESObjectWEAP* weapon)
	{
		if (!weapon || !weapon->fullName.name.m_data || !weapon->fullName.name.m_dataLen)
			return "";

		return weapon->fullName.name.m_data;
	}

	static TESObjectWEAP* FindWeaponByEditorID(const char* editorId)
	{
		if (!editorId || !editorId[0] || !g_dataHandler || !*g_dataHandler || !(*g_dataHandler)->boundObjects)
			return nullptr;

		TESObjectWEAP* match = nullptr;
		for (TESBoundObject* object = (*g_dataHandler)->boundObjects->first; object; object = object->next)
		{
			TESForm* form = reinterpret_cast<TESForm*>(object);
			if (!IsWeaponForm(form))
				continue;

			TESObjectWEAP* weapon = reinterpret_cast<TESObjectWEAP*>(object);
			const char* candidateEditorId = weapon->GetEditorID();
			if (!candidateEditorId || _stricmp(candidateEditorId, editorId))
				continue;

			if (match && match->refID != weapon->refID)
				return nullptr;

			match = weapon;
		}

		return match;
	}

	static void WeaponTypeStoreLog(void*, const char* message)
	{
		_MESSAGE("BladeSeparation: %s", message ? message : "");
	}

	static bool ResolveWeaponTypeKeyToRuntimeFormID(const BladeSeparationShared::WeaponTypeSidecarKey& key, const char* carrierModName, UInt32* outFormId)
	{
		if (outFormId)
			*outFormId = 0;

		const char* sourceMod = key.sourceMod;
		if (!_stricmp(sourceMod, "$SELF"))
			sourceMod = carrierModName;
		if (!sourceMod || !sourceMod[0] || !g_dataHandler || !*g_dataHandler)
			return false;

		const UInt8 modIndex = (*g_dataHandler)->GetModIndex(sourceMod);
		if (modIndex == 0xFF)
			return false;

		const UInt32 formId = (static_cast<UInt32>(modIndex) << 24) | (key.objectId & 0x00FFFFFF);
		TESForm* form = LookupFormByID(formId);
		if (IsWeaponForm(form))
		{
			if (outFormId)
				*outFormId = formId;
			return true;
		}

		if (!_stricmp(key.sourceMod, "$SELF") && key.editorId[0])
		{
			TESObjectWEAP* fallbackWeapon = FindWeaponByEditorID(key.editorId);
			if (fallbackWeapon)
			{
				if (outFormId)
					*outFormId = fallbackWeapon->refID;
				if (!g_loggedLegacySelfWeaponTypeFallback)
				{
					_MESSAGE("BladeSeparation: resolved legacy $SELF weapon Type sidecar row by editor ID %s", key.editorId);
					g_loggedLegacySelfWeaponTypeFallback = true;
				}
				return true;
			}
		}

		if (outFormId)
			*outFormId = 0;
		return false;
	}

	static void LoadEditorWeaponTypeSidecars(bool preserveExistingIfNoCarriers)
	{
		if (!g_dataHandler || !*g_dataHandler || !(*g_dataHandler)->boundObjects)
		{
			if (!preserveExistingIfNoCarriers)
				g_weaponTypeStore.Clear();
			return;
		}

		BladeSeparationShared::WeaponTypeSidecarStore importedStore;
		BladeSeparationShared::WeaponTypeSidecarPayloadStats totals = {};
		const UInt8 activeModCount = (*g_dataHandler)->GetActiveModCount();
		for (UInt32 modIndex = 0; modIndex < activeModCount; ++modIndex)
		{
			const char* carrierModName = (*g_dataHandler)->GetNthModName(modIndex);
			for (TESBoundObject* object = (*g_dataHandler)->boundObjects->first; object; object = object->next)
			{
				if (object->typeID != kFormType_Book || (object->refID >> 24) != modIndex)
					continue;

				TESObjectBOOK* book = reinterpret_cast<TESObjectBOOK*>(object);
				const char* payload = book->description.GetDescription();
				if (!BladeSeparationShared::WeaponTypeSidecarPayloadCodec::LooksLikePayload(payload))
					continue;

				++totals.carrierRecords;
				std::vector<BladeSeparationShared::WeaponTypeSidecarPayloadRow> rows;
				BladeSeparationShared::WeaponTypeSidecarPayloadStats stats = {};
				if (!BladeSeparationShared::WeaponTypeSidecarPayloadCodec::Parse(payload, rows, &stats, WeaponTypeStoreLog, nullptr))
				{
					totals.skippedRows += stats.skippedRows;
					continue;
				}

				totals.parsedEntries += stats.parsedEntries;
				totals.skippedRows += stats.skippedRows;
				for (size_t i = 0; i < rows.size(); ++i)
				{
					UInt32 resolvedFormId = 0;
					if (!ResolveWeaponTypeKeyToRuntimeFormID(rows[i].key, carrierModName, &resolvedFormId))
					{
						++totals.unresolvedEntries;
						continue;
					}

					if (importedStore.SetLoaded(resolvedFormId, rows[i].kind))
					{
						++totals.resolvedEntries;
					}
					else
					{
						++totals.unresolvedEntries;
						if (!g_loggedWeaponTypeCarrierStoreFull)
						{
							_WARNING("BladeSeparation: embedded weapon Type carrier row could not be stored form=%08X kind=%u", resolvedFormId, rows[i].kind);
							g_loggedWeaponTypeCarrierStoreFull = true;
						}
					}
				}
			}
		}

		if (totals.carrierRecords || !preserveExistingIfNoCarriers)
		{
			g_weaponTypeStore.Clear();
			for (UInt32 i = 0; i < importedStore.Count(); ++i)
			{
				const BladeSeparationShared::WeaponTypeSidecarEntry& entry = importedStore.EntryAt(i);
				g_weaponTypeStore.SetLoaded(entry.formId, entry.kind);
			}
		}

		g_weaponTypeStore.ClearDirty();
		_MESSAGE("BladeSeparation: embedded weapon Type carriers=%u parsed=%u resolved=%u unresolved=%u skipped=%u",
			totals.carrierRecords,
			totals.parsedEntries,
			totals.resolvedEntries,
			totals.unresolvedEntries,
			totals.skippedRows);
	}

	static bool TryGetAuthoredWeaponType(TESObjectWEAP* weapon, BladeSeparationShared::WeaponSkillKind* outKind)
	{
		if (outKind)
			*outKind = BladeSeparationShared::kWeaponSkill_None;
		if (!weapon || !weapon->refID)
			return false;

		BladeSeparationShared::WeaponSkillKind kind = BladeSeparationShared::kWeaponSkill_None;
		if (!g_weaponTypeStore.TryGet(weapon->refID, &kind) || kind == BladeSeparationShared::kWeaponSkill_None)
			return false;

		if (outKind)
			*outKind = kind;
		return true;
	}

	static BladeSeparationShared::WeaponSkillKind ClassifySidecarWeapon(TESObjectWEAP* weapon)
	{
		if (!weapon)
			return BladeSeparationShared::kWeaponSkill_None;

		BladeSeparationShared::WeaponSkillKind authoredKind = BladeSeparationShared::kWeaponSkill_None;
		if (TryGetAuthoredWeaponType(weapon, &authoredKind))
			return authoredKind;

		return BladeSeparationShared::ClassifyWeapon(weapon->type, weapon->GetEditorID(), GetWeaponDisplayName(weapon));
	}

	static UInt32 GetPlayerWeaponSidecarIndexForActorValueContext(Actor* actor, UInt32 actorValue)
	{
		PlayerCharacter* player = GetPlayer();
		if (!player || actor != static_cast<Actor*>(player))
			return 0xFFFFFFFF;

		const BladeSeparationShared::WeaponSkillKind kind = ClassifySidecarWeapon(GetPlayerEquippedWeapon());
		const UInt32 index = GetSkillIndexForKind(kind);
		return index < kSkillCount && kSkills[index].fallbackActorValue == actorValue ? index : 0xFFFFFFFF;
	}

	static UInt32 GetSidecarSkillLevel(UInt32 index)
	{
		if (index >= kSkillCount || !g_tcs)
			return 0;

		return g_tcs->GetSkillLevel(kSkills[index].editorId);
	}

	static bool TryGetPlayerWeaponSidecarLevel(Actor* actor, UInt32 actorValue, UInt32* outIndex, UInt32* outLevel)
	{
		if (outIndex)
			*outIndex = 0xFFFFFFFF;
		if (outLevel)
			*outLevel = 0;

		const UInt32 index = GetPlayerWeaponSidecarIndexForActorValueContext(actor, actorValue);
		if (index >= kSkillCount)
			return false;

		if (outIndex)
			*outIndex = index;
		if (outLevel)
			*outLevel = GetSidecarSkillLevel(index);
		return true;
	}

	static UInt32 NativeWeaponSkillAV(TESObjectWEAP* weapon);

	static bool TryGetPlayerWeaponSidecarLevelForWeapon(Actor* actor, TESObjectWEAP* weapon, UInt32 actorValue, UInt32* outLevel)
	{
		if (outLevel)
			*outLevel = 0;

		PlayerCharacter* player = GetPlayer();
		if (!player || actor != static_cast<Actor*>(player) || !weapon || GetPlayerEquippedWeapon() != weapon)
			return false;

		const BladeSeparationShared::WeaponSkillKind kind = ClassifySidecarWeapon(weapon);
		const UInt32 index = GetSkillIndexForKind(kind);
		if (index >= kSkillCount || kSkills[index].fallbackActorValue != actorValue)
			return false;

		if (outLevel)
			*outLevel = GetSidecarSkillLevel(index);
		return true;
	}

	static UInt32 GetCurrentActorValue(Actor* actor, UInt32 actorValue)
	{
		if (!actor)
			return 0;

		return actor->GetActorValue(actorValue);
	}

	// Sentinel meaning "this weapon/actorValue isn't ours" so the naked hook can defer
	// to a chained mod's hook (if any) instead of assuming sole ownership of the patch site.
	static constexpr UInt32 kNotOurWeaponSkill = 0xFFFFFFFF;

	static UInt32 __cdecl TryGetOwnCombatScoringWeaponSkillLevel(Actor* actor, TESObjectWEAP* weapon)
	{
		const UInt32 nativeActorValue = NativeWeaponSkillAV(weapon);
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevelForWeapon(actor, weapon, nativeActorValue, &sidecarLevel))
			return sidecarLevel;

		return kNotOurWeaponSkill;
	}

	static UInt32 __cdecl GetNativeCombatScoringWeaponSkillLevel(Actor* actor, TESObjectWEAP* weapon)
	{
		return GetCurrentActorValue(actor, NativeWeaponSkillAV(weapon));
	}

	static UInt32 __cdecl TryGetOwnCombatSelectionActorValueSkill(Actor* actor, UInt32 actorValue)
	{
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevel(actor, actorValue, nullptr, &sidecarLevel))
			return sidecarLevel;

		return kNotOurWeaponSkill;
	}

	static __declspec(naked) void HookCombatControllerWeaponSkillLevel()
	{
		__asm
		{
			push ecx
			push ecx
			push ebx
			call TryGetOwnCombatScoringWeaponSkillLevel
			add esp, 8
			cmp eax, 0FFFFFFFFh
			je notMine
			add esp, 4
			mov edx, kCombatControllerWeaponSkillContinue
			jmp edx
			notMine :
			mov edx, dword ptr[g_combatControllerWeaponSkillChainTarget]
				test edx, edx
				jz noChain
				pop ecx
				jmp edx
				noChain :
			pop ecx
				push ecx
				push ebx
				call GetNativeCombatScoringWeaponSkillLevel
				add esp, 8
				mov edx, kCombatControllerWeaponSkillContinue
				jmp edx
		}
	}

	static __declspec(naked) void HookCombatSelectionHandToHandSkillCompare()
	{
		__asm
		{
			push ebx
			push esi
			call TryGetOwnCombatSelectionActorValueSkill
			add esp, 8
			cmp eax, 0FFFFFFFFh
			je notMine
			mov ebx, eax
			jmp doCompare
			notMine :
			mov edx, dword ptr[g_combatSelectionHandToHandChainTarget]
				test edx, edx
				jnz haveChain
				push ebx
				push esi
				call GetCurrentActorValue
				add esp, 8
				mov ebx, eax
				jmp doCompare
				haveChain :
			jmp edx
				doCompare :
			mov edx, [esi]
				mov eax, [edx + 284h]
				push 11h
				mov ecx, esi
				call eax
				cmp eax, ebx
				jle keepCandidate
				mov edx, kCombatSelectionHandToHandPreferred
				jmp edx
				keepCandidate :
			mov edx, kCombatSelectionHandToHandCompareContinue
				jmp edx
		}
	}

	static ActorAnimData* GetActorAnimDataFromVTable(Actor* actor)
	{
		if (!actor)
			return nullptr;

		void** vtbl = *reinterpret_cast<void***>(actor);
		if (!vtbl)
			return nullptr;

		typedef ActorAnimData* (__thiscall* GetActorAnimDataFn)(Actor*);
		GetActorAnimDataFn getAnimData = reinterpret_cast<GetActorAnimDataFn>(vtbl[0x164 / sizeof(void*)]);
		return getAnimData ? getAnimData(actor) : nullptr;
	}

	static UInt32 GetCurrentAttackAnimGroup(Actor* actor)
	{
		ActorAnimData* animData = GetActorAnimDataFromVTable(actor);
		if (!animData)
			return 0xFF;

		const UInt32 animKey = ActorAnimDataGetAnimGroupFromField8Value()(animData, 3);
		return AnimKeyGetGroupID()(animKey);
	}

	static bool IsCurrentBackwardPowerAttack(Actor* actor)
	{
		return GetCurrentAttackAnimGroup(actor) == kAnimGroupBackwardPowerAttack;
	}

	static UInt32 __fastcall HookActorGetBaseCalcAViForWeaponSidecarPerk(Actor* actor, void*, UInt32 actorValue)
	{
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevel(actor, actorValue, nullptr, &sidecarLevel))
			return sidecarLevel;

		return ActorGetBaseCalcAVi()(actor, actorValue);
	}

	static UInt32 __fastcall HookActorGetSkillMasteryLevelForWeaponSidecarPerk(Actor* actor, void*, UInt32 actorValue)
	{
		UInt32 sidecarIndex = 0xFFFFFFFF;
		UInt32 sidecarLevel = 0;
		if (TryGetPlayerWeaponSidecarLevel(actor, actorValue, &sidecarIndex, &sidecarLevel))
		{
			if (sidecarIndex == kShortSkillIndex &&
				reinterpret_cast<UInt32>(_ReturnAddress()) == kPostHitExpertMasteryReturn &&
				IsCurrentBackwardPowerAttack(actor))
			{
				return 2;
			}

			return CalcMasteryFromSkill()(static_cast<SInt32>(sidecarLevel));
		}

		return ActorGetSkillMasteryLevel()(actor, actorValue);
	}

	static bool ShortBladeExpertStaggerRollSucceeds()
	{
		// Native backward weapon perk code uses this chance setting for its knockdown roll.
		const UInt32 chance = GetGameSettingUIntOrDefault("iPerkMarksmanKnockdownChance", 5);
		return chance && (GameRandomLargeInteger()(0) % 100) < chance;
	}

	static void TryApplyShortBladeExpertStagger(Actor* attacker, Actor* target, UInt32 actorValue)
	{
		UInt32 sidecarIndex = 0xFFFFFFFF;
		UInt32 sidecarLevel = 0;
		if (!target ||
			!TryGetPlayerWeaponSidecarLevel(attacker, actorValue, &sidecarIndex, &sidecarLevel) ||
			sidecarIndex != kShortSkillIndex ||
			CalcMasteryFromSkill()(static_cast<SInt32>(sidecarLevel)) < 3 ||
			!IsCurrentBackwardPowerAttack(attacker) ||
			!ShortBladeExpertStaggerRollSucceeds())
		{
			return;
		}

		ActorPlayStaggerAnimGroup()(target);
	}

	static UInt8 __fastcall HookActorAttemptAttackDisarmPerkOnHitForWeaponSidecar(Actor* attacker, void*, Actor* target, UInt32 actorValue)
	{
		const UInt8 disarmed = ActorAttemptAttackDisarmPerkOnHit()(attacker, target, actorValue);
		TryApplyShortBladeExpertStagger(attacker, target, actorValue);
		return disarmed;
	}

	static void RefreshPowerAttackAnimData(ActorAnimData* animData, PlayerCharacter* player)
	{
		if (!animData || !player)
			return;

		ActorAnimDataRemovePowerAttackGroups()(animData);
		ObservedActorAnimDataBuildPowerAttackKFList()(animData, static_cast<TESObjectREFR*>(player), 0);
	}

	static void RefreshPlayerWeaponSidecarPowerAttackGroups()
	{
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;

		RefreshPowerAttackAnimData(TESObjectREFRGetAnimData()(static_cast<TESObjectREFR*>(player)), player);
		RefreshPowerAttackAnimData(player->firstPersonAnimData, player);
	}

	static void __cdecl SetMagicPopupWeaponTypeLabel(Tile* tile, UInt32 trait, const char* nativeLabel, TESForm* form)
	{
		const char* label = nativeLabel;
		if (trait == kMagicPopupWeaponLabelTrait && IsWeaponForm(form))
		{
			const BladeSeparationShared::WeaponSkillKind kind = ClassifySidecarWeapon(static_cast<TESObjectWEAP*>(form));
			if (const char* splitLabel = GetWeaponSkillDisplayName(kind))
				label = splitLabel;
		}

		if (tile)
			TileSetString()(tile, trait, label);
	}

	static __declspec(naked) void HookMagicPopupWeaponTypeLabelSetStringFromEdi()
	{
		__asm
		{
			push edi
			push dword ptr[esp + 0x0C]
			push dword ptr[esp + 0x0C]
			push ecx
			call SetMagicPopupWeaponTypeLabel
			add esp, 0x10
			ret 0x08
		}
	}

	static __declspec(naked) void HookMagicPopupWeaponTypeLabelSetStringFromEbp()
	{
		__asm
		{
			push ebp
			push dword ptr[esp + 0x0C]
			push dword ptr[esp + 0x0C]
			push ecx
			call SetMagicPopupWeaponTypeLabel
			add esp, 0x10
			ret 0x08
		}
	}

	static float GetSkillUseIncrement(UInt32 index, UInt32 useType)
	{
		float nativeUseIncrement = 1.0f;
		if (TESSkill* fallbackSkill = TESSkill::SkillForActorVal(kSkills[index].fallbackActorValue))
			nativeUseIncrement = useType == 1 ? fallbackSkill->useValue1 : fallbackSkill->useValue0;

		if (!std::isfinite(nativeUseIncrement) || nativeUseIncrement < 0.0f)
			return 0.0f;

		return nativeUseIncrement;
	}

	static bool AddWeaponProgress(BladeSeparationShared::WeaponSkillKind kind, UInt32 useType, float baseDelta)
	{
		if (kind == BladeSeparationShared::kWeaponSkill_Long)
			return false;

		const UInt32 index = GetSkillIndexForKind(kind);
		if (index >= kSkillCount || !g_tcs)
			return false;

		float gain = GetSkillUseIncrement(index, useType);
		if (baseDelta != 0.0f)
			gain *= baseDelta;
		if (!std::isfinite(gain) || gain <= 0.0f)
			return true;

		return g_tcs->AddSkillXP(kSkills[index].editorId, gain);
	}

	static const SkillUseCondition* GetSkillUseCondition(BladeSeparationShared::WeaponSkillKind kind)
	{
		for (UInt32 i = 0; i < kSkillCount; ++i)
		{
			if (kSkillUseConditions[i].kind == kind)
				return &kSkillUseConditions[i];
		}
		return nullptr;
	}

	static bool SkillConditionAllowsProgress(BladeSeparationShared::WeaponSkillKind kind, UInt32 actorValue, UInt32 useType)
	{
		const SkillUseCondition* condition = GetSkillUseCondition(kind);
		return condition &&
			condition->nativeActorValue == actorValue &&
			condition->useType == useType;
	}

	static void __fastcall HookPlayerModExperience(PlayerCharacter* player, void*, UInt32 actorValue, UInt32 useType, float baseDelta)
	{
		if (player == GetPlayer() && (actorValue == kActorVal_Blade || actorValue == kActorVal_Blunt))
		{
			const BladeSeparationShared::WeaponSkillKind kind = ClassifySidecarWeapon(GetPlayerEquippedWeapon());
			_MESSAGE("BladeSeparation: HookPlayerModExperience actorValue=%08X useType=%u baseDelta=%.4f classifiedKind=%d",
				actorValue, useType, baseDelta, static_cast<int>(kind));

			const bool conditionAllows = SkillConditionAllowsProgress(kind, actorValue, useType);
			if (!conditionAllows)
				_MESSAGE("BladeSeparation: SkillConditionAllowsProgress=false (kind=%d, actorValue=%08X, useType=%u) -- falling through to original Player_ModExperience",
					static_cast<int>(kind), actorValue, useType);

			if (conditionAllows && AddWeaponProgress(kind, useType, baseDelta))
				return;
		}

		if (PlayerModExperienceOriginal())
			PlayerModExperienceOriginal()(player, actorValue, useType, baseDelta);
	}

	static void ClearPendingWeaponSkillConsumer()
	{
		std::memset(&g_pendingWeaponSkillConsumer, 0, sizeof(g_pendingWeaponSkillConsumer));
	}

	static bool IsEquippedByPlayer(TESObjectWEAP* weapon)
	{
		return weapon && GetPlayerEquippedWeapon() == weapon;
	}

	static UInt32 NativeWeaponSkillAV(TESObjectWEAP* weapon)
	{
		static constexpr UInt32 kNativeWeaponSkillActorValues[] =
		{
			kActorVal_Blade,
			kActorVal_Blade,
			kActorVal_Blunt,
			kActorVal_Blunt,
			kActorVal_Blunt,
			kActorVal_Marksman
		};

		if (!weapon || weapon->type >= sizeof(kNativeWeaponSkillActorValues) / sizeof(kNativeWeaponSkillActorValues[0]))
			return kActorVal_Blunt;

		return kNativeWeaponSkillActorValues[weapon->type];
	}

	static UInt32 GetNativeWeaponSkillAV(TESObjectWEAP* weapon)
	{
		if (GetWeaponSkillAVFn original = GetWeaponSkillAVOriginal())
			return original(weapon);

		return NativeWeaponSkillAV(weapon);
	}

	static bool ShouldCaptureWeaponSkillConsumer(UInt32 sourceReturnAddress)
	{
		return sourceReturnAddress == kRetWeaponSkillEquippedDamage ||
			sourceReturnAddress == kRetWeaponSkillPlayerInventoryRating ||
			sourceReturnAddress == kRetWeaponSkillEquippableRatingA ||
			sourceReturnAddress == kRetWeaponSkillEquippableRatingB ||
			sourceReturnAddress == kRetWeaponSkillActorBaseEquippableRating ||
			sourceReturnAddress == kRetWeaponSkillPowerAttackBonus ||
			sourceReturnAddress == kRetWeaponSkillProjectileDamage;
	}

	static bool SourceCanUsePlayerSidecarSkill(UInt32 sourceReturnAddress, TESObjectWEAP* weapon)
	{
		if (sourceReturnAddress == kRetWeaponSkillPlayerInventoryRating)
			return true;

		if (sourceReturnAddress == kRetWeaponSkillActorBaseEquippableRating)
			return false;

		return IsEquippedByPlayer(weapon);
	}

	static void CapturePendingWeaponSkillConsumer(TESObjectWEAP* weapon, BladeSeparationShared::WeaponSkillKind kind, UInt32 sourceReturnAddress)
	{
		g_pendingWeaponSkillConsumer.active = true;
		g_pendingWeaponSkillConsumer.playerFacing = SourceCanUsePlayerSidecarSkill(sourceReturnAddress, weapon);
		g_pendingWeaponSkillConsumer.sourceReturnAddress = sourceReturnAddress;
		g_pendingWeaponSkillConsumer.weapon = weapon;
		g_pendingWeaponSkillConsumer.kind = kind;
	}

	static bool PendingWeaponSkillMatchesDamageReturn(UInt32 sourceReturnAddress, UInt32 damageReturnAddress)
	{
		switch (sourceReturnAddress)
		{
		case kRetWeaponSkillEquippedDamage:
			return damageReturnAddress == kRetEquippedWeaponDamage;
		case kRetWeaponSkillPlayerInventoryRating:
			return damageReturnAddress == kRetPlayerInventoryWeaponRating;
		case kRetWeaponSkillEquippableRatingA:
		case kRetWeaponSkillEquippableRatingB:
			return damageReturnAddress == kRetEquippableWeaponRating;
		case kRetWeaponSkillActorBaseEquippableRating:
			return damageReturnAddress == kRetActorBaseEquippableWeaponRating;
		case kRetWeaponSkillProjectileDamage:
			return damageReturnAddress == kRetWeaponDamageWrapper;
		default:
			return false;
		}
	}

	static bool PendingWeaponSkillMatchesPowerAttackReturn(UInt32 sourceReturnAddress, UInt32 powerAttackReturnAddress)
	{
		return sourceReturnAddress == kRetWeaponSkillPowerAttackBonus &&
			powerAttackReturnAddress == kRetPowerAttackBonus;
	}

	static bool TryGetPendingSidecarWeaponSkill(UInt32 returnAddress, bool powerAttack, UInt32* outLevel)
	{
		if (outLevel)
			*outLevel = 0;
		if (!g_pendingWeaponSkillConsumer.active || !g_pendingWeaponSkillConsumer.playerFacing)
			return false;

		const bool matches = powerAttack ?
			PendingWeaponSkillMatchesPowerAttackReturn(g_pendingWeaponSkillConsumer.sourceReturnAddress, returnAddress) :
			PendingWeaponSkillMatchesDamageReturn(g_pendingWeaponSkillConsumer.sourceReturnAddress, returnAddress);
		if (!matches)
			return false;

		const UInt32 index = GetSkillIndexForKind(g_pendingWeaponSkillConsumer.kind);
		if (index >= kSkillCount)
			return false;

		if (outLevel)
			*outLevel = GetSidecarSkillLevel(index);
		return true;
	}

	static UInt32 __fastcall HookGetWeaponSkillAV(TESObjectWEAP* weapon, void*)
	{
		const UInt32 nativeActorValue = GetNativeWeaponSkillAV(weapon);
		const UInt32 returnAddress = reinterpret_cast<UInt32>(_ReturnAddress());
		ClearPendingWeaponSkillConsumer();

		if (weapon && ShouldCaptureWeaponSkillConsumer(returnAddress))
		{
			const BladeSeparationShared::WeaponSkillKind kind = ClassifySidecarWeapon(weapon);
			if (kind != BladeSeparationShared::kWeaponSkill_None)
				CapturePendingWeaponSkillConsumer(weapon, kind, returnAddress);
		}

		return nativeActorValue;
	}

	static double CallOriginalWeaponDamage(int weaponSkill, int luck, int strengthOrAgility, float fatigue, int weaponDamage, float condition, float multiplier, float ignoreFatigue)
	{
		if (CalcWeaponDamageFn original = CalcWeaponDamageOriginal())
			return original(weaponSkill, luck, strengthOrAgility, fatigue, weaponDamage, condition, multiplier, ignoreFatigue);

		return 0.0;
	}

	static double __cdecl HookCalcWeaponDamage(int weaponSkill, int luck, int strengthOrAgility, float fatigue, int weaponDamage, float condition, float multiplier, float ignoreFatigue)
	{
		UInt32 sidecarSkill = 0;
		if (TryGetPendingSidecarWeaponSkill(reinterpret_cast<UInt32>(_ReturnAddress()), false, &sidecarSkill))
			weaponSkill = static_cast<int>(sidecarSkill);
		ClearPendingWeaponSkillConsumer();
		return CallOriginalWeaponDamage(weaponSkill, luck, strengthOrAgility, fatigue, weaponDamage, condition, multiplier, ignoreFatigue);
	}

	static double CallOriginalPowerAttackBonus(int skillLevel, int attackType)
	{
		if (CalcPowerAttackBonusFn original = CalcPowerAttackBonusOriginal())
			return original(skillLevel, attackType);

		return 1.0;
	}

	static double __cdecl HookCalcPowerAttackBonus(int skillLevel, int attackType)
	{
		UInt32 sidecarSkill = 0;
		if (TryGetPendingSidecarWeaponSkill(reinterpret_cast<UInt32>(_ReturnAddress()), true, &sidecarSkill))
			skillLevel = static_cast<int>(sidecarSkill);
		ClearPendingWeaponSkillConsumer();
		return CallOriginalPowerAttackBonus(skillLevel, attackType);
	}

	static bool BytesEqual(UInt32 address, const UInt8* expected, UInt32 length)
	{
		return std::memcmp(reinterpret_cast<const void*>(address), expected, length) == 0;
	}

	static UInt32 ReadRelCallTarget(UInt32 address)
	{
		const SInt32 offset = *reinterpret_cast<const SInt32*>(address + 1);
		return address + 5 + offset;
	}

	static UInt32 ReadRelJumpTarget(UInt32 address)
	{
		const SInt32 offset = *reinterpret_cast<const SInt32*>(address + 1);
		return address + 5 + offset;
	}

	static void WriteRel32(UInt8* code, UInt32 offset, UInt8 opcode, UInt32 target)
	{
		code[offset] = opcode;
		*reinterpret_cast<UInt32*>(code + offset + 1) =
			target - (reinterpret_cast<UInt32>(code) + offset) - 5;
	}

	static bool WriteRelCallChecked(const char* name, UInt32 address, UInt32 expectedTarget, UInt32 hookTarget)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] != 0xE8)
		{
			_ERROR("BladeSeparation: %s at %08X is not a call", name, address);
			++g_failedPatches;
			return false;
		}

		const UInt32 currentTarget = ReadRelCallTarget(address);
		if (currentTarget == hookTarget)
			return true;
		if (currentTarget != expectedTarget)
		{
			_ERROR("BladeSeparation: %s target mismatch at %08X expected %08X actual %08X", name, address, expectedTarget, currentTarget);
			++g_failedPatches;
			return false;
		}

		WriteRelCall(address, hookTarget);
		++g_appliedPatches;
		return true;
	}

	static bool WriteRelCallChained(const char* name, UInt32 address, UInt32 expectedTarget, UInt32 hookTarget, UInt32& originalTarget)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] != 0xE8)
		{
			_ERROR("BladeSeparation: %s at %08X is not a call", name, address);
			++g_failedPatches;
			return false;
		}

		const UInt32 currentTarget = ReadRelCallTarget(address);
		if (currentTarget == hookTarget)
			return true;
		if (currentTarget != expectedTarget)
		{
			if (originalTarget != expectedTarget && originalTarget != currentTarget)
			{
				_ERROR("BladeSeparation: %s target mismatch at %08X expected %08X or chained %08X actual %08X",
					name, address, expectedTarget, originalTarget, currentTarget);
				++g_failedPatches;
				return false;
			}

			originalTarget = currentTarget;
			_MESSAGE("BladeSeparation: chaining existing %s target=%08X", name, currentTarget);
		}

		WriteRelCall(address, hookTarget);
		++g_appliedPatches;
		return true;
	}

	static void* CreateCalcWeaponDamageGateway()
	{
		if (!BytesEqual(kCalcWeaponDamage, kCalcWeaponDamageExpected, sizeof(kCalcWeaponDamageExpected)))
		{
			_ERROR("BladeSeparation: cannot build Calc_WeaponDamage gateway because signature does not match");
			++g_failedPatches;
			return nullptr;
		}

		static constexpr UInt32 kGatewayLength = 23;
		UInt8* gateway = static_cast<UInt8*>(VirtualAlloc(nullptr, kGatewayLength, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!gateway)
		{
			_ERROR("BladeSeparation: VirtualAlloc failed creating Calc_WeaponDamage gateway gle=%u", GetLastError());
			++g_failedPatches;
			return nullptr;
		}

		std::memcpy(gateway, kCalcWeaponDamageExpected, 13);
		WriteRel32(gateway, 13, 0xE8, kCalcLuckModifiedSkill);
		WriteRel32(gateway, 18, 0xE9, kCalcWeaponDamageContinue);
		FlushInstructionCache(GetCurrentProcess(), gateway, kGatewayLength);
		return gateway;
	}

	static void* CreateCalcPowerAttackBonusGateway()
	{
		if (!BytesEqual(kCalcPowerAttackBonus, kCalcPowerAttackBonusExpected, sizeof(kCalcPowerAttackBonusExpected)))
		{
			_ERROR("BladeSeparation: cannot build Calc_PowerAttackBonus gateway because signature does not match");
			++g_failedPatches;
			return nullptr;
		}

		static constexpr UInt32 kGatewayLength = 26;
		UInt8* gateway = static_cast<UInt8*>(VirtualAlloc(nullptr, kGatewayLength, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!gateway)
		{
			_ERROR("BladeSeparation: VirtualAlloc failed creating Calc_PowerAttackBonus gateway gle=%u", GetLastError());
			++g_failedPatches;
			return nullptr;
		}

		std::memcpy(gateway, kCalcPowerAttackBonusExpected, 16);
		WriteRel32(gateway, 16, 0xE8, kCalcMasteryFromSkill);
		WriteRel32(gateway, 21, 0xE9, kCalcPowerAttackBonusContinue);
		FlushInstructionCache(GetCurrentProcess(), gateway, kGatewayLength);
		return gateway;
	}

	static bool WriteRelJumpChecked(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength = 5)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] == 0xE9 && ReadRelJumpTarget(address) == target)
			return true;
		if (!BytesEqual(address, expected, expectedLength))
		{
			_ERROR("BladeSeparation: signature mismatch for %s at %08X", name, address);
			++g_failedPatches;
			return false;
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, patchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			++g_failedPatches;
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < patchLength; ++i)
			code[i] = 0x90;
		FlushInstructionCache(GetCurrentProcess(), ptr, patchLength);
		DWORD ignored = 0;
		VirtualProtect(ptr, patchLength, oldProtect, &ignored);
		++g_appliedPatches;
		return true;
	}

	static bool WriteRelJumpRaw(const char* name, UInt32 address, UInt32 target, UInt32 patchLength = 5);

	// Like WriteRelJumpChecked, but for jmp-replacement patch sites that a *second* mod
	// using the same convention may legitimately also want to patch (e.g. BladeSeparation
	// and SpearSkill both replacing the same combat-scoring call site). If bytes at
	// `address` are already a jmp to something other than our own hookTarget, that's
	// another mod's hook rather than a corrupted/foreign patch: its target is captured
	// into chainTarget (read by our naked hook's "not mine" fallback) instead of failing
	// with a signature mismatch.
	static bool WriteRelJumpChainable(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 hookTarget, UInt32 patchLength, UInt32& chainTarget)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(address);
			if (currentTarget == hookTarget)
				return true;

			chainTarget = currentTarget;
			_MESSAGE("BladeSeparation: chaining existing %s target=%08X", name, currentTarget);
			return WriteRelJumpRaw(name, address, hookTarget, patchLength);
		}

		return WriteRelJumpChecked(name, address, expected, expectedLength, hookTarget, patchLength);
	}

	static bool WriteRelJumpRaw(const char* name, UInt32 address, UInt32 target, UInt32 patchLength)
	{
		if (patchLength < 5)
		{
			_ERROR("BladeSeparation: invalid raw jump patch length for %s at %08X length=%u", name, address, patchLength);
			++g_failedPatches;
			return false;
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, patchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			_ERROR("BladeSeparation: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			++g_failedPatches;
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < patchLength; ++i)
			code[i] = 0x90;
		FlushInstructionCache(GetCurrentProcess(), ptr, patchLength);
		DWORD ignored = 0;
		VirtualProtect(ptr, patchLength, oldProtect, &ignored);
		++g_appliedPatches;
		_MESSAGE("BladeSeparation: installed %s at %08X", name, address);
		return true;
	}

	static void* CreateTrampoline(UInt32 address, UInt32 stolenLength)
	{
		const UInt32 length = stolenLength + 5;
		UInt8* trampoline = static_cast<UInt8*>(VirtualAlloc(nullptr, length, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!trampoline)
			return nullptr;

		std::memcpy(trampoline, reinterpret_cast<const void*>(address), stolenLength);
		trampoline[stolenLength] = 0xE9;
		*reinterpret_cast<UInt32*>(trampoline + stolenLength + 1) =
			(address + stolenLength) - (reinterpret_cast<UInt32>(trampoline) + stolenLength) - 5;
		FlushInstructionCache(GetCurrentProcess(), trampoline, length);
		return trampoline;
	}

	static bool InstallFunctionJumpHook(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength, void*& original)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(address);
			if (currentTarget == target)
				return true;

			original = reinterpret_cast<void*>(currentTarget);
			_MESSAGE("BladeSeparation: chaining existing %s target=%08X", name, currentTarget);
			return WriteRelJumpRaw(name, address, target);
		}

		if (!original)
			original = CreateTrampoline(address, patchLength);
		if (!original)
		{
			_ERROR("BladeSeparation: failed to create trampoline for %s at %08X", name, address);
			++g_failedPatches;
			return false;
		}

		return WriteRelJumpChecked(name, address, expected, expectedLength, target, patchLength);
	}

	static bool InstallActorValueGetNameHook()
	{
		const UInt32 hookTarget = reinterpret_cast<UInt32>(&HookActorValueGetName);
		const UInt8* actual = reinterpret_cast<const UInt8*>(kActorValueGetName);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(kActorValueGetName);
			if (currentTarget == hookTarget)
				return true;

			g_actorValueGetNameOriginal = reinterpret_cast<void*>(currentTarget);
			_MESSAGE("BladeSeparation: chaining existing ActorValue_GetName hook target=%08X", currentTarget);
			return WriteRelJumpRaw("ActorValue_GetName Blade display mask chained", kActorValueGetName, hookTarget);
		}

		if (!g_actorValueGetNameOriginal)
			g_actorValueGetNameOriginal = CreateTrampoline(kActorValueGetName, kActorValueGetNamePatchLength);
		if (!g_actorValueGetNameOriginal)
		{
			++g_failedPatches;
			return false;
		}

		return WriteRelJumpChecked("ActorValue_GetName Blade display mask", kActorValueGetName, kActorValueGetNameExpected, sizeof(kActorValueGetNameExpected), hookTarget, kActorValueGetNamePatchLength);
	}

	static bool InstallHooks()
	{
		g_appliedPatches = 0;
		g_failedPatches = 0;
		bool ok = true;

		ok &= InstallFunctionJumpHook("Player_ModExperience weapon sidecar progress",
			kPlayerModExperience,
			kPlayerModExperienceExpected,
			sizeof(kPlayerModExperienceExpected),
			reinterpret_cast<UInt32>(&HookPlayerModExperience),
			kPlayerModExperiencePatchLength,
			g_playerModExperienceOriginal);

		ok &= InstallFunctionJumpHook("TESObjectWEAP_GetWeaponSkillAV sidecar consumer context",
			kTESObjectWEAPGetWeaponSkillAV,
			kTESObjectWEAPGetWeaponSkillAVExpected,
			sizeof(kTESObjectWEAPGetWeaponSkillAVExpected),
			reinterpret_cast<UInt32>(&HookGetWeaponSkillAV),
			sizeof(kTESObjectWEAPGetWeaponSkillAVExpected),
			g_getWeaponSkillAVOriginal);

		ok &= WriteRelJumpChainable("CombatController weapon skill sidecar scoring",
			kCombatControllerWeaponSkillCall,
			kCombatControllerWeaponSkillExpected,
			sizeof(kCombatControllerWeaponSkillExpected),
			reinterpret_cast<UInt32>(&HookCombatControllerWeaponSkillLevel),
			sizeof(kCombatControllerWeaponSkillExpected),
			g_combatControllerWeaponSkillChainTarget);

		ok &= WriteRelJumpChainable("Combat selection weapon skill sidecar scoring",
			kCombatSelectionHandToHandComparePatch,
			kCombatSelectionHandToHandCompareExpected,
			sizeof(kCombatSelectionHandToHandCompareExpected),
			reinterpret_cast<UInt32>(&HookCombatSelectionHandToHandSkillCompare),
			sizeof(kCombatSelectionHandToHandCompareExpected),
			g_combatSelectionHandToHandChainTarget);

		if (!g_calcWeaponDamageOriginal)
			g_calcWeaponDamageOriginal = CreateCalcWeaponDamageGateway();
		if (g_calcWeaponDamageOriginal)
		{
			ok &= WriteRelJumpChecked("Calc_WeaponDamage sidecar skill substitution",
				kCalcWeaponDamage,
				kCalcWeaponDamageExpected,
				sizeof(kCalcWeaponDamageExpected),
				reinterpret_cast<UInt32>(&HookCalcWeaponDamage),
				sizeof(kCalcWeaponDamageExpected));
		}
		else
		{
			ok = false;
		}

		if (!g_calcPowerAttackBonusOriginal)
			g_calcPowerAttackBonusOriginal = CreateCalcPowerAttackBonusGateway();
		if (g_calcPowerAttackBonusOriginal)
		{
			ok &= WriteRelJumpChecked("Calc_PowerAttackBonus sidecar skill substitution",
				kCalcPowerAttackBonus,
				kCalcPowerAttackBonusExpected,
				sizeof(kCalcPowerAttackBonusExpected),
				reinterpret_cast<UInt32>(&HookCalcPowerAttackBonus),
				sizeof(kCalcPowerAttackBonusExpected));
		}
		else
		{
			ok = false;
		}

		ok &= InstallActorValueGetNameHook();

		ok &= WriteRelCallChecked("PowerAttack KF list weapon sidecar mastery hook", kPowerAttackBuilderMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk));
		ok &= WriteRelCallChecked("Weapon sidecar sidestep disarm perk level hook", kLongBladeDisarmBaseCalcCall, kActorGetBaseCalcAVi, reinterpret_cast<UInt32>(&HookActorGetBaseCalcAViForWeaponSidecarPerk));
		ok &= WriteRelCallChecked("Weapon sidecar sweeping perk mastery hook", kPostHitExpertMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk));
		ok &= WriteRelCallChecked("Weapon sidecar rushing paralyze mastery hook", kPostHitMasterMasteryCall, kActorGetSkillMasteryLevel, reinterpret_cast<UInt32>(&HookActorGetSkillMasteryLevelForWeaponSidecarPerk));
		ok &= WriteRelCallChecked("Short Blade backward stagger perk hook", kAttackDisarmPostHitCall, kActorAttemptAttackDisarmPerkOnHit, reinterpret_cast<UInt32>(&HookActorAttemptAttackDisarmPerkOnHitForWeaponSidecar));

		ok &= WriteRelCallChecked("MagicPopupMenu enchanted weapon Type sidecar label hook", kMagicPopupEnchantedWeaponLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEbp));
		ok &= WriteRelCallChecked("MagicPopupMenu enchanted weapon Type sidecar null-label hook", kMagicPopupEnchantedWeaponNullLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEbp));
		ok &= WriteRelCallChecked("MagicPopupMenu weapon Type sidecar label hook", kMagicPopupSimpleWeaponLabelSetStringCall, kTileSetString, reinterpret_cast<UInt32>(&HookMagicPopupWeaponTypeLabelSetStringFromEdi));

		_MESSAGE("BladeSeparation: native hooks installed applied=%u failed=%u", g_appliedPatches, g_failedPatches);
		return ok && g_failedPatches == 0;
	}

	static bool InstallHooksOnce()
	{
		if (g_hooksInstalled)
			return true;
		if (g_hookInstallAttempted)
			return false;

		g_hookInstallAttempted = true;
		g_hooksInstalled = InstallHooks();
		return g_hooksInstalled;
	}

	static void SetGameSettingStringIfPresent(const char* settingName, const char* value)
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>(settingName), &setting) && setting)
			setting->Set(value);
	}

	static void PatchVisibleGameSettingLabels()
	{
		SetGameSettingStringIfPresent("sBladeOneHand", "Long Blade / Short Blade");
		SetGameSettingStringIfPresent("sBladeTwoHand", "Long Blade");
	}

	static UInt32 GetShortBladeSkill()
	{
		return g_tcs ? g_tcs->GetSkillLevel(kSkills[kShortSkillIndex].editorId) : 0;
	}

	static float GetShortBladeProgress()
	{
		return g_tcs ? g_tcs->GetSkillProgress(kSkills[kShortSkillIndex].editorId) : 0.0f;
	}

	static float GetShortBladeRequiredProgress()
	{
		return g_tcs ? g_tcs->GetSkillRequiredProgress(kSkills[kShortSkillIndex].editorId) : 0.0f;
	}

	static UInt32 GetShortBladeLevelUps()
	{
		return g_tcs ? g_tcs->GetSkillLevelUps(kSkills[kShortSkillIndex].editorId) : 0;
	}

	static bool IsShortBladeMajorSkill()
	{
		return g_tcs && g_tcs->IsSkillMajor(kSkills[kShortSkillIndex].editorId);
	}

	static UInt32 GetAxeSkill()
	{
		return g_tcs ? g_tcs->GetSkillLevel(kSkills[kAxeSkillIndex].editorId) : 0;
	}

	static float GetAxeProgress()
	{
		return g_tcs ? g_tcs->GetSkillProgress(kSkills[kAxeSkillIndex].editorId) : 0.0f;
	}

	static float GetAxeRequiredProgress()
	{
		return g_tcs ? g_tcs->GetSkillRequiredProgress(kSkills[kAxeSkillIndex].editorId) : 0.0f;
	}

	static UInt32 GetAxeLevelUps()
	{
		return g_tcs ? g_tcs->GetSkillLevelUps(kSkills[kAxeSkillIndex].editorId) : 0;
	}

	static bool IsAxeMajorSkill()
	{
		return g_tcs && g_tcs->IsSkillMajor(kSkills[kAxeSkillIndex].editorId);
	}

	static bool SaveWeaponTypeSidecars()
	{
		if (!g_serialization)
			return false;

		const UInt32 count = g_weaponTypeStore.Count();
		if (!g_serialization->OpenRecord(kRecordWeaponTypes, kWeaponTypesRecordVersion))
			return false;
		if (!g_serialization->WriteRecordData(&count, sizeof(count)))
			return false;

		for (UInt32 i = 0; i < count; ++i)
		{
			const BladeSeparationShared::WeaponTypeSidecarEntry& source = g_weaponTypeStore.EntryAt(i);
			SavedWeaponTypeEntry entry = {};
			entry.formId = source.formId;
			entry.kind = static_cast<UInt32>(source.kind);
			if (!g_serialization->WriteRecordData(&entry, sizeof(entry)))
				return false;
		}

		return true;
	}

	static void LoadWeaponTypeSidecars(UInt32 version, UInt32 length)
	{
		if (!g_serialization)
			return;
		if (version != kWeaponTypesRecordVersion || length < sizeof(UInt32))
		{
			_WARNING("BladeSeparation: ignored incompatible weapon Type save record version=%u length=%u", version, length);
			return;
		}

		UInt32 savedCount = 0;
		if (g_serialization->ReadRecordData(&savedCount, sizeof(savedCount)) != sizeof(savedCount))
			return;

		const UInt32 maxByLength = (length - sizeof(savedCount)) / sizeof(SavedWeaponTypeEntry);
		const UInt32 entriesToRead = savedCount < maxByLength ? savedCount : maxByLength;
		UInt32 resolved = 0;
		UInt32 unresolved = 0;
		for (UInt32 i = 0; i < entriesToRead; ++i)
		{
			SavedWeaponTypeEntry entry = {};
			if (g_serialization->ReadRecordData(&entry, sizeof(entry)) != sizeof(entry))
				break;

			UInt32 resolvedFormId = 0;
			if (!g_serialization->ResolveRefID(entry.formId, &resolvedFormId))
			{
				++unresolved;
				continue;
			}

			TESForm* form = LookupFormByID(resolvedFormId);
			const BladeSeparationShared::WeaponSkillKind kind = static_cast<BladeSeparationShared::WeaponSkillKind>(entry.kind);
			if (!IsWeaponForm(form) || BladeSeparationShared::SkillIdForKind(kind) == 0xFFFFFFFF)
			{
				++unresolved;
				continue;
			}

			if (g_weaponTypeStore.Count() >= kMaxSavedWeaponTypeEntries)
			{
				++unresolved;
				continue;
			}

			if (g_weaponTypeStore.SetLoaded(resolvedFormId, kind))
				++resolved;
		}

		if (savedCount > entriesToRead)
			_WARNING("BladeSeparation: weapon Type save record count=%u length only contained %u entries", savedCount, entriesToRead);

		g_weaponTypeStore.ClearDirty();
		_MESSAGE("BladeSeparation: loaded weapon Type co-save sidecars resolved=%u unresolved=%u", resolved, unresolved);
	}

	static void SaveCallback(void*)
	{
		if (!SaveWeaponTypeSidecars())
			_WARNING("BladeSeparation: failed to write weapon Type sidecar save record");
	}

	static void LoadCallback(void*)
	{
		ClearPendingWeaponSkillConsumer();
		g_weaponTypeStore.Clear();
		if (!g_serialization)
			return;

		UInt32 type = 0;
		UInt32 version = 0;
		UInt32 length = 0;
		while (g_serialization->GetNextRecordInfo(&type, &version, &length))
		{
			if (type == kRecordWeaponTypes)
				LoadWeaponTypeSidecars(version, length);
		}

		LoadEditorWeaponTypeSidecars(true);
		PatchVisibleGameSettingLabels();
	}

	static void NewGameCallback(void*)
	{
		ClearPendingWeaponSkillConsumer();
		g_weaponTypeStore.Clear();
		LoadEditorWeaponTypeSidecars(false);
		PatchVisibleGameSettingLabels();
	}

	static void RegisterSerializationCallbacks()
	{
		if (!g_serialization)
			return;

		g_serialization->SetSaveCallback(g_pluginHandle, SaveCallback);
		g_serialization->SetLoadCallback(g_pluginHandle, LoadCallback);
		g_serialization->SetNewGameCallback(g_pluginHandle, NewGameCallback);
	}

	static void RequestTCSInterfaceOnce();

	static void MessageHandler(OBSEMessagingInterface::Message* message)
	{
		if (!message)
			return;

		switch (message->type)
		{
		case OBSEMessagingInterface::kMessage_PostPostLoad:
			RequestTCSInterfaceOnce();
			if (!InstallHooksOnce())
				_ERROR("BladeSeparation: failed to install native hooks after OBSE plugin load");
			break;
		case OBSEMessagingInterface::kMessage_GameInitialized:
			ClearPendingWeaponSkillConsumer();
			LoadEditorWeaponTypeSidecars(false);
			PatchVisibleGameSettingLabels();
			break;
		case OBSEMessagingInterface::kMessage_PostLoadGame:
			ClearPendingWeaponSkillConsumer();
			LoadEditorWeaponTypeSidecars(true);
			PatchVisibleGameSettingLabels();
			break;
		}
	}

	static void RegisterMessaging(const OBSEInterface* obse)
	{
		if (!obse || !obse->QueryInterface || g_pluginHandle == kPluginHandle_Invalid)
			return;

		OBSEMessagingInterface* messaging =
			static_cast<OBSEMessagingInterface*>(obse->QueryInterface(kInterface_Messaging));
		if (messaging && messaging->RegisterListener)
		{
			g_messaging = messaging;
			messaging->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
		}
	}

	static void RequestTCSInterfaceOnce()
	{
		if (g_tcs || !g_messaging || !g_messaging->Dispatch)
			return;

		g_messaging->Dispatch(g_pluginHandle, kMessage_TCSGetInterface, &g_tcs, sizeof(g_tcs), "TrueCustomSkills");
		if (g_tcs)
			_MESSAGE("BladeSeparation: acquired TrueCustomSkills interface version=%u", g_tcs->interfaceVersion);
		else
			_WARNING("BladeSeparation: TrueCustomSkills interface not available (is TCS installed?)");
	}
}

extern "C"
{
	bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info)
	{
		if (!obse || !info)
			return false;

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "Blade Separation";
		info->version = BladeSeparation::kPluginVersion;

		if (obse->isEditor)
			return false;

		if (obse->obseVersion < OBSE_VERSION_INTEGER)
			return false;
		if (obse->oblivionVersion != OBLIVION_VERSION)
			return false;

		g_serialization = static_cast<OBSESerializationInterface*>(obse->QueryInterface(kInterface_Serialization));
		return g_serialization != nullptr;
	}

	bool OBSEPlugin_Load(const OBSEInterface* obse)
	{
		if (!obse)
			return false;

		g_pluginHandle = obse->GetPluginHandle();
		BladeSeparation::RegisterSerializationCallbacks();
		BladeSeparation::RegisterMessaging(obse);
		return true;
	}
}