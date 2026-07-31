#include <engine/shared/config.h>

#include <game/mapitems.h>

#include <game/server/entities/character.h>
#include <game/server/entities/radar.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/aiskin.h>

#include "roam.h"

#include <game/server/ai.h>
#include <game/server/ai/roam_ai.h>

CGameControllerRoam::CGameControllerRoam(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "Roam";
	m_aBotSpawn[0] = vec2(0, 0);
	m_BotSpawnNum = 0;
	m_NumSpawnPoints = 0;
	m_NumCheckpoints = 0;
	m_NumFinishes = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aRace[i].m_StartTick = 0;
		m_aRace[i].m_NextCheckpoint = 0;
		m_aRace[i].m_FinishTick = -1;
		m_aRace[i].m_Active = false;
	}
	g_Config.m_SvDisablePVP = true;
	m_GameFlags = GAMEFLAG_COOP;

	if(g_Config.m_SvSurvivalMode)
		m_GameFlags |= GAMEFLAG_SURVIVAL;

	// if (g_Config.m_SvSurvivalMode && g_Config.m_SvSurvivalTime && g_Config.m_SvSurvivalAcid)
	//	m_GameFlags |= GAMEFLAG_ACID;

	// if (g_Config.m_SvEnableBuilding)
	//	m_GameFlags |= GAMEFLAG_BUILD;

	// for (int i = 0; i < MAX_CLIENTS; i++)
	//	new CServerRadar(&GameServer()->m_World, RADAR_CHARACTER, i);
}

void CGameControllerRoam::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);

	// init AI
	if(RequestAI)
	{
		GameServer()->GetAISkin(&pChr->GetPlayer()->m_AISkin, false);

		// pChr->GetPlayer()->m_AISkin = GameServer()->GetAISkin(false);
		pChr->GetPlayer()->m_pAI = new CAIroam(GameServer(), pChr->GetPlayer(), 0);
		pChr->GetPlayer()->SetAISkin();

		// m_Skin = SKIN_ALIEN1;
		// Player()->SetCustomSkin(m_Skin);
	}
	else
	{
		const int ClientID = pChr->GetPlayer()->GetCID();
		m_aRace[ClientID].m_StartTick = Server()->Tick();
		m_aRace[ClientID].m_NextCheckpoint = 0;
		m_aRace[ClientID].m_FinishTick = -1;
		m_aRace[ClientID].m_Active = true;
		pChr->GetPlayer()->m_Score = -9999999;
	}
}

int CGameControllerRoam::OnCharacterDeath(class CCharacter *pVictim,
										  class CPlayer *pKiller,
										  const CAttackSource &Source)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Source);

	if(pVictim->m_IsBot)
		pVictim->GetPlayer()->m_ToBeKicked = true;

	/*
	if (g_Config.m_SvSurvivalMode && !pVictim->m_IsBot && CountPlayersAlive(-1, true) <= 1)
	{
		DeathMessage();
		m_RoundOverTick = Server()->Tick();
	}
	*/

	return 0;
}

void CGameControllerRoam::AddEnemy(vec2 Pos)
{
	if(GameServer()->m_pController->CountBots() < 32 && m_BotSpawnNum < 99)
	{
		GameServer()->AddBot();
		m_aBotSpawn[m_BotSpawnNum++] = Pos;
	}
}

void CGameControllerRoam::AddRaceMarker(vec2 *pMarkers, int &Count, vec2 Pos)
{
	for(int i = 0; i < Count; i++)
	{
		if(distance(pMarkers[i], Pos) < 1.0f)
			return;
	}
	if(Count >= MAX_RACE_MARKERS)
		return;

	int Insert = Count;
	while(Insert > 0 && (pMarkers[Insert - 1].x > Pos.x ||
				   (pMarkers[Insert - 1].x == Pos.x && pMarkers[Insert - 1].y > Pos.y)))
	{
		pMarkers[Insert] = pMarkers[Insert - 1];
		Insert--;
	}
	pMarkers[Insert] = Pos;
	Count++;
}

bool CGameControllerRoam::OnEntity(int Index, vec2 Pos)
{
	if((Index == ENTITY_SPAWN || Index == ENTITY_SPAWN_RED || Index == ENTITY_SPAWN_BLUE) &&
	   m_NumSpawnPoints < MAX_ROAM_SPAWNS)
		m_aSpawnPoints[m_NumSpawnPoints++] = Pos;
	else if(Index == ENTITY_CHECKPOINT)
	{
		AddRaceMarker(m_aCheckpoints, m_NumCheckpoints, Pos);
		return true;
	}
	else if(Index == ENTITY_FINISH)
	{
		AddRaceMarker(m_aFinishes, m_NumFinishes, Pos);
		return true;
	}

	return IGameController::OnEntity(Index, Pos);
}

