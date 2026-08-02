#include <engine/shared/config.h>
#include <engine/platform_events.h>

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
#include <game/server/entities/extraction_object.h>
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
	m_RogueliteStarted = false;
	m_RogueliteWaitTick = Server()->Tick() + Server()->TickSpeed() * 2;
	m_RogueliteStageStarted = false;
	m_MidBossPerkOffered = false;
	m_DoorChoicePending = false;
	m_DoorChoiceStarted = false;
	m_EliteContractSpawned = false;
	m_pMidBoss = 0;
	m_pDoor = new CServerRadar(&GameServer()->m_World, RADAR_DOOR);
	m_pEvacObject = 0;
	m_EvacPos = vec2(0, 0);
	m_NumLootCandidates = 0;
	m_NumOutposts = 0;
	m_NumGuardSpawnPos = 0;
	m_Quota = 75;
	m_DepositedValue = 0;
	m_AlertLevel = 0;
	m_PhaseEndTick = 0;
	for(int i = 0; i < MAX_EXTRACTION_LOOT; i++)
	{
		m_apLoot[i] = 0;
		m_aLootCandidate[i] = vec2(0, 0);
	}
	for(int i = 0; i < MAX_EXTRACTION_OUTPOSTS; i++)
	{
		m_apOutposts[i] = 0;
		m_aOutpostPos[i] = vec2(0, 0);
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_apRevive[i] = 0;
		m_aCarriedValue[i] = 0;
		m_aInteractionTarget[i] = -1;
		m_aInteractionTicks[i] = 0;
		m_aInteractionHeld[i] = false;
		m_aBoarded[i] = false;
		m_aEliminated[i] = false;
		m_aDownCount[i] = 0;
		m_aBleedoutTick[i] = 0;
	}

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
	if(Index == ENTITY_EXTRACTION_LOOT_CANDIDATE && m_NumLootCandidates < MAX_EXTRACTION_LOOT)
	{
		m_aLootCandidate[m_NumLootCandidates++] = Pos;
		return true;
	}
	if(Index == ENTITY_EXTRACTION_OUTPOST && m_NumOutposts < MAX_EXTRACTION_OUTPOSTS)
	{
		m_aOutpostPos[m_NumOutposts++] = Pos;
		return true;
	}
	if(Index == ENTITY_EXTRACTION_GUARD_SPAWN && m_NumGuardSpawnPos < MAX_ENEMIES)
	{
		m_aGuardSpawnPos[m_NumGuardSpawnPos++] = Pos;
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
	if(FindBossSpawnPosition(
		   &GameServer()->m_World, m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, pOutPos))
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

int CGameControllerExtract::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, const CAttackSource &Source)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Source);
	if(!pVictim->m_IsBot && GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnPlayerDeath(pVictim->GetPlayer()->GetCID());

	if(pVictim->m_IsBot)
	{
		if(pKiller && !pKiller->m_IsBot && GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnEnemyKilled(Source, pVictim->m_Pos, pVictim);
		pVictim->GetPlayer()->m_ToBeKicked = true;
	}
	else if(!pVictim->m_IsBot)
	{
		const int CID = pVictim->GetPlayer()->GetCID();
		DropCarriedLoot(CID, pVictim->m_Pos);
		m_aDownCount[CID]++;
		const int Seconds = max(10, 30 - m_aDownCount[CID] * 5);
		m_aBleedoutTick[CID] = Server()->Tick() + Server()->TickSpeed() * Seconds;
		m_apRevive[CID] = new CExtractionObject(&GameServer()->m_World, this, pVictim->m_Pos, CExtractionObject::TYPE_REVIVE, 0, CID);
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * 3600;
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

	// First switch already brings mid-boss pressure
	if(!m_MidBossSpawned && m_SwitchesActivated >= 1)
		SpawnMidBoss();

	if(m_SwitchesActivated >= m_SwitchesRequired)
	{
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnObjectiveComplete();
		if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->Enabled())
			m_DoorChoicePending = true;
		else
			BeginEvacuation();
	}
}

void CGameControllerExtract::OnDroidKilled(CDroid *pDroid)
{
	if(pDroid == m_pMidBoss)
		m_pMidBoss = 0;
}

void CGameControllerExtract::DisplayExit(vec2 Pos)
{
	m_EvacPos = Pos;
	if(!m_pEvacObject)
		m_pEvacObject = new CExtractionObject(&GameServer()->m_World, this, Pos, CExtractionObject::TYPE_EVAC);
	if(m_pDoor)
		m_pDoor->Activate(Pos);
}

CExtractionObject *CGameControllerExtract::FindExtractionObject(int ID) const
{
	for(CEntity *pEntity = GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_SCRIPTED); pEntity; pEntity = pEntity->TypeNext())
	{
		CExtractionObject *pObject = dynamic_cast<CExtractionObject *>(pEntity);
		if(pObject && pObject->GetID() == ID)
			return pObject;
	}
	return 0;
}

