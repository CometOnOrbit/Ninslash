

#include "gameworld.h"
#include "entity.h"
#include "gamecontext.h"
#include "entities/turret.h"
#include "entities/weapon.h"
#include "entities/ball.h"
#include "entities/building.h"
#include "entities/droid.h"

#include <game/weapons.h>
#include <engine/shared/config.h>

static float DistanceSquared(vec2 A, vec2 B)
{
	const vec2 Delta = A - B;
	return dot(Delta, Delta);
}

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
CGameWorld::CGameWorld()
{
	m_pGameServer = 0x0;
	m_pServer = 0x0;

	m_Paused = false;
	m_ResetRequested = false;
	m_HasPendingDestroy = false;
	for(int i = 0; i < NUM_ENTTYPES; i++)
		m_apFirstEntityTypes[i] = 0;
}

CGameWorld::~CGameWorld()
{
	// delete all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		while(m_apFirstEntityTypes[i])
			delete m_apFirstEntityTypes[i];
}

void CGameWorld::SetGameServer(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_pServer = m_pGameServer->Server();
}

int CGameWorld::FindEntities(vec2 Pos, float Radius, CEntity **ppEnts, int Max, int Type)
{
	if(Type < 0 || Type >= NUM_ENTTYPES)
		return 0;

	// linear scan per entity type; entity counts are small enough for this to be fine
	vec2 OPos = Pos;

	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[Type]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(Type == CGameWorld::ENTTYPE_DROID)
			Pos = OPos + pEnt->m_Center;

		const float CollisionRange = Radius + pEnt->m_ProximityRadius;
		const float CollisionRangeSquared = CollisionRange * CollisionRange;
		const vec2 BodyDelta = pEnt->m_Pos - Pos;
		// circle body collision
		if(Radius <= 0.0f || dot(BodyDelta, BodyDelta) < CollisionRangeSquared ||
		   // head collision if character
		   (Type == CGameWorld::ENTTYPE_CHARACTER &&
			DistanceSquared(pEnt->m_Pos + vec2(0, -27), Pos) < CollisionRangeSquared))
		{
			if(ppEnts)
				ppEnts[Num] = pEnt;
			Num++;
			if(Num == Max)
				break;
		}
	}

	return Num;
}

int CGameWorld::FindBlocks(vec2 Pos, ivec2 Radius, CEntity **ppEnts, int Max)
{
	int Num = 0;
	for(CEntity *pEnt = m_apFirstEntityTypes[CGameWorld::ENTTYPE_BLOCK]; pEnt; pEnt = pEnt->m_pNextTypeEntity)
	{
		if(abs(pEnt->m_Pos.x - Pos.x) < Radius.x && abs(pEnt->m_Pos.y - Pos.y) < Radius.y)
		{
			if(ppEnts)
				ppEnts[Num] = pEnt;
			Num++;
			if(Num == Max)
				break;
		}
	}

	return Num;
}

void CGameWorld::InsertEntity(CEntity *pEnt)
{
#ifdef CONF_DEBUG
	for(CEntity *pCur = m_apFirstEntityTypes[pEnt->m_ObjType]; pCur; pCur = pCur->m_pNextTypeEntity)
		dbg_assert(pCur != pEnt, "err");
#endif

	// insert it
	if(m_apFirstEntityTypes[pEnt->m_ObjType])
		m_apFirstEntityTypes[pEnt->m_ObjType]->m_pPrevTypeEntity = pEnt;
	pEnt->m_pNextTypeEntity = m_apFirstEntityTypes[pEnt->m_ObjType];
	pEnt->m_pPrevTypeEntity = 0x0;
	m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt;
}

void CGameWorld::DestroyEntity(CEntity *pEnt)
{
	pEnt->m_MarkedForDestroy = true;
	m_HasPendingDestroy = true;
}

