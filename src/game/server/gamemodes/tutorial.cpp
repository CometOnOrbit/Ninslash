#include "tutorial.h"

#include <cstdlib>

#include <engine/shared/config.h>
#include <game/pve/tutorial.h>
#include <game/server/ai.h>
#include <game/server/ai/dm_ai.h>
#include <game/server/ai/inv/alien1_ai.h>
#include <game/server/entities/character.h>
#include <game/server/entities/building.h>
#include <game/server/entities/radar.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_director.h>
#include <game/server/tutorial_director.h>

CGameControllerTutorial::CGameControllerTutorial(CGameContext *pGameServer)
	: IGameController(pGameServer), m_NumTargetSpawnPoints(0), m_TargetSpawnRotation(0), m_TargetSlotsReported(false),
	  m_CurrentTargetPos(0, 0), m_NumObjectiveRadars(0), m_pDoor(0), m_DoorOpened(false)
{
	for(int i = 0; i < MAX_TUTORIAL_TARGET_SLOTS; i++)
		m_aTargetSpawnPoints[i] = vec2(0, 0);
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aRespawnNearTarget[i] = false;
	for(int i = 0; i < 4; i++)
		m_apObjectiveRadars[i] = 0;
	m_pDoor = new CServerRadar(&GameServer()->m_World, RADAR_DOOR);
	m_pGameType = "Tutorial";
	const int Chapter = clamp(g_Config.m_SvTutorialChapter, 1, (int)NUM_TUTORIAL_CHAPTERS);
	g_Config.m_SvTutorialMode = 1;
	if(TutorialChapterForcesBuilding(Chapter))
		g_Config.m_SvEnableBuilding = 1;
	m_GameFlags = Chapter == TUTORIAL_CHAPTER_MULTIPLAYER ? 0 : GAMEFLAG_COOP;
	if(g_Config.m_SvEnableBuilding || TutorialChapterForcesBuilding(Chapter))
		m_GameFlags |= GAMEFLAG_BUILD;
	g_Config.m_SvDisablePVP = Chapter == TUTORIAL_CHAPTER_MULTIPLAYER ? 0 : 1;
	g_Config.m_SvSurvivalMode = 0;
	g_Config.m_SvPveContracts = 0;
	g_Config.m_SvMapGenLevel = Chapter;
	g_Config.m_SvMapGenSeed = TutorialFixedSeed(Chapter);
	g_Config.m_SvMapGenRandSeed = 0;
	dbg_msg("tutorial",
			"TUT controller chapter=%d seed=%d pvp=%d",
			Chapter,
			g_Config.m_SvMapGenSeed,
			Chapter == TUTORIAL_CHAPTER_MULTIPLAYER ? 1 : 0);
}

void CGameControllerTutorial::AddTargetSpawn(vec2 Pos)
{
	if(m_NumTargetSpawnPoints >= MAX_TUTORIAL_TARGET_SLOTS)
		return;
	for(int i = 0; i < m_NumTargetSpawnPoints; i++)
		if(distance(m_aTargetSpawnPoints[i], Pos) < 16.0f)
			return;
	m_aTargetSpawnPoints[m_NumTargetSpawnPoints++] = Pos;
}

bool CGameControllerTutorial::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_SPAWN)
		dbg_msg("tutorial", "player spawn registered at %.0f,%.0f", Pos.x, Pos.y);
	if(Index == ENTITY_SWITCH && g_Config.m_SvTutorialChapter == TUTORIAL_CHAPTER_OBJECTIVES)
	{
		if(!IGameController::OnEntity(Index, Pos))
			return false;
		if(m_NumObjectiveRadars < 4)
		{
			CServerRadar *pRadar = new CServerRadar(&GameServer()->m_World, RADAR_REACTOR);
			pRadar->Activate(Pos + vec2(0, -10));
			m_apObjectiveRadars[m_NumObjectiveRadars++] = pRadar;
			dbg_msg("tutorial", "objective switch and radar registered at %.0f,%.0f", Pos.x, Pos.y);
		}
		return true;
	}
	if(Index == ENTITY_ENEMYSPAWN)
	{
		AddTargetSpawn(Pos);
		return true;
	}
	return IGameController::OnEntity(Index, Pos);
}

void CGameControllerTutorial::ClearObjectiveRadars()
{
	for(int i = 0; i < m_NumObjectiveRadars; i++)
	{
		if(!m_apObjectiveRadars[i])
			continue;
		m_apObjectiveRadars[i]->Deactivate();
		GameServer()->m_World.DestroyEntity(m_apObjectiveRadars[i]);
		m_apObjectiveRadars[i] = 0;
	}
	m_NumObjectiveRadars = 0;
}

