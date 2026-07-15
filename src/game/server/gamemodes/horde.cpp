#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/questinfo.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/bosspool.h>
#include <game/server/entities/droid_crawler.h>
#include <game/server/entities/radar.h>
#include <game/server/ai/base_ai.h>
#include <game/server/pve_bots.h>
#include <game/server/pve_director.h>
#include <game/server/pve_operation_director.h>
#include <game/weapons.h>

#include "horde.h"

static int HordeDifficultyTier()
{
	// The local difficulty slider already scales bot health through map level.
	// Add only a modest archetype offset so higher settings introduce stronger
	// enemies instead of becoming pure health inflation.
	return max(0, (g_Config.m_SvMapGenLevel - 1) / 10);
}

static int HordeConcurrentEnemyCap(int Wave)
{
	return min(18, 12 + Wave / 2);
}

CGameControllerHorde::CGameControllerHorde(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pOperationDirector = new CPveOperationDirector(pGameServer);
	m_pGameType = "HORDE";
	m_GameFlags = GAMEFLAG_COOP;
	m_GameState = STATE_STARTING;

	for(int i = 0; i < MAX_ENEMIES; i++)
		m_aEnemySpawnPos[i] = vec2(0, 0);

	m_RoundOverTick = 0;
	m_NoPlayersTick = 0;
	m_GameOverBroadcast = false;
	m_WaveStartTick = 0;
	m_Wave = 0;
	m_Kills = 0;
	m_EnemyCount = 0;
	m_EnemiesLeft = 0;
	m_Deaths = 0;
	m_NumEnemySpawnPos = 0;
	m_SpawnPosRotation = 0;
	m_TriggerTick = 0;
	m_TriggerLevel = 8;
	m_RogueliteStarted = false;
	m_RogueliteWaitTick = Server()->Tick() + Server()->TickSpeed() * 2;
	m_LastIntermissionWave = -1;
	m_LastContractProgressWave = -1;
	m_EliteContractSpawned = false;
	m_BossCountCacheTick = -1;
	m_BossCountCache = 0;
	m_DefenseAreaCenter = vec2(0, 0);
	m_DefenseAreaReady = false;

	g_Config.m_SvOneHitKill = 0;
	g_Config.m_SvWarmup = 0;
	g_Config.m_SvEnableBuilding = 1;
	g_Config.m_SvDisablePVP = 1;
	g_Config.m_SvSurvivalTime = 0;
	g_Config.m_SvSurvivalAcid = 0;
	// keep map as-is; optional one-shot mapgen via cfg, never level++
	dbg_msg("horde", "rules: target_waves=%d roguelite=%d contracts=%d seed=%d random_seed=%d",
		g_Config.m_SvScorelimit, g_Config.m_SvPveRoguelite, g_Config.m_SvPveContracts,
		g_Config.m_SvMapGenSeed, g_Config.m_SvMapGenRandSeed);

	if(g_Config.m_SvEnableBuilding)
		m_GameFlags |= GAMEFLAG_BUILD;
	if(g_Config.m_SvSurvivalMode)
		m_GameFlags |= GAMEFLAG_SURVIVAL;
}

CGameControllerHorde::~CGameControllerHorde()
{
	delete m_pOperationDirector;
}

bool CGameControllerHorde::OnEntity(int Index, vec2 Pos)
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
	return IGameController::OnEntity(Index, Pos);
}

bool CGameControllerHorde::GetSpawnPos(int Team, vec2 *pOutPos)
{
	if(m_NumEnemySpawnPos <= 0)
		return false;
	m_SpawnPosRotation = (m_SpawnPosRotation + 1) % m_NumEnemySpawnPos;
	*pOutPos = m_aEnemySpawnPos[m_SpawnPosRotation];
	return true;
}

bool CGameControllerHorde::GetBossSpawnPos(vec2 *pOutPos)
{
	if(FindBossSpawnPosition(&GameServer()->m_World, m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, pOutPos))
		return true;
	if(!GetSpawnPos(0, pOutPos))
		return false;
	*pOutPos += vec2(0.0f, -100.0f);
	return true;
}

bool CGameControllerHorde::CanSpawn(int Team, vec2 *pOutPos, bool IsBot)
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

