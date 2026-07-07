#include "obse/PluginAPI.h"
#include "common/IDebugLog.h"
#include "..\shared\BladeSeparation.h"
#include "..\shared\BladeNpcSkillSidecar.h"
#include "..\shared\BladeWeaponTypeSidecar.h"
#include "..\shared\SidecarSkillProvider.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>
#include <commctrl.h>

#ifndef CB_SETMINVISIBLE
#define CB_SETMINVISIBLE 0x1701
#endif

IDebugLog gLog("BladeSkillsRestored_CS.log");

namespace BladeSeparationCSE
{
	static constexpr UInt32 kPluginVersion = 1;
	static constexpr UInt32 kWeaponDialogInitPatch = 0x51D710;
	static constexpr UInt32 kWeaponDialogInitPatchLength = 7;
	static constexpr UInt32 kObjectWindowListViewGetDispInfoPatch = 0x414E60;
	static constexpr UInt32 kObjectWindowListViewGetDispInfoPatchLength = 14;
	static constexpr UInt32 kNpcStatsPopulateVtableEntry = 0x944328;
	static constexpr UInt32 kNpcStatsPopulateOriginal = 0x4CC3C0;
	static constexpr UInt32 kNpcStatsInitSkillListOpenCall = 0x4DAFAC;
	static constexpr UInt32 kNpcStatsInitSkillListCall = 0x4D76FC;
	static constexpr UInt32 kNpcStatsInitSkillListOriginal = 0x4D6160;
	static constexpr UInt32 kNpcStatsSkillEditAutoCalcGatePatch = 0x4DC70D;
	static constexpr UInt32 kNpcStatsSkillEditAutoCalcContinue = 0x4DC715;
	static constexpr UInt32 kNpcStatsSkillEditAutoCalcRejectReturn = 0x4DB81D;
	static constexpr UInt32 kNpcStatsDialogSuccessReturn = 0x4DD6FD;
	static constexpr UInt32 kNpcStatsSkillEditCommitPatch = 0x4DC75B;
	static constexpr UInt32 kNpcStatsSkillEditCommitReturn = 0x4DC765;
	static constexpr UInt32 kAiFormInitDialogControls = 0x48CC50;
	static constexpr UInt32 kAiFormInitDialogControlsPatchLength = 8;
	static constexpr UInt32 kAiFormSaveDialogControls = 0x48CE80;
	static constexpr UInt32 kAiFormSaveDialogControlsPatchLength = 5;
	static constexpr UInt32 kSavePluginPreWritePatch = 0x47EC2F;
	static constexpr UInt32 kSavePluginPreWriteReturn = 0x47EC34;
	static constexpr UInt32 kSavePluginPreWriteOriginalCall = 0x416E50;
	static constexpr UInt32 kWinDialogWndGetExtraDataByType = 0x442990;
	static constexpr UInt32 kRuntimeDynamicCast = 0x88DC0C;
	static constexpr UInt32 kRttiTesAiFormTypeDescriptor = 0x9EDD38;
	static constexpr UInt32 kRttiTesNpcTypeDescriptor = 0x9EA5C8;
	static constexpr UInt32 kCsFormHeapAllocate = 0x401E80;
	static constexpr UInt32 kCsFormHeapFree = 0x401EA0;
	static constexpr UInt32 kNativeBladeCompatibilityActorValue = 0x0E;
	static constexpr UInt32 kNativeBluntCompatibilityActorValue = 0x10;
	static constexpr UInt32 kDialogExtraWorkingDataOwnerOffset = 0x0C;
	static constexpr UInt32 kDialogExtraWorkingDataType = 6;
	static constexpr const char* kWeaponTypeComboPatchedProp = "BladeSeparationWeaponTypeComboPatched";
	static constexpr const char* kWeaponTypeComboOriginalProcProp = "BladeSeparationWeaponTypeComboOriginalProc";
	static constexpr const char* kWeaponTypeComboLastNativeSelectionProp = "BladeSeparationWeaponTypeComboLastNativeSelection";
	static constexpr const char* kWeaponTypeComboSidecarMarkerProp = "BladeSeparationWeaponTypeComboSidecarMarker";
	static constexpr const char* kWeaponTypeComboScrollableProp = "BladeSeparationWeaponTypeComboScrollable";
	static constexpr const char* kWeaponTypeComboApplyingStoredSelectionProp = "BladeSeparationWeaponTypeComboApplyingStoredSelection";
	static constexpr const char* kWeaponTypeComboStoredSelectionAppliedProp = "BladeSeparationWeaponTypeComboStoredSelectionApplied";
	static constexpr const char* kWeaponTypeComboRejectedProp = "BladeSeparationWeaponTypeComboRejected";
	static constexpr const char* kWeaponTypeComboBoundFormIdProp = "BladeSeparationWeaponTypeComboBoundFormId";
	static constexpr const char* kWeaponTypeParentPatchedProp = "BladeSeparationWeaponTypeParentPatched";
	static constexpr const char* kWeaponTypeParentOriginalProcProp = "BladeSeparationWeaponTypeParentOriginalProc";
	static constexpr const char* kWeaponTypeParentComboProp = "BladeSeparationWeaponTypeParentCombo";
	static constexpr const char* kNativeWeaponTypeBladeOneHand = "Blade - One Hand";
	static constexpr const char* kNativeWeaponTypeBladeTwoHand = "Blade - Two Hand";
	static constexpr const char* kNativeWeaponTypeBluntOneHand = "Blunt - One Hand";
	static constexpr const char* kNativeWeaponTypeBluntTwoHand = "Blunt - Two Hand";
	static constexpr const char* kNativeWeaponTypeStaff = "Staff";
	static constexpr const char* kNativeWeaponTypeBow = "Bow";
	static constexpr const char* kLongBladeEditorLabel = "Long Blade";
	static constexpr const char* kShortBladeEditorLabel = "Short Blade";
	static constexpr const char* kAxeEditorLabel = "Axe";
	static constexpr UINT_PTR kWeaponTypeComboPollTimerId = 0xB501;
	static constexpr UINT kWeaponTypeComboPollIntervalMs = 1000;
	static constexpr int kWeaponTypeComboMinimumVisibleRows = 9;
	static constexpr UINT kCseDeferredComboAddItem = WM_USER + 3000;
	static constexpr LRESULT kCseDeferredAddStringMarkerResult = CB_ERRSPACE - 2;
	static constexpr int kWeaponDialogTypeComboId = 1165;
	static constexpr int kNpcStatsSkillsList = 1087;
	static constexpr UInt8 kNpcStatsSidecarDefaultSkillValue = 5;
	static constexpr UInt8 kNpcStatsSidecarSortSkillIndexBase = 0xE0;
	static constexpr UInt8 kNpcStatsSidecarRowMarker = 0xB5;
	static constexpr UInt32 kTesFormRefIdOffset = 0x0C;
	static constexpr UInt32 kTesFormTypeOffset = 0x04;
	static constexpr UInt32 kTesFormEditorIdOffset = 0x10;
	static constexpr UInt32 kTesFileFileNameOffset = 0x1C;
	static constexpr UInt32 kTesFileFileIndexOffset = 0x400;
	static constexpr UInt32 kDataHandlerActiveFileOffset = 0x0E04;
	static constexpr UInt32 kDataHandlerFilesByIdOffset = 0x0E14;
	static constexpr UInt32 kBookFullNameOffset = 0x58;
	static constexpr UInt32 kBookDescriptionOffset = 0xCC;
	static constexpr UInt32 kBookFlagsOffset = 0xDC;
	static constexpr UInt32 kBoundObjectListCountOffset = 0x00;
	static constexpr UInt32 kBoundObjectListFirstOffset = 0x04;
	static constexpr UInt32 kTesObjectNextOffset = 0x30;
	static constexpr UInt32 kBsStringDataOffset = 0x00;
	static constexpr UInt32 kBsStringValueOffset = 0x04;
	static constexpr UInt8 kFormTypeBook = 0x15;
	static constexpr UInt8 kFormTypeWeapon = 0x21;
	static constexpr UInt8 kFormTypeNpc = 0x23;
	static constexpr UInt8 kBookCantBeTakenFlag = 0x02;
	static constexpr UInt32 kDefaultGeneratedWeaponTypeCarrierObjectId = 0x801;
	static constexpr UInt32 kDefaultGeneratedNpcSkillCarrierObjectId = 0x802;
	static constexpr char kWeaponTypeCarrierEditorId[] = "BSEPWeaponTypeData";
	static constexpr char kWeaponTypeCarrierFullName[] = "Blade Separation Weapon Type Data";
	static constexpr char kNpcSkillCarrierEditorId[] = "BSEPNPCSkillData";
	static constexpr char kNpcSkillCarrierFullName[] = "Blade Separation NPC Skill Data";
	static constexpr UInt32 kDefaultGeneratedNpcTrainingCarrierObjectId = 0x803;
	static constexpr char kNpcTrainingCarrierEditorId[] = "BSEPNPCTrainingData";
	static constexpr char kNpcTrainingCarrierFullName[] = "Blade Separation NPC Training Data";
	static constexpr const char* kTrainingComboPatchedProp = "BladeSeparationTrainingComboPatched";
	static constexpr const char* kTrainingComboOriginalProcProp = "BladeSeparationTrainingComboOriginalProc";
	static constexpr const char* kTrainingComboScrollableProp = "BladeSeparationTrainingComboScrollable";
	static constexpr const char* kTrainingComboApplyingSelectionProp = "BladeSeparationTrainingComboApplyingSelection";
	static constexpr const char* kTrainingComboBoundNpcFormIdProp = "BladeSeparationTrainingComboBoundNpcFormId";
	static constexpr const char* kTrainingParentPatchedProp = "BladeSeparationTrainingParentPatched";
	static constexpr const char* kTrainingParentOriginalProcProp = "BladeSeparationTrainingParentOriginalProc";
	static constexpr const char* kTrainingParentComboProp = "BladeSeparationTrainingParentCombo";
	static constexpr int kTrainingComboMinimumVisibleRows = 12;
	static constexpr int kAiTrainingCheckBox = 1542;
	static constexpr int kAiTrainingSkillCombo = 1040;
	static constexpr int kAiTrainingLevelEdit = 1061;
	static constexpr int kObjectWindowColumnType = 12;

	static constexpr UInt8 kExpectedNpcStatsInitSkillListCall[] =
	{
		0xE8, 0x5F, 0xEA, 0xFF, 0xFF
	};

	static constexpr UInt8 kExpectedNpcStatsInitSkillListOpenCall[] =
	{
		0xE8, 0xAF, 0xB1, 0xFF, 0xFF
	};

	static constexpr UInt8 kExpectedNpcStatsSkillEditCommit[] =
	{
		0x8B, 0x44, 0x24, 0x2C, 0xC7, 0x00, 0x01, 0x00, 0x00, 0x00
	};

	static constexpr UInt8 kExpectedNpcStatsSkillEditAutoCalcGate[] =
	{
		0x84, 0xC0,
		0x0F, 0x85, 0x08, 0xF1, 0xFF, 0xFF
	};

	static constexpr UInt8 kExpectedObjectWindowListViewGetDispInfo[] =
	{
		0x55, 0x8D, 0xAC, 0x24, 0x4C, 0xFF, 0xFF, 0xFF,
		0x81, 0xEC, 0x34, 0x01, 0x00, 0x00
	};

	static constexpr UInt8 kExpectedAiFormInitDialogControls[kAiFormInitDialogControlsPatchLength] =
	{
		0x53, 0x55, 0x8B, 0x2D, 0x68, 0x44, 0x92, 0x00
	};

	static constexpr UInt8 kExpectedAiFormSaveDialogControls[kAiFormSaveDialogControlsPatchLength] =
	{
		0x53, 0x8B, 0x5C, 0x24, 0x08
	};

	static constexpr UInt8 kExpectedSavePluginPreWrite[] =
	{
		0xE8, 0x1C, 0x82, 0xF9, 0xFF
	};

	static constexpr UInt8 kExpectedWeaponDialogInit[] =
	{
		0x53,
		0x8B, 0x1D, 0xC8, 0x44, 0x92, 0x00
	};

	struct SkillDefinition
	{
		UInt32 skillId;
		const char* editorId;
		const char* name;
		UInt32 fallbackActorValue;
		const char* editorDescription;
	};

	struct EditorWeaponTypeEntry
	{
		UInt32 skillId;
		const char* name;
		BladeSeparationShared::WeaponSkillKind kind;
		UInt32 nativeWeaponType;
		const char* classifierRule;
	};

	struct NpcStatsSkillRowData
	{
		UInt8* value;
		UInt8 skillIndex;
		UInt8 sidecarIndex;
		UInt8 marker;
		UInt8 padding;
	};

	static const SkillDefinition kSidecarSkills[] =
	{
		{ BladeSeparationShared::kLongSkillId, "BSLong", kLongBladeEditorLabel, kNativeBladeCompatibilityActorValue, "Runtime sidecar for long blades and full-length one-handed blades." },
		{ BladeSeparationShared::kShortSkillId, "BSShort", kShortBladeEditorLabel, kNativeBladeCompatibilityActorValue, "Runtime sidecar for daggers, knives, tantos, wakizashis, and short swords." },
		{ BladeSeparationShared::kAxeSkillId, "BSAxe", kAxeEditorLabel, kNativeBluntCompatibilityActorValue, "Runtime sidecar for axe-tokened one-handed and two-handed blunt weapons." },
	};

	static const EditorWeaponTypeEntry kEditorSeparatedWeaponTypes[] =
	{
		{ BladeSeparationShared::kLongSkillId, kLongBladeEditorLabel, BladeSeparationShared::kWeaponSkill_Long, BladeSeparationShared::kWeaponType_BladeOneHand, "two-handed blades and one-handed non-short blades" },
		{ BladeSeparationShared::kShortSkillId, kShortBladeEditorLabel, BladeSeparationShared::kWeaponSkill_Short, BladeSeparationShared::kWeaponType_BladeOneHand, "one-handed blades with a short-weapon token" },
		{ BladeSeparationShared::kAxeSkillId, kAxeEditorLabel, BladeSeparationShared::kWeaponSkill_Axe, BladeSeparationShared::kWeaponType_BluntOneHand, "one-handed or two-handed blunt weapons with an axe token" },
	};

	static constexpr UInt32 kSidecarSkillCount = sizeof(kSidecarSkills) / sizeof(kSidecarSkills[0]);

	static UINT_PTR g_weaponTypeComboPollTimer = 0;
	static bool g_insideWeaponTypeComboPatch = false;
	static BladeSeparationShared::WeaponTypeSidecarStore g_weaponTypeStore;
	static BladeSeparationShared::NpcSkillSidecarStore g_npcSkillStore;
	static BladeSeparationShared::NpcTrainingSidecarStore g_npcTrainingStore;
	static bool g_weaponTypeStoreLoaded = false;
	static bool g_npcSkillStoreLoaded = false;
	static bool g_npcTrainingStoreLoaded = false;
	static bool g_loggedWeaponTypeNoForm = false;
	static bool g_loggedWeaponTypeNoActivePlugin = false;
	static bool g_loggedLegacySelfWeaponTypeFallback = false;
	static bool g_loggedLegacySelfWeaponTypeAmbiguous = false;
	static bool g_loggedWeaponTypeCarrierStoreFull = false;
	static bool g_loggedNpcSkillNoActivePlugin = false;
	static bool g_loggedNpcTrainingNoForm = false;
	static bool g_loggedNpcTrainingNoActivePlugin = false;
	static bool g_loggedLegacySelfNpcSkillFallback = false;
	static bool g_loggedLegacySelfNpcSkillAmbiguous = false;
	static bool g_loggedNpcSkillCarrierStoreFull = false;
	static bool g_loggedNpcTrainingCarrierStoreFull = false;
	static bool g_loggedNpcTrainingBadDialogOwner = false;
	static bool g_loggedNpcStatsScrollbar = false;
	static UInt8 g_npcStatsSkillValues[kSidecarSkillCount] =
	{
		kNpcStatsSidecarDefaultSkillValue,
		kNpcStatsSidecarDefaultSkillValue,
		kNpcStatsSidecarDefaultSkillValue
	};
	static UInt32 g_activeNpcStatsFormId = 0;
	static HWND g_activeNpcStatsDialog = nullptr;
	static HWND g_activeNpcStatsSkillsList = nullptr;

	typedef void* (__stdcall* LookupEditorFormByEditorIDFn)(const char* editorId);
	typedef void* (__cdecl* LookupEditorFormByFormIDFn)(UInt32 formId);
	typedef void* (__cdecl* CreateEditorFormFn)(UInt8 formType);
	typedef bool(__thiscall* SetEditorIDFn)(void* form, const char* editorId);
	typedef void(__thiscall* SetFormIDFn)(void* form, UInt32 formId, bool reserve);
	typedef bool(__thiscall* SetBSStringFn)(void* bsString, const char* value, SInt16 size);
	typedef void(__thiscall* AddObjectFn)(void* objectList, void* object);
	typedef void* (__thiscall* GetOverrideFileFn)(void* form, int index);
	typedef void* (__cdecl* CsFormHeapAllocateFn)(UInt32 size);
	typedef void(__cdecl* CsFormHeapFreeFn)(void* ptr);
	using WeaponDialogInitFn = int(__thiscall*)(void* weapon, HWND dialog);
	using ObjectWindowListViewGetDispInfoFn = void(__thiscall*)(void* treeEntry, NMLVDISPINFOA* data);
	using NpcStatsPopulateFn = int(__thiscall*)(void* npc, HWND dialog);
	using NpcStatsPopulateChainedFn = int(__fastcall*)(void* npc, void*, HWND dialog);
	using NpcStatsInitSkillListFn = LRESULT(__thiscall*)(void* npc, HWND list);
	using NpcStatsInitSkillListChainedFn = LRESULT(__fastcall*)(void* npc, void*, HWND list);
	using AiFormInitDialogControlsFn = int(__thiscall*)(void* aiForm, HWND dialog);
	using AiFormSaveDialogControlsFn = int(__thiscall*)(void* aiForm, HWND dialog);
	using WinDialogWndGetExtraDataByTypeFn = void*(__cdecl*)(HWND dialog, UInt32 type);
	using RuntimeDynamicCastFn = void*(__cdecl*)(void* source, UInt32 unknown, void* sourceType, void* targetType, UInt32 flags);

	static WeaponDialogInitFn g_weaponDialogInitOriginal = nullptr;
	static UInt8 g_weaponDialogInitTrampoline[kWeaponDialogInitPatchLength + 5] = {};
	static ObjectWindowListViewGetDispInfoFn g_objectWindowListViewGetDispInfoOriginal = nullptr;
	static UInt8 g_objectWindowListViewGetDispInfoTrampoline[kObjectWindowListViewGetDispInfoPatchLength + 5] = {};
	static AiFormInitDialogControlsFn g_aiFormInitDialogControlsOriginal = nullptr;
	static AiFormSaveDialogControlsFn g_aiFormSaveDialogControlsOriginal = nullptr;
	static UInt8 g_aiFormInitDialogControlsTrampoline[kAiFormInitDialogControlsPatchLength + 5] = {};
	static UInt8 g_aiFormSaveDialogControlsTrampoline[kAiFormSaveDialogControlsPatchLength + 5] = {};
	static UInt32 g_npcStatsPopulateOriginalTarget = kNpcStatsPopulateOriginal;
	static bool g_npcStatsPopulateOriginalIsFastcall = false;
	static UInt32 g_npcStatsInitSkillListOriginalTarget = kNpcStatsInitSkillListOriginal;
	static bool g_npcStatsInitSkillListOriginalIsFastcall = false;
	static UInt32 g_npcStatsSkillEditAutoCalcGateChainTarget = 0;
	static UInt32 g_npcStatsSkillEditCommitChainTarget = 0;
	static UInt32 g_savePluginPreWriteChainTarget = 0;

