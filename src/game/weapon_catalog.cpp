#include "weapon_catalog.h"
#include "weapon_lua.h"

#include <base/system.h>
#include <engine/shared/mod_api.h>
#include <generated/game_data.h>

namespace
{
constexpr int ToInt(WeaponDefinitionId Id)
{
	return static_cast<int>(Id);
}

bool IsValidPartCombination(int Part1, int Part2)
{
	if(Part1 < PART1_BASE1 || Part1 > PART1_SPIN || Part2 < PART2_BARREL1 || Part2 > PART2_MELEE6)
		return false;
	if(Part1 <= PART1_BASE6)
		return Part2 <= PART2_RAIL;
	return Part2 >= PART2_MELEE1;
}

int DenseDefinitionIndex(const CWeaponDefinition &Definition)
{
	if(Definition.m_Kind == EWeaponDefinitionKind::Static)
		return Definition.m_StaticType;
	if(Definition.m_Part1 <= PART1_BASE6)
		return NUM_STATIC_WEAPONS + (Definition.m_Part1 - PART1_BASE1) * WEAPON_RANGED_PART2_COUNT +
			   Definition.m_Part2 - PART2_BARREL1;
	return NUM_STATIC_WEAPONS + WEAPON_RANGED_PART1_COUNT * WEAPON_RANGED_PART2_COUNT +
		   (Definition.m_Part1 - PART1_MELEE) * WEAPON_MELEE_PART2_COUNT + Definition.m_Part2 - PART2_MELEE1;
}

CWeaponVisualProfile EmptyVisualProfile()
{
	return {0,
			ivec2(0, 0),
			ivec2(0, 0),
			vec2(0.0f, 0.0f),
			vec2(0.0f, 0.0f),
			vec2(0.0f, 0.0f),
			vec2(0.0f, 0.0f),
			vec2(0.0f, 0.0f),
			0.0f,
			0.0f,
			0.0f,
			0,
			0.0f,
			0,
			0,
			0,
			0,
			0,
			0,
			0.0f,
			WEAPON_IMPACT_EFFECT_NONE};
}

bool IsValidSound(int Sound)
{
	return Sound >= -1 && Sound < NUM_SOUNDS;
}

bool IsValidProfile(const CWeaponCombatProfile &Combat, const CWeaponVisualProfile &Visual)
{
	if(Combat.m_FiringType < WFT_NONE || Combat.m_FiringType > WFT_ACTIVATE || Combat.m_MaxAmmo < 0 ||
	   Combat.m_ShotSpread < 0 || Combat.m_Cost < 0 || Combat.m_ProjectileBounces < 0 ||
	   Combat.m_ProjectilePenetration < WEAPON_INFINITE_PENETRATION || Combat.m_ChargePenetrationMax < 0 ||
	   Combat.m_ChargeDamageMin < 0.0f || Combat.m_ChargeDamageMax < Combat.m_ChargeDamageMin ||
	   Combat.m_ChargeRangeMin < 0.0f || Combat.m_ChargeRangeMax < Combat.m_ChargeRangeMin ||
	   Combat.m_ChargePowerMin < 0.0f || Combat.m_ChargePowerMax < Combat.m_ChargePowerMin ||
	   Combat.m_CursorWeapon < 0 || Combat.m_CursorWeapon >= NUM_WEAPONS ||
	   Combat.m_ProjectilePosType < WEAPON_PROJECTILE_PATH_STANDARD ||
	   Combat.m_ProjectilePosType > WEAPON_PROJECTILE_PATH_ROCKET)
		return false;
	if(Combat.m_FiringType == WFT_PROJECTILE && Combat.m_ProjectileLife <= 0.0f)
		return false;
	if(Visual.m_RenderType < WRT_NONE || Visual.m_RenderType > WRT_SPIN || Visual.m_VisualSize.x < 0 ||
	   Visual.m_VisualSize.y < 0 || Visual.m_VisualSize2.x < 0 || Visual.m_VisualSize2.y < 0 ||
	   Visual.m_ProjectileSprite < 0.0f || Visual.m_ProjectileSprite != static_cast<int>(Visual.m_ProjectileSprite) ||
	   SPRITE_PROJECTILE1_1 + static_cast<int>(Visual.m_ProjectileSprite) >= NUM_SPRITES ||
	   Visual.m_ExplosionSprite < 0 || Visual.m_ExplosionSprite >= NUM_SPRITES || Visual.m_MuzzleType < 0 ||
	   Visual.m_MuzzleType > 2 || Visual.m_MuzzleAmount < 0 || Visual.m_ImpactEffect < WEAPON_IMPACT_EFFECT_NONE ||
	   Visual.m_ImpactEffect >= NUM_WEAPON_IMPACT_EFFECTS)
		return false;
	return IsValidSound(Visual.m_FireSound) && IsValidSound(Visual.m_FireSound2) &&
		   IsValidSound(Visual.m_ExplosionSound);
}

} // namespace

