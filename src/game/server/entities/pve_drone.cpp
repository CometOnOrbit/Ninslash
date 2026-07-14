#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "pve_drone.h"

CPveDrone::CPveDrone(CGameWorld *pGameWorld, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER),
	m_Owner(Owner),
	m_StartTick(Server()->Tick())
{
	m_ProximityRadius = 12.0f;
	CCharacter *pOwner = GameServer()->GetPlayerChar(Owner);
	m_Pos = pOwner ? pOwner->m_Pos + vec2(Owner & 1 ? -38.0f : 38.0f, -52.0f) : vec2(0, 0);
	GameWorld()->InsertEntity(this);
}

void CPveDrone::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CPveDrone::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->IsAlive())
		return;
	const vec2 Target = pOwner->m_Pos + vec2(m_Owner & 1 ? -42.0f : 42.0f, -54.0f);
	// A bounded deterministic follow step avoids introducing another predicted
	// physics object while keeping the server position authoritative.
	m_Pos += (Target - m_Pos) * 0.16f;
}

void CPveDrone::TickPaused()
{
	m_StartTick++;
}

void CPveDrone::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;
	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_ID, sizeof(CNetObj_Laser)));
	if(!pObj)
		return;
	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_FromX = (int)pOwner->m_Pos.x;
	pObj->m_FromY = (int)(pOwner->m_Pos.y - 24.0f);
	pObj->m_StartTick = m_StartTick;
	pObj->m_Charge = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->DroneModule(m_Owner) : PVE_DRONE_NONE;
}
