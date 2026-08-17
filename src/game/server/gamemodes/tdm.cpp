

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>

#include "tdm.h"

#include <game/server/ai.h>
#include <game/server/ai/tdm_ai.h>

CGameControllerTDM::CGameControllerTDM(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "Team deathmatch";
	m_GameFlags = GAMEFLAG_TEAMS;

	g_Config.m_SvDisablePVP = 0;

	if(g_Config.m_SvSurvivalMode)
		m_GameFlags |= GAMEFLAG_SURVIVAL;

	if(g_Config.m_SvSurvivalMode && g_Config.m_SvSurvivalTime && g_Config.m_SvSurvivalAcid)
		m_GameFlags |= GAMEFLAG_ACID;

	if(g_Config.m_SvEnableBuilding)
		m_GameFlags |= GAMEFLAG_BUILD;
}

void CGameControllerTDM::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);

	// init AI
	if(RequestAI)
	{
		if(!pChr->m_AISkin.m_Valid)
			GameServer()->GetAISkin(&pChr->m_AISkin, true);
		pChr->SetAISkin();
		pChr->m_pAI = new CAItdm(GameServer(), pChr);
	}
}

int CGameControllerTDM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Source);

	if(pKiller && !(Source.m_Kind == EAttackSourceKind::World && Source.m_Type == WEAPON_GAME))
	{
		const int KillerTeam = pKiller->GetTeam();
		const int VictimTeam = pVictim->GetTeam();
		if(pKiller == pVictim->GetPlayer() || KillerTeam == VictimTeam)
		{
			if(!g_Config.m_SvSelfKillPenalty)
				m_aTeamscore[KillerTeam & 1]--;
		}
		else
			m_aTeamscore[KillerTeam & 1]++;
	}
	else if(!(Source.m_Kind == EAttackSourceKind::World && Source.m_Type == WEAPON_GAME))
	{
		CCharacter *pKillerChr = GameServer()->GetCoreChar(Source.m_Owner);
		if(pKillerChr)
		{
			const int KillerTeam = pKillerChr->GetTeam();
			const int VictimTeam = pVictim->GetTeam();
			if(pKillerChr == pVictim || KillerTeam == VictimTeam)
			{
				if(!g_Config.m_SvSelfKillPenalty)
					m_aTeamscore[KillerTeam & 1]--;
			}
			else
				m_aTeamscore[KillerTeam & 1]++;
		}
	}

	return 0;
}

void CGameControllerTDM::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameData *pGameDataObj =
		(CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	pGameDataObj->m_TeamscoreRed = m_aTeamscore[TEAM_RED];
	pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];

	pGameDataObj->m_FlagCarrierRed = 0;
	pGameDataObj->m_FlagCarrierBlue = 0;
}

void CGameControllerTDM::Tick()
{
	IGameController::Tick();
	AutoBalance();
	GameServer()->UpdateAI();
}
