#include <engine/shared/config.h>

#include <game/mapitems.h>

#include <game/server/entities/character.h>
#include <game/server/entities/radar.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/aiskin.h>

#include "roam.h"
#include <game/server/roam_mapgen_layout.h>

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
	m_CourseValid = false;
	m_CourseFrozen = false;
	m_SpawningClient = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
		ResetRace(i);
	g_Config.m_SvDisablePVP = true;
	m_GameFlags = GAMEFLAG_COOP;
	// The base controller begins with global-acid path filtering enabled and
	// normally clears it on the first Tick. Roam validates the course before
	// that tick and has no acid mechanic, so low gates must remain navigable.
	GameServer()->Collision()->m_GlobalAcid = false;

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
		pChr->GetPlayer()->m_pAI = new CAIroam(GameServer(), pChr->GetPlayer(), g_Config.m_SvBotLevel);
		pChr->GetPlayer()->SetAISkin();

		// m_Skin = SKIN_ALIEN1;
		// Player()->SetCustomSkin(m_Skin);
	}

	const int ClientID = pChr->GetPlayer()->GetCID();
	if(!m_aRace[ClientID].m_Active || m_aRace[ClientID].m_FinishTick >= 0)
	{
		ResetRace(ClientID);
		m_aRace[ClientID].m_StartTick = Server()->Tick();
		m_aRace[ClientID].m_Active = true;
	}
	pChr->GetPlayer()->m_Score = -9999999;
	m_aRace[ClientID].m_PreviousPos = pChr->m_Pos;
	m_aRace[ClientID].m_HasPreviousPos = true;
}

void CGameControllerRoam::ResetRace(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	m_aRace[ClientID].m_StartTick = 0;
	m_aRace[ClientID].m_NextCheckpoint = 0;
	m_aRace[ClientID].m_FinishTick = -1;
	m_aRace[ClientID].m_Active = false;
	m_aRace[ClientID].m_HasRespawn = false;
	m_aRace[ClientID].m_RespawnPos = vec2(0, 0);
	m_aRace[ClientID].m_HasPreviousPos = false;
	m_aRace[ClientID].m_PreviousPos = vec2(0, 0);
}

int CGameControllerRoam::OnCharacterDeath(class CCharacter *pVictim,
										  class CPlayer *pKiller,
										  const CAttackSource &Source)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Source);

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

void CGameControllerRoam::AddRaceGate(CRaceGate *pGates, int &Count, vec2 Min, vec2 Max, int CourseOrdinal, vec2 RespawnPos)
{
	for(int i = 0; i < Count; i++)
	{
		if(distance(pGates[i].m_Min, Min) < 1.0f && distance(pGates[i].m_Max, Max) < 1.0f)
			return;
	}
	if(Count >= MAX_RACE_MARKERS)
		return;

	int Insert = Count;
	while(Insert > 0)
	{
		const CRaceGate &Prev = pGates[Insert - 1];
		const bool Move =
			CourseOrdinal >= 0 ?
				(Prev.m_CourseOrdinal > CourseOrdinal) :
				(Prev.m_Min.x > Min.x || (Prev.m_Min.x == Min.x && Prev.m_Min.y > Min.y));
		if(!Move)
			break;
		pGates[Insert] = Prev;
		Insert--;
	}
	pGates[Insert].m_Min = Min;
	pGates[Insert].m_Max = Max;
	pGates[Insert].m_RespawnPos = RespawnPos;
	pGates[Insert].m_CourseOrdinal = CourseOrdinal;
	Count++;
}

bool CGameControllerRoam::OnEntity(int Index, vec2 Pos)
{
	if(m_CourseFrozen && (Index == ENTITY_CHECKPOINT || Index == ENTITY_FINISH))
		return true;

	if((Index == ENTITY_SPAWN || Index == ENTITY_SPAWN_RED || Index == ENTITY_SPAWN_BLUE) &&
	   m_NumSpawnPoints < MAX_ROAM_SPAWNS)
		m_aSpawnPoints[m_NumSpawnPoints++] = Pos;
	else if(Index == ENTITY_CHECKPOINT)
	{
		AddRaceGate(m_aCheckpoints, m_NumCheckpoints, Pos - vec2(48, 48), Pos + vec2(48, 48), -1, Pos);
		return true;
	}
	else if(Index == ENTITY_FINISH)
	{
		AddRaceGate(m_aFinishes, m_NumFinishes, Pos - vec2(48, 48), Pos + vec2(48, 48), -1, Pos);
		return true;
	}

	return IGameController::OnEntity(Index, Pos);
}

