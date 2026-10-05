#ifndef GAME_SKITTER_MATRIARCH_H
#define GAME_SKITTER_MATRIARCH_H

// Skitter Matriarch (DROIDTYPE_BOSSRAIL slot): a hovering six-legged spider tank.
// Shared by the server entity, the client renderer and tests/skitter_matriarch.cpp.
// Units: world pixels, 50 ticks per second, layout facing right (+x), y down,
// origin = m_Pos = body center. The server mirrors x by m_Dir.

#include <math.h>
#include <generated/protocol.h>

static const int MATRIARCH_TICK_SPEED = 50;
static const int MATRIARCH_HEALTH = 7000;
static const float MATRIARCH_HOVER = 82.0f;	 // body center above the floor
static const float MATRIARCH_BOX_W = 80.0f;
static const float MATRIARCH_BOX_H = 56.0f;
// Collision body == drawn silhouette: carapace + sac + legs spread to their tips, from the top of
// the shell down to the feet (MATRIARCH_HOVER below the center). Origin = body center.
static const float MATRIARCH_BODY_HALF_W = 205.0f;
static const float MATRIARCH_BODY_TOP = -80.0f;
static const float MATRIARCH_MAX_CROUCH = 22.0f; // sinks between the legs under low ceilings
static const float MATRIARCH_STEP_UP = 32.0f;
static const float MATRIARCH_GRAVITY = 0.75f;
static const float MATRIARCH_GLOB_GRAVITY = 0.5f;

static const int MATRIARCH_POUNCE_DAMAGE = 34;
static const int MATRIARCH_LAND_DAMAGE = 22;
static const int MATRIARCH_STAB_DAMAGE = 20;
static const int MATRIARCH_STAB_DAMAGE_BROKEN = 12;
static const int MATRIARCH_GLOB_DAMAGE = 18;
static const int MATRIARCH_PUDDLE_DAMAGE = 7;
static const int MATRIARCH_THRASH_DAMAGE = 26;
static const int MATRIARCH_ROAR_DAMAGE = 16;

static const float MATRIARCH_LAND_RADIUS = 140.0f;
static const float MATRIARCH_STAB_REACH = 170.0f;
static const float MATRIARCH_STAB_RADIUS = 40.0f;
static const float MATRIARCH_PUDDLE_RADIUS = 56.0f;
static const float MATRIARCH_THRASH_RADIUS = 190.0f;
static const float MATRIARCH_ROAR_RADIUS = 180.0f;
static const float MATRIARCH_POUNCE_RANGE = 360.0f;	 // beyond this she always pounces when able
static const float MATRIARCH_SAC_EXPOSED_MULT = 1.6f;
static const float MATRIARCH_SAC_MULT = 1.25f;
static const float MATRIARCH_CORE_ARMOR = 0.7f;

static const int MATRIARCH_MAX_SHOTS = 16;
static const int MATRIARCH_MAX_BROOD = 4;
static const int MATRIARCH_EGG_HATCH = 70;
static const int MATRIARCH_PUDDLE_LIFE = 150;
static const int MATRIARCH_EXPOSE_TICKS = 30;

// BossShot kinds owned by the matriarch.
enum
{
	MATRIARCH_SHOT_GLOB = 10,
	MATRIARCH_SHOT_PUDDLE = 11,
	MATRIARCH_SHOT_EGG = 12,
};

enum
{
	MATRIARCH_PART_CORE = 0,
	MATRIARCH_PART_FANGS,
	MATRIARCH_PART_LEGS,
	MATRIARCH_PART_SAC,
	NUM_MATRIARCH_PARTS
};

static const float s_aMatriarchPartShare[NUM_MATRIARCH_PARTS] = {0.0f, 0.16f, 0.22f, 0.16f};
static const char *const s_apMatriarchPartName[NUM_MATRIARCH_PARTS] = {"Core", "Fangs", "Legs", "Sac"};

struct CMatriarchHitCircle
{
	int m_Part;
	float m_X, m_Y, m_R;
};

// Facing right. Matches the client layout below.
static const CMatriarchHitCircle s_aMatriarchHit[] = {
	{MATRIARCH_PART_CORE, 5.0f, -8.0f, 48.0f},
	{MATRIARCH_PART_CORE, 62.0f, -6.0f, 36.0f},
	{MATRIARCH_PART_CORE, -48.0f, -10.0f, 36.0f},
	{MATRIARCH_PART_CORE, -112.0f, -18.0f, 46.0f},
	{MATRIARCH_PART_FANGS, 122.0f, 16.0f, 32.0f},
	{MATRIARCH_PART_FANGS, 165.0f, 40.0f, 22.0f},
	{MATRIARCH_PART_LEGS, -120.0f, 40.0f, 24.0f},
	{MATRIARCH_PART_LEGS, -20.0f, 52.0f, 24.0f},
	{MATRIARCH_PART_LEGS, 95.0f, 46.0f, 24.0f},
	{MATRIARCH_PART_SAC, -178.0f, 4.0f, 34.0f},
};
static const int NUM_MATRIARCH_HIT = sizeof(s_aMatriarchHit) / sizeof(s_aMatriarchHit[0]);

