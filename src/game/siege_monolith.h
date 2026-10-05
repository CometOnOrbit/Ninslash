#ifndef GAME_SIEGE_MONOLITH_H
#define GAME_SIEGE_MONOLITH_H

// Siege Monolith (DROIDTYPE_BOSSVAULT slot): a hovering fortress turret with two forms.
// Ground form: struts down, laser sweeps, mortar, ground pound. Air form: thrusters lit,
// carpet bombing, drone launches, crushing drop. Its vents open while it transforms.

#include <base/vmath.h>
#include <game/boss_v5.h>

static const int MONOLITH_HEALTH = 8800;
static const float MONOLITH_HOVER = 178.0f;	  // ground form: body center above the floor
static const float MONOLITH_AIR_UP = 300.0f;  // air form: preferred height above the target
static const float MONOLITH_BOX_W = 96.0f;
static const float MONOLITH_BOX_H = 180.0f;
// Collision body == drawn silhouette (origin = body center). Ground form: hull + launcher/turret
// mount down to the planted strut feet (MONOLITH_HOVER below the center). Air form: struts tucked.
static const float MONOLITH_BODY_HALF_W = 150.0f;
static const float MONOLITH_BODY_TOP = -172.0f;
static const float MONOLITH_MAX_CROUCH = 30.0f;
static const float MONOLITH_AIR_HALF_W = 95.0f;
static const float MONOLITH_AIR_BOTTOM = 168.0f;
static const float MONOLITH_STEP_UP = 36.0f;
static const float MONOLITH_MAX_HOP = 200.0f;
static const float MONOLITH_GRAVITY = 0.8f;
static const float MONOLITH_SHELL_GRAVITY = 0.45f;
static const float MONOLITH_BOMB_GRAVITY = 0.55f;

static const int MONOLITH_BEAM_DAMAGE = 12; // per beam tick
static const int MONOLITH_SHELL_DAMAGE = 22;
static const int MONOLITH_BOMB_DAMAGE = 24;
static const int MONOLITH_POUND_DAMAGE = 30;
static const int MONOLITH_CRUSH_DAMAGE = 40;
static const int MONOLITH_ROAR_DAMAGE = 16;
static const float MONOLITH_BEAM_RANGE = 1400.0f;
static const float MONOLITH_BEAM_RADIUS = 18.0f;
static const float MONOLITH_SHELL_RADIUS = 80.0f;
static const float MONOLITH_BOMB_RADIUS = 85.0f;
static const float MONOLITH_POUND_RADIUS = 175.0f;
static const float MONOLITH_ROAR_RADIUS = 220.0f;
static const int MONOLITH_MAX_DRONES = 4;

static const float MONOLITH_CORE_ARMOR = 0.7f;
static const float MONOLITH_VENT_MULT = 2.2f;
static const float MONOLITH_CORE_VENTED = 1.3f;

enum
{
	MONOLITH_PART_CORE = 0,
	MONOLITH_PART_TURRET,
	MONOLITH_PART_THRUSTERS,
	MONOLITH_PART_VENTS,
};
static const float s_aMonolithPartShare[BOSSV5_NUM_PARTS] = {0.0f, 0.16f, 0.18f, 0.14f};
static const char *const s_apMonolithPartName[BOSSV5_NUM_PARTS] = {"Core", "Turret", "Thrusters", "Vents"};

static const CBossV5Hit s_aMonolithHit[] = {
	{MONOLITH_PART_CORE, 0.0f, -70.0f, 60.0f},
	{MONOLITH_PART_CORE, 0.0f, 4.0f, 64.0f},
	{MONOLITH_PART_CORE, 0.0f, 78.0f, 58.0f},
	{MONOLITH_PART_TURRET, 30.0f, -138.0f, 34.0f},
	{MONOLITH_PART_TURRET, 110.0f, -140.0f, 20.0f},
	{MONOLITH_PART_THRUSTERS, 0.0f, 124.0f, 34.0f},
	{MONOLITH_PART_VENTS, 52.0f, -22.0f, 30.0f},
	{MONOLITH_PART_VENTS, 52.0f, 52.0f, 30.0f},
};
static const int NUM_MONOLITH_HIT = sizeof(s_aMonolithHit) / sizeof(s_aMonolithHit[0]);

// Vents only take damage while open; closed vents are just hull.
inline int MonolithIncomingDamage(int Dmg, int Part, bool Vented)
{
	if(Dmg <= 0)
		return Dmg;
	float Mult = 1.0f;
	if(Part == MONOLITH_PART_VENTS)
		Mult = MONOLITH_VENT_MULT;
	else if(Part == MONOLITH_PART_CORE)
		Mult = Vented ? MONOLITH_CORE_VENTED : MONOLITH_CORE_ARMOR;
	const int Out = (int)(Dmg * Mult + 0.5f);
	return Out > 0 ? Out : 1;
}

enum
{
	MONOLITH_ACT_IDLE = 0,
	MONOLITH_ACT_MOVE,
	MONOLITH_ACT_SWEEP,
	MONOLITH_ACT_MORTAR,
	MONOLITH_ACT_TRANSFORM,
	MONOLITH_ACT_BOMB,
	MONOLITH_ACT_DRONES,
	MONOLITH_ACT_SLAM,
	MONOLITH_ACT_ROAR,
	MONOLITH_ACT_STAGGER,
	MONOLITH_ACT_DEATH,
	NUM_MONOLITH_ACTS
};

