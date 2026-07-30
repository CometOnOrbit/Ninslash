#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "pve_drone.h"

CPveDrone::CPveDrone(CGameWorld *pGameWorld, int Owner)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER), m_Owner(Owner), m_StartTick(Server()->Tick()), m_Health(40),
	  m_DisabledUntilTick(0), m_Vel(0, 0), m_MoveTarget(0, 0), m_Target(m_Pos), m_ActionTick(0),
	  m_AngleTimer(Owner * 1.73f)
{
	m_ProximityRadius = 12.0f;
	CCharacter *pOwner = GameServer()->GetPlayerChar(Owner);
	m_Pos = pOwner ? pOwner->m_Pos + vec2(Owner & 1 ? -38.0f : 38.0f, -52.0f) : vec2(0, 0);
	m_MoveTarget = m_Pos;
	m_Target = m_Pos;
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

	// Mirror CStar::Tick flight: random wander, AngleTimer orbit,
	// IntersectLine clip, soft MoveTarget chase, 0.97 damping. Wander is
	// centered on the owner so the companion stays leashed.
	const bool Disabled = m_Health <= 0 || Server()->Tick() < m_DisabledUntilTick;
	const bool Acting = !Disabled && Server()->Tick() <= m_ActionTick;
	m_AngleTimer += Disabled ? 0.012f : 0.025f;

	const vec2 OwnerAnchor = pOwner->m_Pos + vec2(m_Owner & 1 ? -48.0f : 48.0f, -56.0f);
	const vec2 Center = Acting ? m_Target : OwnerAnchor;
	vec2 To = Center + vec2(frandom() - frandom(), frandom() - frandom()) * (Acting ? 280.0f : 160.0f);
	To += vec2(sin(m_AngleTimer), cos(m_AngleTimer)) * (Acting ? 80.0f : 90.0f);

	if(GameServer()->Collision()->IntersectLine(m_Pos, To, 0x0, &To))
	{
		To = Center + vec2(frandom() - frandom(), frandom() - frandom()) * (Acting ? 180.0f : 110.0f);
		To += vec2(sin(m_AngleTimer), cos(m_AngleTimer)) * 70.0f;
		GameServer()->Collision()->IntersectLine(m_Pos, To, 0x0, &To);
	}

	const vec2 OwnerDelta = m_Pos - pOwner->m_Pos;
	if(dot(OwnerDelta, OwnerDelta) > 280.0f * 280.0f)
		To = OwnerAnchor + vec2(sin(m_AngleTimer), cos(m_AngleTimer)) * 40.0f;

	m_MoveTarget += (To - m_MoveTarget) / 20.0f;

	if(length(m_MoveTarget - m_Pos) > 8.0f)
		m_Vel += normalize(m_MoveTarget - m_Pos) * 0.40f * (Disabled ? 0.5f : 1.0f);

	// Soft per-axis approach toward the action point, same idea as Star's
	// far-target correction but in world space for a companion drone.
	const vec2 ActionDelta = m_Pos - m_Target;
	if(Acting && dot(ActionDelta, ActionDelta) > 140.0f * 140.0f)
	{
		m_MoveTarget += (vec2(m_Pos.x, m_Target.y) - m_MoveTarget) / 10.0f;
		m_MoveTarget += (vec2(m_Target.x, m_Pos.y) - m_MoveTarget) / 10.0f;
	}

	m_Vel *= 0.97f;
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(40.0f, 40.0f), 0, false, true);
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
	CNetObj_PveDrone *pObj =
		static_cast<CNetObj_PveDrone *>(Server()->SnapNewItem(NETOBJTYPE_PVEDRONE, m_ID, sizeof(CNetObj_PveDrone)));
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
	pObj->m_SwitchReadyTick =
		GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->DroneSwitchReadyTick(m_Owner) : 0;
}
