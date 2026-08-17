#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/entities/weapon.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/entities/building.h>
#include <game/server/entities/radar.h>

#include "cs.h"

#include <game/server/ai.h>
#include <game/server/ai/def_ai.h>

CGameControllerCS::CGameControllerCS(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "Reactor Assault";
	m_GameFlags = GAMEFLAG_TEAMS;

	m_GameFlags |= GAMEFLAG_SURVIVAL;
	m_GameFlags |= GAMEFLAG_BUILD;

	g_Config.m_SvWarmup = 0;
	g_Config.m_SvTimelimit = 0;
	g_Config.m_SvSurvivalMode = 1;
	g_Config.m_SvDisablePVP = 0;

	m_pBombRadar = 0;

	if(g_Config.m_SvSurvivalMode && g_Config.m_SvSurvivalTime && g_Config.m_SvSurvivalAcid)
		m_GameFlags |= GAMEFLAG_ACID;

	m_RoundWinner = -1;

	m_GameState = 0;
	m_Bomb = false;

	m_AreaCount = 0;

	for(int i = 0; i < 9; i++)
		m_aArea[i] = vec4(0, 0, 0, 0);

	for(int i = 0; i < MAX_CLIENTS * NUM_SLOTS; i++)
		m_aPlayerWeapon[i] = {};

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aPlayerArmor[i] = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aPlayerKits[i] = 0;
}

void CGameControllerCS::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);

	// init AI
	if(RequestAI)
	{
		if(!pChr->m_AISkin.m_Valid)
			GameServer()->GetAISkin(&pChr->m_AISkin, true);
		pChr->SetAISkin();
		pChr->m_pAI = new CAIdef(GameServer(), pChr);
	}

	CPlayer *pPlayer = pChr->GetPlayer();
	if(!pPlayer)
		return;
	int c = pPlayer->GetCID();

	for(int i = 0; i < NUM_SLOTS; i++)
	{
		if(m_aPlayerWeapon[c * NUM_SLOTS + i].IsValid())
		{
			pChr->GiveWeapon(GameServer()->NewWeapon(m_aPlayerWeapon[c * NUM_SLOTS + i]));
			m_aPlayerWeapon[c * NUM_SLOTS + i] = {};
		}
	}

	pChr->IncreaseArmor(m_aPlayerArmor[c]);
	pChr->AddKits(m_aPlayerKits[c]);
	m_aPlayerArmor[c] = 0;
	m_aPlayerKits[c] = 0;
}

int CGameControllerCS::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source)
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

vec2 CGameControllerCS::GetAttackPos()
{
	if(!m_AreaCount)
		return vec2(0.0f, 0.0f);

	int i = rand() % m_AreaCount;

	return vec2((m_aArea[i].x + m_aArea[i].z) / 2, (m_aArea[i].y + m_aArea[i].w) / 2);
}

bool CGameControllerCS::InBombArea(vec2 Pos)
{
	for(int i = 0; i < m_AreaCount; i++)
	{
		if(Pos.x > m_aArea[i].x && Pos.x < m_aArea[i].z && Pos.y > m_aArea[i].y && Pos.y < m_aArea[i].w)
			return true;
	}

	return false;
}

void CGameControllerCS::StartRound()
{
	FindReactors();
}

void CGameControllerCS::NewSurvivalRound()
{
	// save weapons if character alive
	for(int c = 0; c < MAX_CLIENTS; c++)
	{
		CCharacter *pChar = GameServer()->GetPlayerChar(c);

		if(pChar)
		{
			for(int i = 0; i < NUM_SLOTS; i++)
				if(pChar->GetWeapon(i) && pChar->GetWeapon(i)->GetWeaponProfile().m_Definition.m_StaticType != SW_BOMB)
					m_aPlayerWeapon[c * NUM_SLOTS + i] = pChar->GetWeapon(i)->GetWeaponSpec();

			m_aPlayerArmor[c] = pChar->GetArmor();
			m_aPlayerKits[c] = pChar->m_Kits;
		}
		else
		{
			for(int i = 0; i < NUM_SLOTS; i++)
			{
				m_aPlayerWeapon[c * NUM_SLOTS + i] = {};
				m_aPlayerArmor[c] = 0;
				m_aPlayerKits[c] = 0;
			}
		}

		if(GameServer()->m_apPlayers[c])
			GameServer()->m_apPlayers[c]->IncreaseGold(15);
	}

	m_Bomb = false;
	GameServer()->SendBroadcast("Go!", -1);
}