void CGameControllerExtract::SpawnLoot()
{
	const int Players = max(1, CountHumanPlayersLocal());
	m_Quota = 75 + 45 * (Players - 1);
	if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_LOCKED_ROUTE)
	{
		m_Quota = (m_Quota * 13 + 9) / 10;
		m_AlertLevel = 1;
	}
	// Static maps made before scavenging markers fall back to their enemy anchors.
	if(m_NumOutposts < 2)
		for(int i = 0; i < m_NumEnemySpawnPos && m_NumOutposts < 2; i++)
		{
			const vec2 Pos = m_aEnemySpawnPos[i];
			if(distance(Pos, m_EvacPos) < 24.0f * 32.0f)
				continue;
			bool FarEnough = true;
			for(int j = 0; j < m_NumOutposts; j++)
				if(distance(Pos, m_aOutpostPos[j]) < 28.0f * 32.0f)
					FarEnough = false;
			if(FarEnough)
				m_aOutpostPos[m_NumOutposts++] = Pos;
		}
	if(m_NumLootCandidates < 12)
		for(int i = 0; i < m_NumEnemySpawnPos && m_NumLootCandidates < 12; i++)
		{
			const vec2 Pos = m_aEnemySpawnPos[i] + vec2((i & 1) ? 48.0f : -48.0f, 0.0f);
			if(distance(Pos, m_EvacPos) < 10.0f * 32.0f || GameServer()->Collision()->CheckPoint(Pos))
				continue;
			m_aLootCandidate[m_NumLootCandidates++] = Pos;
		}
	int Total = 0;
	for(int i = 0; i < m_NumOutposts; i++)
		m_apOutposts[i] = new CExtractionObject(&GameServer()->m_World, this, m_aOutpostPos[i], CExtractionObject::TYPE_OUTPOST);
	for(int i = 0; i < m_NumLootCandidates && Total < (m_Quota * 3 + 1) / 2; i++)
	{
		int Value = 10;
		for(int j = 0; j < m_NumOutposts; j++)
			if(distance(m_aLootCandidate[i], m_aOutpostPos[j]) <= 10.0f * 32.0f)
				Value = (i + j) % 3 == 0 ? 50 : 25;
		if(Value == 10 && i % 3 == 0)
			Value = 25;
		if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_BLACK_BOX && i == 0)
			Value = 50;
		m_apLoot[i] = new CExtractionObject(&GameServer()->m_World, this, m_aLootCandidate[i], CExtractionObject::TYPE_LOOT, Value);
		Total += Value;
	}
	// A marked static layout may still have too little nominal value. Upgrade
	// its farthest remaining candidates before allowing an impossible quota.
	for(int i = 0; i < m_NumLootCandidates && Total < m_Quota; i++)
		if(!m_apLoot[i])
		{
			m_apLoot[i] = new CExtractionObject(&GameServer()->m_World, this, m_aLootCandidate[i], CExtractionObject::TYPE_LOOT, 50);
			Total += 50;
		}
	if(Total < m_Quota)
	{
		dbg_msg("extract", "layout disabled: loot value %d below quota %d", Total, m_Quota);
		GameServer()->SendBroadcast("Extraction map has insufficient loot markers", -1);
		m_RoundOverTick = Server()->Tick();
		m_Win = false;
	}
	dbg_msg("extract", "scavenge quota=%d generated_value=%d candidates=%d outposts=%d", m_Quota, Total, m_NumLootCandidates, m_NumOutposts);
}

