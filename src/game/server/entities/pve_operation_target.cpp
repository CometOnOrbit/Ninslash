#include <game/server/entities/character.h>
#include <game/server/entities/radar.h>
#include <game/pve_roguelite.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_director.h>
#include <game/server/pve_operation_director.h>

#include "pve_operation_target.h"

namespace
{
int BuildingTypeForTarget(int TargetType)
{
	switch(TargetType)
	{
	case PVE_OPERATION_TARGET_SHIELD_RELAY: return BUILDING_PVE_SHIELD_RELAY;
	case PVE_OPERATION_TARGET_OVERLOAD_TERMINAL: return BUILDING_PVE_OVERLOAD_TERMINAL;
	case PVE_OPERATION_TARGET_ASSEMBLY_NODE: return BUILDING_PVE_ASSEMBLY_NODE;
	case PVE_OPERATION_TARGET_TARGETING_BEACON: return BUILDING_PVE_TARGETING_BEACON;
	case PVE_OPERATION_TARGET_DATA_CORE: return BUILDING_PVE_DATA_CORE;
	case PVE_OPERATION_TARGET_UPLOAD_POINT: return BUILDING_PVE_UPLOAD_POINT;
	case PVE_OPERATION_TARGET_SHIELD_NODE: return BUILDING_PVE_SHIELD_NODE;
	case PVE_OPERATION_TARGET_ENERGY_CORE: return BUILDING_PVE_ENERGY_CORE;
	default: return BUILDING_PVE_OVERLOAD_TERMINAL;
	}
}
}

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
	m_Type = BuildingTypeForTarget(TargetType);
	m_CanMove = false;
	m_Moving = false;
	m_Collision = IsDestructible();
	m_BoxSize = vec2(42.0f, 56.0f);
	m_ProximityRadius = 32.0f;
	m_Life = m_MaxLife = 300;
	m_pRadar = new CRadar(pWorld, RADAR_REACTOR);
	m_pRadar->Activate(Pos);
}

bool CPveOperationTarget::IsDestructible() const
{
	return m_TargetType == PVE_OPERATION_TARGET_SHIELD_RELAY ||
		m_TargetType == PVE_OPERATION_TARGET_ASSEMBLY_NODE ||
		m_TargetType == PVE_OPERATION_TARGET_TARGETING_BEACON ||
		m_TargetType == PVE_OPERATION_TARGET_SHIELD_NODE;
}

void CPveOperationTarget::DeactivateRadar()
{
	if(m_pRadar)
		m_pRadar->Deactivate();
}

void CPveOperationTarget::TakeDamage(int Damage, int Owner, int Weapon, vec2 Force)
{
	// Hold, upload and cargo objectives are defended/used by players, never
	// destroyed by them. Only explicitly destructive operation steps accept
	// damage and may complete through reaching zero life.
	if(!IsDestructible() && Damage > 0)
		return;
	CBuilding::TakeDamage(Damage, Owner, Weapon, Force);
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
	DeactivateRadar();
	m_pDirector = 0;
	GameServer()->m_World.DestroyEntity(this);
}

void CPveOperationTarget::Tick()
{
	CBuilding::Tick();
	if(m_Complete || !m_pDirector)
		return;
	if(IsDestructible() && m_Life <= 0)
	{
		m_Complete = true;
		DeactivateRadar();
		m_pDirector->OnTargetCompleted(this);
		return;
	}
	if(IsDestructible())
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
			DeactivateRadar();
			m_pDirector->OnTargetCompleted(this);
		}
		return;
	}
	bool Occupied = false;
	float BestScale = 1.0f;
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
			if(GameServer()->m_pPveDirector)
				BestScale = max(BestScale, GameServer()->m_pPveDirector->InteractionSpeedBonus(ClientID));
		}
	}
	m_ProgressTicks = Occupied ? m_ProgressTicks + max(1, (int)(BestScale + frandom())) : max(0, m_ProgressTicks - 2);
	if(m_ProgressTicks >= m_RequiredTicks)
	{
		m_Complete = true;
		DeactivateRadar();
		m_pDirector->OnTargetCompleted(this);
	}
}
