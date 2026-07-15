#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "pve_drone.h"

CPveDrone::CPveDrone(CGameWorld *pGameWorld, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER),
	m_Owner(Owner),
	m_StartTick(Server()->Tick()),
	m_Health(40),
	m_DisabledUntilTick(0),
	m_Vel(0, 0),
	m_Target(m_Pos),
	m_ActionTick(0)
{
	m_ProximityRadius = 12.0f;
	CCharacter *pOwner = GameServer()->GetPlayerChar(Owner);
	m_Pos = pOwner ? pOwner->m_Pos + vec2(Owner & 1 ? -38.0f : 38.0f, -52.0f) : vec2(0, 0);
	GameWorld()->InsertEntity(this);
}

bool CPveDrone::Active()
{
	return m_Health > 0 && Server()->Tick() >= m_DisabledUntilTick;
}

bool CPveDrone::TakeDamage(int Damage)
{
	if(Damage <= 0 || m_Health <= 0)
		return false;
	m_Health = max(0, m_Health - Damage);
	if(m_Health == 0)
		m_DisabledUntilTick = Server()->Tick() + Server()->TickSpeed() * 12;
	return m_Health == 0;
}

void CPveDrone::ApplyEmp(int DurationTicks)
{
	if(m_Health > 0)
		m_DisabledUntilTick = max(m_DisabledUntilTick, Server()->Tick() + max(0, DurationTicks));
}

void CPveDrone::SetAction(vec2 Target, int ActionTick)
{
	m_Target = Target;
	m_ActionTick = ActionTick;
}

void CPveDrone::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CPveDrone::Tick()
{
	if(m_Health == 0 && Server()->Tick() >= m_DisabledUntilTick)
	{
		m_Health = 40;
		m_DisabledUntilTick = 0;
		m_StartTick = Server()->Tick();
	}
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->IsAlive())
		return;
	const vec2 Target = pOwner->m_Pos + vec2(m_Owner & 1 ? -42.0f : 42.0f, -54.0f);
	// A bounded deterministic follow step avoids introducing another predicted
	// physics object while keeping the server position authoritative.
	const vec2 OldPos = m_Pos;
	m_Pos += (Target - m_Pos) * 0.16f;
	m_Vel = m_Pos - OldPos;
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
	CNetObj_PveDrone *pObj = static_cast<CNetObj_PveDrone *>(Server()->SnapNewItem(NETOBJTYPE_PVEDRONE, m_ID, sizeof(CNetObj_PveDrone)));
	if(!pObj)
		return;
	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_VelX = (int)(m_Vel.x * 100.0f);
	pObj->m_VelY = (int)(m_Vel.y * 100.0f);
	pObj->m_Owner = m_Owner;
	pObj->m_Module = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->DroneModule(m_Owner) : PVE_DRONE_NONE;
	pObj->m_Health = m_Health;
	if(m_Health <= 0)
		pObj->m_State = PVE_DRONE_STATE_REBUILDING;
	else if(Server()->Tick() < m_DisabledUntilTick)
		pObj->m_State = PVE_DRONE_STATE_DISABLED;
	else if(Server()->Tick() < m_StartTick + Server()->TickSpeed() / 2)
		pObj->m_State = PVE_DRONE_STATE_DEPLOYING;
	else if(Server()->Tick() <= m_ActionTick)
		pObj->m_State = PVE_DRONE_STATE_ACTING;
	else
		pObj->m_State = PVE_DRONE_STATE_FOLLOWING;
	pObj->m_TargetX = (int)m_Target.x;
	pObj->m_TargetY = (int)m_Target.y;
	pObj->m_ActionTick = m_Health <= 0 || Server()->Tick() < m_DisabledUntilTick ? m_DisabledUntilTick : m_ActionTick;
	pObj->m_SwitchReadyTick = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->DroneSwitchReadyTick(m_Owner) : 0;
}
