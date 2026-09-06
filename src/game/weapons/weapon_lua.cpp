#include "weapon_lua.h"
#include "forge.h"

#include <base/system.h>
#include <engine/shared/sha256.h>
#include <engine/shared/mod_api.h>
#include <generated/game_data.h>

extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstddef>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace
{
enum
{
	MAX_REGISTERED_WEAPONS = 1024,
	WEAPON_SCHEMA = 4,
	FIRST_CUSTOM_WEAPON_ID = WEAPON_DEFINITION_COUNT + 1,
	LUA_WEAPON_MEMORY_LIMIT = 8 * 1024 * 1024,
	LUA_WEAPON_INSTRUCTION_LIMIT = 1000000,
	LUA_COLLECTION_INSTRUCTION_LIMIT = 10000000,
	LUA_FORGE_RESOLVE_INSTRUCTION_LIMIT = 2000000,
	MAX_WEAPON_MODULES = 256,
	MAX_WEAPON_COMPOSES = 256,
	MAX_FORGE_RECIPES = 256,
	MAX_SELECTORS = 32,
	MAX_SELECTOR_LENGTH = 384,
	MAX_PROFILE_INTEGER = 1000000,
};

constexpr float MAX_PROFILE_FLOAT = 1000000.0f;

enum EModuleSlot
{
	MODULE_FRAME,
	MODULE_PART
};

struct CWeaponModule
{
	char m_aStableId[128];
	char m_aPackageId[32];
	char m_aName[128];
	uint64_t m_Tags;
	uint16_t m_Handle;
	int m_Slot;
};

struct CComposeDeclaration
{
	char m_aPackageId[32];
	int m_Capabilities;
	char m_aaDependencies[32][32];
	int m_DependencyCount;
	char m_aId[64];
	char m_aaFrames[MAX_SELECTORS][MAX_SELECTOR_LENGTH];
	char m_aaParts[MAX_SELECTORS][MAX_SELECTOR_LENGTH];
	int m_FrameCount;
	int m_PartCount;
	lua_State *m_pState;
	int m_FunctionRef;
};

struct CForgeDeclaration
{
	char m_aPackageId[32];
	int m_Capabilities;
	char m_aaDependencies[32][32];
	int m_DependencyCount;
	char m_aId[64];
	char m_aName[128];
	char m_aaTargets[MAX_SELECTORS][MAX_SELECTOR_LENGTH];
	char m_aaMaterials[MAX_SELECTORS][MAX_SELECTOR_LENGTH];
	int m_TargetCount;
	int m_MaterialCount;
	int m_Priority;
	lua_State *m_pState;
	int m_FunctionRef;
};

struct CForgeCacheEntry
{
	bool m_Valid;
	CWeaponSpec m_Target;
	CWeaponSpec m_Material;
	int m_TargetAmmo;
	int m_MaterialAmmo;
	int m_BaseCost;
	int m_LevelCost;
	int m_Status;
	CWeaponLuaForgeResult m_Result;
};

struct CLoadContext;

struct CWeaponEntry
{
	CWeaponDefinition m_Definition;
	CWeaponCombatProfile m_aCombat[WEAPON_SPEC_LEVEL_COUNT];
	CWeaponPvpProfile m_aPvp[WEAPON_SPEC_LEVEL_COUNT];
	CWeaponVisualProfile m_aVisual[WEAPON_SPEC_LEVEL_COUNT];
};

CWeaponEntry gs_aEntries[MAX_REGISTERED_WEAPONS];
CWeaponEntry *gs_apById[1 << 16];
int gs_EntryCount;
int gs_OfficialEntryCount;
int gs_NextCustomId = FIRST_CUSTOM_WEAPON_ID;
CWeaponCombatProfile gs_aDroidCombat[WEAPON_DROID_PROFILE_COUNT];
CWeaponVisualProfile gs_aDroidVisual[WEAPON_DROID_PROFILE_COUNT];
CWeaponCombatProfile gs_aDroidDeathCombat[WEAPON_DROID_PROFILE_COUNT];
CWeaponVisualProfile gs_aDroidDeathVisual[WEAPON_DROID_PROFILE_COUNT];
CWeaponCombatProfile gs_aBuildingCombat[WEAPON_BUILDING_PROFILE_COUNT];
CWeaponVisualProfile gs_aBuildingVisual[WEAPON_BUILDING_PROFILE_COUNT];
bool gs_aDroidDefined[WEAPON_DROID_PROFILE_COUNT];
bool gs_aDroidDeathDefined[WEAPON_DROID_PROFILE_COUNT];
bool gs_aBuildingDefined[WEAPON_BUILDING_PROFILE_COUNT];
char gs_aaPart1NameKeys[NUM_PART1 + 1][64];
char gs_aaPart2NameKeys[PART2_END][64];
bool gs_aPart1NamesDefined[NUM_PART1 + 1];
bool gs_aPart2NamesDefined[PART2_END];
bool gs_Initialized;
bool gs_Initializing;
bool gs_CustomFinalized;
char gs_aOfficialContentHash[65];
CWeaponModule gs_aModules[MAX_WEAPON_MODULES];
int gs_ModuleCount;
int gs_OfficialModuleCount;
CComposeDeclaration gs_aComposes[MAX_WEAPON_COMPOSES];
int gs_ComposeCount;
CForgeDeclaration gs_aForgeRecipes[MAX_FORGE_RECIPES];
int gs_ForgeRecipeCount;
lua_State *gs_apCustomStates[MAX_WEAPON_COMPOSES + MAX_FORGE_RECIPES];
int gs_CustomStateCount;
CLoadContext *gs_apCustomContexts[MAX_WEAPON_COMPOSES + MAX_FORGE_RECIPES];
char gs_aaTags[64][64];
int gs_TagCount;
int gs_OfficialTagCount;
CForgeCacheEntry gs_aForgeCache[128];
uint32_t gs_ForgeCacheNext;

struct CReloadSnapshot
{
	CWeaponEntry m_aEntries[MAX_REGISTERED_WEAPONS];
	CWeaponEntry *m_apById[1 << 16];
	int m_EntryCount;
	int m_NextCustomId;
	CWeaponCombatProfile m_aDroidCombat[WEAPON_DROID_PROFILE_COUNT];
	CWeaponVisualProfile m_aDroidVisual[WEAPON_DROID_PROFILE_COUNT];
	CWeaponCombatProfile m_aDroidDeathCombat[WEAPON_DROID_PROFILE_COUNT];
	CWeaponVisualProfile m_aDroidDeathVisual[WEAPON_DROID_PROFILE_COUNT];
	CWeaponCombatProfile m_aBuildingCombat[WEAPON_BUILDING_PROFILE_COUNT];
	CWeaponVisualProfile m_aBuildingVisual[WEAPON_BUILDING_PROFILE_COUNT];
	bool m_aDroidDefined[WEAPON_DROID_PROFILE_COUNT];
	bool m_aDroidDeathDefined[WEAPON_DROID_PROFILE_COUNT];
	bool m_aBuildingDefined[WEAPON_BUILDING_PROFILE_COUNT];
	char m_aaPart1NameKeys[NUM_PART1 + 1][64];
	char m_aaPart2NameKeys[PART2_END][64];
	bool m_aPart1NamesDefined[NUM_PART1 + 1];
	bool m_aPart2NamesDefined[PART2_END];
	bool m_CustomFinalized;
	CWeaponModule m_aModules[MAX_WEAPON_MODULES];
	int m_ModuleCount;
	CComposeDeclaration m_aComposes[MAX_WEAPON_COMPOSES];
	int m_ComposeCount;
	CForgeDeclaration m_aForgeRecipes[MAX_FORGE_RECIPES];
	int m_ForgeRecipeCount;
	lua_State *m_apCustomStates[MAX_WEAPON_COMPOSES + MAX_FORGE_RECIPES];
	CLoadContext *m_apCustomContexts[MAX_WEAPON_COMPOSES + MAX_FORGE_RECIPES];
	int m_CustomStateCount;
	char m_aaTags[64][64];
	int m_TagCount;
	CForgeCacheEntry m_aForgeCache[128];
	uint32_t m_ForgeCacheNext;
};

CReloadSnapshot *gs_pReloadSnapshot;

#include <generated/official_weapons.inc>
#include <generated/weapon_dsl.inc>

enum EStorageType
{
	STORAGE_INT,
	STORAGE_FLOAT,
	STORAGE_BOOL,
};

struct CField
{
	const char *m_pName;
	size_t m_Offset;
	EStorageType m_Type;
};

#define COMBAT_INT(Name, Member)                                                                                       \
	{                                                                                                                  \
		Name, offsetof(CWeaponCombatProfile, Member), STORAGE_INT                                                      \
	}
#define COMBAT_FLOAT(Name, Member)                                                                                     \
	{                                                                                                                  \
		Name, offsetof(CWeaponCombatProfile, Member), STORAGE_FLOAT                                                    \
	}
#define COMBAT_BOOL(Name, Member)                                                                                      \
	{                                                                                                                  \
		Name, offsetof(CWeaponCombatProfile, Member), STORAGE_BOOL                                                     \
	}
const CField gs_aCombatFields[] = {
	COMBAT_INT("firing_type", m_FiringType),
	COMBAT_FLOAT("fire_rate", m_FireRate),
	COMBAT_BOOL("full_auto", m_FullAuto),
	COMBAT_INT("max_ammo", m_MaxAmmo),
	COMBAT_BOOL("uses_ammo", m_UsesAmmo),
	COMBAT_INT("shot_spread", m_ShotSpread),
	COMBAT_FLOAT("projectile_spread", m_ProjectileSpread),
	COMBAT_FLOAT("projectile_speed", m_ProjectileSpeed),
	COMBAT_FLOAT("projectile_curvature", m_ProjectileCurvature),
	COMBAT_FLOAT("projectile_life", m_ProjectileLife),
	COMBAT_FLOAT("projectile_damage", m_ProjectileDamage),
	COMBAT_FLOAT("projectile_knockback", m_ProjectileKnockback),
	COMBAT_FLOAT("explosion_size", m_ExplosionSize),
	COMBAT_FLOAT("explosion_damage", m_ExplosionDamage),
	COMBAT_FLOAT("melee_hit_radius", m_MeleeHitRadius),
	COMBAT_FLOAT("weapon_knockback", m_WeaponKnockback),
	COMBAT_INT("burst_count", m_BurstCount),
	COMBAT_FLOAT("burst_reload", m_BurstReload),
	COMBAT_INT("ai_attack_range", m_AiAttackRange),
	COMBAT_BOOL("valid_for_turret", m_ValidForTurret),
	COMBAT_FLOAT("throw_force", m_ThrowForce),
	COMBAT_FLOAT("flame_amount", m_FlameAmount),
	COMBAT_FLOAT("electro_amount", m_ElectroAmount),
	COMBAT_BOOL("explosive_projectile", m_ExplosiveProjectile),
	COMBAT_BOOL("laser_weapon", m_LaserWeapon),
	COMBAT_INT("cursor_weapon", m_CursorWeapon),
	COMBAT_INT("cost", m_Cost),
	COMBAT_BOOL("aimline", m_Aimline),
	COMBAT_INT("projectile_pos_type", m_ProjectilePosType),
	COMBAT_INT("laser_range", m_LaserRange),
	COMBAT_INT("laser_charge", m_LaserCharge),
	COMBAT_INT("projectile_bounces", m_ProjectileBounces),
	COMBAT_BOOL("auto_pick", m_AutoPick),
	COMBAT_FLOAT("charge_damage_min", m_ChargeDamageMin),
	COMBAT_FLOAT("charge_damage_max", m_ChargeDamageMax),
	COMBAT_FLOAT("charge_range_min", m_ChargeRangeMin),
	COMBAT_FLOAT("charge_range_max", m_ChargeRangeMax),
	COMBAT_FLOAT("charge_power_min", m_ChargePowerMin),
	COMBAT_FLOAT("charge_power_max", m_ChargePowerMax),
	COMBAT_INT("projectile_penetration", m_ProjectilePenetration),
	COMBAT_INT("charge_penetration_max", m_ChargePenetrationMax),
	COMBAT_BOOL("charge_controls_laser", m_ChargeControlsLaser),
	COMBAT_BOOL("direct_melee", m_DirectMelee),
};
#undef COMBAT_INT
#undef COMBAT_FLOAT
#undef COMBAT_BOOL

#define PVP_FLOAT(Name, Member)                                                                                       \
	{                                                                                                                  \
		Name, offsetof(CWeaponPvpProfile, Member), STORAGE_FLOAT                                                         \
	}
const CField gs_aPvpFields[] = {
	PVP_FLOAT("damage_scale", m_DamageScale),
	PVP_FLOAT("explosion_damage_scale", m_ExplosionDamageScale),
	PVP_FLOAT("fire_rate_scale", m_FireRateScale),
	PVP_FLOAT("ammo_scale", m_AmmoScale),
	PVP_FLOAT("projectile_speed_scale", m_ProjectileSpeedScale),
	PVP_FLOAT("melee_range_scale", m_MeleeRangeScale),
	PVP_FLOAT("knockback_scale", m_KnockbackScale),
};
#undef PVP_FLOAT

#define VISUAL_INT(Name, Member)                                                                                       \
	{                                                                                                                  \
		Name, offsetof(CWeaponVisualProfile, Member), STORAGE_INT                                                      \
	}
#define VISUAL_FLOAT(Name, Member)                                                                                     \
	{                                                                                                                  \
		Name, offsetof(CWeaponVisualProfile, Member), STORAGE_FLOAT                                                    \
	}
#define VISUAL_VEC_INT(Name, Member, Component)                                                                        \
	{                                                                                                                  \
		Name, offsetof(CWeaponVisualProfile, Member) + offsetof(ivec2, Component), STORAGE_INT                         \
	}
#define VISUAL_VEC_FLOAT(Name, Member, Component)                                                                      \
	{                                                                                                                  \
		Name, offsetof(CWeaponVisualProfile, Member) + offsetof(vec2, Component), STORAGE_FLOAT                        \
	}
const CField gs_aVisualFields[] = {
	VISUAL_INT("render_type", m_RenderType),
	VISUAL_VEC_INT("visual_size_x", m_VisualSize, x),
	VISUAL_VEC_INT("visual_size_y", m_VisualSize, y),
	VISUAL_VEC_INT("visual_size2_x", m_VisualSize2, x),
	VISUAL_VEC_INT("visual_size2_y", m_VisualSize2, y),
	VISUAL_VEC_FLOAT("render_offset_x", m_RenderOffset, x),
	VISUAL_VEC_FLOAT("render_offset_y", m_RenderOffset, y),
	VISUAL_VEC_FLOAT("muzzle_offset_x", m_MuzzleOffset, x),
	VISUAL_VEC_FLOAT("muzzle_offset_y", m_MuzzleOffset, y),
	VISUAL_VEC_FLOAT("projectile_offset_x", m_ProjectileOffset, x),
	VISUAL_VEC_FLOAT("projectile_offset_y", m_ProjectileOffset, y),
	VISUAL_VEC_FLOAT("hand_offset_x", m_HandOffset, x),
	VISUAL_VEC_FLOAT("hand_offset_y", m_HandOffset, y),
	VISUAL_VEC_FLOAT("color_swap_x", m_ColorSwap, x),
	VISUAL_VEC_FLOAT("color_swap_y", m_ColorSwap, y),
	VISUAL_FLOAT("render_recoil", m_RenderRecoil),
	VISUAL_FLOAT("projectile_size", m_ProjectileSize),
	VISUAL_FLOAT("projectile_sprite", m_ProjectileSprite),
	VISUAL_INT("projectile_trace_type", m_ProjectileTraceType),
	VISUAL_FLOAT("trace_threshold", m_TraceThreshold),
	VISUAL_INT("explosion_sprite", m_ExplosionSprite),
	VISUAL_INT("explosion_sound", m_ExplosionSound),
	VISUAL_INT("fire_sound", m_FireSound),
	VISUAL_INT("fire_sound2", m_FireSound2),
	VISUAL_INT("muzzle_type", m_MuzzleType),
	VISUAL_INT("muzzle_amount", m_MuzzleAmount),
	VISUAL_FLOAT("screenshake_amount", m_ScreenshakeAmount),
	VISUAL_INT("impact_effect", m_ImpactEffect),
	VISUAL_INT("static_sprite", m_StaticSprite),
};
#undef VISUAL_INT
#undef VISUAL_FLOAT
#undef VISUAL_VEC_INT
#undef VISUAL_VEC_FLOAT

struct CLoadContext
{
	char m_aPackageId[32];
	char m_aError[256];
	int m_StartEntryCount;
	int m_MemoryUsed;
	int m_MemoryCeiling;
	int m_Instructions;
	int m_CollectionInstructions;
	bool m_Official;
	bool m_CallbackActive;
	int m_Capabilities;
	char m_aaDependencies[32][32];
	int m_DependencyCount;
	bool m_BuildingCombination;
	uint16_t m_BuildFrame;
	uint16_t m_BuildPart;
	uint64_t m_BuildTags;
	char m_aBuildStableId[384];
};

bool Error(CLoadContext *pContext, const char *pText)
{
	if(!pContext->m_aError[0])
		str_copy(pContext->m_aError, pText, sizeof(pContext->m_aError));
	return false;
}

void CopyError(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText, ErrorSize);
}

void *LuaAllocate(void *pUser, void *pPointer, size_t OldSize, size_t NewSize)
{
	CLoadContext *pContext = static_cast<CLoadContext *>(pUser);
	if(!pPointer)
		OldSize = 0;
	if(NewSize == 0)
	{
		free(pPointer);
		pContext->m_MemoryUsed -= (int)OldSize;
		if(pContext->m_MemoryUsed < 0)
			pContext->m_MemoryUsed = 0;
		return 0;
	}
	const long long NewTotal = (long long)pContext->m_MemoryUsed - (long long)OldSize + (long long)NewSize;
	const int MemoryLimit = pContext->m_MemoryCeiling > 0 ? pContext->m_MemoryCeiling : LUA_WEAPON_MEMORY_LIMIT;
	if(NewTotal > MemoryLimit)
		return 0;
	void *pResult = realloc(pPointer, NewSize);
	if(pResult)
		pContext->m_MemoryUsed = (int)NewTotal;
	return pResult;
}

void InstructionHook(lua_State *pState, lua_Debug *pDebug)
{
	(void)pDebug;
	void *pUser = 0;
	lua_getallocf(pState, &pUser);
	CLoadContext *pContext = static_cast<CLoadContext *>(pUser);
	pContext->m_Instructions += 1000;
	if(pContext->m_Instructions > LUA_WEAPON_INSTRUCTION_LIMIT)
		luaL_error(pState, "weapon definition instruction budget exceeded");
}

