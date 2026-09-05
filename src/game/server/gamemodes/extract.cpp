#include <engine/shared/config.h>
#include <engine/platform_events.h>

#include <game/challenge_variant.h>
#include <base/deterministic_random.h>
#include <game/questinfo.h>
#include <game/mapitems.h>
#include <game/weapons.h>
#include <game/server/entities/character.h>
#include <game/server/entities/building.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/bosspool.h>
#include <game/server/entities/droid_crawler.h>
#include <game/server/entities/radar.h>
#include <game/server/pve_bots.h>
#include <game/server/pve_director.h>

#include "extract.h"

CGameControllerExtract::CGameControllerExtract(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "Extraction";
	m_GameFlags = GAMEFLAG_COOP;
	m_GameState = STATE_STARTING;

	for(int i = 0; i < MAX_ENEMIES; i++)
		m_aEnemySpawnPos[i] = vec2(0, 0);

	m_Phase = 0;
	m_SwitchesRequired = 1;
	m_SwitchesActivated = 0;
	m_AvailableSwitches = 0;
	m_Evacuated = 0;
	m_EvacNeeded = 1;
	m_DoorOpen = false;
	m_MidBossSpawned = false;
	m_RoundOverTick = 0;
	m_DeadlineTick = 0;
	m_StartTick = 0;
	m_EnemiesLeft = 0;
	m_NumEnemySpawnPos = 0;
	m_SpawnPosRotation = 0;
	m_BotSpawnTick = 0;
	m_TriggerTick = 0;
	m_TriggerLevel = 6;
	m_Win = false;
	m_EscapePressure = false;
	m_HadHumanAlive = false;
	m_HumanDeaths = 0;
	m_TasksCompleted = 0;
	m_TasksTotal = 0;
	m_ActiveTask = -1;
	m_TaskCount = 0;
	m_TaskZonesCollected = 0;
	m_TaskTimerTick = 0;
	m_ActiveEvent = EXTRACT_EVT_NONE;
	m_EventActionTick = 0;
	m_LastEventTask = -1;
	m_pTaskRadar = 0;
	m_EscapeWave = 0;
	m_EscapeWaveTick = 0;
	m_ReinforceTick = 0;
	for(int i = 0; i < MAX_EXTRACT_TASKS; i++)
	{
		m_aTaskType[i] = EXTRACT_TASK_NONE;
		m_aTaskProgress[i] = 0;
		m_aTaskTarget[i] = 0;
		m_aTaskZone[i] = vec2(0.0f, 0.0f);
		m_aTaskZones[i] = -1;
	}
	m_RogueliteStarted = false;
	m_RogueliteWaitTick = Server()->Tick() + Server()->TickSpeed() * 2;
	m_RogueliteStageStarted = false;
	m_MidBossPerkOffered = false;
	m_DoorChoicePending = false;
	m_DoorChoiceStarted = false;
	m_EliteContractSpawned = false;
	m_pMidBoss = 0;
	m_pDoor = new CServerRadar(&GameServer()->m_World, RADAR_DOOR);

	g_Config.m_SvOneHitKill = 0;
	g_Config.m_SvWarmup = 0;
	g_Config.m_SvScorelimit = 0;
	g_Config.m_SvEnableBuilding = 1;
	g_Config.m_SvDisablePVP = 1;
	g_Config.m_SvSurvivalTime = 0;
	g_Config.m_SvSurvivalAcid = 0;
	dbg_msg("extract",
			"rules: time_limit=%d roguelite=%d contracts=%d seed=%d random_seed=%d",
			g_Config.m_SvTimelimit,
			g_Config.m_SvPveRoguelite,
			g_Config.m_SvPveContracts,
			g_Config.m_SvMapGenSeed,
			g_Config.m_SvMapGenRandSeed);

	if(g_Config.m_SvEnableBuilding)
		m_GameFlags |= GAMEFLAG_BUILD;
	if(g_Config.m_SvSurvivalMode)
		m_GameFlags |= GAMEFLAG_SURVIVAL;
}

CGameControllerExtract::~CGameControllerExtract()
{
}

bool CGameControllerExtract::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_ENEMYSPAWN)
	{
		if(m_NumEnemySpawnPos < MAX_ENEMIES)
		{
			m_aEnemySpawnPos[m_NumEnemySpawnPos++] = Pos;
		}
		return true;
	}
	if(Index == ENTITY_SWITCH)
		m_AvailableSwitches++;
	if(Index == ENTITY_EXTRACT_ZONE && m_TaskZonesCollected < MAX_EXTRACT_TASKS)
	{
		m_aTaskZone[m_TaskZonesCollected] = Pos;
		m_TaskZonesCollected++;
		return true;
	}
	return IGameController::OnEntity(Index, Pos);
}

bool CGameControllerExtract::GetSpawnPos(int Team, vec2 *pOutPos)
{
	if(!pOutPos || m_NumEnemySpawnPos <= 0)
		return false;
	m_SpawnPosRotation = (m_SpawnPosRotation + 1) % m_NumEnemySpawnPos;
	*pOutPos = m_aEnemySpawnPos[m_SpawnPosRotation];
	return true;
}

bool CGameControllerExtract::GetBossSpawnPos(vec2 *pOutPos)
{
	return FindBossSpawnPosition(
		&GameServer()->m_World, m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, pOutPos);
}

