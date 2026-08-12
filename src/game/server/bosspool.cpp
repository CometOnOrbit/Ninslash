#include <base/system.h>
#include <base/math.h>
#include <engine/shared/config.h>

#include <game/collision.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/entities/droid.h>
#include <game/server/entities/droid_bosscrawler.h>
#include <game/server/entities/droid_bossstar.h>
#include <game/server/entities/droid_bosswalker.h>
#include <game/server/entities/droid_bosssplitter.h>
#include <game/server/entities/droid_cyclonecrawler.h>
#include <game/server/entities/droid_kamikazestar.h>
#include <game/server/entities/droid_mendercrawler.h>
#include <game/server/entities/droid_railstar.h>
#include <game/server/entities/droid_siegebreakercrawler.h>
#include <game/server/entities/droid_splitcrawler.h>
#include <game/server/entities/droid_stalkercrawler.h>
#include <game/server/entities/droid_tempeststar.h>
#include <game/server/entities/droid_teslastar.h>

#include "bosspool.h"

namespace
{
// BossSplitter can grow into the same 96x128 collision box as BossStar. Keep
// a margin around that maximum so the boss can move after spawning.
const vec2 s_BossClearanceSize(112.0f, 144.0f);

bool TryBossLanding(CGameWorld *pWorld, vec2 Probe, vec2 *pOutPos)
{
	if(!pWorld || !pWorld->GameServer() || !pOutPos)
		return false;

	CCollision *pCollision = pWorld->GameServer()->Collision();
	const float WorldWidth = pCollision->GetWidth() * 32.0f;
	const float WorldHeight = pCollision->GetHeight() * 32.0f;
	if(Probe.x < 128.0f || Probe.x > WorldWidth - 128.0f || Probe.y < 32.0f || Probe.y > WorldHeight - 64.0f)
		return false;

	vec2 FloorPos;
	vec2 BeforeFloor;
	if(!pCollision->IntersectLine(Probe, Probe + vec2(0.0f, 448.0f), &FloorPos, &BeforeFloor, false, true))
		return false;

	const float HalfHeight = s_BossClearanceSize.y * 0.5f;
	const vec2 Landing(Probe.x, FloorPos.y - HalfHeight - 3.0f);
	if(Landing.y < HalfHeight || Landing.y > WorldHeight - HalfHeight)
		return false;

	// The center and both side positions must fit. Together they reserve about
	// eight tiles of horizontal space rather than accepting a body-sized pocket.
	static const float s_aMovementOffsets[] = {-64.0f, 0.0f, 64.0f};
	for(float Offset : s_aMovementOffsets)
	{
		const vec2 TestPos = Landing + vec2(Offset, 0.0f);
		if(pCollision->TestBox(TestPos, s_BossClearanceSize))
			return false;
		if(pCollision->IsInFluid(TestPos.x, TestPos.y) ||
		   pCollision->IsInFluid(TestPos.x, TestPos.y + HalfHeight - 4.0f))
			return false;
	}

	*pOutPos = Landing;
	return true;
}
} // namespace

int SelectBossType(int Depth)
{
	int Types[4];
	int Count = 0;
	Types[Count++] = DROIDTYPE_BOSSCRAWLER;
	if(Depth >= 5)
		Types[Count++] = DROIDTYPE_BOSSSTAR;
	if(Depth >= 8) // I like this one so... Uuu
		Types[Count++] = DROIDTYPE_BOSSSPLITTER;
	return Types[rand() % Count];
}

bool FindBossSpawnPosition(
	CGameWorld *pWorld, const vec2 *pSpawnPoints, int NumSpawnPoints, int *pRotation, vec2 *pOutPos)
{
	if(!pWorld || !pSpawnPoints || NumSpawnPoints <= 0 || !pOutPos)
		return false;

	const int Start = pRotation ? (*pRotation + 1 + NumSpawnPoints) % NumSpawnPoints : 0;
	static const float s_aHorizontalOffsets[] = {0.0f, -64.0f, 64.0f, -128.0f, 128.0f, -192.0f, 192.0f};
	static const float s_aVerticalOffsets[] = {-100.0f, -36.0f, -164.0f};
	for(int Attempt = 0; Attempt < NumSpawnPoints; Attempt++)
	{
		const int Index = (Start + Attempt) % NumSpawnPoints;
		for(float VerticalOffset : s_aVerticalOffsets)
		{
			for(float HorizontalOffset : s_aHorizontalOffsets)
			{
				const vec2 Probe = pSpawnPoints[Index] + vec2(HorizontalOffset, VerticalOffset);
				if(!TryBossLanding(pWorld, Probe, pOutPos))
					continue;
				if(pRotation)
					*pRotation = Index;
				return true;
			}
		}
	}
	return false;
}

