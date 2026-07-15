#include <engine/shared/config.h>

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
#include <game/server/pve_operation_director.h>

#include "extract.h"

CGameControllerExtract::CGameControllerExtract(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pOperationDirector = new CPveOperationDirector(pGameServer);
	m_pGameType = "EXTRACT";
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
	m_TriggerLevel = 10;
	m_Win = false;
	m_EscapePressure = false;
	m_HadHumanAlive = false;
	m_RogueliteStarted = false;
	m_RogueliteWaitTick = Server()->Tick() + Server()->TickSpeed() * 2;
	m_RogueliteStageStarted = false;
	m_MidBossPerkOffered = false;
	m_DoorChoicePending = false;
	m_DoorChoiceStarted = false;
	m_EliteContractSpawned = false;
	m_pMidBoss = 0;

	g_Config.m_SvOneHitKill = 0;
	g_Config.m_SvWarmup = 0;
	g_Config.m_SvScorelimit = 0;
	g_Config.m_SvEnableBuilding = 1;
	g_Config.m_SvDisablePVP = 1;
	g_Config.m_SvSurvivalTime = 0;
	g_Config.m_SvSurvivalAcid = 0;
	dbg_msg("extract", "rules: time_limit=%d roguelite=%d contracts=%d seed=%d random_seed=%d",
		g_Config.m_SvTimelimit, g_Config.m_SvPveRoguelite, g_Config.m_SvPveContracts,
		g_Config.m_SvMapGenSeed, g_Config.m_SvMapGenRandSeed);

	if(g_Config.m_SvEnableBuilding)
		m_GameFlags |= GAMEFLAG_BUILD;
	if(g_Config.m_SvSurvivalMode)
		m_GameFlags |= GAMEFLAG_SURVIVAL;
}

CGameControllerExtract::~CGameControllerExtract()
{
	delete m_pOperationDirector;
}

bool CGameControllerExtract::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_ENEMYSPAWN)
	{
		if(m_NumEnemySpawnPos < MAX_ENEMIES)
		{
			m_aEnemySpawnPos[m_NumEnemySpawnPos++] = Pos;
			m_pOperationDirector->AddCandidate(Pos);
		}
		return true;
	}
	if(Index == ENTITY_SWITCH)
		m_AvailableSwitches++;
	return IGameController::OnEntity(Index, Pos);
}

bool CGameControllerExtract::GetSpawnPos(int Team, vec2 *pOutPos)
{
	if(m_NumEnemySpawnPos <= 0)
		return false;
	m_SpawnPosRotation = (m_SpawnPosRotation + 1) % m_NumEnemySpawnPos;
	*pOutPos = m_aEnemySpawnPos[m_SpawnPosRotation];
	return true;
}

bool CGameControllerExtract::GetBossSpawnPos(vec2 *pOutPos)
{
	if(FindBossSpawnPosition(&GameServer()->m_World, m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, pOutPos))
		return true;
	if(!GetSpawnPos(0, pOutPos))
		return false;
	*pOutPos += vec2(0.0f, -100.0f);
	return true;
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
		m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, EnemyLevel(), m_EnemiesLeft, 16);
	m_EnemiesLeft -= ThreatReplacement.m_ThreatSpent;
	const int BotCap = max(0, 16 - ThreatReplacement.m_EntitiesSpawned);
	for(int i = 0; i < m_EnemiesLeft && CountBots() < BotCap; i++)
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
	for(int i = 0; i < 6 && CountBots() < 18; i++)
	{
		m_EnemiesLeft++;
		GameServer()->AddBot();
	}
	if(GetSpawnPos(0, &p))
		new CCrawler(&GameServer()->m_World, p + vec2(0, -80));
	m_TriggerLevel = max(m_TriggerLevel, 14);
	TriggerAllBotAI(GameServer(), m_TriggerLevel);
	GameServer()->SendBroadcast("Mid-run boss! Hold the line", -1);
}