bool CGameControllerExtract::CanSpawn(int Team, vec2 *pOutPos, bool IsBot)
{
	CSpawnEval Eval;

	if(Team == TEAM_SPECTATORS)
		return false;

	if(IsBot)
	{
		if(m_EnemiesLeft <= 0)
			return false;
		if(GetSpawnPos(0, pOutPos))
			return true;
		EvaluateSpawnType(&Eval, 0);
		if(!Eval.m_Got)
			return false;
		*pOutPos = Eval.m_Pos;
		return true;
	}

	EvaluateSpawnType(&Eval, 0);
	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

int CGameControllerExtract::CountHumansAliveLocal() const
{
	int Alive = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *p = GameServer()->m_apPlayers[i];
		if(p && !p->m_IsBot && p->GetCharacter() && p->GetCharacter()->IsAlive())
			Alive++;
	}
	return Alive;
}

int CGameControllerExtract::CountHumanPlayersLocal() const
{
	int Humans = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *p = GameServer()->m_apPlayers[i];
		if(p && !p->m_IsBot && p->GetTeam() != TEAM_SPECTATORS)
			Humans++;
	}
	return Humans;
}

void CGameControllerExtract::SpawnInitialEnemies()
{
	const int Players = max(1, CountPlayers(0));
	const float CountScale = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->EnemyCountMultiplier() : 1.0f;
	m_EnemiesLeft = max(1, (int)((16 + Players * 4) * CountScale + 0.5f));
	const SThreatBudgetResult ThreatReplacement = SpawnThreatBudgetSpecialists(&GameServer()->m_World,
																			   m_aEnemySpawnPos,
																			   m_NumEnemySpawnPos,
																			   &m_SpawnPosRotation,
																			   EnemyLevel(),
																			   m_EnemiesLeft,
																			   16);
	m_EnemiesLeft -= ThreatReplacement.m_ThreatSpent;
	const int BotCap = max(0, 16 - ThreatReplacement.m_EntitiesSpawned);
	const int SpawnCount = min(m_EnemiesLeft, max(0, BotCap - CountBots()));
	for(int i = 0; i < SpawnCount; i++)
		GameServer()->AddBot();

	vec2 p;
	if(GetSpawnPos(0, &p))
		new CCrawler(&GameServer()->m_World, p + vec2(0, -80));
	if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_ELITE_HUNT)
	{
		if(!GetBossSpawnPos(&p))
			p = vec2(4000, 4000);
		CDroid *pBoss = SpawnBoss(&GameServer()->m_World, p, EnemyLevel() + 2);
		GameServer()->m_pPveDirector->RegisterEliteContractBoss(pBoss);
		m_EliteContractSpawned = true;
	}
	if(GetSpawnPos(0, &p))
		new CCrawler(&GameServer()->m_World, p + vec2(0, -80));
}

void CGameControllerExtract::SpawnMidBoss()
{
	if(m_MidBossSpawned)
		return;
	m_MidBossSpawned = true;
	vec2 p;
	if(!GetBossSpawnPos(&p))
		p = vec2(4000, 4000);
	m_pMidBoss = SpawnBoss(&GameServer()->m_World, p, max(10, EnemyLevel() + 2));
	const int SpawnCount = min(6, max(0, 18 - CountBots()));
	for(int i = 0; i < SpawnCount; i++)
	{
		m_EnemiesLeft++;
		GameServer()->AddBot();
	}
	if(GetSpawnPos(0, &p))
		new CCrawler(&GameServer()->m_World, p + vec2(0, -80));
	m_TriggerLevel = max(m_TriggerLevel, 10);
	TriggerAllBotAI(GameServer(), m_TriggerLevel);
	GameServer()->SendBroadcast("Mid-run boss! Hold the line", -1);
}

void CGameControllerExtract::SpawnEscapePressure()
{
	if(m_EscapePressure)
		return;
	m_EscapePressure = true;
	const float Pressure =
		GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ReinforcementMultiplier() : 1.0f;
	const int AddedThreat = (int)((8 + CountPlayers(0) * 2) * Pressure);
	const int ConcurrentCap = (int)(18 * Pressure);
	const SThreatBudgetResult ThreatReplacement = SpawnThreatBudgetSpecialists(
		&GameServer()->m_World,
		m_aEnemySpawnPos,
		m_NumEnemySpawnPos,
		&m_SpawnPosRotation,
		EnemyLevel(),
		AddedThreat,
		max(0, ConcurrentCap - CountBots() - CountAliveSpecialists(&GameServer()->m_World)));
	m_EnemiesLeft += AddedThreat - ThreatReplacement.m_ThreatSpent;
	const int RequestedBots = max(0, (int)(8 * Pressure) - ThreatReplacement.m_ThreatSpent);
	const int AvailableSlots = max(0, ConcurrentCap - CountBots() - CountAliveSpecialists(&GameServer()->m_World));
	const int SpawnCount = min(RequestedBots, AvailableSlots);
	for(int i = 0; i < SpawnCount; i++)
		GameServer()->AddBot();
	vec2 p;
	if(GetSpawnPos(0, &p))
		new CCrawler(&GameServer()->m_World, p + vec2(0, -80));
	m_TriggerLevel = max(m_TriggerLevel, 12);
	TriggerAllBotAI(GameServer(), m_TriggerLevel);
	GameServer()->SendBroadcast("Enemies flooding the exit!", -1);
}