CLoadContext *Context(lua_State *pState)
{
	return static_cast<CLoadContext *>(lua_touserdata(pState, lua_upvalueindex(1)));
}

bool IsLocalId(const char *pId)
{
	if(!pId || !pId[0] || str_length(pId) > 63)
		return false;
	for(int i = 0; pId[i]; ++i)
	{
		const char C = pId[i];
		if(!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '.' || C == '_' || C == '-'))
			return false;
	}
	return true;
}

bool ValidLocalizedName(const char *pName)
{
	if(!pName || !pName[0] || !str_utf8_check(pName))
		return false;
	bool Visible = false;
	const char *pCursor = pName;
	while(*pCursor)
	{
		const unsigned char Byte = static_cast<unsigned char>(*pCursor);
		if(Byte < 32 || Byte == 127)
			return false;
		const int Codepoint = str_utf8_decode(&pCursor);
		Visible = Visible || !str_utf8_is_whitespace(Codepoint);
	}
	return Visible;
}

bool IsPublishedFileId(const char *pId)
{
	if(!pId || !pId[0] || str_length(pId) >= 32)
		return false;
	for(int i = 0; pId[i]; ++i)
		if(pId[i] < '0' || pId[i] > '9')
			return false;
	return str_comp(pId, "0") != 0;
}

CWeaponEntry *FindStable(const char *pStableId)
{
	if(!pStableId)
		return 0;
	for(int i = 0; i < gs_EntryCount; ++i)
		if(str_comp(gs_aEntries[i].m_Definition.m_aStableId, pStableId) == 0)
			return &gs_aEntries[i];
	return 0;
}

CWeaponModule *FindModule(const char *pStableId)
{
	for(int i = 0; i < gs_ModuleCount; ++i)
		if(str_comp(gs_aModules[i].m_aStableId, pStableId) == 0)
			return &gs_aModules[i];
	return 0;
}

CWeaponModule *FindLegacyModule(int Slot, int LegacyId)
{
	if((Slot == MODULE_FRAME && (LegacyId <= 0 || LegacyId > NUM_PART1)) ||
	   (Slot == MODULE_PART && (LegacyId <= 0 || LegacyId >= PART2_END)))
		return 0;
	for(int i = 0; i < gs_ModuleCount; ++i)
	{
		const CWeaponModule &Module = gs_aModules[i];
		if(Module.m_Slot != Slot || str_comp(Module.m_aPackageId, "official") != 0)
			continue;
		static const char *s_apFrames[] = {0, "base1", "base2", "base3", "base4", "base5", "base6", "melee", "spin"};
		static const char *s_apParts[] = {0,
										  "barrel1",
										  "barrel2",
										  "barrel3",
										  "barrel4",
										  "charge",
										  "capacitor",
										  "rail",
										  "melee1",
										  "melee2",
										  "melee3",
										  "melee4",
										  "melee5",
										  "melee6"};
		const char *pLocal = Slot == MODULE_FRAME ? s_apFrames[LegacyId] : s_apParts[LegacyId];
		if(pLocal && str_comp(Module.m_aStableId + str_length(Module.m_aStableId) - str_length(pLocal), pLocal) == 0)
			return &gs_aModules[i];
	}
	return 0;
}

uint64_t TagBit(const char *pTag, bool Create)
{
	if(!IsLocalId(pTag))
		return 0;
	for(int i = 0; i < gs_TagCount; ++i)
		if(str_comp(gs_aaTags[i], pTag) == 0)
			return uint64_t(1) << i;
	if(!Create || gs_TagCount >= 64)
		return 0;
	str_copy(gs_aaTags[gs_TagCount], pTag, sizeof(gs_aaTags[0]));
	return uint64_t(1) << gs_TagCount++;
}

bool CanReferencePackage(const CLoadContext *pContext, const char *pStableId)
{
	if(str_comp_num(pStableId, "official:", 9) == 0)
		return true;
	if(str_comp_num(pStableId, "workshop:", 9) != 0)
		return true; // local IDs and tag selectors are expanded later.
	const char *pOwner = pStableId + 9;
	const char *pSeparator = str_find(pOwner, ":");
	if(!pSeparator)
		return false;
	const int Length = static_cast<int>(pSeparator - pOwner);
	if(Length == str_length(pContext->m_aPackageId) && str_comp_num(pOwner, pContext->m_aPackageId, Length) == 0)
		return true;
	for(int i = 0; i < pContext->m_DependencyCount; ++i)
		if(Length == str_length(pContext->m_aaDependencies[i]) &&
		   str_comp_num(pOwner, pContext->m_aaDependencies[i], Length) == 0)
			return true;
	return false;
}

bool ReadStringArray(lua_State *pState,
					 int Table,
					 const char *pField,
					 char (*pValues)[MAX_SELECTOR_LENGTH],
					 int *pCount,
					 CLoadContext *pContext)
{
	Table = lua_absindex(pState, Table);
	lua_getfield(pState, Table, pField);
	if(!lua_istable(pState, -1))
	{
		lua_pop(pState, 1);
		return false;
	}
	if(lua_getmetatable(pState, -1))
	{
		lua_pop(pState, 2);
		return false;
	}
	const lua_Integer Length = static_cast<lua_Integer>(lua_rawlen(pState, -1));
	if(Length < 1 || Length > MAX_SELECTORS)
	{
		lua_pop(pState, 1);
		return false;
	}
	int KeyCount = 0;
	lua_pushnil(pState);
	while(lua_next(pState, -2) != 0)
	{
		const bool ValidKey =
			lua_isinteger(pState, -2) && lua_tointeger(pState, -2) >= 1 && lua_tointeger(pState, -2) <= Length;
		lua_pop(pState, 1);
		if(!ValidKey)
		{
			lua_pop(pState, 2);
			return false;
		}
		++KeyCount;
	}
	if(KeyCount != Length)
	{
		lua_pop(pState, 1);
		return false;
	}
	for(int i = 0; i < Length; ++i)
	{
		lua_rawgeti(pState, -1, i + 1);
		size_t Size = 0;
		const char *pValue = lua_type(pState, -1) == LUA_TSTRING ? lua_tolstring(pState, -1, &Size) : 0;
		if(!pValue || !Size || Size >= MAX_SELECTOR_LENGTH || str_length(pValue) != (int)Size ||
		   !CanReferencePackage(pContext, pValue))
		{
			lua_pop(pState, 2);
			return false;
		}
		for(int Previous = 0; Previous < i; ++Previous)
		{
			if(str_comp(pValues[Previous], pValue) == 0)
			{
				lua_pop(pState, 2);
				return false;
			}
		}
		mem_copy(pValues[i], pValue, Size + 1);
		lua_pop(pState, 1);
	}
	*pCount = static_cast<int>(Length);
	lua_pop(pState, 1);
	return true;
}

bool DenseArrayLength(lua_State *pState, int Table, int Maximum, int *pLength)
{
	Table = lua_absindex(pState, Table);
	if(!lua_istable(pState, Table))
		return false;
	if(lua_getmetatable(pState, Table))
	{
		lua_pop(pState, 1);
		return false;
	}
	const size_t Length = lua_rawlen(pState, Table);
	if(Length > static_cast<size_t>(Maximum))
		return false;
	int KeyCount = 0;
	lua_pushnil(pState);
	while(lua_next(pState, Table) != 0)
	{
		const bool ValidKey = lua_isinteger(pState, -2) && lua_tointeger(pState, -2) >= 1 &&
							  static_cast<size_t>(lua_tointeger(pState, -2)) <= Length;
		lua_pop(pState, 1);
		if(!ValidKey)
		{
			lua_pop(pState, 1);
			return false;
		}
		++KeyCount;
	}
	if(KeyCount != static_cast<int>(Length))
		return false;
	*pLength = static_cast<int>(Length);
	return true;
}

bool CallbackIsStateless(lua_State *pState, int Function)
{
	Function = lua_absindex(pState, Function);
	if(!lua_isfunction(pState, Function) || lua_iscfunction(pState, Function))
		return false;
	for(int Upvalue = 1;; ++Upvalue)
	{
		const char *pName = lua_getupvalue(pState, Function, Upvalue);
		if(!pName)
			break;
		lua_pop(pState, 1);
		if(str_comp(pName, "_ENV") != 0)
			return false;
	}
	return true;
}

bool SelectorMatches(const char *pSelector, const char *pPackageId, const char *pStableId, uint64_t Tags)
{
	if(str_comp_num(pSelector, "tag:", 4) == 0)
		return (Tags & TagBit(pSelector + 4, false)) != 0;
	if(str_find(pSelector, ":"))
		return str_comp(pSelector, pStableId) == 0;
	char aStable[128];
	str_format(aStable, sizeof(aStable), "workshop:%s:module:%s", pPackageId, pSelector);
	return str_comp(aStable, pStableId) == 0;
}

bool GetIntegerField(lua_State *pState, int Table, const char *pName, int *pValue, bool Required)
{
	lua_getfield(pState, Table, pName);
	if(lua_isnil(pState, -1))
	{
		lua_pop(pState, 1);
		return !Required;
	}
	if(!lua_isinteger(pState, -1))
	{
		lua_pop(pState, 1);
		return false;
	}
	const lua_Integer Value = lua_tointeger(pState, -1);
	if(Value < INT_MIN || Value > INT_MAX)
	{
		lua_pop(pState, 1);
		return false;
	}
	*pValue = static_cast<int>(Value);
	lua_pop(pState, 1);
	return true;
}

bool GetStringField(lua_State *pState, int Table, const char *pName, char *pValue, int ValueSize, bool Required)
{
	lua_getfield(pState, Table, pName);
	if(!Required && lua_isnil(pState, -1))
	{
		pValue[0] = '\0';
		lua_pop(pState, 1);
		return true;
	}
	size_t Length = 0;
	const char *pResult = lua_type(pState, -1) == LUA_TSTRING ? lua_tolstring(pState, -1, &Length) : 0;
	const bool Valid =
		pResult && Length < static_cast<size_t>(ValueSize) && static_cast<size_t>(str_length(pResult)) == Length;
	if(Valid)
		mem_copy(pValue, pResult, Length + 1);
	lua_pop(pState, 1);
	return Valid;
}

bool LuaStringEquals(lua_State *pState, int Index, const char *pExpected)
{
	size_t Length = 0;
	const char *pValue = lua_type(pState, Index) == LUA_TSTRING ? lua_tolstring(pState, Index, &Length) : 0;
	const size_t ExpectedLength = str_length(pExpected);
	return pValue && Length == ExpectedLength && mem_comp(pValue, pExpected, Length) == 0;
}

bool FieldKnown(lua_State *pState, int Index, const CField *pFields, int FieldCount)
{
	for(int i = 0; i < FieldCount; ++i)
		if(LuaStringEquals(pState, Index, pFields[i].m_pName))
			return true;
	return false;
}

bool ValidProfile(const CWeaponCombatProfile &Combat, const CWeaponVisualProfile &Visual)
{
	auto Bounded = [](float Value, float Minimum = 0.0f)
	{
		return Value >= Minimum && Value <= MAX_PROFILE_FLOAT;
	};
	auto BoundedOffset = [](float Value)
	{
		return Value >= -MAX_PROFILE_FLOAT && Value <= MAX_PROFILE_FLOAT;
	};
	if(Combat.m_FiringType < WFT_NONE || Combat.m_FiringType > WFT_ACTIVATE || !BoundedOffset(Combat.m_FireRate) ||
	   Combat.m_MaxAmmo < 0 || Combat.m_MaxAmmo > MAX_PROFILE_INTEGER || Combat.m_ShotSpread < 0 ||
	   Combat.m_ShotSpread > 1024 || !BoundedOffset(Combat.m_ProjectileSpread) ||
	   !BoundedOffset(Combat.m_ProjectileSpeed) || !BoundedOffset(Combat.m_ProjectileCurvature) ||
	   (std::isinf(Combat.m_ProjectileLife) ? Combat.m_ProjectileLife < 0.0f
											: !BoundedOffset(Combat.m_ProjectileLife)) ||
	   !BoundedOffset(Combat.m_ProjectileDamage) || !BoundedOffset(Combat.m_ProjectileKnockback) ||
	   !BoundedOffset(Combat.m_ExplosionSize) || !BoundedOffset(Combat.m_ExplosionDamage) ||
	   !BoundedOffset(Combat.m_MeleeHitRadius) || !BoundedOffset(Combat.m_WeaponKnockback) || Combat.m_BurstCount < 0 ||
	   Combat.m_BurstCount > 1024 || !BoundedOffset(Combat.m_BurstReload) || Combat.m_AiAttackRange < 0 ||
	   Combat.m_AiAttackRange > MAX_PROFILE_INTEGER || !BoundedOffset(Combat.m_ThrowForce) ||
	   !BoundedOffset(Combat.m_FlameAmount) || !BoundedOffset(Combat.m_ElectroAmount) || Combat.m_Cost < 0 ||
	   Combat.m_Cost > MAX_PROFILE_INTEGER || Combat.m_LaserRange < 0 || Combat.m_LaserRange > MAX_PROFILE_INTEGER ||
	   Combat.m_LaserCharge < -1 || Combat.m_LaserCharge > MAX_PROFILE_INTEGER || Combat.m_ProjectileBounces < 0 ||
	   Combat.m_ProjectileBounces > 1024 || Combat.m_CursorWeapon < 0 || Combat.m_CursorWeapon >= NUM_WEAPONS ||
	   Combat.m_ProjectilePenetration < WEAPON_INFINITE_PENETRATION || Combat.m_ProjectilePenetration > 1024 ||
	   Combat.m_ChargePenetrationMax < 0 || Combat.m_ChargePenetrationMax > 1024 ||
	   !Bounded(Combat.m_ChargeDamageMin) || !Bounded(Combat.m_ChargeDamageMax, Combat.m_ChargeDamageMin) ||
	   !Bounded(Combat.m_ChargeRangeMin) || !Bounded(Combat.m_ChargeRangeMax, Combat.m_ChargeRangeMin) ||
	   !Bounded(Combat.m_ChargePowerMin) || !Bounded(Combat.m_ChargePowerMax, Combat.m_ChargePowerMin) ||
	   Combat.m_ProjectilePosType < WEAPON_PROJECTILE_PATH_STANDARD ||
	   Combat.m_ProjectilePosType > WEAPON_PROJECTILE_PATH_ROCKET ||
	   (Combat.m_FiringType == WFT_PROJECTILE && Combat.m_ProjectileLife <= 0.0f))
		return false;
	if(Visual.m_RenderType < WRT_NONE || Visual.m_RenderType > WRT_SPIN || Visual.m_VisualSize.x < 0 ||
	   Visual.m_VisualSize.x > 4096 || Visual.m_VisualSize.y < 0 || Visual.m_VisualSize.y > 4096 ||
	   Visual.m_VisualSize2.x < 0 || Visual.m_VisualSize2.x > 4096 || Visual.m_VisualSize2.y < 0 ||
	   Visual.m_VisualSize2.y > 4096 || !BoundedOffset(Visual.m_RenderOffset.x) ||
	   !BoundedOffset(Visual.m_RenderOffset.y) || !BoundedOffset(Visual.m_MuzzleOffset.x) ||
	   !BoundedOffset(Visual.m_MuzzleOffset.y) || !BoundedOffset(Visual.m_ProjectileOffset.x) ||
	   !BoundedOffset(Visual.m_ProjectileOffset.y) || !BoundedOffset(Visual.m_HandOffset.x) ||
	   !BoundedOffset(Visual.m_HandOffset.y) || !BoundedOffset(Visual.m_ColorSwap.x) ||
	   !BoundedOffset(Visual.m_ColorSwap.y) || !BoundedOffset(Visual.m_RenderRecoil) ||
	   Visual.m_ProjectileSize < -4096.0f || Visual.m_ProjectileSize > 4096.0f || Visual.m_ProjectileSprite < 0.0f ||
	   Visual.m_ProjectileSprite > static_cast<float>(NUM_SPRITES) ||
	   std::trunc(Visual.m_ProjectileSprite) != Visual.m_ProjectileSprite ||
	   SPRITE_PROJECTILE1_1 + static_cast<int>(Visual.m_ProjectileSprite) >= NUM_SPRITES ||
	   Visual.m_ProjectileTraceType < -1024 || Visual.m_ProjectileTraceType > 1024 ||
	   !BoundedOffset(Visual.m_TraceThreshold) || Visual.m_ExplosionSprite < 0 ||
	   Visual.m_ExplosionSprite >= NUM_SPRITES || Visual.m_MuzzleType < 0 || Visual.m_MuzzleType > 2 ||
	   Visual.m_MuzzleAmount < 0 || Visual.m_MuzzleAmount > 1024 || !BoundedOffset(Visual.m_ScreenshakeAmount))
		return false;
	if(Visual.m_ImpactEffect < WEAPON_IMPACT_EFFECT_NONE || Visual.m_ImpactEffect >= NUM_WEAPON_IMPACT_EFFECTS ||
	   Visual.m_StaticSprite < -1 ||
	   (Visual.m_StaticSprite >= 0 && SPRITE_WEAPON_STATIC1 + Visual.m_StaticSprite >= NUM_SPRITES))
		return false;
	return Visual.m_FireSound >= -1 && Visual.m_FireSound < NUM_SOUNDS && Visual.m_FireSound2 >= -1 &&
		   Visual.m_FireSound2 < NUM_SOUNDS && Visual.m_ExplosionSound >= -1 && Visual.m_ExplosionSound < NUM_SOUNDS;
}

void ResetPvpProfile(CWeaponPvpProfile *pProfiles)
{
	for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		pProfiles[Level] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
}

bool ValidPvpProfile(const CWeaponPvpProfile &Pvp)
{
	const float *pValues = &Pvp.m_DamageScale;
	for(int Index = 0; Index < 7; ++Index)
		if(!std::isfinite(pValues[Index]) || pValues[Index] <= 0.0f || pValues[Index] > 16.0f)
			return false;
	return true;
}

bool ReadFieldValue(lua_State *pState, int ValueIndex, int Level, double *pValue, bool *pBoolean)
{
	ValueIndex = lua_absindex(pState, ValueIndex);
	if(lua_isboolean(pState, ValueIndex))
	{
		*pValue = lua_toboolean(pState, ValueIndex) ? 1.0 : 0.0;
		*pBoolean = true;
		return true;
	}
	if(lua_type(pState, ValueIndex) == LUA_TNUMBER)
	{
		*pValue = lua_tonumber(pState, ValueIndex);
		*pBoolean = false;
		return true;
	}
	if(!lua_istable(pState, ValueIndex))
		return false;
	lua_getfield(pState, ValueIndex, "__weapon_levels");
	const bool Levels = lua_toboolean(pState, -1) != 0;
	lua_pop(pState, 1);
	if(!Levels)
		return false;
	lua_rawgeti(pState, ValueIndex, Level + 1);
	const bool Result = lua_type(pState, -1) == LUA_TNUMBER || lua_isboolean(pState, -1);
	if(Result)
	{
		*pBoolean = lua_isboolean(pState, -1);
		*pValue = lua_isboolean(pState, -1) ? (lua_toboolean(pState, -1) ? 1.0 : 0.0) : lua_tonumber(pState, -1);
	}
	lua_pop(pState, 1);
	return Result;
}

