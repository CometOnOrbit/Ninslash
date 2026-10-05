#ifndef GAME_BASTION_STRIDER_H
#define GAME_BASTION_STRIDER_H

// Bastion Strider (DROIDTYPE_BOSSBULKHEAD slot): a heavy four-legged shield walker.
// The body rides a ray spring over the floor (legs are client IK), so ramps and steps never snag it.
// Frontal tower shield: flank it, wait out its slow turn, or break the shield.

#include <game/boss_v5.h>

static const int STRIDER_HEALTH = 8000;
static const float STRIDER_HOVER = 110.0f; // body center above the floor
static const float STRIDER_BOX_W = 96.0f;
static const float STRIDER_BOX_H = 64.0f;
// Collision body == drawn silhouette (hull + shield + thruster + mortar, down to the soles),
// symmetric so a turn never pushes the art into a wall. Origin = body center; bottom = the soles.
static const float STRIDER_BODY_HALF_W = 196.0f;
static const float STRIDER_BODY_TOP = -140.0f;
static const float STRIDER_BODY_BOTTOM = STRIDER_HOVER;
static const float STRIDER_MAX_CROUCH = 36.0f; // body may sink this far between the legs (hull stays above the soles)
static const float STRIDER_STEP_UP = 36.0f;	  // walks straight over steps/ramps this tall
static const float STRIDER_MAX_HOP = 230.0f;	  // hops onto ledges up to this height while walking
static const float STRIDER_GRAVITY = 0.8f;
static const float STRIDER_SHELL_GRAVITY = 0.45f;

static const int STRIDER_BASH_DAMAGE = 28;
static const int STRIDER_CHARGE_DAMAGE = 36;
static const int STRIDER_WAVE_DAMAGE = 22;
static const int STRIDER_LEAP_DAMAGE = 34;
static const int STRIDER_SHELL_DAMAGE = 22;
static const int STRIDER_ROAR_DAMAGE = 16;
static const float STRIDER_BASH_REACH = 210.0f;
static const float STRIDER_LEAP_RADIUS = 150.0f;
static const float STRIDER_SHELL_RADIUS = 84.0f;
static const float STRIDER_ROAR_RADIUS = 210.0f;
static const float STRIDER_WAVE_HEIGHT = 50.0f;
static const int STRIDER_WAVE_LIFE = 70;

// Core armor, shield bleed-through and the rear vents after a charge.
static const float STRIDER_CORE_ARMOR = 0.85f;
static const float STRIDER_SHIELD_BLEED = 0.15f;
static const float STRIDER_VENT_MULT = 1.75f;
static const int STRIDER_VENT_TICKS = 65;

enum
{
	STRIDER_PART_CORE = 0,
	STRIDER_PART_SHIELD,
	STRIDER_PART_LEGS,
	STRIDER_PART_MORTAR,
};
static const float s_aStriderPartShare[BOSSV5_NUM_PARTS] = {0.0f, 0.20f, 0.22f, 0.14f};
static const char *const s_apStriderPartName[BOSSV5_NUM_PARTS] = {"Core", "Shield", "Legs", "Mortar"};

static const CBossV5Hit s_aStriderHit[] = {
	{STRIDER_PART_CORE, 0.0f, -8.0f, 66.0f},
	{STRIDER_PART_CORE, -70.0f, -4.0f, 48.0f},
	{STRIDER_PART_CORE, 70.0f, -14.0f, 44.0f},
	{STRIDER_PART_SHIELD, 142.0f, -6.0f, 46.0f},
	{STRIDER_PART_SHIELD, 146.0f, 52.0f, 34.0f},
	{STRIDER_PART_SHIELD, 140.0f, -62.0f, 30.0f},
	{STRIDER_PART_LEGS, 92.0f, 70.0f, 24.0f},
	{STRIDER_PART_LEGS, -92.0f, 70.0f, 24.0f},
	{STRIDER_PART_MORTAR, -46.0f, -104.0f, 38.0f},
};
static const int NUM_STRIDER_HIT = sizeof(s_aStriderHit) / sizeof(s_aStriderHit[0]);