CDroid *SpawnBoss(CGameWorld *pWorld, vec2 Pos, int Depth, int TypeHint)
{
	// Legacy callers pass a point above an enemy marker. Correct it to a safe,
	// settled landing when possible; mode controllers normally pass one already.
	vec2 SafePos;
	if(TryBossLanding(pWorld, Pos, &SafePos))
		Pos = SafePos;

	int Type = TypeHint;
	if(Type < 0 || !IsBossDroidType(Type))
		Type = SelectBossType(Depth);

	switch(Type)
	{
		case DROIDTYPE_BOSSSTAR:
			return new CBossStar(pWorld, Pos);
		case DROIDTYPE_BOSSSPLITTER:
			return new CBossSplitter(pWorld, Pos);
		default:
			return new CBossCrawler(pWorld, Pos);
	}
}

CDroid *SpawnSpecialist(CGameWorld *pWorld, vec2 Pos, int Type)
{
	switch(Type)
	{
		case DROIDTYPE_SIEGEBREAKERCRAWLER: return new CSiegeBreakerCrawler(pWorld, Pos);
		case DROIDTYPE_TEMPESTSTAR: return new CTempestStar(pWorld, Pos);
		case DROIDTYPE_SPLITCRAWLER: return new CSplitCrawler(pWorld, Pos);
		case DROIDTYPE_KAMIKAZESTAR: return new CKamikazeStar(pWorld, Pos);
		case DROIDTYPE_RAILSTAR: return new CRailstar(pWorld, Pos);
		case DROIDTYPE_MENDERCRAWLER: return new CMenderCrawler(pWorld, Pos);
		case DROIDTYPE_STALKERCRAWLER: return new CStalkerCrawler(pWorld, Pos);
		case DROIDTYPE_TESLASTAR: return new CTeslaStar(pWorld, Pos);
		case DROIDTYPE_CYCLONECRAWLER: return new CCycloneCrawler(pWorld, Pos);
		default: return 0;
	}
}

int DroidThreatCost(int Type)
{
	switch(Type)
	{
		case DROIDTYPE_SPLITCRAWLER:
		case DROIDTYPE_KAMIKAZESTAR:
			return 2;
		case DROIDTYPE_SIEGEBREAKERCRAWLER:
		case DROIDTYPE_TEMPESTSTAR:
		case DROIDTYPE_MENDERCRAWLER:
		case DROIDTYPE_STALKERCRAWLER:
			return 3;
		case DROIDTYPE_RAILSTAR:
		case DROIDTYPE_TESLASTAR:
		case DROIDTYPE_CYCLONECRAWLER:
			return 4;
		default:
			return 1;
	}
}

float DroidSoundThreat(int Type)
{
	switch(Type)
	{
		case DROIDTYPE_WALKER: return 2.5f;
		case DROIDTYPE_STAR: return 2.0f;
		case DROIDTYPE_CRAWLER: return 1.5f;
		case DROIDTYPE_BOSSCRAWLER: return 6.0f;
		case DROIDTYPE_BOSSSTAR: return 7.5f;
		case DROIDTYPE_BOSSWALKER: return 7.5f;
		case DROIDTYPE_BOSSSPLITTER: return 8.0f;
		case DROIDTYPE_SPLITCRAWLER:
		case DROIDTYPE_KAMIKAZESTAR:
			return 2.0f;
		case DROIDTYPE_SIEGEBREAKERCRAWLER:
		case DROIDTYPE_TEMPESTSTAR:
		case DROIDTYPE_MENDERCRAWLER:
		case DROIDTYPE_STALKERCRAWLER:
			return 2.5f;
		case DROIDTYPE_RAILSTAR:
		case DROIDTYPE_TESLASTAR:
		case DROIDTYPE_CYCLONECRAWLER:
			return 3.0f;
		default: return 1.0f;
	}
}