void AssignValue(void *pProfile, const CField &Field, double Value)
{
	char *pDestination = static_cast<char *>(pProfile) + Field.m_Offset;
	if(Field.m_Type == STORAGE_FLOAT)
		*reinterpret_cast<float *>(pDestination) = (float)Value;
	else if(Field.m_Type == STORAGE_BOOL)
		*reinterpret_cast<bool *>(pDestination) = Value != 0.0;
	else
		*reinterpret_cast<int *>(pDestination) = (int)Value;
}

bool ApplyProfileTable(lua_State *pState,
					   int DefinitionTable,
					   const char *pTableName,
					   const CField *pFields,
					   int FieldCount,
					   void *pProfiles,
					   size_t ProfileSize,
					   bool RequireAll,
					   CLoadContext *pContext)
{
	DefinitionTable = lua_absindex(pState, DefinitionTable);
	lua_getfield(pState, DefinitionTable, pTableName);
	if(lua_isnil(pState, -1) && !RequireAll)
	{
		lua_pop(pState, 1);
		return true;
	}
	if(!lua_istable(pState, -1))
	{
		lua_pop(pState, 1);
		return Error(pContext, "combat/visuals must be a table");
	}
	const int ProfileTable = lua_gettop(pState);
	lua_pushnil(pState);
	while(lua_next(pState, ProfileTable) != 0)
	{
		if(!FieldKnown(pState, -2, pFields, FieldCount))
		{
			lua_pop(pState, 2);
			return Error(pContext, "unknown combat/visual field");
		}
		lua_pop(pState, 1);
	}
	for(int FieldIndex = 0; FieldIndex < FieldCount; ++FieldIndex)
	{
		lua_getfield(pState, ProfileTable, pFields[FieldIndex].m_pName);
		if(lua_isnil(pState, -1))
		{
			lua_pop(pState, 1);
			// static_sprite is a player-rendering hint and is intentionally
			// omitted from legacy droid/building attack tables. Their caller
			// initializes it to -1, which preserves the normal fallback.
			if(RequireAll && str_comp(pFields[FieldIndex].m_pName, "static_sprite") != 0)
			{
				lua_pop(pState, 1);
				char aError[256];
				str_format(aError,
						   sizeof(aError),
						   "official %s profile is missing field %s",
						   pTableName,
						   pFields[FieldIndex].m_pName);
				return Error(pContext, aError);
			}
			continue;
		}
		for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		{
			double Value = 0.0;
			bool Boolean = false;
			if(!ReadFieldValue(pState, -1, Level, &Value, &Boolean))
			{
				lua_pop(pState, 2);
				return Error(pContext, "profile value must be a number, boolean, or 16-level curve");
			}
			const CField &Field = pFields[FieldIndex];
			const bool PositiveInfiniteLifetime = Field.m_Type == STORAGE_FLOAT &&
												  str_comp(Field.m_pName, "projectile_life") == 0 &&
												  std::isinf(Value) && Value > 0.0;
			const bool InvalidType = (Field.m_Type == STORAGE_BOOL) != Boolean;
			const bool InvalidInteger = Field.m_Type == STORAGE_INT && (!std::isfinite(Value) || Value < INT_MIN ||
																		Value > INT_MAX || std::trunc(Value) != Value);
			const bool InvalidFloat = Field.m_Type == STORAGE_FLOAT && !PositiveInfiniteLifetime &&
									  (!std::isfinite(Value) || Value < -FLT_MAX || Value > FLT_MAX);
			if(InvalidType || InvalidInteger || InvalidFloat)
			{
				lua_pop(pState, 2);
				return Error(pContext, "profile value is outside the field's accepted range");
			}
			AssignValue(static_cast<char *>(pProfiles) + ProfileSize * Level, pFields[FieldIndex], Value);
		}
		lua_pop(pState, 1);
	}
	lua_pop(pState, 1);
	return true;
}

int LuaLevels(lua_State *pState)
{
	luaL_checktype(pState, 1, LUA_TTABLE);
	int LevelCount = 0;
	if(!DenseArrayLength(pState, 1, WEAPON_SPEC_LEVEL_COUNT, &LevelCount) || LevelCount != WEAPON_SPEC_LEVEL_COUNT)
		return luaL_error(pState, "weapon.levels requires exactly 16 values");
	for(int Level = 1; Level <= WEAPON_SPEC_LEVEL_COUNT; ++Level)
	{
		lua_rawgeti(pState, 1, Level);
		if(lua_type(pState, -1) != LUA_TNUMBER && !lua_isboolean(pState, -1))
			return luaL_error(pState, "weapon.levels values must be numeric or boolean");
		lua_pop(pState, 1);
	}
	lua_pushboolean(pState, 1);
	lua_setfield(pState, 1, "__weapon_levels");
	lua_settop(pState, 1);
	return 1;
}

int PushCurve(lua_State *pState, int Kind)
{
	luaL_checktype(pState, 1, LUA_TNUMBER);
	luaL_checktype(pState, 2, LUA_TNUMBER);
	const double BaseValue = luaL_checknumber(pState, 1);
	const double AmountValue = luaL_checknumber(pState, 2);
	if(!std::isfinite(BaseValue) || !std::isfinite(AmountValue) || std::fabs(BaseValue) > MAX_PROFILE_FLOAT ||
	   std::fabs(AmountValue) > MAX_PROFILE_FLOAT)
		return luaL_error(pState, "curve inputs exceed engine limits");
	const float Base = static_cast<float>(BaseValue);
	const float Amount = static_cast<float>(AmountValue);
	if(!lua_isinteger(pState, 3))
		return luaL_error(pState, "curve max_level must be an integer");
	const lua_Integer MaxLevelValue = lua_tointeger(pState, 3);
	if(MaxLevelValue < 0 || MaxLevelValue > WEAPON_SPEC_MAX_LEVEL)
		return luaL_error(pState, "curve max_level is outside 0..15");
	const int MaxLevel = static_cast<int>(MaxLevelValue);
	lua_newtable(pState);
	for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
	{
		const float Charge = (float)Level / (float)(MaxLevel > 0 ? MaxLevel : 1);
		float Value = Base + Charge * Amount;
		if(Kind == 1)
			lua_pushinteger(pState, static_cast<int>(Value));
		else if(Kind == 2)
		{
			const float Cost = Amount * Charge * (Charge * 0.25f + 0.75f);
			{
				const float Result = Base + Cost;
				if(!std::isfinite(Result) || Result < INT_MIN || Result > INT_MAX)
					return luaL_error(pState, "cost curve exceeds integer limits");
				lua_pushinteger(pState, static_cast<int>(Result));
			}
		}
		else
			lua_pushnumber(pState, Value);
		lua_rawseti(pState, -2, Level + 1);
	}
	lua_pushboolean(pState, 1);
	lua_setfield(pState, -2, "__weapon_levels");
	return 1;
}

int LuaLinear(lua_State *pState)
{
	return PushCurve(pState, 0);
}

int LuaIntegerLinear(lua_State *pState)
{
	return PushCurve(pState, 1);
}

int LuaCostCurve(lua_State *pState)
{
	return PushCurve(pState, 2);
}

bool TopFieldKnown(lua_State *pState, int Index)
{
	const char *apKnown[] = {"id",
							 "schema",
							 "kind",
							 "static_type",
							 "part1",
							 "part2",
							 "max_level",
							 "inherits",
							 "behavior",
							 "combat",
							 "visuals",
							 "assets",
							 "localization"};
	for(const char *pKnown : apKnown)
		if(LuaStringEquals(pState, Index, pKnown))
			return true;
	return false;
}

bool SafeAssetPath(const char *pPath)
{
	if(!pPath || !pPath[0] || str_length(pPath) > 180 || pPath[0] == '/' || str_find(pPath, "..") ||
	   str_find(pPath, "\\") || str_find(pPath, ":"))
		return false;
	for(int i = 0; pPath[i]; ++i)
		if((unsigned char)pPath[i] < 32)
			return false;
	return true;
}

bool ApplyBehavior(
	lua_State *pState, int DefinitionTable, CWeaponDefinition *pDefinition, bool Required, CLoadContext *pContext)
{
	DefinitionTable = lua_absindex(pState, DefinitionTable);
	lua_getfield(pState, DefinitionTable, "behavior");
	if(lua_isnil(pState, -1) && !Required)
	{
		lua_pop(pState, 1);
		return true;
	}
	if(!lua_istable(pState, -1))
	{
		lua_pop(pState, 1);
		return Error(pContext, "behavior must be an array of behavior tags");
	}
	struct CBehaviorName
	{
		const char *m_pName;
		EWeaponBehaviorFlag m_Flag;
		EWeaponVisionKind m_VisionKind;
	};
	const CBehaviorName aNames[] = {
		{"tool", WEAPON_BEHAVIOR_TOOL, WEAPON_VISION_NONE},
		{"claw", WEAPON_BEHAVIOR_CLAW, WEAPON_VISION_NONE},
		{"chainsaw", WEAPON_BEHAVIOR_CHAINSAW, WEAPON_VISION_NONE},
		{"flamer", WEAPON_BEHAVIOR_FLAMER, WEAPON_VISION_NONE},
		{"grenade_timed", WEAPON_BEHAVIOR_GRENADE_TIMED, WEAPON_VISION_NONE},
		{"grenade_laser", WEAPON_BEHAVIOR_GRENADE_LASER, WEAPON_VISION_NONE},
		{"grenade_drop", WEAPON_BEHAVIOR_GRENADE_DROP, WEAPON_VISION_NONE},
		{"upgrade", WEAPON_BEHAVIOR_UPGRADE, WEAPON_VISION_NONE},
		{"controller_activate", WEAPON_BEHAVIOR_CONTROLLER_ACTIVATE, WEAPON_VISION_NONE},
		{"electrowall", WEAPON_BEHAVIOR_ELECTROWALL, WEAPON_VISION_NONE},
		{"area_shield", WEAPON_BEHAVIOR_AREA_SHIELD, WEAPON_VISION_NONE},
		{"shuriken", WEAPON_BEHAVIOR_SHURIKEN, WEAPON_VISION_NONE},
		{"bomb", WEAPON_BEHAVIOR_BOMB, WEAPON_VISION_NONE},
		{"ball", WEAPON_BEHAVIOR_BALL, WEAPON_VISION_NONE},
		{"cluster", WEAPON_BEHAVIOR_CLUSTER, WEAPON_VISION_NONE},
		{"bazooka", WEAPON_BEHAVIOR_BAZOOKA, WEAPON_VISION_NONE},
		{"electric_gun", WEAPON_BEHAVIOR_ELECTRIC_GUN, WEAPON_VISION_NONE},
		{"compact_gun_hands", WEAPON_BEHAVIOR_COMPACT_GUN_HANDS, WEAPON_VISION_NONE},
		{"charged_burst", WEAPON_BEHAVIOR_CHARGED_BURST, WEAPON_VISION_NONE},
		{"spin_reflect", WEAPON_BEHAVIOR_SPIN_REFLECT, WEAPON_VISION_NONE},
		{"impact_spark", WEAPON_BEHAVIOR_IMPACT_SPARK, WEAPON_VISION_NONE},
		{"explosion_smoke", WEAPON_BEHAVIOR_EXPLOSION_SMOKE, WEAPON_VISION_NONE},
		{"green_explosion", WEAPON_BEHAVIOR_GREEN_EXPLOSION, WEAPON_VISION_NONE},
		{"activate_invis", WEAPON_BEHAVIOR_ACTIVATE_INVIS, WEAPON_VISION_NONE},
		{"activate_shield", WEAPON_BEHAVIOR_ACTIVATE_SHIELD, WEAPON_VISION_NONE},
		{"activate_respawner", WEAPON_BEHAVIOR_ACTIVATE_RESPAWNER, WEAPON_VISION_NONE},
		{"melee", WEAPON_BEHAVIOR_MELEE, WEAPON_VISION_NONE},
		{"charged_blade", WEAPON_BEHAVIOR_CHARGED_BLADE, WEAPON_VISION_NONE},
		{"capacitor", WEAPON_BEHAVIOR_CAPACITOR, WEAPON_VISION_NONE},
		{"rail", WEAPON_BEHAVIOR_RAIL, WEAPON_VISION_NONE},
		{"hammer_impact", WEAPON_BEHAVIOR_HAMMER_IMPACT, WEAPON_VISION_NONE},
		{"vision_flash", WEAPON_BEHAVIOR_VISION_GRENADE, WEAPON_VISION_FLASH},
		{"vision_blind", WEAPON_BEHAVIOR_VISION_GRENADE, WEAPON_VISION_BLIND},
	};
	uint32_t Flags = 0;
	EWeaponVisionKind VisionKind = WEAPON_VISION_NONE;
	int Count = 0;
	if(!DenseArrayLength(pState, -1, sizeof(aNames) / sizeof(aNames[0]), &Count))
	{
		lua_pop(pState, 1);
		return Error(pContext, "behavior must be a dense array of behavior tags");
	}
	for(int Index = 1; Index <= Count; ++Index)
	{
		lua_rawgeti(pState, -1, Index);
		size_t NameLength = 0;
		const char *pName = lua_type(pState, -1) == LUA_TSTRING ? lua_tolstring(pState, -1, &NameLength) : 0;
		if(pName && static_cast<size_t>(str_length(pName)) != NameLength)
			pName = 0;
		uint32_t Flag = 0;
		EWeaponVisionKind CurrentVisionKind = WEAPON_VISION_NONE;
		for(const CBehaviorName &Name : aNames)
			if(pName && str_comp(pName, Name.m_pName) == 0)
			{
				Flag = static_cast<uint32_t>(Name.m_Flag);
				CurrentVisionKind = Name.m_VisionKind;
			}
		lua_pop(pState, 1);
		if(!Flag || (Flags & Flag))
		{
			lua_pop(pState, 1);
			return Error(pContext, "unknown or duplicate behavior tag");
		}
		Flags |= Flag;
		if(CurrentVisionKind != WEAPON_VISION_NONE)
		{
			if(VisionKind != WEAPON_VISION_NONE)
			{
				lua_pop(pState, 1);
				return Error(pContext, "behavior may contain only one vision effect tag");
			}
			VisionKind = CurrentVisionKind;
		}
	}
	pDefinition->m_BehaviorFlags = Flags;
	pDefinition->m_VisionKind = VisionKind;
	lua_pop(pState, 1);
	return true;
}

bool ApplyAssets(lua_State *pState, int DefinitionTable, CWeaponDefinition *pDefinition, CLoadContext *pContext)
{
	DefinitionTable = lua_absindex(pState, DefinitionTable);
	lua_getfield(pState, DefinitionTable, "assets");
	if(lua_isnil(pState, -1))
	{
		lua_pop(pState, 1);
		return true;
	}
	if(pContext->m_Official || !lua_istable(pState, -1))
	{
		lua_pop(pState, 1);
		return Error(pContext, "assets are only valid for Workshop weapons");
	}
	struct CAssetField
	{
		const char *m_pName;
		char *m_pDestination;
		int m_Size;
	};
	CAssetField aFields[] = {
		{"held_image", pDefinition->m_aHeldImage, sizeof(pDefinition->m_aHeldImage)},
		{"projectile_image", pDefinition->m_aProjectileImage, sizeof(pDefinition->m_aProjectileImage)},
		{"muzzle_image", pDefinition->m_aMuzzleImage, sizeof(pDefinition->m_aMuzzleImage)},
		{"fire_sound", pDefinition->m_aFireSound, sizeof(pDefinition->m_aFireSound)},
		{"fire_sound2", pDefinition->m_aFireSound2, sizeof(pDefinition->m_aFireSound2)},
		{"explosion_sound", pDefinition->m_aExplosionSound, sizeof(pDefinition->m_aExplosionSound)},
	};
	const int Assets = lua_gettop(pState);
	lua_pushnil(pState);
	while(lua_next(pState, Assets) != 0)
	{
		bool Known = false;
		for(const CAssetField &Field : aFields)
			Known = Known || LuaStringEquals(pState, -2, Field.m_pName);
		if(!Known)
		{
			lua_pop(pState, 3);
			return Error(pContext, "unknown asset slot");
		}
		lua_pop(pState, 1);
	}
	for(const CAssetField &Field : aFields)
	{
		lua_getfield(pState, Assets, Field.m_pName);
		if(!lua_isnil(pState, -1))
		{
			size_t PathLength = 0;
			const char *pPath = lua_type(pState, -1) == LUA_TSTRING ? lua_tolstring(pState, -1, &PathLength) : 0;
			if(!pPath || static_cast<size_t>(str_length(pPath)) != PathLength || !SafeAssetPath(pPath))
			{
				lua_pop(pState, 2);
				return Error(pContext, "asset path is unsafe");
			}
			str_format(Field.m_pDestination, Field.m_Size, "workshop:%s:%s", pContext->m_aPackageId, pPath);
		}
		lua_pop(pState, 1);
	}
	lua_pop(pState, 1);
	return true;
}

bool ApplyLocalization(
	lua_State *pState, int DefinitionTable, CWeaponDefinition *pDefinition, bool Required, CLoadContext *pContext)
{
	DefinitionTable = lua_absindex(pState, DefinitionTable);
	lua_getfield(pState, DefinitionTable, "localization");
	if(lua_isnil(pState, -1))
	{
		lua_pop(pState, 1);
		return Required ? Error(pContext, "localization.name is required for standalone weapons") : true;
	}
	if(pContext->m_Official || !lua_istable(pState, -1))
	{
		lua_pop(pState, 1);
		return Error(pContext, "localization is only valid for Workshop weapons");
	}
	const int Localization = lua_gettop(pState);
	lua_pushnil(pState);
	while(lua_next(pState, Localization) != 0)
	{
		const bool Known = LuaStringEquals(pState, -2, "name") || LuaStringEquals(pState, -2, "description");
		if(!Known)
		{
			lua_pop(pState, 3);
			return Error(pContext, "localization only accepts name and description keys");
		}
		lua_pop(pState, 1);
	}
	lua_getfield(pState, Localization, "name");
	size_t NameLength = 0;
	const char *pName = lua_type(pState, -1) == LUA_TSTRING ? lua_tolstring(pState, -1, &NameLength) : 0;
	if(!pName || NameLength == 0 || NameLength >= sizeof(pDefinition->m_aNameKey) ||
	   static_cast<size_t>(str_length(pName)) != NameLength || !ValidLocalizedName(pName))
	{
		lua_pop(pState, 2);
		return Error(pContext, "localization.name must be valid UTF-8 and fit the length limit");
	}
	mem_copy(pDefinition->m_aNameKey, pName, NameLength + 1);
	lua_pop(pState, 1);
	lua_getfield(pState, Localization, "description");
	if(!lua_isnil(pState, -1))
	{
		size_t DescriptionLength = 0;
		const char *pDescription =
			lua_type(pState, -1) == LUA_TSTRING ? lua_tolstring(pState, -1, &DescriptionLength) : 0;
		if(!pDescription || DescriptionLength >= sizeof(pDefinition->m_aDescriptionKey) ||
		   static_cast<size_t>(str_length(pDescription)) != DescriptionLength || !str_utf8_check(pDescription))
		{
			lua_pop(pState, 2);
			return Error(pContext, "localization.description must be valid UTF-8 and fit the length limit");
		}
		mem_copy(pDefinition->m_aDescriptionKey, pDescription, DescriptionLength + 1);
	}
	lua_pop(pState, 1);
	lua_pop(pState, 1);
	return true;
}