int CGameControllerHorde::AliveBossCount()
{
	const int Tick = Server()->Tick();
	if(m_BossCountCacheTick != Tick)
	{
		m_BossCountCacheTick = Tick;
		m_BossCountCache = CountAliveBosses(&GameServer()->m_World);
	}
	return m_BossCountCache;
}

int CGameControllerHorde::CountHumansAlive(int ExcludeCID) const
{
	int Alive = 0;
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		if(ClientID == ExcludeCID)
			continue;
		CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
		if(!pPlayer || pPlayer->m_IsBot || pPlayer->m_pAI || pPlayer->GetTeam() == TEAM_SPECTATORS)
			continue;
		if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
			Alive++;
	}
	return Alive;
}

void CGameControllerHorde::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);
	if(!RequestAI)
		EnsureDefenseArea(pChr->m_Pos);
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

void CGameControllerHorde::EnsureDefenseArea(vec2 FallbackPos)
{
	if(m_DefenseAreaReady)
		return;
	m_DefenseAreaCenter = FallbackPos;
	m_DefenseAreaReady = true;
	CRadar *pRadar = new CRadar(&GameServer()->m_World, RADAR_REACTOR);
	pRadar->Activate(m_DefenseAreaCenter);
	dbg_msg("horde", "defense area: center=(%.0f,%.0f) radius=%d",
		m_DefenseAreaCenter.x, m_DefenseAreaCenter.y, PVE_HORDE_DEFENSE_RADIUS);
}

bool CGameControllerHorde::InDefenseArea(vec2 Pos) const
{
	return m_DefenseAreaReady && distance(Pos, m_DefenseAreaCenter) <= PVE_HORDE_DEFENSE_RADIUS;
}

int CGameControllerHorde::EnemyLevel() const
{
	// Wave 1 ~3–4, then climbs; caps so late waves stay tough but readable
	return min(14, max(3, 2 + m_Wave + (m_Wave / 3) + HordeDifficultyTier()));
}

int CGameControllerHorde::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
	CPlayer *pVictimPlayer = pVictim->GetPlayer();
	const bool HumanVictim = !pVictim->m_IsBot && pVictimPlayer && !pVictimPlayer->m_IsBot && !pVictimPlayer->m_pAI;
	if(HumanVictim && GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnPlayerDeath(pVictim->GetPlayer()->GetCID());

	if(pVictim->m_IsBot)
	{
		if(!pVictim->GetPlayer()->m_ToBeKicked)
			m_Deaths = max(0, m_Deaths - 1);
		if(pKiller && !pKiller->m_IsBot)
		{
			m_Kills++;
			if(GameServer()->m_pPveDirector)
				GameServer()->m_pPveDirector->OnEnemyKilled(pKiller->GetCID(), Weapon, pVictim->m_Pos, pVictim);
		}
		pVictim->GetPlayer()->m_ToBeKicked = true;
	}
	else if(HumanVictim && g_Config.m_SvSurvivalMode && !m_RoundOverTick &&
		CountHumansAlive(pVictimPlayer->GetCID()) <= 0)
	{
		// OnCharacterDeath runs before the victim is marked dead. Excluding its
		// CID makes a solo death and the final death of a party end the run, while
		// leaving a dead player revivable whenever another human is still alive.
		DeathMessage();
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->CompleteContract(false);
		m_RoundOverTick = Server()->Tick();
	}
	else if(HumanVictim)
	{
		const bool RespawnAllowed = !GameServer()->m_pPveDirector || GameServer()->m_pPveDirector->RespawnAllowed();
		pVictimPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * (RespawnAllowed ? g_Config.m_SvRespawnDelay : 3600);
	}

	return 0;
}