void CGameControllerTutorial::RefreshObjectiveRadars()
{
	ClearObjectiveRadars();
	for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding;
		pBuilding = (CBuilding *)pBuilding->TypeNext())
	{
		if(pBuilding->m_Type != BUILDING_SWITCH || pBuilding->m_aStatus[BSTATUS_ON] || m_NumObjectiveRadars >= 4)
			continue;
		CServerRadar *pRadar = new CServerRadar(&GameServer()->m_World, RADAR_REACTOR);
		pRadar->Activate(pBuilding->m_Pos);
		m_apObjectiveRadars[m_NumObjectiveRadars++] = pRadar;
	}
	dbg_msg("tutorial", "refreshed %d remaining objective radars", m_NumObjectiveRadars);
}

void CGameControllerTutorial::AddEnemy(vec2 Pos)
{
	AddTargetSpawn(Pos);
}

bool CGameControllerTutorial::GetSpawnPos(int Team, vec2 *pOutPos)
{
	(void)Team;
	if(!pOutPos || m_NumTargetSpawnPoints <= 0)
		return false;
	m_TargetSpawnRotation = (m_TargetSpawnRotation + 1) % m_NumTargetSpawnPoints;
	*pOutPos = m_aTargetSpawnPoints[m_TargetSpawnRotation];
	return true;
}

bool CGameControllerTutorial::GetRespawnNearTarget(vec2 *pOutPos) const
{
	if(!pOutPos || m_NumTargetSpawnPoints <= 0)
		return false;
	const vec2 Anchor =
		m_CurrentTargetPos.x != 0.0f || m_CurrentTargetPos.y != 0.0f ? m_CurrentTargetPos : m_aTargetSpawnPoints[0];
	for(int Pass = 0; Pass < 2; Pass++)
		for(int i = 0; i < m_NumTargetSpawnPoints; i++)
		{
			const vec2 Candidate = m_aTargetSpawnPoints[i];
			const float Dist = distance(Candidate, Anchor);
			if((Pass == 0 && (Dist < 48.0f || Dist > 420.0f)) ||
			   GameServer()->Collision()->TestBox(Candidate, vec2(32.0f, 74.0f)))
				continue;
			CCharacter *apCharacters[MAX_CLIENTS];
			const int Num = GameServer()->m_World.FindEntities(
				Candidate, 48.0f, (CEntity **)apCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			if(Num > 0)
				continue;
			*pOutPos = Candidate;
			return true;
		}
	return false;
}

bool CGameControllerTutorial::CanSpawn(int Team, vec2 *pOutPos, bool IsBot)
{
	if(IsBot)
		return GetSpawnPos(1, pOutPos);
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
		if(GameServer()->m_apPlayers[ClientID] && !GameServer()->m_apPlayers[ClientID]->m_IsBot &&
		   GameServer()->m_apPlayers[ClientID]->GetTeam() == Team && m_aRespawnNearTarget[ClientID])
		{
			m_aRespawnNearTarget[ClientID] = false;
			if(GetRespawnNearTarget(pOutPos))
			{
				dbg_msg("tutorial",
						"respawned player %d near current target at %.0f,%.0f",
						ClientID,
						pOutPos->x,
						pOutPos->y);
				return true;
			}
			break;
		}
	if(IGameController::CanSpawn(Team, pOutPos, false))
		return true;
	if(GetRespawnNearTarget(pOutPos))
	{
		dbg_msg(
			"tutorial", "using controlled target slot as fallback player spawn at %.0f,%.0f", pOutPos->x, pOutPos->y);
		return true;
	}
	return false;
}

int CGameControllerTutorial::DesiredBots() const
{
	if(!GameServer()->m_pTutorialDirector || !GameServer()->m_pTutorialDirector->State().m_Active)
		return 0;
	const CTutorialState &State = GameServer()->m_pTutorialDirector->State();
	switch(State.m_Chapter)
	{
		case TUTORIAL_CHAPTER_DEPLOYMENT:
			return 1;
		case TUTORIAL_CHAPTER_COMBAT:
			return State.m_Step == 0 ? 3 : State.m_Step == 2 ? 1 : 0;
		case TUTORIAL_CHAPTER_FORGE:
			return State.m_Step == 2 ? 3 : 0;
		case TUTORIAL_CHAPTER_MULTIPLAYER:
			return State.m_Step == 0 ? 3 : 0;
		default:
			return 0;
	}
}

void CGameControllerTutorial::UpdateControlledBots()
{
	const int Wanted = DesiredBots();
	const int Current = CountBots();
	if(Current < Wanted)
		GameServer()->AddBot();
	else if(Current > Wanted)
		GameServer()->KickOneBot();
}

void CGameControllerTutorial::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr, RequestAI);
	if(!RequestAI)
	{
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnPlayerSpawn(pChr->GetPlayer()->GetCID());
		return;
	}

	pChr->m_IsBot = true;
	pChr->m_IsBot = true;
	pChr->m_TeeInfos.m_IsBot = true;
	GameServer()->GetAISkin(
		&pChr->m_AISkin, g_Config.m_SvTutorialChapter == TUTORIAL_CHAPTER_MULTIPLAYER, 1, 1);
	pChr->SetAISkin();
	m_CurrentTargetPos = pChr->m_Pos;
	dbg_msg("tutorial",
			"spawned controlled %s at %.0f,%.0f",
			g_Config.m_SvTutorialChapter == TUTORIAL_CHAPTER_DEPLOYMENT ? "target" : "bot",
			pChr->m_Pos.x,
			pChr->m_Pos.y);
	// The deployment target deliberately has no AI and therefore cannot attack.
	if(g_Config.m_SvTutorialChapter == TUTORIAL_CHAPTER_DEPLOYMENT)
		return;
	if(g_Config.m_SvTutorialChapter == TUTORIAL_CHAPTER_MULTIPLAYER)
		pChr->m_pAI = new CAIdm(GameServer(), pChr);
	else
		pChr->m_pAI = new CAIalien1(GameServer(), pChr, 1);
}