void CGameControllerExtract::DropCarriedLoot(int ClientID, vec2 Pos)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aCarriedValue[ClientID] <= 0)
		return;
	for(int i = 0; i < MAX_EXTRACTION_LOOT; i++)
		if(!m_apLoot[i] || m_apLoot[i]->State() == 3)
		{
			m_apLoot[i] = new CExtractionObject(&GameServer()->m_World, this, Pos, CExtractionObject::TYPE_LOOT, m_aCarriedValue[ClientID]);
			break;
		}
	m_aCarriedValue[ClientID] = 0;
}

void CGameControllerExtract::TriggerOutpost(CExtractionObject *pOutpost)
{
	if(!pOutpost || pOutpost->State() != 0)
		return;
	pOutpost->SetState(1);
	m_AlertLevel = min(3, m_AlertLevel + 1);
	const int SpawnCount = min(4 + CountHumanPlayersLocal(), max(0, 18 - CountBots()));
	for(int i = 0; i < SpawnCount; i++)
	{
		m_EnemiesLeft++;
		GameServer()->AddBot();
	}
	TriggerAllBotAI(GameServer(), 8 + m_AlertLevel * 2);
	GameServer()->SendBroadcast("Outpost alerted — hostiles incoming", -1);
}

void CGameControllerExtract::OnInteract(int ClientID, int Target, bool Pressed)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_RoundOverTick || m_aBoarded[ClientID] || m_aEliminated[ClientID])
		return;
	m_aInteractionHeld[ClientID] = Pressed;
	if(!Pressed)
	{
		m_aInteractionTarget[ClientID] = -1;
		m_aInteractionTicks[ClientID] = 0;
		return;
	}
	m_aInteractionTarget[ClientID] = Target;
	m_aInteractionTicks[ClientID] = 0;
}

void CGameControllerExtract::OnClientDrop(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	DropCarriedLoot(ClientID, pChr ? pChr->m_Pos : m_EvacPos);
	m_aEliminated[ClientID] = true;
	m_aInteractionHeld[ClientID] = false;
}

bool CGameControllerExtract::WantsInventoryInteraction(int ClientID) const
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS && m_aInteractionHeld[ClientID];
}

void CGameControllerExtract::TickInteractions()
{
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(!m_aInteractionHeld[ClientID])
			continue;
		CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
		CExtractionObject *pObject = FindExtractionObject(m_aInteractionTarget[ClientID]);
		if(!pChr || !pChr->IsAlive() || !pObject || pObject->State() == 3 || distance(pChr->m_Pos, pObject->m_Pos) > 96.0f ||
		   GameServer()->Collision()->IntersectLine(pChr->m_Pos, pObject->m_Pos, 0, 0))
		{
			m_aInteractionHeld[ClientID] = false;
			m_aInteractionTicks[ClientID] = 0;
			continue;
		}
		const float Speed = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->InteractionSpeedBonus(ClientID) : 1.0f;
		m_aInteractionTicks[ClientID] += max(1, (int)(Speed + frandom()));
		int Required = Server()->TickSpeed();
		if(pObject->ObjectType() == CExtractionObject::TYPE_REVIVE)
			Required *= 3;
		else if(pObject->ObjectType() == CExtractionObject::TYPE_EVAC)
			Required *= 2;
		pObject->SetProgress(m_aInteractionTicks[ClientID] * 100 / max(1, Required));
		if(m_aInteractionTicks[ClientID] < Required)
			continue;

		if(pObject->ObjectType() == CExtractionObject::TYPE_LOOT && m_Phase == 1 && m_aCarriedValue[ClientID] == 0)
		{
			m_aCarriedValue[ClientID] = pObject->Value();
			pObject->SetState(3);
			for(int i = 0; i < m_NumOutposts; i++)
				if(m_apOutposts[i] && distance(m_apOutposts[i]->m_Pos, pObject->m_Pos) <= 10.0f * 32.0f)
					TriggerOutpost(m_apOutposts[i]);
			GameServer()->CreateSound(pObject->m_Pos, SOUND_PICKUP_ARMOR);
		}
		else if(pObject->ObjectType() == CExtractionObject::TYPE_EVAC)
		{
			if(m_aCarriedValue[ClientID] > 0)
			{
				const int Deposited = m_aCarriedValue[ClientID];
				m_DepositedValue += m_aCarriedValue[ClientID];
				m_aCarriedValue[ClientID] = 0;
				if(GameServer()->m_pPveDirector)
					GameServer()->m_pPveDirector->OnExtractionLootDeposited(Deposited);
				GameServer()->SendBroadcastFormat(-1, false, "Recovered value %d/%d", m_DepositedValue, m_Quota);
			}
			else if(m_Phase == 1 && m_DepositedValue >= m_Quota)
				BeginEvacuation();
		}
		else if(pObject->ObjectType() == CExtractionObject::TYPE_REVIVE)
		{
			const int Owner = pObject->Owner();
			if(Owner >= 0 && Owner < MAX_CLIENTS && m_aBleedoutTick[Owner] > Server()->Tick() && GameServer()->m_apPlayers[Owner] &&
			   GameServer()->m_apPlayers[Owner]->ForceRespawn(pObject->m_Pos))
			{
				if(GameServer()->GetPlayerChar(Owner))
					GameServer()->GetPlayerChar(Owner)->SetHealth(max(1, GameServer()->GetPlayerChar(Owner)->m_MaxHealth / 4));
				m_aBleedoutTick[Owner] = 0;
				m_apRevive[Owner] = 0;
				pObject->SetState(3);
				Server()->SendPlatformEvent(ClientID, PLATFORM_EVENT_COOP_RESCUE);
			}
		}
		m_aInteractionHeld[ClientID] = false;
		m_aInteractionTicks[ClientID] = 0;
		pObject->SetProgress(0);
	}
}