bool CGameControllerRoam::OnCourseEntity(int Index, vec2 Pos, int CourseOrdinal, vec2 RespawnPos)
{
	if(Index == ENTITY_CHECKPOINT || Index == ENTITY_FINISH)
		return RegisterRaceGate(Index, Pos - vec2(48, 48), Pos + vec2(48, 48), CourseOrdinal, RespawnPos);
	return OnEntity(Index, Pos);
}

bool CGameControllerRoam::RegisterRaceGate(int Index, vec2 Min, vec2 Max, int CourseOrdinal, vec2 RespawnPos)
{
	if(m_CourseFrozen && (Index == ENTITY_CHECKPOINT || Index == ENTITY_FINISH)) return true;
	if(Index == ENTITY_CHECKPOINT)
	{
		AddRaceGate(m_aCheckpoints, m_NumCheckpoints, Min, Max, CourseOrdinal, RespawnPos);
		return true;
	}
	if(Index == ENTITY_FINISH)
	{
		AddRaceGate(m_aFinishes, m_NumFinishes, Min, Max, CourseOrdinal, RespawnPos);
		return true;
	}
	return false;
}

bool CGameControllerRoam::GetRaceTarget(int ClientID, vec2 *pTargetPos, int *pCourseOrdinal) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_aRace[ClientID].m_Active || m_aRace[ClientID].m_FinishTick >= 0)
		return false;
	const CRaceState &State = m_aRace[ClientID];
	const CRaceGate *pGate = 0;
	if(State.m_NextCheckpoint < m_NumCheckpoints)
		pGate = &m_aCheckpoints[State.m_NextCheckpoint];
	else if(m_NumFinishes > 0)
		pGate = &m_aFinishes[0];
	if(!pGate)
		return false;
	if(pTargetPos)
		*pTargetPos = (pGate->m_Min + pGate->m_Max) * 0.5f;
	if(pCourseOrdinal)
		*pCourseOrdinal = pGate->m_CourseOrdinal;
	return true;
}

void CGameControllerRoam::FreezeCourse()
{
	m_CourseFrozen = true;
}

void CGameControllerRoam::FinalizeCourse(bool HasModularSpawn)
{
	const int SpawnCount = m_NumSpawnPoints + (HasModularSpawn ? 1 : 0);
	// Path-v2 has one checkpoint at every placement after the start. Match the
	// generated path exactly so a partially registered variable-length course
	// can never start.
	CMapPath *pPath = GameServer()->Collision()->GetMapPath();
	const int RequiredCheckpoints = pPath ? pPath->PlacementCount() - 1 : 2;
	m_CourseValid = SpawnCount >= 1 && (pPath ? m_NumCheckpoints == RequiredCheckpoints : m_NumCheckpoints >= RequiredCheckpoints) &&
		m_NumFinishes >= 1;
	if(m_CourseValid && GameServer()->Collision()->GetMapPath())
	{
		vec2 Previous;
		bool NavigationValid = GameServer()->GetRoamSpawnPos(&Previous);
		auto CanNavigate = [this](vec2 From, vec2 To)
		{
			CCollision *pCollision = GameServer()->Collision();
			if(distance(From, To) < 1200.0f && !pCollision->FastIntersectLine(From, To))
				return true;
			const bool Found = pCollision->AStar(To, From);
			CWaypointPath *pPath = pCollision->GetPath();
			pCollision->ForgetAboutThePath();
			delete pPath;
			return Found;
		};
		for(int Checkpoint = 0; NavigationValid && Checkpoint < m_NumCheckpoints; Checkpoint++)
		{
			const vec2 Target = (m_aCheckpoints[Checkpoint].m_Min + m_aCheckpoints[Checkpoint].m_Max) * 0.5f;
			NavigationValid = CanNavigate(Previous, Target);
			if(!NavigationValid)
			{
				const vec2 FromWaypoint = GameServer()->Collision()->GetClosestWaypointPos(Previous);
				const vec2 ToWaypoint = GameServer()->Collision()->GetClosestWaypointPos(Target);
				dbg_msg("roam", "ERROR: waypoint graph cannot reach checkpoint %d from %.0f,%.0f (wp %.0f,%.0f) to %.0f,%.0f (wp %.0f,%.0f)",
					Checkpoint + 1, Previous.x, Previous.y, FromWaypoint.x, FromWaypoint.y,
					Target.x, Target.y, ToWaypoint.x, ToWaypoint.y);
			}
			Previous = Target;
		}
		if(NavigationValid)
		{
			const vec2 Target = (m_aFinishes[0].m_Min + m_aFinishes[0].m_Max) * 0.5f;
			NavigationValid = CanNavigate(Previous, Target);
		}
		if(!NavigationValid)
			dbg_msg("roam", "ERROR: race waypoint graph does not connect every ordered gate");
		m_CourseValid = NavigationValid;
	}
	m_CourseFrozen = true;
	if(m_CourseValid)
	{
		dbg_msg("roam", "course ready: %d spawns, %d checkpoints, %d finishes",
			SpawnCount, m_NumCheckpoints, m_NumFinishes);
	}
	else
	{
		dbg_msg("roam", "ERROR: incomplete course: %d spawns, %d checkpoints, %d finishes",
			SpawnCount, m_NumCheckpoints, m_NumFinishes);
	}
}