int LuaDefineWeapon(lua_State *pState)
{
	CLoadContext *pContext = Context(pState);
	if(pContext->m_CallbackActive)
		return luaL_error(pState, "weapon callbacks cannot register definitions");
	if(!pContext->m_Official && !pContext->m_BuildingCombination &&
	   !(pContext->m_Capabilities & MOD_CAPABILITY_WEAPONS))
		return luaL_error(pState, "weapon.define requires the weapons capability");
	luaL_checktype(pState, 1, LUA_TTABLE);
	lua_pushnil(pState);
	while(lua_next(pState, 1) != 0)
	{
		if(!TopFieldKnown(pState, -2))
			return luaL_error(pState, "unknown weapon definition field");
		lua_pop(pState, 1);
	}
	if(gs_EntryCount >= MAX_REGISTERED_WEAPONS)
		return luaL_error(pState, "weapon registry capacity exceeded");
	int Schema = 0;
	if(!GetIntegerField(pState, 1, "schema", &Schema, true) || Schema != WEAPON_SCHEMA)
		return luaL_error(pState, "weapon schema must be 4");
	char aDeclaredId[128];
	if(!GetStringField(pState, 1, "id", aDeclaredId, sizeof(aDeclaredId), true))
		return luaL_error(pState, "weapon id is required");

	CWeaponEntry Entry{};
	ResetPvpProfile(Entry.m_aPvp);
	int DefinitionId = 0;
	bool Inherited = false;
	auto ApplyDeclaredKind = [&]() -> bool
	{
		char aKind[16];
		if(!GetStringField(pState, 1, "kind", aKind, sizeof(aKind), true))
			return false;
		if(str_comp(aKind, "static") == 0)
		{
			Entry.m_Definition.m_Kind = EWeaponDefinitionKind::Static;
			int StaticType = 0;
			if(!GetIntegerField(pState, 1, "static_type", &StaticType, true) || StaticType < 0 ||
			   StaticType >= NUM_STATIC_WEAPONS)
				return false;
			Entry.m_Definition.m_StaticType = StaticType;
			return true;
		}
		if(str_comp(aKind, "modular") == 0)
		{
			Entry.m_Definition.m_Kind = EWeaponDefinitionKind::Modular;
			int Part1 = 0, Part2 = 0;
			if(!GetIntegerField(pState, 1, "part1", &Part1, true) ||
			   !GetIntegerField(pState, 1, "part2", &Part2, true) || Part1 <= 0 || Part1 > NUM_PART1 || Part2 <= 0 ||
			   Part2 >= PART2_END)
				return false;
			Entry.m_Definition.m_Part1 = Part1;
			Entry.m_Definition.m_Part2 = Part2;
			return true;
		}
		return false;
	};
	if(pContext->m_Official)
	{
		DefinitionId = gs_EntryCount + 1;
		if(str_comp_num(aDeclaredId, "official:", 9) != 0 || DefinitionId <= 0 ||
		   DefinitionId >= FIRST_CUSTOM_WEAPON_ID)
			return luaL_error(pState, "invalid official weapon identity");
		if(!ApplyDeclaredKind())
			return luaL_error(pState, "official weapon kind or identity fields are invalid");
		str_copy(Entry.m_Definition.m_aStableId, aDeclaredId, sizeof(Entry.m_Definition.m_aStableId));
		if(Entry.m_Definition.m_Kind == EWeaponDefinitionKind::Modular)
		{
			CWeaponModule *pFrame = FindLegacyModule(MODULE_FRAME, Entry.m_Definition.m_Part1);
			CWeaponModule *pPart = FindLegacyModule(MODULE_PART, Entry.m_Definition.m_Part2);
			if(!pFrame || !pPart)
				return luaL_error(pState, "official modular weapon references unknown modules");
			Entry.m_Definition.m_FrameModule = pFrame->m_Handle;
			Entry.m_Definition.m_PartModule = pPart->m_Handle;
			Entry.m_Definition.m_TagMask = pFrame->m_Tags | pPart->m_Tags;
		}
		else
			Entry.m_Definition.m_TagMask = TagBit("ranged", true);
	}
	else
	{
		lua_getfield(pState, 1, "inherits");
		Inherited = !lua_isnil(pState, -1);
		lua_pop(pState, 1);
		if(Inherited)
		{
			const char *apForbidden[] = {"kind", "static_type", "part1", "part2", "behavior"};
			for(const char *pField : apForbidden)
			{
				lua_getfield(pState, 1, pField);
				const bool Present = !lua_isnil(pState, -1);
				lua_pop(pState, 1);
				if(Present)
					return luaL_error(pState, "inherited Workshop weapons cannot redefine identity or behavior fields");
			}
		}
		if(!pContext->m_BuildingCombination && !IsLocalId(aDeclaredId))
			return luaL_error(pState, "custom weapon id must use lowercase [a-z0-9._-]");
		if(Inherited)
		{
			char aInherits[128];
			if(!GetStringField(pState, 1, "inherits", aInherits, sizeof(aInherits), true) ||
			   !CanReferencePackage(pContext, aInherits))
				return luaL_error(pState, "custom weapon inherits must be a string within the length limit");
			CWeaponEntry *pParent = FindStable(aInherits);
			if(!pParent)
				return luaL_error(pState, "custom weapon inherits an unknown or later definition");
			Entry = *pParent;
		}
		else if(!ApplyDeclaredKind())
			return luaL_error(pState, "standalone Workshop weapon kind or identity fields are invalid");
		DefinitionId = gs_NextCustomId++;
		Entry.m_Definition.m_Custom = true;
		str_copy(Entry.m_Definition.m_aPackageId, pContext->m_aPackageId, sizeof(Entry.m_Definition.m_aPackageId));
		if(pContext->m_BuildingCombination)
		{
			str_copy(
				Entry.m_Definition.m_aStableId, pContext->m_aBuildStableId, sizeof(Entry.m_Definition.m_aStableId));
			str_copy(Entry.m_Definition.m_aComposeId, aDeclaredId, sizeof(Entry.m_Definition.m_aComposeId));
			Entry.m_Definition.m_FrameModule = pContext->m_BuildFrame;
			Entry.m_Definition.m_PartModule = pContext->m_BuildPart;
			Entry.m_Definition.m_TagMask = pContext->m_BuildTags;
		}
		else
			str_format(Entry.m_Definition.m_aStableId,
					   sizeof(Entry.m_Definition.m_aStableId),
					   "workshop:%s:%s",
					   pContext->m_aPackageId,
					   aDeclaredId);
	}
	if(Entry.m_Definition.m_Kind == EWeaponDefinitionKind::Modular && !Entry.m_Definition.m_FrameModule &&
	   !Entry.m_Definition.m_PartModule)
	{
		CWeaponModule *pFrame = FindLegacyModule(MODULE_FRAME, Entry.m_Definition.m_Part1);
		CWeaponModule *pPart = FindLegacyModule(MODULE_PART, Entry.m_Definition.m_Part2);
		if(!pFrame || !pPart)
			return luaL_error(pState, "modular weapon references unknown official modules");
		Entry.m_Definition.m_FrameModule = pFrame->m_Handle;
		Entry.m_Definition.m_PartModule = pPart->m_Handle;
		Entry.m_Definition.m_TagMask |= pFrame->m_Tags | pPart->m_Tags;
	}
	if(DefinitionId < 0 || DefinitionId >= (int)(sizeof(gs_apById) / sizeof(gs_apById[0])) || gs_apById[DefinitionId] ||
	   FindStable(Entry.m_Definition.m_aStableId))
		return luaL_error(pState, "duplicate or invalid weapon id");
	int MaxLevel = Entry.m_Definition.m_MaxLevel;
	const bool RequireComplete = pContext->m_Official || !Inherited;
	if(!GetIntegerField(pState, 1, "max_level", &MaxLevel, RequireComplete) || MaxLevel < 0 ||
	   MaxLevel > WEAPON_SPEC_MAX_LEVEL)
		return luaL_error(pState, "max_level must be in 0..15");
	Entry.m_Definition.m_Id = static_cast<WeaponDefinitionId>(DefinitionId);
	Entry.m_Definition.m_MaxLevel = MaxLevel;
	if(!ApplyBehavior(pState, 1, &Entry.m_Definition, RequireComplete, pContext) ||
	   !ApplyProfileTable(pState,
						  1,
						  "combat",
						  gs_aCombatFields,
						  sizeof(gs_aCombatFields) / sizeof(gs_aCombatFields[0]),
						  Entry.m_aCombat,
						   sizeof(Entry.m_aCombat[0]),
						   RequireComplete,
						   pContext) ||
	   !ApplyProfileTable(pState,
						  1,
						  "pvp",
						  gs_aPvpFields,
						  sizeof(gs_aPvpFields) / sizeof(gs_aPvpFields[0]),
						  Entry.m_aPvp,
						  sizeof(Entry.m_aPvp[0]),
						  false,
						  pContext) ||
	   !ApplyProfileTable(pState,
						  1,
						  "visuals",
						  gs_aVisualFields,
						  sizeof(gs_aVisualFields) / sizeof(gs_aVisualFields[0]),
						  Entry.m_aVisual,
						  sizeof(Entry.m_aVisual[0]),
						  RequireComplete,
						  pContext) ||
	   !ApplyAssets(pState, 1, &Entry.m_Definition, pContext) ||
	   !ApplyLocalization(pState, 1, &Entry.m_Definition, !pContext->m_Official && !Inherited, pContext))
		return luaL_error(pState, "%s", pContext->m_aError);
	for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		if(!ValidProfile(Entry.m_aCombat[Level], Entry.m_aVisual[Level]) || !ValidPvpProfile(Entry.m_aPvp[Level]))
			return luaL_error(pState,
							  "resolved weapon profile %s level %d is outside engine limits",
							  Entry.m_Definition.m_aStableId,
							  Level);
	gs_aEntries[gs_EntryCount] = Entry;
	gs_apById[DefinitionId] = &gs_aEntries[gs_EntryCount];
	++gs_EntryCount;
	return 0;
}

int LuaDefineComponent(lua_State *pState)
{
	CLoadContext *pContext = Context(pState);
	if(!pContext->m_Official)
		return luaL_error(pState, "component metadata is reserved for embedded official content");
	luaL_checktype(pState, 1, LUA_TTABLE);
	lua_pushnil(pState);
	while(lua_next(pState, 1) != 0)
	{
		const bool Known = LuaStringEquals(pState, -2, "slot") || LuaStringEquals(pState, -2, "id") ||
						   LuaStringEquals(pState, -2, "name");
		lua_pop(pState, 1);
		if(!Known)
			return luaL_error(pState, "unknown component metadata field");
	}
	char aSlot[16];
	if(!GetStringField(pState, 1, "slot", aSlot, sizeof(aSlot), true))
		return luaL_error(pState, "component slot is required");
	int Id = 0;
	if(!GetIntegerField(pState, 1, "id", &Id, true))
		return luaL_error(pState, "component id must be an integer");
	lua_getfield(pState, 1, "name");
	size_t NameLength = 0;
	const char *pName = lua_type(pState, -1) == LUA_TSTRING ? lua_tolstring(pState, -1, &NameLength) : 0;
	if(!pName || NameLength == 0 || NameLength >= sizeof(gs_aaPart1NameKeys[0]) ||
	   static_cast<size_t>(str_length(pName)) != NameLength || !str_utf8_check(pName))
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "component name must contain 1..63 bytes");
	}
	bool HasVisibleCharacter = false;
	for(size_t i = 0; i < NameLength; ++i)
	{
		if(static_cast<unsigned char>(pName[i]) < 32 || static_cast<unsigned char>(pName[i]) == 127)
		{
			lua_pop(pState, 1);
			return luaL_error(pState, "component name contains a control character");
		}
	}
	const char *pCursor = pName;
	while(pCursor < pName + NameLength)
	{
		const int Codepoint = str_utf8_decode(&pCursor);
		HasVisibleCharacter = HasVisibleCharacter || !str_utf8_is_whitespace(Codepoint);
	}
	if(!HasVisibleCharacter)
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "component name cannot be blank");
	}
	char(*pNames)[64] = 0;
	bool *pDefined = 0;
	int Count = 0;
	if(str_comp(aSlot, "frame") == 0)
	{
		pNames = gs_aaPart1NameKeys;
		pDefined = gs_aPart1NamesDefined;
		Count = NUM_PART1 + 1;
	}
	else if(str_comp(aSlot, "part") == 0)
	{
		pNames = gs_aaPart2NameKeys;
		pDefined = gs_aPart2NamesDefined;
		Count = PART2_END;
	}
	else
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "component slot must be frame or part");
	}
	if(Id <= 0 || Id >= Count || pDefined[Id])
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "component id is invalid or duplicated");
	}
	mem_copy(pNames[Id], pName, NameLength);
	pNames[Id][NameLength] = '\0';
	pDefined[Id] = true;
	if(gs_ModuleCount >= MAX_WEAPON_MODULES)
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "weapon module registry capacity exceeded");
	}
	CWeaponModule &Module = gs_aModules[gs_ModuleCount];
	Module.m_Handle = static_cast<uint16_t>(gs_ModuleCount + 1);
	Module.m_Slot = str_comp(aSlot, "frame") == 0 ? MODULE_FRAME : MODULE_PART;
	str_copy(Module.m_aPackageId, "official", sizeof(Module.m_aPackageId));
	str_copy(Module.m_aName, pName, sizeof(Module.m_aName));
	const char *pLocal = 0;
	static const char *s_apFrames[] = {0, "base1", "base2", "base3", "base4", "base5", "base6", "melee", "spin"};
	static const char *s_apParts[] = {0,
									  "barrel1",
									  "barrel2",
									  "barrel3",
									  "barrel4",
									  "charge",
									  "capacitor",
									  "rail",
									  "melee1",
									  "melee2",
									  "melee3",
									  "melee4",
									  "melee5",
									  "melee6"};
	pLocal = Module.m_Slot == MODULE_FRAME ? s_apFrames[Id] : s_apParts[Id];
	str_format(Module.m_aStableId, sizeof(Module.m_aStableId), "official:module:%s:%s", aSlot, pLocal);
	if(Module.m_Slot == MODULE_FRAME)
		Module.m_Tags |= TagBit(Id <= PART1_BASE6 ? "ranged" : "melee", true);
	else
	{
		Module.m_Tags |= TagBit(Id <= PART2_RAIL ? "ranged-part" : "melee-part", true);
		if(Id == PART2_CHARGE || Id == PART2_CAPACITOR || Id == PART2_RAIL)
			Module.m_Tags |= TagBit("energy-part", true);
	}
	++gs_ModuleCount;
	lua_pop(pState, 1);
	return 0;
}

bool OnlyFields(lua_State *pState, int Table, const char *const *ppFields, int FieldCount)
{
	Table = lua_absindex(pState, Table);
	lua_pushnil(pState);
	while(lua_next(pState, Table) != 0)
	{
		bool Known = false;
		for(int i = 0; i < FieldCount; ++i)
			Known = Known || LuaStringEquals(pState, -2, ppFields[i]);
		lua_pop(pState, 1);
		if(!Known)
		{
			lua_pop(pState, 1);
			return false;
		}
	}
	return true;
}

int LuaDefineModule(lua_State *pState)
{
	CLoadContext *pContext = Context(pState);
	if(pContext->m_CallbackActive)
		return luaL_error(pState, "weapon callbacks cannot register modules");
	if(pContext->m_Official || !(pContext->m_Capabilities & MOD_CAPABILITY_WEAPON_MODULES))
		return luaL_error(pState, "weapon.module requires the weapon_modules capability");
	if(gs_ModuleCount >= MAX_WEAPON_MODULES)
		return luaL_error(pState, "weapon module registry capacity exceeded");
	luaL_checktype(pState, 1, LUA_TTABLE);
	const char *apFields[] = {"schema", "id", "slot", "tags", "localization"};
	if(!OnlyFields(pState, 1, apFields, sizeof(apFields) / sizeof(apFields[0])))
		return luaL_error(pState, "unknown weapon module field");
	int Schema = 0;
	char aId[64], aSlot[16];
	if(!GetIntegerField(pState, 1, "schema", &Schema, true) || Schema != 1 ||
	   !GetStringField(pState, 1, "id", aId, sizeof(aId), true) || !IsLocalId(aId) ||
	   !GetStringField(pState, 1, "slot", aSlot, sizeof(aSlot), true))
		return luaL_error(pState, "invalid weapon module schema, id, or slot");
	const int Slot = str_comp(aSlot, "frame") == 0 ? MODULE_FRAME : str_comp(aSlot, "part") == 0 ? MODULE_PART : -1;
	if(Slot < 0)
		return luaL_error(pState, "weapon module slot must be frame or part");
	CWeaponModule Module{};
	Module.m_Handle = static_cast<uint16_t>(gs_ModuleCount + 1);
	Module.m_Slot = Slot;
	str_copy(Module.m_aPackageId, pContext->m_aPackageId, sizeof(Module.m_aPackageId));
	str_format(Module.m_aStableId, sizeof(Module.m_aStableId), "workshop:%s:module:%s", pContext->m_aPackageId, aId);
	if(FindModule(Module.m_aStableId))
		return luaL_error(pState, "duplicate weapon module id");
	lua_getfield(pState, 1, "tags");
	if(!lua_isnil(pState, -1))
	{
		int TagCount = 0;
		if(!DenseArrayLength(pState, -1, 32, &TagCount))
		{
			lua_pop(pState, 1);
			return luaL_error(pState, "module tags must be a dense array");
		}
		for(int i = 1; i <= TagCount; ++i)
		{
			lua_rawgeti(pState, -1, i);
			size_t TagLength = 0;
			const char *pTag = lua_type(pState, -1) == LUA_TSTRING ? lua_tolstring(pState, -1, &TagLength) : 0;
			if(pTag && static_cast<size_t>(str_length(pTag)) != TagLength)
				pTag = 0;
			const uint64_t Bit = TagBit(pTag, true);
			lua_pop(pState, 1);
			if(!Bit || (Module.m_Tags & Bit))
			{
				lua_pop(pState, 1);
				return luaL_error(pState, "invalid or duplicate module tag");
			}
			Module.m_Tags |= Bit;
		}
	}
	lua_pop(pState, 1);
	lua_getfield(pState, 1, "localization");
	const char *apLocalizationFields[] = {"name"};
	if(!lua_istable(pState, -1) || !OnlyFields(pState, -1, apLocalizationFields, 1))
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "module localization is invalid");
	}
	if(!GetStringField(pState, -1, "name", Module.m_aName, sizeof(Module.m_aName), true) ||
	   !ValidLocalizedName(Module.m_aName))
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "module localization.name is invalid");
	}
	lua_pop(pState, 1);
	gs_aModules[gs_ModuleCount++] = Module;
	return 0;
}