void CGameControllerExtract::BeginBoarding()
{
	if(m_Phase != 2)
		return;
	m_Phase = 3;
	m_PhaseEndTick = Server()->Tick() + Server()->TickSpeed() * 20;
	TriggerEscape();
	if(m_pEvacObject)
		m_pEvacObject->SetState(2);
	GameServer()->SendBroadcast("Dropship arrived — board now", -1);
}

void CGameControllerExtract::FinishExtraction()
{
	if(m_RoundOverTick)
		return;
	m_Win = m_Evacuated > 0;
	m_Phase = 4;
	m_RoundOverTick = Server()->Tick();
	if(GameServer()->m_pPveDirector)
	{
		GameServer()->m_pPveDirector->OnStageComplete(m_Win);
		if(m_Win)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(m_aBoarded[i])
					GameServer()->m_pPveDirector->RewardResearchPlayer(i, 2 + min(3, max(0, m_DepositedValue - m_Quota) / 50), PVE_REWARD_EXTRACTION);
		}
		else
			GameServer()->m_pPveDirector->CompleteContract(false);
	}
	GameServer()->SendBroadcast(m_Win ? "Extraction complete!" : "Extraction failed — nobody boarded", -1);
	if(m_Win)
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(m_aBoarded[i] && GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->m_IsBot)
			{
				Server()->SendPlatformEvent(i, PLATFORM_EVENT_FIRST_EXTRACTION);
				Server()->SendPlatformEvent(i, PLATFORM_EVENT_FIRST_COOP_COMPLETE);
				Server()->SendPlatformEvent(i, PLATFORM_EVENT_STAT_COOP_COMPLETIONS, 1);
			}
}

void CGameControllerExtract::SendExtractionState(int ClientID)
{
	if(!GameServer()->m_apPlayers[ClientID] || GameServer()->m_apPlayers[ClientID]->m_IsBot)
		return;
	CNetMsg_Sv_ExtractionState Msg;
	Msg.m_Phase = clamp(m_Phase, 0, 4);
	Msg.m_Deposited = m_DepositedValue;
	Msg.m_Quota = m_Quota;
	Msg.m_CarriedValue = m_aCarriedValue[ClientID];
	Msg.m_Alert = m_AlertLevel;
	Msg.m_PhaseEndTick = m_PhaseEndTick;
	Msg.m_Downed = m_aBleedoutTick[ClientID] > Server()->Tick();
	Msg.m_BleedoutSeconds = Msg.m_Downed ? max(0, (m_aBleedoutTick[ClientID] - Server()->Tick()) / Server()->TickSpeed()) : 0;
	Msg.m_Boarded = m_aBoarded[ClientID];
	Server()->SendPackMsg(&Msg, 0, ClientID);
}

