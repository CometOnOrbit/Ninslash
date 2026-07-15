#include <cmath>

#include <generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>

#include "pve_operation_hazard.h"

CPveOperationHazard::CPveOperationHazard(CGameWorld *pWorld, vec2 Pos, EKind Kind, int DurationTicks) :
	CEntity(pWorld, CGameWorld::ENTTYPE_LASER),
	m_Kind(Kind),
	m_Anchor(Pos),
	m_EndTick(Server()->Tick() + DurationTicks),
	m_NextActionTick(Server()->Tick() + Server()->TickSpeed()),
	m_Phase(0)
{
	m_Pos = Pos;
	GameWorld()->InsertEntity(this);
}

void CPveOperationHazard::Reset()
{
	GameWorld()->DestroyEntity(this);
}

void CPveOperationHazard::TickPaused()
{
	m_EndTick++;
	m_NextActionTick++;
}

void CPveOperationHazard::Tick()
{
	if(Server()->Tick() >= m_EndTick)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}
	if(Server()->Tick() < m_NextActionTick)
		return;

	if(m_Kind == BOMBARDMENT)
	{
		CCharacter *pTarget = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacter *pCharacter = GameServer()->GetPlayerChar(i);
			if(pCharacter && pCharacter->IsAlive() && !pCharacter->m_IsBot)
			{
				pTarget = pCharacter;
				if(((i + m_Phase) & 1) == 0)
					break;
			}
		}
		if(pTarget)
		{
			m_Pos = pTarget->m_Pos;
			GameServer()->CreateEffect(FX_ELECTRIC, m_Pos);
			GameServer()->CreateExplosion(m_Pos, NEUTRAL_BASE, WEAPON_GRENADE);
		}
		m_NextActionTick = Server()->Tick() + Server()->TickSpeed() * 3;
	}
	else
	{
		const float Angle = m_Phase * 0.42f;
		m_Pos = m_Anchor + vec2(std::cos(Angle), std::sin(Angle) * 0.45f) * 260.0f;
		CCharacter *apCharacters[MAX_CLIENTS];
		const int Num = GameWorld()->FindEntities(m_Pos, 150.0f, (CEntity **)apCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
			if(apCharacters[i] && !apCharacters[i]->m_IsBot)
				apCharacters[i]->Electrocute(1.0f);
		GameServer()->CreateEffect(FX_ELECTRIC, m_Pos);
		m_NextActionTick = Server()->Tick() + Server()->TickSpeed() / 2;
	}
	m_Phase++;
}
