#ifndef GAME_ABYSS_ANGLER_H
#define GAME_ABYSS_ANGLER_H

#include <math.h>
#include <base/vmath.h>
#include <generated/protocol.h>
#include <game/foundry_warden.h>

// Timings match data/anim/abyss_angler (scripts/build_abyss_angler.py).
static const int ANGLER_HEALTH = 4600;
static const int ANGLER_BITE_DAMAGE = 26;
static const int ANGLER_CHOMP_DAMAGE = 34;
static const int ANGLER_BUBBLE_DAMAGE = 16;
static const int ANGLER_DIVE_DAMAGE = 24;
static const float ANGLER_LURE_RANGE = 560.0f;
static const float ANGLER_LURE_MULT = 1.5f;
static const float ANGLER_GILL_MULT = 1.35f;
static const int ANGLER_MAX_BUBBLES = 9;

enum
{
	ANGLER_PART_CORE = 0,
	ANGLER_PART_LURE,
	ANGLER_PART_FINS,
	ANGLER_PART_JAW,
	NUM_ANGLER_PARTS
};

static const float s_aAnglerPartShare[NUM_ANGLER_PARTS] = {0.0f, 0.14f, 0.2f, 0.18f};

// Offsets from m_Pos facing right, y down: setup pose at the 0.75 render scale in droid_visual.h.
static const CWardenHitCircle s_aAnglerHit[] = {
	{ANGLER_PART_CORE, 8.0f, -15.0f, 69.0f},
	{ANGLER_PART_CORE, 82.0f, -34.0f, 39.0f},
	{ANGLER_PART_CORE, -75.0f, 0.0f, 36.0f},
	{ANGLER_PART_JAW, 86.0f, 34.0f, 36.0f},
	{ANGLER_PART_JAW, 124.0f, 4.0f, 21.0f},
	{ANGLER_PART_LURE, 106.0f, -126.0f, 20.0f},
	{ANGLER_PART_LURE, 76.0f, -143.0f, 12.0f},
	{ANGLER_PART_LURE, 40.0f, -122.0f, 12.0f},
	{ANGLER_PART_FINS, -38.0f, -101.0f, 24.0f},
	{ANGLER_PART_FINS, -38.0f, 56.0f, 22.0f},
	{ANGLER_PART_FINS, -122.0f, -3.0f, 24.0f},
	{ANGLER_PART_FINS, -170.0f, -3.0f, 20.0f},
	{ANGLER_PART_FINS, -225.0f, -3.0f, 34.0f},
};
static const int NUM_ANGLER_HIT = sizeof(s_aAnglerHit) / sizeof(s_aAnglerHit[0]);

// Dir is the facing (+1 right, -1 left); the art is not symmetric so it mirrors, not folds.
inline vec2 AnglerHitPos(int i, int Dir)
{
	return vec2(s_aAnglerHit[i].m_X * (float)Dir, s_aAnglerHit[i].m_Y);
}

inline int AnglerPartAt(float Dx, float Dy, int Dir)
{
	int Best = ANGLER_PART_CORE;
	float BestDist = 40.0f;
	for(int i = 0; i < NUM_ANGLER_HIT; i++)
	{
		const float x = Dx - s_aAnglerHit[i].m_X * (float)Dir;
		const float y = Dy - s_aAnglerHit[i].m_Y;
		const float d = sqrtf(x * x + y * y) - s_aAnglerHit[i].m_R;
		if(d < BestDist)
		{
			BestDist = d;
			Best = s_aAnglerHit[i].m_Part;
		}
	}
	return Best;
}

// The lure is the weak point; open gills leave the core soft.
inline int AnglerIncomingDamage(int Dmg, int Part, bool GillsOpen)
{
	if(Dmg <= 0)
		return Dmg;
	float Mult = 1.0f;
	if(Part == ANGLER_PART_LURE)
		Mult = ANGLER_LURE_MULT;
	else if(Part == ANGLER_PART_CORE && GillsOpen)
		Mult = ANGLER_GILL_MULT;
	return (int)(Dmg * Mult + 0.5f);
}

enum
{
	ANGLER_ACT_IDLE = 0,
	ANGLER_ACT_SWIM,
	ANGLER_ACT_BITE,
	ANGLER_ACT_LURE,
	ANGLER_ACT_SPIT,
	ANGLER_ACT_DIVE,
	ANGLER_ACT_STAGGER,
	ANGLER_ACT_ROAR,
	ANGLER_ACT_DEATH,
	NUM_ANGLER_ACTS
};

inline const CWardenAct &AnglerAct(int Act)
{
	static const CWardenAct s_aAct[NUM_ANGLER_ACTS] = {
		{"idle", DROIDANIM_IDLE, 2.4f, true},
		{"swim", DROIDANIM_MOVE, 1.6f, true},
		{"bite", DROIDANIM_ATTACK, 1.3f, false},
		{"lure", DROIDANIM_VENT, 2.6f, false},
		{"spit", DROIDANIM_MORTAR, 2.0f, false},
		{"dive", DROIDANIM_JUMPATTACK, 1.8f, false},
		{"stagger", DROIDANIM_STAGGER, 0.8f, false},
		{"roar", DROIDANIM_ENRAGE, 2.0f, false},
		{"death", DROIDANIM_IDLE, 3.0f, false},
	};
	if(Act < 0 || Act >= NUM_ANGLER_ACTS)
		return s_aAct[ANGLER_ACT_IDLE];
	return s_aAct[Act];
}

inline int AnglerActFromSnap(int Anim, int Status)
{
	if(Status == DROIDSTATUS_TERMINATED)
		return ANGLER_ACT_DEATH;
	switch(Anim)
	{
	case DROIDANIM_MOVE: return ANGLER_ACT_SWIM;
	case DROIDANIM_ATTACK: return ANGLER_ACT_BITE;
	case DROIDANIM_VENT: return ANGLER_ACT_LURE;
	case DROIDANIM_MORTAR: return ANGLER_ACT_SPIT;
	case DROIDANIM_JUMPATTACK: return ANGLER_ACT_DIVE;
	case DROIDANIM_STAGGER: return ANGLER_ACT_STAGGER;
	case DROIDANIM_ENRAGE: return ANGLER_ACT_ROAR;
	default: return ANGLER_ACT_IDLE;
	}
}

inline float AnglerClipTime(int Act, int StartTick, int NowTick, float Intra)
{
	return BossClipTime(AnglerAct(Act).m_Duration, AnglerAct(Act).m_Loop, StartTick, NowTick, Intra);
}

// Bite lunges between these two, chomping at the end.
inline int AnglerBiteLungeTick() { return WardenTicks(0.35f); }
inline int AnglerBiteChompTick() { return WardenTicks(0.62f); }
// Lure pulls between these two, then the jaw snaps shut.
inline int AnglerLureStartTick() { return WardenTicks(0.5f); }
inline int AnglerLureChompTick() { return WardenTicks(2.0f); }
inline int AnglerDiveStartTick() { return WardenTicks(0.62f); }
inline int AnglerDiveEndTick() { return WardenTicks(1.1f); }

inline int AnglerSpitTick(int Shot)
{
	static const float s_aShot[] = {0.6f, 1.0f, 1.4f};
	if(Shot < 0 || Shot > 2)
		return 0;
	return WardenTicks(s_aShot[Shot]);
}

// Mouth at the setup pose (the bubble muzzle and suction sink), facing right.
static const vec2 ANGLER_MOUTH(110.0f, 8.0f);

#endif