int CGameControllerExtract::EnemyLevel() const
{
	int Mins = 0;
	if(m_StartTick)
		Mins = (Server()->Tick() - m_StartTick) / (Server()->TickSpeed() * 60);
	const int DifficultyTier = max(0, (g_Config.m_SvMapGenLevel - 1) / 10);
	return min(10, max(3, 3 + m_SwitchesActivated + Mins + m_Phase * 2 + DifficultyTier));
}

int CGameControllerExtract::ComputeRating() const
{
	const int TotalTicks = m_DeadlineTick - m_StartTick;
	const int TimeLeft = max(0, (m_DeadlineTick - Server()->Tick()) / Server()->TickSpeed());
	const float Ratio = TotalTicks > 0 ? TimeLeft / (float)max(1, TotalTicks / Server()->TickSpeed()) : 0.0f;
	const bool NoDeaths = m_HumanDeaths == 0;
	const bool AllTasks = m_TasksCompleted >= m_TasksTotal;
	if(NoDeaths && AllTasks && Ratio > 0.25f)
		return 3; // S
	if(NoDeaths && AllTasks)
		return 2; // A
	if(Ratio < 0.10f || !NoDeaths)
		return 0; // C
	return 1; // B
}

bool CGameControllerExtract::PlayerInRadius(const vec2 &Center, float Radius) const
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || pPlayer->m_IsBot || pPlayer->GetTeam() == TEAM_SPECTATORS)
			continue;
		CCharacter *pChar = pPlayer->GetCharacter();
		if(pChar && distance(pChar->m_Pos, Center) <= Radius)
			return true;
	}
	return false;
}

void CGameControllerExtract::PickTasks()
{
	static const struct
	{
		int m_Type;
		int m_Weight;
	} s_aPool[] = {
		{EXTRACT_TASK_SWITCHES, 30},
		{EXTRACT_TASK_ELIMINATE, 22},
		{EXTRACT_TASK_DEFEND, 18},
		{EXTRACT_TASK_COLLECT, 15},
		{EXTRACT_TASK_TIMED_CLEAR, 15},
	};
	const bool LockedRoute = GameServer()->m_pPveDirector &&
							 GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_LOCKED_ROUTE;
	const int Num = CountHumans() > 1 ? 3 : 2;
	m_TaskCount = 0;
	for(int t = 0; t < Num; t++)
	{
		int Chosen = -1;
		if(t == 0 && LockedRoute)
			Chosen = EXTRACT_TASK_SWITCHES;
		else
		{
			auto ZoneAvailable = [&](int Type)
			{
				// DEFEND/COLLECT need a zone anchor; without one they can
				// never complete (mapgen may fail to place any zone).
				if(Type != EXTRACT_TASK_DEFEND && Type != EXTRACT_TASK_COLLECT)
					return true;
				return m_TaskZonesCollected > m_TaskCount;
			};
			int Total = 0;
			for(int i = 0; i < 5; i++)
			{
				bool Used = false;
				for(int k = 0; k < m_TaskCount; k++)
					if(m_aTaskType[k] == s_aPool[i].m_Type)
						Used = true;
				if(Used || !ZoneAvailable(s_aPool[i].m_Type))
					continue;
				Total += s_aPool[i].m_Weight;
			}
			if(Total > 0)
			{
				// Deterministic per-seed stream so challenge runs with the same
				// seed pick the same task set (docs §3.2).
				CDeterministicRandom TaskRng(DeterministicSeed((unsigned long long)g_Config.m_SvMapGenSeed, "extract_tasks"));
				int Pick = TaskRng.NextInt(Total);
				for(int i = 0; i < 5; i++)
				{
					bool Used = false;
					for(int k = 0; k < m_TaskCount; k++)
						if(m_aTaskType[k] == s_aPool[i].m_Type)
							Used = true;
					if(Used || !ZoneAvailable(s_aPool[i].m_Type))
						continue;
					if(Pick < s_aPool[i].m_Weight)
					{
						Chosen = s_aPool[i].m_Type;
						break;
					}
					Pick -= s_aPool[i].m_Weight;
				}
			}
			if(Chosen < 0)
				Chosen = EXTRACT_TASK_SWITCHES;
		}
		m_aTaskType[m_TaskCount] = Chosen;
		m_aTaskProgress[m_TaskCount] = 0;
		m_aTaskTarget[m_TaskCount] = 0;
		m_aTaskZones[m_TaskCount] = m_TaskZonesCollected > m_TaskCount ? m_TaskCount : -1;
		m_TaskCount++;
	}
	m_TasksTotal = m_TaskCount;
	m_ActiveTask = 0;
	StartTask();
}