void CGameControllerHorde::NextWave()
{
	m_WaveStartTick = 0;
	m_Wave++;
	m_TriggerLevel = 8 + m_Wave;

	GameServer()->SendBroadcastFormat(-1, false, "Wave %d", m_Wave);

	const int BaseEnemies = min(10 + m_Wave * 3, 48);
	const int HumanPlayers = clamp(CountHumans(), 1, 4);
	const float PartyScale = 1.0f + (HumanPlayers - 1) * 0.20f;
	m_EnemiesLeft = (int)(BaseEnemies * PartyScale + 0.5f);
	if(GameServer()->m_pPveDirector)
	{
		GameServer()->m_pPveDirector->OnStageStart();
		m_EnemiesLeft = (int)(m_EnemiesLeft * GameServer()->m_pPveDirector->EnemyCountMultiplier() + 0.5f);
	}
	m_Deaths = m_EnemiesLeft;
	const SThreatBudgetResult ThreatReplacement = SpawnThreatBudgetSpecialists(&GameServer()->m_World,
		m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, EnemyLevel(), m_EnemiesLeft,
		HordeConcurrentEnemyCap(m_Wave));
	m_EnemiesLeft -= ThreatReplacement.m_ThreatSpent;
	m_Deaths -= ThreatReplacement.m_ThreatSpent;

	if(m_Wave % 2 == 0)
	{
		vec2 p;
		if(GetSpawnPos(0, &p))
			new CCrawler(&GameServer()->m_World, p + vec2(0, -100));
		if(m_Wave >= 6 && GetSpawnPos(0, &p))
			new CCrawler(&GameServer()->m_World, p + vec2(0, -100));
	}
	const int Operation = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ActiveOperation() : -1;
	if(Operation == PVE_OPERATION_ASSEMBLY_SURGE && m_Wave % 2 == 1)
	{
		vec2 p;
		if(GetSpawnPos(0, &p))
			new CCrawler(&GameServer()->m_World, p + vec2(0, -100));
	}
	if(Operation == PVE_OPERATION_GRID_STORM && (m_Wave - 1) % 4 + 1 == 3)
	{
		vec2 p;
		if(!GetBossSpawnPos(&p))
			p = vec2(4000, 4000);
		SpawnBoss(&GameServer()->m_World, p, EnemyLevel());
	}

	if(m_Wave > 0 && m_Wave % 4 == 0)
	{
		vec2 p;
		if(!GetBossSpawnPos(&p))
			p = vec2(4000, 4000);
		SpawnBoss(&GameServer()->m_World, p, max(5, m_Wave + HordeDifficultyTier()));
		GameServer()->SendBroadcast("Boss incoming!", -1);
	}

	if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_BOSS_RUSH)
	{
		const int SectionWave = (m_Wave - 1) % 4 + 1;
		if(SectionWave == 2 || SectionWave == 4)
		{
			vec2 p;
			if(!GetBossSpawnPos(&p))
				p = vec2(4000, 4000);
			SpawnBoss(&GameServer()->m_World, p, max(5, m_Wave + 2));
		}
	}
	if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_ELITE_HUNT && !m_EliteContractSpawned)
	{
		vec2 p;
		if(!GetBossSpawnPos(&p))
			p = vec2(4000, 4000);
		CDroid *pBoss = SpawnBoss(&GameServer()->m_World, p, EnemyLevel() + 2);
		GameServer()->m_pPveDirector->RegisterEliteContractBoss(pBoss);
		m_EliteContractSpawned = true;
	}

	const int Cap = max(0, HordeConcurrentEnemyCap(m_Wave) - ThreatReplacement.m_EntitiesSpawned);
	for(int i = 0; i < m_EnemiesLeft && CountBots() < Cap; i++)
		GameServer()->AddBot();

	TriggerAllBotAI(GameServer(), m_TriggerLevel);
}

