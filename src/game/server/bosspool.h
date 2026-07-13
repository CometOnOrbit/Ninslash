#ifndef GAME_SERVER_BOSSPOOL_H
#define GAME_SERVER_BOSSPOOL_H

#include <base/vmath.h>
#include <generated/protocol.h>

class CGameWorld;
class CDroid;

inline bool IsBossDroidType(int Type)
{
	return Type == DROIDTYPE_BOSSCRAWLER
		|| Type == DROIDTYPE_BOSSSTAR
		|| Type == DROIDTYPE_BOSSWALKER
		|| Type == DROIDTYPE_BOSSSPLITTER;
}

// Depth unlocks more boss kinds (Invasion level / Horde wave).
int SelectBossType(int Depth);

// Spawn one boss at Pos. TypeHint < 0 => random from pool.
CDroid *SpawnBoss(CGameWorld *pWorld, vec2 Pos, int Depth = 1, int TypeHint = -1);

int CountAliveBosses(CGameWorld *pWorld);

#endif
