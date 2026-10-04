#ifndef GAME_FOUNDRY_WARDEN_H
#define GAME_FOUNDRY_WARDEN_H

#include <math.h>
#include <generated/protocol.h>

// Tick = 50 (SERVER_TICK_SPEED). Timings match warden-motion-v2.combat.json.
static const int WARDEN_TICK_SPEED = 50;
static const int WARDEN_HEALTH = 5000;
static const int WARDEN_CLAMP_DAMAGE = 24;
static const int WARDEN_SLAM_DAMAGE = 28;
static const int WARDEN_GRAB_DAMAGE = 12;
static const int WARDEN_CHARGE_DAMAGE = 26;
static const int WARDEN_ROCKET_DAMAGE = 18;
static const float WARDEN_GRAB_RANGE = 640.0f;
// Past this the warden stops walking and thrusts at the player.
static const float WARDEN_CHARGE_RANGE = 460.0f;
static const float WARDEN_CORE_MULT = 1.6f;
static const float WARDEN_CORE_ARMOR = 0.7f;
static const int WARDEN_MAX_ROCKETS = 8;

enum
{
	WARDEN_PART_CORE = 0,
	WARDEN_PART_ARMS,
	WARDEN_PART_LEGS,
	WARDEN_PART_MORTAR,
	NUM_WARDEN_PARTS
};

// Part pools as a share of max health. Core has no pool; it is the main health bar.
static const float s_aWardenPartShare[NUM_WARDEN_PARTS] = {0.0f, 0.2f, 0.22f, 0.16f};

struct CWardenHitCircle
{
	int m_Part;
	float m_X, m_Y, m_R;
};

// Offsets from m_Pos (feet at +64), mirrored on x. Matches the setup pose at render scale 0.85.
static const CWardenHitCircle s_aWardenHit[] = {
	{WARDEN_PART_CORE, 0.0f, -52.0f, 46.0f},
	{WARDEN_PART_CORE, 48.0f, -58.0f, 30.0f},
	{WARDEN_PART_CORE, 0.0f, -92.0f, 28.0f},
	{WARDEN_PART_MORTAR, 45.0f, -125.0f, 26.0f},
	{WARDEN_PART_MORTAR, 60.0f, -165.0f, 20.0f},
	{WARDEN_PART_ARMS, 80.0f, -72.0f, 24.0f},
	{WARDEN_PART_ARMS, 130.0f, -56.0f, 24.0f},
	{WARDEN_PART_ARMS, 180.0f, -52.0f, 30.0f},
	{WARDEN_PART_ARMS, 230.0f, -46.0f, 22.0f},
	{WARDEN_PART_LEGS, 106.0f, -20.0f, 22.0f},
	{WARDEN_PART_LEGS, 95.0f, 15.0f, 22.0f},
	{WARDEN_PART_LEGS, 110.0f, 45.0f, 20.0f},
};
static const int NUM_WARDEN_HIT = sizeof(s_aWardenHit) / sizeof(s_aWardenHit[0]);

// Surface distance to the nearest circle, negative inside.
inline float WardenHitDist(int i, float Dx, float Dy)
{
	const float x = fabsf(Dx) - s_aWardenHit[i].m_X;
	const float y = Dy - s_aWardenHit[i].m_Y;
	return sqrtf(x * x + y * y) - s_aWardenHit[i].m_R;
}

// Splash and melee hits arrive with no usable point; those land on the core.
inline int WardenPartAt(float Dx, float Dy)
{
	int Best = WARDEN_PART_CORE;
	float BestDist = 40.0f;
	for(int i = 0; i < NUM_WARDEN_HIT; i++)
	{
		const float d = WardenHitDist(i, Dx, Dy);
		if(d < BestDist)
		{
			BestDist = d;
			Best = s_aWardenHit[i].m_Part;
		}
	}
	return Best;
}

// Phase 0 above 66%, 1 above 33%, 2 below. Same integer split as the regional boss controller.
inline int WardenPhase(int Health, int MaxHealth)
{
	if(Health * 3 <= MaxHealth)
		return 2;
	if(Health * 3 <= MaxHealth * 2)
		return 1;
	return 0;
}