void CGameControllerExtract::StartTask()
{
	if(m_ActiveTask < 0 || m_ActiveTask >= m_TaskCount)
		return;
	const int Type = m_aTaskType[m_ActiveTask];
	m_aTaskProgress[m_ActiveTask] = 0;
	m_TaskTimerTick = 0;
	const int DifficultyTier = max(0, (g_Config.m_SvMapGenLevel - 1) / 10);
	// Guide players to the zone anchor with a radar marker.
	const int TaskZone = m_aTaskZones[m_ActiveTask];
	if(TaskZone >= 0)
	{
		if(!m_pTaskRadar)
			m_pTaskRadar = new CServerRadar(&GameServer()->m_World, RADAR_REACTOR);
		m_pTaskRadar->Activate(m_aTaskZone[TaskZone]);
	}
	else if(m_pTaskRadar)
	{
		GameServer()->m_World.DestroyEntity(m_pTaskRadar);
		m_pTaskRadar = 0;
	}
	switch(Type)
	{
		case EXTRACT_TASK_SWITCHES:
			// Count only switches not yet activated (earlier tasks may have
			// triggered some already).
			m_aTaskTarget[m_ActiveTask] = max(0, m_SwitchesRequired - m_SwitchesActivated);
			GameServer()->SendBroadcastFormat(
				-1, false, "Task %d/%d: activate %d switches", m_ActiveTask + 1, m_TaskCount, m_aTaskTarget[m_ActiveTask]);
			break;
		case EXTRACT_TASK_ELIMINATE:
			m_aTaskTarget[m_ActiveTask] = min(2 + CountHumans(), 4);
			GameServer()->SendBroadcastFormat(-1,
											  false,
											  "Task %d/%d: eliminate %d enemies",
											  m_ActiveTask + 1,
											  m_TaskCount,
											  m_aTaskTarget[m_ActiveTask]);
			break;
		case EXTRACT_TASK_DEFEND:
			m_aTaskTarget[m_ActiveTask] = 6 + min(4, DifficultyTier);
			GameServer()->SendBroadcastFormat(-1,
											  false,
											  "Task %d/%d: defend the marked zone for %d seconds",
											  m_ActiveTask + 1,
											  m_TaskCount,
											  m_aTaskTarget[m_ActiveTask]);
			break;
		case EXTRACT_TASK_COLLECT:
			m_aTaskTarget[m_ActiveTask] = 1;
			GameServer()->SendBroadcastFormat(
				-1, false, "Task %d/%d: reach the marked supply point", m_ActiveTask + 1, m_TaskCount);
			break;
		case EXTRACT_TASK_TIMED_CLEAR:
			m_aTaskTarget[m_ActiveTask] = min(8 + g_Config.m_SvMapGenLevel * 2, 40);
			m_TaskTimerTick = Server()->Tick() + Server()->TickSpeed() * max(60, 90 - min(30, DifficultyTier * 5));
			GameServer()->SendBroadcastFormat(-1,
											  false,
											  "Task %d/%d: clear %d enemies in time",
											  m_ActiveTask + 1,
											  m_TaskCount,
											  m_aTaskTarget[m_ActiveTask]);
			break;
		default:
			break;
	}
	// A zero-target task is already satisfied (e.g. all switches activated in
	// an earlier task); complete it immediately.
	if(m_aTaskTarget[m_ActiveTask] <= 0)
	{
		CompleteTask(m_ActiveTask);
		return;
	}
	PickEvent();
}

