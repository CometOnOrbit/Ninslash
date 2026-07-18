#include "weapon_catalog.h"

#include <base/system.h>
#include <generated/game_data.h>

namespace
{
constexpr int ToInt(WeaponDefinitionId Id)
{
	return static_cast<int>(Id);
}

bool IsValidPartCombination(int Part1, int Part2)
{
	if(Part1 < PART1_BASE1 || Part1 > PART1_SPIN || Part2 < PART2_BARREL1 || Part2 > PART2_MELEE4)
		return false;
	if(Part1 <= PART1_BASE4)
		return Part2 <= PART2_CHARGE;
	return Part2 >= PART2_MELEE1;
}

int DenseDefinitionIndex(const CWeaponDefinition &Definition)
{
	if(Definition.m_Kind == EWeaponDefinitionKind::Static)
		return Definition.m_StaticType;
	if(Definition.m_Part1 <= PART1_BASE4)
		return NUM_STATIC_WEAPONS +
			(Definition.m_Part1 - PART1_BASE1) * WEAPON_RANGED_PART2_COUNT +
			Definition.m_Part2 - PART2_BARREL1;
	return NUM_STATIC_WEAPONS + WEAPON_RANGED_PART1_COUNT * WEAPON_RANGED_PART2_COUNT +
		(Definition.m_Part1 - PART1_MELEE) * WEAPON_MELEE_PART2_COUNT +
		Definition.m_Part2 - PART2_MELEE1;
}

CWeaponVisualProfile EmptyVisualProfile()
{
	return {0, ivec2(0, 0), ivec2(0, 0), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f),
		vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), 0.0f, 0.0f, 0.0f, 0, 0.0f, 0, 0, 0, 0, 0, 0, 0.0f};
}

bool IsValidSound(int Sound)
{
	return Sound >= -1 && Sound < NUM_SOUNDS;
}

bool IsValidProfile(const CWeaponCombatProfile &Combat, const CWeaponVisualProfile &Visual)
{
	if(Combat.m_FiringType < WFT_NONE || Combat.m_FiringType > WFT_ACTIVATE || Combat.m_MaxAmmo < 0 ||
		Combat.m_ShotSpread < 0 || Combat.m_Cost < 0 || Combat.m_ProjectileBounces < 0 ||
		Combat.m_CursorWeapon < 0 || Combat.m_CursorWeapon >= NUM_WEAPONS ||
		Combat.m_ProjectilePosType < WEAPON_PROJECTILE_PATH_STANDARD || Combat.m_ProjectilePosType > WEAPON_PROJECTILE_PATH_ROCKET)
		return false;
	if(Combat.m_FiringType == WFT_PROJECTILE && Combat.m_ProjectileLife <= 0.0f)
		return false;
	if(Visual.m_RenderType < WRT_NONE || Visual.m_RenderType > WRT_SPIN ||
		Visual.m_VisualSize.x < 0 || Visual.m_VisualSize.y < 0 || Visual.m_VisualSize2.x < 0 || Visual.m_VisualSize2.y < 0 ||
		Visual.m_ProjectileSprite < 0.0f || Visual.m_ProjectileSprite != static_cast<int>(Visual.m_ProjectileSprite) ||
		SPRITE_PROJECTILE1_1 + static_cast<int>(Visual.m_ProjectileSprite) >= NUM_SPRITES ||
		Visual.m_ExplosionSprite < 0 || Visual.m_ExplosionSprite >= NUM_SPRITES ||
		Visual.m_MuzzleType < 0 || Visual.m_MuzzleType > 2 || Visual.m_MuzzleAmount < 0)
		return false;
	return IsValidSound(Visual.m_FireSound) && IsValidSound(Visual.m_FireSound2) && IsValidSound(Visual.m_ExplosionSound);
}

#include <generated/weapon_profiles.inc>
}

bool CWeaponCatalog::TryGetDefinition(WeaponDefinitionId Id, CWeaponDefinition *pDefinition)
{
	const int Value = ToInt(Id);
	CWeaponDefinition Definition{};
	Definition.m_Id = Id;
	if(Value >= ToInt(WeaponDefinitionId::StaticFirst) && Value <= ToInt(WeaponDefinitionId::StaticLast))
	{
		Definition.m_Kind = EWeaponDefinitionKind::Static;
		Definition.m_StaticType = Value - ToInt(WeaponDefinitionId::StaticFirst);
	}
	else if(Value >= ToInt(WeaponDefinitionId::ModularFirst) && Value <= ToInt(WeaponDefinitionId::ModularLast))
	{
		const int Index = Value - ToInt(WeaponDefinitionId::ModularFirst);
		Definition.m_Kind = EWeaponDefinitionKind::Modular;
		Definition.m_Part1 = Index / WEAPON_MODULAR_PART2_COUNT + PART1_BASE1;
		Definition.m_Part2 = Index % WEAPON_MODULAR_PART2_COUNT + PART2_BARREL1;
		if(!IsValidPartCombination(Definition.m_Part1, Definition.m_Part2))
			return false;
	}
	else
		return false;

	const int DenseIndex = DenseDefinitionIndex(Definition);
	if(DenseIndex < 0 || DenseIndex >= WEAPON_DEFINITION_COUNT)
		return false;
	Definition.m_MaxLevel = s_aWeaponMaxLevels[DenseIndex];
	if(pDefinition)
		*pDefinition = Definition;
	return true;
}