// Dx is already in facing space (world dx * dir).
inline int MatriarchPartAt(float Dx, float Dy)
{
	int Best = MATRIARCH_PART_CORE;
	float BestDist = 36.0f;
	for(int i = 0; i < NUM_MATRIARCH_HIT; i++)
	{
		const float x = Dx - s_aMatriarchHit[i].m_X;
		const float y = Dy - s_aMatriarchHit[i].m_Y;
		const float d = sqrtf(x * x + y * y) - s_aMatriarchHit[i].m_R;
		// Parts win ties against the core so the sac and fangs are worth aiming for.
		const float Bias = s_aMatriarchHit[i].m_Part == MATRIARCH_PART_CORE ? 6.0f : 0.0f;
		if(d + Bias < BestDist)
		{
			BestDist = d + Bias;
			Best = s_aMatriarchHit[i].m_Part;
		}
	}
	return Best;
}

inline int MatriarchIncomingDamage(int Dmg, int Part, bool SacExposed)
{
	if(Dmg <= 0)
		return Dmg;
	float Mult = 1.0f;
	if(Part == MATRIARCH_PART_SAC)
		Mult = SacExposed ? MATRIARCH_SAC_EXPOSED_MULT : MATRIARCH_SAC_MULT;
	else if(Part == MATRIARCH_PART_CORE)
		Mult = SacExposed ? 1.0f : MATRIARCH_CORE_ARMOR;
	const int Out = (int)(Dmg * Mult + 0.5f);
	return Out > 0 ? Out : 1;
}

inline int MatriarchPhase(int Health, int MaxHealth)
{
	if(Health * 3 <= MaxHealth)
		return 2;
	if(Health * 3 <= MaxHealth * 2)
		return 1;
	return 0;
}

enum
{
	MATRIARCH_ACT_IDLE = 0,
	MATRIARCH_ACT_SKITTER,
	MATRIARCH_ACT_STAB,
	MATRIARCH_ACT_POUNCE,
	MATRIARCH_ACT_SPIT,
	MATRIARCH_ACT_BROOD,
	MATRIARCH_ACT_ROAR,
	MATRIARCH_ACT_STAGGER,
	MATRIARCH_ACT_CLING,
	MATRIARCH_ACT_THRASH,
	MATRIARCH_ACT_DEATH,
	NUM_MATRIARCH_ACTS
};

struct CMatriarchAct
{
	const char *m_pName;
	int m_Anim;
	float m_Duration; // pounce and cling end on their own; this is the safety cap
	bool m_Loop;
};

inline int MatriarchTicks(float Seconds)
{
	return (int)(Seconds * MATRIARCH_TICK_SPEED + 0.5f);
}

inline const CMatriarchAct &MatriarchAct(int Act)
{
	static const CMatriarchAct s_aAct[NUM_MATRIARCH_ACTS] = {
		{"idle", DROIDANIM_IDLE, 2.0f, true},
		{"skitter", DROIDANIM_MOVE, 0.8f, true},
		{"stab", DROIDANIM_ATTACK, 1.3f, false},
		{"pounce", DROIDANIM_JUMPATTACK, 3.0f, false},
		{"spit", DROIDANIM_MORTAR, 1.2f, false},
		{"brood", DROIDANIM_VENT, 1.2f, false},
		{"roar", DROIDANIM_ENRAGE, 1.1f, false},
		{"stagger", DROIDANIM_STAGGER, 0.6f, false},
		{"cling", DROIDANIM_GRAB, 0.6f, false},
		{"thrash", DROIDANIM_CHARGE, 0.85f, false},
		{"death", DROIDANIM_IDLE, 2.6f, false},
	};
	if(Act < 0 || Act >= NUM_MATRIARCH_ACTS)
		return s_aAct[MATRIARCH_ACT_IDLE];
	return s_aAct[Act];
}

inline int MatriarchActFromSnap(int Anim, int Status)
{
	if(Status == DROIDSTATUS_TERMINATED)
		return MATRIARCH_ACT_DEATH;
	for(int i = 0; i < MATRIARCH_ACT_DEATH; i++)
		if(MatriarchAct(i).m_Anim == Anim)
			return i;
	return MATRIARCH_ACT_IDLE;
}