void CGameControllerExtract::CompleteTask(int Index)
{
	if(m_Phase != 0 || Index != m_ActiveTask)
		return;
	m_aTaskProgress[Index] = m_aTaskTarget[Index];
	if(m_pTaskRadar)
	{
		GameServer()->m_World.DestroyEntity(m_pTaskRadar);
		m_pTaskRadar = 0;
	}
	if(GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnObjectiveComplete();
	m_TasksCompleted = Index + 1;
	GameServer()->SendBroadcastFormat(-1, false, "Task %d/%d complete", Index + 1, m_TaskCount);
	if(Index + 1 >= m_TaskCount)
	{
		// All tasks done: spawn the mid boss; the door opens once it falls
		// (existing m_DoorChoicePending + m_MidBossPerkOffered chain).
		if(!m_MidBossSpawned)
			SpawnMidBoss();
		m_DoorChoicePending = true;
	}
	else
	{
		m_ActiveTask = Index + 1;
		StartTask();
	}
}

void CGameControllerExtract::UpdateTask()
{
	if(m_Phase != 0 || m_ActiveTask < 0 || m_ActiveTask >= m_TaskCount)
		return;
	const int Type = m_aTaskType[m_ActiveTask];
	const int Zone = m_aTaskZones[m_ActiveTask];
	switch(Type)
	{
		case EXTRACT_TASK_DEFEND:
			if(Zone >= 0 && PlayerInRadius(m_aTaskZone[Zone], 300.0f))
			{
				m_aTaskProgress[m_ActiveTask]++;
				if(m_aTaskProgress[m_ActiveTask] >= m_aTaskTarget[m_ActiveTask])
					CompleteTask(m_ActiveTask);
			}
			break;
		case EXTRACT_TASK_COLLECT:
			if(Zone >= 0 && PlayerInRadius(m_aTaskZone[Zone], 150.0f))
			{
				m_aTaskProgress[m_ActiveTask] = 1;
				CompleteTask(m_ActiveTask);
			}
			break;
		case EXTRACT_TASK_TIMED_CLEAR:
			if(m_TaskTimerTick && Server()->Tick() > m_TaskTimerTick)
			{
				// Out of time: reset progress and retry with a fresh timer.
				m_aTaskProgress[m_ActiveTask] = 0;
				const int DifficultyTier = max(0, (g_Config.m_SvMapGenLevel - 1) / 10);
				m_TaskTimerTick =
					Server()->Tick() + Server()->TickSpeed() * max(60, 90 - min(30, DifficultyTier * 5));
				GameServer()->SendBroadcastFormat(-1,
												  false,
												  "Task %d/%d: out of time, clear %d enemies",
												  m_ActiveTask + 1,
												  m_TaskCount,
												  m_aTaskTarget[m_ActiveTask]);
			}
			break;
		default:
			break;
	}
}

void CGameControllerExtract::PickEvent()
{
	m_ActiveEvent = EXTRACT_EVT_NONE;
	if(m_ActiveTask <= 0 || m_ActiveTask - m_LastEventTask < 1)
		return;
	CDeterministicRandom EventRng(DeterministicSeed((unsigned long long)g_Config.m_SvMapGenSeed, "extract_events"));
	if(EventRng.NextInt(100) >= 35)
		return;
	static const struct
	{
		int m_Event;
		int m_Weight;
	} s_aEvents[] = {
		{EXTRACT_EVT_REINFORCEMENTS, 12},
		{EXTRACT_EVT_ELITE_AMBUSH, 10},
		{EXTRACT_EVT_SUPPLY_DROP, 12},
		{EXTRACT_EVT_TRAP_ZONE, 10},
		{EXTRACT_EVT_BOSS_RUSH, 8},
	};
	const bool LastWave = m_ActiveTask + 1 >= m_TaskCount;
	auto Excluded = [&](int Event)
	{
		return LastWave && (Event == EXTRACT_EVT_TRAP_ZONE || Event == EXTRACT_EVT_BOSS_RUSH);
	};
	int Total = 0;
	for(int i = 0; i < 5; i++)
		if(!Excluded(s_aEvents[i].m_Event))
			Total += s_aEvents[i].m_Weight;
	if(Total <= 0)
		return;
	int Pick = EventRng.NextInt(Total);
	for(int i = 0; i < 5; i++)
	{
		if(Excluded(s_aEvents[i].m_Event))
			continue;
		if(Pick < s_aEvents[i].m_Weight)
		{
			m_ActiveEvent = s_aEvents[i].m_Event;
			break;
		}
		Pick -= s_aEvents[i].m_Weight;
	}
	if(m_ActiveEvent != EXTRACT_EVT_NONE)
	{
		m_LastEventTask = m_ActiveTask;
		m_EventActionTick = Server()->Tick() + Server()->TickSpeed() * 2;
	}
}

void CGameControllerExtract::RunEvent()
{
	// Events belong to the task phase; drop any scheduled for evacuation.
	if(m_Phase != 0)
	{
		m_ActiveEvent = EXTRACT_EVT_NONE;
		return;
	}
	switch(m_ActiveEvent)
	{
		case EXTRACT_EVT_REINFORCEMENTS:
		{
			const int Count = 3 + irandom(4);
			for(int i = 0; i < Count; i++)
				GameServer()->AddBot();
			GameServer()->SendBroadcast("Event: Reinforcements inbound!", -1);
			break;
		}
		case EXTRACT_EVT_ELITE_AMBUSH:
		{
			vec2 p;
			if(GetBossSpawnPos(&p))
				SpawnBoss(&GameServer()->m_World, p, EnemyLevel());
			GameServer()->SendBroadcast("Event: Elite ambush!", -1);
			break;
		}
		case EXTRACT_EVT_SUPPLY_DROP:
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if(!pPlayer || pPlayer->m_IsBot || pPlayer->GetTeam() == TEAM_SPECTATORS)
					continue;
				CCharacter *pChar = pPlayer->GetCharacter();
				if(pChar)
				{
					pChar->IncreaseHealth(15);
					pChar->IncreaseArmor(10);
				}
			}
			GameServer()->SendBroadcast("Event: Supply drop!", -1);
			break;
		}
		case EXTRACT_EVT_TRAP_ZONE:
		{
			// Traps are baked into the generated maze; pressure rises instead.
			const int Count = 4 + irandom(3);
			for(int i = 0; i < Count; i++)
				GameServer()->AddBot();
			GameServer()->SendBroadcast("Event: Trap zone ahead!", -1);
			break;
		}
		case EXTRACT_EVT_BOSS_RUSH:
		{
			vec2 p;
			if(GetBossSpawnPos(&p))
				SpawnBoss(&GameServer()->m_World, p, max(3, EnemyLevel() - 1));
			GameServer()->SendBroadcast("Event: Boss rush!", -1);
			break;
		}
		default:
			break;
	}
	m_ActiveEvent = EXTRACT_EVT_NONE;
}

void CGameControllerExtract::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);
	if(!RequestAI && GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnPlayerSpawn(pChr->GetPlayer()->GetCID());

	if(RequestAI)
	{
		if(m_EnemiesLeft <= 0)
			return;
		m_EnemiesLeft--;
		const int Level = EnemyLevel();
		GameServer()->GetAISkin(&pChr->m_AISkin, false, Level);
		pChr->SetAISkin();
		pChr->m_pAI = CreatePveBotAI(GameServer(), pChr, Level);
		pChr->m_IsBot = true;
		pChr->m_IsBot = true;
		pChr->m_SkipPickups = 999;
	}
}