// Front hits land on the shield while it stands; it soaks most of it. Vents open = rear is soft.
inline int StriderIncomingDamage(int Dmg, int Part, bool FromBehind, bool Vented, int *pShieldDmg)
{
	*pShieldDmg = 0;
	if(Dmg <= 0)
		return Dmg;
	float Mult = 1.0f;
	if(Part == STRIDER_PART_SHIELD)
	{
		*pShieldDmg = Dmg;
		Mult = STRIDER_SHIELD_BLEED;
	}
	else if(Part == STRIDER_PART_CORE)
		Mult = Vented ? (FromBehind ? STRIDER_VENT_MULT : 1.25f) : (FromBehind ? 1.1f : STRIDER_CORE_ARMOR);
	const int Out = (int)(Dmg * Mult + 0.5f);
	return Out > 0 ? Out : 1;
}

enum
{
	STRIDER_ACT_IDLE = 0,
	STRIDER_ACT_WALK,
	STRIDER_ACT_BASH,
	STRIDER_ACT_CHARGE,
	STRIDER_ACT_STOMP,
	STRIDER_ACT_MORTAR,
	STRIDER_ACT_LEAP,
	STRIDER_ACT_ROAR,
	STRIDER_ACT_STAGGER,
	STRIDER_ACT_OVERHEAT,
	STRIDER_ACT_TURN,
	STRIDER_ACT_DEATH,
	NUM_STRIDER_ACTS
};

inline const CBossV5Act &StriderAct(int Act)
{
	static const CBossV5Act s_aAct[NUM_STRIDER_ACTS] = {
		{"idle", DROIDANIM_IDLE, 2.0f, true},
		{"walk", DROIDANIM_MOVE, 1.0f, true},
		{"bash", DROIDANIM_ATTACK, 1.05f, false},
		{"charge", DROIDANIM_CHARGE, 2.3f, false},
		{"stomp", DROIDANIM_VENT, 1.0f, false},
		{"mortar", DROIDANIM_MORTAR, 1.25f, false},
		{"leap", DROIDANIM_JUMPATTACK, 3.0f, false},
		{"roar", DROIDANIM_ENRAGE, 1.1f, false},
		{"stagger", DROIDANIM_STAGGER, 0.7f, false},
		{"overheat", DROIDANIM_GRAB, 1.0f, false},
		{"turn", DROIDANIM_TURN, 0.34f, false},
		{"death", DROIDANIM_IDLE, 2.6f, false},
	};
	return s_aAct[Act < 0 || Act >= NUM_STRIDER_ACTS ? 0 : Act];
}

inline int StriderActFromSnap(int Anim, int Status)
{
	static CBossV5Act s_aActs[NUM_STRIDER_ACTS];
	for(int i = 0; i < NUM_STRIDER_ACTS; i++)
		s_aActs[i] = StriderAct(i);
	return BossV5ActFromAnim(s_aActs, NUM_STRIDER_ACTS, STRIDER_ACT_DEATH, Anim, Status);
}

