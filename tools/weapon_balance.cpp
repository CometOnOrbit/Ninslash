#include <game/weapons/weapon_catalog.h>

#include <cstdio>

int main()
{
	char aError[256];
	if(!CWeaponCatalog::Initialize(aError, sizeof(aError)))
	{
		std::fprintf(stderr, "%s\n", aError);
		return 1;
	}
	std::puts("category,stable_id,runtime_id,level,max_level,behavior_flags,firing_type,fire_rate,max_ammo,shot_spread,"
			  "projectile_speed,projectile_life,projectile_damage,projectile_knockback,explosion_size,explosion_damage,"
			  "melee_radius,burst_count,ai_range,cost");
	for(int Index = 0; Index < CWeaponCatalog::DefinitionCount(); ++Index)
	{
		CWeaponDefinition Definition;
		if(!CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition))
			return 2;
		for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		{
			CResolvedWeaponProfile Profile;
			if(!CWeaponCatalog::TryResolve({Definition.m_Id, static_cast<uint8_t>(Level)}, &Profile))
				return 3;
			const CWeaponCombatProfile &C = Profile.m_Combat;
			std::printf("%s,%s,%u,%d,%u,%u,%d,%.6g,%d,%d,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%d,%d,%d\n",
						Definition.m_Kind == EWeaponDefinitionKind::Static ? "static" : "modular",
						Definition.m_aStableId,
						static_cast<unsigned>(Definition.m_Id),
						Level,
						Definition.m_MaxLevel,
						Definition.m_BehaviorFlags,
						C.m_FiringType,
						C.m_FireRate,
						C.m_MaxAmmo,
						C.m_ShotSpread,
						C.m_ProjectileSpeed,
						C.m_ProjectileLife,
						C.m_ProjectileDamage,
						C.m_ProjectileKnockback,
						C.m_ExplosionSize,
						C.m_ExplosionDamage,
						C.m_MeleeHitRadius,
						C.m_BurstCount,
						C.m_AiAttackRange,
						C.m_Cost);
		}
	}
	for(int KindIndex = 0; KindIndex < 3; ++KindIndex)
	{
		const char *pCategory = KindIndex == 0 ? "droid" : KindIndex == 1 ? "droid_death" : "building";
		const int Count = KindIndex < 2 ? WEAPON_DROID_PROFILE_COUNT : WEAPON_BUILDING_PROFILE_COUNT;
		for(int Type = 0; Type < Count; ++Type)
		{
			const CAttackSource Source = KindIndex == 0	  ? CAttackSource::Droid(-1, Type)
										 : KindIndex == 1 ? CAttackSource::Droid(-1, Type, true)
														  : CAttackSource::Building(-1, Type);
			CWeaponCombatProfile C;
			if(!CWeaponCatalog::TryResolveAttack(Source, &C))
				return 4;
			std::printf("%s,%s:%d,%d,0,0,0,%d,%.6g,%d,%d,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%d,%d,%d\n",
						pCategory,
						pCategory,
						Type,
						Type,
						C.m_FiringType,
						C.m_FireRate,
						C.m_MaxAmmo,
						C.m_ShotSpread,
						C.m_ProjectileSpeed,
						C.m_ProjectileLife,
						C.m_ProjectileDamage,
						C.m_ProjectileKnockback,
						C.m_ExplosionSize,
						C.m_ExplosionDamage,
						C.m_MeleeHitRadius,
						C.m_BurstCount,
						C.m_AiAttackRange,
						C.m_Cost);
		}
	}
	return 0;
}