int CGameControllerExtract::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, const CAttackSource &Source)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Source);
	if(!pVictim->m_IsBot && GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnPlayerDeath(pVictim->GetPlayer()->GetCID());

	if(pVictim->m_IsBot)
	{
		if(pKiller && !pKiller->m_IsBot && GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnEnemyKilled(Source, pVictim->m_Pos, pVictim);
		// Kill-type tasks (eliminate elites / timed clear) advance on any
		// AI enemy death during phase 0.
		if(m_Phase == 0 && m_ActiveTask >= 0 && m_ActiveTask < m_TaskCount &&
		   (m_aTaskType[m_ActiveTask] == EXTRACT_TASK_ELIMINATE ||
			m_aTaskType[m_ActiveTask] == EXTRACT_TASK_TIMED_CLEAR))
		{
			m_aTaskProgress[m_ActiveTask]++;
			if(m_aTaskProgress[m_ActiveTask] >= m_aTaskTarget[m_ActiveTask])
				CompleteTask(m_ActiveTask);
		}
		pVictim->MarkToBeKicked();
	}
	else if(!pVictim->m_IsBot)
	{
		m_HumanDeaths++;
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvRespawnDelay;
	}

	return 0;
}

void CGameControllerExtract::OnSwitchTriggered()
{
	if(m_Phase != 0 || m_DoorOpen)
		return;

	m_SwitchesActivated++;
	if(GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnSwitchTriggered();
	GameServer()->SendBroadcastFormat(-1, false, "Switch %d/%d activated", m_SwitchesActivated, m_SwitchesRequired);
	m_TriggerLevel = max(m_TriggerLevel, 6 + m_SwitchesActivated * 2);
	TriggerAllBotAI(GameServer(), m_TriggerLevel);

	// Mid boss and door opening are driven by the task pool (CompleteTask).
	if(m_ActiveTask >= 0 && m_ActiveTask < m_TaskCount && m_aTaskType[m_ActiveTask] == EXTRACT_TASK_SWITCHES)
	{
		m_aTaskProgress[m_ActiveTask]++;
		if(m_aTaskProgress[m_ActiveTask] >= m_aTaskTarget[m_ActiveTask])
			CompleteTask(m_ActiveTask);
	}
}

void CGameControllerExtract::OnDroidKilled(CDroid *pDroid)
{
	if(pDroid == m_pMidBoss)
		m_pMidBoss = 0;
}

void CGameControllerExtract::DisplayExit(vec2 Pos)
{
	if(m_pDoor)
		m_pDoor->Activate(Pos);
}

void CGameControllerExtract::BeginEvacuation()
{
	if(m_DoorOpen)
		return;
	m_DoorChoicePending = false;
	m_DoorChoiceStarted = false;
	m_DoorOpen = true;
	m_Phase = 1;
	TriggerEscape();
	m_EvacNeeded = max(1, CountHumans());
	m_Evacuated = 0;
	if(GameServer()->m_pPveDirector)
	{
		GameServer()->m_pPveDirector->OnStageStart();
		GameServer()->m_pPveDirector->OnEvacuationStarted();
	}
	SpawnEscapePressure();
	// Evacuation pressure escalates: reinforcement waves every 20 s and
	// route blockades at 30-45 s intervals (docs §6.1/§6.2).
	m_EscapeWave = 0;
	m_EscapeWaveTick = Server()->Tick() + Server()->TickSpeed() * 30;
	m_ReinforceTick = Server()->Tick() + Server()->TickSpeed() * 20;
	GameServer()->SendBroadcast("Door open — evacuate!", -1);
}

void CGameControllerExtract::NextLevel(int CID)
{
	if(!m_DoorOpen || m_RoundOverTick)
		return;

	CPlayer *pPlayer = GameServer()->m_apPlayers[CID];
	if(!pPlayer || pPlayer->m_IsBot || !pPlayer->GetCharacter())
		return;
	if(pPlayer->GetCharacter()->IgnoreCollision())
		return;
	if(GameServer()->m_pPveDirector)
	{
		GameServer()->m_pPveDirector->OnEvacuationZoneEntered(CID);
		if(GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_HEAVY_CARGO &&
		   pPlayer->GetCharacter()->IsBombCarrier())
			GameServer()->m_pPveDirector->OnCargoDelivered();
	}

	pPlayer->GetCharacter()->Warp();
	m_Evacuated++;
	// Each evacuee tightens the pressure on those still running.
	m_ReinforceTick = min(m_ReinforceTick, Server()->Tick() + Server()->TickSpeed() * 8);
	GameServer()->SendBroadcastFormat(-1, false, "Evacuated %d/%d", m_Evacuated, m_EvacNeeded);

	if(m_Evacuated >= m_EvacNeeded)
	{
		m_Win = true;
		m_RoundOverTick = Server()->Tick();
		const int Rating = ComputeRating();
		if(GameServer()->m_pPveDirector)
		{
			GameServer()->m_pPveDirector->OnStageComplete(true);
			// Research points scale with the exit rating (S=3, A=2, B/C=1).
			GameServer()->m_pPveDirector->RewardResearch(Rating >= 2 ? Rating : 1, PVE_REWARD_EXTRACTION);
		}
		static const char *s_apRatingNames[4] = {"C", "B", "A", "S"};
		GameServer()->SendBroadcastFormat(
			-1, false, "Extraction complete — rating %s", s_apRatingNames[clamp(Rating, 0, 3)]);
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->m_IsBot)
			{
				Server()->SendPlatformEvent(i, PLATFORM_EVENT_FIRST_EXTRACTION);
				Server()->SendPlatformEvent(i, PLATFORM_EVENT_FIRST_COOP_COMPLETE);
				Server()->SendPlatformEvent(i, PLATFORM_EVENT_STAT_COOP_COMPLETIONS, 1);
			}
		// no sv_mapgen_level++
	}
}

void CGameControllerExtract::Tick()
{
	IGameController::Tick();
	if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->InIntermission())
	{
		if(m_DeadlineTick > 0)
			m_DeadlineTick++;
		if(m_StartTick > 0)
			m_StartTick++;
		if(m_BotSpawnTick > 0)
			m_BotSpawnTick++;
		if(m_TriggerTick > 0)
			m_TriggerTick++;
		return;
	}
	if(m_DoorChoiceStarted)
		BeginEvacuation();
	UpdateTask();
	if(m_ActiveEvent != EXTRACT_EVT_NONE && m_EventActionTick && Server()->Tick() >= m_EventActionTick)
		RunEvent();
	if(m_Phase == 1 && !m_RoundOverTick && m_DoorOpen)
	{
		// Regular reinforcement pressure while evacuating.
		if(m_ReinforceTick && Server()->Tick() >= m_ReinforceTick)
		{
			const int Count = 2 + irandom(3);
			for(int i = 0; i < Count; i++)
				GameServer()->AddBot();
			m_ReinforceTick = Server()->Tick() + Server()->TickSpeed() * 20;
		}
		// Route blockade waves (up to three) at tightening intervals.
		if(m_EscapeWaveTick && m_EscapeWave < 3 && Server()->Tick() >= m_EscapeWaveTick)
		{
			m_EscapeWave++;
			GameServer()->SendBroadcastFormat(-1, false, "Evacuation route blocked — sector %d", m_EscapeWave);
			const int Count = 4 + m_EscapeWave * 2;
			for(int i = 0; i < Count; i++)
				GameServer()->AddBot();
			m_EscapeWaveTick = Server()->Tick() + Server()->TickSpeed() * (45 - m_EscapeWave * 5);
		}
	}
	if(m_EliteContractSpawned && GameServer()->m_pPveDirector && CountAliveBosses(&GameServer()->m_World) <= 0)
	{
		m_EliteContractSpawned = false;
		GameServer()->m_pPveDirector->OnBossKilled();
	}

	if(m_GameState == STATE_STARTING)
	{
		if(CountPlayers(0) > 0)
		{
			if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->Enabled() &&
			   !GameServer()->m_pPveDirector->ProgressReady() && Server()->Tick() < m_RogueliteWaitTick)
				return;
			if(!m_RogueliteStarted)
			{
				m_RogueliteStarted = true;
				if(GameServer()->m_pPveDirector)
				{
					GameServer()->m_pPveDirector->StartIntermission(true, true);
					if(GameServer()->m_pPveDirector->InIntermission())
						return;
				}
			}
			if(!m_RogueliteStageStarted)
			{
				m_RogueliteStageStarted = true;
				if(GameServer()->m_pPveDirector)
					GameServer()->m_pPveDirector->OnStageStart();
			}
			m_GameState = STATE_GAME;
			m_StartTick = Server()->Tick();
			const float DeadlineScale =
				GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->DeadlineMultiplier() : 1.0f;
			m_DeadlineTick =
				Server()->Tick() + (int)(Server()->TickSpeed() * 60 * max(1, g_Config.m_SvTimelimit) * DeadlineScale);
			const int ExtraSwitches = GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() ==
																		  PVE_CONTRACT_LOCKED_ROUTE
										  ? 2
										  : 0;
			if(ExtraSwitches > 0)
				for(int Extra = 0; Extra < ExtraSwitches; Extra++)
				{
					vec2 Pos;
					bool Placed = false;
					for(int Tries = 0; Tries < m_NumEnemySpawnPos + 4 && !Placed; Tries++)
					{
						if(!GetSpawnPos(0, &Pos))
							break;
						Pos = GameServer()->Collision()->SnapToStandPos(Pos);
						if(!GameServer()->Collision()->IsSafeStandPos(Pos))
							continue;
						new CBuilding(&GameServer()->m_World, Pos, BUILDING_SWITCH, TEAM_NEUTRAL);
						m_AvailableSwitches++;
						CServerRadar *pRadar = new CServerRadar(&GameServer()->m_World, RADAR_REACTOR);
						pRadar->Activate(Pos);
						Placed = true;
					}
				}
			// Use the authoritative number actually placed on this generated map.
			// Requiring an artificial minimum of two softlocked rare layouts where
			// map generation could only place one; zero keeps the timed boss fallback.
			m_SwitchesRequired = m_AvailableSwitches;
			// Tasks need the authoritative switch count (incl. LOCKED_ROUTE
			// extras) so a SWITCHES target matches the placed map.
			PickTasks();
			SpawnInitialEnemies();
			if(GameServer()->m_pPveDirector &&
			   GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_HEAVY_CARGO)
				for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
				{
					CCharacter *pCharacter = GameServer()->GetPlayerChar(ClientID);
					if(pCharacter && !pCharacter->m_IsBot && pCharacter->IsAlive())
					{
						pCharacter->GiveBomb();
						break;
					}
				}
			m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * 5;
			m_TriggerTick = Server()->Tick() + Server()->TickSpeed() * 2;
			GameServer()->SendBroadcast("Extraction — activate switches, then escape", -1);
		}
		return;
	}

	const int HumansAlive = CountHumansAliveLocal();
	if(HumansAlive > 0)
		m_HadHumanAlive = true;

	if(g_Config.m_SvSurvivalMode && !m_RoundOverTick && m_HadHumanAlive && HumansAlive <= 0 &&
	   CountHumanPlayersLocal() > 0)
	{
		GameServer()->SendBroadcast("Extraction failed — team wiped", -1);
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->CompleteContract(false);
		m_RoundOverTick = Server()->Tick();
		m_Win = false;
	}

	// reinforce while fighting / evacuating
	if(m_BotSpawnTick && m_BotSpawnTick <= Server()->Tick() && !m_RoundOverTick)
	{
		m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * (m_Phase == 0 ? 5 : 4);
		const float Pressure = m_Phase == 1 && GameServer()->m_pPveDirector
								   ? GameServer()->m_pPveDirector->ReinforcementMultiplier()
								   : 1.0f;
		const int Cap = (int)((m_Phase == 0 ? 16 : 18) * Pressure);
		if(CountBots() < Cap)
		{
			const int Count = max(1, (int)(3 * Pressure));
			m_EnemiesLeft += Count;
			for(int i = 0; i < Count; i++)
				GameServer()->AddBot();
		}
	}

	if(m_Phase == 0 && m_MidBossSpawned && !m_MidBossPerkOffered && !m_pMidBoss)
	{
		m_MidBossPerkOffered = true;
		if(GameServer()->m_pPveDirector)
		{
			GameServer()->m_pPveDirector->StartIntermission(false, true);
			if(GameServer()->m_pPveDirector->InIntermission())
				return;
		}
	}
	if(m_Phase == 0 && m_DoorChoicePending && m_MidBossPerkOffered && !m_DoorChoiceStarted)
	{
		m_DoorChoiceStarted = true;
		if(GameServer()->m_pPveDirector)
		{
			GameServer()->m_pPveDirector->StartIntermission(false, true);
			if(GameServer()->m_pPveDirector->InIntermission())
				return;
		}
		BeginEvacuation();
	}

	// no switches on map: after 25s mid boss then door
	if(m_Phase == 0 && !m_DoorOpen && m_AvailableSwitches <= 0 && m_StartTick &&
	   Server()->Tick() > m_StartTick + Server()->TickSpeed() * 25)
	{
		if(!m_MidBossSpawned)
			SpawnMidBoss();
		if(m_MidBossSpawned)
			m_DoorChoicePending = true;
	}

	if(m_TriggerTick < Server()->Tick())
	{
		TriggerAllBotAI(GameServer(), m_TriggerLevel);
		m_TriggerTick = Server()->Tick() + Server()->TickSpeed() * 2;
	}

	if(!m_RoundOverTick && m_DeadlineTick && Server()->Tick() > m_DeadlineTick)
	{
		GameServer()->SendBroadcast("Extraction failed — time's up", -1);
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->CompleteContract(false);
		m_RoundOverTick = Server()->Tick();
		m_Win = false;
	}

	if(m_RoundOverTick && m_RoundOverTick < Server()->Tick() - Server()->TickSpeed() * 5.0f)
	{
		m_RoundOverTick = 0;
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->ClearRun();
		EndRound(); // does not bump mapgen level
	}

	GameServer()->UpdateAI();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(pPlayer && pPlayer->m_IsBot && pPlayer->m_ToBeKicked)
			GameServer()->KickBot(pPlayer->GetCID());
	}
}