bool CGameControllerRoam::CanSpawn(int Team, vec2 *pOutPos, bool IsBot)
{
	if(Team == TEAM_SPECTATORS || !m_CourseValid)
		return false;

	if(m_SpawningClient >= 0 && m_SpawningClient < MAX_CLIENTS &&
		m_aRace[m_SpawningClient].m_Active && m_aRace[m_SpawningClient].m_FinishTick < 0 &&
		m_aRace[m_SpawningClient].m_HasRespawn)
	{
		*pOutPos = m_aRace[m_SpawningClient].m_RespawnPos;
	}
	else if(CMapPath *pPath = GameServer()->Collision()->GetMapPath())
	{
		const CMapPathPlacementData *pStart = pPath->Placement(0);
		if(!pStart)
			return false;
		const int Slot = m_SpawningClient >= 0 ? m_SpawningClient % 4 : 0;
		const int WorldX = pStart->m_GridX * pPath->Info().m_ChunkWidth + RoamMapGen::SpawnLocalX(Slot);
		const int WorldY = pStart->m_GridY * pPath->Info().m_ChunkHeight + RoamMapGen::SpawnLocalY();
		*pOutPos = vec2(WorldX * 32.0f, WorldY * 32.0f);
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

bool CGameControllerRoam::CanCharacterSpawn(int ClientID)
{
	m_SpawningClient = ClientID;
	return IGameController::CanCharacterSpawn(ClientID);
}

void CGameControllerRoam::TickRace()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || !m_aRace[i].m_Active || m_aRace[i].m_FinishTick >= 0)
			continue;

		CCharacter *pCharacter = pPlayer->GetCharacter();
		if(!pCharacter)
			continue;

		CRaceState &State = m_aRace[i];
		const vec2 CurrentPos = pCharacter->m_Pos;
		// PlayerInfo score is used only as the sort key in race mode; the client
		// renders the authoritative RacePlayer time. Keep unfinished racers sorted
		// by checkpoint progress and then elapsed time so bots and humans share a
		// meaningful live ranking.
		const int Elapsed = max(0, (Server()->Tick() - State.m_StartTick) * 100 / Server()->TickSpeed());
		pPlayer->m_Score = -10000000 + State.m_NextCheckpoint * 100000 - min(Elapsed, 99999);
		if(!State.m_HasPreviousPos)
		{
			State.m_PreviousPos = CurrentPos;
			State.m_HasPreviousPos = true;
		}
		if(State.m_NextCheckpoint < m_NumCheckpoints &&
		   RoamMapGen::SegmentIntersectsAabb(State.m_PreviousPos, CurrentPos,
			   m_aCheckpoints[State.m_NextCheckpoint].m_Min, m_aCheckpoints[State.m_NextCheckpoint].m_Max))
		{
			State.m_NextCheckpoint++;
			State.m_RespawnPos = m_aCheckpoints[State.m_NextCheckpoint - 1].m_RespawnPos;
			State.m_HasRespawn = true;
			if(!pPlayer->m_IsBot)
				GameServer()->SendBroadcastFormat(i, false, "Checkpoint %d/%d", State.m_NextCheckpoint, m_NumCheckpoints);
		}

		if(State.m_NextCheckpoint < m_NumCheckpoints)
		{
			State.m_PreviousPos = CurrentPos;
			continue;
		}

		for(int Finish = 0; Finish < m_NumFinishes; Finish++)
		{
			if(!RoamMapGen::SegmentIntersectsAabb(State.m_PreviousPos, CurrentPos,
				   m_aFinishes[Finish].m_Min, m_aFinishes[Finish].m_Max))
				continue;

			State.m_FinishTick = Server()->Tick();
			const int Time = (State.m_FinishTick - State.m_StartTick) * 100 / Server()->TickSpeed();
			pPlayer->m_Score = -Time;
			if(!pPlayer->m_IsBot)
				GameServer()->SendBroadcastFormat(i, false, "Finished in %d:%02d.%02d", Time / 6000, (Time / 100) % 60, Time % 100);
			break;
		}
		State.m_PreviousPos = CurrentPos;
	}
}

void CGameControllerRoam::Tick()
{
	IGameController::Tick();
	AutoBalance();
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
		if(!pPlayer || !m_aRace[i].m_Active)
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
