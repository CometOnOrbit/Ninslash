#include "weapon_catalog.h"

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
		Definition.m_Part1 = Index / 9 + 1;
		Definition.m_Part2 = Index % 9 + 1;
		if(!IsValidPartCombination(Definition.m_Part1, Definition.m_Part2))
			return false;
	}
	else
		return false;

	const CWeaponSpec BaseSpec{Id, 0};
	Definition.m_MaxLevel = WeaponMaxLevel(ToLegacy(BaseSpec));
	if(pDefinition)
		*pDefinition = Definition;
	return true;
}

bool CWeaponCatalog::IsValidSpec(const CWeaponSpec &Spec)
{
	CWeaponDefinition Definition;
	return TryGetDefinition(Spec.m_DefinitionId, &Definition) && Spec.m_Level <= Definition.m_MaxLevel;
}

int CWeaponCatalog::ToLegacy(const CWeaponSpec &Spec)
{
	const int Value = ToInt(Spec.m_DefinitionId);
	int Weapon = 0;
	if(Value >= ToInt(WeaponDefinitionId::StaticFirst) && Value <= ToInt(WeaponDefinitionId::StaticLast))
		Weapon = GetStaticWeapon(static_cast<StaticWeaponType>(Value - ToInt(WeaponDefinitionId::StaticFirst)));
	else if(Value >= ToInt(WeaponDefinitionId::ModularFirst) && Value <= ToInt(WeaponDefinitionId::ModularLast))
	{
		const int Index = Value - ToInt(WeaponDefinitionId::ModularFirst);
		if(IsValidPartCombination(Index / 9 + 1, Index % 9 + 1))
			Weapon = GetModularWeapon(Index / 9 + 1, Index % 9 + 1);
	}
	return Weapon ? GetChargedWeapon(Weapon, Spec.m_Level) : 0;
}

bool CWeaponCatalog::TryFromLegacy(int LegacyWeapon, CWeaponSpec *pSpec)
{
	WeaponDefinitionId Id = WeaponDefinitionId::Invalid;
	if(IsStaticWeapon(LegacyWeapon))
	{
		const int Type = GetStaticType(LegacyWeapon);
		if(Type < 0 || Type >= NUM_STATIC_WEAPONS)
			return false;
		Id = static_cast<WeaponDefinitionId>(ToInt(WeaponDefinitionId::StaticFirst) + Type);
	}
	else if(IsModularWeapon(LegacyWeapon))
	{
		const int Part1 = GetPart(LegacyWeapon, PART_GROUP1);
		const int Part2 = GetPart(LegacyWeapon, PART_GROUP2);
		if(!IsValidPartCombination(Part1, Part2))
			return false;
		Id = static_cast<WeaponDefinitionId>(ToInt(WeaponDefinitionId::ModularFirst) + (Part1 - 1) * 9 + Part2 - 1);
	}
	else
		return false;

	const CWeaponSpec Spec{Id, static_cast<uint8_t>(GetWeaponCharge(LegacyWeapon))};
	if(!IsValidSpec(Spec))
		return false;
	if(pSpec)
		*pSpec = Spec;
	return true;
}

bool CWeaponCatalog::TryResolve(const CWeaponSpec &Spec, CResolvedWeaponProfile *pProfile)
{
	CWeaponDefinition Definition;
	if(!pProfile || !TryGetDefinition(Spec.m_DefinitionId, &Definition) || Spec.m_Level > Definition.m_MaxLevel)
		return false;
	const int W = ToLegacy(Spec);
	*pProfile = {Definition, Spec,
		{GetWeaponFiringType(W), GetWeaponFireRate(W), GetWeaponFullAuto(W), GetWeaponMaxAmmo(W), WeaponUseAmmo(W), GetShotSpread(W), GetProjectileSpread(W), GetProjectileSpeed(W), GetProjectileCurvature(W), GetProjectileLife(W), GetProjectileDamage(W), GetProjectileKnockback(W), GetExplosionSize(W), GetExplosionDamage(W), GetMeleeHitRadius(W), GetWeaponKnockback(W), WeaponBurstCount(W), WeaponBurstReload(W), AIAttackRange(W), ValidForTurret(W)},
		{GetWeaponRenderType(W), GetWeaponVisualSize(W), GetWeaponVisualSize2(W), GetWeaponRenderOffset(W), GetMuzzleRenderOffset(W), GetProjectileOffset(W), GetHandOffset(W), GetWeaponColorswap(W), GetWeaponRenderRecoil(W), GetProjectileSize(W), GetProjectileSprite(W), GetProjectileTraceType(W), GetWeaponTraceThreshold(W), GetExplosionSprite(W), GetExplosionSound(W), GetWeaponFireSound(W), GetWeaponFireSound2(W), GetMuzzleType(W), GetMuzzleAmount(W)}};
	return true;
}

bool CWeaponCatalog::Validate()
{
	for(int Value = ToInt(WeaponDefinitionId::StaticFirst); Value <= ToInt(WeaponDefinitionId::ModularLast); ++Value)
	{
		CWeaponDefinition Definition;
		const auto Id = static_cast<WeaponDefinitionId>(Value);
		if(!TryGetDefinition(Id, &Definition))
			continue;
		for(int Level = 0; Level <= Definition.m_MaxLevel; ++Level)
		{
			const CWeaponSpec Spec{Id, static_cast<uint8_t>(Level)};
			CWeaponSpec RoundTrip;
			CResolvedWeaponProfile Profile;
			if(!TryFromLegacy(ToLegacy(Spec), &RoundTrip) || !(RoundTrip == Spec) || !TryResolve(Spec, &Profile))
				return false;
			if(Profile.m_Combat.m_FiringType == WFT_PROJECTILE && Profile.m_Combat.m_ProjectileLife <= 0.0f)
				return false;
		}
	}
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

CAttackSource CAttackSource::World()
{
	return {};
}