void CGameControllerExtract::SpawnEscapePressure()
{
	if(m_EscapePressure)
		return;
	m_EscapePressure = true;
	const float Pressure = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ReinforcementMultiplier() : 1.0f;
	const int AddedThreat = (int)((8 + CountPlayers(0) * 2) * Pressure);
	const int ConcurrentCap = (int)(18 * Pressure);
	const SThreatBudgetResult ThreatReplacement = SpawnThreatBudgetSpecialists(&GameServer()->m_World,
		m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, EnemyLevel(), AddedThreat,
		max(0, ConcurrentCap - CountBots() - CountAliveSpecialists(&GameServer()->m_World)));
	m_EnemiesLeft += AddedThreat - ThreatReplacement.m_ThreatSpent;
	for(int i = 0; i < (int)(8 * Pressure) - ThreatReplacement.m_ThreatSpent &&
		CountBots() + CountAliveSpecialists(&GameServer()->m_World) < ConcurrentCap; i++)
		GameServer()->AddBot();
	vec2 p;
	if(GetSpawnPos(0, &p))
		new CCrawler(&GameServer()->m_World, p + vec2(0, -80));
	m_TriggerLevel = max(m_TriggerLevel, 16);
	TriggerAllBotAI(GameServer(), m_TriggerLevel);
	GameServer()->SendBroadcast("Enemies flooding the exit!", -1);
}

int CGameControllerExtract::EnemyLevel() const
{
	int Mins = 0;
	if(m_StartTick)
		Mins = (Server()->Tick() - m_StartTick) / (Server()->TickSpeed() * 45);
	const int DifficultyTier = max(0, (g_Config.m_SvMapGenLevel - 1) / 10);
	return min(14, max(4, 4 + m_SwitchesActivated * 2 + Mins + m_Phase * 3 + DifficultyTier));
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
		GameServer()->GetAISkin(&pChr->GetPlayer()->m_AISkin, false, Level);
		pChr->GetPlayer()->SetAISkin();
		pChr->GetPlayer()->m_pAI = CreatePveBotAI(GameServer(), pChr->GetPlayer(), Level);
		pChr->GetPlayer()->m_IsBot = true;
		pChr->m_IsBot = true;
		pChr->m_SkipPickups = 999;
	}
}

int CGameControllerExtract::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
	if(!pVictim->m_IsBot && GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnPlayerDeath(pVictim->GetPlayer()->GetCID());

	if(pVictim->m_IsBot)
	{
		if(pKiller && !pKiller->m_IsBot && GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnEnemyKilled(pKiller->GetCID(), Weapon, pVictim->m_Pos, pVictim);
		pVictim->GetPlayer()->m_ToBeKicked = true;
	}
	else if(!pVictim->m_IsBot)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvRespawnDelay;

	return 0;
}

void CGameControllerExtract::OnSwitchTriggered()
{
	if(m_Phase != 0 || m_DoorOpen)
		return;

	m_SwitchesActivated++;
	m_pOperationDirector->OnEvent(CPveOperationDirector::EVENT_SWITCH);
	if(GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnSwitchTriggered();
	GameServer()->SendBroadcastFormat(-1, false, "Switch %d/%d activated", m_SwitchesActivated, m_SwitchesRequired);
	m_TriggerLevel = max(m_TriggerLevel, 10 + m_SwitchesActivated * 3);
	TriggerAllBotAI(GameServer(), m_TriggerLevel);

	// First switch already brings mid-boss pressure
	if(!m_MidBossSpawned && m_SwitchesActivated >= 1)
		SpawnMidBoss();

	if(m_SwitchesActivated >= m_SwitchesRequired)
	{
		if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->Enabled())
			m_DoorChoicePending = true;
		else
			BeginEvacuation();
	}
}

void CGameControllerExtract::OnDroidKilled(CDroid *pDroid)
{
	if(pDroid && IsBossDroidType(pDroid->m_Type))
		m_pOperationDirector->OnEvent(CPveOperationDirector::EVENT_BOSS);
	if(pDroid == m_pMidBoss)
		m_pMidBoss = 0;
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
	if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_HEAVY_CARGO && pPlayer->GetCharacter()->IsBombCarrier())
		GameServer()->m_pPveDirector->OnCargoDelivered();

	pPlayer->GetCharacter()->Warp();
	m_Evacuated++;
	m_pOperationDirector->OnEvent(CPveOperationDirector::EVENT_EVACUATE);
	GameServer()->SendBroadcastFormat(-1, false, "Evacuated %d/%d", m_Evacuated, m_EvacNeeded);

	if(m_Evacuated >= m_EvacNeeded)
	{
		m_Win = true;
		m_RoundOverTick = Server()->Tick();
		if(GameServer()->m_pPveDirector)
		{
			GameServer()->m_pPveDirector->OnStageComplete(true);
			GameServer()->m_pPveDirector->RewardResearch(2, PVE_REWARD_EXTRACTION);
		}
		GameServer()->SendBroadcast("Extraction complete!", -1);
		// no sv_mapgen_level++
	}
}

