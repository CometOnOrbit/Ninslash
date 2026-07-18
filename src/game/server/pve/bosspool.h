#ifndef GAME_SERVER_PVE_BOSSPOOL_H
#define GAME_SERVER_PVE_BOSSPOOL_H

#include <base/vmath.h>
#include <generated/protocol.h>

class CGameWorld;
class CDroid;

inline bool IsBossDroidType(int Type)
{
	return Type == DROIDTYPE_BOSSCRAWLER
		|| Type == DROIDTYPE_BOSSSTAR
		|| Type == DROIDTYPE_BOSSWALKER
		|| Type == DROIDTYPE_BOSSSPLITTER
		|| Type == DROIDTYPE_SIEGE_ENGINE
		|| Type == DROIDTYPE_OVERSEER_CORE;
}

// Depth unlocks more boss kinds (Invasion level / Horde wave).
int SelectBossType(int Depth);

// Pick a settled boss position from regular enemy spawn markers. The selected
// position has room for the largest boss body and a short movement lane.
bool FindBossSpawnPosition(CGameWorld *pWorld, const vec2 *pSpawnPoints, int NumSpawnPoints, int *pRotation, vec2 *pOutPos);

// Spawn one boss at Pos. TypeHint < 0 => random from pool.
CDroid *SpawnBoss(CGameWorld *pWorld, vec2 Pos, int Depth = 1, int TypeHint = -1);
CDroid *SpawnSpecialist(CGameWorld *pWorld, vec2 Pos, int Type);

// Shared threat accounting. Boss values are exposed for threat budgets,
// but bosses are never selected by ordinary-batch replacement.
int DroidThreatCost(int Type);

struct SThreatBudgetResult
{
	int m_ThreatSpent;
	int m_EntitiesSpawned;
};

// Replaces part of an ordinary-enemy batch with specialists. One ordinary
// enemy is one threat point; specialists cost 2 or 3 points but occupy only
// one of the batch's original concurrent slots.
// ThreatDivisor: spend OrdinaryThreat/ThreatDivisor on specialists (default 4 => ~25%).
// Small batches that cannot afford the specialist cost spawn none.
SThreatBudgetResult SpawnThreatBudgetSpecialists(CGameWorld *pWorld, const vec2 *pSpawnPoints,
	int NumSpawnPoints, int *pRotation, int Depth, int OrdinaryThreat, int MaxEntities, int ThreatDivisor = 4);

int CountAliveSpecialists(CGameWorld *pWorld);

int CountAliveBosses(CGameWorld *pWorld);

#endif