SThreatBudgetResult SpawnThreatBudgetSpecialists(CGameWorld *pWorld,
												 const vec2 *pSpawnPoints,
												 int NumSpawnPoints,
												 int *pRotation,
												 int Depth,
												 int OrdinaryThreat,
												 int MaxEntities,
												 int ThreatDivisor)
{
	SThreatBudgetResult Result = {0, 0};
	if(!pWorld || !pSpawnPoints || NumSpawnPoints <= 0 || OrdinaryThreat < 2 || MaxEntities <= 0 || Depth < 21)
		return Result;

	ThreatDivisor = max(1, ThreatDivisor);
	const int SpendLimit = min(OrdinaryThreat, max(2, OrdinaryThreat / ThreatDivisor));
	while(Result.m_EntitiesSpawned < MaxEntities && Result.m_ThreatSpent + 2 <= SpendLimit)
	{
		int aTypes[9];
		int NumTypes = 0;
		aTypes[NumTypes++] = DROIDTYPE_SIEGEBREAKERCRAWLER;
		aTypes[NumTypes++] = DROIDTYPE_SPLITCRAWLER;
		if(Depth >= 23)
		{
			aTypes[NumTypes++] = DROIDTYPE_TEMPESTSTAR;
			aTypes[NumTypes++] = DROIDTYPE_MENDERCRAWLER;
		}
		if(Depth >= 25)
		{
			aTypes[NumTypes++] = DROIDTYPE_KAMIKAZESTAR;
			aTypes[NumTypes++] = DROIDTYPE_STALKERCRAWLER;
		}
		if(Depth >= 27)
		{
			aTypes[NumTypes++] = DROIDTYPE_RAILSTAR;
			aTypes[NumTypes++] = DROIDTYPE_TESLASTAR;
			aTypes[NumTypes++] = DROIDTYPE_CYCLONECRAWLER;
		}

		const int Type = aTypes[rand() % NumTypes];
		const int Cost = DroidThreatCost(Type);
		if(Result.m_ThreatSpent + Cost > SpendLimit)
			break;

		const int Index = pRotation ? (*pRotation + 1 + NumSpawnPoints) % NumSpawnPoints :
			Result.m_EntitiesSpawned % NumSpawnPoints;
		if(pRotation)
			*pRotation = Index;
		if(!SpawnSpecialist(pWorld, pSpawnPoints[Index] + vec2(0.0f, -100.0f), Type))
			break;
		Result.m_ThreatSpent += Cost;
		Result.m_EntitiesSpawned++;
	}
	return Result;
}

// TODO
int CountAliveSpecialists(CGameWorld *pWorld)
{
	if(!pWorld)
		return 0;
	return 0 // For now.
	/*
	CDroid *apEnts[256];
	const int Num = pWorld->FindEntities(vec2(0, 0), 0.0f, (CEntity **)apEnts, 256, CGameWorld::ENTTYPE_DROID);
	int Specialists = 0;
	for(int i = 0; i < Num; i++)
	{
		if(!apEnts[i] || apEnts[i]->m_Health <= 0)
			continue;
		switch(apEnts[i]->m_Type)
		{
			case DROIDTYPE_SIEGEBREAKERCRAWLER:
			case DROIDTYPE_TEMPESTSTAR:
			case DROIDTYPE_SPLITCRAWLER:
			case DROIDTYPE_KAMIKAZESTAR:
			case DROIDTYPE_RAILSTAR:
			case DROIDTYPE_MENDERCRAWLER:
			case DROIDTYPE_STALKERCRAWLER:
			case DROIDTYPE_TESLASTAR:
			case DROIDTYPE_CYCLONECRAWLER:
				Specialists++;
				break;
		}
	}
	return Specialists;*/
}

int CountAliveBosses(CGameWorld *pWorld)
{
	if(!pWorld)
		return 0;
	CDroid *apEnts[256];
	int Num = pWorld->FindEntities(vec2(0, 0), 0.0f, (CEntity **)apEnts, 256, CGameWorld::ENTTYPE_DROID);
	int Bosses = 0;
	for(int i = 0; i < Num; i++)
	{
		if(apEnts[i] && IsBossDroidType(apEnts[i]->m_Type) && apEnts[i]->m_Health > 0)
			Bosses++;
	}
	return Bosses;
}