	static bool ReadComboString(HWND combo, int index, std::string& value);
	static WNDPROC GetOriginalWeaponTypeComboProc(HWND combo);
	static void PersistWeaponTypeSelection(HWND combo);
	static bool ApplyStoredWeaponTypeSelection(HWND combo);
	static bool PrepareNativeWeaponTypeSelectionForSave(HWND combo, int* restoreSelection);
	static void RestoreWeaponTypeSelectionAfterSave(HWND combo, int restoreSelection);
	static void PatchWeaponTypeParentDialog(HWND combo);
	static void PatchWeaponTypeCombo(HWND combo);
	static void RememberNativeWeaponTypeSelection(HWND combo);
	static void* LookupWeaponFormByEditorId(const char* editorId);
	static void* LookupUniqueWeaponFormByEditorId(const char* editorId);
	static void* LookupNpcFormByEditorId(const char* editorId);
	static void* LookupUniqueNpcFormByEditorId(const char* editorId);
	static bool IsNpcStatsBladeSeparationListRow(HWND list, int row);
	static void LoadNpcTrainingStoreOnce();
	static bool SaveNpcTrainingStore();
	static bool FlushPendingSidecarStoresBeforePluginSave();
	static void SelectTrainingComboSidecarSkill(HWND combo, UInt32 skillId);
	static void RestoreTrainingComboSidecarIfAffixed(HWND combo, const char* source);
	static void ApplyNpcAiTrainingSidecarSelection(void* aiForm, HWND dialog);
	static void SaveNpcAiTrainingSidecarSelection(void* aiForm, HWND dialog);

	static BOOL CALLBACK InvalidateVisibleListViews(HWND hwnd, LPARAM)
	{
		if (!IsWindowVisible(hwnd))
			return TRUE;

		char className[32] = {};
		if (GetClassNameA(hwnd, className, sizeof(className)) && !_stricmp(className, "SysListView32"))
		{
			InvalidateRect(hwnd, nullptr, FALSE);
			UpdateWindow(hwnd);
			return TRUE;
		}

		EnumChildWindows(hwnd, InvalidateVisibleListViews, 0);
		return TRUE;
	}

	static void RefreshVisibleListViews()
	{
		EnumThreadWindows(GetCurrentThreadId(), InvalidateVisibleListViews, 0);
	}