void CGameWorld::RemoveEntity(CEntity *pEnt)
{
	// not in the list
	if(!pEnt->m_pNextTypeEntity && !pEnt->m_pPrevTypeEntity && m_apFirstEntityTypes[pEnt->m_ObjType] != pEnt)
		return;

	// remove
	if(pEnt->m_pPrevTypeEntity)
		pEnt->m_pPrevTypeEntity->m_pNextTypeEntity = pEnt->m_pNextTypeEntity;
	else
		m_apFirstEntityTypes[pEnt->m_ObjType] = pEnt->m_pNextTypeEntity;
	if(pEnt->m_pNextTypeEntity)
		pEnt->m_pNextTypeEntity->m_pPrevTypeEntity = pEnt->m_pPrevTypeEntity;

	// keep list traversing valid
	if(m_pNextTraverseEntity == pEnt)
		m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;

	pEnt->m_pNextTypeEntity = 0;
	pEnt->m_pPrevTypeEntity = 0;
}

//
void CGameWorld::Snap(int SnappingClient)
{
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Snap(SnappingClient);
			pEnt = m_pNextTraverseEntity;
		}
}

void CGameWorld::Reset()
{
	// reset all entities
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			pEnt->Reset();
			pEnt = m_pNextTraverseEntity;
		}
	RemoveEntities();

	GameServer()->m_pController->PostReset();
	RemoveEntities();

	m_ResetRequested = false;
}

void CGameWorld::RemoveEntities()
{
	if(!m_HasPendingDestroy)
		return;
	m_HasPendingDestroy = false;

	// destroy objects marked for destruction
	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			if(pEnt->m_MarkedForDestroy)
			{
				RemoveEntity(pEnt);
				pEnt->Destroy();
			}
			pEnt = m_pNextTraverseEntity;
		}
}

int CGameWorld::CountEntities()
{
	int Entities = 0;

	for(int i = 0; i < NUM_ENTTYPES; i++)
		for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
		{
			m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
			Entities++;
			pEnt = m_pNextTraverseEntity;
		}

	return Entities;
}

void CGameWorld::Tick()
{
	if(m_ResetRequested)
		Reset();

	if(!m_Paused)
	{
		if(GameServer()->m_pController->IsForceBalanced())
			GameServer()->SendChatTarget(-1, "Teams have been balanced");
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
		{
			if(i == ENTTYPE_CHARACTER)
				m_Core.ClearDroidHookImpacts();

			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->Tick();
				pEnt = m_pNextTraverseEntity;
			}
		}

		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickDefered();
				pEnt = m_pNextTraverseEntity;
			}
	}
	else
	{
		// update all objects
		for(int i = 0; i < NUM_ENTTYPES; i++)
			for(CEntity *pEnt = m_apFirstEntityTypes[i]; pEnt;)
			{
				m_pNextTraverseEntity = pEnt->m_pNextTypeEntity;
				pEnt->TickPaused();
				pEnt = m_pNextTraverseEntity;
			}
	}

	RemoveEntities();
}

bool CGameWorld::IsShielded(vec2 Pos0, vec2 Pos1, float Radius, int Team)
{
	CWeapon *w = (CWeapon *)FindFirst(ENTTYPE_WEAPON);
	for(; w; w = (CWeapon *)w->TypeNext())
	{
		if(!w->m_Disabled && WeaponHasBehavior(w->GetWeaponProfile().m_Definition, WEAPON_BEHAVIOR_AREA_SHIELD))
		{

			const vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, w->m_Pos);
			const float ShieldRange = 180.0f + Radius;
			const float ShieldRangeSquared = ShieldRange * ShieldRange;
			const vec2 ShieldPos = w->m_Pos + w->m_Center;
			if(DistanceSquared(ShieldPos, IntersectPos) < ShieldRangeSquared &&
			   DistanceSquared(ShieldPos, Pos0) >= ShieldRangeSquared)
			{
				return true;
			}
		}
	}

	CBuilding *pBuilding = (CBuilding *)FindFirst(ENTTYPE_BUILDING);
	for(; pBuilding; pBuilding = (CBuilding *)pBuilding->TypeNext())
	{
		if(!pBuilding->IsNodesBuilding() || pBuilding->NodesType() != NODES_SHIELD || !pBuilding->NodesAlive() ||
			!pBuilding->NodesPower() || pBuilding->m_Team == Team)
			continue;
		const vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, pBuilding->m_Pos);
		const float ShieldRange = 180.0f + Radius;
		if(DistanceSquared(pBuilding->m_Pos, IntersectPos) < ShieldRange * ShieldRange &&
			DistanceSquared(pBuilding->m_Pos, Pos0) >= ShieldRange * ShieldRange)
			return true;
	}

	return false;
}

