#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "building.h"
#include "character.h"
#include "lightning.h"
#include "teslacoil.h"

CTeslacoil::CTeslacoil(CGameWorld *pGameWorld, vec2 Pos, int Team, int OwnerPlayer)
	: CBuilding(pGameWorld, Pos, BUILDING_TESLACOIL, Team)
{
	m_ProximityRadius = TeslacoilPhysSize;
	m_Life = 80;
	m_MaxLife = 80;

	m_AttachOnFall = true;
	m_Bounciness = 0.0f;
	m_BoxSize = vec2(24.0f, 34.0f);
	m_CanMove = true;
	m_Moving = false;

	m_OwnerPlayer = OwnerPlayer;
	m_AttackTick = Server()->Tick() + Server()->TickSpeed() * frandom();

	if(!GameServer()->m_pController->IsTeamplay())
		m_Team = m_OwnerPlayer;
	else
		m_Team = Team;

	m_Center = vec2(0, -55);
	m_FlipY = 1;

	if(GameServer()->Collision()->IsTileSolid(Pos.x, Pos.y - 40))
	{
		m_Mirror = true;
		m_Center = vec2(0, +55);
		m_FlipY = -1;
	}
}

void CTeslacoil::Tick()
{
	if(m_SnapTick && m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 5.0f)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
		{
			GameServer()->m_World.DestroyEntity(this);
			return;
		}
	}

	if(m_Life < 40)
		m_aStatus[BSTATUS_REPAIR] = 1;
	else
		m_aStatus[BSTATUS_REPAIR] = 0;

	UpdateStatus();

	if(Server()->Tick() > m_AttackTick + Server()->TickSpeed() * (0.3f + frandom() * 0.3f))
		Fire();

	Move();

	// destroy
	if(m_Life <= 0)
	{
		GameServer()->CreateExplosion(m_Pos, CAttackSource::Building(m_DamageOwner, m_Type));
		// GameServer()->CreateSound(m_Pos + vec2(0, -50*m_FlipY), SOUND_GRENADE_EXPLODE);
		GameServer()->m_World.DestroyEntity(this);
	}
}

void CTeslacoil::Fire()
{
	m_AttackTick = Server()->Tick();

	vec2 TurretPos = m_Pos + vec2(0, -67 * m_FlipY);

	bool Sound = false;

	for(CCharacter *pCharacter = (CCharacter *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER);
		pCharacter;
		pCharacter = (CCharacter *)pCharacter->TypeNext())
	{
		if(pCharacter->GetTeam() == m_Team && GameServer()->m_pController->IsTeamplay())
			continue;

		if((!pCharacter->IsAlive() || pCharacter->GetCID() == m_OwnerPlayer) &&
		   !GameServer()->m_pController->IsTeamplay())
			continue;

		if(GameServer()->m_pController->IsCoop())
		{
			if(!pCharacter->m_IsBot && m_Team >= 0)
				continue;

			if(pCharacter->m_IsBot && m_Team < 0)
				continue;
		}

		if(pCharacter->Invisible())
			continue;

		int Distance = distance(pCharacter->m_Pos, TurretPos);
		if(Distance < 700 && !GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos, TurretPos))
		{
			new CLightning(GameWorld(), TurretPos, pCharacter->m_Pos);
			pCharacter->TakeDamage(
				CAttackSource::Building(m_OwnerPlayer, BUILDING_TESLACOIL), 5, vec2(0, 0), vec2(0, 0));
			Sound = true;
		}
	}

	if(Sound)
		GameServer()->CreateSound(m_Pos + vec2(0, -50 * m_FlipY), SOUND_TESLACOIL_FIRE);
}

void CTeslacoil::TickPaused()
{
}

void CTeslacoil::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	m_SnapTick = Server()->Tick();

	CNetObj_Building *pP =
		static_cast<CNetObj_Building *>(Server()->SnapNewItem(NETOBJTYPE_BUILDING, m_ID, sizeof(CNetObj_Building)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Status = m_Status;
	pP->m_Type = m_Type;

	if(GameServer()->m_pController->IsTeamplay())
		pP->m_Team = m_Team;
	else
	{
		if(SnappingClient == m_OwnerPlayer)
			pP->m_Team = TEAM_RED;
		else
			pP->m_Team = -1;
	}
}