// Pounce: crouch and track, then a ballistic leap.
inline int MatriarchPounceWindup(int Phase, bool Chained)
{
	if(Chained)
		return 7;
	return Phase >= 2 ? 11 : 15;
}

// Flight time for a pounce, in ticks.
inline float MatriarchPounceFlight(float Dist)
{
	float T = Dist / 16.0f;
	if(T < 14.0f)
		T = 14.0f;
	if(T > 34.0f)
		T = 34.0f;
	return T;
}

// Launch velocity that lands on (Dx, Dy) after T ticks under Gravity, capped at MaxSpeed.
inline void MatriarchBallistic(float Dx, float Dy, float T, float Gravity, float MaxSpeed, float *pVx, float *pVy)
{
	float Vx = Dx / T;
	float Vy = Dy / T - 0.5f * Gravity * T;
	const float Len = sqrtf(Vx * Vx + Vy * Vy);
	if(Len > MaxSpeed && Len > 0.0f)
	{
		Vx *= MaxSpeed / Len;
		Vy *= MaxSpeed / Len;
	}
	*pVx = Vx;
	*pVy = Vy;
}

inline int MatriarchStabCount(int Phase) { return Phase >= 2 ? 5 : (Phase >= 1 ? 4 : 3); }
inline int MatriarchStabTick(int Stab)
{
	static const float s_aStab[5] = {0.28f, 0.46f, 0.64f, 0.82f, 1.0f};
	return MatriarchTicks(s_aStab[Stab < 0 ? 0 : (Stab > 4 ? 4 : Stab)]);
}

inline int MatriarchSpitCount(int Phase) { return Phase >= 2 ? 5 : 4; }
inline int MatriarchSpitTick(int Shot)
{
	static const float s_aSpit[5] = {0.32f, 0.5f, 0.68f, 0.84f, 1.0f};
	return MatriarchTicks(s_aSpit[Shot < 0 ? 0 : (Shot > 4 ? 4 : Shot)]);
}

inline int MatriarchBroodTick() { return MatriarchTicks(0.6f); }
inline int MatriarchThrashTick() { return MatriarchTicks(0.32f); }
inline int MatriarchRoarTick() { return MatriarchTicks(0.4f); }

inline float MatriarchSpeed(int Phase, bool LegsAlive)
{
	static const float s_aSpeed[3] = {12.0f, 13.5f, 15.0f};
	const float s = s_aSpeed[Phase < 0 ? 0 : (Phase > 2 ? 2 : Phase)];
	return LegsAlive ? s : s * 0.65f;
}

// ---- Client layout (facing right, y down, origin = body center) ----
struct CMatriarchSprite
{
	const char *m_pName;
	float m_X, m_Y, m_W, m_H, m_Rot;
};
static const CMatriarchSprite s_MatriarchSac = {"sac", -178.0f, 4.0f, 66.0f, 90.0f, -0.5f};
static const CMatriarchSprite s_MatriarchAbdomen = {"abdomen", -112.0f, -18.0f, 150.0f, 116.0f, 0.0f};
static const CMatriarchSprite s_MatriarchThorax = {"thorax", 8.0f, -8.0f, 236.0f, 98.0f, 0.0f};
static const CMatriarchSprite s_MatriarchHead = {"head", 122.0f, 16.0f, 112.0f, 79.0f, 0.0f};
static const float MATRIARCH_MANDIBLE_X = 166.0f, MATRIARCH_MANDIBLE_Y = 26.0f;
static const float MATRIARCH_MANDIBLE_W = 34.0f, MATRIARCH_MANDIBLE_H = 60.0f;

static const float MATRIARCH_THIGH = 112.0f;
static const float MATRIARCH_SHIN = 138.0f;
// Leg order: 0-2 far side (rear to front), 3-5 near side (rear to front).
static const float s_aMatriarchHipX[6] = {-58.0f, 4.0f, 66.0f, -72.0f, -12.0f, 52.0f};
static const float s_aMatriarchHipY[6] = {6.0f, 8.0f, 6.0f, 14.0f, 18.0f, 14.0f};
static const float s_aMatriarchFootX[6] = {-150.0f, -25.0f, 195.0f, -200.0f, -70.0f, 150.0f};
// Tripod gait groups: rear-near, mid-far, front-near step together, then the rest.
static const int s_aMatriarchGait[6] = {1, 0, 1, 0, 1, 0};
static const float MATRIARCH_STEP_DIST = 46.0f;
static const float MATRIARCH_STEP_LIFT = 30.0f;

#endif