void CGameControllerExtract::Tick()
{
	IGameController::Tick();
	const int ActiveOperation = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ActiveOperation() : -1;
	if(ActiveOperation >= 0 && m_pOperationDirector->Operation() != ActiveOperation)
		m_pOperationDirector->Start(ActiveOperation);
	else if(ActiveOperation < 0 && m_pOperationDirector->Operation() >= 0)
		m_pOperationDirector->Clear();
	m_pOperationDirector->Tick();
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
			const float DeadlineScale = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->DeadlineMultiplier() : 1.0f;
			m_DeadlineTick = Server()->Tick() + (int)(Server()->TickSpeed() * 60 * max(1, g_Config.m_SvTimelimit) * DeadlineScale);
			const int Operation = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ActiveOperation() : -1;
			const int ExtraSwitches = GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_LOCKED_ROUTE ? 2 :
				(Operation == PVE_OPERATION_SIEGE_ROUTE ? 1 : 0);
			if(ExtraSwitches > 0)
				for(int Extra = 0; Extra < ExtraSwitches; Extra++)
				{
					vec2 Pos;
					if(!GetSpawnPos(0, &Pos))
						Pos = vec2(4000.0f + Extra * 96.0f, 4000.0f);
					new CBuilding(&GameServer()->m_World, Pos, BUILDING_SWITCH, TEAM_NEUTRAL);
					m_AvailableSwitches++;
					CRadar *pRadar = new CRadar(&GameServer()->m_World, RADAR_REACTOR);
					pRadar->Activate(Pos);
				}
			// Use the authoritative number actually placed on this generated map.
			// Requiring an artificial minimum of two softlocked rare layouts where
			// map generation could only place one; zero keeps the timed boss fallback.
			m_SwitchesRequired = m_AvailableSwitches;
			if((Operation == PVE_OPERATION_CORE_RECOVERY || Operation == PVE_OPERATION_LOCKDOWN_BREAK) && m_SwitchesRequired > 1)
				m_SwitchesRequired--;
			SpawnInitialEnemies();
			if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_HEAVY_CARGO)
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

	if(g_Config.m_SvSurvivalMode && !m_RoundOverTick && m_HadHumanAlive
		&& HumansAlive <= 0 && CountHumanPlayersLocal() > 0)
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
		const float Pressure = m_Phase == 1 && GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ReinforcementMultiplier() : 1.0f;
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
			GameServer()->m_pPveDirector->StartIntermission(false, true, false);
			if(GameServer()->m_pPveDirector->InIntermission())
				return;
		}
	}
	if(m_Phase == 0 && m_DoorChoicePending && m_MidBossPerkOffered && !m_DoorChoiceStarted)
	{
		m_DoorChoiceStarted = true;
		if(GameServer()->m_pPveDirector)
		{
			GameServer()->m_pPveDirector->StartIntermission(false, true, false);
			if(GameServer()->m_pPveDirector->InIntermission())
				return;
		}
		BeginEvacuation();
	}

	// no switches on map: after 25s mid boss then door
	if(m_Phase == 0 && !m_DoorOpen && m_AvailableSwitches <= 0 && m_StartTick
		&& Server()->Tick() > m_StartTick + Server()->TickSpeed() * 25)
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

	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	if(m_Phase == 0)
	{
		pGameDataObj->m_TeamscoreRed = QUEST_EXTRACT;
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
	pGameDataObj->m_FlagCarrierBlue = (m_Phase << 8) | (m_Win ? 1 : 0);
}