	static void RefreshWeaponTypeComboDisplay(HWND combo)
	{
		if (!combo)
			return;

		RedrawWindow(combo, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
		if (HWND parent = GetParent(combo))
			RedrawWindow(parent, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}

	static bool IsNativeBladeCompatibilityActorValue(UInt32 actorValue)
	{
		return actorValue == kNativeBladeCompatibilityActorValue;
	}

	static void WeaponTypeStoreLog(void*, const char* message)
	{
		_MESSAGE("BladeSeparation CS: %s", message ? message : "");
	}

	static void NpcSkillStoreLog(void*, const char* message)
	{
		_MESSAGE("BladeSeparation CS: %s", message ? message : "");
	}

	static void NpcTrainingStoreLog(void*, const char* message)
	{
		_MESSAGE("BladeSeparation CS: %s", message ? message : "");
	}

	static void* GetEditorDataHandler()
	{
		void** dataHandler = reinterpret_cast<void**>(0x00A0E064);
		return dataHandler ? *dataHandler : nullptr;
	}

	static void* GetActiveEditorFile()
	{
		void* dataHandler = GetEditorDataHandler();
		return dataHandler ? *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(dataHandler) + kDataHandlerActiveFileOffset) : nullptr;
	}

	static const char* GetEditorFileName(const void* file)
	{
		return file ? reinterpret_cast<const char*>(reinterpret_cast<const UInt8*>(file) + kTesFileFileNameOffset) : nullptr;
	}

	static UInt8 GetEditorFileIndex(const void* file)
	{
		return file ? *reinterpret_cast<const UInt8*>(reinterpret_cast<const UInt8*>(file) + kTesFileFileIndexOffset) : 0xFF;
	}

	static void* LookupEditorFileByName(const char* fileName)
	{
		if (!fileName || !fileName[0])
			return nullptr;

		void* dataHandler = GetEditorDataHandler();
		if (!dataHandler)
			return nullptr;

		void** filesById = reinterpret_cast<void**>(reinterpret_cast<UInt8*>(dataHandler) + kDataHandlerFilesByIdOffset);
		for (UInt32 i = 0; i < 0xFF; ++i)
		{
			void* file = filesById[i];
			const char* candidate = GetEditorFileName(file);
			if (candidate && !_stricmp(candidate, fileName))
				return file;
		}

		return nullptr;
	}

	static const char* ReadBSStringValue(const void* bsString)
	{
		if (!bsString)
			return nullptr;

		return *reinterpret_cast<char* const*>(reinterpret_cast<const UInt8*>(bsString) + kBsStringDataOffset);
	}

	static bool SetBSStringValue(void* bsString, const char* value)
	{
		static const SetBSStringFn setString = reinterpret_cast<SetBSStringFn>(0x004051E0);
		return bsString && setString && setString(bsString, value ? value : "", 0);
	}

	static UInt8 ReadEditorFormType(const void* form)
	{
		return form ? *reinterpret_cast<const UInt8*>(reinterpret_cast<const UInt8*>(form) + kTesFormTypeOffset) : 0;
	}

	static UInt8 TryReadEditorFormType(const void* form)
	{
		if (!form)
			return 0;

		const void* address = reinterpret_cast<const UInt8*>(form) + kTesFormTypeOffset;
		if (IsBadReadPtr(address, sizeof(UInt8)))
			return 0;

		return ReadEditorFormType(form);
	}

	static UInt32 ReadEditorFormId(const void* form)
	{
		return form ? *reinterpret_cast<const UInt32*>(reinterpret_cast<const UInt8*>(form) + kTesFormRefIdOffset) : 0;
	}

	static const char* ReadEditorFormEditorId(const void* form)
	{
		return form ? ReadBSStringValue(reinterpret_cast<const UInt8*>(form) + kTesFormEditorIdOffset) : nullptr;
	}

	static void* LookupEditorFormById(UInt32 formId)
	{
		static const LookupEditorFormByFormIDFn lookupByFormId = reinterpret_cast<LookupEditorFormByFormIDFn>(0x00495EF0);
		return lookupByFormId ? lookupByFormId(formId) : nullptr;
	}

	static void MarkEditorFormFromActiveFile(void* form, bool active)
	{
		if (!form)
			return;

		void*** vtbl = reinterpret_cast<void***>(form);
		if (!vtbl || !*vtbl)
			return;

		typedef void(__thiscall* SetFromActiveFileFn)(void*, bool);
		SetFromActiveFileFn setFromActiveFile = reinterpret_cast<SetFromActiveFileFn>((*vtbl)[0x94 / sizeof(void*)]);
		if (setFromActiveFile)
			setFromActiveFile(form, active);
	}

	static void* GetEditorFormOverrideFile(void* form, int index)
	{
		static const GetOverrideFileFn getOverrideFile = reinterpret_cast<GetOverrideFileFn>(0x00495FE0);
		return form && getOverrideFile ? getOverrideFile(form, index) : nullptr;
	}

	static void* LookupWeaponTypeCarrierBook()
	{
		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(kWeaponTypeCarrierEditorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeBook ? form : nullptr;
	}

	static void* FindOrCreateWeaponTypeCarrierBook()
	{
		if (void* existing = LookupWeaponTypeCarrierBook())
			return existing;

		void* dataHandler = GetEditorDataHandler();
		if (!dataHandler || !GetActiveEditorFile())
		{
			_WARNING("BladeSeparation CS: cannot create embedded weapon Type carrier without an active plugin");
			return nullptr;
		}

		static const CreateEditorFormFn createForm = reinterpret_cast<CreateEditorFormFn>(0x004793F0);
		static const SetEditorIDFn setEditorId = reinterpret_cast<SetEditorIDFn>(0x00497670);
		static const AddObjectFn addObject = reinterpret_cast<AddObjectFn>(0x005135F0);
		void* book = createForm ? createForm(kFormTypeBook) : nullptr;
		if (!book)
			return nullptr;

		if (!ReadEditorFormId(book))
		{
			const UInt32 nextFormId = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(dataHandler) + 0x0E00);
			static const SetFormIDFn setFormId = reinterpret_cast<SetFormIDFn>(0x00497E50);
			if (setFormId)
				setFormId(book, nextFormId ? nextFormId : kDefaultGeneratedWeaponTypeCarrierObjectId, true);
		}

		if (setEditorId)
			setEditorId(book, kWeaponTypeCarrierEditorId);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookFullNameOffset + kBsStringValueOffset, kWeaponTypeCarrierFullName);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookDescriptionOffset + kBsStringValueOffset, BladeSeparationShared::WeaponTypeSidecarPayloadCodec::Header());
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(book) + kBookFlagsOffset) |= kBookCantBeTakenFlag;

		void* objectList = *reinterpret_cast<void**>(dataHandler);
		if (addObject && objectList)
			addObject(objectList, book);

		MarkEditorFormFromActiveFile(book, true);
		return book;
	}

	static const char* ReadEmbeddedWeaponTypePayload(bool* found)
	{
		void* carrier = LookupWeaponTypeCarrierBook();
		if (found)
			*found = carrier != nullptr;
		if (!carrier)
			return nullptr;

		return ReadBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset);
	}

	static bool WriteEmbeddedWeaponTypePayload(const char* payload)
	{
		void* carrier = FindOrCreateWeaponTypeCarrierBook();
		if (!carrier)
			return false;

		SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookFullNameOffset + kBsStringValueOffset, kWeaponTypeCarrierFullName);
		const bool written = SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset, payload ? payload : "");
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(carrier) + kBookFlagsOffset) |= kBookCantBeTakenFlag;
		MarkEditorFormFromActiveFile(carrier, true);
		return written;
	}

	static void* LookupNpcSkillCarrierBook()
	{
		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(kNpcSkillCarrierEditorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeBook ? form : nullptr;
	}

	static void* FindOrCreateNpcSkillCarrierBook()
	{
		if (void* existing = LookupNpcSkillCarrierBook())
			return existing;

		void* dataHandler = GetEditorDataHandler();
		if (!dataHandler || !GetActiveEditorFile())
		{
			_WARNING("BladeSeparation CS: cannot create embedded NPC skill carrier without an active plugin");
			return nullptr;
		}

		static const CreateEditorFormFn createForm = reinterpret_cast<CreateEditorFormFn>(0x004793F0);
		static const SetEditorIDFn setEditorId = reinterpret_cast<SetEditorIDFn>(0x00497670);
		static const AddObjectFn addObject = reinterpret_cast<AddObjectFn>(0x005135F0);
		void* book = createForm ? createForm(kFormTypeBook) : nullptr;
		if (!book)
			return nullptr;

		if (!ReadEditorFormId(book))
		{
			const UInt32 nextFormId = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(dataHandler) + 0x0E00);
			static const SetFormIDFn setFormId = reinterpret_cast<SetFormIDFn>(0x00497E50);
			if (setFormId)
				setFormId(book, nextFormId ? nextFormId : kDefaultGeneratedNpcSkillCarrierObjectId, true);
		}

		if (setEditorId)
			setEditorId(book, kNpcSkillCarrierEditorId);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookFullNameOffset + kBsStringValueOffset, kNpcSkillCarrierFullName);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookDescriptionOffset + kBsStringValueOffset, BladeSeparationShared::NpcSkillSidecarPayloadCodec::Header());
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(book) + kBookFlagsOffset) |= kBookCantBeTakenFlag;

		void* objectList = *reinterpret_cast<void**>(dataHandler);
		if (addObject && objectList)
			addObject(objectList, book);

		MarkEditorFormFromActiveFile(book, true);
		return book;
	}

	static const char* ReadEmbeddedNpcSkillPayload(bool* found)
	{
		void* carrier = LookupNpcSkillCarrierBook();
		if (found)
			*found = carrier != nullptr;
		if (!carrier)
			return nullptr;

		return ReadBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset);
	}

	static bool WriteEmbeddedNpcSkillPayload(const char* payload)
	{
		void* carrier = FindOrCreateNpcSkillCarrierBook();
		if (!carrier)
			return false;

		SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookFullNameOffset + kBsStringValueOffset, kNpcSkillCarrierFullName);
		const bool written = SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset, payload ? payload : "");
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(carrier) + kBookFlagsOffset) |= kBookCantBeTakenFlag;
		MarkEditorFormFromActiveFile(carrier, true);
		return written;
	}

	static void* LookupNpcTrainingCarrierBook()
	{
		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(kNpcTrainingCarrierEditorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeBook ? form : nullptr;
	}

	static void* FindOrCreateNpcTrainingCarrierBook()
	{
		if (void* existing = LookupNpcTrainingCarrierBook())
			return existing;

		void* dataHandler = GetEditorDataHandler();
		if (!dataHandler || !GetActiveEditorFile())
		{
			_WARNING("BladeSeparation CS: cannot create embedded NPC training carrier without an active plugin");
			return nullptr;
		}

		static const CreateEditorFormFn createForm = reinterpret_cast<CreateEditorFormFn>(0x004793F0);
		static const SetEditorIDFn setEditorId = reinterpret_cast<SetEditorIDFn>(0x00497670);
		static const AddObjectFn addObject = reinterpret_cast<AddObjectFn>(0x005135F0);
		void* book = createForm ? createForm(kFormTypeBook) : nullptr;
		if (!book)
			return nullptr;

		if (!ReadEditorFormId(book))
		{
			const UInt32 nextFormId = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(dataHandler) + 0x0E00);
			static const SetFormIDFn setFormId = reinterpret_cast<SetFormIDFn>(0x00497E50);
			if (setFormId)
				setFormId(book, nextFormId ? nextFormId : kDefaultGeneratedNpcTrainingCarrierObjectId, true);
		}

		if (setEditorId)
			setEditorId(book, kNpcTrainingCarrierEditorId);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookFullNameOffset + kBsStringValueOffset, kNpcTrainingCarrierFullName);
		SetBSStringValue(reinterpret_cast<UInt8*>(book) + kBookDescriptionOffset + kBsStringValueOffset, BladeSeparationShared::NpcTrainingSidecarPayloadCodec::Header());
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(book) + kBookFlagsOffset) |= kBookCantBeTakenFlag;

		void* objectList = *reinterpret_cast<void**>(dataHandler);
		if (addObject && objectList)
			addObject(objectList, book);

		MarkEditorFormFromActiveFile(book, true);
		return book;
	}

	static const char* ReadEmbeddedNpcTrainingPayload(bool* found)
	{
		void* carrier = LookupNpcTrainingCarrierBook();
		if (found)
			*found = carrier != nullptr;
		if (!carrier)
			return nullptr;

		return ReadBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset);
	}

	static bool WriteEmbeddedNpcTrainingPayload(const char* payload)
	{
		void* carrier = FindOrCreateNpcTrainingCarrierBook();
		if (!carrier)
			return false;

		SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookFullNameOffset + kBsStringValueOffset, kNpcTrainingCarrierFullName);
		const bool written = SetBSStringValue(reinterpret_cast<UInt8*>(carrier) + kBookDescriptionOffset + kBsStringValueOffset, payload ? payload : "");
		*reinterpret_cast<UInt8*>(reinterpret_cast<UInt8*>(carrier) + kBookFlagsOffset) |= kBookCantBeTakenFlag;
		MarkEditorFormFromActiveFile(carrier, true);
		return written;
	}

	static bool ResolveWeaponTypeKeyToEditorFormID(const BladeSeparationShared::WeaponTypeSidecarKey& key, UInt32* formId)
	{
		if (formId)
			*formId = 0;

		const bool self = !_stricmp(key.sourceMod, "$SELF");
		void* file = self ? GetActiveEditorFile() : LookupEditorFileByName(key.sourceMod);
		const UInt8 fileIndex = GetEditorFileIndex(file);
		if (!file || fileIndex == 0xFF)
			return false;

		const UInt32 candidateFormId = (static_cast<UInt32>(fileIndex) << 24) | (key.objectId & 0x00FFFFFF);
		static const LookupEditorFormByFormIDFn lookupByFormId = reinterpret_cast<LookupEditorFormByFormIDFn>(0x00495EF0);
		void* form = lookupByFormId ? lookupByFormId(candidateFormId) : nullptr;
		if (ReadEditorFormType(form) == kFormTypeWeapon)
		{
			if (formId)
				*formId = candidateFormId;
			return true;
		}

		if (self && key.editorId[0])
		{
			form = LookupUniqueWeaponFormByEditorId(key.editorId);
			if (ReadEditorFormType(form) == kFormTypeWeapon)
			{
				if (formId)
					*formId = ReadEditorFormId(form);
				if (!g_loggedLegacySelfWeaponTypeFallback)
				{
					_MESSAGE("BladeSeparation CS: resolved legacy $SELF weapon Type sidecar row by editor ID %s", key.editorId);
					g_loggedLegacySelfWeaponTypeFallback = true;
				}
				return true;
			}
		}

		if (formId)
			*formId = 0;
		return false;
	}

	static bool MakeWeaponTypeKeyFromEditorForm(void* form, BladeSeparationShared::WeaponTypeSidecarKey* key)
	{
		if (!key || ReadEditorFormType(form) != kFormTypeWeapon)
			return false;

		void* activeFile = GetActiveEditorFile();
		void* sourceFile = GetEditorFormOverrideFile(form, 0);
		std::memset(key, 0, sizeof(*key));
		// Existing override records keep the original source mod as their sidecar
		// identity so the carrier does not require a duplicate native WEAP record.
		if (sourceFile && sourceFile != activeFile)
			_snprintf_s(key->sourceMod, sizeof(key->sourceMod), _TRUNCATE, "%s", GetEditorFileName(sourceFile) ? GetEditorFileName(sourceFile) : "$SELF");
		else
			_snprintf_s(key->sourceMod, sizeof(key->sourceMod), _TRUNCATE, "$SELF");

		key->objectId = ReadEditorFormId(form) & 0x00FFFFFF;
		_snprintf_s(key->editorId, sizeof(key->editorId), _TRUNCATE, "%s", ReadEditorFormEditorId(form) ? ReadEditorFormEditorId(form) : "");
		return key->objectId != 0;
	}

	static bool MakeWeaponTypeKeyFromEditorFormID(UInt32 formId, BladeSeparationShared::WeaponTypeSidecarKey* key)
	{
		static const LookupEditorFormByFormIDFn lookupByFormId = reinterpret_cast<LookupEditorFormByFormIDFn>(0x00495EF0);
		void* form = lookupByFormId ? lookupByFormId(formId) : nullptr;
		return MakeWeaponTypeKeyFromEditorForm(form, key);
	}

	static bool ResolveNpcSkillKeyToEditorFormID(const BladeSeparationShared::NpcSkillSidecarKey& key, UInt32* formId)
	{
		if (formId)
			*formId = 0;

		const bool self = !_stricmp(key.sourceMod, "$SELF");
		void* file = self ? GetActiveEditorFile() : LookupEditorFileByName(key.sourceMod);
		const UInt8 fileIndex = GetEditorFileIndex(file);
		if (!file || fileIndex == 0xFF)
			return false;

		const UInt32 candidateFormId = (static_cast<UInt32>(fileIndex) << 24) | (key.objectId & 0x00FFFFFF);
		static const LookupEditorFormByFormIDFn lookupByFormId = reinterpret_cast<LookupEditorFormByFormIDFn>(0x00495EF0);
		void* form = lookupByFormId ? lookupByFormId(candidateFormId) : nullptr;
		if (ReadEditorFormType(form) == kFormTypeNpc)
		{
			if (formId)
				*formId = candidateFormId;
			return true;
		}

		if (self && key.editorId[0])
		{
			form = LookupUniqueNpcFormByEditorId(key.editorId);
			if (ReadEditorFormType(form) == kFormTypeNpc)
			{
				if (formId)
					*formId = ReadEditorFormId(form);
				if (!g_loggedLegacySelfNpcSkillFallback)
				{
					_MESSAGE("BladeSeparation CS: resolved legacy $SELF NPC skill sidecar row by editor ID %s", key.editorId);
					g_loggedLegacySelfNpcSkillFallback = true;
				}
				return true;
			}
		}

		if (formId)
			*formId = 0;
		return false;
	}

	static bool MakeNpcSkillKeyFromEditorForm(void* form, BladeSeparationShared::NpcSkillSidecarKey* key)
	{
		if (!key || ReadEditorFormType(form) != kFormTypeNpc)
			return false;

		void* activeFile = GetActiveEditorFile();
		void* sourceFile = GetEditorFormOverrideFile(form, 0);
		std::memset(key, 0, sizeof(*key));
		if (sourceFile && sourceFile != activeFile)
			_snprintf_s(key->sourceMod, sizeof(key->sourceMod), _TRUNCATE, "%s", GetEditorFileName(sourceFile) ? GetEditorFileName(sourceFile) : "$SELF");
		else
			_snprintf_s(key->sourceMod, sizeof(key->sourceMod), _TRUNCATE, "$SELF");

		key->objectId = ReadEditorFormId(form) & 0x00FFFFFF;
		_snprintf_s(key->editorId, sizeof(key->editorId), _TRUNCATE, "%s", ReadEditorFormEditorId(form) ? ReadEditorFormEditorId(form) : "");
		return key->objectId != 0;
	}

	static bool MakeNpcSkillKeyFromEditorFormID(UInt32 formId, BladeSeparationShared::NpcSkillSidecarKey* key)
	{
		static const LookupEditorFormByFormIDFn lookupByFormId = reinterpret_cast<LookupEditorFormByFormIDFn>(0x00495EF0);
		void* form = lookupByFormId ? lookupByFormId(formId) : nullptr;
		return MakeNpcSkillKeyFromEditorForm(form, key);
	}

	static void LoadWeaponTypeStoreOnce()
	{
		if (g_weaponTypeStoreLoaded)
			return;

		g_weaponTypeStore.Clear();
		bool found = false;
		const char* payload = ReadEmbeddedWeaponTypePayload(&found);
		if (!found || !payload)
		{
			g_weaponTypeStoreLoaded = true;
			return;
		}

		std::vector<BladeSeparationShared::WeaponTypeSidecarPayloadRow> rows;
		BladeSeparationShared::WeaponTypeSidecarPayloadStats stats = {};
		stats.carrierRecords = 1;
		if (!BladeSeparationShared::WeaponTypeSidecarPayloadCodec::Parse(payload, rows, &stats, WeaponTypeStoreLog, nullptr))
		{
			_WARNING("BladeSeparation CS: embedded weapon Type carrier found but payload parse failed");
			g_weaponTypeStoreLoaded = true;
			return;
		}

		for (size_t i = 0; i < rows.size(); ++i)
		{
			UInt32 formId = 0;
			if (!ResolveWeaponTypeKeyToEditorFormID(rows[i].key, &formId) || !formId)
			{
				++stats.unresolvedEntries;
				continue;
			}

			if (g_weaponTypeStore.SetLoaded(formId, rows[i].kind))
			{
				++stats.resolvedEntries;
			}
			else
			{
				++stats.unresolvedEntries;
				if (!g_loggedWeaponTypeCarrierStoreFull)
				{
					_WARNING("BladeSeparation CS: embedded weapon Type carrier row could not be stored form=%08X kind=%u", formId, rows[i].kind);
					g_loggedWeaponTypeCarrierStoreFull = true;
				}
			}
		}

		g_weaponTypeStore.ClearDirty();
		g_weaponTypeStoreLoaded = true;
		_MESSAGE("BladeSeparation CS: embedded weapon Type carrier load parsed=%u resolved=%u unresolved=%u skipped=%u",
			stats.parsedEntries,
			stats.resolvedEntries,
			stats.unresolvedEntries,
			stats.skippedRows);
	}

	static bool SaveWeaponTypeStore()
	{
		std::vector<BladeSeparationShared::WeaponTypeSidecarPayloadRow> rows;
		UInt32 skipped = 0;
		for (UInt32 i = 0; i < g_weaponTypeStore.Count(); ++i)
		{
			const BladeSeparationShared::WeaponTypeSidecarEntry& entry = g_weaponTypeStore.EntryAt(i);
			if (!entry.formId)
				continue;

			BladeSeparationShared::WeaponTypeSidecarPayloadRow row = {};
			if (!MakeWeaponTypeKeyFromEditorFormID(entry.formId, &row.key))
			{
				++skipped;
				continue;
			}

			row.kind = entry.kind;
			rows.push_back(row);
		}

		const std::string payload = BladeSeparationShared::WeaponTypeSidecarPayloadCodec::Write(rows);
		if (!WriteEmbeddedWeaponTypePayload(payload.c_str()))
		{
			_WARNING("BladeSeparation CS: failed to save weapon Type sidecar carrier entries=%u skipped=%u",
				static_cast<unsigned int>(rows.size()),
				skipped);
			return false;
		}

		g_weaponTypeStore.ClearDirty();
		return true;
	}

	static void LoadNpcSkillStoreOnce()
	{
		if (g_npcSkillStoreLoaded)
			return;

		g_npcSkillStore.Clear();
		bool found = false;
		const char* payload = ReadEmbeddedNpcSkillPayload(&found);
		if (!found || !payload)
		{
			g_npcSkillStoreLoaded = true;
			return;
		}

		std::vector<BladeSeparationShared::NpcSkillSidecarPayloadRow> rows;
		BladeSeparationShared::NpcSkillSidecarPayloadStats stats = {};
		stats.carrierRecords = 1;
		if (!BladeSeparationShared::NpcSkillSidecarPayloadCodec::Parse(payload, rows, &stats, NpcSkillStoreLog, nullptr))
		{
			_WARNING("BladeSeparation CS: embedded NPC skill carrier found but payload parse failed");
			g_npcSkillStoreLoaded = true;
			return;
		}

		for (size_t i = 0; i < rows.size(); ++i)
		{
			UInt32 formId = 0;
			if (!ResolveNpcSkillKeyToEditorFormID(rows[i].key, &formId) || !formId)
			{
				++stats.unresolvedEntries;
				continue;
			}

			if (g_npcSkillStore.SetLoaded(formId, rows[i].skillId, rows[i].level, rows[i].progress, rows[i].levelUps))
			{
				++stats.resolvedEntries;
			}
			else
			{
				++stats.unresolvedEntries;
				if (!g_loggedNpcSkillCarrierStoreFull)
				{
					_WARNING("BladeSeparation CS: embedded NPC skill carrier row could not be stored form=%08X skill=%u",
						formId,
						rows[i].skillId);
					g_loggedNpcSkillCarrierStoreFull = true;
				}
			}
		}

		g_npcSkillStore.ClearDirty();
		g_npcSkillStoreLoaded = true;
		_MESSAGE("BladeSeparation CS: embedded NPC skill carrier load parsed=%u resolved=%u unresolved=%u skipped=%u",
			stats.parsedEntries,
			stats.resolvedEntries,
			stats.unresolvedEntries,
			stats.skippedRows);
	}

	static bool SaveNpcSkillStore()
	{
		std::vector<BladeSeparationShared::NpcSkillSidecarPayloadRow> rows;
		UInt32 skipped = 0;
		for (UInt32 i = 0; i < g_npcSkillStore.Count(); ++i)
		{
			const BladeSeparationShared::NpcSkillSidecarEntry& entry = g_npcSkillStore.EntryAt(i);
			if (!entry.formId)
				continue;

			BladeSeparationShared::NpcSkillSidecarPayloadRow row = {};
			if (!MakeNpcSkillKeyFromEditorFormID(entry.formId, &row.key))
			{
				++skipped;
				continue;
			}

			row.skillId = entry.skillId;
			row.level = entry.level;
			row.progress = entry.progress;
			row.levelUps = entry.levelUps;
			rows.push_back(row);
		}

		const std::string payload = BladeSeparationShared::NpcSkillSidecarPayloadCodec::Write(rows);
		if (!WriteEmbeddedNpcSkillPayload(payload.c_str()))
		{
			_WARNING("BladeSeparation CS: failed to save NPC skill sidecar carrier entries=%u skipped=%u",
				static_cast<unsigned int>(rows.size()),
				skipped);
			return false;
		}

		g_npcSkillStore.ClearDirty();
		return true;
	}

	static void LoadNpcTrainingStoreOnce()
	{
		if (g_npcTrainingStoreLoaded)
			return;

		g_npcTrainingStore.Clear();
		bool found = false;
		const char* payload = ReadEmbeddedNpcTrainingPayload(&found);
		if (!found || !payload)
		{
			g_npcTrainingStoreLoaded = true;
			return;
		}

		std::vector<BladeSeparationShared::NpcTrainingSidecarPayloadRow> rows;
		BladeSeparationShared::NpcSkillSidecarPayloadStats stats = {};
		stats.carrierRecords = 1;
		if (!BladeSeparationShared::NpcTrainingSidecarPayloadCodec::Parse(payload, rows, &stats, NpcTrainingStoreLog, nullptr))
		{
			_WARNING("BladeSeparation CS: embedded NPC training carrier found but payload parse failed");
			g_npcTrainingStoreLoaded = true;
			return;
		}

		for (size_t i = 0; i < rows.size(); ++i)
		{
			UInt32 formId = 0;
			if (!ResolveNpcSkillKeyToEditorFormID(rows[i].key, &formId) || !formId)
			{
				++stats.unresolvedEntries;
				continue;
			}

			if (g_npcTrainingStore.SetLoaded(formId, rows[i].skillId))
			{
				++stats.resolvedEntries;
			}
			else
			{
				++stats.unresolvedEntries;
				if (!g_loggedNpcTrainingCarrierStoreFull)
				{
					_WARNING("BladeSeparation CS: embedded NPC training carrier row could not be stored form=%08X skill=%u",
						formId,
						rows[i].skillId);
					g_loggedNpcTrainingCarrierStoreFull = true;
				}
			}
		}

		g_npcTrainingStore.ClearDirty();
		g_npcTrainingStoreLoaded = true;
		_MESSAGE("BladeSeparation CS: embedded NPC training carrier load parsed=%u resolved=%u unresolved=%u skipped=%u",
			stats.parsedEntries,
			stats.resolvedEntries,
			stats.unresolvedEntries,
			stats.skippedRows);
	}

	static bool SaveNpcTrainingStore()
	{
		std::vector<BladeSeparationShared::NpcTrainingSidecarPayloadRow> rows;
		UInt32 skipped = 0;
		for (UInt32 i = 0; i < g_npcTrainingStore.Count(); ++i)
		{
			const BladeSeparationShared::NpcTrainingSidecarEntry& entry = g_npcTrainingStore.EntryAt(i);
			if (!entry.formId)
				continue;

			BladeSeparationShared::NpcTrainingSidecarPayloadRow row = {};
			if (!MakeNpcSkillKeyFromEditorFormID(entry.formId, &row.key))
			{
				++skipped;
				continue;
			}

			row.skillId = entry.skillId;
			rows.push_back(row);
		}

		const std::string payload = BladeSeparationShared::NpcTrainingSidecarPayloadCodec::Write(rows);
		if (!WriteEmbeddedNpcTrainingPayload(payload.c_str()))
		{
			_WARNING("BladeSeparation CS: failed to save NPC training sidecar carrier entries=%u skipped=%u",
				static_cast<unsigned int>(rows.size()),
				skipped);
			return false;
		}

		g_npcTrainingStore.ClearDirty();
		return true;
	}

	static bool FlushPendingSidecarStoresBeforePluginSave()
	{
		if (!g_weaponTypeStore.IsDirty() && !g_npcSkillStore.IsDirty() && !g_npcTrainingStore.IsDirty())
			return true;
		if (!GetActiveEditorFile())
			return false;

		bool ok = true;
		if (g_weaponTypeStore.IsDirty())
			ok = SaveWeaponTypeStore() && ok;
		if (g_npcSkillStore.IsDirty())
			ok = SaveNpcSkillStore() && ok;
		if (g_npcTrainingStore.IsDirty())
			ok = SaveNpcTrainingStore() && ok;

		if (ok)
			_MESSAGE("BladeSeparation CS: flushed pending sidecar carrier stores before plugin save");
		return ok;
	}

	static UInt32 ReadNpcFormId(const void* npc)
	{
		return TryReadEditorFormType(npc) == kFormTypeNpc ? ReadEditorFormId(npc) : 0;
	}

	static bool IsValidNpcStatsSidecarIndex(UInt32 index)
	{
		return index < kSidecarSkillCount;
	}

	static UInt32 NpcStatsSkillIdForIndex(UInt32 index)
	{
		return IsValidNpcStatsSidecarIndex(index) ? kSidecarSkills[index].skillId : 0;
	}

	static const char* NpcStatsSkillNameForIndex(UInt32 index)
	{
		return IsValidNpcStatsSidecarIndex(index) ? kSidecarSkills[index].name : "";
	}

	static UInt8 NpcStatsSortSkillIndexForSidecar(UInt32 index)
	{
		return static_cast<UInt8>(kNpcStatsSidecarSortSkillIndexBase + index);
	}

	static bool ListViewItemTextEquals(HWND list, int index, int subItem, const char* text)
	{
		char buffer[128] = {};
		LVITEMA item = {};
		item.mask = LVIF_TEXT;
		item.iItem = index;
		item.iSubItem = subItem;
		item.pszText = buffer;
		item.cchTextMax = sizeof(buffer);
		SendMessageA(list, LVM_GETITEMTEXTA, index, reinterpret_cast<LPARAM>(&item));
		return _stricmp(buffer, text) == 0;
	}

	static void SetListViewItemText(HWND list, int row, int subItem, const char* text)
	{
		LVITEMA item = {};
		item.mask = LVIF_TEXT;
		item.iItem = row;
		item.iSubItem = subItem;
		item.pszText = const_cast<char*>(text);
		SendMessageA(list, LVM_SETITEMTEXTA, row, reinterpret_cast<LPARAM>(&item));
	}

	static bool GetListViewItemText(HWND list, int row, int subItem, char* buffer, UInt32 bufferSize)
	{
		if (!list || !buffer || !bufferSize)
			return false;

		buffer[0] = 0;
		LVITEMA item = {};
		item.mask = LVIF_TEXT;
		item.iItem = row;
		item.iSubItem = subItem;
		item.pszText = buffer;
		item.cchTextMax = bufferSize;
		SendMessageA(list, LVM_GETITEMTEXTA, row, reinterpret_cast<LPARAM>(&item));
		return buffer[0] != 0;
	}

	static void* CsFormHeapAllocate(UInt32 size)
	{
		return reinterpret_cast<CsFormHeapAllocateFn>(kCsFormHeapAllocate)(size);
	}

	static void CsFormHeapFree(void* ptr)
	{
		if (ptr)
			reinterpret_cast<CsFormHeapFreeFn>(kCsFormHeapFree)(ptr);
	}

	static LPARAM GetListViewItemData(HWND list, int row)
	{
		LVITEMA item = {};
		item.mask = LVIF_PARAM;
		item.iItem = row;
		SendMessageA(list, LVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item));
		return item.lParam;
	}

	static bool ParseNpcStatsSidecarValue(const char* text, UInt8* value)
	{
		if (!text || !value)
			return false;

		while (*text == ' ' || *text == '\t')
			++text;
		if (!*text)
			return false;

		char* end = nullptr;
		const unsigned long parsed = std::strtoul(text, &end, 10);
		if (end == text)
			return false;

		*value = static_cast<UInt8>(BladeSeparationShared::NpcSkillSidecarStore::ClampLevel(parsed > 100 ? 100 : parsed));
		return true;
	}

	static bool GetNpcStatsSidecarIndexFromRowData(const NpcStatsSkillRowData* rowData, UInt32* index)
	{
		if (index)
			*index = 0;
		if (!rowData || rowData->marker != kNpcStatsSidecarRowMarker ||
			!IsValidNpcStatsSidecarIndex(rowData->sidecarIndex))
		{
			return false;
		}

		const UInt32 sidecarIndex = rowData->sidecarIndex;
		if (rowData->value != &g_npcStatsSkillValues[sidecarIndex] ||
			rowData->skillIndex != NpcStatsSortSkillIndexForSidecar(sidecarIndex))
		{
			return false;
		}

		if (index)
			*index = sidecarIndex;
		return true;
	}

	static bool __stdcall IsNpcStatsBladeSeparationRowData(const NpcStatsSkillRowData* rowData)
	{
		return GetNpcStatsSidecarIndexFromRowData(rowData, nullptr);
	}

	static NpcStatsSkillRowData* InitializeNpcStatsSidecarRowData(NpcStatsSkillRowData* rowData, UInt32 index)
	{
		if (!rowData || !IsValidNpcStatsSidecarIndex(index))
			return nullptr;

		rowData->value = &g_npcStatsSkillValues[index];
		rowData->skillIndex = NpcStatsSortSkillIndexForSidecar(index);
		rowData->sidecarIndex = static_cast<UInt8>(index);
		rowData->marker = kNpcStatsSidecarRowMarker;
		rowData->padding = 0;
		return rowData;
	}

	static NpcStatsSkillRowData* AllocateNpcStatsSidecarRowData(UInt32 index)
	{
		NpcStatsSkillRowData* rowData = reinterpret_cast<NpcStatsSkillRowData*>(
			CsFormHeapAllocate(sizeof(NpcStatsSkillRowData)));
		if (!rowData)
		{
			_ERROR("BladeSeparation CS: failed to allocate NPC stats sidecar row data");
			return nullptr;
		}

		return InitializeNpcStatsSidecarRowData(rowData, index);
	}

	static bool SetNpcStatsSidecarRowData(HWND list, int row, NpcStatsSkillRowData* rowData, UInt32 index)
	{
		if (!list || row < 0 || !InitializeNpcStatsSidecarRowData(rowData, index))
			return false;

		LVITEMA item = {};
		item.mask = LVIF_PARAM;
		item.iItem = row;
		item.iSubItem = 0;
		item.lParam = reinterpret_cast<LPARAM>(rowData);
		return SendMessageA(list, LVM_SETITEMA, 0, reinterpret_cast<LPARAM>(&item)) != FALSE;
	}

	static bool EnsureNpcStatsSidecarRowData(HWND list, int row, UInt32 index)
	{
		NpcStatsSkillRowData* rowData = reinterpret_cast<NpcStatsSkillRowData*>(
			GetListViewItemData(list, row));
		UInt32 existingIndex = 0;
		if (GetNpcStatsSidecarIndexFromRowData(rowData, &existingIndex) && existingIndex == index)
			return SetNpcStatsSidecarRowData(list, row, rowData, index);

		rowData = AllocateNpcStatsSidecarRowData(index);
		if (!rowData)
			return false;

		if (!SetNpcStatsSidecarRowData(list, row, rowData, index))
		{
			CsFormHeapFree(rowData);
			return false;
		}

		return true;
	}

	static bool IsNpcStatsBladeSeparationListRow(HWND list, int row, UInt32* index)
	{
		UInt32 rowDataIndex = 0;
		if (GetNpcStatsSidecarIndexFromRowData(reinterpret_cast<NpcStatsSkillRowData*>(GetListViewItemData(list, row)), &rowDataIndex))
		{
			if (index)
				*index = rowDataIndex;
			return true;
		}

		for (UInt32 i = 0; i < kSidecarSkillCount; ++i)
		{
			if (ListViewItemTextEquals(list, row, 1, NpcStatsSkillNameForIndex(i)))
			{
				if (index)
					*index = i;
				return true;
			}
		}

		return false;
	}

	static bool IsNpcStatsBladeSeparationListRow(HWND list, int row)
	{
		return IsNpcStatsBladeSeparationListRow(list, row, nullptr);
	}

	static void FormatNpcStatsSidecarValue(UInt32 index, char* buffer, UInt32 bufferSize)
	{
		if (!buffer || !bufferSize || !IsValidNpcStatsSidecarIndex(index))
			return;

		_snprintf_s(buffer, bufferSize, _TRUNCATE, "%u",
			static_cast<unsigned int>(g_npcStatsSkillValues[index]));
	}

	static bool ConfigureNpcStatsSidecarRow(HWND list, int row, UInt32 index)
	{
		if (!EnsureNpcStatsSidecarRowData(list, row, index))
			return false;

		char valueText[16] = {};
		FormatNpcStatsSidecarValue(index, valueText, sizeof(valueText));
		SetListViewItemText(list, row, 0, valueText);
		SetListViewItemText(list, row, 1, NpcStatsSkillNameForIndex(index));
		return true;
	}

	static void EnsureNpcStatsSkillsListScrollbar(HWND list, int itemCount)
	{
		if (!list)
			return;

		const LONG_PTR style = GetWindowLongPtrA(list, GWL_STYLE);
		const LONG_PTR newStyle = (style | WS_VSCROLL) & ~static_cast<LONG_PTR>(LVS_NOSCROLL);
		if (newStyle != style)
			SetWindowLongPtrA(list, GWL_STYLE, newStyle);

		ShowScrollBar(list, SB_VERT, TRUE);
		SetWindowPos(list, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		InvalidateRect(list, nullptr, TRUE);

		if (!g_loggedNpcStatsScrollbar)
		{
			_MESSAGE("BladeSeparation CS: NPC stats skills list scrollbar enabled hwnd=%08X items=%u style=%08X",
				reinterpret_cast<UInt32>(list),
				static_cast<unsigned int>(itemCount),
				static_cast<UInt32>(newStyle));
			g_loggedNpcStatsScrollbar = true;
		}
	}

	static void PrepareNpcStatsSidecarValues(void* npc)
	{
		LoadNpcSkillStoreOnce();

		g_activeNpcStatsFormId = ReadNpcFormId(npc);
		for (UInt32 i = 0; i < kSidecarSkillCount; ++i)
			g_npcStatsSkillValues[i] = kNpcStatsSidecarDefaultSkillValue;

		for (UInt32 i = 0; i < kSidecarSkillCount; ++i)
		{
			BladeSeparationShared::NpcSkillSidecarEntry entry = {};
			if (g_npcSkillStore.TryGet(g_activeNpcStatsFormId, NpcStatsSkillIdForIndex(i), &entry))
				g_npcStatsSkillValues[i] = static_cast<UInt8>(BladeSeparationShared::NpcSkillSidecarStore::ClampLevel(entry.level));
		}
	}

	static bool TryReadNpcStatsSidecarListValue(UInt32 index, UInt8* value)
	{
		HWND list = g_activeNpcStatsSkillsList;
		if (!list && g_activeNpcStatsDialog)
			list = GetDlgItem(g_activeNpcStatsDialog, kNpcStatsSkillsList);
		if (!list || !value || !IsValidNpcStatsSidecarIndex(index))
			return false;

		const int count = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0));
		for (int i = 0; i < count; ++i)
		{
			UInt32 rowIndex = 0;
			if (!IsNpcStatsBladeSeparationListRow(list, i, &rowIndex) || rowIndex != index)
				continue;

			char buffer[32] = {};
			if (GetListViewItemText(list, i, 0, buffer, sizeof(buffer)) &&
				ParseNpcStatsSidecarValue(buffer, value))
			{
				return true;
			}

			_WARNING("BladeSeparation CS: could not parse NPC stats %s value row=%d",
				NpcStatsSkillNameForIndex(index),
				i);
			return false;
		}

		return false;
	}

	static void SaveActiveNpcStatsSidecarValue(UInt32 index, const char* source)
	{
		if (!g_activeNpcStatsFormId || !IsValidNpcStatsSidecarIndex(index))
			return;

		g_npcStatsSkillValues[index] = static_cast<UInt8>(BladeSeparationShared::NpcSkillSidecarStore::ClampLevel(g_npcStatsSkillValues[index]));
		const UInt32 skillId = NpcStatsSkillIdForIndex(index);
		if (g_npcSkillStore.SetLevel(g_activeNpcStatsFormId, skillId, g_npcStatsSkillValues[index]))
		{
			_MESSAGE("BladeSeparation CS: saved NPC stats %s form=%08X value=%u source=%s",
				NpcStatsSkillNameForIndex(index),
				g_activeNpcStatsFormId,
				static_cast<unsigned int>(g_npcStatsSkillValues[index]),
				source ? source : "unknown");
			if (!GetActiveEditorFile())
			{
				if (!g_loggedNpcSkillNoActivePlugin)
				{
					_WARNING("BladeSeparation CS: NPC stats sidecar values are affixed for this editor session only; activate a plugin to persist them to BSEPNPCSkillData");
					g_loggedNpcSkillNoActivePlugin = true;
				}
				return;
			}

			if (!SaveNpcSkillStore())
			{
				_WARNING("BladeSeparation CS: NPC stats %s form=%08X value=%u kept in memory; embedded save pending an active plugin",
					NpcStatsSkillNameForIndex(index),
					g_activeNpcStatsFormId,
					static_cast<unsigned int>(g_npcStatsSkillValues[index]));
			}
		}
	}

	static void PersistActiveNpcStatsSidecar(UInt32 index)
	{
		UInt8 listValue = 0;
		if (TryReadNpcStatsSidecarListValue(index, &listValue))
			g_npcStatsSkillValues[index] = listValue;

		SaveActiveNpcStatsSidecarValue(index, "native-edit");
	}

	static bool __stdcall IsNpcStatsBladeSeparationEditRow(NMLVDISPINFOA* data)
	{
		if (!data)
			return false;

		if (IsNpcStatsBladeSeparationRowData(reinterpret_cast<NpcStatsSkillRowData*>(data->item.lParam)))
			return true;

		return data->hdr.hwndFrom && data->item.iItem >= 0 &&
			IsNpcStatsBladeSeparationListRow(data->hdr.hwndFrom, data->item.iItem);
	}

	static bool __stdcall HandleNpcStatsSkillEditGate(void* npc, NMLVDISPINFOA* data, UInt32 autoCalc)
	{
		if (!data)
			return false;

		UInt32 index = 0;
		NpcStatsSkillRowData* rowData = reinterpret_cast<NpcStatsSkillRowData*>(data->item.lParam);
		if (!GetNpcStatsSidecarIndexFromRowData(rowData, &index))
		{
			if (!data->hdr.hwndFrom || data->item.iItem < 0 ||
				!IsNpcStatsBladeSeparationListRow(data->hdr.hwndFrom, data->item.iItem, &index))
			{
				return false;
			}
		}

		UInt8 editedValue = 0;
		if (!ParseNpcStatsSidecarValue(data->item.pszText, &editedValue))
		{
			_WARNING("BladeSeparation CS: could not parse auto-calc NPC stats %s edit", NpcStatsSkillNameForIndex(index));
			return false;
		}

		const UInt32 formId = ReadNpcFormId(npc);
		if (formId)
			g_activeNpcStatsFormId = formId;
		if (!g_activeNpcStatsFormId)
			return false;

		g_npcStatsSkillValues[index] = editedValue;

		char valueText[16] = {};
		FormatNpcStatsSidecarValue(index, valueText, sizeof(valueText));
		if (data->item.pszText && data->item.cchTextMax > 0)
			_snprintf_s(data->item.pszText, data->item.cchTextMax, _TRUNCATE, "%s", valueText);
		if (data->hdr.hwndFrom && data->item.iItem >= 0)
			SetListViewItemText(data->hdr.hwndFrom, data->item.iItem, 0, valueText);

		SaveActiveNpcStatsSidecarValue(index, autoCalc ? "autocalc-edit" : "pre-native-edit");
		return true;
	}

	static void RemoveNpcStatsSidecarRows(HWND list)
	{
		if (!list)
			return;

		for (int i = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0)) - 1; i >= 0; --i)
		{
			if (!IsNpcStatsBladeSeparationListRow(list, i))
				continue;

			NpcStatsSkillRowData* rowData = reinterpret_cast<NpcStatsSkillRowData*>(GetListViewItemData(list, i));
			if (IsNpcStatsBladeSeparationRowData(rowData))
				CsFormHeapFree(rowData);
			SendMessageA(list, LVM_DELETEITEM, i, 0);
		}
	}

	static void EnsureNpcStatsSidecarListRows(void* npc, HWND list)
	{
		if (!list)
			return;

		g_activeNpcStatsSkillsList = list;
		PrepareNpcStatsSidecarValues(npc);
		RemoveNpcStatsSidecarRows(list);

		int count = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0));
		for (UInt32 i = 0; i < kSidecarSkillCount; ++i)
		{
			char valueText[16] = {};
			FormatNpcStatsSidecarValue(i, valueText, sizeof(valueText));
			NpcStatsSkillRowData* rowData = AllocateNpcStatsSidecarRowData(i);
			if (!rowData)
				continue;

			LVITEMA item = {};
			item.mask = LVIF_TEXT | LVIF_PARAM;
			item.iItem = static_cast<int>(i);
			item.iSubItem = 0;
			item.pszText = valueText;
			item.lParam = reinterpret_cast<LPARAM>(rowData);
			const int row = static_cast<int>(SendMessageA(list, LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&item)));
			if (row >= 0)
				ConfigureNpcStatsSidecarRow(list, row, i);
			else
				CsFormHeapFree(rowData);
		}

		count = static_cast<int>(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0));
		EnsureNpcStatsSkillsListScrollbar(list, count);
	}

	static void EnsureNpcStatsSidecarRows(void* npc, HWND dialog)
	{
		if (!dialog)
			return;

		g_activeNpcStatsDialog = dialog;
		EnsureNpcStatsSidecarListRows(npc, GetDlgItem(dialog, kNpcStatsSkillsList));
	}

	static int CallNpcStatsPopulateOriginal(void* npc, HWND dialog)
	{
		if (g_npcStatsPopulateOriginalIsFastcall)
			return reinterpret_cast<NpcStatsPopulateChainedFn>(g_npcStatsPopulateOriginalTarget)(npc, nullptr, dialog);

		return reinterpret_cast<NpcStatsPopulateFn>(g_npcStatsPopulateOriginalTarget)(npc, dialog);
	}

	static LRESULT CallNpcStatsInitSkillListOriginal(void* npc, HWND list)
	{
		if (g_npcStatsInitSkillListOriginalIsFastcall)
			return reinterpret_cast<NpcStatsInitSkillListChainedFn>(g_npcStatsInitSkillListOriginalTarget)(npc, nullptr, list);

		return reinterpret_cast<NpcStatsInitSkillListFn>(g_npcStatsInitSkillListOriginalTarget)(npc, list);
	}

	static int __fastcall NpcStatsPopulateHook(void* npc, void*, HWND dialog)
	{
		const int result = CallNpcStatsPopulateOriginal(npc, dialog);
		EnsureNpcStatsSidecarRows(npc, dialog);
		return result;
	}

	static LRESULT __fastcall NpcStatsInitSkillListHook(void* npc, void*, HWND list)
	{
		const LRESULT result = CallNpcStatsInitSkillListOriginal(npc, list);
		EnsureNpcStatsSidecarListRows(npc, list);
		return result;
	}

	static void __stdcall HandleNpcStatsSkillEditCommit(NpcStatsSkillRowData* rowData)
	{
		UInt32 index = 0;
		if (GetNpcStatsSidecarIndexFromRowData(rowData, &index))
			PersistActiveNpcStatsSidecar(index);
	}

	static void __declspec(naked) NpcStatsSkillEditAutoCalcGateHook()
	{
		__asm
		{
			test al, al
			jz notAutoCalc
			pushad
			push 1
			push ebp
			push edi
			call HandleNpcStatsSkillEditGate
			mov [esp+1Ch], eax
			popad
			test al, al
			jz autoCalcNotHandled
			mov ecx, [esp+20h]
			mov dword ptr [ecx], 1
			mov al, 1
			jmp kNpcStatsDialogSuccessReturn
		autoCalcNotHandled:
			cmp dword ptr [g_npcStatsSkillEditAutoCalcGateChainTarget], 0
			jne chainPrevious
			jmp kNpcStatsSkillEditAutoCalcRejectReturn
		notAutoCalc:
			pushad
			push 0
			push ebp
			push edi
			call HandleNpcStatsSkillEditGate
			mov [esp+1Ch], eax
			popad
			test al, al
			jz notAutoCalcNotHandled
			jmp kNpcStatsSkillEditAutoCalcContinue
		notAutoCalcNotHandled:
			cmp dword ptr [g_npcStatsSkillEditAutoCalcGateChainTarget], 0
			jne chainPrevious
			jmp kNpcStatsSkillEditAutoCalcContinue
		chainPrevious:
			jmp dword ptr [g_npcStatsSkillEditAutoCalcGateChainTarget]
		}
	}

	static void __declspec(naked) NpcStatsSkillEditCommitHook()
	{
		__asm
		{
			pushad
			push esi
			call IsNpcStatsBladeSeparationRowData
			mov [esp+1Ch], eax
			test al, al
			popad
			jnz handleBladeSeparation
			cmp dword ptr [g_npcStatsSkillEditCommitChainTarget], 0
			jne chainPrevious
			mov eax, [esp+2Ch]
			mov dword ptr [eax], 1
			jmp kNpcStatsSkillEditCommitReturn
		chainPrevious:
			jmp dword ptr [g_npcStatsSkillEditCommitChainTarget]
		handleBladeSeparation:
			pushad
			push esi
			call HandleNpcStatsSkillEditCommit
			popad
			mov eax, [esp+2Ch]
			mov dword ptr [eax], 1
			jmp kNpcStatsSkillEditCommitReturn
		}
	}

	static int __fastcall AiFormInitDialogControlsHook(void* aiForm, void*, HWND dialog)
	{
		const int result = g_aiFormInitDialogControlsOriginal ?
			g_aiFormInitDialogControlsOriginal(aiForm, dialog) : 0;
		ApplyNpcAiTrainingSidecarSelection(aiForm, dialog);
		return result;
	}

	static int __fastcall AiFormSaveDialogControlsHook(void* aiForm, void*, HWND dialog)
	{
		SaveNpcAiTrainingSidecarSelection(aiForm, dialog);
		const int result = g_aiFormSaveDialogControlsOriginal ?
			g_aiFormSaveDialogControlsOriginal(aiForm, dialog) : 0;
		RestoreTrainingComboSidecarIfAffixed(GetDlgItem(dialog, kAiTrainingSkillCombo), "dialog save");
		return result;
	}

	static void __declspec(naked) SavePluginPreWriteHook()
	{
		__asm
		{
			pushfd
			pushad
			call FlushPendingSidecarStoresBeforePluginSave
			popad
			popfd

			mov eax, dword ptr [g_savePluginPreWriteChainTarget]
			test eax, eax
			jz noChain
			jmp eax

		noChain:
			call kSavePluginPreWriteOriginalCall
			jmp kSavePluginPreWriteReturn
		}
	}

	static void RegisterSidecarsAlongsideVanillaEditorSurface()
	{
		// The editor keeps vanilla skill/type surfaces and layers sidecar metadata through local tables.
	}

	static bool IsComboBox(HWND hwnd)
	{
		char className[32] = {};
		return hwnd && GetClassNameA(hwnd, className, sizeof(className)) && !_stricmp(className, "ComboBox");
	}

	static std::string NormalizeWeaponTypeLabel(const std::string& value)
	{
		std::string normalized;
		normalized.reserve(value.size());
		for (char ch : value)
		{
			const unsigned char uch = static_cast<unsigned char>(ch);
			if (std::isalnum(uch))
				normalized.push_back(static_cast<char>(std::tolower(uch)));
		}
		return normalized;
	}

	static bool NormalizedContains(const std::string& value, const char* token)
	{
		return value.find(token) != std::string::npos;
	}

	static bool LabelMatchesNativeWeaponType(const std::string& label, const char* nativeLabel)
	{
		const std::string normalized = NormalizeWeaponTypeLabel(label);
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBladeOneHand))
			return NormalizedContains(normalized, "blade") && (NormalizedContains(normalized, "one") || NormalizedContains(normalized, "1"));
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBladeTwoHand))
			return NormalizedContains(normalized, "blade") && (NormalizedContains(normalized, "two") || NormalizedContains(normalized, "2"));
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBluntOneHand))
			return NormalizedContains(normalized, "blunt") && (NormalizedContains(normalized, "one") || NormalizedContains(normalized, "1"));
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBluntTwoHand))
			return NormalizedContains(normalized, "blunt") && (NormalizedContains(normalized, "two") || NormalizedContains(normalized, "2"));
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeStaff))
			return NormalizedContains(normalized, "staff");
		if (!std::strcmp(nativeLabel, kNativeWeaponTypeBow))
			return NormalizedContains(normalized, "bow");
		return false;
	}

	static int FindComboStringExact(HWND combo, const char* text)
	{
		if (!combo || !text)
			return CB_ERR;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		for (int i = 0; i < count; ++i)
		{
			std::string value;
			if (!ReadComboString(combo, i, value))
				continue;
			if (!_stricmp(value.c_str(), text))
				return i;
		}
		return CB_ERR;
	}

	static int FindNativeWeaponTypeString(HWND combo, const char* nativeLabel)
	{
		const int exact = FindComboStringExact(combo, nativeLabel);
		if (exact != CB_ERR)
			return exact;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		for (int i = 0; i < count; ++i)
		{
			std::string value;
			if (ReadComboString(combo, i, value) && LabelMatchesNativeWeaponType(value, nativeLabel))
				return i;
		}
		return CB_ERR;
	}

	static bool ReadComboString(HWND combo, int index, std::string& value)
	{
		value.clear();
		if (!combo || index < 0)
			return false;

		const LONG_PTR style = GetWindowLongPtrA(combo, GWL_STYLE);
		if ((style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)) && !(style & CBS_HASSTRINGS))
			return false;

		const LRESULT length = SendMessageA(combo, CB_GETLBTEXTLEN, index, 0);
		if (length == CB_ERR || length < 0 || length > 4096)
			return false;

		std::string buffer(static_cast<size_t>(length) + 1, '\0');
		const LRESULT copied = SendMessageA(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(buffer.data()));
		if (copied == CB_ERR || copied < 0)
			return false;

		buffer.resize(static_cast<size_t>(copied));
		value.swap(buffer);
		return true;
	}

	static bool IsSidecarWeaponTypeLabel(const char* value)
	{
		return value &&
			(!_stricmp(value, kLongBladeEditorLabel) ||
			 !_stricmp(value, kShortBladeEditorLabel) ||
			 !_stricmp(value, kAxeEditorLabel));
	}

	static BladeSeparationShared::WeaponSkillKind KindForSidecarWeaponTypeLabel(const char* value)
	{
		if (!value)
			return BladeSeparationShared::kWeaponSkill_None;
		if (!_stricmp(value, kLongBladeEditorLabel))
			return BladeSeparationShared::kWeaponSkill_Long;
		if (!_stricmp(value, kShortBladeEditorLabel))
			return BladeSeparationShared::kWeaponSkill_Short;
		if (!_stricmp(value, kAxeEditorLabel))
			return BladeSeparationShared::kWeaponSkill_Axe;
		return BladeSeparationShared::kWeaponSkill_None;
	}

	static const char* LabelForSidecarWeaponTypeKind(BladeSeparationShared::WeaponSkillKind kind)
	{
		switch (kind)
		{
			case BladeSeparationShared::kWeaponSkill_Long:
				return kLongBladeEditorLabel;
			case BladeSeparationShared::kWeaponSkill_Short:
				return kShortBladeEditorLabel;
			case BladeSeparationShared::kWeaponSkill_Axe:
				return kAxeEditorLabel;
			default:
				return "";
		}
	}

	static UInt32 SkillIdForTrainingLabel(const char* value)
	{
		return BladeSeparationShared::NpcSkillSidecarPayloadCodec::SkillIdFromPayloadName(value);
	}

	static const char* TrainingLabelForSkillId(UInt32 skillId)
	{
		return BladeSeparationShared::NpcTrainingSidecarPayloadCodec::TrainingNameForSkillId(skillId);
	}

	static UInt32 TrainingFallbackActorValueForSkillId(UInt32 skillId)
	{
		for (UInt32 i = 0; i < kSidecarSkillCount; ++i)
		{
			if (kSidecarSkills[i].skillId == skillId)
				return kSidecarSkills[i].fallbackActorValue;
		}

		return kNativeBladeCompatibilityActorValue;
	}

	static bool IsSidecarTrainingSkill(UInt32 skillId)
	{
		return skillId == BladeSeparationShared::kLongSkillId ||
			skillId == BladeSeparationShared::kShortSkillId ||
			skillId == BladeSeparationShared::kAxeSkillId;
	}

	static void* DynamicCastTesAiFormToNpc(void* aiForm)
	{
		static const RuntimeDynamicCastFn dynamicCast =
			reinterpret_cast<RuntimeDynamicCastFn>(kRuntimeDynamicCast);
		if (!aiForm || !dynamicCast)
			return nullptr;

		void* npc = dynamicCast(aiForm,
			0,
			reinterpret_cast<void*>(kRttiTesAiFormTypeDescriptor),
			reinterpret_cast<void*>(kRttiTesNpcTypeDescriptor),
			0);
		return TryReadEditorFormType(npc) == kFormTypeNpc ? npc : nullptr;
	}

	static void* ResolveNpcFormFromAiDialog(HWND dialog)
	{
		static const WinDialogWndGetExtraDataByTypeFn getExtraDataByType =
			reinterpret_cast<WinDialogWndGetExtraDataByTypeFn>(kWinDialogWndGetExtraDataByType);
		void* extraData = dialog && getExtraDataByType ?
			getExtraDataByType(dialog, kDialogExtraWorkingDataType) : nullptr;
		if (!extraData)
			return nullptr;

		const void* ownerAddress = reinterpret_cast<const UInt8*>(extraData) + kDialogExtraWorkingDataOwnerOffset;
		if (IsBadReadPtr(ownerAddress, sizeof(void*)))
			return nullptr;

		void* owner = *reinterpret_cast<void* const*>(ownerAddress);
		if (void* npc = DynamicCastTesAiFormToNpc(owner))
			return npc;

		if (!g_loggedNpcTrainingBadDialogOwner)
		{
			_WARNING("BladeSeparation CS: NPC training dialog owner did not RTTI-cast from TESAIForm to TESNPC; refusing raw owner pointer=%p",
				owner);
			g_loggedNpcTrainingBadDialogOwner = true;
		}
		return nullptr;
	}

	static void* ResolveNpcFormFromAiDialogOrForm(void* aiForm, HWND dialog)
	{
		(void)aiForm;
		return ResolveNpcFormFromAiDialog(dialog);
	}

	static void AddSidecarTrainingComboItems(HWND combo)
	{
		if (!combo)
			return;

		for (UInt32 i = 0; i < kSidecarSkillCount; ++i)
		{
			const char* label = TrainingLabelForSkillId(kSidecarSkills[i].skillId);
			if (!label || !label[0] || FindComboStringExact(combo, label) != CB_ERR)
				continue;

			const int index = static_cast<int>(SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label)));
			if (index != CB_ERR && index != CB_ERRSPACE)
				SendMessageA(combo, CB_SETITEMDATA, index, TrainingFallbackActorValueForSkillId(kSidecarSkills[i].skillId));
		}
	}

	static void MakeTrainingComboScrollable(HWND combo)
	{
		if (!combo || GetPropA(combo, kTrainingComboScrollableProp))
			return;
		if (SendMessageA(combo, CB_GETDROPPEDSTATE, 0, 0))
			return;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		const int itemHeight = static_cast<int>(SendMessageA(combo, CB_GETITEMHEIGHT, 0, 0));
		const int selectionHeight = static_cast<int>(SendMessageA(combo, CB_GETITEMHEIGHT, static_cast<WPARAM>(-1), 0));
		if (count <= 0 || itemHeight <= 0 || selectionHeight <= 0)
			return;

		SendMessageA(combo, CB_SETMINVISIBLE, static_cast<WPARAM>(kTrainingComboMinimumVisibleRows), 0);

		RECT rect = {};
		if (!GetWindowRect(combo, &rect))
			return;

		const int width = rect.right - rect.left;
		const int visibleRows = count > kTrainingComboMinimumVisibleRows ? kTrainingComboMinimumVisibleRows : count;
		const int height = selectionHeight + (itemHeight * visibleRows) + 10;
		const LONG_PTR style = GetWindowLongPtrA(combo, GWL_STYLE);
		SetWindowLongPtrA(combo, GWL_STYLE, style | WS_VSCROLL | CBS_DISABLENOSCROLL);
		SetWindowPos(combo, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		SetPropA(combo, kTrainingComboScrollableProp, reinterpret_cast<HANDLE>(1));
	}

	static WNDPROC GetOriginalTrainingComboProc(HWND combo)
	{
		return reinterpret_cast<WNDPROC>(GetPropA(combo, kTrainingComboOriginalProcProp));
	}

	static LRESULT CALLBACK TrainingComboProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC original = GetOriginalTrainingComboProc(hwnd);
		if (!original)
			return DefWindowProcA(hwnd, msg, wParam, lParam);

		if (msg == WM_NCDESTROY)
		{
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
			RemovePropA(hwnd, kTrainingComboPatchedProp);
			RemovePropA(hwnd, kTrainingComboOriginalProcProp);
			RemovePropA(hwnd, kTrainingComboScrollableProp);
			RemovePropA(hwnd, kTrainingComboApplyingSelectionProp);
			RemovePropA(hwnd, kTrainingComboBoundNpcFormIdProp);
			return CallWindowProcA(original, hwnd, msg, wParam, lParam);
		}

		if (msg == WM_MBUTTONDOWN)
		{
			SetFocus(hwnd);
			SendMessageA(hwnd, CB_SHOWDROPDOWN, TRUE, 0);
			return 0;
		}

		if (msg == WM_MOUSEWHEEL && !SendMessageA(hwnd, CB_GETDROPPEDSTATE, 0, 0))
		{
			SetFocus(hwnd);
			SendMessageA(hwnd, CB_SHOWDROPDOWN, TRUE, 0);
		}

		return CallWindowProcA(original, hwnd, msg, wParam, lParam);
	}

	static void PatchTrainingCombo(HWND combo)
	{
		if (!combo)
			return;

		AddSidecarTrainingComboItems(combo);
		MakeTrainingComboScrollable(combo);

		if (GetPropA(combo, kTrainingComboPatchedProp))
			return;

		WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(combo, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&TrainingComboProc)));
		if (!original)
			return;

		SetPropA(combo, kTrainingComboOriginalProcProp, reinterpret_cast<HANDLE>(original));
		SetPropA(combo, kTrainingComboPatchedProp, reinterpret_cast<HANDLE>(1));
	}

	static UInt32 GetTrainingComboSelectionSkillId(HWND combo)
	{
		const int selection = combo ? static_cast<int>(SendMessageA(combo, CB_GETCURSEL, 0, 0)) : CB_ERR;
		if (selection == CB_ERR)
			return 0;

		std::string value;
		if (!ReadComboString(combo, selection, value))
			return 0;

		return SkillIdForTrainingLabel(value.c_str());
	}

	static void BindTrainingComboToNpc(HWND combo, void* npc)
	{
		if (!combo || TryReadEditorFormType(npc) != kFormTypeNpc)
			return;

		const UInt32 formId = ReadEditorFormId(npc);
		if (formId)
			SetPropA(combo, kTrainingComboBoundNpcFormIdProp, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(formId)));
	}

	static UInt32 GetBoundTrainingComboNpcFormId(HWND combo)
	{
		const UInt32 formId = static_cast<UInt32>(reinterpret_cast<UINT_PTR>(GetPropA(combo, kTrainingComboBoundNpcFormIdProp)));
		if (!formId)
			return 0;

		void* form = LookupEditorFormById(formId);
		return TryReadEditorFormType(form) == kFormTypeNpc ? formId : 0;
	}

	static bool PersistNpcTrainingSelectionForFormId(UInt32 formId, UInt32 skillId, const char* source)
	{
		if (!formId)
			return false;

		LoadNpcTrainingStoreOnce();
		const bool changed = IsSidecarTrainingSkill(skillId) ?
			g_npcTrainingStore.Set(formId, skillId) :
			g_npcTrainingStore.Remove(formId);

		if (!changed && !g_npcTrainingStore.IsDirty())
			return true;

		if (!GetActiveEditorFile())
		{
			if (!g_loggedNpcTrainingNoActivePlugin)
			{
				_WARNING("BladeSeparation CS: NPC training sidecar changed for form=%08X but no active plugin can receive %s; selection is affixed for this editor session only",
					formId,
					kNpcTrainingCarrierEditorId);
				g_loggedNpcTrainingNoActivePlugin = true;
			}
			return false;
		}

		if (!SaveNpcTrainingStore())
		{
			_WARNING("BladeSeparation CS: failed to save NPC training sidecar for form=%08X source=%s",
				formId,
				source ? source : "unknown");
			return false;
		}

		_MESSAGE("BladeSeparation CS: persisted NPC training sidecar form=%08X skill=%s source=%s",
			formId,
			IsSidecarTrainingSkill(skillId) ? TrainingLabelForSkillId(skillId) : "native",
			source ? source : "unknown");
		return true;
	}

	static bool PersistTrainingComboSelection(HWND combo, const char* source)
	{
		if (!combo || GetPropA(combo, kTrainingComboApplyingSelectionProp))
			return false;

		HWND dialog = GetParent(combo);
		UInt32 formId = GetBoundTrainingComboNpcFormId(combo);
		if (!formId)
		{
			if (void* npc = ResolveNpcFormFromAiDialog(dialog))
			{
				BindTrainingComboToNpc(combo, npc);
				formId = GetBoundTrainingComboNpcFormId(combo);
			}
		}
		if (!formId)
		{
			if (!g_loggedNpcTrainingNoForm)
			{
				_WARNING("BladeSeparation CS: cannot persist NPC training sidecar; edited NPC form was not resolved for combo=%p", combo);
				g_loggedNpcTrainingNoForm = true;
			}
			return false;
		}

		const bool trainingEnabled = !dialog || IsDlgButtonChecked(dialog, kAiTrainingCheckBox) == BST_CHECKED;
		const UInt32 skillId = trainingEnabled ? GetTrainingComboSelectionSkillId(combo) : 0;
		return PersistNpcTrainingSelectionForFormId(formId, skillId, source);
	}

	static UInt32 TrainingComboBoundNpcSkillId(HWND combo)
	{
		const UInt32 formId = GetBoundTrainingComboNpcFormId(combo);
		if (!formId)
			return 0;

		LoadNpcTrainingStoreOnce();
		BladeSeparationShared::NpcTrainingSidecarEntry entry = {};
		return g_npcTrainingStore.TryGet(formId, &entry) ? entry.skillId : 0;
	}

	static void RestoreTrainingComboSidecarIfAffixed(HWND combo, const char* source)
	{
		if (!combo || GetPropA(combo, kTrainingComboApplyingSelectionProp))
			return;
		if (IsWindowEnabled(GetDlgItem(GetParent(combo), kAiTrainingCheckBox)) &&
			IsDlgButtonChecked(GetParent(combo), kAiTrainingCheckBox) != BST_CHECKED)
			return;

		const UInt32 skillId = TrainingComboBoundNpcSkillId(combo);
		if (!IsSidecarTrainingSkill(skillId) || GetTrainingComboSelectionSkillId(combo) == skillId)
			return;

		SelectTrainingComboSidecarSkill(combo, skillId);
		_MESSAGE("BladeSeparation CS: restored NPC AI %s training combo selection after native normalization source=%s",
			TrainingLabelForSkillId(skillId),
			source ? source : "unknown");
	}

	static WNDPROC GetOriginalTrainingParentProc(HWND parent)
	{
		return reinterpret_cast<WNDPROC>(GetPropA(parent, kTrainingParentOriginalProcProp));
	}

	static LRESULT CALLBACK TrainingParentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC original = GetOriginalTrainingParentProc(hwnd);
		if (!original)
			return DefWindowProcA(hwnd, msg, wParam, lParam);

		if (msg == WM_NCDESTROY)
		{
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
			RemovePropA(hwnd, kTrainingParentPatchedProp);
			RemovePropA(hwnd, kTrainingParentOriginalProcProp);
			RemovePropA(hwnd, kTrainingParentComboProp);
			return CallWindowProcA(original, hwnd, msg, wParam, lParam);
		}

		HWND restoreCombo = nullptr;
		if (msg == WM_COMMAND)
		{
			const UINT controlId = LOWORD(wParam);
			const UINT notification = HIWORD(wParam);
			if (controlId == kAiTrainingSkillCombo && lParam)
			{
				HWND combo = reinterpret_cast<HWND>(lParam);
				if (GetPropA(combo, kTrainingComboPatchedProp) &&
					(notification == CBN_SELCHANGE || notification == CBN_SELENDOK || notification == CBN_CLOSEUP))
				{
					PersistTrainingComboSelection(combo, "training combo selection");
					if (TrainingComboBoundNpcSkillId(combo))
						restoreCombo = combo;
				}
			}
			else if (controlId == kAiTrainingCheckBox)
			{
				HWND combo = reinterpret_cast<HWND>(GetPropA(hwnd, kTrainingParentComboProp));
				if (!combo)
					combo = GetDlgItem(hwnd, kAiTrainingSkillCombo);
				if (combo && GetPropA(combo, kTrainingComboPatchedProp))
				{
					PersistTrainingComboSelection(combo, "training checkbox toggle");
					if (TrainingComboBoundNpcSkillId(combo))
						restoreCombo = combo;
				}
			}
		}

		const LRESULT result = CallWindowProcA(original, hwnd, msg, wParam, lParam);
		if (restoreCombo)
			RestoreTrainingComboSidecarIfAffixed(restoreCombo, "parent dialog notification");

		return result;
	}

	static void PatchTrainingParentDialog(HWND dialog, HWND combo)
	{
		if (!dialog || !combo)
			return;

		SetPropA(dialog, kTrainingParentComboProp, combo);
		if (GetPropA(dialog, kTrainingParentPatchedProp))
			return;

		WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(dialog, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&TrainingParentProc)));
		if (!original)
		{
			RemovePropA(dialog, kTrainingParentComboProp);
			return;
		}

		SetPropA(dialog, kTrainingParentOriginalProcProp, reinterpret_cast<HANDLE>(original));
		SetPropA(dialog, kTrainingParentPatchedProp, reinterpret_cast<HANDLE>(1));
	}

	static void SelectTrainingComboSidecarSkill(HWND combo, UInt32 skillId)
	{
		if (!combo || !IsSidecarTrainingSkill(skillId))
			return;

		PatchTrainingCombo(combo);
		const int index = FindComboStringExact(combo, TrainingLabelForSkillId(skillId));
		if (index != CB_ERR)
		{
			SetPropA(combo, kTrainingComboApplyingSelectionProp, reinterpret_cast<HANDLE>(1));
			SendMessageA(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
			RemovePropA(combo, kTrainingComboApplyingSelectionProp);
		}
	}

	static void ApplyNpcAiTrainingSidecarSelection(void* aiForm, HWND dialog)
	{
		HWND combo = GetDlgItem(dialog, kAiTrainingSkillCombo);
		if (!combo)
			return;

		PatchTrainingCombo(combo);
		PatchTrainingParentDialog(dialog, combo);

		LoadNpcTrainingStoreOnce();
		void* npc = ResolveNpcFormFromAiDialogOrForm(aiForm, dialog);
		if (!npc)
			return;

		BindTrainingComboToNpc(combo, npc);
		BladeSeparationShared::NpcTrainingSidecarEntry entry = {};
		if (!g_npcTrainingStore.TryGet(ReadEditorFormId(npc), &entry))
			return;

		HWND trainingCheckBox = GetDlgItem(dialog, kAiTrainingCheckBox);
		const BOOL canEditTraining = !trainingCheckBox || IsWindowEnabled(trainingCheckBox);
		CheckDlgButton(dialog, kAiTrainingCheckBox, BST_CHECKED);
		if (canEditTraining)
		{
			EnableWindow(combo, TRUE);
			if (HWND level = GetDlgItem(dialog, kAiTrainingLevelEdit))
				EnableWindow(level, TRUE);
		}
		SelectTrainingComboSidecarSkill(combo, entry.skillId);
		_MESSAGE("BladeSeparation CS: applied NPC AI %s training sidecar selection form=%08X",
			TrainingLabelForSkillId(entry.skillId),
			ReadEditorFormId(npc));
	}

	static void SaveNpcAiTrainingSidecarSelection(void* aiForm, HWND dialog)
	{
		void* npc = ResolveNpcFormFromAiDialogOrForm(aiForm, dialog);
		HWND combo = GetDlgItem(dialog, kAiTrainingSkillCombo);
		if (!combo)
			return;

		if (npc)
			BindTrainingComboToNpc(combo, npc);

		const UInt32 formId = npc ? ReadEditorFormId(npc) : GetBoundTrainingComboNpcFormId(combo);
		if (!formId)
			return;

		const bool trainingEnabled = IsDlgButtonChecked(dialog, kAiTrainingCheckBox) == BST_CHECKED;
		UInt32 skillId = trainingEnabled ? GetTrainingComboSelectionSkillId(combo) : 0;
		if (trainingEnabled && !IsSidecarTrainingSkill(skillId))
			skillId = TrainingComboBoundNpcSkillId(combo);
		PersistNpcTrainingSelectionForFormId(formId, skillId, "dialog save");
	}

	static bool IsNativeWeaponTypeColumnText(const char* text)
	{
		if (!text || !text[0])
			return false;

		const std::string normalized = NormalizeWeaponTypeLabel(text);
		return normalized == "bladeonehand" ||
			normalized == "blade1hand" ||
			normalized == "bladeonehanded" ||
			normalized == "blade1handed" ||
			normalized == "bladetwohand" ||
			normalized == "blade2hand" ||
			normalized == "bladetwohanded" ||
			normalized == "blade2handed" ||
			normalized == "bluntonehand" ||
			normalized == "blunt1hand" ||
			normalized == "bluntonehanded" ||
			normalized == "blunt1handed" ||
			normalized == "blunttwohand" ||
			normalized == "blunt2hand" ||
			normalized == "blunttwohanded" ||
			normalized == "blunt2handed" ||
			normalized == "staff" ||
			normalized == "bow";
	}

	static void ApplyObjectWindowWeaponTypeColumnText(NMLVDISPINFOA* data)
	{
		if (!data || !(data->item.mask & LVIF_TEXT) || !data->item.pszText || data->item.cchTextMax <= 0)
			return;
		// Type=12 is the decoded CSE logical column, but the stock Object
		// Window LVN_GETDISPINFO callback reaches this hook with visible list
		// subitem indices. Use the stronger native output/form checks below so
		// reordered visible columns still get sidecar text.
		(void)kObjectWindowColumnType;
		if (!IsNativeWeaponTypeColumnText(data->item.pszText))
			return;

		void* form = reinterpret_cast<void*>(data->item.lParam);
		if (ReadEditorFormType(form) != kFormTypeWeapon)
			return;

		LoadWeaponTypeStoreOnce();

		BladeSeparationShared::WeaponSkillKind kind = BladeSeparationShared::kWeaponSkill_None;
		if (!g_weaponTypeStore.TryGet(ReadEditorFormId(form), &kind))
			return;

		const char* label = LabelForSidecarWeaponTypeKind(kind);
		if (!label || !label[0])
			return;

		_snprintf_s(data->item.pszText, data->item.cchTextMax, _TRUNCATE, "%s", label);
	}

	static void __fastcall ObjectWindowListViewGetDispInfoHook(void* treeEntry, void*, NMLVDISPINFOA* data)
	{
		if (g_objectWindowListViewGetDispInfoOriginal)
			g_objectWindowListViewGetDispInfoOriginal(treeEntry, data);
		ApplyObjectWindowWeaponTypeColumnText(data);
	}

	static const char* NativeFallbackLabelForSidecarWeaponTypeKind(BladeSeparationShared::WeaponSkillKind kind)
	{
		return kind == BladeSeparationShared::kWeaponSkill_Axe ? kNativeWeaponTypeBluntOneHand : kNativeWeaponTypeBladeOneHand;
	}

	static int GetActualWeaponTypeComboSelection(HWND combo)
	{
		WNDPROC original = GetOriginalWeaponTypeComboProc(combo);
		const LRESULT selection = original ?
			CallWindowProcA(original, combo, CB_GETCURSEL, 0, 0) :
			SendMessageA(combo, CB_GETCURSEL, 0, 0);
		return selection == CB_ERR ? CB_ERR : static_cast<int>(selection);
	}

	static LRESULT SetActualWeaponTypeComboSelection(HWND combo, int selection)
	{
		WNDPROC original = GetOriginalWeaponTypeComboProc(combo);
		return original ?
			CallWindowProcA(original, combo, CB_SETCURSEL, static_cast<WPARAM>(selection), 0) :
			SendMessageA(combo, CB_SETCURSEL, static_cast<WPARAM>(selection), 0);
	}

	static void BindWeaponTypeComboToForm(HWND combo, void* form)
	{
		if (!combo || ReadEditorFormType(form) != kFormTypeWeapon)
			return;

		const UInt32 formId = ReadEditorFormId(form);
		if (!formId)
			return;

		SetPropA(combo, kWeaponTypeComboBoundFormIdProp, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(formId)));
	}

	static UInt32 GetBoundWeaponTypeComboFormId(HWND combo)
	{
		const UInt32 formId = static_cast<UInt32>(reinterpret_cast<UINT_PTR>(GetPropA(combo, kWeaponTypeComboBoundFormIdProp)));
		if (!formId)
			return 0;

		void* form = LookupEditorFormById(formId);
		return ReadEditorFormType(form) == kFormTypeWeapon ? formId : 0;
	}

	static int __fastcall HookWeaponDialogInit(void* weapon, void*, HWND dialog)
	{
		const int result = g_weaponDialogInitOriginal ? g_weaponDialogInitOriginal(weapon, dialog) : 0;

		if (ReadEditorFormType(weapon) == kFormTypeWeapon)
		{
			HWND combo = GetDlgItem(dialog, kWeaponDialogTypeComboId);
			if (combo)
			{
				BindWeaponTypeComboToForm(combo, weapon);
				PatchWeaponTypeCombo(combo);
			}
		}

		return result;
	}

	static bool WindowTextContainsWeapon(HWND hwnd)
	{
		char title[256] = {};
		if (!hwnd || !GetWindowTextA(hwnd, title, sizeof(title)))
			return false;

		std::string normalized = NormalizeWeaponTypeLabel(title);
		return NormalizedContains(normalized, "weapon");
	}

	static HWND FindParentWeaponDialog(HWND combo)
	{
		for (HWND current = GetParent(combo); current; current = GetParent(current))
		{
			if (WindowTextContainsWeapon(current))
				return current;
		}

		return combo ? GetAncestor(combo, GA_ROOT) : nullptr;
	}

	static void* LookupWeaponFormByEditorId(const char* editorId)
	{
		if (!editorId || !editorId[0])
			return nullptr;

		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(editorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeWeapon ? form : nullptr;
	}

	static void* LookupUniqueWeaponFormByEditorId(const char* editorId)
	{
		if (!editorId || !editorId[0])
			return nullptr;

		void* dataHandler = GetEditorDataHandler();
		void* objectList = dataHandler ? *reinterpret_cast<void**>(dataHandler) : nullptr;
		if (!objectList)
			return LookupWeaponFormByEditorId(editorId);

		void* match = nullptr;
		void* current = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(objectList) + kBoundObjectListFirstOffset);
		const UInt32 objectCount = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(objectList) + kBoundObjectListCountOffset);
		const UInt32 scanLimit = objectCount && objectCount < 0x200000 ? objectCount + 16 : 0x200000;
		for (UInt32 scanned = 0; current && scanned < scanLimit; ++scanned)
		{
			if (ReadEditorFormType(current) == kFormTypeWeapon)
			{
				const char* candidateEditorId = ReadEditorFormEditorId(current);
				if (candidateEditorId && !_stricmp(candidateEditorId, editorId))
				{
					if (match && ReadEditorFormId(match) != ReadEditorFormId(current))
					{
						if (!g_loggedLegacySelfWeaponTypeAmbiguous)
						{
							_WARNING("BladeSeparation CS: ambiguous legacy $SELF weapon Type sidecar editor ID %s; row was not resolved", editorId);
							g_loggedLegacySelfWeaponTypeAmbiguous = true;
						}
						return nullptr;
					}

					match = current;
				}
			}

			current = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(current) + kTesObjectNextOffset);
		}

		return match;
	}

	static void* LookupNpcFormByEditorId(const char* editorId)
	{
		if (!editorId || !editorId[0])
			return nullptr;

		static const LookupEditorFormByEditorIDFn lookupByEditorId = reinterpret_cast<LookupEditorFormByEditorIDFn>(0x0047B340);
		void* form = lookupByEditorId ? lookupByEditorId(editorId) : nullptr;
		return ReadEditorFormType(form) == kFormTypeNpc ? form : nullptr;
	}

	static void* LookupUniqueNpcFormByEditorId(const char* editorId)
	{
		if (!editorId || !editorId[0])
			return nullptr;

		void* dataHandler = GetEditorDataHandler();
		void* objectList = dataHandler ? *reinterpret_cast<void**>(dataHandler) : nullptr;
		if (!objectList)
			return LookupNpcFormByEditorId(editorId);

		void* match = nullptr;
		void* current = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(objectList) + kBoundObjectListFirstOffset);
		const UInt32 objectCount = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(objectList) + kBoundObjectListCountOffset);
		const UInt32 scanLimit = objectCount && objectCount < 0x200000 ? objectCount + 16 : 0x200000;
		for (UInt32 scanned = 0; current && scanned < scanLimit; ++scanned)
		{
			if (ReadEditorFormType(current) == kFormTypeNpc)
			{
				const char* candidateEditorId = ReadEditorFormEditorId(current);
				if (candidateEditorId && !_stricmp(candidateEditorId, editorId))
				{
					if (match && ReadEditorFormId(match) != ReadEditorFormId(current))
					{
						if (!g_loggedLegacySelfNpcSkillAmbiguous)
						{
							_WARNING("BladeSeparation CS: ambiguous legacy $SELF NPC skill sidecar editor ID %s; row was not resolved", editorId);
							g_loggedLegacySelfNpcSkillAmbiguous = true;
						}
						return nullptr;
					}

					match = current;
				}
			}

			current = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(current) + kTesObjectNextOffset);
		}

		return match;
	}

	static void* FindWeaponFormFromDialogTitle(HWND dialog)
	{
		char title[512] = {};
		if (!dialog || !GetWindowTextA(dialog, title, sizeof(title)))
			return nullptr;

		const char* open = std::strchr(title, '[');
		const char* close = open ? std::strchr(open + 1, ']') : nullptr;
		if (open && close && close > open + 1)
		{
			std::string editorId(open + 1, close);
			if (void* form = LookupWeaponFormByEditorId(editorId.c_str()))
				return form;
		}

		return LookupWeaponFormByEditorId(title);
	}

	struct FindWeaponFormInEditsContext
	{
		void* form;
	};

	static BOOL CALLBACK FindWeaponFormInEdits(HWND hwnd, LPARAM lParam)
	{
		FindWeaponFormInEditsContext* context = reinterpret_cast<FindWeaponFormInEditsContext*>(lParam);
		if (!context || context->form)
			return FALSE;

		char className[32] = {};
		if (!GetClassNameA(hwnd, className, sizeof(className)) || _stricmp(className, "Edit"))
			return TRUE;

		char text[256] = {};
		if (!GetWindowTextA(hwnd, text, sizeof(text)) || !text[0])
			return TRUE;

		context->form = LookupWeaponFormByEditorId(text);
		return context->form ? FALSE : TRUE;
	}

	static void* FindEditedWeaponFormForCombo(HWND combo)
	{
		HWND dialog = FindParentWeaponDialog(combo);
		if (!dialog)
			return nullptr;

		if (void* form = FindWeaponFormFromDialogTitle(dialog))
			return form;

		FindWeaponFormInEditsContext context = {};
		EnumChildWindows(dialog, FindWeaponFormInEdits, reinterpret_cast<LPARAM>(&context));
		return context.form;
	}

	static bool GetEditedWeaponFormIdForCombo(HWND combo, UInt32* formId)
	{
		if (formId)
			*formId = 0;

		if (const UInt32 boundFormId = GetBoundWeaponTypeComboFormId(combo))
		{
			if (formId)
				*formId = boundFormId;
			return true;
		}

		void* form = FindEditedWeaponFormForCombo(combo);
		if (ReadEditorFormType(form) != kFormTypeWeapon)
			return false;

		const UInt32 id = ReadEditorFormId(form);
		if (!id)
			return false;

		if (formId)
			*formId = id;
		return true;
	}

	static void PersistWeaponTypeSelection(HWND combo)
	{
		if (!combo || GetPropA(combo, kWeaponTypeComboApplyingStoredSelectionProp))
			return;

		LoadWeaponTypeStoreOnce();

		const int selection = GetActualWeaponTypeComboSelection(combo);
		if (selection == CB_ERR)
			return;

		std::string value;
		if (!ReadComboString(combo, selection, value))
			return;

		UInt32 formId = 0;
		if (!GetEditedWeaponFormIdForCombo(combo, &formId))
		{
			if (!g_loggedWeaponTypeNoForm)
			{
				_WARNING("BladeSeparation CS: cannot persist weapon Type sidecar; edited WEAP form was not resolved for combo=%p", combo);
				g_loggedWeaponTypeNoForm = true;
			}
			return;
		}
		SetPropA(combo, kWeaponTypeComboStoredSelectionAppliedProp, reinterpret_cast<HANDLE>(1));

		const BladeSeparationShared::WeaponSkillKind kind = KindForSidecarWeaponTypeLabel(value.c_str());
		bool changed = false;
		if (kind != BladeSeparationShared::kWeaponSkill_None)
		{
			BladeSeparationShared::WeaponSkillKind existing = BladeSeparationShared::kWeaponSkill_None;
			const bool hadExisting = g_weaponTypeStore.TryGet(formId, &existing);
			if (!hadExisting || existing != kind)
				changed = true;
			if (!g_weaponTypeStore.Set(formId, kind))
			{
				_WARNING("BladeSeparation CS: weapon Type sidecar table is full; cannot store %08X", formId);
				return;
			}
		}
		else
		{
			changed = g_weaponTypeStore.Remove(formId);
		}

		if (changed)
			RefreshVisibleListViews();

		if (changed || g_weaponTypeStore.IsDirty())
		{
			if (!GetActiveEditorFile())
			{
				if (!g_loggedWeaponTypeNoActivePlugin)
				{
					_WARNING("BladeSeparation CS: weapon %08X Type sidecar=%s is affixed for this editor session only; activate a plugin to persist it to BSEPWeaponTypeData",
						formId,
						kind != BladeSeparationShared::kWeaponSkill_None ? value.c_str() : "native");
					g_loggedWeaponTypeNoActivePlugin = true;
				}
				return;
			}

			SaveWeaponTypeStore();
		}
	}

	static bool ApplyStoredWeaponTypeSelection(HWND combo)
	{
		if (!combo)
			return false;

		LoadWeaponTypeStoreOnce();

		UInt32 formId = 0;
		if (!GetEditedWeaponFormIdForCombo(combo, &formId))
			return false;

		BladeSeparationShared::WeaponSkillKind kind = BladeSeparationShared::kWeaponSkill_None;
		if (!g_weaponTypeStore.TryGet(formId, &kind))
			return true;

		const char* label = LabelForSidecarWeaponTypeKind(kind);
		const int index = FindComboStringExact(combo, label);
		if (index == CB_ERR)
			return true;

		const int current = GetActualWeaponTypeComboSelection(combo);
		std::string currentValue;
		if (current != CB_ERR && ReadComboString(combo, current, currentValue) && !_stricmp(currentValue.c_str(), label))
			return true;

		SetPropA(combo, kWeaponTypeComboApplyingStoredSelectionProp, reinterpret_cast<HANDLE>(1));
		SendMessageA(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
		RemovePropA(combo, kWeaponTypeComboApplyingStoredSelectionProp);
		RefreshWeaponTypeComboDisplay(combo);
		return true;
	}

	static bool PrepareNativeWeaponTypeSelectionForSave(HWND combo, int* restoreSelection)
	{
		if (restoreSelection)
			*restoreSelection = CB_ERR;
		if (!combo)
			return false;

		const int selection = GetActualWeaponTypeComboSelection(combo);
		if (selection == CB_ERR)
			return false;

		std::string value;
		if (!ReadComboString(combo, selection, value) || !IsSidecarWeaponTypeLabel(value.c_str()))
			return false;

		const int fallback = FindNativeWeaponTypeString(combo,
			NativeFallbackLabelForSidecarWeaponTypeKind(KindForSidecarWeaponTypeLabel(value.c_str())));
		if (fallback == CB_ERR)
			return false;

		SetPropA(combo, kWeaponTypeComboApplyingStoredSelectionProp, reinterpret_cast<HANDLE>(1));
		SetActualWeaponTypeComboSelection(combo, fallback);
		RemovePropA(combo, kWeaponTypeComboApplyingStoredSelectionProp);
		if (restoreSelection)
			*restoreSelection = selection;
		return true;
	}

	static void RestoreWeaponTypeSelectionAfterSave(HWND combo, int restoreSelection)
	{
		if (!combo || restoreSelection == CB_ERR || !IsWindow(combo))
			return;

		SetPropA(combo, kWeaponTypeComboApplyingStoredSelectionProp, reinterpret_cast<HANDLE>(1));
		SetActualWeaponTypeComboSelection(combo, restoreSelection);
		RemovePropA(combo, kWeaponTypeComboApplyingStoredSelectionProp);
		RefreshWeaponTypeComboDisplay(combo);
	}

	static WNDPROC GetOriginalWeaponTypeParentProc(HWND parent)
	{
		return reinterpret_cast<WNDPROC>(GetPropA(parent, kWeaponTypeParentOriginalProcProp));
	}

	static LRESULT CALLBACK WeaponTypeParentProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC original = GetOriginalWeaponTypeParentProc(hwnd);
		if (!original)
			return DefWindowProcA(hwnd, msg, wParam, lParam);

		if (msg == WM_NCDESTROY)
		{
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
			RemovePropA(hwnd, kWeaponTypeParentPatchedProp);
			RemovePropA(hwnd, kWeaponTypeParentOriginalProcProp);
			RemovePropA(hwnd, kWeaponTypeParentComboProp);
			return CallWindowProcA(original, hwnd, msg, wParam, lParam);
		}

		if (msg == WM_COMMAND && lParam)
		{
			HWND combo = reinterpret_cast<HWND>(lParam);
			const UINT notification = HIWORD(wParam);
			if (GetPropA(combo, kWeaponTypeComboPatchedProp) &&
				(notification == CBN_SELENDOK || notification == CBN_CLOSEUP))
			{
				RememberNativeWeaponTypeSelection(combo);
				PersistWeaponTypeSelection(combo);
			}
		}

		if (msg == WM_COMMAND && LOWORD(wParam) == IDOK)
		{
			HWND combo = reinterpret_cast<HWND>(GetPropA(hwnd, kWeaponTypeParentComboProp));
			if (combo && GetPropA(combo, kWeaponTypeComboPatchedProp))
			{
				PersistWeaponTypeSelection(combo);
				int restoreSelection = CB_ERR;
				PrepareNativeWeaponTypeSelectionForSave(combo, &restoreSelection);
				const LRESULT result = CallWindowProcA(original, hwnd, msg, wParam, lParam);
				RestoreWeaponTypeSelectionAfterSave(combo, restoreSelection);
				return result;
			}
		}

		const LRESULT result = CallWindowProcA(original, hwnd, msg, wParam, lParam);

		return result;
	}

	static void PatchWeaponTypeParentWindow(HWND parent, HWND combo)
	{
		if (!parent)
			return;

		SetPropA(parent, kWeaponTypeParentComboProp, combo);
		if (GetPropA(parent, kWeaponTypeParentPatchedProp))
			return;

		WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(parent, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WeaponTypeParentProc)));
		if (!original)
		{
			RemovePropA(parent, kWeaponTypeParentComboProp);
			return;
		}

		SetPropA(parent, kWeaponTypeParentOriginalProcProp, reinterpret_cast<HANDLE>(original));
		SetPropA(parent, kWeaponTypeParentPatchedProp, reinterpret_cast<HANDLE>(1));
	}

	static void PatchWeaponTypeParentDialog(HWND combo)
	{
		PatchWeaponTypeParentWindow(GetParent(combo), combo);

		HWND dialog = FindParentWeaponDialog(combo);
		if (dialog && dialog != GetParent(combo))
			PatchWeaponTypeParentWindow(dialog, combo);
	}

	static bool ComboHasString(HWND combo, const char* text)
	{
		return FindNativeWeaponTypeString(combo, text) != CB_ERR;
	}

	static bool IsWeaponTypeCombo(HWND combo)
	{
		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		return IsComboBox(combo) &&
			GetDlgCtrlID(combo) == kWeaponDialogTypeComboId &&
			count >= 6 &&
			ComboHasString(combo, kNativeWeaponTypeBladeOneHand) &&
			ComboHasString(combo, kNativeWeaponTypeBladeTwoHand) &&
			ComboHasString(combo, kNativeWeaponTypeBluntOneHand) &&
			ComboHasString(combo, kNativeWeaponTypeBluntTwoHand) &&
			ComboHasString(combo, kNativeWeaponTypeStaff) &&
			ComboHasString(combo, kNativeWeaponTypeBow);
	}

	static void MakeWeaponTypeComboScrollable(HWND combo)
	{
		if (!combo)
			return;
		if (GetPropA(combo, kWeaponTypeComboScrollableProp))
			return;
		if (SendMessageA(combo, CB_GETDROPPEDSTATE, 0, 0))
			return;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		const int itemHeight = static_cast<int>(SendMessageA(combo, CB_GETITEMHEIGHT, 0, 0));
		const int selectionHeight = static_cast<int>(SendMessageA(combo, CB_GETITEMHEIGHT, static_cast<WPARAM>(-1), 0));
		if (count <= 0 || itemHeight <= 0 || selectionHeight <= 0)
			return;

		SendMessageA(combo, CB_SETMINVISIBLE, static_cast<WPARAM>(kWeaponTypeComboMinimumVisibleRows), 0);

		RECT rect = {};
		if (!GetWindowRect(combo, &rect))
			return;

		const int width = rect.right - rect.left;
		const int visibleRows = std::min(std::max(count, kWeaponTypeComboMinimumVisibleRows), kWeaponTypeComboMinimumVisibleRows);
		const int height = selectionHeight + (itemHeight * visibleRows) + 10;
		const LONG_PTR style = GetWindowLongPtrA(combo, GWL_STYLE);
		SetWindowLongPtrA(combo, GWL_STYLE, style | WS_VSCROLL | CBS_DISABLENOSCROLL);
		SetWindowPos(combo, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		SetPropA(combo, kWeaponTypeComboScrollableProp, reinterpret_cast<HANDLE>(1));
	}

	static void RememberNativeWeaponTypeSelection(HWND combo)
	{
		const int selection = GetActualWeaponTypeComboSelection(combo);
		if (selection == CB_ERR)
			return;

		std::string value;
		if (ReadComboString(combo, selection, value) &&
			!IsSidecarWeaponTypeLabel(value.c_str()))
			SetPropA(combo, kWeaponTypeComboLastNativeSelectionProp, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(selection + 1)));
	}

	static WNDPROC GetOriginalWeaponTypeComboProc(HWND combo)
	{
		return reinterpret_cast<WNDPROC>(GetPropA(combo, kWeaponTypeComboOriginalProcProp));
	}

	static LRESULT CALLBACK WeaponTypeComboProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		WNDPROC original = GetOriginalWeaponTypeComboProc(hwnd);
		if (!original)
			return DefWindowProcA(hwnd, msg, wParam, lParam);

		if (msg == WM_NCDESTROY)
		{
			SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
			RemovePropA(hwnd, kWeaponTypeComboPatchedProp);
			RemovePropA(hwnd, kWeaponTypeComboOriginalProcProp);
			RemovePropA(hwnd, kWeaponTypeComboLastNativeSelectionProp);
			RemovePropA(hwnd, kWeaponTypeComboSidecarMarkerProp);
			RemovePropA(hwnd, kWeaponTypeComboScrollableProp);
			RemovePropA(hwnd, kWeaponTypeComboApplyingStoredSelectionProp);
			RemovePropA(hwnd, kWeaponTypeComboStoredSelectionAppliedProp);
			RemovePropA(hwnd, kWeaponTypeComboRejectedProp);
			RemovePropA(hwnd, kWeaponTypeComboBoundFormIdProp);
			return CallWindowProcA(original, hwnd, msg, wParam, lParam);
		}

		if (msg == WM_MBUTTONDOWN)
		{
			SetFocus(hwnd);
			SendMessageA(hwnd, CB_SHOWDROPDOWN, TRUE, 0);
			return 0;
		}

		const LRESULT result = CallWindowProcA(original, hwnd, msg, wParam, lParam);
		if (msg == CB_SETCURSEL || msg == WM_SETTEXT)
			RememberNativeWeaponTypeSelection(hwnd);
		return result;
	}

	static void AddSidecarWeaponTypeComboItem(HWND combo, const char* label, const char* nativeFallback)
	{
		if (ComboHasString(combo, label))
			return;

		const int fallback = FindNativeWeaponTypeString(combo, nativeFallback);
		if (fallback != CB_ERR)
		{
			LRESULT fallbackData = SendMessageA(combo, CB_GETITEMDATA, fallback, 0);
			if (fallbackData == CB_ERR)
				fallbackData = fallback;
			SendMessageA(combo, kCseDeferredComboAddItem, reinterpret_cast<WPARAM>(label), fallbackData);
			SendMessageA(combo, CB_GETCOUNT, 0, 0);
			if (ComboHasString(combo, label))
				return;

			const int index = static_cast<int>(SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label)));
			if (index != CB_ERR && index != CB_ERRSPACE && index != kCseDeferredAddStringMarkerResult)
				SendMessageA(combo, CB_SETITEMDATA, index, fallbackData);
		}
	}

	static void PatchWeaponTypeCombo(HWND combo)
	{
		if (g_insideWeaponTypeComboPatch || !combo)
			return;

		if (GetPropA(combo, kWeaponTypeComboPatchedProp))
		{
			if (!GetPropA(combo, kWeaponTypeComboStoredSelectionAppliedProp) &&
				!SendMessageA(combo, CB_GETDROPPEDSTATE, 0, 0) &&
				ApplyStoredWeaponTypeSelection(combo))
			{
				SetPropA(combo, kWeaponTypeComboStoredSelectionAppliedProp, reinterpret_cast<HANDLE>(1));
			}
			return;
		}

		if (GetPropA(combo, kWeaponTypeComboRejectedProp))
			return;
		if (SendMessageA(combo, CB_GETDROPPEDSTATE, 0, 0))
			return;

		const int count = static_cast<int>(SendMessageA(combo, CB_GETCOUNT, 0, 0));
		if (count < 6)
			return;
		if (!IsWeaponTypeCombo(combo))
		{
			SetPropA(combo, kWeaponTypeComboRejectedProp, reinterpret_cast<HANDLE>(1));
			return;
		}

		g_insideWeaponTypeComboPatch = true;
		RememberNativeWeaponTypeSelection(combo);
		AddSidecarWeaponTypeComboItem(combo, kLongBladeEditorLabel, kNativeWeaponTypeBladeOneHand);
		AddSidecarWeaponTypeComboItem(combo, kShortBladeEditorLabel, kNativeWeaponTypeBladeOneHand);
		AddSidecarWeaponTypeComboItem(combo, kAxeEditorLabel, kNativeWeaponTypeBluntOneHand);
		MakeWeaponTypeComboScrollable(combo);

		if (!GetPropA(combo, kWeaponTypeComboPatchedProp))
		{
			WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(combo, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WeaponTypeComboProc)));
			if (original)
			{
				SetPropA(combo, kWeaponTypeComboOriginalProcProp, reinterpret_cast<HANDLE>(original));
				SetPropA(combo, kWeaponTypeComboPatchedProp, reinterpret_cast<HANDLE>(1));
			}
		}
		PatchWeaponTypeParentDialog(combo);
		if (ApplyStoredWeaponTypeSelection(combo))
			SetPropA(combo, kWeaponTypeComboStoredSelectionAppliedProp, reinterpret_cast<HANDLE>(1));
		g_insideWeaponTypeComboPatch = false;
	}

	static BOOL CALLBACK PatchWeaponTypeCombosInWindow(HWND hwnd, LPARAM)
	{
		if (!IsWindowVisible(hwnd))
			return TRUE;
		if (IsComboBox(hwnd))
			PatchWeaponTypeCombo(hwnd);
		else
			EnumChildWindows(hwnd, PatchWeaponTypeCombosInWindow, 0);
		return TRUE;
	}

	static void PatchVisibleWeaponTypeCombos()
	{
		EnumThreadWindows(GetCurrentThreadId(), PatchWeaponTypeCombosInWindow, 0);
	}

	static void CALLBACK WeaponTypeComboPollTimerProc(HWND, UINT, UINT_PTR, DWORD)
	{
		PatchVisibleWeaponTypeCombos();
	}

	static void StartWeaponTypeDropdownPolling()
	{
		if (!g_weaponTypeComboPollTimer)
			g_weaponTypeComboPollTimer = SetTimer(nullptr, kWeaponTypeComboPollTimerId, kWeaponTypeComboPollIntervalMs, WeaponTypeComboPollTimerProc);
		if (!g_weaponTypeComboPollTimer)
			_WARNING("BladeSeparation CS: failed to start weapon Type dropdown sidecar polling gle=%u", GetLastError());
	}

	static void ExtendVanillaWeaponTypeDropdowns()
	{
		StartWeaponTypeDropdownPolling();
	}

	static bool BytesEqual(UInt32 address, const UInt8* expected, UInt32 length)
	{
		return std::memcmp(reinterpret_cast<const void*>(address), expected, length) == 0;
	}

	static UInt32 ReadRelJumpTarget(UInt32 address)
	{
		const SInt32 offset = *reinterpret_cast<const SInt32*>(address + 1);
		return address + 5 + offset;
	}

	static bool WriteRelJumpChecked(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target)
	{
		if (expectedLength < 5)
		{
			_ERROR("BladeSeparation CS: invalid patch length for %s at %08X length=%u", name, address, expectedLength);
			return false;
		}

		if (!BytesEqual(address, expected, expectedLength))
		{
			const UInt8* actual = reinterpret_cast<const UInt8*>(address);
			if (actual[0] == 0xE9)
			{
				const UInt32 existingTarget = ReadRelJumpTarget(address);
				if (existingTarget == target)
					_MESSAGE("BladeSeparation CS: %s already installed at %08X", name, address);
				else
					_MESSAGE("BladeSeparation CS: %s already patched at %08X by %08X; leaving existing hook in place",
						name, address, existingTarget);

				return true;
			}

			_ERROR("BladeSeparation CS: signature mismatch for %s at %08X expected %02X %02X %02X %02X %02X actual %02X %02X %02X %02X %02X",
				name, address,
				expected[0], expected[1], expected[2], expected[3], expected[4],
				actual[0], actual[1], actual[2], actual[3], actual[4]);
			return false;
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, expectedLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			_ERROR("BladeSeparation CS: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < expectedLength; ++i)
			code[i] = 0x90;

		FlushInstructionCache(GetCurrentProcess(), ptr, expectedLength);

		DWORD ignored = 0;
		VirtualProtect(ptr, expectedLength, oldProtect, &ignored);

		_MESSAGE("BladeSeparation CS: installed %s at %08X", name, address);
		return true;
	}

	static bool WriteRelJumpChainedChecked(const char* name, UInt32 address, const UInt8* expected,
		UInt32 expectedLength, UInt32 target, UInt32* chainTargetOut)
	{
		if (chainTargetOut)
			*chainTargetOut = 0;

		if (expectedLength < 5)
		{
			_ERROR("BladeSeparation CS: invalid patch length for %s at %08X length=%u", name, address, expectedLength);
			return false;
		}

		if (!BytesEqual(address, expected, expectedLength))
		{
			const UInt8* actual = reinterpret_cast<const UInt8*>(address);
			if (actual[0] == 0xE9)
			{
				const UInt32 existingTarget = ReadRelJumpTarget(address);
				if (existingTarget == target)
				{
					_MESSAGE("BladeSeparation CS: %s already installed at %08X", name, address);
					return true;
				}

				if (chainTargetOut)
					*chainTargetOut = existingTarget;
				_MESSAGE("BladeSeparation CS: %s chaining existing hook at %08X target=%08X",
					name, address, existingTarget);
			}
			else
			{
				_ERROR("BladeSeparation CS: signature mismatch for %s at %08X expected %02X %02X %02X %02X %02X actual %02X %02X %02X %02X %02X",
					name, address,
					expected[0], expected[1], expected[2], expected[3], expected[4],
					actual[0], actual[1], actual[2], actual[3], actual[4]);
				return false;
			}
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, expectedLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			_ERROR("BladeSeparation CS: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < expectedLength; ++i)
			code[i] = 0x90;

		FlushInstructionCache(GetCurrentProcess(), ptr, expectedLength);

		DWORD ignored = 0;
		VirtualProtect(ptr, expectedLength, oldProtect, &ignored);

		_MESSAGE("BladeSeparation CS: installed %s at %08X", name, address);
		return true;
	}

	static bool WriteRelCallChainedChecked(const char* name, UInt32 address, const UInt8* expected,
		UInt32 expectedLength, UInt32 target, UInt32 expectedOriginalTarget, UInt32* originalOut, bool* originalIsFastcall)
	{
		if (originalOut)
			*originalOut = expectedOriginalTarget;
		if (originalIsFastcall)
			*originalIsFastcall = false;
		if (expectedLength < 5)
		{
			_ERROR("BladeSeparation CS: invalid patch length for %s at %08X length=%u", name, address, expectedLength);
			return false;
		}

		if (!BytesEqual(address, expected, expectedLength))
		{
			const UInt8* actual = reinterpret_cast<const UInt8*>(address);
			if (actual[0] == 0xE8)
			{
				const UInt32 existingTarget = ReadRelJumpTarget(address);
				if (existingTarget == target)
				{
					_MESSAGE("BladeSeparation CS: %s already installed at %08X", name, address);
					return true;
				}

				if (originalOut)
					*originalOut = existingTarget;
				if (originalIsFastcall)
					*originalIsFastcall = true;
				_MESSAGE("BladeSeparation CS: %s chaining existing call hook at %08X target=%08X",
					name, address, existingTarget);
			}
			else
			{
				_ERROR("BladeSeparation CS: signature mismatch for %s at %08X expected %02X %02X %02X %02X %02X actual %02X %02X %02X %02X %02X",
					name, address,
					expected[0], expected[1], expected[2], expected[3], expected[4],
					actual[0], actual[1], actual[2], actual[3], actual[4]);
				return false;
			}
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, expectedLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			_ERROR("BladeSeparation CS: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE8;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < expectedLength; ++i)
			code[i] = 0x90;

		FlushInstructionCache(GetCurrentProcess(), ptr, expectedLength);

		DWORD ignored = 0;
		VirtualProtect(ptr, expectedLength, oldProtect, &ignored);

		_MESSAGE("BladeSeparation CS: installed %s at %08X", name, address);
		return true;
	}

	static bool WritePointerChainedChecked(const char* name, UInt32 address, UInt32 expected, UInt32 target,
		UInt32* originalOut, bool* originalIsFastcall)
	{
		if (originalOut)
			*originalOut = expected;
		if (originalIsFastcall)
			*originalIsFastcall = false;

		const UInt32 actual = *reinterpret_cast<const UInt32*>(address);
		if (actual != expected)
		{
			if (actual == target)
			{
				_MESSAGE("BladeSeparation CS: %s already installed at %08X", name, address);
				return true;
			}

			if (originalOut)
				*originalOut = actual;
			if (originalIsFastcall)
				*originalIsFastcall = true;
			_MESSAGE("BladeSeparation CS: %s chaining existing pointer hook at %08X target=%08X",
				name, address, actual);
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, sizeof(UInt32), PAGE_READWRITE, &oldProtect))
		{
			_ERROR("BladeSeparation CS: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		*reinterpret_cast<UInt32*>(ptr) = target;
		FlushInstructionCache(GetCurrentProcess(), ptr, sizeof(UInt32));

		DWORD ignored = 0;
		VirtualProtect(ptr, sizeof(UInt32), oldProtect, &ignored);

		_MESSAGE("BladeSeparation CS: installed %s at %08X", name, address);
		return true;
	}

	static bool WriteRelJumpWithTrampolineChecked(const char* name, UInt32 address, const UInt8* expected,
		UInt32 expectedLength, UInt32 target, UInt8* trampoline, UInt32 trampolineLength, UInt32* originalOut)
	{
		if (expectedLength < 5 || trampolineLength < expectedLength + 5)
		{
			_ERROR("BladeSeparation CS: invalid trampoline patch length for %s at %08X length=%u trampoline=%u",
				name, address, expectedLength, trampolineLength);
			return false;
		}

		if (!BytesEqual(address, expected, expectedLength))
		{
			const UInt8* actual = reinterpret_cast<const UInt8*>(address);
			if (actual[0] == 0xE9)
			{
				const UInt32 existingTarget = ReadRelJumpTarget(address);
				if (existingTarget == target)
				{
					_MESSAGE("BladeSeparation CS: %s already installed at %08X", name, address);
					return true;
				}
				else
				{
					if (originalOut)
						*originalOut = existingTarget;
					_MESSAGE("BladeSeparation CS: %s chaining existing hook at %08X target=%08X",
						name, address, existingTarget);
				}
			}
			else
			{
				_ERROR("BladeSeparation CS: signature mismatch for %s at %08X expected %02X %02X %02X %02X %02X actual %02X %02X %02X %02X %02X",
					name, address,
					expected[0], expected[1], expected[2], expected[3], expected[4],
					actual[0], actual[1], actual[2], actual[3], actual[4]);
				return false;
			}
		}
		else
		{
			std::memcpy(trampoline, reinterpret_cast<const void*>(address), expectedLength);
			trampoline[expectedLength] = 0xE9;
			*reinterpret_cast<UInt32*>(trampoline + expectedLength + 1) =
				address + expectedLength - (reinterpret_cast<UInt32>(trampoline) + expectedLength + 5);

			DWORD oldTrampolineProtect = 0;
			if (!VirtualProtect(trampoline, trampolineLength, PAGE_EXECUTE_READWRITE, &oldTrampolineProtect))
			{
				_ERROR("BladeSeparation CS: VirtualProtect failed for %s trampoline gle=%u", name, GetLastError());
				return false;
			}
			FlushInstructionCache(GetCurrentProcess(), trampoline, trampolineLength);

			if (originalOut)
				*originalOut = reinterpret_cast<UInt32>(trampoline);
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, expectedLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			_ERROR("BladeSeparation CS: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			return false;
		}

		UInt8* code = reinterpret_cast<UInt8*>(ptr);
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < expectedLength; ++i)
			code[i] = 0x90;

		FlushInstructionCache(GetCurrentProcess(), ptr, expectedLength);

		DWORD ignored = 0;
		VirtualProtect(ptr, expectedLength, oldProtect, &ignored);

		_MESSAGE("BladeSeparation CS: installed %s at %08X", name, address);
		return true;
	}

	static bool InstallWeaponDialogTypeComboHook()
	{
		UInt32 weaponDialogInitOriginal = 0;
		const bool ok = WriteRelJumpWithTrampolineChecked("weapon dialog Type combo sidecar initialization",
			kWeaponDialogInitPatch, kExpectedWeaponDialogInit, sizeof(kExpectedWeaponDialogInit),
			reinterpret_cast<UInt32>(&HookWeaponDialogInit),
			g_weaponDialogInitTrampoline, sizeof(g_weaponDialogInitTrampoline),
			&weaponDialogInitOriginal);
		if (weaponDialogInitOriginal)
			g_weaponDialogInitOriginal = reinterpret_cast<WeaponDialogInitFn>(weaponDialogInitOriginal);
		return ok;
	}

	static bool InstallObjectWindowWeaponTypeColumnHook()
	{
		UInt32 objectWindowOriginal = 0;
		const bool ok = WriteRelJumpWithTrampolineChecked("object window weapon Type sidecar display",
			kObjectWindowListViewGetDispInfoPatch, kExpectedObjectWindowListViewGetDispInfo,
			sizeof(kExpectedObjectWindowListViewGetDispInfo), reinterpret_cast<UInt32>(&ObjectWindowListViewGetDispInfoHook),
			g_objectWindowListViewGetDispInfoTrampoline, sizeof(g_objectWindowListViewGetDispInfoTrampoline),
			&objectWindowOriginal);
		if (objectWindowOriginal)
			g_objectWindowListViewGetDispInfoOriginal = reinterpret_cast<ObjectWindowListViewGetDispInfoFn>(objectWindowOriginal);
		return ok;
	}

	static bool InstallNpcStatsSidecarHooks()
	{
		LoadNpcSkillStoreOnce();

		bool ok = true;
		UInt32 populateOriginal = kNpcStatsPopulateOriginal;
		bool populateFastcall = false;
		ok &= WritePointerChainedChecked("NPC stats BladeSeparation sidecar rows",
			kNpcStatsPopulateVtableEntry, kNpcStatsPopulateOriginal, reinterpret_cast<UInt32>(&NpcStatsPopulateHook),
			&populateOriginal, &populateFastcall);
		g_npcStatsPopulateOriginalTarget = populateOriginal;
		g_npcStatsPopulateOriginalIsFastcall = populateFastcall;

		UInt32 initOriginal = kNpcStatsInitSkillListOriginal;
		bool initFastcall = false;
		ok &= WriteRelCallChainedChecked("NPC stats BladeSeparation skill list open",
			kNpcStatsInitSkillListOpenCall, kExpectedNpcStatsInitSkillListOpenCall, sizeof(kExpectedNpcStatsInitSkillListOpenCall),
			reinterpret_cast<UInt32>(&NpcStatsInitSkillListHook), kNpcStatsInitSkillListOriginal,
			&initOriginal, &initFastcall);
		g_npcStatsInitSkillListOriginalTarget = initOriginal;
		g_npcStatsInitSkillListOriginalIsFastcall = initFastcall;

		initOriginal = kNpcStatsInitSkillListOriginal;
		initFastcall = false;
		ok &= WriteRelCallChainedChecked("NPC stats BladeSeparation skill list refresh",
			kNpcStatsInitSkillListCall, kExpectedNpcStatsInitSkillListCall, sizeof(kExpectedNpcStatsInitSkillListCall),
			reinterpret_cast<UInt32>(&NpcStatsInitSkillListHook), kNpcStatsInitSkillListOriginal,
			&initOriginal, &initFastcall);
		if (g_npcStatsInitSkillListOriginalTarget == kNpcStatsInitSkillListOriginal || initFastcall)
		{
			g_npcStatsInitSkillListOriginalTarget = initOriginal;
			g_npcStatsInitSkillListOriginalIsFastcall = initFastcall;
		}

		UInt32 editChainTarget = 0;
		ok &= WriteRelJumpChainedChecked("NPC stats BladeSeparation auto-calc sidecar value save",
			kNpcStatsSkillEditAutoCalcGatePatch, kExpectedNpcStatsSkillEditAutoCalcGate, sizeof(kExpectedNpcStatsSkillEditAutoCalcGate),
			reinterpret_cast<UInt32>(&NpcStatsSkillEditAutoCalcGateHook), &editChainTarget);
		g_npcStatsSkillEditAutoCalcGateChainTarget = editChainTarget;

		editChainTarget = 0;
		ok &= WriteRelJumpChainedChecked("NPC stats BladeSeparation sidecar value save",
			kNpcStatsSkillEditCommitPatch, kExpectedNpcStatsSkillEditCommit, sizeof(kExpectedNpcStatsSkillEditCommit),
			reinterpret_cast<UInt32>(&NpcStatsSkillEditCommitHook), &editChainTarget);
		g_npcStatsSkillEditCommitChainTarget = editChainTarget;
		return ok;
	}

	static bool InstallNpcAiTrainingSidecarHooks()
	{
		LoadNpcTrainingStoreOnce();

		bool ok = true;
		UInt32 aiInitOriginal = 0;
		ok &= WriteRelJumpWithTrampolineChecked("NPC AI BladeSeparation training dropdown initialization",
			kAiFormInitDialogControls, kExpectedAiFormInitDialogControls, sizeof(kExpectedAiFormInitDialogControls),
			reinterpret_cast<UInt32>(&AiFormInitDialogControlsHook),
			g_aiFormInitDialogControlsTrampoline, sizeof(g_aiFormInitDialogControlsTrampoline),
			&aiInitOriginal);
		if (aiInitOriginal)
			g_aiFormInitDialogControlsOriginal = reinterpret_cast<AiFormInitDialogControlsFn>(aiInitOriginal);

		UInt32 aiSaveOriginal = 0;
		ok &= WriteRelJumpWithTrampolineChecked("NPC AI BladeSeparation training sidecar save",
			kAiFormSaveDialogControls, kExpectedAiFormSaveDialogControls, sizeof(kExpectedAiFormSaveDialogControls),
			reinterpret_cast<UInt32>(&AiFormSaveDialogControlsHook),
			g_aiFormSaveDialogControlsTrampoline, sizeof(g_aiFormSaveDialogControlsTrampoline),
			&aiSaveOriginal);
		if (aiSaveOriginal)
			g_aiFormSaveDialogControlsOriginal = reinterpret_cast<AiFormSaveDialogControlsFn>(aiSaveOriginal);

		UInt32 saveChainTarget = 0;
		ok &= WriteRelJumpChainedChecked("BladeSeparation sidecar pre-save carrier flush",
			kSavePluginPreWritePatch, kExpectedSavePluginPreWrite, sizeof(kExpectedSavePluginPreWrite),
			reinterpret_cast<UInt32>(&SavePluginPreWriteHook), &saveChainTarget);
		g_savePluginPreWriteChainTarget = saveChainTarget;
		return ok;
	}

	static void InitializeEditorWeaponTypeSurface()
	{
		(void)IsNativeBladeCompatibilityActorValue(kNativeBladeCompatibilityActorValue);
		RegisterSidecarsAlongsideVanillaEditorSurface();
		ExtendVanillaWeaponTypeDropdowns();
		if (!InstallWeaponDialogTypeComboHook())
			_WARNING("BladeSeparation CS: weapon dialog Type sidecar initialization hook was not installed");
		if (!InstallObjectWindowWeaponTypeColumnHook())
			_WARNING("BladeSeparation CS: object window weapon Type sidecar display hook was not installed");
	}

	static void InitializeEditorNpcStatsSurface()
	{
		if (!InstallNpcStatsSidecarHooks())
			_WARNING("BladeSeparation CS: NPC stats sidecar hooks were not fully installed");
		if (!InstallNpcAiTrainingSidecarHooks())
			_WARNING("BladeSeparation CS: NPC AI training sidecar hooks were not fully installed");
	}

	static bool ProviderIsBladeSkill(unsigned long skillId)
	{
		return skillId == BladeSeparationShared::kLongSkillId ||
			skillId == BladeSeparationShared::kShortSkillId ||
			skillId == BladeSeparationShared::kAxeSkillId;
	}

	static bool ProviderValueToLevel(double value, UInt32* outLevel)
	{
		if (outLevel)
			*outLevel = 0;
		if (!(value == value))
			return false;
		if (value <= 0.0)
			return true;
		if (value >= 100.0)
		{
			if (outLevel)
				*outLevel = 100;
			return true;
		}
		if (outLevel)
			*outLevel = static_cast<UInt32>(value);
		return true;
	}

	static bool ProviderValueToDelta(double value, SInt32* outDelta)
	{
		if (outDelta)
			*outDelta = 0;
		if (!(value == value))
			return false;
		if (value >= 2147483647.0)
		{
			if (outDelta)
				*outDelta = 2147483647;
			return true;
		}
		if (value <= -2147483648.0)
		{
			if (outDelta)
				*outDelta = static_cast<SInt32>(0x80000000);
			return true;
		}
		if (outDelta)
			*outDelta = static_cast<SInt32>(value);
		return true;
	}

	static bool ProviderIsBladeWeapon(unsigned long skillId, void* form, bool* outResult)
	{
		if (outResult)
			*outResult = false;
		if (!ProviderIsBladeSkill(skillId))
			return false;
		if (TryReadEditorFormType(form) != kFormTypeWeapon)
			return true;

		LoadWeaponTypeStoreOnce();
		BladeSeparationShared::WeaponSkillKind kind = BladeSeparationShared::kWeaponSkill_None;
		if (g_weaponTypeStore.TryGet(ReadEditorFormId(form), &kind) &&
			BladeSeparationShared::SkillIdForKind(kind) == skillId)
		{
			if (outResult)
				*outResult = true;
		}
		return true;
	}

	static bool ProviderGetNpcBladeEntry(UInt32 formId, unsigned long skillId, BladeSeparationShared::NpcSkillSidecarEntry* outEntry)
	{
		if (outEntry)
			std::memset(outEntry, 0, sizeof(*outEntry));
		if (!formId || !ProviderIsBladeSkill(skillId))
			return false;

		LoadNpcSkillStoreOnce();
		return g_npcSkillStore.TryGet(formId, static_cast<UInt32>(skillId), outEntry);
	}

	static bool ProviderSetNpcBladeEntry(UInt32 formId, unsigned long skillId, UInt32 level, float progress, UInt32 levelUps)
	{
		if (!formId || !ProviderIsBladeSkill(skillId))
			return false;

		LoadNpcSkillStoreOnce();
		BladeSeparationShared::NpcSkillSidecarEntry entry = {};
		entry.formId = formId;
		entry.skillId = static_cast<UInt32>(skillId);
		entry.level = level;
		entry.progress = progress;
		entry.levelUps = levelUps;
		return g_npcSkillStore.Set(formId, entry);
	}

	static bool ProviderGetNpcBladeAV(void* npc, unsigned long skillId, double* outValue)
	{
		if (outValue)
			*outValue = 0.0;
		if (!ProviderIsBladeSkill(skillId))
			return false;

		BladeSeparationShared::NpcSkillSidecarEntry entry = {};
		if (!ProviderGetNpcBladeEntry(ReadNpcFormId(npc), skillId, &entry))
			return false;
		if (outValue)
			*outValue = static_cast<double>(BladeSeparationShared::NpcSkillSidecarStore::ClampLevel(entry.level));
		return true;
	}

	static bool ProviderSetNpcBladeAV(void* npc, unsigned long skillId, double value)
	{
		UInt32 level = 0;
		const UInt32 formId = ReadNpcFormId(npc);
		if (!ProviderValueToLevel(value, &level) || !formId || !ProviderIsBladeSkill(skillId))
			return false;

		BladeSeparationShared::NpcSkillSidecarEntry entry = {};
		if (!ProviderGetNpcBladeEntry(formId, skillId, &entry))
		{
			entry.formId = formId;
			entry.skillId = static_cast<UInt32>(skillId);
			entry.progress = 0.0f;
			entry.levelUps = 0;
		}
		return ProviderSetNpcBladeEntry(formId, skillId, level, entry.progress, entry.levelUps);
	}

	static bool ProviderModNpcBladeAV(void* npc, unsigned long skillId, double value)
	{
		SInt32 delta = 0;
		const UInt32 formId = ReadNpcFormId(npc);
		if (!ProviderValueToDelta(value, &delta) || !formId || !ProviderIsBladeSkill(skillId))
			return false;

		BladeSeparationShared::NpcSkillSidecarEntry entry = {};
		if (!ProviderGetNpcBladeEntry(formId, skillId, &entry))
		{
			entry.formId = formId;
			entry.skillId = static_cast<UInt32>(skillId);
			entry.level = 0;
			entry.progress = 0.0f;
			entry.levelUps = 0;
		}

		const long long adjusted = static_cast<long long>(entry.level) + static_cast<long long>(delta);
		const UInt32 level = adjusted <= 0 ? 0 : adjusted >= 100 ? 100 : static_cast<UInt32>(adjusted);
		const UInt32 levelUps = delta > 0 && level > entry.level ? entry.levelUps + (level - entry.level) : entry.levelUps;
		return ProviderSetNpcBladeEntry(formId, skillId, level, entry.progress, levelUps);
	}

	static bool ProviderGetNpcBladeProgress(void* npc, unsigned long skillId, double* outValue)
	{
		if (outValue)
			*outValue = 0.0;
		if (!ProviderIsBladeSkill(skillId))
			return false;

		BladeSeparationShared::NpcSkillSidecarEntry entry = {};
		if (!ProviderGetNpcBladeEntry(ReadNpcFormId(npc), skillId, &entry))
			return false;
		if (outValue)
			*outValue = static_cast<double>(entry.progress);
		return true;
	}

	static bool ProviderSetNpcBladeProgress(void* npc, unsigned long skillId, double value)
	{
		const UInt32 formId = ReadNpcFormId(npc);
		if (!(value == value) || !formId || !ProviderIsBladeSkill(skillId))
			return false;

		BladeSeparationShared::NpcSkillSidecarEntry entry = {};
		if (!ProviderGetNpcBladeEntry(formId, skillId, &entry))
		{
			entry.formId = formId;
			entry.skillId = static_cast<UInt32>(skillId);
			entry.level = 0;
			entry.levelUps = 0;
		}
		return ProviderSetNpcBladeEntry(formId, skillId, entry.level, static_cast<float>(value), entry.levelUps);
	}

	static bool ProviderGetNpcBladeLevelUps(void* npc, unsigned long skillId, double* outValue)
	{
		if (outValue)
			*outValue = 0.0;
		if (!ProviderIsBladeSkill(skillId))
			return false;

		BladeSeparationShared::NpcSkillSidecarEntry entry = {};
		if (!ProviderGetNpcBladeEntry(ReadNpcFormId(npc), skillId, &entry))
			return false;
		if (outValue)
			*outValue = static_cast<double>(entry.levelUps);
		return true;
	}

	static bool ProviderHasNpcBladeSkill(void* npc, unsigned long skillId, bool* outResult)
	{
		if (outResult)
			*outResult = false;
		const UInt32 formId = ReadNpcFormId(npc);
		if (!formId || !ProviderIsBladeSkill(skillId))
			return false;

		LoadNpcSkillStoreOnce();
		BladeSeparationShared::NpcSkillSidecarEntry entry = {};
		if (outResult)
			*outResult = g_npcSkillStore.TryGet(formId, static_cast<UInt32>(skillId), &entry);
		return true;
	}

	static bool ProviderClearNpcBladeSkill(void* npc, unsigned long skillId)
	{
		const UInt32 formId = ReadNpcFormId(npc);
		if (!formId || !ProviderIsBladeSkill(skillId))
			return false;

		LoadNpcSkillStoreOnce();
		return g_npcSkillStore.Remove(formId, static_cast<UInt32>(skillId));
	}

	static const SidecarSkillCommandsShared::SidecarSkillProvider kSidecarSkillProviders[] =
	{
		{
			SidecarSkillCommandsShared::kSidecarSkillProviderApiVersion,
			sizeof(SidecarSkillCommandsShared::SidecarSkillProvider),
			BladeSeparationShared::kLongSkillId,
			"Long Blade",
			"LongBlade|BSLong|Long",
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
			nullptr, nullptr, nullptr, nullptr,
			&ProviderIsBladeWeapon,
			nullptr, nullptr, nullptr, nullptr,
			&ProviderGetNpcBladeAV,
			&ProviderGetNpcBladeAV,
			&ProviderSetNpcBladeAV,
			&ProviderModNpcBladeAV,
			&ProviderSetNpcBladeAV,
			&ProviderGetNpcBladeProgress,
			&ProviderSetNpcBladeProgress,
			nullptr,
			&ProviderGetNpcBladeLevelUps,
			&ProviderHasNpcBladeSkill,
			&ProviderClearNpcBladeSkill,
		},
		{
			SidecarSkillCommandsShared::kSidecarSkillProviderApiVersion,
			sizeof(SidecarSkillCommandsShared::SidecarSkillProvider),
			BladeSeparationShared::kShortSkillId,
			"Short Blade",
			"ShortBlade|BSShort|Short",
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
			nullptr, nullptr, nullptr, nullptr,
			&ProviderIsBladeWeapon,
			nullptr, nullptr, nullptr, nullptr,
			&ProviderGetNpcBladeAV,
			&ProviderGetNpcBladeAV,
			&ProviderSetNpcBladeAV,
			&ProviderModNpcBladeAV,
			&ProviderSetNpcBladeAV,
			&ProviderGetNpcBladeProgress,
			&ProviderSetNpcBladeProgress,
			nullptr,
			&ProviderGetNpcBladeLevelUps,
			&ProviderHasNpcBladeSkill,
			&ProviderClearNpcBladeSkill,
		},
		{
			SidecarSkillCommandsShared::kSidecarSkillProviderApiVersion,
			sizeof(SidecarSkillCommandsShared::SidecarSkillProvider),
			BladeSeparationShared::kAxeSkillId,
			"Axe",
			"BSAxe",
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
			nullptr, nullptr, nullptr, nullptr,
			&ProviderIsBladeWeapon,
			nullptr, nullptr, nullptr, nullptr,
			&ProviderGetNpcBladeAV,
			&ProviderGetNpcBladeAV,
			&ProviderSetNpcBladeAV,
			&ProviderModNpcBladeAV,
			&ProviderSetNpcBladeAV,
			&ProviderGetNpcBladeProgress,
			&ProviderSetNpcBladeProgress,
			nullptr,
			&ProviderGetNpcBladeLevelUps,
			&ProviderHasNpcBladeSkill,
			&ProviderClearNpcBladeSkill,
		},
	};

	static const SidecarSkillCommandsShared::SidecarSkillProviderTable kSidecarSkillProviderTable =
	{
		SidecarSkillCommandsShared::kSidecarSkillProviderApiVersion,
		sizeof(SidecarSkillCommandsShared::SidecarSkillProviderTable),
		sizeof(kSidecarSkillProviders) / sizeof(kSidecarSkillProviders[0]),
		kSidecarSkillProviders,
	};

	static const SidecarSkillCommandsShared::SidecarSkillProviderTable* GetSidecarSkillProviderTableInternal()
	{
		return &kSidecarSkillProviderTable;
	}
}

extern "C"
{
	const SidecarSkillCommandsShared::SidecarSkillProviderTable* GetSidecarSkillProviderTable()
	{
		return BladeSeparationCSE::GetSidecarSkillProviderTableInternal();
	}

	bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info)
	{
		if (!obse || !info)
			return false;

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "Blade Separation CS";
		info->version = BladeSeparationCSE::kPluginVersion;

		return obse->isEditor;
	}

	bool OBSEPlugin_Load(const OBSEInterface* obse)
	{
		if (!obse || !obse->isEditor)
			return false;

		BladeSeparationCSE::InitializeEditorWeaponTypeSurface();
		BladeSeparationCSE::InitializeEditorNpcStatsSurface();
		return true;
	}
}