enum
{
	WARDEN_ACT_IDLE = 0,
	WARDEN_ACT_WALK,
	WARDEN_ACT_CLAMP,
	WARDEN_ACT_SLAM,
	WARDEN_ACT_MORTAR,
	WARDEN_ACT_VENT,
	WARDEN_ACT_ENRAGE,
	WARDEN_ACT_STAGGER,
	WARDEN_ACT_GRAB,
	WARDEN_ACT_CHARGE,
	WARDEN_ACT_DEATH,
	NUM_WARDEN_ACTS
};

struct CWardenAct
{
	const char *m_pClip;
	int m_Anim;
	float m_Duration;
	bool m_Loop;
};

inline int WardenTicks(float Seconds)
{
	return (int)(Seconds * WARDEN_TICK_SPEED + 0.5f);
}

inline const CWardenAct &WardenAct(int Act)
{
	static const CWardenAct s_aAct[NUM_WARDEN_ACTS] = {
		{"idle", DROIDANIM_IDLE, 3.6f, true},
		{"walk", DROIDANIM_MOVE, 1.4f, true},
		{"clamp_sweep", DROIDANIM_ATTACK, 2.05f, false},
		{"furnace_slam", DROIDANIM_JUMPATTACK, 2.5f, false},
		{"mortar_barrage", DROIDANIM_MORTAR, 2.8f, false},
		{"vent", DROIDANIM_VENT, 3.1f, false},
		{"enrage", DROIDANIM_ENRAGE, 2.2f, false},
		{"stagger", DROIDANIM_STAGGER, 0.85f, false},
		{"clamp_sweep", DROIDANIM_GRAB, 0.45f, false},
		{"charge", DROIDANIM_CHARGE, 1.5f, false},
		{"death", DROIDANIM_IDLE, 3.6f, false},
	};
	if(Act < 0 || Act >= NUM_WARDEN_ACTS)
		return s_aAct[WARDEN_ACT_IDLE];
	return s_aAct[Act];
}

inline int WardenActFromSnap(int Anim, int Status)
{
	if(Status == DROIDSTATUS_TERMINATED)
		return WARDEN_ACT_DEATH;
	switch(Anim)
	{
	case DROIDANIM_MOVE: return WARDEN_ACT_WALK;
	case DROIDANIM_ATTACK: return WARDEN_ACT_CLAMP;
	case DROIDANIM_JUMPATTACK: return WARDEN_ACT_SLAM;
	case DROIDANIM_MORTAR: return WARDEN_ACT_MORTAR;
	case DROIDANIM_VENT: return WARDEN_ACT_VENT;
	case DROIDANIM_ENRAGE: return WARDEN_ACT_ENRAGE;
	case DROIDANIM_STAGGER: return WARDEN_ACT_STAGGER;
	case DROIDANIM_GRAB: return WARDEN_ACT_GRAB;
	case DROIDANIM_CHARGE: return WARDEN_ACT_CHARGE;
	default: return WARDEN_ACT_IDLE;
	}
}

inline float BossClipTime(float Duration, bool Loop, int StartTick, int NowTick, float Intra)
{
	float Time = ((float)(NowTick - StartTick) + Intra) / (float)WARDEN_TICK_SPEED;
	if(Time < 0.0f)
		Time = 0.0f;
	if(Loop)
	{
		if(Duration > 0.0f)
			Time = fmodf(Time, Duration);
	}
	else if(Time > Duration)
		Time = Duration;
	return Time;
}

inline float WardenClipTime(int Act, int StartTick, int NowTick, float Intra)
{
	return BossClipTime(WardenAct(Act).m_Duration, WardenAct(Act).m_Loop, StartTick, NowTick, Intra);
}

inline int WardenClampImpactTick()
{
	return WardenTicks(0.775f);
}

inline int WardenSlamImpactTick()
{
	return WardenTicks(1.12f);
}

// Charge: tuck and aim until launch, thrust until end, then skid out the rest of the clip.
inline int WardenChargeLaunchTick()
{
	return WardenTicks(0.4f);
}

inline int WardenChargeEndTick()
{
	return WardenTicks(1.1f);
}

