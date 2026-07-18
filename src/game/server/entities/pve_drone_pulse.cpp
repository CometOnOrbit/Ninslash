#include <generated/protocol.h>

#include <game/server/gamecontext.h>

#include "pve_drone_pulse.h"

CPveDronePulse::CPveDronePulse(CGameWorld *pGameWorld, vec2 From, vec2 To, const CAttackSource &Source) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE),
	m_From(From),
	m_To(To),
	m_Source(Source),
	m_StartTick(Server()->Tick()),
	m_EndTick(Server()->Tick() + max(2, Server()->TickSpeed() / 5))
{
	m_Pos = From;
	m_ProximityRadius = 4.0f;
	GameWorld()->InsertEntity(this);
}

void CPveDronePulse::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CPveDronePulse::Tick()
{
	const float Amount = clamp((Server()->Tick() - m_StartTick) / (float)max(1, m_EndTick - m_StartTick), 0.0f, 1.0f);
	m_Pos = mix(m_From, m_To, Amount);
	if(Server()->Tick() >= m_EndTick)
		GameServer()->m_World.DestroyEntity(this);
}

void CPveDronePulse::TickPaused()
{
	m_StartTick++;
	m_EndTick++;
}

void CPveDronePulse::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if(!pObj)
		return;
	const vec2 Dir = normalize(m_To - m_From);
	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_VelX = (int)(Dir.x * 100.0f);
	pObj->m_VelY = (int)(Dir.y * 100.0f);
	pObj->m_Vel2X = 0;
	pObj->m_Vel2Y = 0;
	pObj->m_SourceKind = static_cast<int>(m_Source.m_Kind);
	pObj->m_SourceType = m_Source.m_Type;
	pObj->m_WeaponDefinitionId = static_cast<int>(m_Source.m_Weapon.m_DefinitionId);
	pObj->m_WeaponLevel = m_Source.m_Weapon.m_Level;
	pObj->m_StartTick = Server()->Tick();
}