void CGameControllerExtract::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameData *pGameDataObj =
		(CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	if(m_Phase == 0)
	{
		pGameDataObj->m_TeamscoreRed = QUEST_EXTRACT;
		if(m_ActiveTask >= 0 && m_ActiveTask < m_TaskCount)
			pGameDataObj->m_TeamscoreBlue =
				max(0, m_aTaskTarget[m_ActiveTask] - m_aTaskProgress[m_ActiveTask]);
		else
			pGameDataObj->m_TeamscoreBlue = max(0, m_SwitchesRequired - m_SwitchesActivated);
	}
	else
	{
		pGameDataObj->m_TeamscoreRed = QUEST_EXTRACT;
		pGameDataObj->m_TeamscoreBlue = max(0, m_EvacNeeded - m_Evacuated);
	}

	int SecLeft = 0;
	if(m_DeadlineTick > Server()->Tick())
		SecLeft = (m_DeadlineTick - Server()->Tick()) / Server()->TickSpeed();
	pGameDataObj->m_FlagCarrierRed = SecLeft;
	// WaveType bits carry the active task type during phase 0 so the client HUD
	// can label the remaining counter (switches / enemies / seconds / supplies).
	pGameDataObj->m_FlagCarrierBlue =
		(m_Phase << 8) | (m_Win ? 1 : 0) |
		((m_Phase == 0 && m_ActiveTask >= 0 && m_ActiveTask < m_TaskCount)
				? (m_aTaskType[m_ActiveTask] << 4)
				: 0);
}