int LuaDefineCompose(lua_State *pState)
{
	CLoadContext *pContext = Context(pState);
	if(pContext->m_CallbackActive)
		return luaL_error(pState, "weapon callbacks cannot register compose declarations");
	if(pContext->m_Official || !(pContext->m_Capabilities & MOD_CAPABILITY_WEAPON_MODULES))
		return luaL_error(pState, "weapon.compose requires the weapon_modules capability");
	if(gs_ComposeCount >= MAX_WEAPON_COMPOSES)
		return luaL_error(pState, "weapon compose registry capacity exceeded");
	luaL_checktype(pState, 1, LUA_TTABLE);
	const char *apFields[] = {"schema", "id", "frames", "parts", "build"};
	if(!OnlyFields(pState, 1, apFields, sizeof(apFields) / sizeof(apFields[0])))
		return luaL_error(pState, "unknown weapon compose field");
	CComposeDeclaration Compose{};
	int Schema = 0;
	if(!GetIntegerField(pState, 1, "schema", &Schema, true) || Schema != 1 ||
	   !GetStringField(pState, 1, "id", Compose.m_aId, sizeof(Compose.m_aId), true) || !IsLocalId(Compose.m_aId) ||
	   !ReadStringArray(pState, 1, "frames", Compose.m_aaFrames, &Compose.m_FrameCount, pContext) ||
	   !ReadStringArray(pState, 1, "parts", Compose.m_aaParts, &Compose.m_PartCount, pContext))
		return luaL_error(pState, "invalid weapon compose declaration");
	for(int i = 0; i < gs_ComposeCount; ++i)
		if(str_comp(gs_aComposes[i].m_aPackageId, pContext->m_aPackageId) == 0 &&
		   str_comp(gs_aComposes[i].m_aId, Compose.m_aId) == 0)
			return luaL_error(pState, "duplicate weapon compose id");
	lua_getfield(pState, 1, "build");
	if(!CallbackIsStateless(pState, -1))
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "weapon compose build callback must not capture mutable upvalues");
	}
	Compose.m_FunctionRef = luaL_ref(pState, LUA_REGISTRYINDEX);
	Compose.m_pState = pState;
	str_copy(Compose.m_aPackageId, pContext->m_aPackageId, sizeof(Compose.m_aPackageId));
	Compose.m_Capabilities = pContext->m_Capabilities;
	Compose.m_DependencyCount = pContext->m_DependencyCount;
	for(int i = 0; i < pContext->m_DependencyCount; ++i)
		str_copy(Compose.m_aaDependencies[i], pContext->m_aaDependencies[i], sizeof(Compose.m_aaDependencies[i]));
	gs_aComposes[gs_ComposeCount++] = Compose;
	return 0;
}

int LuaDefineForgeRecipe(lua_State *pState)
{
	CLoadContext *pContext = Context(pState);
	if(pContext->m_CallbackActive)
		return luaL_error(pState, "weapon callbacks cannot register forge recipes");
	if(pContext->m_Official || !(pContext->m_Capabilities & MOD_CAPABILITY_FORGE_RECIPES))
		return luaL_error(pState, "forge.recipe requires the forge_recipes capability");
	if(gs_ForgeRecipeCount >= MAX_FORGE_RECIPES)
		return luaL_error(pState, "forge recipe registry capacity exceeded");
	luaL_checktype(pState, 1, LUA_TTABLE);
	const char *apFields[] = {"schema", "id", "priority", "targets", "materials", "localization", "resolve"};
	if(!OnlyFields(pState, 1, apFields, sizeof(apFields) / sizeof(apFields[0])))
		return luaL_error(pState, "unknown forge recipe field");
	CForgeDeclaration Recipe{};
	int Schema = 0;
	if(!GetIntegerField(pState, 1, "schema", &Schema, true) || Schema != 1 ||
	   !GetStringField(pState, 1, "id", Recipe.m_aId, sizeof(Recipe.m_aId), true) || !IsLocalId(Recipe.m_aId) ||
	   !GetIntegerField(pState, 1, "priority", &Recipe.m_Priority, true) || Recipe.m_Priority < -1000000 ||
	   Recipe.m_Priority > 1000000 ||
	   !ReadStringArray(pState, 1, "targets", Recipe.m_aaTargets, &Recipe.m_TargetCount, pContext) ||
	   !ReadStringArray(pState, 1, "materials", Recipe.m_aaMaterials, &Recipe.m_MaterialCount, pContext))
		return luaL_error(pState, "invalid forge recipe declaration");
	for(int i = 0; i < gs_ForgeRecipeCount; ++i)
		if(str_comp(gs_aForgeRecipes[i].m_aPackageId, pContext->m_aPackageId) == 0 &&
		   str_comp(gs_aForgeRecipes[i].m_aId, Recipe.m_aId) == 0)
			return luaL_error(pState, "duplicate forge recipe id");
	lua_getfield(pState, 1, "localization");
	const char *apLocalizationFields[] = {"name"};
	if(!lua_istable(pState, -1) || !OnlyFields(pState, -1, apLocalizationFields, 1) ||
	   !GetStringField(pState, -1, "name", Recipe.m_aName, sizeof(Recipe.m_aName), true) ||
	   !ValidLocalizedName(Recipe.m_aName))
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "forge recipe localization.name is required");
	}
	lua_pop(pState, 1);
	lua_getfield(pState, 1, "resolve");
	if(!CallbackIsStateless(pState, -1))
	{
		lua_pop(pState, 1);
		return luaL_error(pState, "forge recipe resolve callback must not capture mutable upvalues");
	}
	Recipe.m_FunctionRef = luaL_ref(pState, LUA_REGISTRYINDEX);
	Recipe.m_pState = pState;
	str_copy(Recipe.m_aPackageId, pContext->m_aPackageId, sizeof(Recipe.m_aPackageId));
	Recipe.m_Capabilities = pContext->m_Capabilities;
	Recipe.m_DependencyCount = pContext->m_DependencyCount;
	for(int i = 0; i < pContext->m_DependencyCount; ++i)
		str_copy(Recipe.m_aaDependencies[i], pContext->m_aaDependencies[i], sizeof(Recipe.m_aaDependencies[i]));
	gs_aForgeRecipes[gs_ForgeRecipeCount++] = Recipe;
	return 0;
}

int LuaDefineAttack(lua_State *pState)
{
	CLoadContext *pContext = Context(pState);
	if(!pContext->m_Official)
		return luaL_error(pState, "player weapon packages cannot define non-player attack profiles");
	luaL_checktype(pState, 1, LUA_TTABLE);
	lua_pushnil(pState);
	while(lua_next(pState, 1) != 0)
	{
		const bool Known = LuaStringEquals(pState, -2, "schema") || LuaStringEquals(pState, -2, "kind") ||
						   LuaStringEquals(pState, -2, "type") || LuaStringEquals(pState, -2, "name") ||
						   LuaStringEquals(pState, -2, "combat") || LuaStringEquals(pState, -2, "visuals");
		lua_pop(pState, 1);
		if(!Known)
			return luaL_error(pState, "unknown attack profile field");
	}
	int Schema = 0;
	if(!GetIntegerField(pState, 1, "schema", &Schema, true) || Schema != WEAPON_SCHEMA)
		return luaL_error(pState, "attack profile schema must be 4");
	char aKind[32];
	int Type = 0;
	if(!GetStringField(pState, 1, "kind", aKind, sizeof(aKind), true) ||
	   !GetIntegerField(pState, 1, "type", &Type, true))
		return luaL_error(pState, "attack profile kind and type are required");
	CWeaponCombatProfile *pCombat = 0;
	CWeaponVisualProfile *pVisual = 0;
	bool *pDefined = 0;
	int Count = 0;
	if(str_comp(aKind, "droid") == 0)
	{
		pCombat = gs_aDroidCombat;
		pVisual = gs_aDroidVisual;
		pDefined = gs_aDroidDefined;
		Count = WEAPON_DROID_PROFILE_COUNT;
	}
	else if(str_comp(aKind, "droid_death") == 0)
	{
		pCombat = gs_aDroidDeathCombat;
		pVisual = gs_aDroidDeathVisual;
		pDefined = gs_aDroidDeathDefined;
		Count = WEAPON_DROID_PROFILE_COUNT;
	}
	else if(str_comp(aKind, "building") == 0)
	{
		pCombat = gs_aBuildingCombat;
		pVisual = gs_aBuildingVisual;
		pDefined = gs_aBuildingDefined;
		Count = WEAPON_BUILDING_PROFILE_COUNT;
	}
	else
		return luaL_error(pState, "unknown attack profile kind");
	if(Type < 0 || Type >= Count || pDefined[Type])
		return luaL_error(pState, "invalid or duplicate attack profile type");
	CWeaponCombatProfile aCombat[WEAPON_SPEC_LEVEL_COUNT]{};
	CWeaponVisualProfile aVisual[WEAPON_SPEC_LEVEL_COUNT]{};
	for(auto &Visual : aVisual)
		Visual.m_StaticSprite = -1;
	if(!ApplyProfileTable(pState,
						  1,
						  "combat",
						  gs_aCombatFields,
						  sizeof(gs_aCombatFields) / sizeof(gs_aCombatFields[0]),
						  aCombat,
						  sizeof(aCombat[0]),
						  true,
						  pContext) ||
	   !ApplyProfileTable(pState,
						  1,
						  "visuals",
						  gs_aVisualFields,
						  sizeof(gs_aVisualFields) / sizeof(gs_aVisualFields[0]),
						  aVisual,
						  sizeof(aVisual[0]),
						  true,
						  pContext))
		return luaL_error(pState, "%s", pContext->m_aError);
	pCombat[Type] = aCombat[0];
	pVisual[Type] = aVisual[0];
	pDefined[Type] = true;
	return 0;
}

int LuaOverrideVisuals(lua_State *pState)
{
	CLoadContext *pContext = Context(pState);
	if(!pContext->m_Official)
		return luaL_error(pState, "visual overrides are reserved for embedded official content");
	const char *pStableId = luaL_checkstring(pState, 1);
	luaL_checktype(pState, 2, LUA_TTABLE);
	CWeaponEntry *pEntry = FindStable(pStableId);
	if(!pEntry)
		return luaL_error(pState, "visual override references an unknown or later weapon");
	lua_newtable(pState);
	lua_pushvalue(pState, 2);
	lua_setfield(pState, -2, "visuals");
	if(!ApplyProfileTable(pState,
						  -1,
						  "visuals",
						  gs_aVisualFields,
						  sizeof(gs_aVisualFields) / sizeof(gs_aVisualFields[0]),
						  pEntry->m_aVisual,
						  sizeof(pEntry->m_aVisual[0]),
						  false,
						  pContext))
		return luaL_error(pState, "%s", pContext->m_aError);
	lua_pop(pState, 1);
	for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		if(!ValidProfile(pEntry->m_aCombat[Level], pEntry->m_aVisual[Level]))
			return luaL_error(pState, "resolved visual override is outside engine limits");
	return 0;
}

int LuaOverrideCombat(lua_State *pState)
{
	CLoadContext *pContext = Context(pState);
	if(!pContext->m_Official)
		return luaL_error(pState, "combat overrides are reserved for embedded official content");
	const char *pStableId = luaL_checkstring(pState, 1);
	luaL_checktype(pState, 2, LUA_TTABLE);
	CWeaponEntry *pEntry = FindStable(pStableId);
	if(!pEntry)
		return luaL_error(pState, "combat override references an unknown or later weapon");
	lua_newtable(pState);
	lua_pushvalue(pState, 2);
	lua_setfield(pState, -2, "combat");
	if(!ApplyProfileTable(pState,
						  -1,
						  "combat",
						  gs_aCombatFields,
						  sizeof(gs_aCombatFields) / sizeof(gs_aCombatFields[0]),
						  pEntry->m_aCombat,
						  sizeof(pEntry->m_aCombat[0]),
						  false,
						  pContext))
		return luaL_error(pState, "%s", pContext->m_aError);
	lua_pop(pState, 1);
	for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		if(!ValidProfile(pEntry->m_aCombat[Level], pEntry->m_aVisual[Level]))
			return luaL_error(pState, "resolved combat override is outside engine limits");
	return 0;
}

void RegisterFunction(lua_State *pState, const char *pName, lua_CFunction Function, CLoadContext *pContext)
{
	lua_pushlightuserdata(pState, pContext);
	lua_pushcclosure(pState, Function, 1);
	lua_setfield(pState, -2, pName);
}

void OpenDefinitionLibraries(lua_State *pState)
{
	const struct
	{
		const char *m_pName;
		lua_CFunction m_Open;
	} aLibraries[] = {
		{LUA_GNAME, luaopen_base},
		{LUA_TABLIBNAME, luaopen_table},
		{LUA_STRLIBNAME, luaopen_string},
		{LUA_MATHLIBNAME, luaopen_math},
	};
	for(const auto &Library : aLibraries)
	{
		luaL_requiref(pState, Library.m_pName, Library.m_Open, 1);
		lua_pop(pState, 1);
	}
}

void DisableGlobals(lua_State *pState, const char *const *ppNames, int Count)
{
	for(int i = 0; i < Count; ++i)
	{
		lua_pushnil(pState);
		lua_setglobal(pState, ppNames[i]);
	}
}

void OpenDefinitionSandbox(lua_State *pState)
{
	OpenDefinitionLibraries(pState);
}

void LockDefinitionSandbox(lua_State *pState)
{
	const char *apDisabled[] = {"io",	  "os",		  "debug",		  "package",		"require",
								"load",	  "loadfile", "dofile",		  "collectgarbage", "pairs",
								"next",	  "rawset",	  "setmetatable", "getmetatable",	"pcall",
								"xpcall", "print",	  "warn",		  "tostring",		"tonumber"};
	DisableGlobals(pState, apDisabled, sizeof(apDisabled) / sizeof(apDisabled[0]));

	// Definition order becomes the compact network-ID order. Remove unordered
	// iteration and nondeterministic random APIs so all supported platforms
	// register an identical sequence for the same content hash.
	lua_getglobal(pState, "math");
	if(lua_istable(pState, -1))
	{
		const char *apDisabledMath[] = {"acos",
										"asin",
										"atan",
										"cos",
										"deg",
										"exp",
										"fmod",
										"log",
										"rad",
										"random",
										"randomseed",
										"sin",
										"sqrt",
										"tan"};
		for(const char *pName : apDisabledMath)
		{
			lua_pushnil(pState);
			lua_setfield(pState, -2, pName);
		}
	}
	lua_pop(pState, 1);
	lua_getglobal(pState, "string");
	if(lua_istable(pState, -1))
	{
		const char *apDisabledString[] = {"dump", "format", "pack", "packsize", "unpack"};
		for(const char *pName : apDisabledString)
		{
			lua_pushnil(pState);
			lua_setfield(pState, -2, pName);
		}
	}
	lua_pop(pState, 1);
}

int LuaImmutableEnvironment(lua_State *pState)
{
	return luaL_error(pState, "weapon definition globals are immutable");
}

int LuaProxyLength(lua_State *pState)
{
	lua_pushinteger(pState, lua_rawlen(pState, lua_upvalueindex(1)));
	return 1;
}

void MakeReadOnlyProxy(lua_State *pState);

bool IsReadOnlyProxy(lua_State *pState, int Value)
{
	Value = lua_absindex(pState, Value);
	if(!lua_getmetatable(pState, Value))
		return false;
	lua_getfield(pState, -1, "__weapon_readonly_proxy");
	const bool Result = lua_toboolean(pState, -1) != 0;
	lua_pop(pState, 2);
	return Result;
}

int LuaProxyIndex(lua_State *pState)
{
	if(lua_type(pState, 2) == LUA_TSTRING && str_comp(lua_tostring(pState, 2), "_G") == 0)
	{
		lua_pushnil(pState);
		return 1;
	}
	lua_pushvalue(pState, 2);
	lua_rawget(pState, lua_upvalueindex(1));
	if(lua_istable(pState, -1) && !IsReadOnlyProxy(pState, -1))
		MakeReadOnlyProxy(pState);
	return 1;
}

void MakeReadOnlyProxy(lua_State *pState)
{
	const int Backing = lua_absindex(pState, -1);
	lua_newtable(pState);
	lua_newtable(pState);
	lua_pushvalue(pState, Backing);
	lua_pushcclosure(pState, LuaProxyIndex, 1);
	lua_setfield(pState, -2, "__index");
	lua_pushcfunction(pState, LuaImmutableEnvironment);
	lua_setfield(pState, -2, "__newindex");
	lua_pushvalue(pState, Backing);
	lua_pushcclosure(pState, LuaProxyLength, 1);
	lua_setfield(pState, -2, "__len");
	lua_pushboolean(pState, 0);
	lua_setfield(pState, -2, "__metatable");
	lua_pushboolean(pState, 1);
	lua_setfield(pState, -2, "__weapon_readonly_proxy");
	lua_setmetatable(pState, -2);
	lua_remove(pState, Backing);
}