void CGameControllerHorde::Tick()
{
	IGameController::Tick();
	const int ActiveOperation = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ActiveOperation() : -1;
	if(ActiveOperation >= 0 && m_pOperationDirector->Operation() != ActiveOperation)
		m_pOperationDirector->Start(ActiveOperation);
	else if(ActiveOperation < 0 && m_pOperationDirector->Operation() >= 0)
		m_pOperationDirector->Clear();
	m_pOperationDirector->Tick();
	if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->InIntermission())
		return;
	if(m_EliteContractSpawned && GameServer()->m_pPveDirector && AliveBossCount() <= 0)
	{
		m_EliteContractSpawned = false;
		GameServer()->m_pPveDirector->OnBossKilled();
	}

	if(m_Wave > 0 && !m_NoPlayersTick && CountHumans() <= 0)
		m_NoPlayersTick = Server()->Tick() + Server()->TickSpeed() * 10.0f;

	if(m_NoPlayersTick && m_NoPlayersTick < Server()->Tick())
	{
		m_NoPlayersTick = 0;
		m_Wave = 0;
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->ClearRun();
		EndRound();
	}

	if(m_GameState == STATE_STARTING)
	{
		if(CountPlayers(0) > 0 && !m_WaveStartTick)
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
			m_GameState = STATE_GAME;
			m_WaveStartTick = Server()->Tick() + Server()->TickSpeed() * 5.0f;
			m_Wave = 0;
			GameServer()->SendBroadcast("Horde — survive the waves", -1);
		}
	}
	else
	{
		if(!m_RoundOverTick && m_WaveStartTick && m_WaveStartTick < Server()->Tick())
			NextWave();

		// Waves can contain more enemies than the safe concurrent bot cap. Feed
		// the remaining queue as slots open; without this, later waves stopped
		// permanently after their first batch was killed.
		if(!m_RoundOverTick && !m_WaveStartTick && m_EnemiesLeft > 0)
		{
			const int Missing = max(0, HordeConcurrentEnemyCap(m_Wave) - CountAliveSpecialists(&GameServer()->m_World) - CountBots());
			const int SpawnCount = min(m_EnemiesLeft, Missing);
			for(int i = 0; i < SpawnCount; i++)
				GameServer()->AddBot();
		}

		if(!m_RoundOverTick && m_Deaths <= 0 && !CountBotsAlive() && CountAliveSpecialists(&GameServer()->m_World) <= 0 && AliveBossCount() <= 0
			&& CountPlayersAlive(-1, true) > 0 && !m_WaveStartTick)
		{
			m_pOperationDirector->OnEvent(CPveOperationDirector::EVENT_WAVE);
			if(m_LastContractProgressWave != m_Wave && GameServer()->m_pPveDirector)
			{
				m_LastContractProgressWave = m_Wave;
				GameServer()->m_pPveDirector->OnStageComplete(true);
			}
			const bool ContractBoundary = m_Wave > 0 && m_Wave % 4 == 0;
			const bool PerkBoundary = m_Wave > 0 && m_Wave % 3 == 0;
			const bool RunComplete = g_Config.m_SvScorelimit > 0 && m_Wave >= g_Config.m_SvScorelimit;
			if((ContractBoundary || PerkBoundary) && m_LastIntermissionWave != m_Wave && GameServer()->m_pPveDirector)
			{
				m_LastIntermissionWave = m_Wave;
				if(ContractBoundary)
					GameServer()->m_pPveDirector->RewardResearch(1, PVE_REWARD_HORDE_SECTION);
				if(!RunComplete)
				{
					GameServer()->m_pPveDirector->StartIntermission(ContractBoundary, PerkBoundary);
					if(GameServer()->m_pPveDirector->InIntermission())
						return;
				}
			}
			if(RunComplete)
			{
				GameServer()->SendBroadcastFormat(-1, false, "Cleared %d waves!", m_Wave);
				m_RoundOverTick = Server()->Tick();
			}
			else
			{
				m_WaveStartTick = Server()->Tick() + Server()->TickSpeed() * 6.0f;
				GameServer()->SendBroadcast("Wave cleared", -1);
			}
		}

		if(m_RoundOverTick && !m_GameOverBroadcast && m_RoundOverTick < Server()->Tick() - Server()->TickSpeed() * 2.0f)
		{
			m_GameOverBroadcast = true;
			GameServer()->SendBroadcastFormat(-1, false, "Survived %d waves — %d kills", m_Wave, m_Kills);
		}

		if(m_RoundOverTick && m_RoundOverTick < Server()->Tick() - Server()->TickSpeed() * 6.0f)
		{
			m_RoundOverTick = 0;
			if(GameServer()->m_pPveDirector)
				GameServer()->m_pPveDirector->ClearRun();
			EndRound();
		}
	}

	if(m_TriggerTick < Server()->Tick())
	{
		TriggerAllBotAI(GameServer(), m_TriggerLevel);
		m_TriggerTick = Server()->Tick() + Server()->TickSpeed() * 3;
	}

	GameServer()->UpdateAI();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(pPlayer && pPlayer->m_IsBot && pPlayer->m_ToBeKicked)
			GameServer()->KickBot(pPlayer->GetCID());
	}
}

void CGameControllerHorde::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	pGameDataObj->m_TeamscoreRed = QUEST_HORDE;
	pGameDataObj->m_TeamscoreBlue = m_Deaths + AliveBossCount();
	pGameDataObj->m_FlagCarrierRed = m_Wave;
	pGameDataObj->m_FlagCarrierBlue = m_Kills;
}
