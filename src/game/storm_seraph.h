#ifndef GAME_STORM_SERAPH_H
#define GAME_STORM_SERAPH_H

// Storm Seraph (DROIDTYPE_BOSSARC slot): a fast Star-style flyer ringed by four tesla nodes.
// The nodes armor the core while they orbit; after a thunder plunge they are recalled and the
// grounded core is wide open.

#include <game/boss_v5.h>

static const int SERAPH_HEALTH = 6000;
static const float SERAPH_BOX_W = 70.0f;
static const float SERAPH_BOX_H = 70.0f;
// Collision body == drawn silhouette: halo to thruster tip, body + inner wings (outer feathers
// excluded so it still fits the shafts it hunts through). Origin = body center.
static const float SERAPH_BODY_HALF_W = 110.0f;
static const float SERAPH_BODY_TOP = -126.0f;
static const float SERAPH_BODY_BOTTOM = 145.0f;
static const float SERAPH_GRAVITY = 0.7f; // only while dead or plunging
static const int SERAPH_NODES = 4;
static const float SERAPH_NODE_ORBIT = 100.0f;
static const float SERAPH_NODE_RADIUS = 22.0f;
static const float SERAPH_HOVER_UP = 230.0f;   // preferred height above the target
static const float SERAPH_HOVER_SIDE = 240.0f; // preferred side offset

static const int SERAPH_DIVE_DAMAGE = 32;
static const int SERAPH_LATTICE_DAMAGE = 18;
static const int SERAPH_ORB_DAMAGE = 22;
static const int SERAPH_PLUNGE_DAMAGE = 36;
static const int SERAPH_SPARK_DAMAGE = 18;
static const int SERAPH_ZAP_DAMAGE = 24;
static const int SERAPH_ROAR_DAMAGE = 16;
static const float SERAPH_LATTICE_HALF = 190.0f; // half side of the node square around the target
static const float SERAPH_PLUNGE_RADIUS = 150.0f;
static const float SERAPH_ZAP_RANGE = 280.0f;
static const float SERAPH_ROAR_RADIUS = 220.0f;
static const int SERAPH_ORB_LIFE = 200;
static const int SERAPH_SPARK_LIFE = 50;

static const float SERAPH_CORE_SHIELDED = 0.45f; // nodes orbiting
static const float SERAPH_CORE_EXPOSED = 1.8f;	 // recalled nodes, grounded

enum
{
	SERAPH_PART_CORE = 0,
	SERAPH_PART_NODES,
	SERAPH_PART_WINGS,
	SERAPH_PART_HALO,
};
static const float s_aSeraphPartShare[BOSSV5_NUM_PARTS] = {0.0f, 0.24f, 0.18f, 0.12f};
static const char *const s_apSeraphPartName[BOSSV5_NUM_PARTS] = {"Core", "Nodes", "Wings", "Halo"};

// Node states (BossShot VelX).
enum
{
	SERAPH_NODE_ORBIT_STATE = 0,
	SERAPH_NODE_ARMED,	 // deployed, telegraph lines
	SERAPH_NODE_LIVE,	 // deployed, lightning on
	SERAPH_NODE_DOCKED,	 // recalled into the core
};

static const CBossV5Hit s_aSeraphHit[] = {
	{SERAPH_PART_CORE, 4.0f, -10.0f, 44.0f},
	{SERAPH_PART_CORE, -6.0f, 46.0f, 34.0f},
	{SERAPH_PART_CORE, 28.0f, -70.0f, 28.0f},
	{SERAPH_PART_WINGS, -86.0f, -52.0f, 42.0f},
	{SERAPH_PART_WINGS, -150.0f, -100.0f, 30.0f},
	{SERAPH_PART_HALO, 18.0f, -110.0f, 26.0f},
};
static const int NUM_SERAPH_HIT = sizeof(s_aSeraphHit) / sizeof(s_aSeraphHit[0]);

inline int SeraphIncomingDamage(int Dmg, int Part, bool Exposed, bool Shielded)
{
	if(Dmg <= 0)
		return Dmg;
	float Mult = 1.0f;
	if(Part == SERAPH_PART_CORE)
		Mult = Exposed ? SERAPH_CORE_EXPOSED : (Shielded ? SERAPH_CORE_SHIELDED : 1.0f);
	else if(Exposed)
		Mult = 1.3f;
	const int Out = (int)(Dmg * Mult + 0.5f);
	return Out > 0 ? Out : 1;
}

enum
{
	SERAPH_ACT_IDLE = 0,
	SERAPH_ACT_FLY,
	SERAPH_ACT_DIVE,
	SERAPH_ACT_LATTICE,
	SERAPH_ACT_ORBS,
	SERAPH_ACT_PLUNGE,
	SERAPH_ACT_RECHARGE,
	SERAPH_ACT_ROAR,
	SERAPH_ACT_STAGGER,
	SERAPH_ACT_ZAP,
	SERAPH_ACT_DEATH,
	NUM_SERAPH_ACTS
};