void RegisterDefinitionApi(lua_State *pState, CLoadContext *pContext)
{
	lua_newtable(pState);
	RegisterFunction(pState, "_define", LuaDefineWeapon, pContext);
	RegisterFunction(pState, "_define_component", LuaDefineComponent, pContext);
	RegisterFunction(pState, "_define_module", LuaDefineModule, pContext);
	RegisterFunction(pState, "_define_compose", LuaDefineCompose, pContext);
	lua_pushcfunction(pState, LuaLevels);
	lua_setfield(pState, -2, "levels");
	lua_pushcfunction(pState, LuaLinear);
	lua_setfield(pState, -2, "linear");
	lua_pushcfunction(pState, LuaIntegerLinear);
	lua_setfield(pState, -2, "integer_linear");
	lua_pushcfunction(pState, LuaCostCurve);
	lua_setfield(pState, -2, "cost_curve");
	RegisterFunction(pState, "_override_visuals", LuaOverrideVisuals, pContext);
	RegisterFunction(pState, "_override_combat", LuaOverrideCombat, pContext);
	lua_setglobal(pState, "weapon");

	lua_newtable(pState);
	RegisterFunction(pState, "_define", LuaDefineAttack, pContext);
	lua_setglobal(pState, "attack_profile");

	lua_newtable(pState);
	RegisterFunction(pState, "_define_recipe", LuaDefineForgeRecipe, pContext);
	lua_setglobal(pState, "forge");
}

bool ExecuteContentChunk(lua_State *pState,
						 const char *pName,
						 const char *pSource,
						 int SourceSize,
						 CLoadContext *pContext,
						 char *pError,
						 int ErrorSize)
{
	bool Ok = luaL_loadbufferx(pState, pSource, SourceSize, pName, "t") == LUA_OK;
	if(Ok)
	{
		lua_pushglobaltable(pState);
		MakeReadOnlyProxy(pState);
		lua_setupvalue(pState, -2, 1);
		Ok = lua_pcall(pState, 0, 0, 0) == LUA_OK;
	}
	if(!Ok)
	{
		const char *pLuaError = lua_tostring(pState, -1);
		CopyError(pError,
				  ErrorSize,
				  pContext->m_aError[0] ? pContext->m_aError
				  : pLuaError			? pLuaError
										: "weapon Lua error");
		lua_settop(pState, 0);
	}
	return Ok;
}

bool InitializeCustomState(CLoadContext *pContext, char *pError, int ErrorSize)
{
	lua_State *pState = lua_newstate(LuaAllocate, pContext);
	if(!pState)
	{
		CopyError(pError, ErrorSize, "unable to create weapon collection Lua state");
		return false;
	}
	OpenDefinitionSandbox(pState);
	RegisterDefinitionApi(pState, pContext);
	lua_sethook(pState, InstructionHook, LUA_MASKCOUNT, 1000);
	const bool Ok =
		luaL_loadbufferx(
			pState, reinterpret_cast<const char *>(gs_aWeaponDslLua), gs_aWeaponDslLuaSize, "weapon_dsl.lua", "t") ==
			LUA_OK &&
		lua_pcall(pState, 0, 0, 0) == LUA_OK;
	if(!Ok)
	{
		const char *pLuaError = lua_tostring(pState, -1);
		CopyError(pError, ErrorSize, pLuaError ? pLuaError : "weapon DSL initialization failed");
		lua_close(pState);
		return false;
	}
	LockDefinitionSandbox(pState);
	gs_apCustomStates[0] = pState;
	gs_apCustomContexts[0] = pContext;
	gs_CustomStateCount = 1;
	return true;
}

bool Execute(const char *pName,
			 const char *pSource,
			 int SourceSize,
			 CLoadContext *pContext,
			 bool RetainCallbacks,
			 bool *pRetained,
			 char *pError,
			 int ErrorSize)
{
	if(pRetained)
		*pRetained = false;
	const int StartComposeCount = gs_ComposeCount;
	const int StartRecipeCount = gs_ForgeRecipeCount;
	lua_State *pState = lua_newstate(LuaAllocate, pContext);
	if(!pState)
	{
		CopyError(pError, ErrorSize, "unable to create weapon Lua state");
		return false;
	}
	OpenDefinitionSandbox(pState);
	RegisterDefinitionApi(pState, pContext);
	lua_sethook(pState, InstructionHook, LUA_MASKCOUNT, 1000);
	bool Ok =
		luaL_loadbufferx(
			pState, reinterpret_cast<const char *>(gs_aWeaponDslLua), gs_aWeaponDslLuaSize, "weapon_dsl.lua", "t") ==
			LUA_OK &&
		lua_pcall(pState, 0, 0, 0) == LUA_OK;
	if(Ok)
	{
		LockDefinitionSandbox(pState);
		Ok = ExecuteContentChunk(pState, pName, pSource, SourceSize, pContext, pError, ErrorSize);
	}
	if(!Ok)
	{
		const char *pLuaError = lua_tostring(pState, -1);
		if(!pError || ErrorSize <= 0 || !pError[0])
			CopyError(pError,
					  ErrorSize,
					  pContext->m_aError[0] ? pContext->m_aError
					  : pLuaError			? pLuaError
											: "weapon Lua error");
	}
	const bool HasCallbacks = gs_ComposeCount != StartComposeCount || gs_ForgeRecipeCount != StartRecipeCount;
	if(Ok && RetainCallbacks && HasCallbacks)
	{
		if(gs_CustomStateCount >= (int)(sizeof(gs_apCustomStates) / sizeof(gs_apCustomStates[0])))
		{
			CopyError(pError, ErrorSize, "weapon callback state capacity exceeded");
			Ok = false;
		}
		else
		{
			gs_apCustomStates[gs_CustomStateCount] = pState;
			gs_apCustomContexts[gs_CustomStateCount] = pContext;
			++gs_CustomStateCount;
			if(pRetained)
				*pRetained = true;
		}
	}
	if(!Ok || !RetainCallbacks || !HasCallbacks)
		lua_close(pState);
	return Ok;
}

void RollbackEntries(int EntryCount)
{
	while(gs_EntryCount > EntryCount)
	{
		--gs_EntryCount;
		const int Id = static_cast<int>(gs_aEntries[gs_EntryCount].m_Definition.m_Id);
		if(Id >= 0 && Id < (int)(sizeof(gs_apById) / sizeof(gs_apById[0])))
			gs_apById[Id] = 0;
		mem_zero(&gs_aEntries[gs_EntryCount], sizeof(gs_aEntries[gs_EntryCount]));
	}
	gs_NextCustomId = FIRST_CUSTOM_WEAPON_ID;
	for(int i = gs_OfficialEntryCount; i < gs_EntryCount; ++i)
	{
		const int Next = static_cast<int>(gs_aEntries[i].m_Definition.m_Id) + 1;
		if(Next > gs_NextCustomId)
			gs_NextCustomId = Next;
	}
}

void ResetOfficialRegistry()
{
	mem_zero(gs_aEntries, sizeof(gs_aEntries));
	mem_zero(gs_apById, sizeof(gs_apById));
	mem_zero(gs_aPart1NamesDefined, sizeof(gs_aPart1NamesDefined));
	mem_zero(gs_aPart2NamesDefined, sizeof(gs_aPart2NamesDefined));
	mem_zero(gs_aaPart1NameKeys, sizeof(gs_aaPart1NameKeys));
	mem_zero(gs_aaPart2NameKeys, sizeof(gs_aaPart2NameKeys));
	mem_zero(gs_aDroidDefined, sizeof(gs_aDroidDefined));
	mem_zero(gs_aDroidDeathDefined, sizeof(gs_aDroidDeathDefined));
	mem_zero(gs_aBuildingDefined, sizeof(gs_aBuildingDefined));
	mem_zero(gs_aDroidCombat, sizeof(gs_aDroidCombat));
	mem_zero(gs_aDroidVisual, sizeof(gs_aDroidVisual));
	mem_zero(gs_aDroidDeathCombat, sizeof(gs_aDroidDeathCombat));
	mem_zero(gs_aDroidDeathVisual, sizeof(gs_aDroidDeathVisual));
	mem_zero(gs_aBuildingCombat, sizeof(gs_aBuildingCombat));
	mem_zero(gs_aBuildingVisual, sizeof(gs_aBuildingVisual));
	mem_zero(gs_aModules, sizeof(gs_aModules));
	gs_ModuleCount = 0;
	gs_OfficialModuleCount = 0;
	mem_zero(gs_aaTags, sizeof(gs_aaTags));
	gs_TagCount = 0;
	gs_OfficialTagCount = 0;
	gs_EntryCount = 0;
	gs_OfficialEntryCount = 0;
	gs_NextCustomId = FIRST_CUSTOM_WEAPON_ID;
	gs_Initialized = false;
	gs_CustomFinalized = false;
}

bool OfficialRegistryComplete()
{
	if(gs_EntryCount != WEAPON_DEFINITION_COUNT)
		return false;
	bool aStaticSeen[NUM_STATIC_WEAPONS] = {};
	bool aaModularSeen[NUM_PART1 + 1][PART2_END] = {};
	for(int i = 0; i < gs_EntryCount; ++i)
	{
		const CWeaponDefinition &Definition = gs_aEntries[i].m_Definition;
		if(Definition.m_Kind == EWeaponDefinitionKind::Static)
		{
			if(Definition.m_StaticType >= NUM_STATIC_WEAPONS || aStaticSeen[Definition.m_StaticType])
				return false;
			aStaticSeen[Definition.m_StaticType] = true;
		}
		else
		{
			const int Part1 = Definition.m_Part1;
			const int Part2 = Definition.m_Part2;
			const bool Ranged =
				Part1 >= PART1_BASE1 && Part1 <= PART1_BASE6 && Part2 >= PART2_BARREL1 && Part2 <= PART2_RAIL;
			const bool Melee =
				Part1 >= PART1_MELEE && Part1 <= PART1_SPIN && Part2 >= PART2_MELEE1 && Part2 <= PART2_MELEE6;
			if((!Ranged && !Melee) || aaModularSeen[Part1][Part2])
				return false;
			aaModularSeen[Part1][Part2] = true;
		}
	}
	for(bool Seen : aStaticSeen)
		if(!Seen)
			return false;
	for(int Part1 = PART1_BASE1; Part1 <= PART1_BASE6; ++Part1)
		for(int Part2 = PART2_BARREL1; Part2 <= PART2_RAIL; ++Part2)
			if(!aaModularSeen[Part1][Part2])
				return false;
	for(int Part1 = PART1_MELEE; Part1 <= PART1_SPIN; ++Part1)
		for(int Part2 = PART2_MELEE1; Part2 <= PART2_MELEE6; ++Part2)
			if(!aaModularSeen[Part1][Part2])
				return false;
	for(int i = 0; i < WEAPON_DROID_PROFILE_COUNT; ++i)
		if(!gs_aDroidDefined[i] || !gs_aDroidDeathDefined[i])
			return false;
	for(int i = 0; i < WEAPON_BUILDING_PROFILE_COUNT; ++i)
		if(!gs_aBuildingDefined[i])
			return false;
	for(int i = 1; i <= NUM_PART1; ++i)
		if(!gs_aPart1NamesDefined[i])
			return false;
	for(int i = 1; i < PART2_END; ++i)
		if(!gs_aPart2NamesDefined[i])
			return false;
	return true;
}

bool InitializeOfficialSource(const char *pSource, int SourceSize, char *pError, int ErrorSize)
{
	CLoadContext Context{};
	Context.m_Official = true;
	const bool Loaded = Execute("official_weapons.lua", pSource, SourceSize, &Context, false, 0, pError, ErrorSize);
	const bool Complete = Loaded && OfficialRegistryComplete();
	if(!Complete)
	{
		if(Loaded)
			CopyError(pError, ErrorSize, "official Lua profile count mismatch");
		ResetOfficialRegistry();
		return false;
	}
	gs_OfficialEntryCount = gs_EntryCount;
	gs_OfficialModuleCount = gs_ModuleCount;
	gs_OfficialTagCount = gs_TagCount;
	gs_Initialized = true;
	return true;
}

CLoadContext *ContextForState(lua_State *pState)
{
	for(int i = 0; i < gs_CustomStateCount; ++i)
		if(gs_apCustomStates[i] == pState)
			return gs_apCustomContexts[i];
	return 0;
}

void SetContextAccess(CLoadContext *pContext,
					  const char *pPackageId,
					  int Capabilities,
					  const char (*pDependencies)[32],
					  int DependencyCount)
{
	str_copy(pContext->m_aPackageId, pPackageId, sizeof(pContext->m_aPackageId));
	pContext->m_Capabilities = Capabilities;
	pContext->m_DependencyCount = DependencyCount;
	for(int i = 0; i < DependencyCount; ++i)
		str_copy(pContext->m_aaDependencies[i], pDependencies[i], sizeof(pContext->m_aaDependencies[i]));
}

bool ModuleMatches(const CComposeDeclaration &Compose, const CWeaponModule &Module, bool Frame)
{
	CLoadContext *pContext = ContextForState(Compose.m_pState);
	if(!pContext)
		return false;
	SetContextAccess(
		pContext, Compose.m_aPackageId, Compose.m_Capabilities, Compose.m_aaDependencies, Compose.m_DependencyCount);
	if(!CanReferencePackage(pContext, Module.m_aStableId))
		return false;
	const int Count = Frame ? Compose.m_FrameCount : Compose.m_PartCount;
	const char(*pSelectors)[MAX_SELECTOR_LENGTH] = Frame ? Compose.m_aaFrames : Compose.m_aaParts;
	for(int i = 0; i < Count; ++i)
		if(SelectorMatches(pSelectors[i], Compose.m_aPackageId, Module.m_aStableId, Module.m_Tags))
			return true;
	return false;
}

bool SelectorHasModuleMatch(const CComposeDeclaration &Compose, const char *pSelector, int Slot)
{
	CLoadContext *pContext = ContextForState(Compose.m_pState);
	if(!pContext)
		return false;
	SetContextAccess(
		pContext, Compose.m_aPackageId, Compose.m_Capabilities, Compose.m_aaDependencies, Compose.m_DependencyCount);
	for(int i = 0; i < gs_ModuleCount; ++i)
		if(gs_aModules[i].m_Slot == Slot && CanReferencePackage(pContext, gs_aModules[i].m_aStableId) &&
		   SelectorMatches(pSelector, Compose.m_aPackageId, gs_aModules[i].m_aStableId, gs_aModules[i].m_Tags))
			return true;
	return false;
}

int LegacyModuleId(const CWeaponModule &Module)
{
	if(str_comp(Module.m_aPackageId, "official") != 0)
		return 1;
	static const char *s_apFrames[] = {0, "base1", "base2", "base3", "base4", "base5", "base6", "melee", "spin"};
	static const char *s_apParts[] = {0,
									  "barrel1",
									  "barrel2",
									  "barrel3",
									  "barrel4",
									  "charge",
									  "capacitor",
									  "rail",
									  "melee1",
									  "melee2",
									  "melee3",
									  "melee4",
									  "melee5",
									  "melee6"};
	const char *const *ppNames = Module.m_Slot == MODULE_FRAME ? s_apFrames : s_apParts;
	const int Count = Module.m_Slot == MODULE_FRAME ? (int)(sizeof(s_apFrames) / sizeof(s_apFrames[0]))
													: (int)(sizeof(s_apParts) / sizeof(s_apParts[0]));
	for(int i = 1; i < Count; ++i)
		if(str_comp(Module.m_aStableId + str_length(Module.m_aStableId) - str_length(ppNames[i]), ppNames[i]) == 0)
			return i;
	return 1;
}

void PushModuleContext(lua_State *pState, const CWeaponModule &Module)
{
	lua_newtable(pState);
	lua_pushstring(pState, Module.m_aStableId);
	lua_setfield(pState, -2, "stable_id");
	lua_pushstring(pState, Module.m_aName);
	lua_setfield(pState, -2, "name");
	lua_newtable(pState);
	int TagIndex = 1;
	for(int i = 0; i < gs_TagCount; ++i)
		if(Module.m_Tags & (uint64_t(1) << i))
		{
			lua_pushstring(pState, gs_aaTags[i]);
			lua_rawseti(pState, -2, TagIndex++);
		}
	MakeReadOnlyProxy(pState);
	lua_setfield(pState, -2, "tags");
	MakeReadOnlyProxy(pState);
}

bool BuildCombination(const CComposeDeclaration &Compose,
					  const CWeaponModule &Frame,
					  const CWeaponModule &Part,
					  char *pError,
					  int ErrorSize)
{
	lua_State *pState = Compose.m_pState;
	CLoadContext *pContext = ContextForState(pState);
	if(!pContext)
	{
		CopyError(pError, ErrorSize, "weapon compose callback state is unavailable");
		return false;
	}
	SetContextAccess(
		pContext, Compose.m_aPackageId, Compose.m_Capabilities, Compose.m_aaDependencies, Compose.m_DependencyCount);
	pContext->m_Instructions = 0;
	pContext->m_MemoryCeiling = std::min<int>(LUA_WEAPON_MEMORY_LIMIT, pContext->m_MemoryUsed + 1024 * 1024);
	lua_settop(pState, 0);
	lua_rawgeti(pState, LUA_REGISTRYINDEX, Compose.m_FunctionRef);
	lua_newtable(pState);
	PushModuleContext(pState, Frame);
	lua_setfield(pState, -2, "frame");
	PushModuleContext(pState, Part);
	lua_setfield(pState, -2, "part");
	MakeReadOnlyProxy(pState);
	pContext->m_CallbackActive = true;
	const bool CallbackOk = lua_pcall(pState, 1, 1, 0) == LUA_OK;
	pContext->m_CallbackActive = false;
	if(!CallbackOk || !lua_istable(pState, -1))
	{
		const char *pLuaError = lua_tostring(pState, -1);
		CopyError(pError, ErrorSize, pLuaError ? pLuaError : "weapon compose build must return a definition table");
		lua_settop(pState, 0);
		pContext->m_MemoryCeiling = LUA_WEAPON_MEMORY_LIMIT;
		lua_gc(pState, LUA_GCCOLLECT, 0);
		return false;
	}
	const char *apReservedFields[] = {"id", "schema", "inherits", "kind", "static_type", "part1", "part2"};
	for(const char *pField : apReservedFields)
	{
		lua_getfield(pState, -1, pField);
		const bool Present = !lua_isnil(pState, -1);
		lua_pop(pState, 1);
		if(Present)
		{
			CopyError(pError, ErrorSize, "weapon compose build returned a reserved identity field");
			lua_settop(pState, 0);
			pContext->m_MemoryCeiling = LUA_WEAPON_MEMORY_LIMIT;
			lua_gc(pState, LUA_GCCOLLECT, 0);
			return false;
		}
	}
	const int Definition = lua_gettop(pState);
	lua_pushinteger(pState, WEAPON_SCHEMA);
	lua_setfield(pState, Definition, "schema");
	lua_pushstring(pState, Compose.m_aId);
	lua_setfield(pState, Definition, "id");
	lua_pushstring(pState, "modular");
	lua_setfield(pState, Definition, "kind");
	lua_pushinteger(pState, LegacyModuleId(Frame));
	lua_setfield(pState, Definition, "part1");
	lua_pushinteger(pState, LegacyModuleId(Part));
	lua_setfield(pState, Definition, "part2");

	pContext->m_BuildingCombination = true;
	pContext->m_BuildFrame = Frame.m_Handle;
	pContext->m_BuildPart = Part.m_Handle;
	pContext->m_BuildTags = Frame.m_Tags | Part.m_Tags;
	str_format(pContext->m_aBuildStableId,
			   sizeof(pContext->m_aBuildStableId),
			   "workshop:%s:compose:%s:%s:%s",
			   Compose.m_aPackageId,
			   Compose.m_aId,
			   Frame.m_aStableId,
			   Part.m_aStableId);
	lua_getglobal(pState, "weapon");
	lua_getfield(pState, -1, "define");
	lua_pushvalue(pState, Definition);
	const bool Ok = lua_pcall(pState, 1, 0, 0) == LUA_OK;
	pContext->m_BuildingCombination = false;
	if(!Ok)
	{
		const char *pLuaError = lua_tostring(pState, -1);
		CopyError(pError,
				  ErrorSize,
				  pContext->m_aError[0] ? pContext->m_aError
				  : pLuaError			? pLuaError
										: "weapon compose build failed");
	}
	lua_settop(pState, 0);
	pContext->m_MemoryCeiling = LUA_WEAPON_MEMORY_LIMIT;
	lua_gc(pState, LUA_GCCOLLECT, 0);
	return Ok;
}