void CGameControllerExtract::BeginEvacuation()
{
	if(m_DoorOpen)
		return;
	m_DoorChoicePending = false;
	m_DoorChoiceStarted = false;
	m_DoorOpen = true;
	m_Phase = 2;
	m_PhaseEndTick = Server()->Tick() + Server()->TickSpeed() * 25;
	m_EvacNeeded = max(1, CountHumans());
	m_Evacuated = 0;
	if(GameServer()->m_pPveDirector)
	{
		GameServer()->m_pPveDirector->OnStageStart();
		GameServer()->m_pPveDirector->OnEvacuationStarted();
	}
	SpawnEscapePressure();
	GameServer()->SendBroadcast("Dropship called — hold the extraction zone", -1);
}

void CGameControllerExtract::NextLevel(int CID)
{
	if(m_Phase != 3 || m_RoundOverTick)
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
	m_aBoarded[CID] = true;
	m_Evacuated++;
	GameServer()->SendBroadcastFormat(-1, false, "Evacuated %d/%d", m_Evacuated, m_EvacNeeded);

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
	TickInteractions();
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(m_aBleedoutTick[ClientID] > 0 && m_aBleedoutTick[ClientID] <= Server()->Tick())
		{
			m_aBleedoutTick[ClientID] = 0;
			m_aEliminated[ClientID] = true;
			if(m_apRevive[ClientID])
			{
				m_apRevive[ClientID]->SetState(3);
				m_apRevive[ClientID] = 0;
			}
		}
		if(Server()->Tick() % max(1, Server()->TickSpeed() / 5) == 0)
			SendExtractionState(ClientID);
	}
	if(m_Phase == 2 && m_PhaseEndTick <= Server()->Tick())
		BeginBoarding();
	if(m_Phase == 3 && m_PhaseEndTick <= Server()->Tick())
		FinishExtraction();
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
			m_Phase = 1;
			vec2 ExitPos;
			if(FindEscape(&ExitPos))
				DisplayExit(ExitPos);
			SpawnLoot();
			SpawnInitialEnemies();
			m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * 5;
			m_TriggerTick = Server()->Tick() + Server()->TickSpeed() * 2;
			GameServer()->SendBroadcast("Extraction — recover valuables and meet the quota", -1);
		}
		return;
	}

	const int HumansAlive = CountHumansAliveLocal();
	if(HumansAlive > 0)
		m_HadHumanAlive = true;

	if(!m_RoundOverTick && m_HadHumanAlive && HumansAlive <= 0 && m_Evacuated <= 0 &&
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
		m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * (m_Phase <= 1 ? 7 : 4);
		const float Pressure = m_Phase == 1 && GameServer()->m_pPveDirector
								   ? GameServer()->m_pPveDirector->ReinforcementMultiplier()
								   : 1.0f;
		const int Cap = (int)((m_Phase <= 1 ? 12 : 18) * Pressure);
		if(CountBots() < Cap)
		{
			const int Count = max(1, (int)(3 * Pressure));
			m_EnemiesLeft += Count;
			for(int i = 0; i < Count; i++)
				GameServer()->AddBot();
		}
	}

	if(false && m_Phase == 0 && m_MidBossSpawned && !m_MidBossPerkOffered && !m_pMidBoss)
	{
		m_MidBossPerkOffered = true;
		if(GameServer()->m_pPveDirector)
		{
			GameServer()->m_pPveDirector->StartIntermission(false, true);
			if(GameServer()->m_pPveDirector->InIntermission())
				return;
		}
	}
	if(false && m_Phase == 0 && m_DoorChoicePending && m_MidBossPerkOffered && !m_DoorChoiceStarted)
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
	if(false && m_Phase == 0 && !m_DoorOpen && m_AvailableSwitches <= 0 && m_StartTick &&
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

	if(m_Phase <= 1)
	{
		pGameDataObj->m_TeamscoreRed = QUEST_EXTRACT;
		pGameDataObj->m_TeamscoreBlue = max(0, m_Quota - m_DepositedValue);
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
	pGameDataObj->m_FlagCarrierBlue = (min(3, m_Phase) << 8) | (m_Win ? 1 : 0);
}
