#include <game/weapons/weapon_catalog.h>
#include <game/weapons/forge.h>
#include <game/weapons/weapon_lua.h>
#include <engine/shared/mod_api.h>
#include <game/localization.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <initializer_list>
#include <stdio.h>
#include <string>

#include "weapon_visual_baseline.inc"

static uint64_t Hash(uint64_t Value, const void *pData, size_t Size)
{
	const unsigned char *pBytes = static_cast<const unsigned char *>(pData);
	for(size_t i = 0; i < Size; ++i)
	{
		Value ^= pBytes[i];
		Value *= 1099511628211ULL;
	}
	return Value;
}

static uint64_t OfficialCombatDigest()
{
	uint64_t Value = 1469598103934665603ULL;
	for(int Index = 0; Index < WEAPON_DEFINITION_COUNT; ++Index)
	{
		CWeaponDefinition Definition;
		assert(CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition));
		Value = Hash(Value, &Definition.m_MaxLevel, sizeof(Definition.m_MaxLevel));
	}
	for(int Index = 0; Index < WEAPON_DEFINITION_COUNT; ++Index)
	{
		CWeaponDefinition Definition;
		assert(CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition));
		for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		{
			CResolvedWeaponProfile Profile;
			const CWeaponSpec Spec{Definition.m_Id, static_cast<uint8_t>(Level)};
			assert(CWeaponCatalog::TryResolve(Spec, &Profile));
			Value = Hash(Value, &Profile.m_Combat, sizeof(Profile.m_Combat));
		}
	}
	for(EAttackSourceKind Kind : {EAttackSourceKind::Droid, EAttackSourceKind::DeathEffect})
	{
		for(int Type = 0; Type < WEAPON_DROID_PROFILE_COUNT; ++Type)
		{
			CWeaponCombatProfile Combat;
			assert(CWeaponCatalog::TryResolveAttack(Kind == EAttackSourceKind::Droid
														? CAttackSource::Droid(-1, Type)
														: CAttackSource::Droid(-1, Type, true),
													&Combat));
			Value = Hash(Value, &Combat, sizeof(Combat));
		}
	}
	for(int Type = 0; Type < WEAPON_BUILDING_PROFILE_COUNT; ++Type)
	{
		CWeaponCombatProfile Combat;
		assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Building(-1, Type), &Combat));
		Value = Hash(Value, &Combat, sizeof(Combat));
	}
	return Value;
}

static uint64_t OfficialVisualDigest()
{
	uint64_t Value = 1469598103934665603ULL;
	for(int Index = 0; Index < WEAPON_DEFINITION_COUNT; ++Index)
	{
		CWeaponDefinition Definition;
		assert(CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition));
		for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		{
			CResolvedWeaponProfile Profile;
			assert(CWeaponCatalog::TryResolve({Definition.m_Id, static_cast<uint8_t>(Level)}, &Profile));
			Value = Hash(Value, &Profile.m_Visual, sizeof(Profile.m_Visual));
		}
	}
	for(EAttackSourceKind Kind : {EAttackSourceKind::Droid, EAttackSourceKind::DeathEffect})
		for(int Type = 0; Type < WEAPON_DROID_PROFILE_COUNT; ++Type)
		{
			CWeaponVisualProfile Visual;
			assert(CWeaponCatalog::TryResolveAttack(Kind == EAttackSourceKind::Droid
														? CAttackSource::Droid(-1, Type)
														: CAttackSource::Droid(-1, Type, true),
													0,
													&Visual));
			Value = Hash(Value, &Visual, sizeof(Visual));
		}
	for(int Type = 0; Type < WEAPON_BUILDING_PROFILE_COUNT; ++Type)
	{
		CWeaponVisualProfile Visual;
		assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Building(-1, Type), 0, &Visual));
		Value = Hash(Value, &Visual, sizeof(Visual));
	}
	return Value;
}

static void ValidateLegacyPlayerVisuals()
{
	auto Values = [](const CWeaponVisualProfile &Visual, float *pValues)
	{
		const float aValues[] = {
			(float)Visual.m_RenderType,
			(float)Visual.m_VisualSize.x,
			(float)Visual.m_VisualSize.y,
			(float)Visual.m_VisualSize2.x,
			(float)Visual.m_VisualSize2.y,
			Visual.m_RenderOffset.x,
			Visual.m_RenderOffset.y,
			Visual.m_MuzzleOffset.x,
			Visual.m_MuzzleOffset.y,
			Visual.m_ProjectileOffset.x,
			Visual.m_ProjectileOffset.y,
			Visual.m_HandOffset.x,
			Visual.m_HandOffset.y,
			Visual.m_ColorSwap.x,
			Visual.m_ColorSwap.y,
			Visual.m_RenderRecoil,
			Visual.m_ProjectileSize,
			Visual.m_ProjectileSprite,
			(float)Visual.m_ProjectileTraceType,
			Visual.m_TraceThreshold,
			(float)Visual.m_ExplosionSprite,
			(float)Visual.m_ExplosionSound,
			(float)Visual.m_FireSound,
			(float)Visual.m_FireSound2,
			(float)Visual.m_MuzzleType,
			(float)Visual.m_MuzzleAmount,
			Visual.m_ScreenshakeAmount,
		};
		mem_copy(pValues, aValues, sizeof(aValues));
	};
	auto Validate = [&](const CWeaponVisualProfile &Visual, const float *pExpected, const char *pName, int Level)
	{
		float aActual[27];
		Values(Visual, aActual);
		for(int Field = 0; Field < 27; ++Field)
		{
			if(fabsf(aActual[Field] - pExpected[Field]) > 0.000001f)
				fprintf(stderr,
						"visual mismatch id=%s level=%d field=%d actual=%.9g expected=%.9g\n",
						pName,
						Level,
						Field,
						aActual[Field],
						pExpected[Field]);
			assert(fabsf(aActual[Field] - pExpected[Field]) <= 0.000001f);
		}
	};
	for(int Index = 0; Index < WEAPON_DEFINITION_COUNT; ++Index)
	{
		CWeaponDefinition Definition;
		assert(CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition));
		for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level)
		{
			CResolvedWeaponProfile Profile;
			assert(CWeaponCatalog::TryResolve({Definition.m_Id, static_cast<uint8_t>(Level)}, &Profile));
			Validate(Profile.m_Visual,
					 gs_aLegacyPlayerVisuals[Index * WEAPON_SPEC_LEVEL_COUNT + Level],
					 Definition.m_aStableId,
					 Level);
		}
	}
	for(int Type = 0; Type < WEAPON_DROID_PROFILE_COUNT; ++Type)
	{
		CWeaponVisualProfile Visual;
		assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Droid(-1, Type), 0, &Visual));
		Validate(Visual, gs_aLegacyDroidVisuals[Type], "droid", Type);
		assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Droid(-1, Type, true), 0, &Visual));
		Validate(Visual, gs_aLegacyDroidDeathVisuals[Type], "droid_death", Type);
	}
	for(int Type = 0; Type < WEAPON_BUILDING_PROFILE_COUNT; ++Type)
	{
		CWeaponVisualProfile Visual;
		assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Building(-1, Type), 0, &Visual));
		Validate(Visual, gs_aLegacyBuildingVisuals[Type], "building", Type);
	}
}

