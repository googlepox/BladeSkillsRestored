#pragma once

namespace SidecarSkillCommandsShared
{
	static constexpr unsigned long kSidecarSkillProviderApiVersion = 1;

	enum SidecarItemClass
	{
		kSidecarItemClass_None = 0,
		kSidecarItemClass_Light = 1,
		kSidecarItemClass_Medium = 2,
		kSidecarItemClass_Heavy = 3,
	};

	typedef bool (*SidecarGetValueFn)(unsigned long skillId, double* outValue);
	typedef bool (*SidecarSetValueFn)(unsigned long skillId, double value);
	typedef bool (*SidecarItemTestFn)(unsigned long skillId, void* form, bool* outResult);
	typedef bool (*SidecarItemClassGetFn)(void* form, unsigned long* outClass);
	typedef bool (*SidecarItemClassSetFn)(void* form, unsigned long itemClass);
	typedef bool (*SidecarNpcGetValueFn)(void* npc, unsigned long skillId, double* outValue);
	typedef bool (*SidecarNpcSetValueFn)(void* npc, unsigned long skillId, double value);
	typedef bool (*SidecarNpcHasFn)(void* npc, unsigned long skillId, bool* outResult);
	typedef bool (*SidecarNpcClearFn)(void* npc, unsigned long skillId);

	struct SidecarSkillProvider
	{
		unsigned long apiVersion;
		unsigned long structSize;
		unsigned long skillId;
		const char* canonicalName;
		const char* aliases;
		SidecarGetValueFn getAV;
		SidecarGetValueFn getBaseAV;
		SidecarSetValueFn setAV;
		SidecarSetValueFn modAV;
		SidecarSetValueFn forceAV;
		SidecarSetValueFn advanceSkill;
		SidecarSetValueFn modPCSkill;
		SidecarGetValueFn getProgress;
		SidecarSetValueFn setProgress;
		SidecarGetValueFn getRequiredProgress;
		SidecarGetValueFn getLevelUps;
		SidecarItemTestFn isWeapon;
		SidecarItemTestFn isArmor;
		SidecarItemClassGetFn getItemClass;
		SidecarItemClassGetFn getExplicitItemClass;
		SidecarItemClassSetFn setItemClass;
		SidecarNpcGetValueFn getNPCAV;
		SidecarNpcGetValueFn getBaseNPCAV;
		SidecarNpcSetValueFn setNPCAV;
		SidecarNpcSetValueFn modNPCAV;
		SidecarNpcSetValueFn forceNPCAV;
		SidecarNpcGetValueFn getNPCProgress;
		SidecarNpcSetValueFn setNPCProgress;
		SidecarNpcGetValueFn getNPCRequiredProgress;
		SidecarNpcGetValueFn getNPCLevelUps;
		SidecarNpcHasFn hasNPCSkill;
		SidecarNpcClearFn clearNPCSkill;
	};

	struct SidecarSkillProviderTable
	{
		unsigned long apiVersion;
		unsigned long structSize;
		unsigned long providerCount;
		const SidecarSkillProvider* providers;
	};

	typedef const SidecarSkillProviderTable* (*GetSidecarSkillProviderTableFn)();
}