bool CWeaponCatalog::Initialize(char *pError, int ErrorSize)
{
	return WeaponLuaInitialize(pError, ErrorSize);
}

const char *CWeaponCatalog::OfficialContentHash()
{
	return WeaponLuaOfficialContentHash();
}

bool CWeaponCatalog::LoadLuaDefinitions(
	const char *pPackageId, const char *pSource, int SourceSize, char *pError, int ErrorSize)
{
	return WeaponLuaLoadPackage(pPackageId,
								MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_WEAPON_MODULES | MOD_CAPABILITY_FORGE_RECIPES,
								0,
								0,
								pSource,
								SourceSize,
								pError,
								ErrorSize);
}

bool CWeaponCatalog::LoadLuaDefinitions(const char *pPackageId,
										int Capabilities,
										const char *const *ppDependencies,
										int DependencyCount,
										const char *pSource,
										int SourceSize,
										char *pError,
										int ErrorSize)
{
	return WeaponLuaLoadPackage(
		pPackageId, Capabilities, ppDependencies, DependencyCount, pSource, SourceSize, pError, ErrorSize);
}

bool CWeaponCatalog::FinalizeLuaDefinitions(char *pError, int ErrorSize)
{
	return WeaponLuaFinalize(pError, ErrorSize);
}

void CWeaponCatalog::ResetCustomDefinitions()
{
	WeaponLuaResetCustom();
}

void CWeaponCatalog::BeginCustomDefinitionReload()
{
	WeaponLuaBeginCustomReload();
}

void CWeaponCatalog::CommitCustomDefinitionReload()
{
	WeaponLuaCommitCustomReload();
}

void CWeaponCatalog::RollbackCustomDefinitionReload()
{
	WeaponLuaRollbackCustomReload();
}

bool CWeaponCatalog::TryFromStableId(const char *pStableId, int Level, CWeaponSpec *pSpec)
{
	if(Level < 0 || Level > WEAPON_SPEC_MAX_LEVEL)
		return false;
	WeaponDefinitionId Id;
	if(!WeaponLuaTryStableId(pStableId, &Id))
		return false;
	const CWeaponSpec Spec{Id, static_cast<uint8_t>(Level)};
	if(pSpec)
		*pSpec = Spec;
	return true;
}

const char *CWeaponCatalog::StableId(const CWeaponSpec &Spec)
{
	return WeaponLuaStableId(Spec.m_DefinitionId);
}

bool CWeaponCatalog::IsCustom(const CWeaponSpec &Spec)
{
	CWeaponDefinition Definition;
	return TryGetDefinition(Spec.m_DefinitionId, &Definition) && Definition.m_Custom;
}

int CWeaponCatalog::DefinitionCount()
{
	return WeaponLuaDefinitionCount();
}

bool CWeaponCatalog::TryGetDefinitionByIndex(int Index, CWeaponDefinition *pDefinition)
{
	return WeaponLuaDefinitionByIndex(Index, pDefinition);
}

bool CWeaponCatalog::TryGetDefinition(WeaponDefinitionId Id, CWeaponDefinition *pDefinition)
{
	return WeaponLuaTryGetDefinition(Id, pDefinition);
}

