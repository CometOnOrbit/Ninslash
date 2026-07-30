#ifndef GAME_WEAPON_LUA_H
#define GAME_WEAPON_LUA_H

#include "weapon_catalog.h"

bool WeaponLuaInitialize(char *pError, int ErrorSize);
bool WeaponLuaLoadPackage(const char *pPackageId,
						  int Capabilities,
						  const char *const *ppDependencies,
						  int DependencyCount,
						  const char *pSource,
						  int SourceSize,
						  char *pError,
						  int ErrorSize);
bool WeaponLuaFinalize(char *pError, int ErrorSize);
void WeaponLuaResetCustom();
void WeaponLuaBeginCustomReload();
void WeaponLuaCommitCustomReload();
void WeaponLuaRollbackCustomReload();
bool WeaponLuaTryGetDefinition(WeaponDefinitionId Id, CWeaponDefinition *pDefinition);
bool WeaponLuaTryResolve(const CWeaponSpec &Spec, CResolvedWeaponProfile *pProfile);
bool WeaponLuaTryStableId(const char *pStableId, WeaponDefinitionId *pId);
const char *WeaponLuaStableId(WeaponDefinitionId Id);
bool WeaponLuaTryAttack(EAttackSourceKind Kind, int Type, CWeaponCombatProfile *pCombat, CWeaponVisualProfile *pVisual);
int WeaponLuaDefinitionCount();
bool WeaponLuaDefinitionByIndex(int Index, CWeaponDefinition *pDefinition);
const char *WeaponLuaOfficialContentHash();
const char *WeaponLuaPart1NameKey(int Part1);
const char *WeaponLuaPart2NameKey(int Part2);

struct CWeaponLuaForgeResult
{
	CWeaponSpec m_Product;
	int m_Level;
	int m_Cost;
	char m_aName[128];
};
// Returns 1 for a selected recipe, 0 for no match, and -1 for an invalid or
// conflicting Mod recipe resolution.
int WeaponLuaResolveForge(const CWeaponSpec &Target,
						  const CWeaponSpec &Material,
						  int TargetAmmo,
						  int MaterialAmmo,
						  int BaseCost,
						  int LevelCost,
						  CWeaponLuaForgeResult *pResult);

#endif