static void ValidateLegacyRangedMechanics()
{
	int Row = 0;
	for(int Part1 = PART1_BASE1; Part1 <= PART1_BASE6; ++Part1)
		for(int Part2 = PART2_BARREL1; Part2 <= PART2_RAIL; ++Part2)
			for(int Level = 0; Level < WEAPON_SPEC_LEVEL_COUNT; ++Level, ++Row)
			{
				CResolvedWeaponProfile Profile;
				assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(Part1, Part2, Level), &Profile));
				const CWeaponCombatProfile &Combat = Profile.m_Combat;
				const float aActual[] = {
					(float)Combat.m_FiringType,
					(float)Combat.m_FullAuto,
					(float)Combat.m_UsesAmmo,
					(float)Combat.m_ShotSpread,
					Combat.m_ProjectileSpread,
					Combat.m_ProjectileCurvature,
					(float)Combat.m_BurstCount,
					Combat.m_BurstReload,
					(float)Combat.m_ValidForTurret,
					Combat.m_ElectroAmount,
					(float)Combat.m_ExplosiveProjectile,
					(float)Combat.m_LaserWeapon,
					(float)Combat.m_Aimline,
					(float)Combat.m_ProjectilePosType,
					(float)Combat.m_LaserRange,
					(float)Combat.m_LaserCharge,
					(float)Combat.m_ProjectileBounces,
				};
				for(int Field = 0; Field < 17; ++Field)
				{
					if(fabsf(aActual[Field] - gs_aLegacyRangedMechanics[Row][Field]) > 0.000001f)
						fprintf(stderr,
								"mechanic mismatch part1=%d part2=%d level=%d field=%d actual=%.9g expected=%.9g\n",
								Part1,
								Part2,
								Level,
								Field,
								aActual[Field],
								gs_aLegacyRangedMechanics[Row][Field]);
					assert(fabsf(aActual[Field] - gs_aLegacyRangedMechanics[Row][Field]) <= 0.000001f);
				}
			}
}