CWeaponSpec CWeaponCatalog::Static(StaticWeaponType Type, int Level)
{
	if(Type < 0 || Type >= NUM_STATIC_WEAPONS || Level < 0 || Level > WEAPON_SPEC_MAX_LEVEL)
		return {};
	for(int Index = 0; Index < DefinitionCount(); ++Index)
	{
		CWeaponDefinition Definition;
		if(TryGetDefinitionByIndex(Index, &Definition) && Definition.m_Kind == EWeaponDefinitionKind::Static &&
		   Definition.m_StaticType == Type)
			return {Definition.m_Id, static_cast<uint8_t>(Level)};
	}
	return {};
}

CWeaponSpec CWeaponCatalog::Modular(int Part1, int Part2, int Level)
{
	if(!IsValidPartCombination(Part1, Part2) || Level < 0 || Level > WEAPON_SPEC_MAX_LEVEL)
		return {};
	for(int Index = 0; Index < DefinitionCount(); ++Index)
	{
		CWeaponDefinition Definition;
		if(TryGetDefinitionByIndex(Index, &Definition) && Definition.m_Kind == EWeaponDefinitionKind::Modular &&
		   Definition.m_Part1 == Part1 && Definition.m_Part2 == Part2)
			return {Definition.m_Id, static_cast<uint8_t>(Level)};
	}
	return {};
}

const char *CWeaponCatalog::Part1NameKey(int Part1)
{
	return WeaponLuaPart1NameKey(Part1);
}

const char *CWeaponCatalog::Part2NameKey(int Part2)
{
	return WeaponLuaPart2NameKey(Part2);
}

bool CWeaponCatalog::IsValidSpec(const CWeaponSpec &Spec)
{
	CWeaponDefinition Definition;
	return TryGetDefinition(Spec.m_DefinitionId, &Definition) && Spec.m_Level <= WEAPON_SPEC_MAX_LEVEL;
}

bool CWeaponCatalog::TryFromProtocol(int DefinitionId, int Level, CWeaponSpec *pSpec)
{
	if(DefinitionId < 0 || Level < 0 || Level > WEAPON_SPEC_MAX_LEVEL)
		return false;
	const CWeaponSpec Spec{static_cast<WeaponDefinitionId>(DefinitionId), static_cast<uint8_t>(Level)};
	if(!IsValidSpec(Spec))
		return false;
	if(pSpec)
		*pSpec = Spec;
	return true;
}

bool CWeaponCatalog::TryResolve(const CWeaponSpec &Spec, CResolvedWeaponProfile *pProfile)
{
	return WeaponLuaTryResolve(Spec, pProfile);
}