inline int WardenMortarShotTick(int Shot)
{
	static const float s_aShot[] = {0.72f, 1.08f, 1.44f};
	if(Shot < 0 || Shot > 2)
		return 0;
	return WardenTicks(s_aShot[Shot]);
}

inline bool WardenCoreExposed(int Act, int ElapsedTicks)
{
	if(Act != WARDEN_ACT_VENT)
		return false;
	return ElapsedTicks >= WardenTicks(0.52f) && ElapsedTicks < WardenTicks(2.55f);
}

// The core is armored unless the vent has it open; limbs take hits as-is.
inline int WardenIncomingDamage(int Dmg, int Part, bool Exposed)
{
	if(Dmg <= 0 || Part != WARDEN_PART_CORE)
		return Dmg;
	const int Out = (int)(Dmg * (Exposed ? WARDEN_CORE_MULT : WARDEN_CORE_ARMOR) + 0.5f);
	return Out > 0 ? Out : 1;
}

// Skeleton units, y down, root on the floor. Order: rear L, front L, front R, rear R.
// Legs 0+2 and 1+3 step as diagonal pairs. Rest X sits a little inside the setup pose so a
// foot can trail WARDEN_STEP_DIST behind before IK runs out of reach.
static const float s_aWardenHipX[4] = {-54.0f, -48.0f, 48.0f, 54.0f};
static const float s_aWardenHipY[4] = {-132.0f, -111.0f, -111.0f, -132.0f};
static const float s_aWardenUpper[4] = {76.8505f, 81.57f, 81.57f, 76.8505f};
static const float s_aWardenLower[4] = {93.9415f, 50.04f, 50.04f, 93.9415f};
static const float s_aWardenFootX[4] = {-128.0f, -88.0f, 88.0f, 128.0f};
static const float WARDEN_FOOT_ABOVE_GROUND = 11.0f;
static const float WARDEN_STEP_DIST = 36.0f;
static const float WARDEN_STEP_LIFT = 30.0f;

// Bend > 0 folds the knee to the left of the hip-to-target line. Angles match CalcTransformationMatrix.
inline bool WardenSolveLeg(float Hx, float Hy, float Tx, float Ty, float L1, float L2, float Bend, float *pHip, float *pKnee)
{
	const float dx = Tx - Hx;
	const float dy = Ty - Hy;
	float Dist = sqrtf(dx * dx + dy * dy);
	const float MaxReach = L1 + L2 - 0.05f;
	const float MinReach = fabsf(L1 - L2) + 0.05f;
	if(Dist < 0.001f)
		Dist = 0.001f;
	if(Dist > MaxReach)
		Dist = MaxReach;
	if(Dist < MinReach)
		Dist = MinReach;
	float CosHip = (L1 * L1 + Dist * Dist - L2 * L2) / (2.0f * L1 * Dist);
	float CosKnee = (Dist * Dist - L1 * L1 - L2 * L2) / (2.0f * L1 * L2);
	if(CosHip > 1.0f)
		CosHip = 1.0f;
	if(CosHip < -1.0f)
		CosHip = -1.0f;
	if(CosKnee > 1.0f)
		CosKnee = 1.0f;
	if(CosKnee < -1.0f)
		CosKnee = -1.0f;
	*pHip = atan2f(dy, dx) - Bend * acosf(CosHip);
	*pKnee = Bend * acosf(CosKnee);
	return true;
}

inline void WardenSolveLegUp(float Hx, float Hy, float Tx, float Ty, float L1, float L2, float *pHip, float *pKnee)
{
	float aHip, aKnee, bHip, bKnee;
	WardenSolveLeg(Hx, Hy, Tx, Ty, L1, L2, 1.0f, &aHip, &aKnee);
	WardenSolveLeg(Hx, Hy, Tx, Ty, L1, L2, -1.0f, &bHip, &bKnee);
	const float aY = Hy + sinf(aHip) * L1;
	const float bY = Hy + sinf(bHip) * L1;
	if(aY <= bY)
	{
		*pHip = aHip;
		*pKnee = aKnee;
	}
	else
	{
		*pHip = bHip;
		*pKnee = bKnee;
	}
}

#endif