const CWeaponModule *ModuleByHandle(uint16_t Handle)
{
	return Handle > 0 && Handle <= gs_ModuleCount ? &gs_aModules[Handle - 1] : 0;
}

bool WeaponSelectorMatches(const char *pSelector, const CForgeDeclaration &Recipe, const CWeaponDefinition &Definition)
{
	if(str_comp_num(pSelector, "tag:", 4) == 0)
		return (Definition.m_TagMask & TagBit(pSelector + 4, false)) != 0;
	if(str_find(pSelector, ":"))
		return str_comp(pSelector, Definition.m_aStableId) == 0;
	if(Definition.m_aComposeId[0] && str_comp(pSelector, Definition.m_aComposeId) == 0 &&
	   str_comp(Recipe.m_aPackageId, Definition.m_aPackageId) == 0)
		return true;
	char aStable[384];
	str_format(aStable, sizeof(aStable), "workshop:%s:%s", Recipe.m_aPackageId, pSelector);
	return str_comp(aStable, Definition.m_aStableId) == 0;
}

bool AnyWeaponSelectorMatches(const char (*pSelectors)[MAX_SELECTOR_LENGTH],
							  int Count,
							  const CForgeDeclaration &Recipe,
							  const CWeaponDefinition &Definition)
{
	for(int i = 0; i < Count; ++i)
		if(WeaponSelectorMatches(pSelectors[i], Recipe, Definition))
			return true;
	return false;
}

bool ForgeSelectorHasWeaponMatch(const CForgeDeclaration &Recipe, const char *pSelector)
{
	for(int i = 0; i < gs_EntryCount; ++i)
		if(WeaponSelectorMatches(pSelector, Recipe, gs_aEntries[i].m_Definition))
			return true;
	return false;
}

void PushWeaponForgeContext(lua_State *pState, const CResolvedWeaponProfile &Profile, int Ammo)
{
	lua_newtable(pState);
	lua_pushstring(pState, Profile.m_Definition.m_aStableId);
	lua_setfield(pState, -2, "stable_id");
	const CWeaponModule *pFrame = ModuleByHandle(Profile.m_Definition.m_FrameModule);
	const CWeaponModule *pPart = ModuleByHandle(Profile.m_Definition.m_PartModule);
	if(pFrame)
		lua_pushstring(pState, pFrame->m_aStableId);
	else
		lua_pushnil(pState);
	lua_setfield(pState, -2, "frame_module");
	if(pPart)
		lua_pushstring(pState, pPart->m_aStableId);
	else
		lua_pushnil(pState);
	lua_setfield(pState, -2, "part_module");
	lua_pushinteger(pState, Profile.m_Spec.m_Level);
	lua_setfield(pState, -2, "level");
	lua_pushinteger(pState, Ammo);
	lua_setfield(pState, -2, "ammo");
	lua_pushinteger(pState, Profile.m_Combat.m_MaxAmmo);
	lua_setfield(pState, -2, "max_ammo");
	lua_newtable(pState);
	int TagIndex = 1;
	for(int i = 0; i < gs_TagCount; ++i)
		if(Profile.m_Definition.m_TagMask & (uint64_t(1) << i))
		{
			lua_pushstring(pState, gs_aaTags[i]);
			lua_rawseti(pState, -2, TagIndex++);
		}
	MakeReadOnlyProxy(pState);
	lua_setfield(pState, -2, "tags");
	MakeReadOnlyProxy(pState);
}

int ResolveForgeDeclaration(const CForgeDeclaration &Recipe,
							const CResolvedWeaponProfile &Target,
							const CResolvedWeaponProfile &Material,
							int TargetAmmo,
							int MaterialAmmo,
							int BaseCost,
							int LevelCost,
							CWeaponLuaForgeResult *pResult)
{
	lua_State *pState = Recipe.m_pState;
	CLoadContext *pContext = ContextForState(pState);
	if(!pContext)
		return -1;
	pContext->m_Instructions = 0;
	if(!AnyWeaponSelectorMatches(Recipe.m_aaTargets, Recipe.m_TargetCount, Recipe, Target.m_Definition) ||
	   !AnyWeaponSelectorMatches(Recipe.m_aaMaterials, Recipe.m_MaterialCount, Recipe, Material.m_Definition))
		return 0;
	SetContextAccess(
		pContext, Recipe.m_aPackageId, Recipe.m_Capabilities, Recipe.m_aaDependencies, Recipe.m_DependencyCount);
	pContext->m_MemoryCeiling = std::min<int>(LUA_WEAPON_MEMORY_LIMIT, pContext->m_MemoryUsed + 1024 * 1024);
	pContext->m_aError[0] = '\0';
	auto Finish = [&](int Status)
	{
		lua_settop(pState, 0);
		pContext->m_MemoryCeiling = LUA_WEAPON_MEMORY_LIMIT;
		lua_gc(pState, LUA_GCCOLLECT, 0);
		return Status;
	};
	lua_settop(pState, 0);
	lua_rawgeti(pState, LUA_REGISTRYINDEX, Recipe.m_FunctionRef);
	lua_newtable(pState);
	PushWeaponForgeContext(pState, Target, TargetAmmo);
	lua_setfield(pState, -2, "target");
	PushWeaponForgeContext(pState, Material, MaterialAmmo);
	lua_setfield(pState, -2, "material");
	lua_pushinteger(pState, BaseCost);
	lua_setfield(pState, -2, "base_cost");
	lua_pushinteger(pState, LevelCost);
	lua_setfield(pState, -2, "level_cost");
	MakeReadOnlyProxy(pState);
	pContext->m_CallbackActive = true;
	const bool CallbackOk = lua_pcall(pState, 1, 1, 0) == LUA_OK;
	pContext->m_CallbackActive = false;
	if(!CallbackOk)
	{
		const char *pLuaError = lua_tostring(pState, -1);
		dbg_msg("forge",
				"Mod recipe %s:%s failed: %s",
				Recipe.m_aPackageId,
				Recipe.m_aId,
				pLuaError ? pLuaError : "unknown Lua error");
		return Finish(-1);
	}
	if(lua_isnil(pState, -1))
	{
		return Finish(0);
	}
	if(!lua_istable(pState, -1))
	{
		return Finish(-1);
	}
	const int ResultTable = lua_gettop(pState);
	lua_pushnil(pState);
	while(lua_next(pState, ResultTable) != 0)
	{
		const bool Known = LuaStringEquals(pState, -2, "product") || LuaStringEquals(pState, -2, "level") ||
						   LuaStringEquals(pState, -2, "cost");
		lua_pop(pState, 1);
		if(!Known)
		{
			return Finish(-1);
		}
	}
	char aProduct[384];
	int Level = 0, Cost = 0;
	if(!GetStringField(pState, ResultTable, "product", aProduct, sizeof(aProduct), true) ||
	   !GetIntegerField(pState, ResultTable, "level", &Level, true) || Level < 0 || Level > WEAPON_SPEC_MAX_LEVEL ||
	   !GetIntegerField(pState, ResultTable, "cost", &Cost, true) || Cost < 0 || Cost > FORGE_MAX_COST)
	{
		return Finish(-1);
	}
	char aStable[384];
	const char *pStable = aProduct;
	if(!str_find(aProduct, ":"))
	{
		str_format(aStable, sizeof(aStable), "workshop:%s:%s", Recipe.m_aPackageId, aProduct);
		pStable = aStable;
	}
	WeaponDefinitionId ProductId;
	if(!CanReferencePackage(pContext, pStable) || !WeaponLuaTryStableId(pStable, &ProductId))
	{
		return Finish(-1);
	}
	pResult->m_Product = {ProductId, static_cast<uint8_t>(Level)};
	CResolvedWeaponProfile ProductProfile;
	if(!WeaponLuaTryResolve(pResult->m_Product, &ProductProfile))
	{
		return Finish(-1);
	}
	pResult->m_Level = Level;
	pResult->m_Cost = Cost;
	str_copy(pResult->m_aName, Recipe.m_aName, sizeof(pResult->m_aName));
	return Finish(1);
}
} // namespace

bool WeaponLuaInitialize(char *pError, int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = '\0';
	if(gs_Initialized)
		return true;
	if(gs_Initializing)
	{
		CopyError(pError, ErrorSize, "recursive weapon Lua initialization");
		return false;
	}
	gs_Initializing = true;
	const bool Complete = InitializeOfficialSource(
		reinterpret_cast<const char *>(gs_aOfficialWeaponsLua), gs_aOfficialWeaponsLuaSize, pError, ErrorSize);
	gs_Initializing = false;
	return Complete;
}

const char *WeaponLuaOfficialContentHash()
{
	if(!gs_aOfficialContentHash[0])
	{
		CSha256 Hash;
		unsigned char aDigest[32];
		Hash.Update(gs_aWeaponDslLua, gs_aWeaponDslLuaSize);
		Hash.Update(gs_aOfficialWeaponsLua, gs_aOfficialWeaponsLuaSize);
		Hash.Finish(aDigest);
		CSha256::ToHex(aDigest, gs_aOfficialContentHash);
	}
	return gs_aOfficialContentHash;
}

bool WeaponLuaLoadPackage(const char *pPackageId,
						  int Capabilities,
						  const char *const *ppDependencies,
						  int DependencyCount,
						  const char *pSource,
						  int SourceSize,
						  char *pError,
						  int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = '\0';
	if(!WeaponLuaInitialize(pError, ErrorSize) || !IsPublishedFileId(pPackageId) || !pSource || SourceSize <= 0 ||
	   SourceSize > 1024 * 1024)
	{
		if(pError && ErrorSize > 0 && !pError[0])
			CopyError(pError, ErrorSize, "invalid weapon definition package");
		return false;
	}
	if(gs_CustomFinalized)
	{
		CopyError(pError, ErrorSize, "weapon collection is already finalized");
		return false;
	}
	mem_zero(gs_aForgeCache, sizeof(gs_aForgeCache));
	gs_ForgeCacheNext = 0;
	if(DependencyCount < 0 || DependencyCount > 32)
	{
		CopyError(pError, ErrorSize, "invalid weapon package dependency count");
		return false;
	}
	if(DependencyCount > 0 && !ppDependencies)
	{
		CopyError(pError, ErrorSize, "weapon package dependencies are missing");
		return false;
	}
	for(int i = 0; i < DependencyCount; ++i)
	{
		if(!IsPublishedFileId(ppDependencies[i]) || str_comp(ppDependencies[i], pPackageId) == 0)
		{
			CopyError(pError, ErrorSize, "weapon package dependency is invalid");
			return false;
		}
		for(int j = 0; j < i; ++j)
			if(str_comp(ppDependencies[i], ppDependencies[j]) == 0)
			{
				CopyError(pError, ErrorSize, "weapon package dependency is duplicated");
				return false;
			}
	}
	if(gs_CustomStateCount == 0)
	{
		CLoadContext *pNewContext = new CLoadContext{};
		pNewContext->m_MemoryCeiling = LUA_WEAPON_MEMORY_LIMIT;
		if(!InitializeCustomState(pNewContext, pError, ErrorSize))
		{
			delete pNewContext;
			return false;
		}
	}
	CLoadContext *pContext = gs_apCustomContexts[0];
	str_copy(pContext->m_aPackageId, pPackageId, sizeof(pContext->m_aPackageId));
	pContext->m_Capabilities = Capabilities;
	pContext->m_DependencyCount = DependencyCount;
	for(int i = 0; i < DependencyCount; ++i)
		str_copy(pContext->m_aaDependencies[i], ppDependencies[i], sizeof(pContext->m_aaDependencies[i]));
	pContext->m_aError[0] = '\0';
	pContext->m_Instructions = 0;
	pContext->m_MemoryCeiling = LUA_WEAPON_MEMORY_LIMIT;
	pContext->m_StartEntryCount = gs_EntryCount;
	const int StartModuleCount = gs_ModuleCount;
	const int StartComposeCount = gs_ComposeCount;
	const int StartRecipeCount = gs_ForgeRecipeCount;
	const int StartTagCount = gs_TagCount;
	const bool Loaded = ExecuteContentChunk(
		gs_apCustomStates[0], "workshop_weapon.lua", pSource, SourceSize, pContext, pError, ErrorSize);
	pContext->m_CollectionInstructions += pContext->m_Instructions;
	const bool CollectionBudgetExceeded = pContext->m_CollectionInstructions > LUA_COLLECTION_INSTRUCTION_LIMIT;
	if(!Loaded || CollectionBudgetExceeded)
	{
		if(CollectionBudgetExceeded)
			CopyError(pError, ErrorSize, "weapon collection instruction budget exceeded");
		RollbackEntries(pContext->m_StartEntryCount);
		gs_ModuleCount = StartModuleCount;
		for(int i = StartComposeCount; i < gs_ComposeCount; ++i)
			luaL_unref(gs_apCustomStates[0], LUA_REGISTRYINDEX, gs_aComposes[i].m_FunctionRef);
		for(int i = StartRecipeCount; i < gs_ForgeRecipeCount; ++i)
			luaL_unref(gs_apCustomStates[0], LUA_REGISTRYINDEX, gs_aForgeRecipes[i].m_FunctionRef);
		mem_zero(gs_aComposes + StartComposeCount, sizeof(gs_aComposes[0]) * (gs_ComposeCount - StartComposeCount));
		mem_zero(gs_aForgeRecipes + StartRecipeCount,
				 sizeof(gs_aForgeRecipes[0]) * (gs_ForgeRecipeCount - StartRecipeCount));
		gs_ComposeCount = StartComposeCount;
		gs_ForgeRecipeCount = StartRecipeCount;
		mem_zero(gs_aaTags + StartTagCount, sizeof(gs_aaTags[0]) * (gs_TagCount - StartTagCount));
		gs_TagCount = StartTagCount;
		lua_gc(gs_apCustomStates[0], LUA_GCCOLLECT, 0);
		return false;
	}
	lua_gc(gs_apCustomStates[0], LUA_GCCOLLECT, 0);
	return true;
}

bool WeaponLuaFinalize(char *pError, int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = '\0';
	if(!WeaponLuaInitialize(pError, ErrorSize))
		return false;
	if(gs_CustomFinalized)
		return true;
	struct CCandidate
	{
		const CComposeDeclaration *m_pCompose;
		const CWeaponModule *m_pFrame;
		const CWeaponModule *m_pPart;
	};
	CCandidate aCandidates[MAX_REGISTERED_WEAPONS];
	int CandidateCount = 0;
	for(int ComposeIndex = 0; ComposeIndex < gs_ComposeCount; ++ComposeIndex)
	{
		const CComposeDeclaration &Compose = gs_aComposes[ComposeIndex];
		for(int i = 0; i < Compose.m_FrameCount; ++i)
			if(!SelectorHasModuleMatch(Compose, Compose.m_aaFrames[i], MODULE_FRAME))
			{
				CopyError(pError, ErrorSize, "weapon compose frame selector matches no visible module");
				return false;
			}
		for(int i = 0; i < Compose.m_PartCount; ++i)
			if(!SelectorHasModuleMatch(Compose, Compose.m_aaParts[i], MODULE_PART))
			{
				CopyError(pError, ErrorSize, "weapon compose part selector matches no visible module");
				return false;
			}
		for(int FrameIndex = 0; FrameIndex < gs_ModuleCount; ++FrameIndex)
		{
			const CWeaponModule &Frame = gs_aModules[FrameIndex];
			if(Frame.m_Slot != MODULE_FRAME || !ModuleMatches(Compose, Frame, true))
				continue;
			for(int PartIndex = 0; PartIndex < gs_ModuleCount; ++PartIndex)
			{
				const CWeaponModule &Part = gs_aModules[PartIndex];
				if(Part.m_Slot != MODULE_PART || !ModuleMatches(Compose, Part, false))
					continue;
				for(int Existing = 0; Existing < CandidateCount; ++Existing)
					if(aCandidates[Existing].m_pFrame->m_Handle == Frame.m_Handle &&
					   aCandidates[Existing].m_pPart->m_Handle == Part.m_Handle)
					{
						CopyError(pError, ErrorSize, "multiple weapon compose builders match the same module pair");
						return false;
					}
				if(CandidateCount >= MAX_REGISTERED_WEAPONS || gs_EntryCount + CandidateCount >= MAX_REGISTERED_WEAPONS)
				{
					CopyError(pError, ErrorSize, "generated weapon definition capacity exceeded");
					return false;
				}
				aCandidates[CandidateCount++] = {&Compose, &Frame, &Part};
			}
		}
	}
	std::sort(aCandidates,
			  aCandidates + CandidateCount,
			  [](const CCandidate &Left, const CCandidate &Right)
			  {
				  const int FrameCompare = str_comp(Left.m_pFrame->m_aStableId, Right.m_pFrame->m_aStableId);
				  if(FrameCompare != 0)
					  return FrameCompare < 0;
				  const int PartCompare = str_comp(Left.m_pPart->m_aStableId, Right.m_pPart->m_aStableId);
				  if(PartCompare != 0)
					  return PartCompare < 0;
				  const int PackageCompare = str_comp(Left.m_pCompose->m_aPackageId, Right.m_pCompose->m_aPackageId);
				  return PackageCompare != 0 ? PackageCompare < 0
											 : str_comp(Left.m_pCompose->m_aId, Right.m_pCompose->m_aId) < 0;
			  });
	const int StartEntryCount = gs_EntryCount;
	for(int i = 0; i < CandidateCount; ++i)
	{
		CLoadContext *pContext = ContextForState(aCandidates[i].m_pCompose->m_pState);
		if(pContext)
			pContext->m_aError[0] = '\0';
		if(!BuildCombination(
			   *aCandidates[i].m_pCompose, *aCandidates[i].m_pFrame, *aCandidates[i].m_pPart, pError, ErrorSize))
		{
			RollbackEntries(StartEntryCount);
			return false;
		}
		if(pContext)
		{
			pContext->m_CollectionInstructions += pContext->m_Instructions;
			if(pContext->m_CollectionInstructions > LUA_COLLECTION_INSTRUCTION_LIMIT)
			{
				CopyError(pError, ErrorSize, "weapon collection instruction budget exceeded during composition");
				RollbackEntries(StartEntryCount);
				return false;
			}
		}
	}
	for(int RecipeIndex = 0; RecipeIndex < gs_ForgeRecipeCount; ++RecipeIndex)
	{
		const CForgeDeclaration &Recipe = gs_aForgeRecipes[RecipeIndex];
		for(int i = 0; i < Recipe.m_TargetCount; ++i)
			if(!ForgeSelectorHasWeaponMatch(Recipe, Recipe.m_aaTargets[i]))
			{
				CopyError(pError, ErrorSize, "forge recipe target selector matches no weapon");
				RollbackEntries(StartEntryCount);
				return false;
			}
		for(int i = 0; i < Recipe.m_MaterialCount; ++i)
			if(!ForgeSelectorHasWeaponMatch(Recipe, Recipe.m_aaMaterials[i]))
			{
				CopyError(pError, ErrorSize, "forge recipe material selector matches no weapon");
				RollbackEntries(StartEntryCount);
				return false;
			}
	}
	gs_CustomFinalized = true;
	return true;
}