inline const CBossV5Act &SeraphAct(int Act)
{
	static const CBossV5Act s_aAct[NUM_SERAPH_ACTS] = {
		{"idle", DROIDANIM_IDLE, 2.0f, true},
		{"fly", DROIDANIM_MOVE, 1.2f, true},
		{"dive", DROIDANIM_CHARGE, 2.6f, false},
		{"lattice", DROIDANIM_ATTACK, 2.6f, false},
		{"orbs", DROIDANIM_MORTAR, 1.05f, false},
		{"plunge", DROIDANIM_JUMPATTACK, 3.0f, false},
		{"recharge", DROIDANIM_VENT, 1.7f, false},
		{"roar", DROIDANIM_ENRAGE, 1.1f, false},
		{"stagger", DROIDANIM_STAGGER, 0.6f, false},
		{"zap", DROIDANIM_GRAB, 0.7f, false},
		{"death", DROIDANIM_IDLE, 2.4f, false},
	};
	return s_aAct[Act < 0 || Act >= NUM_SERAPH_ACTS ? 0 : Act];
}

inline int SeraphActFromSnap(int Anim, int Status)
{
	static CBossV5Act s_aActs[NUM_SERAPH_ACTS];
	for(int i = 0; i < NUM_SERAPH_ACTS; i++)
		s_aActs[i] = SeraphAct(i);
	return BossV5ActFromAnim(s_aActs, NUM_SERAPH_ACTS, SERAPH_ACT_DEATH, Anim, Status);
}

inline float SeraphSpeed(int Phase, bool WingsAlive)
{
	static const float s_aSpeed[3] = {13.0f, 14.5f, 16.0f};
	const float s = s_aSpeed[Phase < 0 ? 0 : (Phase > 2 ? 2 : Phase)];
	return WingsAlive ? s : s * 0.7f;
}
inline int SeraphDiveWindup(int Phase) { return BossV5Ticks(Phase >= 2 ? 0.26f : 0.34f); }
inline int SeraphDiveTicks() { return BossV5Ticks(0.55f); }
inline float SeraphDiveSpeed(bool WingsAlive) { return WingsAlive ? 31.0f : 23.0f; }
inline int SeraphDiveCount(int Phase) { return Phase >= 2 ? 3 : 2; }
inline int SeraphLatticeArm() { return BossV5Ticks(0.35f); }
inline int SeraphLatticeLive(int Phase) { return BossV5Ticks(Phase >= 2 ? 1.1f : 1.3f); }
inline int SeraphLatticeEnd(int Phase) { return BossV5Ticks(Phase >= 2 ? 2.0f : 2.1f); }
inline int SeraphOrbCount(int Phase, bool HaloAlive) { return (HaloAlive ? 4 : 2) + (Phase >= 1 ? 1 : 0); }
inline int SeraphOrbTick(int Shot)
{
	static const float s_aShot[5] = {0.3f, 0.42f, 0.54f, 0.66f, 0.78f};
	return BossV5Ticks(s_aShot[Shot < 0 ? 0 : (Shot > 4 ? 4 : Shot)]);
}
inline int SeraphPlungeRise() { return BossV5Ticks(0.45f); }
inline int SeraphPlungeHold(int Phase) { return BossV5Ticks(Phase >= 2 ? 0.5f : 0.65f); }
inline int SeraphZapTick() { return BossV5Ticks(0.3f); }
inline int SeraphRoarTick() { return BossV5Ticks(0.5f); }

// Orbit angle of node i at server tick Tick (radians).
inline float SeraphNodeAngle(int i, int Tick) { return Tick * 0.045f + i * 1.5707963f; }

// ---- Client layout (facing right, y down, origin = body center) ----
struct CSeraphSprite
{
	const char *m_pName;
	float m_X, m_Y, m_W, m_H;
};
static const CSeraphSprite s_SeraphBody = {"body", 0.0f, 0.0f, 112.0f, 175.0f};
static const CSeraphSprite s_SeraphHead = {"head", 30.0f, -78.0f, 60.0f, 70.0f};
static const CSeraphSprite s_SeraphCore = {"core", 10.0f, -6.0f, 62.0f, 60.0f};
static const CSeraphSprite s_SeraphHalo = {"halo", 18.0f, -110.0f, 96.0f, 29.0f};
static const CSeraphSprite s_SeraphThruster = {"thruster", -4.0f, 112.0f, 46.0f, 67.0f};
static const CSeraphSprite s_SeraphNode = {"node", 0.0f, 0.0f, 40.0f, 55.0f};
static const CSeraphSprite s_SeraphOrb = {"orb", 0.0f, 0.0f, 62.0f, 41.0f};
// Wings hang from their root joint (source-pixel fraction of the mirrored art).
static const float SERAPH_WING_W = 230.0f, SERAPH_WING_H = 155.0f, SERAPH_WING_RX = 0.88f, SERAPH_WING_RY = 0.62f;
static const float SERAPH_WING2_W = 150.0f, SERAPH_WING2_H = 84.0f, SERAPH_WING2_RX = 0.82f, SERAPH_WING2_RY = 0.45f;
static const float SERAPH_WING_X = -34.0f, SERAPH_WING_Y = -40.0f; // near wing root; raised ~0.3 rad at rest
static const float SERAPH_WING2_X = -24.0f, SERAPH_WING2_Y = -14.0f;

#endif
