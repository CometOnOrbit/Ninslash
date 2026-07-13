#include <base/system.h>
#include <base/math.h>

#include <game/server/gameworld.h>
#include <game/server/entities/droid.h>
#include <game/server/entities/droid_bosscrawler.h>
#include <game/server/entities/droid_bossstar.h>
#include <game/server/entities/droid_bosswalker.h>
#include <game/server/entities/droid_bosssplitter.h>

#include "bosspool.h"

int SelectBossType(int Depth)
{
	// Always available
	int Types[4];
	int Count = 0;
	Types[Count++] = DROIDTYPE_BOSSCRAWLER;
	if(Depth >= 5)
		Types[Count++] = DROIDTYPE_BOSSSTAR;
	if(Depth >= 10)
		Types[Count++] = DROIDTYPE_BOSSWALKER;
	if(Depth >= 15)
		Types[Count++] = DROIDTYPE_BOSSSPLITTER;
	return Types[rand() % Count];
}

CDroid *SpawnBoss(CGameWorld *pWorld, vec2 Pos, int Depth, int TypeHint)
{
	int Type = TypeHint;
	if(Type < 0 || !IsBossDroidType(Type))
		Type = SelectBossType(Depth);

	switch(Type)
	{
	case DROIDTYPE_BOSSSTAR:
		return new CBossStar(pWorld, Pos);
	case DROIDTYPE_BOSSWALKER:
		return new CBossWalker(pWorld, Pos);
	case DROIDTYPE_BOSSSPLITTER:
		return new CBossSplitter(pWorld, Pos);
	default:
		return new CBossCrawler(pWorld, Pos);
	}
}

int CountAliveBosses(CGameWorld *pWorld)
{
	CDroid *apEnts[256];
	int Num = pWorld->FindEntities(vec2(0, 0), 0.0f, (CEntity**)apEnts, 256, CGameWorld::ENTTYPE_DROID);
	int Bosses = 0;
	for(int i = 0; i < Num; i++)
	{
		if(apEnts[i] && IsBossDroidType(apEnts[i]->m_Type) && apEnts[i]->m_Health > 0)
			Bosses++;
	}
	return Bosses;
}
