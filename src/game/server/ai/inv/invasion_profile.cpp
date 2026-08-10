#include "invasion_profile.h"

#include <game/questinfo.h>
#include <game/weapons.h>

namespace
{
void AddPrimary(CInvasionSkinProfile &Profile, const CWeaponSpec &Spec)
{
	if(Profile.m_PrimaryCount < INVASION_PROFILE_MAX_PRIMARY_CHOICES)
		Profile.m_aPrimaryChoices[Profile.m_PrimaryCount++] = Spec;
}

void AddUtility(CInvasionSkinProfile &Profile, const CWeaponSpec &Spec)
{
	if(Profile.m_UtilityCount < INVASION_PROFILE_MAX_UTILITY_WEAPONS)
		Profile.m_aUtilityWeapons[Profile.m_UtilityCount++] = Spec;
}

void SetHealth(CInvasionSkinProfile &Profile, int Health, int HealthPerLevel, int HealthCap, int Armor,
	int ArmorPerLevel, int ArmorCap)
{
	Profile.m_Health = Health;
	Profile.m_HealthPerLevel = HealthPerLevel;
	Profile.m_HealthCap = HealthCap;
	Profile.m_Armor = Armor;
	Profile.m_ArmorPerLevel = ArmorPerLevel;
	Profile.m_ArmorCap = ArmorCap;
}

void SetRanged(CInvasionSkinProfile &Profile, int PreferredRange, int RetreatRange,
	EInvasionMovementStrategy Movement, EInvasionTargetStrategy Targeting)
{
	Profile.m_PreferredRange = PreferredRange;
	Profile.m_RetreatRange = RetreatRange;
	Profile.m_Movement = Movement;
	Profile.m_Targeting = Targeting;
}

CInvasionSkinProfile BuildProfile(EInvasionSkinId Id)
{
	CInvasionSkinProfile Profile;
	Profile.m_Id = Id;
	Profile.m_ShockTicks = 2;
	Profile.m_TriggerLevel = 10;

	switch(Id)
	{
	case INVASION_SKIN_ALIEN1:
		Profile.m_Family = INVASION_FAMILY_ALIEN;
		SetRanged(Profile, 560, 180, INVASION_MOVE_HOLD_RANGE, INVASION_TARGET_NEAREST);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_GUN1));
		AddPrimary(Profile, CWeaponCatalog::Static(SW_GUN2));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL4));
		SetHealth(Profile, 60, 3, 300, 0, 0, 0);
		Profile.m_PowerLevel = 6;
		Profile.m_ShockTicks = 10;
		break;
	case INVASION_SKIN_ALIEN2:
		Profile.m_Family = INVASION_FAMILY_ALIEN;
		SetRanged(Profile, 150, 80, INVASION_MOVE_RUSH, INVASION_TARGET_NEAREST);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_CHAINSAW));
		SetHealth(Profile, 60, 3, 300, 60, 3, 300);
		Profile.m_PowerLevel = 8;
		Profile.m_TriggerLevel = 15;
		Profile.m_ShockTicks = 10;
		break;
	case INVASION_SKIN_ALIEN3:
		Profile.m_Family = INVASION_FAMILY_ALIEN;
		SetRanged(Profile, 650, 220, INVASION_MOVE_STRAFE, INVASION_TARGET_CLUSTER);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE4, PART2_BARREL4, 2));
		SetHealth(Profile, 60, 3, 300, 60, 3, 300);
		Profile.m_PowerLevel = 8;
		Profile.m_TriggerLevel = 15;
		Profile.m_ShockTicks = 10;
		break;
	case INVASION_SKIN_ALIEN4:
		Profile.m_Family = INVASION_FAMILY_ALIEN;
		SetRanged(Profile, 700, 240, INVASION_MOVE_HOLD_RANGE, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL1, 4));
		SetHealth(Profile, 60, 3, 200, 60, 3, 350);
		Profile.m_PowerLevel = 12;
		Profile.m_TriggerLevel = 15;
		Profile.m_ShockTicks = 10;
		break;
	case INVASION_SKIN_ALIEN5:
		Profile.m_Family = INVASION_FAMILY_ALIEN;
		SetRanged(Profile, 460, 150, INVASION_MOVE_RUSH, INVASION_TARGET_CLUSTER);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_FLAMER, 1));
		SetHealth(Profile, 50, 3, 150, 60, 3, 300);
		Profile.m_PowerLevel = 10;
		Profile.m_TriggerLevel = 15;
		Profile.m_ShockTicks = 10;
		break;

	case INVASION_SKIN_BUNNY1:
		Profile.m_Family = INVASION_FAMILY_BUNNY;
		SetRanged(Profile, 380, 130, INVASION_MOVE_RUSH, INVASION_TARGET_NEAREST);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL4));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE1));
		SetHealth(Profile, 40, 3, 220, 0, 0, 0);
		Profile.m_PowerLevel = 7;
		Profile.m_ShockTicks = 10;
		break;
	case INVASION_SKIN_BUNNY2:
		Profile.m_Family = INVASION_FAMILY_BUNNY;
		SetRanged(Profile, 440, 160, INVASION_MOVE_FLANK, INVASION_TARGET_ISOLATED);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL4));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE1));
		AddUtility(Profile, CWeaponCatalog::Static(SW_GRENADE2));
		AddUtility(Profile, CWeaponCatalog::Static(SW_SHIELD));
		SetHealth(Profile, 80, 4, 320, 0, 0, 0);
		Profile.m_PowerLevel = 12;
		Profile.m_TriggerLevel = 15;
		Profile.m_ShockTicks = 10;
		break;
	case INVASION_SKIN_BUNNY3:
		Profile.m_Family = INVASION_FAMILY_BUNNY;
		SetRanged(Profile, 520, 180, INVASION_MOVE_STRAFE, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL3));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL1));
		AddUtility(Profile, CWeaponCatalog::Static(SW_SHIELD));
		SetHealth(Profile, 80, 3, 320, 0, 0, 0);
		Profile.m_PowerLevel = 12;
		Profile.m_TriggerLevel = 15;
		Profile.m_ShockTicks = 10;
		break;
	case INVASION_SKIN_FOXY1:
		Profile.m_Family = INVASION_FAMILY_BUNNY;
		SetRanged(Profile, 600, 220, INVASION_MOVE_KITE, INVASION_TARGET_ISOLATED);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE4, PART2_BARREL1, 2));
		AddUtility(Profile, CWeaponCatalog::Static(SW_SHIELD));
		SetHealth(Profile, 80, 3, 320, 0, 0, 0);
		Profile.m_PowerLevel = 12;
		Profile.m_TriggerLevel = 15;
		Profile.m_ShockTicks = 6;
		break;
	case INVASION_SKIN_BUNNY4:
		Profile.m_Family = INVASION_FAMILY_BUNNY;
		SetRanged(Profile, 170, 90, INVASION_MOVE_FLANK, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_SPIN, PART2_MELEE1));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_SPIN, PART2_MELEE2));
		AddUtility(Profile, CWeaponCatalog::Static(SW_SHIELD));
		AddUtility(Profile, CWeaponCatalog::Static(SW_SHIELD));
		AddUtility(Profile, CWeaponCatalog::Static(SW_MASK2));
		SetHealth(Profile, 80, 3, 320, 0, 0, 0);
		Profile.m_PowerLevel = 14;
		Profile.m_TriggerLevel = 15;
		Profile.m_ShockTicks = 2;
		Profile.m_AttackOnDamage = true;
		break;

	case INVASION_SKIN_ROBO1:
		Profile.m_Family = INVASION_FAMILY_ROBOT;
		SetRanged(Profile, 650, 240, INVASION_MOVE_HOLD_RANGE, INVASION_TARGET_NEAREST);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL4));
		SetHealth(Profile, 50, 2, 100, 50, 2, 100);
		Profile.m_PowerLevel = 6;
		Profile.m_ReactionTime = 3;
		break;
	case INVASION_SKIN_ROBO2:
		Profile.m_Family = INVASION_FAMILY_ROBOT;
		SetRanged(Profile, 160, 90, INVASION_MOVE_RUSH, INVASION_TARGET_NEAREST);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_CHAINSAW));
		SetHealth(Profile, 60, 2, 200, 60, 2, 200);
		Profile.m_PowerLevel = 2;
		Profile.m_ReactionTime = 3;
		break;
	case INVASION_SKIN_ROBO3:
		Profile.m_Family = INVASION_FAMILY_ROBOT;
		SetRanged(Profile, 620, 240, INVASION_MOVE_SIEGE, INVASION_TARGET_OBJECTIVE);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL2));
		AddUtility(Profile, CWeaponCatalog::Static(SW_GRENADE1));
		AddUtility(Profile, CWeaponCatalog::Static(SW_GRENADE1));
		SetHealth(Profile, 70, 3, 200, 70, 3, 300);
		Profile.m_PowerLevel = 6;
		Profile.m_ReactionTime = 3;
		break;
	case INVASION_SKIN_ROBO4:
		Profile.m_Family = INVASION_FAMILY_ROBOT;
		SetRanged(Profile, 650, 220, INVASION_MOVE_STRAFE, INVASION_TARGET_CLUSTER);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL1, 2));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL2, 2));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL3, 2));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL4, 2));
		SetHealth(Profile, 80, 3, 200, 80, 3, 300);
		Profile.m_PowerLevel = 10;
		Profile.m_ReactionTime = 2;
		break;
	case INVASION_SKIN_ROBO5:
		Profile.m_Family = INVASION_FAMILY_ROBOT;
		SetRanged(Profile, 170, 90, INVASION_MOVE_RUSH, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_SPIN, PART2_MELEE4, 2));
		SetHealth(Profile, 90, 3, 200, 90, 3, 300);
		Profile.m_PowerLevel = 14;
		Profile.m_AttackOnDamage = true;
		break;

	case INVASION_SKIN_PYRO1:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 360, 120, INVASION_MOVE_RUSH, INVASION_TARGET_NEAREST);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_CHAINSAW));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL1));
		SetHealth(Profile, 90, 3, 100, 80, 3, 100);
		Profile.m_PowerLevel = 8;
		Profile.m_ShockTicks = 10;
		break;
	case INVASION_SKIN_PYRO2:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 660, 230, INVASION_MOVE_SIEGE, INVASION_TARGET_CLUSTER);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_BAZOOKA));
		AddPrimary(Profile, CWeaponCatalog::Static(SW_BOUNCER));
		SetHealth(Profile, 90, 3, 100, 80, 3, 100);
		Profile.m_PowerLevel = 8;
		Profile.m_ShockTicks = 10;
		Profile.m_BurstTicks = 20;
		break;
	case INVASION_SKIN_SKELETON1:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 680, 260, INVASION_MOVE_HOLD_RANGE, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL4, 3));
		SetHealth(Profile, 90, 3, 100, 80, 3, 100);
		Profile.m_PowerLevel = 8;
		Profile.m_ShockTicks = 6;
		break;
	case INVASION_SKIN_SKELETON2:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 560, 170, INVASION_MOVE_KITE, INVASION_TARGET_ISOLATED);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL4, 3));
		AddPrimary(Profile, CWeaponCatalog::Static(SW_CHAINSAW));
		SetHealth(Profile, 90, 3, 100, 80, 3, 100);
		Profile.m_PowerLevel = 8;
		Profile.m_ShockTicks = 6;
		break;
	case INVASION_SKIN_SKELETON3:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 220, 100, INVASION_MOVE_FLANK, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE1, PART2_BARREL2, 2));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_MELEE, PART2_MELEE4, 3));
		SetHealth(Profile, 90, 3, 100, 80, 3, 100);
		Profile.m_PowerLevel = 8;
		Profile.m_ShockTicks = 4;
		break;
	case INVASION_SKIN_PYRO3:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 450, 140, INVASION_MOVE_RUSH, INVASION_TARGET_CLUSTER);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_FLAMER));
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_SPIN, PART2_MELEE2, 4));
		AddUtility(Profile, CWeaponCatalog::Static(SW_MASK3));
		SetHealth(Profile, 90, 3, 100, 80, 3, 100);
		Profile.m_PowerLevel = 8;
		Profile.m_ShockTicks = 4;
		Profile.m_AttackOnDamage = true;
		break;

	case INVASION_SKIN_CYBORG_GUNNER:
		Profile.m_Family = INVASION_FAMILY_CYBORG;
		SetRanged(Profile, 620, 230, INVASION_MOVE_HOLD_RANGE, INVASION_TARGET_NEAREST);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL4, 2));
		SetHealth(Profile, 110, 4, 300, 80, 3, 250);
		Profile.m_PowerLevel = 9;
		Profile.m_ReactionTime = 4;
		Profile.m_TriggerLevel = 12;
		break;
	case INVASION_SKIN_CYBORG_RAIL:
		Profile.m_Family = INVASION_FAMILY_CYBORG;
		SetRanged(Profile, 820, 330, INVASION_MOVE_SIEGE, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE5, PART2_RAIL, 3));
		SetHealth(Profile, 120, 4, 350, 100, 3, 300);
		Profile.m_PowerLevel = 11;
		Profile.m_ReactionTime = 5;
		Profile.m_BurstTicks = 30;
		break;
	case INVASION_SKIN_CYBORG_BREACHER:
		Profile.m_Family = INVASION_FAMILY_CYBORG;
		SetRanged(Profile, 430, 150, INVASION_MOVE_RUSH, INVASION_TARGET_OBJECTIVE);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_BAZOOKA, 2));
		SetHealth(Profile, 150, 5, 400, 120, 4, 350);
		Profile.m_PowerLevel = 10;
		Profile.m_ReactionTime = 3;
		Profile.m_AttackOnDamage = true;
		break;

	case INVASION_SKIN_ELITE_ALIEN_ALPHA:
		Profile.m_Family = INVASION_FAMILY_ALIEN;
		SetRanged(Profile, 680, 220, INVASION_MOVE_STRAFE, INVASION_TARGET_CLUSTER);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE4, PART2_BARREL4, 4));
		SetHealth(Profile, 130, 6, 420, 110, 5, 360);
		Profile.m_PowerLevel = 13;
		Profile.m_ReactionTime = 2;
		Profile.m_TriggerLevel = 8;
		Profile.m_AttackOnDamage = true;
		break;
	case INVASION_SKIN_ELITE_ALIEN_BIO:
		Profile.m_Family = INVASION_FAMILY_ALIEN;
		SetRanged(Profile, 470, 150, INVASION_MOVE_RUSH, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_FLAMER, 3));
		SetHealth(Profile, 150, 6, 450, 130, 5, 400);
		Profile.m_PowerLevel = 14;
		Profile.m_ReactionTime = 2;
		Profile.m_AttackOnDamage = true;
		break;
	case INVASION_SKIN_ELITE_ALIEN_STALKER:
		Profile.m_Family = INVASION_FAMILY_ALIEN;
		SetRanged(Profile, 190, 90, INVASION_MOVE_AMBUSH, INVASION_TARGET_ISOLATED);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_SPIN, PART2_MELEE3, 4));
		SetHealth(Profile, 115, 5, 380, 90, 4, 300);
		Profile.m_PowerLevel = 15;
		Profile.m_ReactionTime = 2;
		Profile.m_AttackOnDamage = true;
		Profile.m_StartTriggered = true;
		break;
	case INVASION_SKIN_ELITE_ROBOT_COMMANDER:
		Profile.m_Family = INVASION_FAMILY_ROBOT;
		SetRanged(Profile, 680, 250, INVASION_MOVE_SIEGE, INVASION_TARGET_OBJECTIVE);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE3, PART2_BARREL4, 4));
		AddUtility(Profile, CWeaponCatalog::Static(SW_GRENADE1));
		SetHealth(Profile, 170, 6, 500, 150, 5, 450);
		Profile.m_PowerLevel = 14;
		Profile.m_ReactionTime = 3;
		Profile.m_AttackOnDamage = true;
		break;
	case INVASION_SKIN_ELITE_ROBOT_RAIL:
		Profile.m_Family = INVASION_FAMILY_ROBOT;
		SetRanged(Profile, 850, 340, INVASION_MOVE_HOLD_RANGE, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE5, PART2_RAIL, 4));
		SetHealth(Profile, 160, 6, 500, 160, 5, 480);
		Profile.m_PowerLevel = 15;
		Profile.m_ReactionTime = 4;
		Profile.m_BurstTicks = 35;
		break;
	case INVASION_SKIN_ELITE_ROBOT_BREACHER:
		Profile.m_Family = INVASION_FAMILY_ROBOT;
		SetRanged(Profile, 420, 140, INVASION_MOVE_RUSH, INVASION_TARGET_CLUSTER);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_BOUNCER, 3));
		SetHealth(Profile, 200, 7, 550, 170, 5, 500);
		Profile.m_PowerLevel = 14;
		Profile.m_ReactionTime = 2;
		Profile.m_AttackOnDamage = true;
		break;
	case INVASION_SKIN_ELITE_PYRO_SIEGE:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 760, 300, INVASION_MOVE_SIEGE, INVASION_TARGET_CLUSTER);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_BAZOOKA, 3));
		SetHealth(Profile, 190, 6, 500, 150, 5, 450);
		Profile.m_PowerLevel = 14;
		Profile.m_ReactionTime = 4;
		Profile.m_BurstTicks = 35;
		break;
	case INVASION_SKIN_ELITE_PYRO_BOUNCER:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 650, 240, INVASION_MOVE_KITE, INVASION_TARGET_ISOLATED);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_BOUNCER, 4));
		SetHealth(Profile, 180, 6, 480, 140, 5, 430);
		Profile.m_PowerLevel = 14;
		Profile.m_ReactionTime = 3;
		break;
	case INVASION_SKIN_ELITE_PYRO_FURNACE:
		Profile.m_Family = INVASION_FAMILY_PYRO;
		SetRanged(Profile, 440, 140, INVASION_MOVE_RUSH, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Static(SW_FLAMER, 4));
		AddUtility(Profile, CWeaponCatalog::Modular(PART1_SPIN, PART2_MELEE2, 4));
		SetHealth(Profile, 210, 7, 560, 170, 5, 500);
		Profile.m_PowerLevel = 15;
		Profile.m_ReactionTime = 2;
		Profile.m_AttackOnDamage = true;
		break;
	case INVASION_SKIN_ELITE_BUNNY_ASSASSIN:
		Profile.m_Family = INVASION_FAMILY_BUNNY;
		SetRanged(Profile, 260, 100, INVASION_MOVE_FLANK, INVASION_TARGET_ISOLATED);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_SPIN, PART2_MELEE2, 4));
		SetHealth(Profile, 100, 5, 360, 80, 4, 300);
		Profile.m_PowerLevel = 15;
		Profile.m_ReactionTime = 2;
		Profile.m_AttackOnDamage = true;
		break;
	case INVASION_SKIN_ELITE_BUNNY_DUELIST:
		Profile.m_Family = INVASION_FAMILY_BUNNY;
		SetRanged(Profile, 520, 170, INVASION_MOVE_STRAFE, INVASION_TARGET_LOW_HEALTH);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE4, PART2_BARREL1, 4));
		AddUtility(Profile, CWeaponCatalog::Static(SW_SHIELD));
		SetHealth(Profile, 140, 5, 420, 100, 4, 350);
		Profile.m_PowerLevel = 14;
		Profile.m_ReactionTime = 2;
		break;
	case INVASION_SKIN_ELITE_BUNNY_SABOTEUR:
		Profile.m_Family = INVASION_FAMILY_BUNNY;
		SetRanged(Profile, 600, 220, INVASION_MOVE_AMBUSH, INVASION_TARGET_OBJECTIVE);
		AddPrimary(Profile, CWeaponCatalog::Modular(PART1_BASE2, PART2_BARREL4, 4));
		AddUtility(Profile, CWeaponCatalog::Static(SW_GRENADE2));
		SetHealth(Profile, 120, 5, 400, 90, 4, 320);
		Profile.m_PowerLevel = 13;
		Profile.m_ReactionTime = 3;
		Profile.m_StartTriggered = true;
		break;
	case INVASION_SKIN_INVALID:
	case NUM_INVASION_SKINS:
		break;
	}

	if(Profile.m_PrimaryCount == 0)
		AddPrimary(Profile, CWeaponCatalog::Static(SW_GUN1));
	return Profile;
}