bool CWeaponCatalog::Validate()
{
	CWeaponSpec InvalidSpec;
	if(IsValidSpec(InvalidSpec) || TryFromProtocol(0, 0, &InvalidSpec) || TryFromProtocol(65535, 0, &InvalidSpec) ||
	   TryFromProtocol(ToInt(WeaponDefinitionId::StaticFirst), WEAPON_SPEC_MAX_LEVEL + 1, &InvalidSpec))
		return false;

	bool aSeenDefinitions[WEAPON_DEFINITION_COUNT] = {};
	int DefinitionCount = 0;
	for(int Value = ToInt(WeaponDefinitionId::StaticFirst); Value <= ToInt(WeaponDefinitionId::ModularLast); ++Value)
	{
		CWeaponDefinition Definition;
		const auto Id = static_cast<WeaponDefinitionId>(Value);
		if(!TryGetDefinition(Id, &Definition))
			continue;
		const int DenseIndex = DenseDefinitionIndex(Definition);
		if(Definition.m_Id != Id || Definition.m_MaxLevel > WEAPON_SPEC_MAX_LEVEL || DenseIndex < 0 ||
		   DenseIndex >= WEAPON_DEFINITION_COUNT || aSeenDefinitions[DenseIndex])
			return false;
		aSeenDefinitions[DenseIndex] = true;
		++DefinitionCount;
		const CWeaponSpec FactorySpec = Definition.m_Kind == EWeaponDefinitionKind::Static
											? Static(static_cast<StaticWeaponType>(Definition.m_StaticType))
											: Modular(Definition.m_Part1, Definition.m_Part2);
		if(FactorySpec.m_DefinitionId != Id)
			return false;
		for(int Level = 0; Level <= WEAPON_SPEC_MAX_LEVEL; ++Level)
		{
			const CWeaponSpec Spec{Id, static_cast<uint8_t>(Level)};
			CResolvedWeaponProfile Profile;
			if(!TryResolve(Spec, &Profile))
				return false;
			if(!(Profile.m_Spec == Spec) || Profile.m_Definition.m_Id != Id)
				return false;
			if(!IsValidProfile(Profile.m_Combat, Profile.m_Visual))
			{
				dbg_msg("weapon-catalog", "invalid player profile definition=%d level=%d", Value, Level);
				return false;
			}
		}
	}
	if(DefinitionCount != WEAPON_DEFINITION_COUNT)
		return false;
	CResolvedWeaponProfile Base5;
	CResolvedWeaponProfile ProjectileCapacitor;
	CResolvedWeaponProfile ExplosiveCapacitor;
	CResolvedWeaponProfile Capacitor;
	CResolvedWeaponProfile Ricochet;
	CResolvedWeaponProfile RicochetLevel4;
	CResolvedWeaponProfile RicochetLevel15;
	CResolvedWeaponProfile Rail;
	CResolvedWeaponProfile LaserRail;
	CResolvedWeaponProfile LightBlade;
	CResolvedWeaponProfile SpinLightBlade;
	CResolvedWeaponProfile ChargedBlade;
	CResolvedWeaponProfile SpinChargedBlade;
	if(!TryResolve(Modular(PART1_BASE5, PART2_BARREL1), &Base5) || Base5.m_Combat.m_ProjectileDamage != 20.0f ||
	   Base5.m_Combat.m_MaxAmmo != 16 || !Base5.m_Combat.m_LaserWeapon || !Base5.m_Combat.m_Aimline ||
	   !Base5.m_Combat.m_ValidForTurret || !TryResolve(Modular(PART1_BASE1, PART2_CAPACITOR), &ProjectileCapacitor) ||
	   ProjectileCapacitor.m_Combat.m_LaserWeapon || ProjectileCapacitor.m_Combat.m_ExplosiveProjectile ||
	   !TryResolve(Modular(PART1_BASE2, PART2_CAPACITOR), &ExplosiveCapacitor) ||
	   ExplosiveCapacitor.m_Combat.m_LaserWeapon || !ExplosiveCapacitor.m_Combat.m_ExplosiveProjectile ||
	   !TryResolve(Modular(PART1_BASE5, PART2_CAPACITOR), &Capacitor) ||
	   Capacitor.m_Combat.m_FiringType != WFT_CHARGE || Capacitor.m_Combat.m_ProjectileDamage != 40.0f ||
	   Capacitor.m_Combat.m_MaxAmmo != 9 || !Capacitor.m_Combat.m_LaserWeapon ||
	   Capacitor.m_Combat.m_LaserRange != 900 || Capacitor.m_Combat.m_ValidForTurret ||
	   !TryResolve(Modular(PART1_BASE6, PART2_BARREL1), &Ricochet) || Ricochet.m_Combat.m_MaxAmmo != 20 ||
	   Ricochet.m_Combat.m_ProjectileDamage != 15.6f || Ricochet.m_Combat.m_ProjectileSpeed != 1260.0f ||
	   Ricochet.m_Combat.m_ProjectileBounces != 3 || Ricochet.m_Visual.m_ProjectileSprite != 13.0f ||
	   !Ricochet.m_Combat.m_ValidForTurret || !TryResolve(Modular(PART1_BASE6, PART2_BARREL1, 4), &RicochetLevel4) ||
	   RicochetLevel4.m_Combat.m_ProjectileBounces != 7 ||
	   !TryResolve(Modular(PART1_BASE6, PART2_BARREL1, 15), &RicochetLevel15) ||
	   RicochetLevel15.m_Combat.m_ProjectileBounces != 18 || !TryResolve(Modular(PART1_BASE1, PART2_RAIL), &Rail) ||
	   Rail.m_Combat.m_MaxAmmo != 13 || Rail.m_Combat.m_ProjectileDamage != 23.0f ||
	   Rail.m_Combat.m_ProjectileSpeed != 1680.0f || !Rail.m_Combat.m_Aimline || Rail.m_Combat.m_LaserWeapon ||
	   !TryResolve(Modular(PART1_BASE5, PART2_RAIL), &LaserRail) || LaserRail.m_Combat.m_MaxAmmo != 8 ||
	   LaserRail.m_Combat.m_ProjectileDamage != 23.0f || !LaserRail.m_Combat.m_LaserWeapon ||
	   !LaserRail.m_Combat.m_Aimline || !TryResolve(Modular(PART1_MELEE, PART2_MELEE5, 4), &LightBlade) ||
	   LightBlade.m_Combat.m_FireRate != 220.0f || LightBlade.m_Combat.m_ProjectileDamage != 32.0f ||
	   LightBlade.m_Combat.m_ProjectileKnockback != 5.6f || LightBlade.m_Combat.m_MeleeHitRadius != 76.0f ||
	   !TryResolve(Modular(PART1_SPIN, PART2_MELEE5, 4), &SpinLightBlade) ||
	   SpinLightBlade.m_Combat.m_FireRate != 50.0f || SpinLightBlade.m_Combat.m_ProjectileDamage != 8.0f ||
	   SpinLightBlade.m_Combat.m_ProjectileKnockback != 1.4f || SpinLightBlade.m_Combat.m_MeleeHitRadius != 96.0f ||
	   !TryResolve(Modular(PART1_MELEE, PART2_MELEE6, 4), &ChargedBlade) ||
	   ChargedBlade.m_Combat.m_FiringType != WFT_CHARGE || ChargedBlade.m_Combat.m_FireRate != 320.0f ||
	   ChargedBlade.m_Combat.m_ProjectileDamage != 40.0f || ChargedBlade.m_Combat.m_MeleeHitRadius != 80.0f ||
	   !TryResolve(Modular(PART1_SPIN, PART2_MELEE6, 4), &SpinChargedBlade) ||
	   SpinChargedBlade.m_Combat.m_FiringType != WFT_HOLD || SpinChargedBlade.m_Combat.m_FireRate != 60.0f ||
	   SpinChargedBlade.m_Combat.m_ProjectileDamage != 11.0f || SpinChargedBlade.m_Combat.m_MeleeHitRadius != 104.0f)
		return false;
	for(int Type = 0; Type < WEAPON_DROID_PROFILE_COUNT; ++Type)
	{
		CWeaponCombatProfile Combat;
		CWeaponVisualProfile Visual;
		if(!TryResolveAttack(CAttackSource::Droid(-1, Type), &Combat, &Visual) || !IsValidProfile(Combat, Visual))
		{
			dbg_msg("weapon-catalog", "invalid droid profile type=%d", Type);
			return false;
		}
		if(!TryResolveAttack(CAttackSource::Droid(-1, Type, true), &Combat, &Visual) || !IsValidProfile(Combat, Visual))
		{
			dbg_msg("weapon-catalog", "invalid droid death profile type=%d", Type);
			return false;
		}
	}
	for(int Type = 0; Type < WEAPON_BUILDING_PROFILE_COUNT; ++Type)
	{
		CWeaponCombatProfile Combat;
		CWeaponVisualProfile Visual;
		if(!TryResolveAttack(CAttackSource::Building(-1, Type), &Combat, &Visual) || !IsValidProfile(Combat, Visual))
		{
			dbg_msg("weapon-catalog", "invalid building profile type=%d", Type);
			return false;
		}
	}
	CWeaponCombatProfile AcidCombat;
	if(!TryResolveAttack(CAttackSource::World(WEAPON_ACID), &AcidCombat) || AcidCombat.m_ProjectileSpeed != 0.0f ||
	   AcidCombat.m_FlameAmount != 0.0f || AcidCombat.m_ElectroAmount != 0.0f)
		return false;
	CAttackSource ProtocolSource;
	if(TryAttackSourceFromProtocol(
		   static_cast<int>(EAttackSourceKind::Droid), WEAPON_DROID_PROFILE_COUNT, 0, 0, &ProtocolSource) ||
	   TryAttackSourceFromProtocol(static_cast<int>(EAttackSourceKind::Building), 0, 0, 0, &ProtocolSource) ||
	   TryAttackSourceFromProtocol(static_cast<int>(EAttackSourceKind::World), WEAPON_ACID, 1, 0, &ProtocolSource))
		return false;
	return true;
}