inline const CBossV5Act &MonolithAct(int Act)
{
	static const CBossV5Act s_aAct[NUM_MONOLITH_ACTS] = {
		{"idle", DROIDANIM_IDLE, 2.0f, true},
		{"move", DROIDANIM_MOVE, 1.0f, true},
		{"sweep", DROIDANIM_ATTACK, 2.0f, false},
		{"mortar", DROIDANIM_MORTAR, 1.3f, false},
		{"transform", DROIDANIM_VENT, 1.1f, false},
		{"bomb", DROIDANIM_CHARGE, 2.6f, false},
		{"drones", DROIDANIM_GRAB, 0.8f, false},
		{"slam", DROIDANIM_JUMPATTACK, 2.6f, false},
		{"roar", DROIDANIM_ENRAGE, 1.1f, false},
		{"stagger", DROIDANIM_STAGGER, 0.7f, false},
		{"death", DROIDANIM_IDLE, 2.8f, false},
	};
	return s_aAct[Act < 0 || Act >= NUM_MONOLITH_ACTS ? 0 : Act];
}

inline int MonolithActFromSnap(int Anim, int Status)
{
	static CBossV5Act s_aActs[NUM_MONOLITH_ACTS];
	for(int i = 0; i < NUM_MONOLITH_ACTS; i++)
		s_aActs[i] = MonolithAct(i);
	return BossV5ActFromAnim(s_aActs, NUM_MONOLITH_ACTS, MONOLITH_ACT_DEATH, Anim, Status);
}

inline float MonolithSpeed(int Phase, bool Air)
{
	static const float s_aGround[3] = {6.5f, 7.5f, 8.5f};
	static const float s_aAir[3] = {9.5f, 10.5f, 11.5f};
	const int p = Phase < 0 ? 0 : (Phase > 2 ? 2 : Phase);
	return Air ? s_aAir[p] : s_aGround[p];
}
inline int MonolithSweepStart() { return BossV5Ticks(0.5f); }
// Phase 3 sweeps there and back again.
inline int MonolithSweepEnd(int Phase) { return BossV5Ticks(Phase >= 2 ? 1.9f : 1.35f); }
inline float MonolithSweepArc() { return 1.25f; } // radians swept through the target
inline int MonolithShellCount(int Phase) { return Phase >= 2 ? 7 : 5; }
inline int MonolithShellTick(int Shot) { return BossV5Ticks(0.3f + Shot * 0.13f); }
inline int MonolithBombEvery(int Phase) { return Phase >= 2 ? 4 : 5; }
inline int MonolithBombTicks() { return BossV5Ticks(1.5f); }
inline int MonolithDroneTick() { return BossV5Ticks(0.4f); }
inline int MonolithDroneCount(int Phase) { return Phase >= 2 ? 3 : 2; }
inline int MonolithSlamRise() { return BossV5Ticks(0.32f); }
inline int MonolithRoarTick() { return BossV5Ticks(0.5f); }

// ---- Client layout (facing right, y down, origin = body center) ----
struct CMonolithSprite
{
	const char *m_pName;
	float m_X, m_Y, m_W, m_H;
};
static const CMonolithSprite s_MonolithHull = {"hull", 0.0f, 0.0f, 146.0f, 280.0f};
static const CMonolithSprite s_MonolithLauncher = {"launcher", -40.0f, -150.0f, 80.0f, 50.0f};
static const CMonolithSprite s_MonolithThruster = {"thruster", 0.0f, 122.0f, 70.0f, 82.0f};
static const CMonolithSprite s_aMonolithVent[2] = {{"vent", 50.0f, -22.0f, 50.0f, 49.0f}, {"vent", 50.0f, 52.0f, 50.0f, 49.0f}};
static const float MONOLITH_VENT_CLOSED_W = 48.0f, MONOLITH_VENT_CLOSED_H = 42.0f;
// Turret pivots on its base; the art is 867x266 with the pivot near (260, 128) and muzzle at (867, 125).
static const float MONOLITH_TURRET_X = 26.0f, MONOLITH_TURRET_Y = -142.0f;
static const float MONOLITH_TURRET_W = 230.0f, MONOLITH_TURRET_H = 70.6f;
static const float MONOLITH_TURRET_PX = 0.30f, MONOLITH_TURRET_PY = 0.48f; // pivot as a fraction of the art
static const float MONOLITH_MUZZLE = 161.0f;							  // pivot to muzzle distance (world)
// Struts: art 255x361, hip at (45, 42), foot at (150, 325).
static const float MONOLITH_STRUT_ART_W = 255.0f, MONOLITH_STRUT_ART_H = 361.0f;
static const float MONOLITH_STRUT_J0X = 45.0f, MONOLITH_STRUT_J0Y = 42.0f, MONOLITH_STRUT_J1X = 150.0f, MONOLITH_STRUT_J1Y = 325.0f;
// Strut order: 0 far, 1 near-front, 2 near-rear.
static const float s_aMonolithStrutX[3] = {-30.0f, 50.0f, -56.0f};
static const float s_aMonolithStrutY[3] = {96.0f, 104.0f, 104.0f};
static const float s_aMonolithFootX[3] = {-70.0f, 140.0f, -140.0f};
// Longest a strut may reach from its hip to the floor (struts scale uniformly, keep it modest).
static const float MONOLITH_STRUT_REACH = 175.0f;

inline vec2 MonolithMuzzleLocal(float Angle)
{
	// Angle is the world aim angle in facing space (0 = straight ahead).
	return vec2(MONOLITH_TURRET_X + cosf(Angle) * MONOLITH_MUZZLE, MONOLITH_TURRET_Y + sinf(Angle) * MONOLITH_MUZZLE);
}

#endif