bool CGameControllerRoam::CanSpawn(int Team, vec2 *pOutPos, bool IsBot)
{
	if(Team == TEAM_SPECTATORS)
		return false;

	if(IsBot)
	{
		if(m_BotSpawnNum <= 0)
			return false;
		*pOutPos = m_aBotSpawn[--m_BotSpawnNum];
	}
	else if(!GameServer()->GetRoamSpawnPos(pOutPos))
	{
		if(m_NumSpawnPoints <= 0)
			return false;
		*pOutPos = m_aSpawnPoints[rand() % m_NumSpawnPoints];
	}

	GameServer()->ActivateBlockEntities((int)pOutPos->x);
	return true;
}

void CGameControllerRoam::TickRace()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || pPlayer->m_IsBot || !m_aRace[i].m_Active || m_aRace[i].m_FinishTick >= 0)
			continue;

		CCharacter *pCharacter = pPlayer->GetCharacter();
		if(!pCharacter)
			continue;

		CRaceState &State = m_aRace[i];
		if(State.m_NextCheckpoint < m_NumCheckpoints &&
		   distance(pCharacter->m_Pos, m_aCheckpoints[State.m_NextCheckpoint]) <= 48.0f)
		{
			State.m_NextCheckpoint++;
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Checkpoint %d/%d", State.m_NextCheckpoint, m_NumCheckpoints);
			GameServer()->SendBroadcast(aBuf, i);
		}

		if(State.m_NextCheckpoint < m_NumCheckpoints)
			continue;

		for(int Finish = 0; Finish < m_NumFinishes; Finish++)
		{
			if(distance(pCharacter->m_Pos, m_aFinishes[Finish]) > 48.0f)
				continue;

			State.m_FinishTick = Server()->Tick();
			const int Time = (State.m_FinishTick - State.m_StartTick) * 100 / Server()->TickSpeed();
			pPlayer->m_Score = -Time;
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Finished in %d:%02d.%02d", Time / 6000, (Time / 100) % 60, Time % 100);
			GameServer()->SendBroadcast(aBuf, i);
			break;
		}
	}
}

void CGameControllerRoam::Tick()
{
	IGameController::Tick();
	// AutoBalance();
	GameServer()->UpdateAI();
	TickRace();

	// kick unwanted bots
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer)
			continue;

		if(pPlayer->m_IsBot && pPlayer->m_ToBeKicked)
			GameServer()->KickBot(pPlayer->GetCID());
	}
}

void CGameControllerRoam::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_RaceInfo *pRaceInfo =
		(CNetObj_RaceInfo *)Server()->SnapNewItem(NETOBJTYPE_RACEINFO, 0, sizeof(CNetObj_RaceInfo));
	if(pRaceInfo)
		pRaceInfo->m_NumCheckpoints = m_NumCheckpoints;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || pPlayer->m_IsBot || !m_aRace[i].m_Active)
			continue;

		CNetObj_RacePlayer *pRacePlayer =
			(CNetObj_RacePlayer *)Server()->SnapNewItem(NETOBJTYPE_RACEPLAYER, i, sizeof(CNetObj_RacePlayer));
		if(!pRacePlayer)
			continue;

		const CRaceState &State = m_aRace[i];
		pRacePlayer->m_ClientID = i;
		pRacePlayer->m_StartTick = State.m_StartTick;
		pRacePlayer->m_Time = State.m_FinishTick < 0 ? -1 :
			(State.m_FinishTick - State.m_StartTick) * 100 / Server()->TickSpeed();
		pRacePlayer->m_Checkpoint = min(State.m_NextCheckpoint, m_NumCheckpoints);
	}
}

void CGameControllerRoam::DoWincheck()
{
	if(m_GameOverTick != -1 || m_Warmup || GameServer()->m_World.m_ResetRequested)
		return;

	bool HasRacers = false;
	bool AllFinished = true;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || pPlayer->m_IsBot || pPlayer->GetTeam() == TEAM_SPECTATORS || !m_aRace[i].m_Active)
			continue;
		HasRacers = true;
		if(m_aRace[i].m_FinishTick < 0)
			AllFinished = false;
	}

	const bool TimeLimitReached = g_Config.m_SvTimelimit > 0 &&
		(Server()->Tick() - m_RoundStartTick) >= g_Config.m_SvTimelimit * Server()->TickSpeed() * 60;
	if((HasRacers && AllFinished) || TimeLimitReached)
		EndRound();
}
