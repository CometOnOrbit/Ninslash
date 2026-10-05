#ifndef GAME_BOSS_V5_H
#define GAME_BOSS_V5_H

// Shared helpers for the v5 bosses that took over the old industrial slots:
//   BULKHEAD (20) Bastion Strider, ARC (21) Storm Seraph, VAULT (22) Siege Monolith.
// (RAIL (19) is the Skitter Matriarch, see skitter_matriarch.h.)
// Units: world pixels, 50 ticks per second, y down, layouts face right (+x).

#include <math.h>
#include <generated/protocol.h>

static const int BOSSV5_TICK_SPEED = 50;
static const int BOSSV5_MAX_SHOTS = 24;
static const int BOSSV5_NUM_PARTS = 4; // part 0 is always the core

// BossShot kinds 2..9 used to belong to the retired industrial rigs; the v5 bosses own them now.
enum
{
	STRIDER_SHOT_WAVE = 2,	 // ground shockwave, VelX = direction*100, VelY = life left
	STRIDER_SHOT_SHELL = 3,	 // mortar shell
	SERAPH_SHOT_ORB = 4,	 // homing ion orb
	SERAPH_SHOT_NODE = 5,	 // tesla node; VelX = node state, VelY = node index
	SERAPH_SHOT_SPARK = 6,	 // ground lightning crawling away from a plunge, VelX = direction*100
	MONOLITH_SHOT_BOMB = 7,	 // carpet bomb
	MONOLITH_SHOT_SHELL = 8, // mortar shell
	MONOLITH_SHOT_BLAST = 9, // impact marker (purely visual), VelY = age
};

inline int BossV5Ticks(float Seconds) { return (int)(Seconds * BOSSV5_TICK_SPEED + 0.5f); }

inline int BossV5Phase(int Health, int MaxHealth)
{
	if(Health * 3 <= MaxHealth)
		return 2;
	if(Health * 3 <= MaxHealth * 2)
		return 1;
	return 0;
}

inline float BossV5Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Deeper floors get tougher bosses: hull health and outgoing damage scale with sv_mapgen_level.
inline float BossV5DepthHealth(int Level) { return BossV5Clamp(1.0f + 0.02f * (Level - 20), 1.0f, 2.0f); }
inline float BossV5DepthDamage(int Level) { return BossV5Clamp(1.0f + 0.01f * (Level - 20), 1.0f, 1.6f); }
// Outgoing damage also ramps with the phase (enrage).
inline float BossV5PhaseDamage(int Phase) { return Phase >= 2 ? 1.25f : (Phase >= 1 ? 1.12f : 1.0f); }
// Plating on the hull while the weak point is closed (parts and exposed windows bypass it).
inline float BossV5Armor(int Phase) { return Phase >= 2 ? 0.68f : 0.75f; }
// Recent hull damage (decays 2.5% per tick) that makes a boss counter-attack instead of soaking.
static const float BOSSV5_PRESSURE_DECAY = 0.975f;
static const float BOSSV5_PRESSURE_TRIGGER = 0.035f; // fraction of max health

// Launch velocity that lands on (Dx, Dy) after T ticks under Gravity, capped at MaxSpeed.
inline void BossV5Ballistic(float Dx, float Dy, float T, float Gravity, float MaxSpeed, float *pVx, float *pVy)
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

struct CBossV5Act
{
	const char *m_pName;
	int m_Anim;		  // DROIDANIM_* carried in the droid snapshot
	float m_Duration; // seconds; open-ended acts treat it as a safety cap
	bool m_Loop;
};

// Every act of a boss maps to a distinct DROIDANIM so the client can recover it from the snapshot.
inline int BossV5ActFromAnim(const CBossV5Act *pActs, int NumActs, int DeathAct, int Anim, int Status)
{
	if(Status == DROIDSTATUS_TERMINATED)
		return DeathAct;
	for(int i = 0; i < NumActs; i++)
		if(i != DeathAct && pActs[i].m_Anim == Anim)
			return i;
	return 0;
}

struct CBossV5Hit
{
	int m_Part;
	float m_X, m_Y, m_R; // facing right, relative to the body center
};

// Dx already in facing space (world dx * dir). Parts win near-ties over the core.
inline int BossV5PartAt(const CBossV5Hit *pHits, int Num, float Dx, float Dy)
{
	int Best = 0;
	float BestDist = 36.0f;
	for(int i = 0; i < Num; i++)
	{
		const float x = Dx - pHits[i].m_X;
		const float y = Dy - pHits[i].m_Y;
		const float d = sqrtf(x * x + y * y) - pHits[i].m_R;
		const float Bias = pHits[i].m_Part == 0 ? 6.0f : 0.0f;
		if(d + Bias < BestDist)
		{
			BestDist = d + Bias;
			Best = pHits[i].m_Part;
		}
	}
	return Best;
}

// Droid snapshot m_Angle carries boss flags for the v5 bosses (they never aim with it).
enum
{
	BOSSV5_FLAG_EXPOSED = 1, // weak point open
	BOSSV5_FLAG_AIR = 2,	 // monolith air form / seraph grounded inverted
	BOSSV5_FLAG_BEAM = 4,	 // monolith laser live
	BOSSV5_FLAG_THRUST = 8,	 // strider charge thrusters / seraph dive
};

#endif