int ClampProfileLevel(int Level, int Count)
{
	if(Level < 0)
		return 0;
	if(Level >= Count)
		return Count - 1;
	return Level;
}
}

bool IsValidInvasionSkinProfile(EInvasionSkinId Id)
{
	return Id >= 0 && Id < NUM_INVASION_SKINS;
}

const CInvasionSkinProfile &InvasionSkinProfile(EInvasionSkinId Id)
{
	static CInvasionSkinProfile s_aProfiles[NUM_INVASION_SKINS];
	static bool s_Initialized = false;
	if(!s_Initialized)
	{
		for(int i = 0; i < NUM_INVASION_SKINS; ++i)
			s_aProfiles[i] = BuildProfile(static_cast<EInvasionSkinId>(i));
		s_Initialized = true;
	}

	static const CInvasionSkinProfile s_Fallback = BuildProfile(INVASION_SKIN_ALIEN1);
	return IsValidInvasionSkinProfile(Id) ? s_aProfiles[Id] : s_Fallback;
}

EInvasionSkinId InvasionSkinForWave(int WaveType, int Level, bool Elite)
{
	if(Elite)
	{
		const int Variant = Level < 0 ? 0 : Level % 3;
		switch(WaveType)
		{
		case WAVE_ALIENS:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_ELITE_ALIEN_ALPHA + Variant);
		case WAVE_ROBOTS:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_ELITE_ROBOT_COMMANDER + Variant);
		case WAVE_SKELETONS:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_ELITE_PYRO_SIEGE + Variant);
		case WAVE_FURRIES:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_ELITE_BUNNY_ASSASSIN + Variant);
		case WAVE_CYBORGS:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_CYBORG_GUNNER + Variant);
		default:
			break;
		}
	}
	else
	{
		switch(WaveType)
		{
		case WAVE_ALIENS:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_ALIEN1 + ClampProfileLevel(Level, 5));
		case WAVE_ROBOTS:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_ROBO1 + ClampProfileLevel(Level, 5));
		case WAVE_SKELETONS:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_PYRO1 + ClampProfileLevel(Level, 6));
		case WAVE_FURRIES:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_BUNNY1 + ClampProfileLevel(Level, 5));
		case WAVE_CYBORGS:
			return static_cast<EInvasionSkinId>(INVASION_SKIN_CYBORG_GUNNER + ClampProfileLevel(Level, 3));
		default:
			break;
		}
	}

	return INVASION_SKIN_ALIEN1;
}