CAttackSource CAttackSource::PlayerWeapon(int Owner, CWeaponSpec Weapon)
{
	return {EAttackSourceKind::PlayerWeapon, Owner, 0, Weapon};
}

CAttackSource CAttackSource::Droid(int Owner, int DroidType, bool OnDeath)
{
	return {OnDeath ? EAttackSourceKind::DeathEffect : EAttackSourceKind::Droid, Owner, DroidType, {}};
}

CAttackSource CAttackSource::Building(int Owner, int BuildingType)
{
	return {EAttackSourceKind::Building, Owner, BuildingType, {}};
}

CAttackSource CAttackSource::World(int Type, int Owner)
{
	CAttackSource Source;
	Source.m_Type = Type;
	Source.m_Owner = Owner;
	return Source;
}

bool CWeaponCatalog::TryResolveAttack(const CAttackSource &Source,
									  CWeaponCombatProfile *pCombat,
									  CWeaponVisualProfile *pVisual)
{
	if(Source.m_Kind == EAttackSourceKind::PlayerWeapon)
	{
		CResolvedWeaponProfile Profile;
		if(!TryResolve(Source.m_Weapon, &Profile))
			return false;
		if(pCombat)
			*pCombat = Profile.m_Combat;
		if(pVisual)
			*pVisual = Profile.m_Visual;
		return true;
	}

	switch(Source.m_Kind)
	{
		case EAttackSourceKind::Droid:
		case EAttackSourceKind::DeathEffect:
		case EAttackSourceKind::Building:
			return WeaponLuaTryAttack(Source.m_Kind, Source.m_Type, pCombat, pVisual);
		case EAttackSourceKind::World:
			if(Source.m_Type == 0)
				return false;
			if(pCombat)
				*pCombat = {};
			if(pVisual)
				*pVisual = EmptyVisualProfile();
			return true;
		case EAttackSourceKind::PlayerWeapon:
			return false;
	}
	return false;
}