void WeaponLuaResetCustom()
{
	if(!gs_Initialized)
		return;
	for(int i = 0; i < gs_CustomStateCount; ++i)
	{
		lua_close(gs_apCustomStates[i]);
		delete gs_apCustomContexts[i];
		gs_apCustomStates[i] = 0;
		gs_apCustomContexts[i] = 0;
	}
	gs_CustomStateCount = 0;
	gs_ModuleCount = gs_OfficialModuleCount;
	gs_ComposeCount = 0;
	gs_ForgeRecipeCount = 0;
	gs_CustomFinalized = false;
	gs_TagCount = gs_OfficialTagCount;
	mem_zero(gs_aForgeCache, sizeof(gs_aForgeCache));
	gs_ForgeCacheNext = 0;
	RollbackEntries(gs_OfficialEntryCount);
}

void WeaponLuaBeginCustomReload()
{
	dbg_assert(!gs_pReloadSnapshot, "weapon definition reload already active");
	CReloadSnapshot *pSnapshot = new CReloadSnapshot;
	mem_copy(pSnapshot->m_aEntries, gs_aEntries, sizeof(gs_aEntries));
	mem_copy(pSnapshot->m_apById, gs_apById, sizeof(gs_apById));
	pSnapshot->m_EntryCount = gs_EntryCount;
	pSnapshot->m_NextCustomId = gs_NextCustomId;
	mem_copy(pSnapshot->m_aDroidCombat, gs_aDroidCombat, sizeof(gs_aDroidCombat));
	mem_copy(pSnapshot->m_aDroidVisual, gs_aDroidVisual, sizeof(gs_aDroidVisual));
	mem_copy(pSnapshot->m_aDroidDeathCombat, gs_aDroidDeathCombat, sizeof(gs_aDroidDeathCombat));
	mem_copy(pSnapshot->m_aDroidDeathVisual, gs_aDroidDeathVisual, sizeof(gs_aDroidDeathVisual));
	mem_copy(pSnapshot->m_aBuildingCombat, gs_aBuildingCombat, sizeof(gs_aBuildingCombat));
	mem_copy(pSnapshot->m_aBuildingVisual, gs_aBuildingVisual, sizeof(gs_aBuildingVisual));
	mem_copy(pSnapshot->m_aDroidDefined, gs_aDroidDefined, sizeof(gs_aDroidDefined));
	mem_copy(pSnapshot->m_aDroidDeathDefined, gs_aDroidDeathDefined, sizeof(gs_aDroidDeathDefined));
	mem_copy(pSnapshot->m_aBuildingDefined, gs_aBuildingDefined, sizeof(gs_aBuildingDefined));
	mem_copy(pSnapshot->m_aaPart1NameKeys, gs_aaPart1NameKeys, sizeof(gs_aaPart1NameKeys));
	mem_copy(pSnapshot->m_aaPart2NameKeys, gs_aaPart2NameKeys, sizeof(gs_aaPart2NameKeys));
	mem_copy(pSnapshot->m_aPart1NamesDefined, gs_aPart1NamesDefined, sizeof(gs_aPart1NamesDefined));
	mem_copy(pSnapshot->m_aPart2NamesDefined, gs_aPart2NamesDefined, sizeof(gs_aPart2NamesDefined));
	pSnapshot->m_CustomFinalized = gs_CustomFinalized;
	mem_copy(pSnapshot->m_aModules, gs_aModules, sizeof(gs_aModules));
	pSnapshot->m_ModuleCount = gs_ModuleCount;
	mem_copy(pSnapshot->m_aComposes, gs_aComposes, sizeof(gs_aComposes));
	pSnapshot->m_ComposeCount = gs_ComposeCount;
	mem_copy(pSnapshot->m_aForgeRecipes, gs_aForgeRecipes, sizeof(gs_aForgeRecipes));
	pSnapshot->m_ForgeRecipeCount = gs_ForgeRecipeCount;
	mem_copy(pSnapshot->m_apCustomStates, gs_apCustomStates, sizeof(gs_apCustomStates));
	mem_copy(pSnapshot->m_apCustomContexts, gs_apCustomContexts, sizeof(gs_apCustomContexts));
	pSnapshot->m_CustomStateCount = gs_CustomStateCount;
	mem_copy(pSnapshot->m_aaTags, gs_aaTags, sizeof(gs_aaTags));
	pSnapshot->m_TagCount = gs_TagCount;
	mem_copy(pSnapshot->m_aForgeCache, gs_aForgeCache, sizeof(gs_aForgeCache));
	pSnapshot->m_ForgeCacheNext = gs_ForgeCacheNext;
	gs_pReloadSnapshot = pSnapshot;

	// Keep the old Lua states alive while ResetCustom prepares a clean candidate.
	gs_CustomStateCount = 0;
	WeaponLuaResetCustom();
}

void WeaponLuaCommitCustomReload()
{
	if(!gs_pReloadSnapshot)
		return;
	for(int i = 0; i < gs_pReloadSnapshot->m_CustomStateCount; ++i)
	{
		lua_close(gs_pReloadSnapshot->m_apCustomStates[i]);
		delete gs_pReloadSnapshot->m_apCustomContexts[i];
	}
	delete gs_pReloadSnapshot;
	gs_pReloadSnapshot = 0;
}

void WeaponLuaRollbackCustomReload()
{
	if(!gs_pReloadSnapshot)
		return;
	WeaponLuaResetCustom();
	CReloadSnapshot *pSnapshot = gs_pReloadSnapshot;
	mem_copy(gs_aEntries, pSnapshot->m_aEntries, sizeof(gs_aEntries));
	mem_copy(gs_apById, pSnapshot->m_apById, sizeof(gs_apById));
	gs_EntryCount = pSnapshot->m_EntryCount;
	gs_NextCustomId = pSnapshot->m_NextCustomId;
	mem_copy(gs_aDroidCombat, pSnapshot->m_aDroidCombat, sizeof(gs_aDroidCombat));
	mem_copy(gs_aDroidVisual, pSnapshot->m_aDroidVisual, sizeof(gs_aDroidVisual));
	mem_copy(gs_aDroidDeathCombat, pSnapshot->m_aDroidDeathCombat, sizeof(gs_aDroidDeathCombat));
	mem_copy(gs_aDroidDeathVisual, pSnapshot->m_aDroidDeathVisual, sizeof(gs_aDroidDeathVisual));
	mem_copy(gs_aBuildingCombat, pSnapshot->m_aBuildingCombat, sizeof(gs_aBuildingCombat));
	mem_copy(gs_aBuildingVisual, pSnapshot->m_aBuildingVisual, sizeof(gs_aBuildingVisual));
	mem_copy(gs_aDroidDefined, pSnapshot->m_aDroidDefined, sizeof(gs_aDroidDefined));
	mem_copy(gs_aDroidDeathDefined, pSnapshot->m_aDroidDeathDefined, sizeof(gs_aDroidDeathDefined));
	mem_copy(gs_aBuildingDefined, pSnapshot->m_aBuildingDefined, sizeof(gs_aBuildingDefined));
	mem_copy(gs_aaPart1NameKeys, pSnapshot->m_aaPart1NameKeys, sizeof(gs_aaPart1NameKeys));
	mem_copy(gs_aaPart2NameKeys, pSnapshot->m_aaPart2NameKeys, sizeof(gs_aaPart2NameKeys));
	mem_copy(gs_aPart1NamesDefined, pSnapshot->m_aPart1NamesDefined, sizeof(gs_aPart1NamesDefined));
	mem_copy(gs_aPart2NamesDefined, pSnapshot->m_aPart2NamesDefined, sizeof(gs_aPart2NamesDefined));
	gs_CustomFinalized = pSnapshot->m_CustomFinalized;
	mem_copy(gs_aModules, pSnapshot->m_aModules, sizeof(gs_aModules));
	gs_ModuleCount = pSnapshot->m_ModuleCount;
	mem_copy(gs_aComposes, pSnapshot->m_aComposes, sizeof(gs_aComposes));
	gs_ComposeCount = pSnapshot->m_ComposeCount;
	mem_copy(gs_aForgeRecipes, pSnapshot->m_aForgeRecipes, sizeof(gs_aForgeRecipes));
	gs_ForgeRecipeCount = pSnapshot->m_ForgeRecipeCount;
	mem_copy(gs_apCustomStates, pSnapshot->m_apCustomStates, sizeof(gs_apCustomStates));
	mem_copy(gs_apCustomContexts, pSnapshot->m_apCustomContexts, sizeof(gs_apCustomContexts));
	gs_CustomStateCount = pSnapshot->m_CustomStateCount;
	mem_copy(gs_aaTags, pSnapshot->m_aaTags, sizeof(gs_aaTags));
	gs_TagCount = pSnapshot->m_TagCount;
	mem_copy(gs_aForgeCache, pSnapshot->m_aForgeCache, sizeof(gs_aForgeCache));
	gs_ForgeCacheNext = pSnapshot->m_ForgeCacheNext;
	delete pSnapshot;
	gs_pReloadSnapshot = 0;
}

int WeaponLuaResolveForge(const CWeaponSpec &Target,
						  const CWeaponSpec &Material,
						  int TargetAmmo,
						  int MaterialAmmo,
						  int BaseCost,
						  int LevelCost,
						  CWeaponLuaForgeResult *pResult)
{
	if(!pResult)
		return -1;
	*pResult = {};
	if(gs_ForgeRecipeCount > 0 && !gs_CustomFinalized)
		return -1;
	for(const CForgeCacheEntry &Entry : gs_aForgeCache)
		if(Entry.m_Valid && Entry.m_Target == Target && Entry.m_Material == Material &&
		   Entry.m_TargetAmmo == TargetAmmo && Entry.m_MaterialAmmo == MaterialAmmo && Entry.m_BaseCost == BaseCost &&
		   Entry.m_LevelCost == LevelCost)
		{
			*pResult = Entry.m_Result;
			return Entry.m_Status;
		}
	auto Store = [&](int Status, const CWeaponLuaForgeResult &Result)
	{
		CForgeCacheEntry &Entry =
			gs_aForgeCache[gs_ForgeCacheNext++ % (int)(sizeof(gs_aForgeCache) / sizeof(gs_aForgeCache[0]))];
		Entry.m_Valid = true;
		Entry.m_Target = Target;
		Entry.m_Material = Material;
		Entry.m_TargetAmmo = TargetAmmo;
		Entry.m_MaterialAmmo = MaterialAmmo;
		Entry.m_BaseCost = BaseCost;
		Entry.m_LevelCost = LevelCost;
		Entry.m_Status = Status;
		Entry.m_Result = Result;
		*pResult = Result;
		return Status;
	};
	CResolvedWeaponProfile TargetProfile;
	CResolvedWeaponProfile MaterialProfile;
	if(!WeaponLuaTryResolve(Target, &TargetProfile) || !WeaponLuaTryResolve(Material, &MaterialProfile))
		return Store(-1, {});
	int PriorityCeiling = INT_MAX;
	int TotalInstructions = 0;
	while(true)
	{
		int Priority = INT_MIN;
		for(int i = 0; i < gs_ForgeRecipeCount; ++i)
			if(gs_aForgeRecipes[i].m_Priority < PriorityCeiling && gs_aForgeRecipes[i].m_Priority > Priority)
				Priority = gs_aForgeRecipes[i].m_Priority;
		if(Priority == INT_MIN)
			break;
		int MatchCount = 0;
		CWeaponLuaForgeResult Best{};
		for(int i = 0; i < gs_ForgeRecipeCount; ++i)
		{
			if(gs_aForgeRecipes[i].m_Priority != Priority)
				continue;
			CWeaponLuaForgeResult Candidate{};
			const int Status = ResolveForgeDeclaration(gs_aForgeRecipes[i],
													   TargetProfile,
													   MaterialProfile,
													   TargetAmmo,
													   MaterialAmmo,
													   BaseCost,
													   LevelCost,
													   &Candidate);
			CLoadContext *pRecipeContext = ContextForState(gs_aForgeRecipes[i].m_pState);
			if(pRecipeContext)
				TotalInstructions += pRecipeContext->m_Instructions;
			if(TotalInstructions > LUA_FORGE_RESOLVE_INSTRUCTION_LIMIT)
				return Store(-1, {});
			if(Status < 0)
				return Store(-1, {});
			if(Status > 0)
			{
				Best = Candidate;
				++MatchCount;
			}
		}
		if(MatchCount > 1)
		{
			dbg_msg("forge", "conflicting Mod forge recipes at priority %d", Priority);
			return Store(-1, {});
		}
		if(MatchCount == 1)
			return Store(1, Best);
		PriorityCeiling = Priority;
	}
	return Store(0, {});
}

bool WeaponLuaTryGetDefinition(WeaponDefinitionId Id, CWeaponDefinition *pDefinition)
{
	if(!WeaponLuaInitialize(0, 0))
		return false;
	const int Value = static_cast<int>(Id);
	if(Value <= 0 || Value >= (int)(sizeof(gs_apById) / sizeof(gs_apById[0])) || !gs_apById[Value])
		return false;
	if(pDefinition)
		*pDefinition = gs_apById[Value]->m_Definition;
	return true;
}

bool WeaponLuaTryResolve(const CWeaponSpec &Spec, CResolvedWeaponProfile *pProfile)
{
	if(!pProfile || Spec.m_Level > WEAPON_SPEC_MAX_LEVEL || !WeaponLuaInitialize(0, 0))
		return false;
	const int Value = static_cast<int>(Spec.m_DefinitionId);
	if(Value <= 0 || Value >= (int)(sizeof(gs_apById) / sizeof(gs_apById[0])) || !gs_apById[Value])
		return false;
	CWeaponEntry *pEntry = gs_apById[Value];
	*pProfile = {pEntry->m_Definition,
		Spec,
		pEntry->m_aCombat[Spec.m_Level],
		pEntry->m_aPvp[Spec.m_Level],
		pEntry->m_aVisual[Spec.m_Level]};
	return true;
}

bool WeaponLuaTryStableId(const char *pStableId, WeaponDefinitionId *pId)
{
	if(!WeaponLuaInitialize(0, 0))
		return false;
	CWeaponEntry *pEntry = FindStable(pStableId);
	if(!pEntry)
		return false;
	if(pId)
		*pId = pEntry->m_Definition.m_Id;
	return true;
}

const char *WeaponLuaStableId(WeaponDefinitionId Id)
{
	if(!WeaponLuaInitialize(0, 0))
		return 0;
	const int Value = static_cast<int>(Id);
	return Value > 0 && Value < (int)(sizeof(gs_apById) / sizeof(gs_apById[0])) && gs_apById[Value]
			   ? gs_apById[Value]->m_Definition.m_aStableId
			   : 0;
}

bool WeaponLuaTryAttack(EAttackSourceKind Kind, int Type, CWeaponCombatProfile *pCombat, CWeaponVisualProfile *pVisual)
{
	if(!WeaponLuaInitialize(0, 0))
		return false;
	CWeaponCombatProfile *pResolvedCombat = 0;
	CWeaponVisualProfile *pResolvedVisual = 0;
	if(Kind == EAttackSourceKind::Droid && Type >= 0 && Type < WEAPON_DROID_PROFILE_COUNT)
	{
		pResolvedCombat = &gs_aDroidCombat[Type];
		pResolvedVisual = &gs_aDroidVisual[Type];
	}
	else if(Kind == EAttackSourceKind::DeathEffect && Type >= 0 && Type < WEAPON_DROID_PROFILE_COUNT)
	{
		pResolvedCombat = &gs_aDroidDeathCombat[Type];
		pResolvedVisual = &gs_aDroidDeathVisual[Type];
	}
	else if(Kind == EAttackSourceKind::Building && Type >= 0 && Type < WEAPON_BUILDING_PROFILE_COUNT)
	{
		pResolvedCombat = &gs_aBuildingCombat[Type];
		pResolvedVisual = &gs_aBuildingVisual[Type];
	}
	else
		return false;
	if(pCombat)
		*pCombat = *pResolvedCombat;
	if(pVisual)
		*pVisual = *pResolvedVisual;
	return true;
}

int WeaponLuaDefinitionCount()
{
	return WeaponLuaInitialize(0, 0) ? gs_EntryCount : 0;
}

bool WeaponLuaDefinitionByIndex(int Index, CWeaponDefinition *pDefinition)
{
	if(!WeaponLuaInitialize(0, 0) || Index < 0 || Index >= gs_EntryCount)
		return false;
	if(pDefinition)
		*pDefinition = gs_aEntries[Index].m_Definition;
	return true;
}

const char *WeaponLuaPart1NameKey(int Part1)
{
	static const char s_aUnknown[] = "Unknown item";
	return WeaponLuaInitialize(0, 0) && Part1 > 0 && Part1 <= NUM_PART1 && gs_aPart1NamesDefined[Part1]
			   ? gs_aaPart1NameKeys[Part1]
			   : s_aUnknown;
}

const char *WeaponLuaPart2NameKey(int Part2)
{
	static const char s_aUnknown[] = "Unknown item";
	return WeaponLuaInitialize(0, 0) && Part2 > 0 && Part2 < PART2_END && gs_aPart2NamesDefined[Part2]
			   ? gs_aaPart2NameKeys[Part2]
			   : s_aUnknown;
}