int CGameControllerTutorial::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, const CAttackSource &Source)
{
	if(pVictim && !pVictim->m_IsBot)
	{
		const int ClientID = pVictim->GetCID();
		if(ClientID >= 0 && ClientID < MAX_CLIENTS)
			m_aRespawnNearTarget[ClientID] = true;
	}
	else if(pVictim)
		m_CurrentTargetPos = pVictim->m_Pos;
	return IGameController::OnCharacterDeath(pVictim, pKiller, Source);
}

void CGameControllerTutorial::OpenDoorIfReady()
{
	if(m_DoorOpened || !GameServer()->m_pTutorialDirector)
		return;
	const CTutorialState &State = GameServer()->m_pTutorialDirector->State();
	if(!State.m_Active || !TutorialStepIsDoor(State.m_Chapter, State.m_Step))
		return;
	m_DoorOpened = TriggerEscape();
}

void CGameControllerTutorial::DisplayExit(vec2 Pos)
{
	if(m_pDoor)
		m_pDoor->Activate(Pos);
}

void CGameControllerTutorial::NextLevel(int CID)
{
	if(IsGameOver())
		return;
	if(!GameServer()->m_pTutorialDirector)
		return;
	const CTutorialState &State = GameServer()->m_pTutorialDirector->State();
	if(!TutorialStepIsDoor(State.m_Chapter, State.m_Step))
		return;
	GameServer()->m_pTutorialDirector->OnGameplayProgress(CID, TUTORIAL_EVENT_DOOR);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? GameServer()->m_apPlayers[CID] : 0;
	if(pPlayer && pPlayer->GetCharacter() && !pPlayer->GetCharacter()->IgnoreCollision())
		pPlayer->GetCharacter()->Warp();
	EndRound();
}

void CGameControllerTutorial::Tick()
{
	IGameController::Tick();
	if(!m_TargetSlotsReported)
	{
		dbg_msg("tutorial", "registered %d controlled target spawn slots", m_NumTargetSpawnPoints);
		m_TargetSlotsReported = true;
	}
	OpenDoorIfReady();
	if(Server()->Tick() % max(1, Server()->TickSpeed() / 2) == 0)
		UpdateControlledBots();
	GameServer()->UpdateAI();
}

void CGameControllerTutorial::OnSwitchTriggered()
{
	if(GameServer()->m_pTutorialDirector)
		GameServer()->m_pTutorialDirector->OnGameplayProgress(-1, TUTORIAL_EVENT_OBJECTIVE);
	RefreshObjectiveRadars();
	// Tutorial switches only advance their controlled objective. They never
	// start Invasion acid, floors, quests, or map transitions.
}