void CGameControllerCS::TriggerBomb()
{
	// time limit display
	m_SurvivalStartTick = Server()->Tick() - Server()->TickSpeed() * (g_Config.m_SvSurvivalTime - 20.1f);
	GameServer()->SendBroadcast("Bomb armed", -1);
	m_SurvivalDeathReset = false;
}

void CGameControllerCS::OnSurvivalTimeOut()
{
	if(!m_SurvivalResetTick)
	{
		m_SurvivalResetTick = Server()->Tick() + Server()->TickSpeed() * 4.0f;
		m_RoundWinner = TEAM_BLUE;
		GameServer()->SendBroadcast("Time out - Blue team wins", -1);
		m_aTeamscore[TEAM_BLUE] += g_Config.m_SvSurvivalReward;
	}
}

void CGameControllerCS::DisarmBomb()
{
	if(!m_SurvivalResetTick)
	{
		m_SurvivalResetTick = Server()->Tick() + Server()->TickSpeed() * 4.0f;
		m_RoundWinner = TEAM_BLUE;
		GameServer()->SendBroadcast("Bomb disarmed - Blue team wins", -1);
		m_aTeamscore[TEAM_BLUE] += g_Config.m_SvSurvivalReward;
	}
}

void CGameControllerCS::ReactorDestroyed()
{
	if(!m_SurvivalResetTick)
	{
		m_SurvivalResetTick = Server()->Tick() + Server()->TickSpeed() * 4.0f;
		m_RoundWinner = TEAM_RED;
		GameServer()->SendBroadcast("Reactor destroyed - Red team wins", -1);
		m_aTeamscore[TEAM_RED] += g_Config.m_SvSurvivalReward;
	}
}

void CGameControllerCS::AddToArea(vec2 Pos)
{
	int Size = 200;

	// no bomb areas
	if(!m_AreaCount)
	{
		m_aArea[m_AreaCount++] = vec4(Pos.x - Size, Pos.y - Size, Pos.x + Size, Pos.y + Size);
		return;
	}

	// expand existing area
	for(int i = 0; i < m_AreaCount; i++)
	{
		if(Pos.x > m_aArea[i].x && Pos.x < m_aArea[i].z && Pos.y > m_aArea[i].y && Pos.y < m_aArea[i].w)
		{
			m_aArea[i].x = min(Pos.x - Size, m_aArea[i].x);
			m_aArea[i].y = min(Pos.y - Size, m_aArea[i].y);
			m_aArea[i].z = max(Pos.x + Size, m_aArea[i].z);
			m_aArea[i].w = max(Pos.y + Size, m_aArea[i].w);
			return;
		}
	}

	if(m_AreaCount >= 9)
		return;

	// ...or new area
	m_aArea[m_AreaCount++] = vec4(Pos.x - Size, Pos.y - Size, Pos.x + Size, Pos.y + Size);
}

void CGameControllerCS::FindReactors()
{
	CBuilding *apEnts[9999];
	int Num = GameServer()->m_World.FindEntities(vec2(0, 0), 0, (CEntity **)apEnts, 9999, CGameWorld::ENTTYPE_BUILDING);

	for(int i = 0; i < Num; ++i)
	{
		CBuilding *pBuilding = apEnts[i];

		if(pBuilding->m_Type == BUILDING_REACTOR)
			AddToArea(pBuilding->m_Pos);
	}

	for(int i = 0; i < m_AreaCount; i++)
	{
		CServerRadar *pRadar = new CServerRadar(&GameServer()->m_World, RADAR_REACTOR);
		pRadar->Activate(vec2((m_aArea[i].x + m_aArea[i].z) / 2, (m_aArea[i].y + m_aArea[i].w) / 2));
	}
}

void CGameControllerCS::Snap(int SnappingClient)
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

void CGameControllerCS::Tick()
{
	IGameController::Tick();
	AutoBalance();
	GameServer()->UpdateAI();

	if(!m_Bomb)
	{
		int i = frandom() * MAX_CLIENTS;

		if(GameServer()->GetPlayerChar(i) && GameServer()->GetPlayerChar(i)->GiveBomb())
			m_Bomb = true;
	}

	if(!m_pBombRadar)
		m_pBombRadar = new CServerRadar(&GameServer()->m_World, RADAR_BOMB);
	else
		m_pBombRadar->Activate(GameServer()->m_pController->m_BombPos);

	if(!m_GameState)
	{
		m_GameState++;
		StartRound();
	}
}
