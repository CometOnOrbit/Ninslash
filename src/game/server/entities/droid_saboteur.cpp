#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "droid_saboteur.h"
#include "building.h"
#include "character.h"
#include "pve_drone.h"
CSaboteur::CSaboteur(CGameWorld *pWorld, vec2 Pos) : CSpecialistDroid(pWorld, Pos, DROIDTYPE_SABOTEUR, 420, false), m_pEmpTarget(0), m_ChargeStart(0) {}
void CSaboteur::AbilityTick()
{
	if(m_pEmpTarget)
	{
		CEntity *apExisting[128];
		bool Found = false;
		for(int Type : {CGameWorld::ENTTYPE_BUILDING, CGameWorld::ENTTYPE_LASER})
		{
			const int Num = GameServer()->m_World.FindEntities(m_Pos, 1200.0f, apExisting, 128, Type);
			for(int i = 0; i < Num; i++)
				Found |= apExisting[i] == m_pEmpTarget;
		}
		if(!Found)
		{
			m_pEmpTarget = 0;
			m_ChargeStart = 0;
		}
	}
	if(!m_pEmpTarget)
	{
		CEntity *apTargets[64]; float Best = 800.0f;
		for(int Type : {CGameWorld::ENTTYPE_BUILDING, CGameWorld::ENTTYPE_LASER})
		{
			int Num = GameServer()->m_World.FindEntities(m_Pos, 800.0f, apTargets, 64, Type);
			for(int i = 0; i < Num; i++)
			{
				if(Type == CGameWorld::ENTTYPE_LASER && dynamic_cast<CPveDrone *>(apTargets[i]) == 0) continue;
				if(distance(m_Pos, apTargets[i]->m_Pos) < Best && !GameServer()->Collision()->FastIntersectLine(m_Pos + m_Center, apTargets[i]->m_Pos)) { Best = distance(m_Pos, apTargets[i]->m_Pos); m_pEmpTarget = apTargets[i]; }
			}
		}
		m_ChargeStart = m_pEmpTarget ? Server()->Tick() : 0;
	}
	if(m_pEmpTarget)
	{
		if((Server()->Tick() & 7) == 0) GameServer()->CreateEffect(FX_ELECTRIC, m_pEmpTarget->m_Pos);
		if(Server()->Tick() - m_ChargeStart >= Server()->TickSpeed() * 5)
		{
			if(CPveDrone *pDrone = dynamic_cast<CPveDrone *>(m_pEmpTarget)) { pDrone->TakeDamage(10); pDrone->ApplyEmp(Server()->TickSpeed() * 5); }
			else { CBuilding *pBuilding = static_cast<CBuilding *>(m_pEmpTarget); pBuilding->m_aStatus[BSTATUS_ON] = 0; pBuilding->TakeDamage(20, NEUTRAL_BASE, GetDroidWeapon(m_Type)); }
			GameServer()->CreateEffect(FX_ELECTRIC, m_pEmpTarget->m_Pos); m_pEmpTarget = 0; m_ChargeStart = 0; m_AbilityTick = Server()->Tick() + Server()->TickSpeed() * 4; return;
		}
	}
	else
	{
		CCharacter *apChars[MAX_CLIENTS]; int Num = GameServer()->m_World.FindEntities(m_Pos, 240.0f, (CEntity **)apChars, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++) if(apChars[i] && !apChars[i]->m_IsBot) apChars[i]->Electrocute(1.0f);
		GameServer()->CreateEffect(FX_ELECTRIC, m_Pos);
	}
	m_AbilityTick = Server()->Tick() + 5;
}
