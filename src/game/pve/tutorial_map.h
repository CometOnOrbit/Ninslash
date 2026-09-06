#ifndef GAME_TUTORIAL_MAP_H
#define GAME_TUTORIAL_MAP_H

#include <game/mapitems.h>
#include <game/pve/tutorial.h>

enum
{
	TUTORIAL_MAP_W = 48,
	TUTORIAL_MAP_H = 28,
	TUTORIAL_MAP_HALL_X0 = 2,
	TUTORIAL_MAP_HALL_Y0 = 6,
	TUTORIAL_MAP_WALK_Y = 18,
	TUTORIAL_MAP_MAX_STAMPS = 24
};

struct CTutorialStamp
{
	int m_X;
	int m_Y;
	int m_Entity;
};

inline bool TutorialHallAir(int x, int y, int W, int H)
{
	return x >= TUTORIAL_MAP_HALL_X0 && x < W - 2 && y >= TUTORIAL_MAP_HALL_Y0 && y <= TUTORIAL_MAP_WALK_Y && y < H;
}

inline void TutorialCarveHall(unsigned char *pSolid, int W, int H)
{
	if(!pSolid || W < 8 || H < 8)
		return;
	for(int y = 0; y < H; y++)
		for(int x = 0; x < W; x++)
			pSolid[y * W + x] = TutorialHallAir(x, y, W, H) ? 0 : 1;
}

inline int TutorialMapPush(CTutorialStamp *pOut, int Max, int N, int X, int Entity)
{
	if(!pOut || N < 0 || N >= Max)
		return N;
	pOut[N].m_X = X;
	pOut[N].m_Y = TUTORIAL_MAP_WALK_Y;
	pOut[N].m_Entity = Entity;
	return N + 1;
}

// It wasn't until after I finished writing this that I remembered I could use the map editor...
inline int TutorialMapStamps(int Chapter, CTutorialStamp *pOut, int Max)
{
	int N = 0;
	N = TutorialMapPush(pOut, Max, N, 5, ENTITY_SPAWN);
	N = TutorialMapPush(pOut, Max, N, 7, ENTITY_SPAWN);
	if(Chapter == TUTORIAL_CHAPTER_DEPLOYMENT)
	{
		N = TutorialMapPush(pOut, Max, N, 14, ENTITY_RANDOM_WEAPON);
		N = TutorialMapPush(pOut, Max, N, 20, ENTITY_RANDOM_WEAPON);
		N = TutorialMapPush(pOut, Max, N, 28, ENTITY_ENEMYSPAWN);
	}
	else if(Chapter == TUTORIAL_CHAPTER_COMBAT)
	{
		N = TutorialMapPush(pOut, Max, N, 12, ENTITY_HEALTH_1);
		N = TutorialMapPush(pOut, Max, N, 14, ENTITY_HEALTH_1);
		N = TutorialMapPush(pOut, Max, N, 16, ENTITY_HEALTH_1);
		N = TutorialMapPush(pOut, Max, N, 20, ENTITY_AMMO_1);
		N = TutorialMapPush(pOut, Max, N, 22, ENTITY_AMMO_1);
		N = TutorialMapPush(pOut, Max, N, 24, ENTITY_AMMO_1);
		N = TutorialMapPush(pOut, Max, N, 28, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 32, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 36, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 40, ENTITY_ENEMYSPAWN);
	}
	else if(Chapter == TUTORIAL_CHAPTER_OBJECTIVES)
	{
		N = TutorialMapPush(pOut, Max, N, 10, ENTITY_SWITCH);
		N = TutorialMapPush(pOut, Max, N, 18, ENTITY_SWITCH);
		N = TutorialMapPush(pOut, Max, N, 26, ENTITY_SWITCH);
		N = TutorialMapPush(pOut, Max, N, 34, ENTITY_SWITCH);
	}
	else if(Chapter == TUTORIAL_CHAPTER_FORGE)
	{
		N = TutorialMapPush(pOut, Max, N, 12, ENTITY_KIT);
		N = TutorialMapPush(pOut, Max, N, 15, ENTITY_KIT);
		N = TutorialMapPush(pOut, Max, N, 18, ENTITY_KIT);
		N = TutorialMapPush(pOut, Max, N, 21, ENTITY_KIT);
		N = TutorialMapPush(pOut, Max, N, 24, ENTITY_AMMO_1);
		N = TutorialMapPush(pOut, Max, N, 27, ENTITY_AMMO_1);
		N = TutorialMapPush(pOut, Max, N, 30, ENTITY_ARMOR_1);
		N = TutorialMapPush(pOut, Max, N, 33, ENTITY_ARMOR_1);
		N = TutorialMapPush(pOut, Max, N, 36, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 39, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 41, ENTITY_ENEMYSPAWN);
	}
	else if(Chapter == TUTORIAL_CHAPTER_BUILD)
	{
		N = TutorialMapPush(pOut, Max, N, 12, ENTITY_KIT);
		N = TutorialMapPush(pOut, Max, N, 16, ENTITY_KIT);
		N = TutorialMapPush(pOut, Max, N, 20, ENTITY_KIT);
		N = TutorialMapPush(pOut, Max, N, 28, ENTITY_POWERUPPER);
	}
	else if(Chapter == TUTORIAL_CHAPTER_MULTIPLAYER)
	{
		N = TutorialMapPush(pOut, Max, N, 16, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 20, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 24, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 28, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 32, ENTITY_ENEMYSPAWN);
		N = TutorialMapPush(pOut, Max, N, 36, ENTITY_ENEMYSPAWN);
	}
	N = TutorialMapPush(pOut, Max, N, 44, ENTITY_DOOR1);
	return N;
}

inline int TutorialMapCountEntity(const CTutorialStamp *pStamps, int N, int Entity)
{
	int Count = 0;
	if(!pStamps)
		return 0;
	for(int i = 0; i < N; i++)
		if(pStamps[i].m_Entity == Entity)
			Count++;
	return Count;
}

#endif