inline float StriderSpeed(int Phase, bool LegsAlive)
{
	static const float s_aSpeed[3] = {8.5f, 9.5f, 10.5f};
	const float s = s_aSpeed[Phase < 0 ? 0 : (Phase > 2 ? 2 : Phase)];
	return LegsAlive ? s : s * 0.65f;
}
inline float StriderTurnTime(int Phase) { return Phase >= 2 ? 0.22f : 0.32f; }
inline int StriderBashTick() { return BossV5Ticks(0.3f); }
inline int StriderBashCount(int Phase) { return 1 + (Phase >= 1 ? 1 : 0) + (Phase >= 2 ? 1 : 0); }
inline int StriderBashGap() { return BossV5Ticks(0.26f); }
inline int StriderChargeWindup(int Phase) { return BossV5Ticks(Phase >= 2 ? 0.3f : 0.42f); }
inline int StriderChargeTicks() { return BossV5Ticks(1.0f); }
inline float StriderChargeSpeed(int Phase) { return Phase >= 2 ? 25.0f : 22.0f; }
inline int StriderStompTick() { return BossV5Ticks(0.4f); }
inline float StriderWaveSpeed(int Phase) { return Phase >= 2 ? 14.5f : 12.5f; }
inline int StriderShellCount(int Phase) { return Phase >= 2 ? 5 : 4; }
inline int StriderShellTick(int Shot)
{
	static const float s_aShot[5] = {0.3f, 0.46f, 0.62f, 0.78f, 0.94f};
	return BossV5Ticks(s_aShot[Shot < 0 ? 0 : (Shot > 4 ? 4 : Shot)]);
}
inline int StriderLeapWindup(int Phase) { return BossV5Ticks(Phase >= 2 ? 0.28f : 0.38f); }
inline float StriderLeapFlight(float Dist) { return BossV5Clamp(Dist / 14.0f, 20.0f, 40.0f); }
inline int StriderRoarTick() { return BossV5Ticks(0.4f); }

// ---- Client layout (facing right, y down, origin = body center). Art sizes are source pixels. ----
struct CStriderSprite
{
	const char *m_pName;
	float m_X, m_Y, m_W, m_H;
};
static const CStriderSprite s_StriderHull = {"hull", 0.0f, -10.0f, 250.0f, 153.0f};
static const CStriderSprite s_StriderShield = {"shield", 146.0f, -4.0f, 104.0f, 172.0f};
static const CStriderSprite s_StriderShieldBroken = {"shield_broken", 146.0f, 4.0f, 92.0f, 164.0f};
static const CStriderSprite s_StriderMortar = {"mortar", -46.0f, -100.0f, 96.0f, 100.0f};
static const CStriderSprite s_StriderThruster = {"thruster", -134.0f, -20.0f, 70.0f, 45.0f};
static const float STRIDER_FOOT_W = 84.0f, STRIDER_FOOT_H = 39.0f;
// Leg art joints (source pixels): thigh = top of the full-leg painting, shin = the piston segment.
static const float STRIDER_THIGH_ART_W = 286.0f, STRIDER_THIGH_ART_H = 260.0f;
static const float STRIDER_THIGH_J0X = 65.0f, STRIDER_THIGH_J0Y = 75.0f, STRIDER_THIGH_J1X = 150.0f, STRIDER_THIGH_J1Y = 225.0f;
static const float STRIDER_SHIN_ART_W = 181.0f, STRIDER_SHIN_ART_H = 416.0f;
static const float STRIDER_SHIN_J0X = 40.0f, STRIDER_SHIN_J0Y = 55.0f, STRIDER_SHIN_J1X = 72.0f, STRIDER_SHIN_J1Y = 378.0f;
static const float STRIDER_THIGH = 66.0f;
static const float STRIDER_SHIN = 80.0f;
static const float STRIDER_ANKLE = 24.0f; // ankle above the floor
// Hip-to-floor reach of a leg (thigh + shin + ankle, minus a little slack so it never locks straight).
inline float StriderLegReach() { return STRIDER_THIGH + STRIDER_SHIN + STRIDER_ANKLE - 6.0f; }
// Lowest point of the hull art below the body center (hull sprite: y -10, h 153).
static const float STRIDER_HULL_BOTTOM = 60.0f;
// Leg order: 0 far-rear, 1 far-front, 2 near-rear, 3 near-front.
static const float s_aStriderHipX[4] = {-78.0f, 70.0f, -96.0f, 88.0f};
static const float s_aStriderHipY[4] = {28.0f, 28.0f, 36.0f, 36.0f};
static const float s_aStriderFootX[4] = {-96.0f, 104.0f, -122.0f, 124.0f};
static const int s_aStriderGait[4] = {0, 1, 1, 0}; // diagonal pairs
static const float STRIDER_STEP_DIST = 52.0f;
static const float STRIDER_STEP_LIFT = 34.0f;

#endif
