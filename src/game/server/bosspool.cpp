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
#include <game/server/entities/droid_bulwark.h>
#include <game/server/entities/droid_assembler.h>
#include <game/server/entities/droid_saboteur.h>
#include <game/server/entities/droid_railgunner.h>
#include <game/server/entities/droid_siege_engine.h>
#include <game/server/entities/droid_overseer_core.h>

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
	// Classic bosses only — Lost Protocol Siege/Overseer stay disabled while
	// that kit is still rough.
	int Types[4];
	int Count = 0;
	Types[Count++] = DROIDTYPE_BOSSCRAWLER;
	if(Depth >= 5)
		Types[Count++] = DROIDTYPE_BOSSSTAR;
	if(Depth >= 15)
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
	// Remap unfinished Lost Protocol bosses onto the classic pool.
	if(Type == DROIDTYPE_SIEGE_ENGINE || Type == DROIDTYPE_OVERSEER_CORE)
		Type = -1;
	if(Type < 0 || !IsBossDroidType(Type) || Type == DROIDTYPE_SIEGE_ENGINE || Type == DROIDTYPE_OVERSEER_CORE)
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
	// ponytail: Lost Protocol specialists (Bulwark/Assembler/Saboteur/Railgunner)
	// are too rough to ship in ordinary spawns. Re-enable when the kit is ready.
	/*
	switch(Type)
	{
	case DROIDTYPE_BULWARK: return new CBulwark(pWorld, Pos);
	case DROIDTYPE_ASSEMBLER: return new CAssembler(pWorld, Pos);
	case DROIDTYPE_SABOTEUR: return new CSaboteur(pWorld, Pos);
	case DROIDTYPE_RAILGUNNER: return new CRailgunner(pWorld, Pos);
	default: return 0;
	}
	*/
	(void)pWorld;
	(void)Pos;
	(void)Type;
	return 0;
}

int DroidThreatCost(int Type)
{
	switch(Type)
	{
		case DROIDTYPE_RAILGUNNER:
		case DROIDTYPE_SABOTEUR:
			return 3;
		case DROIDTYPE_BULWARK:
		case DROIDTYPE_ASSEMBLER:
			return 4;
		case DROIDTYPE_SIEGE_ENGINE:
		case DROIDTYPE_OVERSEER_CORE:
			return 10;
		default:
			return 1;
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
	// Lost Protocol specialist replacement is disabled (see SpawnSpecialist).
	(void)pWorld;
	(void)pSpawnPoints;
	(void)NumSpawnPoints;
	(void)pRotation;
	(void)Depth;
	(void)OrdinaryThreat;
	(void)MaxEntities;
	(void)ThreatDivisor;
	SThreatBudgetResult Result = {0, 0};
	/*
	if(!pWorld || !pSpawnPoints || NumSpawnPoints <= 0 || OrdinaryThreat < 2 || MaxEntities <= 0)
		return Result;

	// Spend roughly OrdinaryThreat/ThreatDivisor of each ordinary batch on specialists.
	// Unlock support units gradually so early runs cannot roll a three-point wall.
	ThreatDivisor = max(1, ThreatDivisor);
	const int SpendLimit = min(OrdinaryThreat, max(2, OrdinaryThreat / ThreatDivisor));
	while(Result.m_EntitiesSpawned < MaxEntities && Result.m_ThreatSpent + 2 <= SpendLimit)
	{
		int aTypes[4];
		int NumTypes = 0;
		aTypes[NumTypes++] = DROIDTYPE_RAILGUNNER;
		if(Depth >= 3)
			aTypes[NumTypes++] = DROIDTYPE_SABOTEUR;
		if(Depth >= 5)
			aTypes[NumTypes++] = DROIDTYPE_BULWARK;
		if(Depth >= 7)
			aTypes[NumTypes++] = DROIDTYPE_ASSEMBLER;

		int Type = aTypes[rand() % NumTypes];
		int Cost = DroidThreatCost(Type);
		if(Result.m_ThreatSpent + Cost > SpendLimit)
		{
			Type = DROIDTYPE_RAILGUNNER;
			Cost = 2;
		}
		if(Result.m_ThreatSpent + Cost > SpendLimit)
			break;

		const int Index = pRotation ? (*pRotation + 1 + NumSpawnPoints) % NumSpawnPoints : Result.m_EntitiesSpawned %
	NumSpawnPoints; if(pRotation) *pRotation = Index; if(!SpawnSpecialist(pWorld, pSpawnPoints[Index] + vec2(0.0f,
	-100.0f), Type)) break; Result.m_ThreatSpent += Cost; Result.m_EntitiesSpawned++;
	}
	*/
	return Result;
}

int CountAliveSpecialists(CGameWorld *pWorld)
{
	if(!pWorld)
		return 0;
	CDroid *apEnts[256];
	const int Num = pWorld->FindEntities(vec2(0, 0), 0.0f, (CEntity **)apEnts, 256, CGameWorld::ENTTYPE_DROID);
	int Specialists = 0;
	for(int i = 0; i < Num; i++)
		if(apEnts[i] && apEnts[i]->m_Health > 0 &&
		   (apEnts[i]->m_Type == DROIDTYPE_BULWARK || apEnts[i]->m_Type == DROIDTYPE_ASSEMBLER ||
			apEnts[i]->m_Type == DROIDTYPE_SABOTEUR || apEnts[i]->m_Type == DROIDTYPE_RAILGUNNER))
			Specialists++;
	return Specialists;
}

int CountAliveBosses(CGameWorld *pWorld)
{
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