int main()
{
	char aError[256];
	if(!CWeaponCatalog::Initialize(aError, sizeof(aError)))
	{
		fprintf(stderr, "weapon initialization failed: %s\n", aError);
		assert(false);
	}
	assert(CWeaponCatalog::Validate());
	assert(CForge::Validate());
	assert(CWeaponCatalog::DefinitionCount() == WEAPON_DEFINITION_COUNT);
	assert(strcmp(CWeaponCatalog::Part1NameKey(PART1_BASE1), "Ballistic frame") == 0);
	assert(strcmp(CWeaponCatalog::Part1NameKey(PART1_BASE2), "Explosive frame") == 0);
	assert(strcmp(CWeaponCatalog::Part1NameKey(PART1_BASE3), "Arc frame") == 0);
	assert(strcmp(CWeaponCatalog::Part1NameKey(PART1_BASE4), "Marksman frame") == 0);
	assert(strcmp(CWeaponCatalog::Part1NameKey(PART1_BASE5), "Beam frame") == 0);
	assert(strcmp(CWeaponCatalog::Part1NameKey(PART1_BASE6), "Ricochet frame") == 0);
	assert(strcmp(CWeaponCatalog::Part2NameKey(PART2_BARREL3), "Precision barrel") == 0);
	assert(strcmp(CWeaponCatalog::Part2NameKey(PART2_BARREL4), "Rapid-fire barrel") == 0);
	assert(strcmp(CWeaponCatalog::Part2NameKey(PART2_CAPACITOR), "Overcharge capacitor") == 0);
	assert(strcmp(CWeaponCatalog::Part2NameKey(PART2_RAIL), "Piercing barrel") == 0);
	for(int Part1 = PART1_BASE1; Part1 <= PART1_SPIN; ++Part1)
		assert(CWeaponCatalog::Part1NameKey(Part1)[0] != '\0');
	for(int Part2 = PART2_BARREL1; Part2 <= PART2_MELEE6; ++Part2)
		assert(CWeaponCatalog::Part2NameKey(Part2)[0] != '\0');
	assert(strcmp(CWeaponCatalog::Part1NameKey(-1), "Unknown item") == 0);
	assert(strcmp(CWeaponCatalog::Part2NameKey(PART2_END), "Unknown item") == 0);
	assert(strlen(CWeaponCatalog::OfficialContentHash()) == 64);
	CResolvedWeaponProfile ImpactProfile;
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1), &ImpactProfile) &&
		   ImpactProfile.m_Visual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_BALLISTIC);
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL1), &ImpactProfile) &&
		   ImpactProfile.m_Visual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_LAUNCHER);
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE4, PART2_BARREL1), &ImpactProfile) &&
		   ImpactProfile.m_Visual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_GREEN);
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE5, PART2_BARREL1), &ImpactProfile) &&
		   ImpactProfile.m_Visual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_ELECTRIC);
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE6, PART2_BARREL1), &ImpactProfile) &&
		   ImpactProfile.m_Visual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_NONE);
	CResolvedWeaponProfile FlashGrenade, BlindGrenade;
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Static(SW_FLASH_GRENADE), &FlashGrenade) &&
		   FlashGrenade.m_Definition.m_VisionKind == WEAPON_VISION_FLASH &&
		   FlashGrenade.m_Visual.m_StaticSprite == 3);
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Static(SW_BLIND_GRENADE), &BlindGrenade) &&
		   BlindGrenade.m_Definition.m_VisionKind == WEAPON_VISION_BLIND &&
		   BlindGrenade.m_Visual.m_StaticSprite == 3);
	CWeaponVisualProfile AttackVisual;
	assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Droid(-1, DROIDTYPE_WALKER), 0, &AttackVisual) &&
		   AttackVisual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_ELECTRIC);
	assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Droid(-1, DROIDTYPE_STAR), 0, &AttackVisual) &&
		   AttackVisual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_ELECTRIC_AREA);
	CWeaponCombatProfile AttackCombat;
	assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Droid(-1, DROIDTYPE_CRAWLER), &AttackCombat) &&
		   AttackCombat.m_DirectMelee);
	assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Droid(-1, DROIDTYPE_WALKER), &AttackCombat) &&
		   !AttackCombat.m_DirectMelee);
	assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Building(-1, BUILDING_STAND), 0, &AttackVisual) &&
		   AttackVisual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_SPARKS);
	assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Building(-1, BUILDING_GENERATOR), 0, &AttackVisual) &&
		   AttackVisual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_SPRITE_ELECTRIC);
	assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Building(-1, BUILDING_TURRET), 0, &AttackVisual) &&
		   AttackVisual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_SPRITE);
	assert(CWeaponCatalog::TryResolveAttack(CAttackSource::Building(-1, BUILDING_BASE), 0, &AttackVisual) &&
		   AttackVisual.m_ImpactEffect == WEAPON_IMPACT_EFFECT_NONE);
	ValidateLegacyPlayerVisuals();
	ValidateLegacyRangedMechanics();
	const uint64_t CombatDigest = OfficialCombatDigest();
	const uint64_t VisualDigest = OfficialVisualDigest();
	if(CombatDigest != 0x243a5ec52d8a38e6ULL)
		fprintf(stderr, "official combat digest: 0x%016llx\n", (unsigned long long)CombatDigest);
	assert(CombatDigest == 0x243a5ec52d8a38e6ULL);
	if(VisualDigest != 0x0b5b8ff4aa7ebe31ULL)
		fprintf(stderr, "official visual digest: 0x%016llx\n", (unsigned long long)VisualDigest);
	assert(VisualDigest == 0x0b5b8ff4aa7ebe31ULL);
	CResolvedWeaponProfile HeavyStandard, HeavyScatter, HeavyLong, HeavyAutomatic, HeavyCharge;
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL1), &HeavyStandard));
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL2), &HeavyScatter));
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL3), &HeavyLong));
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL4), &HeavyAutomatic));
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE3, PART2_CHARGE), &HeavyCharge));
	assert(!HeavyStandard.m_Combat.m_LaserWeapon && HeavyStandard.m_Combat.m_ElectroAmount == 1.0f);
	assert(HeavyScatter.m_Combat.m_LaserWeapon && HeavyScatter.m_Combat.m_LaserRange == 450 &&
		   HeavyScatter.m_Combat.m_LaserCharge == -1);
	assert(HeavyLong.m_Combat.m_LaserWeapon && HeavyLong.m_Combat.m_LaserRange == 700 &&
		   HeavyLong.m_Combat.m_LaserCharge == 60);
	assert(!HeavyAutomatic.m_Combat.m_LaserWeapon && HeavyAutomatic.m_Combat.m_BurstCount == 3 &&
		   HeavyAutomatic.m_Combat.m_BurstReload == 0.25f);
	assert(!HeavyCharge.m_Combat.m_LaserWeapon && HeavyCharge.m_Combat.m_FiringType == WFT_CHARGE &&
		   HeavyCharge.m_Combat.m_ElectroAmount == 0.0f);
	CResolvedWeaponProfile CapacitorData, RailData, ChargedBladeData, PlainBladeData;
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_CAPACITOR), &CapacitorData));
	assert(CapacitorData.m_Combat.m_ChargeDamageMin == 0.25f && CapacitorData.m_Combat.m_ChargeDamageMax == 1.0f);
	assert(CapacitorData.m_Combat.m_ChargeRangeMin == 0.6f && CapacitorData.m_Combat.m_ChargePenetrationMax == 2 &&
		   CapacitorData.m_Combat.m_ChargeControlsLaser);
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_RAIL), &RailData) &&
		   RailData.m_Combat.m_ProjectilePenetration == WEAPON_INFINITE_PENETRATION);
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE6), &ChargedBladeData));
	assert(ChargedBladeData.m_Combat.m_ChargePowerMin == 0.5f && ChargedBladeData.m_Combat.m_ChargePowerMax == 2.0f);
	assert(WeaponHasBehavior(ChargedBladeData.m_Definition, WEAPON_BEHAVIOR_CHARGED_BLADE));
	assert(CWeaponCatalog::TryResolve(CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE1), &PlainBladeData));
	assert(PlainBladeData.m_Combat.m_ChargePowerMin == 1.0f && PlainBladeData.m_Combat.m_ChargePowerMax == 1.0f);
	assert(!WeaponHasBehavior(PlainBladeData.m_Definition, WEAPON_BEHAVIOR_CHARGED_BLADE));

	CWeaponSpec Gun;
	assert(CWeaponCatalog::TryFromStableId("official:static:gun1", 0, &Gun));
	assert(Gun == CWeaponCatalog::Static(SW_GUN1));
	assert(strcmp(CWeaponCatalog::StableId(Gun), "official:static:gun1") == 0);
	assert(!CWeaponCatalog::IsCustom(Gun));
	g_Localization.AddString("Plasma Carbine", "Carabine plasma");
	assert(strcmp(Localize("Plasma Carbine"), "Carabine plasma") == 0);
	g_Localization.AddString("Plasma Carbine", "Plasma-Karabiner");
	assert(strcmp(Localize("Plasma Carbine"), "Plasma-Karabiner") == 0);

	const char *pDefinition = "weapon.define {"
							  " id='plasma-carbine', schema=4, inherits='official:static:gun1', max_level=4,"
							  " combat={projectile_damage=weapon.curve.linear(20, 20, 4), max_ammo=24, cost=99, "
							  "projectile_pos_type=weapon.path.rocket},"
							  " visuals={projectile_size=1.5, render_offset={12, -3}}, localization={name='Plasma "
							  "Carbine', description='A test weapon.'}"
							  "}";
	assert(CWeaponCatalog::LoadLuaDefinitions("12345", pDefinition, (int)strlen(pDefinition), aError, sizeof(aError)));
	assert(CWeaponCatalog::DefinitionCount() == WEAPON_DEFINITION_COUNT + 1);
	CWeaponSpec Plasma;
	assert(CWeaponCatalog::TryFromStableId("workshop:12345:plasma-carbine", 4, &Plasma));
	assert((int)Plasma.m_DefinitionId == WEAPON_DEFINITION_COUNT + 1);
	assert(CWeaponCatalog::IsCustom(Plasma));
	CResolvedWeaponProfile Profile;
	assert(CWeaponCatalog::TryResolve(Plasma, &Profile));
	assert(Profile.m_Combat.m_ProjectileDamage == 40.0f);
	assert(Profile.m_Combat.m_MaxAmmo == 24);
	assert(Profile.m_Combat.m_Cost == 99);
	assert(Profile.m_Combat.m_ProjectilePosType == WEAPON_PROJECTILE_PATH_ROCKET);
	assert(Profile.m_Visual.m_ProjectileSize == 1.5f);
	assert(Profile.m_Visual.m_RenderOffset.x == 12.0f && Profile.m_Visual.m_RenderOffset.y == -3.0f);
	assert(Profile.m_Definition.m_Kind == EWeaponDefinitionKind::Static &&
		   Profile.m_Definition.m_StaticType == SW_GUN1);
	assert(WeaponHasBehavior(Profile.m_Definition, WEAPON_BEHAVIOR_COMPACT_GUN_HANDS));

	const char *pInvalid = "weapon.define {id='bad', inherits='missing:weapon'}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pInvalid, (int)strlen(pInvalid), aError, sizeof(aError)));
	assert(CWeaponCatalog::DefinitionCount() == WEAPON_DEFINITION_COUNT + 1);
	const char *pTemplateConflict = "weapon.define {id='bad-template', schema=4, inherits='official:static:gun1', "
									"combat_template=weapon.combat.projectile}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pTemplateConflict, (int)strlen(pTemplateConflict), aError, sizeof(aError)));
	const char *pBadVector =
		"weapon.define {id='bad-vector', schema=4, inherits='official:static:gun1', visuals={visual_size={1,2,3}}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pBadVector, (int)strlen(pBadVector), aError, sizeof(aError)));
	const char *pFractionalInteger = "weapon.define {id='bad-int', schema=4, inherits='official:static:gun1', "
									 "combat={max_ammo=1.5}, localization={name='Bad'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pFractionalInteger, (int)strlen(pFractionalInteger), aError, sizeof(aError)));
	const char *pNumericBoolean = "weapon.define {id='bad-bool', schema=4, inherits='official:static:gun1', "
								  "combat={full_auto=1}, localization={name='Bad'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pNumericBoolean, (int)strlen(pNumericBoolean), aError, sizeof(aError)));
	const char *pNumericString = "weapon.define {id='bad-number-string', schema=4, inherits='official:static:gun1', "
								 "combat={max_ammo='12'}, localization={name='Bad'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pNumericString, (int)strlen(pNumericString), aError, sizeof(aError)));
	const char *pNan = "weapon.define {id='bad-nan', schema=4, inherits='official:static:gun1', "
					   "combat={projectile_damage=0/0}, localization={name='Bad'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pNan, (int)strlen(pNan), aError, sizeof(aError)));
	const char *pHugeInteger = "weapon.define {id='bad-huge', schema=4, inherits='official:static:gun1', "
							   "combat={max_ammo=9223372036854775807}, localization={name='Bad'}}";
	assert(
		!CWeaponCatalog::LoadLuaDefinitions("12345", pHugeInteger, (int)strlen(pHugeInteger), aError, sizeof(aError)));
	const char *pVisualOverride = "weapon.override_visuals('official:static:gun1', {projectile_size=99})";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pVisualOverride, (int)strlen(pVisualOverride), aError, sizeof(aError)));
	const char *pCombatOverride = "weapon.override_combat('official:static:gun1', {laser_weapon=true})";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pCombatOverride, (int)strlen(pCombatOverride), aError, sizeof(aError)));
	const char *pComponent =
		"weapon.component {slot=weapon.component_slot.frame, id=weapon.part1.base1, name='Replacement'}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pComponent, (int)strlen(pComponent), aError, sizeof(aError)));
	const char *pUnsafe = "os.execute('echo unsafe')";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pUnsafe, (int)strlen(pUnsafe), aError, sizeof(aError)));
	const char *pRandom = "math.random()";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pRandom, (int)strlen(pRandom), aError, sizeof(aError)));
	const char *pUnordered = "for key,value in pairs({a=1}) do end";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pUnordered, (int)strlen(pUnordered), aError, sizeof(aError)));
	const char *pLoop = "while true do end";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pLoop, (int)strlen(pLoop), aError, sizeof(aError)));
	const char *pCaughtBudget = "while true do pcall(function() while true do end end) end";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pCaughtBudget, (int)strlen(pCaughtBudget), aError, sizeof(aError)));
	const char *pBadCurve = "weapon.levels {1, 2}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pBadCurve, (int)strlen(pBadCurve), aError, sizeof(aError)));
	const char *pLevelsExtraKey = "weapon.levels "
								  "{[1]=1,[2]=2,[3]=3,[4]=4,[5]=5,[6]=6,[7]=7,[8]=8,[9]=9,[10]=10,[11]=11,[12]=12,[13]="
								  "13,[14]=14,[15]=15,[16]=16,extra=17}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pLevelsExtraKey, (int)strlen(pLevelsExtraKey), aError, sizeof(aError)));
	const char *pOverride = "weapon.define {id='override', schema=4, inherits='official:static:gun1', kind='static', "
							"localization={name='No'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pOverride, (int)strlen(pOverride), aError, sizeof(aError)));
	const char *pOldSchema =
		"weapon.define {id='old', schema=3, inherits='official:static:gun1', localization={name='Old'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pOldSchema, (int)strlen(pOldSchema), aError, sizeof(aError)));
	const char *pNumericId =
		"weapon.define {id=123, schema=4, inherits='official:static:gun1', localization={name='Bad'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pNumericId, (int)strlen(pNumericId), aError, sizeof(aError)));
	const char *pNumericName =
		"weapon.define {id='bad-name', schema=4, inherits='official:static:gun1', localization={name=123}}";
	assert(
		!CWeaponCatalog::LoadLuaDefinitions("12345", pNumericName, (int)strlen(pNumericName), aError, sizeof(aError)));
	const char *pNulName = "weapon.define {id='nul-name', schema=4, inherits='official:static:gun1', "
						   "localization={name='A' .. string.char(0) .. 'B'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pNulName, (int)strlen(pNulName), aError, sizeof(aError)));
	const char *pControlName = "weapon.define {id='control-name', schema=4, inherits='official:static:gun1', "
							   "localization={name='A' .. string.char(10) .. 'B'}}";
	assert(
		!CWeaponCatalog::LoadLuaDefinitions("12345", pControlName, (int)strlen(pControlName), aError, sizeof(aError)));
	const char *pLegacyId = "weapon.define {id='legacy', schema=4, legacy_id=999, inherits='official:static:gun1', "
							"localization={name='Legacy'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pLegacyId, (int)strlen(pLegacyId), aError, sizeof(aError)));
	const char *pMutateApi = "weapon.kind.static = 'modular'";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pMutateApi, (int)strlen(pMutateApi), aError, sizeof(aError)));
	const char *pHugeCurve = "weapon.curve.integer_linear(1e300, 1, 4)";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pHugeCurve, (int)strlen(pHugeCurve), aError, sizeof(aError)));
	const char *pHugeProfile =
		"weapon.define "
		"{id='huge',schema=4,inherits='official:static:gun1',combat={fire_rate=1e20},localization={name='Huge'}}";
	assert(
		!CWeaponCatalog::LoadLuaDefinitions("12345", pHugeProfile, (int)strlen(pHugeProfile), aError, sizeof(aError)));
	const char *pPlatformBytecode = "weapon.define "
									"{id='platform-bytecode',schema=4,inherits='official:static:gun1',combat={max_ammo="
									"#string.dump(function() end)},localization={name='Platform Bytecode'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pPlatformBytecode, (int)strlen(pPlatformBytecode), aError, sizeof(aError)));
	const char *pNulTopField = "weapon.define "
							   "{id='nul-field',schema=4,inherits='official:static:gun1',['combat'..string.char(0)..'"
							   "ignored']={},localization={name='Nul Field'}}";
	assert(
		!CWeaponCatalog::LoadLuaDefinitions("12345", pNulTopField, (int)strlen(pNulTopField), aError, sizeof(aError)));
	const char *pNulAsset = "weapon.define "
							"{id='nul-asset',schema=4,inherits='official:static:gun1',assets={held_image='resources/"
							"good.png'..string.char(0)..'ignored'},localization={name='Nul Asset'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions("12345", pNulAsset, (int)strlen(pNulAsset), aError, sizeof(aError)));

	CWeaponCatalog::ResetCustomDefinitions();
	assert(CWeaponCatalog::DefinitionCount() == WEAPON_DEFINITION_COUNT);
	assert(!CWeaponCatalog::TryFromStableId("workshop:12345:plasma-carbine", 0, &Plasma));
	assert(CWeaponCatalog::LoadLuaDefinitions("12345", pDefinition, (int)strlen(pDefinition), aError, sizeof(aError)));
	assert(CWeaponCatalog::TryFromStableId("workshop:12345:plasma-carbine", 0, &Plasma));
	assert((int)Plasma.m_DefinitionId == WEAPON_DEFINITION_COUNT + 1);
	const char *pDependencyWeapon = "weapon.define {id='plasma-burst', schema=4, "
									"inherits='workshop:12345:plasma-carbine', localization={name='Plasma Burst'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"67890", pDependencyWeapon, (int)strlen(pDependencyWeapon), aError, sizeof(aError)));
	const char *apWeaponDependencies[] = {"12345"};
	assert(CWeaponCatalog::LoadLuaDefinitions("67890",
											  MOD_CAPABILITY_WEAPONS,
											  apWeaponDependencies,
											  1,
											  pDependencyWeapon,
											  (int)strlen(pDependencyWeapon),
											  aError,
											  sizeof(aError)));
	CWeaponSpec Burst;
	assert(CWeaponCatalog::TryFromStableId("workshop:67890:plasma-burst", 0, &Burst));
	assert((int)Burst.m_DefinitionId == WEAPON_DEFINITION_COUNT + 2);
	CWeaponCatalog::ResetCustomDefinitions();
	assert(CWeaponCatalog::LoadLuaDefinitions("12345", pDefinition, (int)strlen(pDefinition), aError, sizeof(aError)));
	assert(CWeaponCatalog::LoadLuaDefinitions("67890",
											  MOD_CAPABILITY_WEAPONS,
											  apWeaponDependencies,
											  1,
											  pDependencyWeapon,
											  (int)strlen(pDependencyWeapon),
											  aError,
											  sizeof(aError)));
	assert(CWeaponCatalog::TryFromStableId("workshop:67890:plasma-burst", 0, &Burst) &&
		   (int)Burst.m_DefinitionId == WEAPON_DEFINITION_COUNT + 2);
	CWeaponCatalog::ResetCustomDefinitions();
	const char *pStandalone = "weapon.define {id='standalone', schema=4, kind=weapon.kind.static, "
							  "static_type=weapon.static.gun1, max_level=0,"
							  " behavior={'impact_spark'}, combat_template=weapon.combat.projectile, "
							  "visual_template=weapon.visual.compact_weapon,"
							  " combat={projectile_life=0.5, projectile_damage=7}, visuals={visual_size={4,2}}, "
							  "localization={name='Standalone'}}";
	assert(CWeaponCatalog::LoadLuaDefinitions("12345", pStandalone, (int)strlen(pStandalone), aError, sizeof(aError)));
	CWeaponSpec Standalone;
	assert(CWeaponCatalog::TryFromStableId("workshop:12345:standalone", 0, &Standalone));
	assert(CWeaponCatalog::TryResolve(Standalone, &Profile));
	assert(Profile.m_Combat.m_FiringType == WFT_PROJECTILE && Profile.m_Combat.m_ProjectileDamage == 7.0f);
	assert(WeaponHasBehavior(Profile.m_Definition, WEAPON_BEHAVIOR_IMPACT_SPARK));
	CWeaponCatalog::ResetCustomDefinitions();
	const char *pBehaviorExtraKey =
		"weapon.define {id='bad-behavior', schema=4, kind=weapon.kind.static, static_type=weapon.static.gun1, "
		"max_level=0,"
		" behavior={[1]='impact_spark',extra='rail'}, combat_template=weapon.combat.projectile, "
		"visual_template=weapon.visual.compact_weapon,"
		" combat={projectile_life=0.5}, visuals={visual_size={4,2}}, localization={name='Bad Behavior'}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pBehaviorExtraKey, (int)strlen(pBehaviorExtraKey), aError, sizeof(aError)));
	const char *pMissingStandaloneName =
		"weapon.define {id='missing-name', schema=4, kind=weapon.kind.static, static_type=weapon.static.gun1, "
		"max_level=0,"
		" behavior={}, combat_template=weapon.combat.projectile, visual_template=weapon.visual.compact_weapon,"
		" combat={projectile_life=0.5}, visuals={visual_size={4,2}}}";
	assert(!CWeaponCatalog::LoadLuaDefinitions(
		"12345", pMissingStandaloneName, (int)strlen(pMissingStandaloneName), aError, sizeof(aError)));
	const char *pNulBehavior =
		"weapon.define {id='nul-behavior', schema=4, kind=weapon.kind.static, static_type=weapon.static.gun1, "
		"max_level=0,"
		" behavior={'impact_spark'..string.char(0)..'ignored'}, combat_template=weapon.combat.projectile, "
		"visual_template=weapon.visual.compact_weapon,"
		" combat={projectile_life=0.5}, visuals={visual_size={4,2}}, localization={name='Nul Behavior'}}";
	assert(
		!CWeaponCatalog::LoadLuaDefinitions("12345", pNulBehavior, (int)strlen(pNulBehavior), aError, sizeof(aError)));

	const char *pModulesAndForge =
		"weapon.module "
		"{schema=1,id='plasma-frame',slot=weapon.module_slot.frame,tags={'ranged','energy'},localization={name='Plasma "
		"Frame'}}"
		"weapon.compose "
		"{schema=1,id='plasma-combination',frames={'plasma-frame'},parts={'official:module:part:barrel1'},build="
		"function(ctx) "
		" return "
		"{max_level=6,behavior={},combat_template=weapon.combat.projectile,visual_template=weapon.visual.compact_"
		"weapon,"
		" combat={max_ammo=20,uses_ammo=true,projectile_life=0.5,projectile_damage=17},visuals={visual_size={4,2}},"
		"localization={name='Plasma Combination'}} end}"
		"forge.recipe "
		"{schema=1,id='plasma-refit',priority=100,targets={'tag:ranged'},materials={'plasma-combination'},localization="
		"{name='Plasma Refit'},"
		" resolve=function(ctx) if ctx.target.level < 2 then return nil end return "
		"{product=ctx.material.stable_id,level=ctx.target.level,cost=ctx.base_cost+ctx.level_cost*2} end}";
	assert(CWeaponCatalog::LoadLuaDefinitions(
		"12345", pModulesAndForge, (int)strlen(pModulesAndForge), aError, sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	CWeaponSpec Combination;
	CWeaponDefinition CombinationDefinition{};
	for(int Index = WEAPON_DEFINITION_COUNT; Index < CWeaponCatalog::DefinitionCount(); ++Index)
	{
		CWeaponDefinition Definition;
		if(CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition) &&
		   strcmp(Definition.m_aComposeId, "plasma-combination") == 0)
		{
			Combination = {Definition.m_Id, 0};
			CombinationDefinition = Definition;
			break;
		}
	}
	assert(Combination.IsValid());
	assert(strstr(CombinationDefinition.m_aStableId, "workshop:12345:compose:plasma-combination:") ==
		   CombinationDefinition.m_aStableId);
	assert(CombinationDefinition.m_FrameModule != 0 && CombinationDefinition.m_PartModule != 0);
	const CWeaponSpec ForgeTarget = CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 2);
	const CForgeRecipe ModRecipe = CForge::Resolve(ForgeTarget, Combination, 5, 10, 3, 7);
	assert(ModRecipe.m_Result == FORGERESULT_SUCCESS && ModRecipe.m_Operation == 3);
	assert(ModRecipe.m_Product.m_DefinitionId == Combination.m_DefinitionId && ModRecipe.m_Product.m_Level == 2);
	assert(ModRecipe.m_Cost == 16 && strcmp(ModRecipe.m_aRecipeName, "Plasma Refit") == 0);
	char aCombinationStableId[sizeof(CombinationDefinition.m_aStableId)];
	str_copy(aCombinationStableId, CombinationDefinition.m_aStableId, sizeof(aCombinationStableId));
	const WeaponDefinitionId CombinationId = Combination.m_DefinitionId;
	CWeaponCatalog::ResetCustomDefinitions();
	assert(CWeaponCatalog::LoadLuaDefinitions(
		"12345", pModulesAndForge, (int)strlen(pModulesAndForge), aError, sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	CWeaponSpec ReloadedCombination;
	assert(CWeaponCatalog::TryFromStableId(aCombinationStableId, 0, &ReloadedCombination));
	assert(ReloadedCombination.m_DefinitionId == CombinationId);
	assert(CWeaponCatalog::TryResolve(ReloadedCombination, &Profile) && Profile.m_Combat.m_ProjectileDamage == 17.0f);
	CWeaponCatalog::ResetCustomDefinitions();
	const char *pModuleOnlyCompose =
		"weapon.module {schema=1,id='module-only-frame',slot=weapon.module_slot.frame,localization={name='Module Only "
		"Frame'}}"
		"weapon.compose "
		"{schema=1,id='module-only-combination',frames={'module-only-frame'},parts={'official:module:part:barrel1'},"
		"build=function(ctx) "
		"return "
		"{max_level=0,behavior={},combat_template=weapon.combat.projectile,visual_template=weapon.visual.compact_"
		"weapon,combat={projectile_life=1},visuals={visual_size={4,2}},localization={name='Module Only Combination'}} "
		"end}";
	assert(WeaponLuaLoadPackage("12346",
								MOD_CAPABILITY_WEAPON_MODULES,
								0,
								0,
								pModuleOnlyCompose,
								(int)strlen(pModuleOnlyCompose),
								aError,
								sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pDependencyModule =
		"weapon.module {schema=1,id='dependency-frame',slot=weapon.module_slot.frame,localization={name='Dependency "
		"Frame'}}";
	assert(!WeaponLuaLoadPackage("111",
								 MOD_CAPABILITY_WEAPONS,
								 0,
								 0,
								 pDependencyModule,
								 (int)strlen(pDependencyModule),
								 aError,
								 sizeof(aError)));
	assert(WeaponLuaLoadPackage("111",
								MOD_CAPABILITY_WEAPON_MODULES,
								0,
								0,
								pDependencyModule,
								(int)strlen(pDependencyModule),
								aError,
								sizeof(aError)));
	const char *pCrossPackageCompose = "weapon.compose "
									   "{schema=1,id='dependency-combination',frames={'workshop:111:module:dependency-"
									   "frame'},parts={'official:module:part:barrel1'},"
									   "build=function(ctx) return "
									   "{max_level=0,behavior={},combat_template=weapon.combat.projectile,visual_"
									   "template=weapon.visual.compact_weapon,"
									   "combat={projectile_life=0.5,projectile_damage=5},visuals={visual_size={4,2}},"
									   "localization={name='Dependency Combination'}} end}";
	assert(!WeaponLuaLoadPackage("222",
								 MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_WEAPON_MODULES,
								 0,
								 0,
								 pCrossPackageCompose,
								 (int)strlen(pCrossPackageCompose),
								 aError,
								 sizeof(aError)));
	const char *apDependencies[] = {"111"};
	assert(WeaponLuaLoadPackage("222",
								MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_WEAPON_MODULES,
								apDependencies,
								1,
								pCrossPackageCompose,
								(int)strlen(pCrossPackageCompose),
								aError,
								sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pComposeConflict =
		"weapon.module {schema=1,id='frame',slot=weapon.module_slot.frame,localization={name='Frame'}}"
		"local function build(ctx) return "
		"{max_level=0,behavior={},combat_template=weapon.combat.projectile,visual_template=weapon.visual.compact_"
		"weapon,combat={projectile_life=1},visuals={visual_size={4,2}},localization={name='Built'}} end "
		"weapon.compose {schema=1,id='first',frames={'frame'},parts={'official:module:part:barrel1'},build=build}"
		"weapon.compose {schema=1,id='second',frames={'frame'},parts={'official:module:part:barrel1'},build=build}";
	assert(CWeaponCatalog::LoadLuaDefinitions(
		"333", pComposeConflict, (int)strlen(pComposeConflict), aError, sizeof(aError)));
	assert(!CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pRecipeConflict =
		"local function resolve(ctx) return {product=ctx.target.stable_id,level=ctx.target.level,cost=0} end "
		"forge.recipe "
		"{schema=1,id='first',priority=50,targets={'tag:ranged'},materials={'tag:ranged'},localization={name='First'},"
		"resolve=resolve}"
		"forge.recipe "
		"{schema=1,id='second',priority=50,targets={'tag:ranged'},materials={'tag:ranged'},localization={name='Second'}"
		",resolve=resolve}";
	assert(WeaponLuaLoadPackage("444",
								MOD_CAPABILITY_FORGE_RECIPES,
								0,
								0,
								pRecipeConflict,
								(int)strlen(pRecipeConflict),
								aError,
								sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	const CForgeRecipe ConflictRecipe = CForge::Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1),
														CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2),
														5,
														10,
														3,
														5);
	assert(ConflictRecipe.m_Result == FORGERESULT_INVALID_RECIPE);
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pNilRecipe = "forge.recipe "
							 "{schema=1,id='no-match',priority=100,targets={'tag:ranged'},materials={'tag:ranged'},"
							 "localization={name='No Match'},resolve=function(ctx) return nil end}";
	assert(WeaponLuaLoadPackage(
		"555", MOD_CAPABILITY_FORGE_RECIPES, 0, 0, pNilRecipe, (int)strlen(pNilRecipe), aError, sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	const CForgeRecipe FallbackRecipe = CForge::Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1),
														CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2),
														5,
														10,
														3,
														5);
	assert(FallbackRecipe.m_Result == FORGERESULT_SUCCESS && FallbackRecipe.m_Operation == FORGEOP_REPLACE_PART2);
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pCapturedState = "local counter=0 forge.recipe "
								 "{schema=1,id='stateful',priority=1,targets={'tag:ranged'},materials={'tag:ranged'},"
								 "localization={name='Stateful'},"
								 "resolve=function(ctx) counter=counter+1 return nil end}";
	assert(!WeaponLuaLoadPackage("666",
								 MOD_CAPABILITY_FORGE_RECIPES,
								 0,
								 0,
								 pCapturedState,
								 (int)strlen(pCapturedState),
								 aError,
								 sizeof(aError)));
	assert(!WeaponLuaLoadPackage(
		"0", MOD_CAPABILITY_FORGE_RECIPES, 0, 0, pNilRecipe, (int)strlen(pNilRecipe), aError, sizeof(aError)));
	assert(!WeaponLuaLoadPackage(
		"666", MOD_CAPABILITY_FORGE_RECIPES, 0, 1, pNilRecipe, (int)strlen(pNilRecipe), aError, sizeof(aError)));
	const char *pNulModuleTag = "weapon.module "
								"{schema=1,id='nul-tag',slot=weapon.module_slot.frame,tags={'ranged'..string.char(0)..'"
								"ignored'},localization={name='Nul Tag'}}";
	assert(!WeaponLuaLoadPackage(
		"666", MOD_CAPABILITY_WEAPON_MODULES, 0, 0, pNulModuleTag, (int)strlen(pNulModuleTag), aError, sizeof(aError)));
	const char *pModuleLocalizationExtra = "weapon.module "
										   "{schema=1,id='extra-localization',slot=weapon.module_slot.frame,"
										   "localization={name='Extra',description='ignored'}}";
	assert(!WeaponLuaLoadPackage("666",
								 MOD_CAPABILITY_WEAPON_MODULES,
								 0,
								 0,
								 pModuleLocalizationExtra,
								 (int)strlen(pModuleLocalizationExtra),
								 aError,
								 sizeof(aError)));
	const char *pRecipeLocalizationExtra =
		"forge.recipe "
		"{schema=1,id='extra-localization',priority=1,targets={'tag:ranged'},materials={'tag:ranged'},localization={"
		"name='Extra',description='ignored'},resolve=function(ctx) return nil end}";
	assert(!WeaponLuaLoadPackage("666",
								 MOD_CAPABILITY_FORGE_RECIPES,
								 0,
								 0,
								 pRecipeLocalizationExtra,
								 (int)strlen(pRecipeLocalizationExtra),
								 aError,
								 sizeof(aError)));

	const char *pSparseSelectors = "weapon.module "
								   "{schema=1,id='rollback-frame',slot=weapon.module_slot.frame,tags={'rollback-tag'},"
								   "localization={name='Rollback Frame'}}"
								   "weapon.compose "
								   "{schema=1,id='sparse',frames={[1]='rollback-frame',extra='rollback-frame'},parts={'"
								   "official:module:part:barrel1'},build=function(ctx) return {} end}";
	assert(!WeaponLuaLoadPackage("666",
								 MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_WEAPON_MODULES,
								 0,
								 0,
								 pSparseSelectors,
								 (int)strlen(pSparseSelectors),
								 aError,
								 sizeof(aError)));
	const char *pDuplicateSelectors = "weapon.compose "
									  "{schema=1,id='duplicates',frames={'rollback-frame','rollback-frame'},parts={'"
									  "official:module:part:barrel1'},build=function(ctx) return {} end}";
	assert(!WeaponLuaLoadPackage("666",
								 MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_WEAPON_MODULES,
								 0,
								 0,
								 pDuplicateSelectors,
								 (int)strlen(pDuplicateSelectors),
								 aError,
								 sizeof(aError)));
	const char *pRollbackModule = "weapon.module "
								  "{schema=1,id='rollback-frame',slot=weapon.module_slot.frame,tags={'rollback-tag'},"
								  "localization={name='Rollback Frame'}}";
	assert(WeaponLuaLoadPackage("666",
								MOD_CAPABILITY_WEAPON_MODULES,
								0,
								0,
								pRollbackModule,
								(int)strlen(pRollbackModule),
								aError,
								sizeof(aError)));
	const char *pUnknownSelector = "weapon.compose "
								   "{schema=1,id='unknown',frames={'rollback-frame'},parts={'tag:no-such-part'},build="
								   "function(ctx) return {} end}";
	assert(WeaponLuaLoadPackage("666",
								MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_WEAPON_MODULES,
								0,
								0,
								pUnknownSelector,
								(int)strlen(pUnknownSelector),
								aError,
								sizeof(aError)));
	assert(!CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pPriorityIsolation =
		"forge.recipe "
		"{schema=1,id='high',priority=100,targets={'tag:ranged'},materials={'tag:ranged'},localization={name='High'},"
		"resolve=function(ctx) return {product=ctx.material.stable_id,level=ctx.target.level,cost=7} end}"
		"forge.recipe "
		"{schema=1,id='low',priority=1,targets={'tag:ranged'},materials={'tag:ranged'},localization={name='Low'},"
		"resolve=function(ctx) error('must not run') end}";
	assert(WeaponLuaLoadPackage("777",
								MOD_CAPABILITY_FORGE_RECIPES,
								0,
								0,
								pPriorityIsolation,
								(int)strlen(pPriorityIsolation),
								aError,
								sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	const CForgeRecipe IsolatedRecipe = CForge::Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 2),
														CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2),
														5,
														10,
														3,
														5);
	assert(IsolatedRecipe.m_Result == FORGERESULT_SUCCESS && IsolatedRecipe.m_Cost == 7);
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pImmutableContext = "forge.recipe "
									"{schema=1,id='immutable',priority=1,targets={'tag:ranged'},materials={'tag:ranged'"
									"},localization={name='Immutable'},"
									"resolve=function(ctx) ctx.target.level=9 return nil end}";
	assert(WeaponLuaLoadPackage("888",
								MOD_CAPABILITY_FORGE_RECIPES,
								0,
								0,
								pImmutableContext,
								(int)strlen(pImmutableContext),
								aError,
								sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	assert(CForge::Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1),
						   CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2),
						   5,
						   10,
						   3,
						   5)
			   .m_Result == FORGERESULT_INVALID_RECIPE);
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pUnknownRecipeSelector =
		"forge.recipe "
		"{schema=1,id='unknown-selector',priority=1,targets={'tag:no-such-weapon-tag'},materials={'tag:ranged'},"
		"localization={name='Unknown'},resolve=function(ctx) return nil end}";
	assert(WeaponLuaLoadPackage("889",
								MOD_CAPABILITY_FORGE_RECIPES,
								0,
								0,
								pUnknownRecipeSelector,
								(int)strlen(pUnknownRecipeSelector),
								aError,
								sizeof(aError)));
	assert(!CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	CWeaponCatalog::ResetCustomDefinitions();

	const int OfficialModuleCount = NUM_PART1 + PART2_END - 1;
	std::string ModuleCapacitySource;
	for(int i = 0; i < 256 - OfficialModuleCount; ++i)
	{
		char aLine[256];
		str_format(
			aLine,
			sizeof(aLine),
			"weapon.module {schema=1,id='frame-%d',slot=weapon.module_slot.frame,localization={name='Frame %d'}}",
			i,
			i);
		ModuleCapacitySource += aLine;
	}
	assert(WeaponLuaLoadPackage("901",
								MOD_CAPABILITY_WEAPON_MODULES,
								0,
								0,
								ModuleCapacitySource.c_str(),
								(int)ModuleCapacitySource.size(),
								aError,
								sizeof(aError)));
	const char *pModuleOverflow =
		"weapon.module {schema=1,id='overflow',slot=weapon.module_slot.frame,localization={name='Overflow'}}";
	assert(!WeaponLuaLoadPackage("901",
								 MOD_CAPABILITY_WEAPON_MODULES,
								 0,
								 0,
								 pModuleOverflow,
								 (int)strlen(pModuleOverflow),
								 aError,
								 sizeof(aError)));
	CWeaponCatalog::ResetCustomDefinitions();

	std::string RecipeCapacitySource;
	for(int i = 0; i < 256; ++i)
	{
		char aLine[512];
		str_format(aLine,
				   sizeof(aLine),
				   "forge.recipe "
				   "{schema=1,id='recipe-%d',priority=%d,targets={'tag:ranged'},materials={'tag:ranged'},localization={"
				   "name='Recipe %d'},resolve=function(ctx) return nil end}",
				   i,
				   i,
				   i);
		RecipeCapacitySource += aLine;
	}
	assert(WeaponLuaLoadPackage("902",
								MOD_CAPABILITY_FORGE_RECIPES,
								0,
								0,
								RecipeCapacitySource.c_str(),
								(int)RecipeCapacitySource.size(),
								aError,
								sizeof(aError)));
	const char *pRecipeOverflow = "forge.recipe "
								  "{schema=1,id='overflow',priority=0,targets={'tag:ranged'},materials={'tag:ranged'},"
								  "localization={name='Overflow'},resolve=function(ctx) return nil end}";
	assert(!WeaponLuaLoadPackage("902",
								 MOD_CAPABILITY_FORGE_RECIPES,
								 0,
								 0,
								 pRecipeOverflow,
								 (int)strlen(pRecipeOverflow),
								 aError,
								 sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	CWeaponCatalog::ResetCustomDefinitions();

	std::string TagCapacitySource;
	for(int Module = 0; Module < 2; ++Module)
	{
		char aPrefix[192];
		str_format(aPrefix,
				   sizeof(aPrefix),
				   "weapon.module {schema=1,id='tag-frame-%d',slot=weapon.module_slot.frame,tags={",
				   Module);
		TagCapacitySource += aPrefix;
		const int Begin = Module == 0 ? 0 : 32;
		const int End = Module == 0 ? 32 : 59;
		for(int i = Begin; i < End; ++i)
		{
			if(i != Begin)
				TagCapacitySource += ',';
			char aTag[32];
			str_format(aTag, sizeof(aTag), "'capacity-tag-%d'", i);
			TagCapacitySource += aTag;
		}
		char aSuffix[128];
		str_format(aSuffix, sizeof(aSuffix), "},localization={name='Tag Frame %d'}}", Module);
		TagCapacitySource += aSuffix;
	}
	assert(WeaponLuaLoadPackage("903",
								MOD_CAPABILITY_WEAPON_MODULES,
								0,
								0,
								TagCapacitySource.c_str(),
								(int)TagCapacitySource.size(),
								aError,
								sizeof(aError)));
	const char *pTagOverflow = "weapon.module "
							   "{schema=1,id='tag-overflow',slot=weapon.module_slot.frame,tags={'capacity-tag-59'},"
							   "localization={name='Tag Overflow'}}";
	assert(!WeaponLuaLoadPackage(
		"903", MOD_CAPABILITY_WEAPON_MODULES, 0, 0, pTagOverflow, (int)strlen(pTagOverflow), aError, sizeof(aError)));
	CWeaponCatalog::ResetCustomDefinitions();

	std::string DefinitionCapacitySource;
	for(int i = 0; i < 40; ++i)
	{
		char aLine[384];
		str_format(aLine,
				   sizeof(aLine),
				   "weapon.module "
				   "{schema=1,id='capacity-frame-%d',slot=weapon.module_slot.frame,tags={'ranged'},localization={name='"
				   "Capacity Frame %d'}}",
				   i,
				   i);
		DefinitionCapacitySource += aLine;
		str_format(aLine,
				   sizeof(aLine),
				   "weapon.module "
				   "{schema=1,id='capacity-part-%d',slot=weapon.module_slot.part,tags={'energy-part'},localization={"
				   "name='Capacity Part %d'}}",
				   i,
				   i);
		DefinitionCapacitySource += aLine;
	}
	DefinitionCapacitySource +=
		"weapon.compose {schema=1,id='too-many',frames={'tag:ranged'},parts={'tag:energy-part'},build=function(ctx) "
		"return {} end}";
	assert(WeaponLuaLoadPackage("904",
								MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_WEAPON_MODULES,
								0,
								0,
								DefinitionCapacitySource.c_str(),
								(int)DefinitionCapacitySource.size(),
								aError,
								sizeof(aError)));
	const int DefinitionsBeforeOverflow = CWeaponCatalog::DefinitionCount();
	assert(!CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	assert(CWeaponCatalog::DefinitionCount() == DefinitionsBeforeOverflow);
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pFinalizeRollback =
		"weapon.module {schema=1,id='frame-a',slot=weapon.module_slot.frame,localization={name='Frame A'}}"
		"weapon.module {schema=1,id='frame-b',slot=weapon.module_slot.frame,localization={name='Frame B'}}"
		"weapon.compose "
		"{schema=1,id='rollback-build',frames={'frame-a','frame-b'},parts={'official:module:part:barrel1'},build="
		"function(ctx) "
		"if ctx.frame.stable_id == 'workshop:905:module:frame-b' then error('second build failed') end "
		"return "
		"{max_level=0,behavior={},combat_template=weapon.combat.projectile,visual_template=weapon.visual.compact_"
		"weapon,combat={projectile_life=1},visuals={visual_size={4,2}},localization={name='Rollback Build'}} end}";
	assert(WeaponLuaLoadPackage("905",
								MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_WEAPON_MODULES,
								0,
								0,
								pFinalizeRollback,
								(int)strlen(pFinalizeRollback),
								aError,
								sizeof(aError)));
	const int DefinitionsBeforeBuildFailure = CWeaponCatalog::DefinitionCount();
	assert(!CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	assert(CWeaponCatalog::DefinitionCount() == DefinitionsBeforeBuildFailure);
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pInfiniteForge = "forge.recipe "
								 "{schema=1,id='infinite',priority=1,targets={'tag:ranged'},materials={'tag:ranged'},"
								 "localization={name='Infinite'},"
								 "resolve=function(ctx) while true do end end}";
	assert(WeaponLuaLoadPackage("906",
								MOD_CAPABILITY_FORGE_RECIPES,
								0,
								0,
								pInfiniteForge,
								(int)strlen(pInfiniteForge),
								aError,
								sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	assert(CForge::Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1),
						   CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2),
						   5,
						   10,
						   3,
						   5)
			   .m_Result == FORGERESULT_INVALID_RECIPE);
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pMemoryForge =
		"forge.recipe "
		"{schema=1,id='memory',priority=1,targets={'tag:ranged'},materials={'tag:ranged'},localization={name='Memory'},"
		"resolve=function(ctx) local value=string.rep('x',2*1024*1024) return nil end}";
	assert(WeaponLuaLoadPackage(
		"907", MOD_CAPABILITY_FORGE_RECIPES, 0, 0, pMemoryForge, (int)strlen(pMemoryForge), aError, sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	assert(CForge::Resolve(CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1),
						   CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2),
						   5,
						   10,
						   3,
						   5)
			   .m_Result == FORGERESULT_INVALID_RECIPE);
	CWeaponCatalog::ResetCustomDefinitions();

	const char *pCacheRecipe = "forge.recipe "
							   "{schema=1,id='cache-key',priority=1,targets={'tag:ranged'},materials={'tag:ranged'},"
							   "localization={name='Cache Key'},"
							   "resolve=function(ctx) return "
							   "{product=ctx.target.stable_id,level=ctx.material.level,cost=(ctx.target.ammo+ctx."
							   "material.ammo+ctx.base_cost+ctx.level_cost)%1000} end}";
	assert(WeaponLuaLoadPackage(
		"908", MOD_CAPABILITY_FORGE_RECIPES, 0, 0, pCacheRecipe, (int)strlen(pCacheRecipe), aError, sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	const CWeaponSpec CacheTarget = CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 1);
	const CWeaponSpec CacheMaterial = CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2, 2);
	assert(CForge::Resolve(CacheTarget, CacheMaterial, 3, 5, 7, 11).m_Cost == 26);
	assert(CForge::Resolve(CacheTarget, CacheMaterial, 4, 5, 7, 11).m_Cost == 27);
	assert(CForge::Resolve(CacheTarget, CacheMaterial, 3, 6, 7, 11).m_Cost == 27);
	assert(CForge::Resolve(CacheTarget, CacheMaterial, 3, 5, 8, 11).m_Cost == 27);
	assert(CForge::Resolve(CacheTarget, CacheMaterial, 3, 5, 7, 12).m_Cost == 27);
	assert(CForge::Resolve(CacheTarget, CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL2, 3), 3, 5, 7, 11)
			   .m_Product.m_Level == 3);
	CWeaponCatalog::ResetCustomDefinitions();
	const char *pReloadedCacheRecipe =
		"forge.recipe "
		"{schema=1,id='cache-key',priority=1,targets={'tag:ranged'},materials={'tag:ranged'},localization={name='"
		"Reloaded Cache'},"
		"resolve=function(ctx) return {product=ctx.target.stable_id,level=ctx.material.level,cost=99} end}";
	assert(WeaponLuaLoadPackage("908",
								MOD_CAPABILITY_FORGE_RECIPES,
								0,
								0,
								pReloadedCacheRecipe,
								(int)strlen(pReloadedCacheRecipe),
								aError,
								sizeof(aError)));
	assert(CWeaponCatalog::FinalizeLuaDefinitions(aError, sizeof(aError)));
	assert(CForge::Resolve(CacheTarget, CacheMaterial, 3, 5, 7, 11).m_Cost == 99);
	CWeaponCatalog::ResetCustomDefinitions();
	return 0;
}