bool CWeaponCatalog::TryAttackSourceFromProtocol(
	int Kind, int Type, int DefinitionId, int Level, CAttackSource *pSource)
{
	if(!pSource || Kind < static_cast<int>(EAttackSourceKind::PlayerWeapon) ||
	   Kind > static_cast<int>(EAttackSourceKind::DeathEffect))
		return false;
	CAttackSource Source;
	Source.m_Kind = static_cast<EAttackSourceKind>(Kind);
	Source.m_Type = Type;
	switch(Source.m_Kind)
	{
		case EAttackSourceKind::PlayerWeapon:
			if(Type != 0 || !TryFromProtocol(DefinitionId, Level, &Source.m_Weapon))
				return false;
			break;
		case EAttackSourceKind::Droid:
		case EAttackSourceKind::DeathEffect:
			if(Type < 0 || Type >= WEAPON_DROID_PROFILE_COUNT)
				return false;
			break;
		case EAttackSourceKind::Building:
			if(Type <= 0 || Type >= WEAPON_BUILDING_PROFILE_COUNT)
				return false;
			break;
		case EAttackSourceKind::World:
			if(Type == 0)
				return false;
			break;
	}
	if(Source.m_Kind != EAttackSourceKind::PlayerWeapon && (DefinitionId != 0 || Level != 0))
		return false;
	*pSource = Source;
	return true;
}
