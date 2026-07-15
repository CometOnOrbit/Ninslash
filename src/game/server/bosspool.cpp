#include <base/system.h>
#include <base/math.h>

#include <game/collision.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/entities/droid.h>
#include <game/server/entities/droid_bosscrawler.h>
#include <game/server/entities/droid_bossstar.h>
#include <game/server/entities/droid_bosswalker.h>
#include <game/server/entities/droid_bosssplitter.h>

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
}

int SelectBossType(int Depth)
{
	// Always available
	int Types[4];
	int Count = 0;
	Types[Count++] = DROIDTYPE_BOSSCRAWLER;
	if(Depth >= 5)
		Types[Count++] = DROIDTYPE_BOSSSTAR;
	if(Depth >= 15)
		Types[Count++] = DROIDTYPE_BOSSSPLITTER;
	return Types[rand() % Count];
}

bool FindBossSpawnPosition(CGameWorld *pWorld, const vec2 *pSpawnPoints, int NumSpawnPoints, int *pRotation, vec2 *pOutPos)
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