CWeaponSpec CWeaponCatalog::Static(StaticWeaponType Type, int Level)
{
	if(Type < 0 || Type >= NUM_STATIC_WEAPONS || Level < 0 || Level > WEAPON_SPEC_MAX_LEVEL)
		return {};
	const CWeaponSpec Spec{static_cast<WeaponDefinitionId>(ToInt(WeaponDefinitionId::StaticFirst) + Type), static_cast<uint8_t>(Level)};
	return IsValidSpec(Spec) ? Spec : CWeaponSpec{};
}

CWeaponSpec CWeaponCatalog::Modular(int Part1, int Part2, int Level)
{
	if(!IsValidPartCombination(Part1, Part2) || Level < 0 || Level > WEAPON_SPEC_MAX_LEVEL)
		return {};
	const CWeaponSpec Spec{static_cast<WeaponDefinitionId>(ToInt(WeaponDefinitionId::ModularFirst) + (Part1 - PART1_BASE1) * WEAPON_MODULAR_PART2_COUNT + Part2 - PART2_BARREL1), static_cast<uint8_t>(Level)};
	return IsValidSpec(Spec) ? Spec : CWeaponSpec{};
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
	CWeaponDefinition Definition;
	if(!pProfile || !TryGetDefinition(Spec.m_DefinitionId, &Definition) || Spec.m_Level > WEAPON_SPEC_MAX_LEVEL)
		return false;
	const int ProfileIndex = DenseDefinitionIndex(Definition) * WEAPON_SPEC_LEVEL_COUNT + Spec.m_Level;
	*pProfile = {Definition, Spec, s_aWeaponCombatProfiles[ProfileIndex], s_aWeaponVisualProfiles[ProfileIndex]};
	return true;
}

bool CWeaponCatalog::Validate()
{
	CWeaponSpec InvalidSpec;
	if(IsValidSpec(InvalidSpec) || TryFromProtocol(0, 0, &InvalidSpec) ||
		TryFromProtocol(ToInt(WeaponDefinitionId::ModularLast) + 1, 0, &InvalidSpec) ||
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
		const CWeaponSpec FactorySpec = Definition.m_Kind == EWeaponDefinitionKind::Static ?
			Static(static_cast<StaticWeaponType>(Definition.m_StaticType)) : Modular(Definition.m_Part1, Definition.m_Part2);
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
	if(TryAttackSourceFromProtocol(static_cast<int>(EAttackSourceKind::Droid), WEAPON_DROID_PROFILE_COUNT, 0, 0, &ProtocolSource) ||
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

bool CWeaponCatalog::TryResolveAttack(const CAttackSource &Source, CWeaponCombatProfile *pCombat, CWeaponVisualProfile *pVisual)
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

	const CWeaponCombatProfile *pResolvedCombat = nullptr;
	const CWeaponVisualProfile *pResolvedVisual = nullptr;
	switch(Source.m_Kind)
	{
	case EAttackSourceKind::Droid:
		if(Source.m_Type < 0 || Source.m_Type >= WEAPON_DROID_PROFILE_COUNT)
			return false;
		pResolvedCombat = &s_aDroidCombatProfiles[Source.m_Type];
		pResolvedVisual = &s_aDroidVisualProfiles[Source.m_Type];
		break;
	case EAttackSourceKind::DeathEffect:
		if(Source.m_Type < 0 || Source.m_Type >= WEAPON_DROID_PROFILE_COUNT)
			return false;
		pResolvedCombat = &s_aDroidDeathCombatProfiles[Source.m_Type];
		pResolvedVisual = &s_aDroidDeathVisualProfiles[Source.m_Type];
		break;
	case EAttackSourceKind::Building:
		if(Source.m_Type < 0 || Source.m_Type >= WEAPON_BUILDING_PROFILE_COUNT)
			return false;
		pResolvedCombat = &s_aBuildingCombatProfiles[Source.m_Type];
		pResolvedVisual = &s_aBuildingVisualProfiles[Source.m_Type];
		break;
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
	if(pCombat)
		*pCombat = *pResolvedCombat;
	if(pVisual)
		*pVisual = *pResolvedVisual;
	return true;
}

bool CWeaponCatalog::TryAttackSourceFromProtocol(int Kind, int Type, int DefinitionId, int Level, CAttackSource *pSource)
{
	if(!pSource || Kind < static_cast<int>(EAttackSourceKind::PlayerWeapon) || Kind > static_cast<int>(EAttackSourceKind::DeathEffect))
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
