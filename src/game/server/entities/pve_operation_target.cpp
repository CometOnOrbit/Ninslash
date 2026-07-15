#include <game/server/entities/character.h>
#include <game/server/entities/radar.h>
#include <game/pve_roguelite.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_operation_director.h>

#include "pve_operation_target.h"

CPveOperationTarget::CPveOperationTarget(CGameWorld *pWorld, CPveOperationDirector *pDirector, vec2 Pos, vec2 DeliveryPos, float Radius, int RequiredTicks, int TargetType) :
	CBuilding(pWorld, Pos, BUILDING_GENERATOR, TEAM_NEUTRAL),
	m_pDirector(pDirector),
	m_pRadar(0),
	m_Radius(Radius),
	m_RequiredTicks(RequiredTicks),
	m_ProgressTicks(0),
	m_Complete(false),
	m_TargetType(TargetType), m_CargoType(PveCargoFromOperationTarget(TargetType)), m_SourcePos(Pos), m_DeliveryPos(DeliveryPos), m_CarrierCID(-1)
{
	m_Pos = Pos;
	m_Life = m_MaxLife = 300;
	m_pRadar = new CRadar(pWorld, RADAR_REACTOR);
	m_pRadar->Activate(Pos);
}

CPveOperationTarget::~CPveOperationTarget()
{
	if(m_CargoType != PVE_CARGO_NONE && m_CarrierCID >= 0)
	{
		CCharacter *pCarrier = GameServer()->GetPlayerChar(m_CarrierCID);
		if(pCarrier)
			pCarrier->RemovePveCargo(m_CargoType);
	}
	if(m_pRadar)
	{
		m_pRadar->Deactivate();
		GameServer()->m_World.DestroyEntity(m_pRadar);
		m_pRadar = 0;
	}
}

void CPveOperationTarget::Snap(int SnappingClient)
{
	// Cargo has dedicated world and character rendering. The inherited
	// generator snapshot was only a placeholder and obscured the core.
	if(m_CargoType != PVE_CARGO_NONE)
		return;
	CBuilding::Snap(SnappingClient);
}

void CPveOperationTarget::Reset()
{
	if(m_pRadar)
	{
		m_pRadar->Deactivate();
	}
	m_pDirector = 0;
	GameServer()->m_World.DestroyEntity(this);
}

void CPveOperationTarget::Tick()
{
	CBuilding::Tick();
	if(m_Complete || !m_pDirector)
		return;
	if(m_Life <= 0)
	{
		m_Complete = true;
		m_pDirector->OnTargetCompleted(this);
		return;
	}
	const bool Destructible = m_TargetType == PVE_OPERATION_TARGET_SHIELD_RELAY ||
		m_TargetType == PVE_OPERATION_TARGET_ASSEMBLY_NODE || m_TargetType == PVE_OPERATION_TARGET_TARGETING_BEACON ||
		m_TargetType == PVE_OPERATION_TARGET_SHIELD_NODE;
	if(Destructible)
		return;
	const bool Carry = m_CargoType != PVE_CARGO_NONE;
	if(Carry && m_CarrierCID >= 0)
	{
		CCharacter *pCarrier = GameServer()->GetPlayerChar(m_CarrierCID);
		if(!pCarrier || !pCarrier->IsAlive() || !pCarrier->HasPveCargo(m_CargoType))
		{
			m_CarrierCID = -1;
			m_Pos = m_SourcePos;
			m_pRadar->Activate(m_Pos);
			m_pDirector->OnCargoStateChanged();
			return;
		}
		m_Pos = pCarrier->m_Pos;
		m_pRadar->Activate(m_DeliveryPos);
		if(distance(pCarrier->m_Pos, m_DeliveryPos) <= m_Radius)
		{
			pCarrier->RemovePveCargo(m_CargoType);
			m_CarrierCID = -1;
			m_Complete = true;
			m_pDirector->OnTargetCompleted(this);
		}
		return;
	}
	bool Occupied = false;
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
		CCharacter *pCharacter = pPlayer ? pPlayer->GetCharacter() : 0;
		if(pCharacter && !pPlayer->m_IsBot && pPlayer->GetTeam() != TEAM_SPECTATORS && pCharacter->IsAlive() && distance(pCharacter->m_Pos, m_Pos) <= m_Radius)
		{
			if(Carry && pCharacter->GivePveCargo(m_CargoType))
			{
				m_CarrierCID = ClientID;
				m_pRadar->Activate(m_DeliveryPos);
				m_pDirector->OnCargoStateChanged();
				return;
			}
			Occupied = true;
			break;
		}
	}
	m_ProgressTicks = Occupied ? m_ProgressTicks + 1 : max(0, m_ProgressTicks - 2);
	if(m_ProgressTicks >= m_RequiredTicks)
	{
		m_Complete = true;
		m_pDirector->OnTargetCompleted(this);
	}
}