CBuilding *CGameWorld::IntersectBuilding(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, int Team, CEntity *pNotThis)
{
	float ClosestLenSquared = DistanceSquared(Pos0, Pos1) * 10000.0f;
	CBuilding *pClosest = 0;

	CBuilding *p = (CBuilding *)FindFirst(ENTTYPE_BUILDING);
	for(; p; p = (CBuilding *)p->TypeNext())
	{
		if(p == pNotThis || !p->m_Collision)
			continue;

		// if (!GameServer()->m_pController->IsTeamplay())
		//	continue;

		/*
		if (GameServer()->m_pController->IsTeamplay())
		{
			if (Team == p->m_Team)
				continue;
		}
		else
		{
			if (Team ==
		}
		*/

		if(Team == p->m_Team)
			continue;

		// if (p->m_Team >= 0)
		//	continue;

		if(GameServer()->m_pController->IsCoop() && Team >= 0 && p->m_Team >= 0 &&
		   (p->m_Type == BUILDING_TURRET || p->m_Type == BUILDING_GENERATOR || p->m_Type == BUILDING_TESLACOIL ||
			p->m_Type == BUILDING_REACTOR))
		{
			if(Team >= 0 && Team < MAX_CLIENTS)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[Team];

				if(pPlayer && !pPlayer->m_IsBot)
					continue;
			}
		}

		/*
				// co-op player to player collisiong ignore
		if (g_Config.m_SvDisablePVP && !p->m_IsBot)
		{
			if (pNotThis && pNotThis->GetType() != CGameWorld::ENTTYPE_CHARACTER)
				continue;

			if (pNotThis && pNotThis->GetType() == CGameWorld::ENTTYPE_CHARACTER)
			{
				CCharacter *pOwnerChar = (CCharacter *)pNotThis;
				if (!pOwnerChar->m_IsBot)
					continue;
			}
		}
		*/

		const vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
		const vec2 Center = p->m_Pos + p->m_Center;
		const float CenterDistanceSquared = DistanceSquared(Center, IntersectPos);
		const float CollisionRange = p->m_ProximityRadius + Radius;
		const float GeneratorRange = 240.0f + Radius;
		if(CenterDistanceSquared < CollisionRange * CollisionRange ||
		   (p->m_Type == BUILDING_GENERATOR && CenterDistanceSquared < GeneratorRange * GeneratorRange &&
			DistanceSquared(Center, Pos0) >= GeneratorRange * GeneratorRange))
		{
			const float AlongSegmentSquared = DistanceSquared(Pos0, IntersectPos);
			if(AlongSegmentSquared < ClosestLenSquared)
			{
				NewPos = IntersectPos;
				ClosestLenSquared = AlongSegmentSquared;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

CBall *CGameWorld::IntersectBall(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos)
{
	if(!GameServer()->m_pController->m_pBall)
		return 0;

	CBall *pBall = GameServer()->m_pController->m_pBall;

	const vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, pBall->m_Pos);
	const float CollisionRange = pBall->m_ProximityRadius + Radius;
	if(DistanceSquared(pBall->m_Pos, IntersectPos) < CollisionRange * CollisionRange)
		return pBall;

	return 0;
}

CDroid *CGameWorld::IntersectWalker(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, CEntity *pNotThis)
{
	float ClosestLenSquared = DistanceSquared(Pos0, Pos1) * 10000.0f;
	CDroid *pClosest = 0;

	CDroid *p = (CDroid *)FindFirst(ENTTYPE_DROID);
	for(; p; p = (CDroid *)p->TypeNext())
	{
		if(p == pNotThis || p->m_Health <= 0)
			continue;

		const vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
		const float CollisionRange = p->m_ProximityRadius + Radius;
		if(DistanceSquared(p->m_Pos + p->m_Center, IntersectPos) < CollisionRange * CollisionRange)
		{
			const float AlongSegmentSquared = DistanceSquared(Pos0, IntersectPos);
			if(AlongSegmentSquared < ClosestLenSquared)
			{
				NewPos = IntersectPos;
				ClosestLenSquared = AlongSegmentSquared;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

bool CGameWorld::GetDroidPosChange(int ID)
{
	CDroid *p = (CDroid *)FindFirst(ENTTYPE_DROID);
	for(; p; p = (CDroid *)p->TypeNext())
	{
		if(p->m_ID == ID)
		{
			return true;
		}
	}

	return false;
}

// line-segment vs. character hit test (body + head)
CCharacter *CGameWorld::IntersectCharacter(vec2 Pos0,
										   vec2 Pos1,
										   float Radius,
										   vec2 &NewPos,
										   CEntity *pNotThis,
										   bool IgnoreDeathrayed,
										   CCharacter **ppReflect,
										   float ReflectRadius,
										   CEntity *pNotThis2)
{
	// Find other players
	float ClosestLenSquared = DistanceSquared(Pos0, Pos1) * 10000.0f;
	CCharacter *pClosest = 0;
	vec2 ClosestPos = NewPos;
	float ClosestReflectLenSquared = ClosestLenSquared;
	CCharacter *pClosestReflect = 0;
	vec2 ClosestReflectPos = NewPos;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(p == pNotThis || p == pNotThis2)
			continue;

		if(p->IgnoreCollision())
			continue;

		if(IgnoreDeathrayed && p->Deathrayed())
			continue;

		// co-op player to player collisiong ignore
		if(g_Config.m_SvDisablePVP && !p->m_IsBot)
		{
			if(pNotThis && pNotThis->GetType() != CGameWorld::ENTTYPE_CHARACTER)
				continue;

			if(pNotThis && pNotThis->GetType() == CGameWorld::ENTTYPE_CHARACTER)
			{
				CCharacter *pOwnerChar = (CCharacter *)pNotThis;
				if(!pOwnerChar->m_IsBot)
					continue;
			}
		}

		const vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
		const float AlongSegmentSquared = DistanceSquared(Pos0, IntersectPos);
		if(ppReflect)
		{
			const int Reflect = p->Reflect();
			if(Reflect > 0)
			{
				const float ReflectRange = Reflect + ReflectRadius;
				if(DistanceSquared(p->m_Pos + vec2(0, -32), IntersectPos) < ReflectRange * ReflectRange)
				{
					if(AlongSegmentSquared < ClosestReflectLenSquared)
					{
						ClosestReflectPos = IntersectPos;
						ClosestReflectLenSquared = AlongSegmentSquared;
						pClosestReflect = p;
					}
				}
			}
		}

		const float BodyRange = p->m_ProximityRadius + Radius + p->m_ShieldRadius;
		if(DistanceSquared(p->m_Pos, IntersectPos) < BodyRange * BodyRange)
		{
			if(AlongSegmentSquared < ClosestLenSquared)
			{
				ClosestPos = IntersectPos;
				ClosestLenSquared = AlongSegmentSquared;
				pClosest = p;
			}
		}
		// head shot
		const float HeadRange = p->m_ProximityRadius + Radius;
		if(DistanceSquared(p->m_Pos + vec2(0, -28), IntersectPos) < HeadRange * HeadRange)
		{
			if(AlongSegmentSquared < ClosestLenSquared)
			{
				ClosestPos = IntersectPos;
				ClosestLenSquared = AlongSegmentSquared;
				pClosest = p;
			}
		}
	}

	if(ppReflect)
	{
		*ppReflect = pClosestReflect;
		if(pClosestReflect)
		{
			NewPos = ClosestReflectPos;
			return 0;
		}
	}
	if(pClosest)
		NewPos = ClosestPos;
	return pClosest;
}

CCharacter *CGameWorld::GetFriendlyCharacterInBox(vec2 TopLeft, vec2 BotRight, int Team)
{
	vec2 Center = (TopLeft + BotRight) / 2;

	// Find other players
	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(!p->GetPlayer())
			continue;

		// team checks, assume team is clientID in dm
		if(g_Config.m_SvDisablePVP)
		{
			if((Team < 0 && !p->m_IsBot) || (Team >= 0 && p->m_IsBot))
				continue;
		}
		else
		{
			if(GameServer()->m_pController->IsTeamplay())
			{
				if(Team != p->GetPlayer()->GetTeam())
					continue;
			}
			else if(Team != p->GetPlayer()->GetCID())
				continue;
		}

		if(abs(p->m_Pos.x - Center.x) < abs(TopLeft.x - BotRight.x) &&
		   abs(p->m_Pos.y - Center.y) < abs(TopLeft.y - BotRight.y))
			return p;
	}

	return 0;
}

CCharacter *CGameWorld::IntersectReflect(vec2 Pos0, vec2 Pos1, float Radius, vec2 &NewPos, CEntity *pNotThis)
{
	// Find other players
	float ClosestLen = distance(Pos0, Pos1) * 100.0f;
	CCharacter *pClosest = 0;

	CCharacter *p = (CCharacter *)FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		if(p->IgnoreCollision())
			continue;

		// co-op player to player collisiong ignore
		if(g_Config.m_SvDisablePVP && !p->m_IsBot)
		{
			if(pNotThis && pNotThis->GetType() != CGameWorld::ENTTYPE_CHARACTER)
				continue;

			if(pNotThis && pNotThis->GetType() == CGameWorld::ENTTYPE_CHARACTER)
			{
				CCharacter *pOwnerChar = (CCharacter *)pNotThis;
				if(!pOwnerChar->m_IsBot)
					continue;
			}
		}

		int Reflect = p->Reflect();

		if(!Reflect)
			continue;

		vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);

		// only reflect in some directions
		// if (abs(GetAngle(normalize(p->m_Pos - IntersectPos)) - GetAngle(normalize(p->GetVel()))) > pi/4.0f)
		//	continue;

		float Len = distance(p->m_Pos + vec2(0, -32), IntersectPos);
		if(Len < Reflect + Radius)
		{
			Len = distance(Pos0, IntersectPos);
			if(Len < ClosestLen)
			{
				NewPos = IntersectPos;
				ClosestLen = Len;
				pClosest = p;
			}
		}
	}

	return pClosest;
}

CCharacter *CGameWorld::ClosestCharacter(vec2 Pos, float Radius, CEntity *pNotThis)
{
	// Find other players
	const float InitialRange = Radius * 2.0f;
	float ClosestRangeSquared = InitialRange * InitialRange;
	CCharacter *pClosest = 0;

	CCharacter *p = (CCharacter *)GameServer()->m_World.FindFirst(ENTTYPE_CHARACTER);
	for(; p; p = (CCharacter *)p->TypeNext())
	{
		if(p == pNotThis)
			continue;

		const float CollisionRange = p->m_ProximityRadius + Radius;
		const float CollisionRangeSquared = CollisionRange * CollisionRange;
		const float DeltaX = Pos.x - p->m_Pos.x;
		const float DeltaXSquared = DeltaX * DeltaX;
		if(DeltaXSquared >= CollisionRangeSquared || DeltaXSquared >= ClosestRangeSquared)
			continue;

		const float BodyDeltaY = Pos.y - p->m_Pos.y;
		const float BodyDistanceSquared = DeltaXSquared + BodyDeltaY * BodyDeltaY;
		if(BodyDistanceSquared < CollisionRangeSquared && BodyDistanceSquared < ClosestRangeSquared)
		{
			ClosestRangeSquared = BodyDistanceSquared;
			pClosest = p;
		}
		// head collision
		const float HeadDeltaY = BodyDeltaY + 28.0f;
		const float HeadDistanceSquared = DeltaXSquared + HeadDeltaY * HeadDeltaY;
		if(HeadDistanceSquared < CollisionRangeSquared && HeadDistanceSquared < ClosestRangeSquared)
		{
			ClosestRangeSquared = HeadDistanceSquared;
			pClosest = p;
		}
	}

	return pClosest;
}
