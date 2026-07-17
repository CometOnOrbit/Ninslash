#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/questinfo.h>
#include <game/pve_roguelite.h>
#include <game/weapons.h>

#include <game/server/entities/character.h>
#include <game/server/entities/building.h>
#include <game/server/entities/droid.h>
#include <game/server/bosspool.h>
#include <game/server/entities/radar.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/pve_director.h>
#include <game/server/pve_operation_director.h>

#include "invasion.h"

#include <game/server/playerdata.h>
#include <game/server/ai.h>
#include <game/server/ai/inv/robot1_ai.h>
#include <game/server/ai/inv/robot2_ai.h>
#include <game/server/ai/inv/alien1_ai.h>
#include <game/server/ai/inv/alien2_ai.h>
#include <game/server/ai/inv/bunny1_ai.h>
#include <game/server/ai/inv/bunny2_ai.h>
#include <game/server/ai/inv/pyro1_ai.h>
#include <game/server/ai/inv/pyro2_ai.h>

static CAI* CreateAIalien1(CGameContext *pGameServer, CPlayer *pPlayer, int Level) { return new CAIalien1(pGameServer, pPlayer, Level); }
static CAI* CreateAIrobot1(CGameContext *pGameServer, CPlayer *pPlayer, int Level) { return new CAIrobot1(pGameServer, pPlayer, Level); }
static CAI* CreateAIpyro1(CGameContext *pGameServer, CPlayer *pPlayer, int Level) { return new CAIpyro1(pGameServer, pPlayer, Level); }
static CAI* CreateAIbunny1(CGameContext *pGameServer, CPlayer *pPlayer, int Level) { return new CAIbunny1(pGameServer, pPlayer, Level); }
static CAI* CreateAIrobot2(CGameContext *pGameServer, CPlayer *pPlayer, int Level) { (void)Level; return new CAIrobot2(pGameServer, pPlayer); }
static CAI* CreateAIalien2(CGameContext *pGameServer, CPlayer *pPlayer, int Level) { (void)Level; return new CAIalien2(pGameServer, pPlayer); }
static CAI* CreateAIbunny2(CGameContext *pGameServer, CPlayer *pPlayer, int Level) { (void)Level; return new CAIbunny2(pGameServer, pPlayer); }
static CAI* CreateAIpyro2(CGameContext *pGameServer, CPlayer *pPlayer, int Level) { (void)Level; return new CAIpyro2(pGameServer, pPlayer); }

static const float INV_QUEST_QUEUE_TIME = 1.5f;
static const float INV_QUEST_DOOR_TIME = 3.0f;
static const int INV_FINAL_ATTEMPT = 6;
static const int INV_FORCE_FLOOR_ONE = 7;

static int InvasionDepthQuests(int Level)
{
	if (Level >= 21)
		return min(2 + Level/12, 3);
	return 2;
}

static int InvasionOpeningEnemies(int Level)
{
	return min(18, max(7, 6 + Level));
}

static int InvasionWaveCap(int Level, int Players)
{
	return min(28, 10 + Level/2 + Players);
}

CGameControllerInvasion::CGameControllerInvasion(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pOperationDirector = new CPveOperationDirector(pGameServer);
	m_pGameType = "INV";
	m_GameFlags = GAMEFLAG_COOP;
	m_GameState = STATE_STARTING;
	// Regeneration loads the source template first and generated.map second.
	// Keep the marker through the transient template controller and consume it
	// only in the controller that owns the final generated map.
	m_ForceFloorOne = g_Config.m_SvInvFails == INV_FORCE_FLOOR_ONE && Server()->m_MapGenerated;
	if(m_ForceFloorOne)
	{
		g_Config.m_SvInvFails = 0;
		dbg_msg("inv", "forced Floor 1 reset applied on generated map");
	}
	
	m_BotSpawnTick = 0;
	
	if (g_Config.m_SvMapGenRandSeed)
	{
		g_Config.m_SvMapGenSeed = rand()%32767;
		g_Config.m_SvMapGenRandSeed = 0;
	}
	
	srand(g_Config.m_SvMapGenLevel + g_Config.m_SvMapGenSeed);
	
	for (int i = 0; i < MAX_ENEMIES; i++)
		m_aEnemySpawnPos[i] = vec2(0, 0);
	
	m_RoundOverTick = 0;
	m_RoundWinTick = 0;
	m_RoundWin = false;
	m_QuestsCompleted = 0;
	
	m_QuestWaveSize = 0;
	m_QuestWaveEndTick = 0;
	m_QuestWaveEnemiesLeft = 0;
	m_Quest = QUEST_NONE;
	m_NextQuest = QUEST_NONE;
	m_QuestChangeTick = 0;
	m_QuestProgressCounter = 0;
	m_QuestWaveType = WAVE_NONE;
	m_EliteWave = false;
	m_DefendEndTick = 0;
	m_DefendPrepEndTick = 0;
	m_SwitchesRequired = 0;
	m_SwitchesActivated = 0;
	m_BossesLeft = 0;
	m_DefendLevel = false;
	m_SwitchCoopLevel = false;
	m_ReactorCountCheckTick = 0;
	m_CachedReactorsLeft = 0;
	m_ForcedWaveType = WAVE_NONE;
	m_WaveSizeNerf = 0;
	m_RunBuffActive = false;
	m_ProgressSynced = false;
	m_RogueliteWaitTick = 0;
	m_StartBriefingSent = false;
	m_RogueliteOpeningStarted = false;
	m_RogueliteStageStarted = false;
	m_RogueliteCompletionStarted = false;
	m_EliteContractSpawned = false;
	m_CheckpointApplied = false;
	m_OperationOrdinaryWaveActive = false;
	m_RouteQuestActive = false;
	m_RouteTargetType = 0;
	m_RetryVoteNonce = 0;
	m_RetryVoteEndTick = 0;
	m_RetryVoteLastSyncTick = 0;
	m_RetryResult = PVE_INVASION_RETRY_RESULT_RESET;
	m_RetryResultEndTick = 0;
	m_RetryResultLastSyncTick = 0;
	m_aRetryPlayerName[0] = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aRetryVotes[i] = -1;
	
	m_TriggerLevel = 0;
	m_GroupSpawnPos = vec2(0, 0);
	m_EscapeSpawnActive = false;

	SetupLevelTheme();

	m_AutoRestart = false;
	
	m_NumEnemySpawnPos = 0;
	m_SpawnPosRotation = 0;
	m_TriggerTick = 0;
	
	g_Config.m_SvRandomWeapons = 0;
	g_Config.m_SvOneHitKill = 0;
	g_Config.m_SvWarmup = 0;
	g_Config.m_SvTimelimit = 0;
	g_Config.m_SvScorelimit = 0;
	g_Config.m_SvSurvivalTime = 0;
	g_Config.m_SvEnableBuilding = 1;
	g_Config.m_SvDisablePVP = 1;
	
	if (g_Config.m_SvEnableBuilding)
		m_GameFlags |= GAMEFLAG_BUILD;
	
	if (g_Config.m_SvSurvivalMode)
		m_GameFlags |= GAMEFLAG_SURVIVAL;
	
	m_GameFlags |= GAMEFLAG_ACID;
	
	for (int i = 0; i < MAX_CLIENTS; i++)
		new CRadar(&GameServer()->m_World, RADAR_HUMAN, i);
	
	m_pDoor = new CRadar(&GameServer()->m_World, RADAR_DOOR);
	m_pEnemySpawn = new CRadar(&GameServer()->m_World, RADAR_ENEMY);
	m_pReactor = new CRadar(&GameServer()->m_World, RADAR_REACTOR);
	m_NumSwitchRadars = 0;
	for(int i = 0; i < 8; i++)
		m_apSwitchRadar[i] = 0;
}

CGameControllerInvasion::~CGameControllerInvasion()
{
	delete m_pOperationDirector;
}


void CGameControllerInvasion::SetupLevelTheme()
{
	int Level = g_Config.m_SvMapGenLevel;
	m_LevelTheme = InvasionThemeFromLevel(Level);
	m_EscapeLevel = (m_LevelTheme == INVASION_THEME_ACID_ESCAPE);
	m_DefendLevel = (m_LevelTheme == INVASION_THEME_REACTOR_DEFEND);
	m_SwitchCoopLevel = (m_LevelTheme == INVASION_THEME_DUAL_SWITCHES);
	m_EliteWave = (m_LevelTheme == INVASION_THEME_ELITE_WAVE);
	m_SwitchesRequired = m_SwitchCoopLevel ? 2 : (m_EscapeLevel ? 1 : 0);
	m_SwitchesActivated = 0;

	// Fast floors: clear + one signature objective (deeper runs add a third).
	const int DepthQuests = InvasionDepthQuests(Level);

	switch (m_LevelTheme)
	{
	case INVASION_THEME_BOSS_ASSAULT:
		m_LevelQuestsLeft = max(2, DepthQuests);
		m_BossesLeft = 1 + Level/25;
		break;
	case INVASION_THEME_DUAL_SWITCHES:
		m_LevelQuestsLeft = max(2, DepthQuests);
		break;
	case INVASION_THEME_REACTOR_DEFEND:
		m_LevelQuestsLeft = max(2, DepthQuests);
		break;
	case INVASION_THEME_TIMED_SURVIVE:
		m_LevelQuestsLeft = max(2, DepthQuests);
		break;
	case INVASION_THEME_ELITE_WAVE:
		m_LevelQuestsLeft = max(2, DepthQuests);
		break;
	case INVASION_THEME_ACID_ESCAPE:
		m_LevelQuestsLeft = 0;
		break;
	default:
		m_LevelQuestsLeft = max(2, DepthQuests);
		break;
	}

	if (m_EscapeLevel)
	{
		m_EnemiesLeft = min(10, 4 + Level/4);
		m_QuestWaveSize = 10;
		m_QuestWaveEnemiesLeft = 0;
		m_QuestWaveEndTick = 0;
		m_Deaths = m_QuestWaveSize;
		m_EnemyCount = 0;
		m_RouteQuestActive = false;
	}
	else
	{
		SpawnNewWave(false);
		m_EnemiesLeft = InvasionOpeningEnemies(Level);
		m_QuestWaveSize = InvasionWaveCap(Level, max(1, CountPlayers(0)));
		m_Deaths = m_QuestWaveSize;
	}

	if (g_Config.m_SvInvFails != INV_FORCE_FLOOR_ONE && g_Config.m_SvInvFails >= 2)
	{
		m_RunBuffActive = true;
		m_WaveSizeNerf = 1;
	}
	else if (g_Config.m_SvInvFails != INV_FORCE_FLOOR_ONE && g_Config.m_SvInvFails >= 1)
		m_WaveSizeNerf = 1;
}


bool CGameControllerInvasion::OnEntity(int Index, vec2 Pos)
{
	// Invasion switches are objective entities. Keep them absent from snapshots
	// and collision until the matching quest actually becomes active.
	if(Index == ENTITY_SWITCH)
	{
		CBuilding *pSwitch = new CBuilding(&GameServer()->m_World, Pos + vec2(0, -10), BUILDING_SWITCH, TEAM_NEUTRAL);
		pSwitch->SetPveSwitchActive(false);
		return true;
	}
	if (IGameController::OnEntity(Index, Pos))
		return true;

	if (Index == ENTITY_ENEMYSPAWN && m_NumEnemySpawnPos < MAX_ENEMIES)
	{
		m_aEnemySpawnPos[m_NumEnemySpawnPos++] = Pos;
		m_pOperationDirector->AddCandidate(Pos);
		return true;
	}
	
	return false;
}


bool CGameControllerInvasion::GetSpawnPos(int Team, vec2 *pOutPos)
{
	if (!pOutPos || !m_NumEnemySpawnPos)
		return false;

	m_SpawnPosRotation++;
	m_SpawnPosRotation = m_SpawnPosRotation%m_NumEnemySpawnPos;
	
	*pOutPos = m_aEnemySpawnPos[m_SpawnPosRotation];
	return true;
}

bool CGameControllerInvasion::GetBossSpawnPos(vec2 *pOutPos)
{
	if(FindBossSpawnPosition(&GameServer()->m_World, m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, pOutPos))
		return true;
	if(!GetSpawnPos(0, pOutPos))
		return false;
	*pOutPos += vec2(0.0f, -100.0f);
	return true;
}

vec2 CGameControllerInvasion::GetBotSpawnPos()
{
	if (m_GroupSpawnPos.x < 1.0f)
	{
		vec2 p;
		GetSpawnPos(0, &p);
		return p;
	}
	
	vec2 Pos = m_GroupSpawnPos;
	
	for (int i = 0; i < 99; i++)
	{
		Pos = m_GroupSpawnPos + vec2(frandom()-frandom(), frandom()-frandom()) * 400;
		if (!GameServer()->Collision()->TestBox(Pos, vec2(32.0f, 74.0f)))
			return Pos;
	}

	return m_GroupSpawnPos;
}

void CGameControllerInvasion::RandomGroupSpawnPos()
{
	if (!m_NumEnemySpawnPos)
		return;
	m_GroupSpawnPos = m_aEnemySpawnPos[rand()%m_NumEnemySpawnPos];
	m_pEnemySpawn->Activate(m_GroupSpawnPos, Server()->Tick() + Server()->TickSpeed()*5);
}



bool CGameControllerInvasion::CanSpawn(int Team, vec2 *pOutPos, bool IsBot)
{
	CSpawnEval Eval;

	if(Team == TEAM_SPECTATORS)
		return false;

	if (IsBot)
	{
		if (m_EnemiesLeft <= 0)
			return false;
		
		if (m_BotSpawnTick > Server()->Tick())
			return false;
		
		if (m_GroupSpawnPos.x < 1.0f && GetSpawnPos(1, pOutPos))
			return true;
	
		vec2 Pos = GetBotSpawnPos();
		*pOutPos = Pos;
		
		m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * max(0.1f, 0.5f-g_Config.m_SvMapGenLevel*0.01f);
		
		return true;
	}
	else
		EvaluateSpawnType(&Eval, 0);

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}


static bool IsHumanCoopPlayer(const CPlayer *pPlayer)
{
	return pPlayer && !pPlayer->m_IsBot && !pPlayer->m_pAI;
}


int CGameControllerInvasion::CountHumansAlive(int ExcludeCID) const
{
	int Num = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (i == ExcludeCID)
			continue;
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if (!IsHumanCoopPlayer(pPlayer) || pPlayer->GetTeam() == TEAM_SPECTATORS)
			continue;
		if (pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
			Num++;
	}
	return Num;
}


bool CGameControllerInvasion::IsRetryVoter(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !Server()->ClientIngame(ClientID))
		return false;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	return IsHumanCoopPlayer(pPlayer) && pPlayer->GetTeam() != TEAM_SPECTATORS;
}


int CGameControllerInvasion::RetryVoterCount() const
{
	int Count = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsRetryVoter(i))
			Count++;
	return Count;
}


void CGameControllerInvasion::CountRetryVotes(int *pRetry, int *pReset, int *pVoted) const
{
	int Retry = 0;
	int Reset = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!IsRetryVoter(i))
			continue;
		if(m_aRetryVotes[i] == PVE_INVASION_RETRY)
			Retry++;
		else if(m_aRetryVotes[i] == PVE_INVASION_RESET)
			Reset++;
	}
	if(pRetry)
		*pRetry = Retry;
	if(pReset)
		*pReset = Reset;
	if(pVoted)
		*pVoted = Retry + Reset;
}


void CGameControllerInvasion::SendRetryVote(int ClientID)
{
	if(m_GameState != STATE_RETRY_VOTE || m_RetryVoteNonce <= 0)
		return;
	if(ClientID < 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(IsRetryVoter(i))
				SendRetryVote(i);
		return;
	}
	if(!IsRetryVoter(ClientID))
		return;
	int Retry = 0;
	int Reset = 0;
	CountRetryVotes(&Retry, &Reset);
	CNetMsg_Sv_PveInvasionRetryVote Msg;
	Msg.m_Nonce = m_RetryVoteNonce;
	Msg.m_EndTick = m_RetryVoteEndTick;
	Msg.m_CurrentFloor = max(1, g_Config.m_SvMapGenLevel);
	Msg.m_RetryVotes = Retry;
	Msg.m_ResetVotes = Reset;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}


void CGameControllerInvasion::StartRetryVote()
{
	m_GameState = STATE_RETRY_VOTE;
	m_RoundOverTick = 0;
	GameServer()->m_World.m_Paused = true;
	m_RetryVoteNonce = max(1, Server()->Tick() + 1);
	m_RetryVoteEndTick = Server()->Tick() + Server()->TickSpeed() * 15;
	m_RetryVoteLastSyncTick = Server()->Tick() + Server()->TickSpeed();
	m_aRetryPlayerName[0] = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aRetryVotes[i] = -1;
	SendRetryVote();
}


void CGameControllerInvasion::OnRetryVote(int ClientID, int Nonce, int Choice)
{
	if(m_GameState != STATE_RETRY_VOTE || Nonce != m_RetryVoteNonce || Server()->Tick() >= m_RetryVoteEndTick ||
		!IsRetryVoter(ClientID) || Choice < PVE_INVASION_RETRY || Choice > PVE_INVASION_RESET || m_aRetryVotes[ClientID] != -1)
		return;
	m_aRetryVotes[ClientID] = Choice;
	if(Choice == PVE_INVASION_RETRY && !m_aRetryPlayerName[0])
		str_copy(m_aRetryPlayerName, Server()->ClientName(ClientID), sizeof(m_aRetryPlayerName));
	SendRetryVote();
	int Voted = 0;
	CountRetryVotes(0, 0, &Voted);
	if(RetryVoterCount() > 0 && Voted >= RetryVoterCount())
		FinishRetryVote();
}


void CGameControllerInvasion::TickRetryVote()
{
	if(Server()->Tick() >= m_RetryVoteEndTick)
	{
		FinishRetryVote();
		return;
	}
	int Voted = 0;
	CountRetryVotes(0, 0, &Voted);
	const int Voters = RetryVoterCount();
	if(Voters > 0 && Voted >= Voters)
	{
		FinishRetryVote();
		return;
	}
	if(Server()->Tick() >= m_RetryVoteLastSyncTick)
	{
		SendRetryVote();
		m_RetryVoteLastSyncTick = Server()->Tick() + Server()->TickSpeed();
	}
}


void CGameControllerInvasion::FinishRetryVote()
{
	if(m_GameState != STATE_RETRY_VOTE)
		return;
	int Retry = 0;
	int Reset = 0;
	CountRetryVotes(&Retry, &Reset);
	if(Retry >= Reset)
	{
		m_aRetryPlayerName[0] = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(IsRetryVoter(i) && m_aRetryVotes[i] == PVE_INVASION_RETRY)
			{
				str_copy(m_aRetryPlayerName, Server()->ClientName(i), sizeof(m_aRetryPlayerName));
				break;
			}
		StartRetryResult(PVE_INVASION_RETRY_RESULT_RETRY);
	}
	else
		StartRetryResult(PVE_INVASION_RETRY_RESULT_RESET);
}


void CGameControllerInvasion::SendRetryResult(int ClientID)
{
	if(m_GameState != STATE_RETRY_RESULT)
		return;
	if(ClientID < 0)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(IsRetryVoter(i))
				SendRetryResult(i);
		return;
	}
	if(!IsRetryVoter(ClientID))
		return;
	CNetMsg_Sv_PveInvasionRetryResult Msg;
	Msg.m_Result = m_RetryResult;
	Msg.m_EndTick = m_RetryResultEndTick;
	Msg.m_pPlayerName = m_RetryResult == PVE_INVASION_RETRY_RESULT_RETRY ? m_aRetryPlayerName : "";
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}


void CGameControllerInvasion::StartRetryResult(int Result)
{
	m_GameState = STATE_RETRY_RESULT;
	m_RoundOverTick = 0;
	m_RetryVoteNonce = 0;
	m_RetryResult = clamp(Result, (int)PVE_INVASION_RETRY_RESULT_RETRY, (int)PVE_INVASION_RETRY_RESULT_FINAL_FAILURE);
	m_RetryResultEndTick = Server()->Tick() + Server()->TickSpeed() * 3;
	m_RetryResultLastSyncTick = Server()->Tick() + Server()->TickSpeed();
	GameServer()->m_World.m_Paused = true;
	SendRetryResult();
}


void CGameControllerInvasion::TickRetryResult()
{
	if(Server()->Tick() >= m_RetryResultEndTick)
	{
		FinishRetryResult();
		return;
	}
	if(Server()->Tick() >= m_RetryResultLastSyncTick)
	{
		SendRetryResult();
		m_RetryResultLastSyncTick = Server()->Tick() + Server()->TickSpeed();
	}
}


void CGameControllerInvasion::FinishRetryResult()
{
	if(m_GameState != STATE_RETRY_RESULT)
		return;
	const int Result = m_RetryResult;
	m_GameState = STATE_FAIL;
	m_RetryResultEndTick = 0;
	if(Result == PVE_INVASION_RETRY_RESULT_RETRY)
	{
		// Six is a cross-map sentinel for the one final attempt. Reaching the
		// next floor resets it through the normal completion path.
		g_Config.m_SvInvFails = INV_FINAL_ATTEMPT;
		GameServer()->ReloadMap();
		return;
	}

	if(GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->ClearRun();
	g_Config.m_SvMapGenLevel = 1;
	// The next controller consumes this marker before checkpoint selection, so
	// a preferred deep checkpoint cannot override the team's reset decision.
	g_Config.m_SvInvFails = INV_FORCE_FLOOR_ONE;
	g_Config.m_SvMapGenSeed = rand() % 32767;
	RegenerateMapFromTemplate();
}


void CGameControllerInvasion::RegenerateMapFromTemplate()
{
	// A generated map must never be used as the next generation template.
	// Load the original modular source first; OnInit then produces generated.map
	// and the server performs the normal second map hand-off to clients.
	if(g_Config.m_SvMapGen && g_Config.m_SvInvMap[0] && str_comp(g_Config.m_SvInvMap, "generated") != 0)
	{
		str_copy(g_Config.m_SvMap, g_Config.m_SvInvMap, sizeof(g_Config.m_SvMap));
		Server()->m_MapGenerated = false;
	}
	GameServer()->ReloadMap();
}


void CGameControllerInvasion::RewardQuestGold()
{
	int Gold = 5 + g_Config.m_SvMapGenLevel/5;
	if (m_RunBuffActive)
		Gold += 3;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if (!pPlayer || pPlayer->m_IsBot)
			continue;
		pPlayer->IncreaseGold(Gold);
	}
}


void CGameControllerInvasion::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);

	if (!RequestAI)
	{
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnPlayerSpawn(pChr->GetPlayer()->GetCID());
		if (m_DefendLevel)
			pChr->m_Kits = max(pChr->m_Kits, 10);
		return;
	}
	
	{
		bool Found = false;
		
		if (m_EnemiesLeft > 0)
		{
			m_EnemiesLeft--;
			Found = true;
			
			int Level = 0;
			
			for (int i = 0; i < 9; i++)
				if (m_EnemiesLeft < 1-i*3 + g_Config.m_SvMapGenLevel/2)
					Level++;
			
			if (frandom() < 0.7f && Level > 2)
				Level = rand()%(Level-1);
			
						
			GameServer()->GetAISkin(&pChr->GetPlayer()->m_AISkin, false, 1+rand()%(max(1, 1+g_Config.m_SvMapGenLevel/4-m_QuestWaveType*3)), m_QuestWaveType);
			pChr->GetPlayer()->SetAISkin();
			pChr->m_IsBot = true;
		
			typedef CAI* (*AIFactory)(CGameContext*, CPlayer*, int);
			// Aligned with WaveTypes in questinfo.h
			static const AIFactory s_aAIFactories[] = {
			    nullptr,             // WAVE_NONE (0)
			    CreateAIalien1,      // WAVE_ALIENS (1)
			    CreateAIrobot1,      // WAVE_ROBOTS (2)
			    CreateAIpyro1,       // WAVE_SKELETONS (3)
			    CreateAIbunny1,      // WAVE_FURRIES (4)
			    CreateAIrobot2,      // WAVE_CYBORGS (5)
			};
			static const AIFactory s_aEliteFactories[] = {
			    nullptr,
			    CreateAIalien2,
			    CreateAIrobot2,
			    CreateAIpyro2,
			    CreateAIbunny2,
			    CreateAIrobot2,
			};
			static const int s_NumFactories = sizeof(s_aAIFactories) / sizeof(s_aAIFactories[0]);

			bool UseElite = m_EliteWave && frandom() < 0.45f;
			if (!UseElite && g_Config.m_SvMapGenLevel > 15 && frandom() < 0.15f)
				UseElite = frandom() < 0.45f;
			AIFactory Factory = 0;
			if (m_QuestWaveType >= 0 && m_QuestWaveType < s_NumFactories)
				Factory = UseElite ? s_aEliteFactories[m_QuestWaveType] : s_aAIFactories[m_QuestWaveType];

			if (Factory)
			    pChr->GetPlayer()->m_pAI = Factory(GameServer(), pChr->GetPlayer(), Level);
			else 
			    pChr->GetPlayer()->m_pAI = new CAIalien1(GameServer(), pChr->GetPlayer(), Level);

			pChr->GetPlayer()->m_IsBot = true;
			pChr->GetPlayer()->m_TeeInfos.m_IsBot = true;
			
			m_EnemyCount++;
			pChr->m_SkipPickups = 999;
			Trigger(false);
		}
		
		if (!Found)
		{
			pChr->GetPlayer()->m_pAI = new CAIalien1(GameServer(), pChr->GetPlayer(), g_Config.m_SvMapGenLevel);
			pChr->GetPlayer()->m_IsBot = true;
			pChr->GetPlayer()->m_TeeInfos.m_IsBot = true;
			pChr->GetPlayer()->m_ToBeKicked = true;
			Trigger(false);
			}
		}
	}

void CGameControllerInvasion::Trigger(bool IncreaseLevel)
{
	if (IncreaseLevel)
		m_TriggerLevel++;
	
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if (!pPlayer)
			continue;
		
		if (pPlayer->m_pAI)
			pPlayer->m_pAI->Trigger(m_TriggerLevel);
	}
}

void CGameControllerInvasion::SpawnNewWave(bool AddBots)
{
	int Level = g_Config.m_SvMapGenLevel;
	const int Players = max(1, CountPlayers(0));
	const int WaveCap = max(8, InvasionWaveCap(Level, Players) - m_WaveSizeNerf*3);

	if (m_ForcedWaveType > WAVE_NONE && m_ForcedWaveType < NUM_WAVES)
		m_QuestWaveType = m_ForcedWaveType;
	else
	{
		int WaveUnlocked = min(NUM_WAVES-1, max(2, Level/5+1));
		if (Level > 8 && frandom() < 0.2f)
			WaveUnlocked = min(NUM_WAVES-1, WaveUnlocked+1);
		m_QuestWaveType = rand()%WaveUnlocked+1;
	}
	if (m_LevelTheme == INVASION_THEME_ELITE_WAVE)
		m_QuestWaveType = WAVE_CYBORGS;
	else if (m_LevelTheme == INVASION_THEME_Z_SECTOR)
		m_QuestWaveType = WAVE_ALIENS;
	
	if (m_Quest == QUEST_SURVIVEWAVETIME || (m_Quest == QUEST_NONE && m_LevelTheme == INVASION_THEME_TIMED_SURVIVE) || (m_LevelTheme == INVASION_THEME_TRAP_RUN && m_QuestsCompleted >= 1))
	{
		int TimedSecs = 35 + min(20, Level/2);
		if (m_LevelTheme == INVASION_THEME_TRAP_RUN)
			TimedSecs = 30 + Level/3;
		m_QuestWaveEndTick = Server()->Tick() + Server()->TickSpeed() * TimedSecs;
		m_QuestWaveEnemiesLeft = 9999;
		m_QuestWaveSize = WaveCap;
		m_EnemiesLeft = m_QuestWaveEnemiesLeft;
	}
	else if (m_Quest == QUEST_SURVIVEWAVE || m_Quest == QUEST_DEFEND)
	{
		m_QuestWaveEndTick = 0;
		m_QuestWaveEnemiesLeft = min(int(8+Level*2), 50)*(1.0f + (Players-1)*0.2f);
		if(m_LevelTheme == INVASION_THEME_Z_SECTOR)
			m_QuestWaveEnemiesLeft = (int)(m_QuestWaveEnemiesLeft * 1.25f + 0.5f);
		if(GameServer()->m_pPveDirector)
			m_QuestWaveEnemiesLeft = (int)(m_QuestWaveEnemiesLeft * GameServer()->m_pPveDirector->EnemyCountMultiplier() + 0.5f);
		m_QuestWaveSize = WaveCap;
		m_EnemiesLeft = m_QuestWaveEnemiesLeft;
	}
	else
	{
		m_QuestWaveEndTick = 0;
		m_QuestWaveEnemiesLeft = 0;
		m_QuestWaveSize = WaveCap;
		m_EnemiesLeft = InvasionOpeningEnemies(Level);
	}
	
	m_EnemyCount = 0;
	
	if (AddBots)
	{
		RandomGroupSpawnPos();
		const int ThreatDivisor = m_LevelTheme == INVASION_THEME_ELITE_WAVE ? 2 : 4;
		const SThreatBudgetResult ThreatReplacement = SpawnThreatBudgetSpecialists(&GameServer()->m_World,
			m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, Level, m_EnemiesLeft, m_QuestWaveSize, ThreatDivisor);
		m_EnemiesLeft -= ThreatReplacement.m_ThreatSpent;
		const int BotCap = max(0, m_QuestWaveSize - ThreatReplacement.m_EntitiesSpawned);
		
		for (int i = 0; i < m_EnemiesLeft && CountBots() < BotCap; i++)
			GameServer()->AddBot();
	}
	
	m_Deaths = m_QuestWaveSize;
}


void CGameControllerInvasion::DisplayExit(vec2 Pos)
{
	m_pDoor->Activate(Pos);	
}


void CGameControllerInvasion::SpawnBosses(int Count)
{
	for (int i = 0; i < Count; i++)
	{
		vec2 p;
		if (!GetBossSpawnPos(&p))
			p = vec2(4000, 4000);
		SpawnBoss(&GameServer()->m_World, p, g_Config.m_SvMapGenLevel);
	}
	m_BossesLeft = Count;
}


int CGameControllerInvasion::CountBossesAlive() const
{
	return CountAliveBosses(&GameServer()->m_World);
}

bool CGameControllerInvasion::IsObjectiveTarget(bool Boss) const
{
	if(RouteQuestActive())
		return true;
	if(m_Quest == QUEST_KILL_BOSS)
		return Boss;
	return m_Quest == QUEST_SURVIVEWAVE || m_Quest == QUEST_SURVIVEWAVETIME ||
		m_Quest == QUEST_KILLREMAININGENEMIES || m_Quest == QUEST_DEFEND;
}


int CGameControllerInvasion::CountBuildingsOfType(int Type) const
{
	CBuilding *apEnts[256];
	int Num = GameServer()->m_World.FindEntities(vec2(0, 0), 0.0f, (CEntity**)apEnts, 256, CGameWorld::ENTTYPE_BUILDING);
	int Count = 0;
	for (int i = 0; i < Num; i++)
	{
		if (apEnts[i] && apEnts[i]->m_Type == Type)
			Count++;
	}
	return Count;
}


int CGameControllerInvasion::ReactorsLeft()
{
	if(Server()->Tick() >= m_ReactorCountCheckTick)
	{
		m_CachedReactorsLeft = CountBuildingsOfType(BUILDING_REACTOR);
		m_ReactorCountCheckTick = Server()->Tick() + max(1, Server()->TickSpeed() / 10);
	}
	return m_CachedReactorsLeft;
}


int CGameControllerInvasion::SwitchesAvailable() const
{
	return CountBuildingsOfType(BUILDING_SWITCH);
}

void CGameControllerInvasion::ClearSwitchRadars()
{
	for(int i = 0; i < m_NumSwitchRadars; i++)
	{
		if(m_apSwitchRadar[i])
		{
			m_apSwitchRadar[i]->Deactivate();
			GameServer()->m_World.DestroyEntity(m_apSwitchRadar[i]);
			m_apSwitchRadar[i] = 0;
		}
	}
	m_NumSwitchRadars = 0;
}

bool CGameControllerInvasion::AnyCartographer() const
{
	if(!GameServer()->m_pPveDirector || !GameServer()->m_pPveDirector->Enabled())
		return false;
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->m_pPveDirector->PerkStacks(i, PVE_CARD_CARTOGRAPHER) > 0)
			return true;
	return false;
}

void CGameControllerInvasion::RefreshSwitchRadars()
{
	ClearSwitchRadars();
	const bool Show = m_Quest == QUEST_ACTIVATE_SWITCHES || m_Quest == QUEST_FIND_SWITCH || AnyCartographer();
	if(!Show)
		return;
	for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding; pBuilding = (CBuilding *)pBuilding->TypeNext())
	{
		if(pBuilding->m_Type != BUILDING_SWITCH || !pBuilding->m_PveSwitchActive || pBuilding->m_aStatus[BSTATUS_ON])
			continue;
		if(m_NumSwitchRadars >= 8)
			break;
		CRadar *pRadar = new CRadar(&GameServer()->m_World, RADAR_REACTOR);
		pRadar->Activate(pBuilding->m_Pos);
		m_apSwitchRadar[m_NumSwitchRadars++] = pRadar;
	}
}

void CGameControllerInvasion::SetSwitchesActive(bool Active)
{
	for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding; pBuilding = (CBuilding *)pBuilding->TypeNext())
		if(pBuilding->m_Type == BUILDING_SWITCH)
			pBuilding->SetPveSwitchActive(Active);
	if(Active)
		RefreshSwitchRadars();
	else
		ClearSwitchRadars();
}


bool CGameControllerInvasion::IsReactorDefenseActive() const
{
	return m_GameState == STATE_GAME && m_Quest == QUEST_DEFEND && !m_RoundOverTick;
}


void CGameControllerInvasion::SetReactorDefenseActive(bool Active)
{
	const int ReactorLife = min(1200, 600 + max(1, g_Config.m_SvMapGenLevel) * 20);
	bool RadarActivated = false;
	for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding; pBuilding = (CBuilding *)pBuilding->TypeNext())
	{
		if(pBuilding->m_Type != BUILDING_REACTOR)
			continue;
		pBuilding->SetPveReactorObjective(Active, ReactorLife);
		if(Active && !RadarActivated)
		{
			m_pReactor->Activate(pBuilding->m_Pos);
			RadarActivated = true;
		}
	}
	if(!Active || !RadarActivated)
		m_pReactor->Deactivate();
}


void CGameControllerInvasion::SpawnOperationOrdinaryEnemies(int Stage, int Count)
{
	const int Level = g_Config.m_SvMapGenLevel;
	const int Players = max(1, CountPlayers(0));
	const int WaveCap = max(8, InvasionWaveCap(Level, Players) - m_WaveSizeNerf * 3);
	const int NativeWaveTotal = (int)(min(8 + Level * 2, 50) * (1.0f + (Players - 1) * 0.2f));
	const int Existing = CountBots();

	// Keep a finite ordinary Invasion pack in play for the whole stage. Tick
	// tops the pack back up when the floor goes quiet, so destroy/carry steps
	// stay contested instead of empty travel between markers.
	m_QuestWaveType = (Stage & 1) ? WAVE_SKELETONS : WAVE_ROBOTS;
	m_ForcedWaveType = m_QuestWaveType;
	m_QuestWaveSize = WaveCap;
	m_QuestWaveEnemiesLeft = max(Count, NativeWaveTotal);
	m_EnemiesLeft = m_QuestWaveEnemiesLeft;
	m_OperationOrdinaryWaveActive = true;
	m_BotSpawnTick = Server()->Tick();
	const int InitialBots = min(m_EnemiesLeft, max(0, m_QuestWaveSize - Existing));
	if(InitialBots > 0)
		RandomGroupSpawnPos();
	dbg_msg("pve-operation", "foundry ordinary stage=%d wave=%d total=%d cap=%d existing=%d queued=%d", Stage + 1,
		m_QuestWaveType, m_QuestWaveEnemiesLeft, m_QuestWaveSize, Existing, InitialBots);
	for(int i = 0; i < InitialBots; i++)
		GameServer()->AddBot();
}


void CGameControllerInvasion::TickOperationOrdinaryEnemies()
{
	if(!m_OperationOrdinaryWaveActive)
		return;
	if(CountBots() >= m_QuestWaveSize)
		return;
	if(m_BotSpawnTick > Server()->Tick())
		return;
	// Route objectives can last longer than one finite wave. When the queue is
	// empty but the fight is still quiet, top up a small pack so players are
	// contested while moving between targets.
	if(m_EnemiesLeft <= 0 && CountBotsAlive() < max(3, m_QuestWaveSize / 3))
		m_EnemiesLeft = max(4, m_QuestWaveSize / 2);
	if(m_EnemiesLeft <= 0)
		return;
	RandomGroupSpawnPos();
	GameServer()->AddBot();
	m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * max(0.45f, 0.85f - g_Config.m_SvMapGenLevel * 0.01f);
	if(m_EnemiesLeft > 0 && m_EnemiesLeft < 9000)
		m_EnemiesLeft--;
}


void CGameControllerInvasion::ClearOperationOrdinaryEnemies()
{
	m_OperationOrdinaryWaveActive = false;
	m_EnemiesLeft = 0;
	m_QuestWaveEnemiesLeft = 0;
	m_QuestWaveSize = 0;
	for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
		if(!pPlayer || !pPlayer->m_IsBot)
			continue;
		pPlayer->m_ToBeKicked = true;
	}
}


void CGameControllerInvasion::FinishOperationFloor()
{
	if(!m_pOperationDirector->Complete())
		return;

	m_RouteQuestActive = false;
	m_pOperationDirector->Clear();
	m_LevelQuestsLeft = 1;
	m_QuestsCompleted = 0;
	ChangeQuest(QUEST_REACHDOOR, 0.5f);
	TriggerEscape();
}


bool CGameControllerInvasion::RouteQuestActive() const
{
	return m_RouteQuestActive && m_pOperationDirector->Running();
}


void CGameControllerInvasion::TryStartRouteQuest()
{
	if(m_RouteQuestActive || m_EscapeLevel)
		return;
	const int Operation = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ActiveOperation() : -1;
	if(Operation < 0 || !PveOperationAvailableInMode(Operation, PVE_MODE_INVASION))
		return;
	m_RouteQuestActive = true;
	m_LevelQuestsLeft = 3;
	m_QuestsCompleted = 0;
	m_Quest = QUEST_ROUTE;
	m_NextQuest = QUEST_NONE;
	m_QuestChangeTick = 0;
	m_pOperationDirector->Start(Operation);
	SyncRouteQuestFromOperation();
	const CPveOperationDef *pDef = PveOperationDef(Operation);
	if(pDef)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s - %s", pDef->m_pName, pDef->m_apSteps[0]);
		GameServer()->SendBroadcast(aBuf, -1);
	}
}


void CGameControllerInvasion::SyncRouteQuestFromOperation()
{
	if(!m_RouteQuestActive)
		return;
	m_Quest = QUEST_ROUTE;
	m_QuestWaveType = m_pOperationDirector->Operation();
	m_QuestsCompleted = m_pOperationDirector->Stage();
	m_LevelQuestsLeft = 3;
	const int TargetType = m_pOperationDirector->TargetType();
	m_RouteTargetType = TargetType;
	if(TargetType == PVE_OPERATION_TARGET_OVERLOAD_TERMINAL || TargetType == PVE_OPERATION_TARGET_UPLOAD_POINT)
	{
		// Remaining hold time in whole seconds for the Quest HUD countdown.
		const int TickSpeed = max(1, Server()->TickSpeed());
		m_QuestProgressCounter = max(0, (m_pOperationDirector->Required() - m_pOperationDirector->Progress() + TickSpeed - 1) / TickSpeed);
	}
	else if(TargetType == PVE_OPERATION_TARGET_EVACUATION)
		m_QuestProgressCounter = 0;
	else if(TargetType == PVE_OPERATION_TARGET_BOSS)
		m_QuestProgressCounter = CountBossesAlive();
	else if(m_pOperationDirector->Stage() == 1 && m_pOperationDirector->Operation() == PVE_OPERATION_FIRE_CONTROL_PURGE)
		m_QuestProgressCounter = CountBotsAlive() + CountAliveSpecialists(&GameServer()->m_World);
	else
		m_QuestProgressCounter = max(0, m_pOperationDirector->Required() - m_pOperationDirector->Progress());
}


int CGameControllerInvasion::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
	if(!pVictim->m_IsBot && GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnPlayerDeath(pVictim->GetPlayer()->GetCID());

	if (pVictim->m_IsBot && !pVictim->GetPlayer()->m_ToBeKicked)
	{
		if(pKiller && !pKiller->m_IsBot && GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnEnemyKilled(pKiller->GetCID(), Weapon, pVictim->m_Pos, pVictim);
		if (m_EnemiesLeft <= 0 || m_EscapeSpawnActive || m_DefendLevel)
			pVictim->GetPlayer()->m_ToBeKicked = true;
		
		if (pKiller)
		{
			Trigger(true);
			
			if (frandom() < 0.013f)
				GameServer()->m_pController->DropWeapon(pVictim->m_Pos, vec2(frandom()*6.0-frandom()*6.0, 0-frandom()*14.0), GameServer()->NewWeapon(GetStaticWeapon(SW_UPGRADE)));
			else if (frandom() < 0.013f)
				GameServer()->m_pController->DropWeapon(pVictim->m_Pos, vec2(frandom()*6.0-frandom()*6.0, 0-frandom()*14.0), GameServer()->NewWeapon(GetStaticWeapon(SW_RESPAWNER)));
		}
	}
	
	if (g_Config.m_SvSurvivalMode && IsHumanCoopPlayer(pVictim->GetPlayer()))
	{
		const int CID = pVictim->GetPlayer()->GetCID();
		// Dead until ally uses Respawn device or next floor; wipe when nobody left to revive.
		if (CountHumansAlive(CID) <= 0)
		{
			DeathMessage();
			if(GameServer()->m_pPveDirector)
				GameServer()->m_pPveDirector->CompleteContract(false);
			m_RoundOverTick = Server()->Tick();
			if(m_Quest == QUEST_DEFEND)
			{
				SetReactorDefenseActive(false);
				m_DefendPrepEndTick = 0;
			}
		}
	}

	return 0;
}



void CGameControllerInvasion::NextLevel(int CID)
{
	// Route evacuate steps open the door themselves. Entering finishes the chain
	// instead of abandoning a side objective.
	if(m_RouteQuestActive && m_pOperationDirector->Running())
	{
		m_pOperationDirector->OnEvent(CPveOperationDirector::EVENT_EVACUATE);
		if(m_pOperationDirector->Complete())
		{
			m_RouteQuestActive = false;
			m_pOperationDirector->Clear();
		}
		else
		{
			// Door should stay closed until the evacuate step; refuse early leave.
			return;
		}
	}
	if (!m_RoundWin)
	{
		m_RoundWin = true;
		m_RoundWinTick = Server()->Tick() + Server()->TickSpeed() * 2;
		
		if (CountHumans() > 1 && CID >= 0 && CID < MAX_CLIENTS)
			GameServer()->SendBroadcastFormat(-1, false, "%s reached the door", Server()->ClientName(CID));
	}
	
	
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? GameServer()->m_apPlayers[CID] : 0;
	if(pPlayer && pPlayer->GetCharacter() && !pPlayer->GetCharacter()->IgnoreCollision())
		pPlayer->GetCharacter()->Warp();
}


	
void CGameControllerInvasion::ChangeQuest(int NextQuest, float QueueTimeInSeconds)
{
	if (m_NextQuest == NextQuest)
		return;
	
	m_NextQuest = NextQuest;
	m_QuestChangeTick = Server()->Tick() + Server()->TickSpeed() * QueueTimeInSeconds;
}


void CGameControllerInvasion::SendQuestStartMessage(int Quest)
{
	if (m_EscapeLevel && Quest == QUEST_REACHDOOR)
		GameServer()->SendBroadcast("Rising acid! Reach the exit", -1);
	else if (m_DefendLevel && Quest == QUEST_DEFEND)
		GameServer()->SendBroadcast("Reach the reactor — defense starts in 10s", -1);
	else
		GameServer()->SendBroadcast(GetQuestStartMessage(Quest, m_QuestWaveType), -1);
}


void CGameControllerInvasion::SendQuestCompletedMessage(int Quest)
{
	GameServer()->SendBroadcast(GetQuestCompletedMessage(Quest, m_QuestWaveType), -1);
}


void CGameControllerInvasion::CompleteCurrentQuest()
{
	if(!RouteQuestActive())
		m_pOperationDirector->OnEvent(CPveOperationDirector::EVENT_WAVE);
	if(m_Quest == QUEST_DEFEND)
		SetReactorDefenseActive(false);
	m_DefendPrepEndTick = 0;
	if(m_Quest == QUEST_ACTIVATE_SWITCHES || m_Quest == QUEST_FIND_SWITCH)
		SetSwitchesActive(false);
	SendQuestCompletedMessage(m_Quest);
	RewardQuestGold();
	m_Quest = QUEST_NONE;
	m_NextQuest = QUEST_NONE;
	m_QuestsCompleted++;
	if(GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnObjectiveComplete();
	m_ForcedWaveType = WAVE_NONE;
}


void CGameControllerInvasion::StartThemeQuest()
{
	ChangeQuest(QUEST_KILLREMAININGENEMIES, INV_QUEST_QUEUE_TIME);
}


void CGameControllerInvasion::QueueNextObjectiveQuest()
{
	const int Done = m_QuestsCompleted;
	const int LastSlot = max(1, m_LevelQuestsLeft - 1);
	int Next = QUEST_SURVIVEWAVE;

	switch (m_LevelTheme)
	{
	case INVASION_THEME_BOSS_ASSAULT:
		if (Done >= LastSlot)
			Next = QUEST_KILL_BOSS;
		else
			Next = QUEST_SURVIVEWAVE;
		break;
	case INVASION_THEME_PURGE:
		if (Done >= LastSlot)
		{
			Next = QUEST_KILLREMAININGENEMIES;
			m_EnemiesLeft = min(16, 8 + g_Config.m_SvMapGenLevel);
			m_QuestWaveSize = InvasionWaveCap(g_Config.m_SvMapGenLevel, max(1, CountPlayers(0)));
			RandomGroupSpawnPos();
			for (int i = 0; i < m_EnemiesLeft && CountBots() < m_QuestWaveSize; i++)
				GameServer()->AddBot();
		}
		else
			Next = QUEST_SURVIVEWAVE;
		break;
	case INVASION_THEME_STANDARD_WAVE:
		m_ForcedWaveType = 1 + (g_Config.m_SvMapGenLevel/7) % (NUM_WAVES-1);
		Next = QUEST_SURVIVEWAVE;
		break;
	case INVASION_THEME_DUAL_SWITCHES:
		if (Done >= LastSlot)
		{
			const int Switches = SwitchesAvailable();
			if (Switches > 0)
			{
				m_SwitchesRequired = min(2, Switches);
				m_SwitchCoopLevel = true;
				Next = QUEST_ACTIVATE_SWITCHES;
			}
			else
			{
				dbg_msg("inv", "theme dual-switch: no switches on map, skip switch quest");
				GameServer()->SendBroadcast("Switches missing — survive the wave instead", -1);
				m_SwitchCoopLevel = false;
				m_SwitchesRequired = 0;
				Next = QUEST_SURVIVEWAVE;
			}
		}
		else
			Next = QUEST_SURVIVEWAVE;
		break;
	case INVASION_THEME_REACTOR_DEFEND:
		if (Done >= LastSlot)
		{
			if (ReactorsLeft() > 0)
				Next = QUEST_DEFEND;
			else
			{
				dbg_msg("inv", "theme reactor-defend: no reactor on map, skip defend quest");
				GameServer()->SendBroadcast("Reactor missing — survive the wave instead", -1);
				Next = QUEST_SURVIVEWAVE;
			}
		}
		else
			Next = QUEST_SURVIVEWAVE;
		break;
	case INVASION_THEME_TIMED_SURVIVE:
		Next = QUEST_SURVIVEWAVETIME;
		break;
	case INVASION_THEME_TRAP_RUN:
		Next = QUEST_SURVIVEWAVETIME;
		break;
	case INVASION_THEME_ELITE_WAVE:
		Next = QUEST_SURVIVEWAVE;
		break;
	case INVASION_THEME_Z_SECTOR:
		Next = QUEST_SURVIVEWAVE;
		break;
	default:
		Next = QUEST_SURVIVEWAVE;
		break;
	}

	ChangeQuest(Next, INV_QUEST_QUEUE_TIME);
}

void CGameControllerInvasion::SpawnEliteContractGuard()
{
	if(!GameServer()->m_pPveDirector || GameServer()->m_pPveDirector->ActiveContract() != PVE_CONTRACT_ELITE_GUARD || m_Quest == QUEST_NONE || m_Quest == QUEST_REACHDOOR)
		return;
	vec2 Pos;
	if(!GetBossSpawnPos(&Pos))
		Pos = vec2(4000, 4000);
	CDroid *pGuard = SpawnBoss(&GameServer()->m_World, Pos, g_Config.m_SvMapGenLevel + 1);
	GameServer()->m_pPveDirector->RegisterEliteContractBoss(pGuard);
}


void CGameControllerInvasion::OnSwitchTriggered()
{
	if(!RouteQuestActive())
		m_pOperationDirector->OnEvent(CPveOperationDirector::EVENT_SWITCH);
	if(m_Quest != QUEST_ACTIVATE_SWITCHES && m_Quest != QUEST_FIND_SWITCH)
		return;
	m_SwitchesActivated++;
	if(GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnSwitchTriggered();

	if (m_SwitchCoopLevel)
	{
		m_QuestProgressCounter = max(0, m_SwitchesRequired - m_SwitchesActivated);
		RefreshSwitchRadars();
		if (m_SwitchesActivated < m_SwitchesRequired)
		{
			GameServer()->SendBroadcastFormat(-1, false, "Switch %d/%d activated", m_SwitchesActivated, m_SwitchesRequired);
			return;
		}

		if (m_Quest == QUEST_ACTIVATE_SWITCHES || m_NextQuest == QUEST_ACTIVATE_SWITCHES)
		{
			m_QuestChangeTick = 0;
			m_NextQuest = QUEST_NONE;
			if (m_Quest == QUEST_ACTIVATE_SWITCHES)
				CompleteCurrentQuest();
			else
				m_Quest = QUEST_NONE;
		}
		// Door opens only when REACHDOOR starts after remaining objectives.
		return;
	}

	if (m_EscapeLevel)
	{
		BeginRisingAcid(50);
		m_EscapeSpawnActive = true;
		m_EnemiesLeft = 9999;
		m_QuestWaveSize = min(8 + g_Config.m_SvMapGenLevel/2 + CountPlayers(0), 28);
		m_BotSpawnTick = Server()->Tick();

		if (m_Quest == QUEST_FIND_SWITCH || m_NextQuest == QUEST_FIND_SWITCH)
		{
			m_QuestChangeTick = 0;
			m_NextQuest = QUEST_NONE;
			if (m_Quest == QUEST_FIND_SWITCH)
				CompleteCurrentQuest();
			else
				m_Quest = QUEST_NONE;
			ChangeQuest(QUEST_REACHDOOR, 0.5f);
		}
		else if (m_Quest != QUEST_REACHDOOR && m_NextQuest != QUEST_REACHDOOR)
			ChangeQuest(QUEST_REACHDOOR, 0.5f);

		TriggerEscape();
		return;
	}

	// Default: open door
	TriggerEscape();
}


void CGameControllerInvasion::Tick()
{
	IGameController::Tick();
	const bool OperationIntermission = GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->InIntermission();
	const int ActiveOperation = !OperationIntermission && GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->ActiveOperation() : -1;
	if(!OperationIntermission && ActiveOperation < 0 && m_pOperationDirector->Operation() >= 0)
		m_pOperationDirector->Clear();
	if(m_RouteQuestActive || (ActiveOperation >= 0 && PveOperationAvailableInMode(ActiveOperation, PVE_MODE_INVASION) && !m_EscapeLevel))
	{
		if(!m_RouteQuestActive)
			TryStartRouteQuest();
		if(m_RouteQuestActive && !m_pOperationDirector->Running() && !m_pOperationDirector->Complete())
		{
			m_RouteQuestActive = false;
			m_Quest = QUEST_NONE;
		}
		if(!OperationIntermission)
			m_pOperationDirector->Tick();
		if(m_pOperationDirector->Complete())
			FinishOperationFloor();
		if(RouteQuestActive())
			SyncRouteQuestFromOperation();
	}
	else if(!OperationIntermission)
	{
		if(ActiveOperation >= 0 && m_pOperationDirector->Operation() != ActiveOperation)
			m_pOperationDirector->Start(ActiveOperation);
		else if(ActiveOperation < 0 && m_pOperationDirector->Operation() >= 0)
			m_pOperationDirector->Clear();
		m_pOperationDirector->Tick();
		if(m_pOperationDirector->Complete())
			FinishOperationFloor();
	}
	if(m_GameState == STATE_RETRY_VOTE)
	{
		TickRetryVote();
		return;
	}
	if(m_GameState == STATE_RETRY_RESULT)
	{
		TickRetryResult();
		return;
	}
	if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->InIntermission())
		return;
	
	if (m_GameState == STATE_FAIL)
		return;
	
	if (m_GameState == STATE_GAME)
	{
		if(m_EliteContractSpawned && GameServer()->m_pPveDirector && CountBossesAlive() <= 0)
		{
			m_EliteContractSpawned = false;
			GameServer()->m_pPveDirector->OnBossKilled();
		}
		// Wipe only after someone has already died this round (SURVIVAL_NOCANDO).
		// At join/spawn, humans exist but aren't alive yet — don't end the round.
		if (g_Config.m_SvSurvivalMode && !m_RoundOverTick
			&& m_SurvivalStatus == SURVIVAL_NOCANDO
			&& CountHumans() > 0 && CountHumansAlive() <= 0)
		{
			DeathMessage();
			if(GameServer()->m_pPveDirector)
				GameServer()->m_pPveDirector->CompleteContract(false);
			m_RoundOverTick = Server()->Tick();
			if(m_Quest == QUEST_DEFEND)
			{
				SetReactorDefenseActive(false);
				m_DefendPrepEndTick = 0;
			}
		}

		if (m_QuestChangeTick && m_QuestChangeTick <= Server()->Tick())
		{
			m_Quest = m_NextQuest;
			m_NextQuest = QUEST_NONE;
			m_QuestChangeTick = 0;
			m_QuestProgressCounter = 0;
			if(m_Quest == QUEST_DEFEND)
				SetReactorDefenseActive(true);
			SpawnEliteContractGuard();
			if(m_Quest == QUEST_ACTIVATE_SWITCHES || m_Quest == QUEST_FIND_SWITCH)
			{
				m_SwitchesActivated = 0;
				SetSwitchesActive(true);
			}
			
			if (m_Quest == QUEST_REACHDOOR && !m_EscapeLevel && !m_SwitchCoopLevel)
				TriggerEscape();
			else if (m_Quest == QUEST_REACHDOOR && m_SwitchCoopLevel)
			{
				// Open if switches done, or map had no usable switches (avoid softlock).
				if (m_SwitchesActivated >= m_SwitchesRequired || SwitchesAvailable() <= 0 || m_SwitchesRequired <= 0)
					TriggerEscape();
			}
			
			if (m_Quest == QUEST_SURVIVEWAVE || m_Quest == QUEST_SURVIVEWAVETIME)
				SpawnNewWave();
			else if (m_Quest == QUEST_DEFEND && ReactorsLeft() > 0)
			{
				// Prep window: radar on, no waves until players reach the reactor.
				m_DefendPrepEndTick = Server()->Tick() + Server()->TickSpeed() * 10;
				m_DefendEndTick = 0;
			}

			if (m_Quest == QUEST_KILL_BOSS)
			{
				SpawnBosses(max(1, m_BossesLeft));
				m_EnemiesLeft = min(16, 6 + g_Config.m_SvMapGenLevel/3);
				m_QuestWaveSize = min(20, 10 + g_Config.m_SvMapGenLevel/4);
				RandomGroupSpawnPos();
				for (int i = 0; i < m_EnemiesLeft && CountBots() < m_QuestWaveSize; i++)
					GameServer()->AddBot();
			}

			if (m_Quest == QUEST_DEFEND)
			{
				if (!ReactorsLeft())
				{
					dbg_msg("inv", "defend started with no reactor, auto-complete");
					m_DefendPrepEndTick = 0;
					CompleteCurrentQuest();
				}
			}

			if (m_Quest == QUEST_ACTIVATE_SWITCHES)
			{
				const int Switches = SwitchesAvailable();
				if (Switches <= 0)
				{
					dbg_msg("inv", "switch quest started with no switches, auto-complete");
					m_SwitchCoopLevel = false;
					m_SwitchesRequired = 0;
					CompleteCurrentQuest();
				}
				else
				{
					m_SwitchesRequired = min(max(1, m_SwitchesRequired), Switches);
					m_QuestProgressCounter = max(0, m_SwitchesRequired - m_SwitchesActivated);
				}
			}

			if (m_Quest == QUEST_FIND_SWITCH && SwitchesAvailable() <= 0)
			{
				dbg_msg("inv", "escape switch missing, force acid climb");
				BeginRisingAcid(50);
				m_EscapeSpawnActive = true;
				CompleteCurrentQuest();
				ChangeQuest(QUEST_REACHDOOR, 0.5f);
				TriggerEscape();
			}
			
			if (m_Quest != QUEST_NONE)
				SendQuestStartMessage(m_Quest);
		}
		
		
		if(!RouteQuestActive())
		{
		if (m_Quest == QUEST_NONE && m_NextQuest == QUEST_NONE)
		{
			if (m_EscapeLevel)
			{
				if (!m_EscapeSpawnActive)
					ChangeQuest(QUEST_FIND_SWITCH, 2.0f);
				else
					ChangeQuest(QUEST_REACHDOOR, 1.0f);
			}
			else if (m_QuestsCompleted >= m_LevelQuestsLeft)
				ChangeQuest(QUEST_REACHDOOR, INV_QUEST_DOOR_TIME);
			else if (m_QuestsCompleted == 0)
				StartThemeQuest();
			else
				QueueNextObjectiveQuest();
		}
		
		if (m_Quest == QUEST_SURVIVEWAVE || m_Quest == QUEST_SURVIVEWAVETIME)
		{
			const int AliveBots = CountBotsAlive() + CountAliveSpecialists(&GameServer()->m_World);
			if (m_Quest == QUEST_SURVIVEWAVETIME)
				m_QuestProgressCounter = int((m_QuestWaveEndTick - Server()->Tick()) / Server()->TickSpeed());
			else
				m_QuestProgressCounter = m_EnemiesLeft + AliveBots;
			
			if ((m_QuestWaveEndTick && m_QuestWaveEndTick <= Server()->Tick()) || (m_EnemiesLeft <= 0 && AliveBots <= 0))
			{
				m_EnemiesLeft = 0;
				m_QuestWaveEndTick = 0;
				int CompletedQuest = m_Quest;
				CompleteCurrentQuest();
				
			if (CompletedQuest == QUEST_SURVIVEWAVETIME && AliveBots > 4)
				ChangeQuest(QUEST_KILLREMAININGENEMIES, INV_QUEST_QUEUE_TIME);
		}
	}
		
	if (m_Quest == QUEST_KILLREMAININGENEMIES)
	{
		// The old HUD counted only currently alive Bots, while the server could
		// still have enemies queued in m_EnemiesLeft. This let the counter show
		// zero before the purge encounter had reached a stable completion state.
		// Use one value for both rendering and completion; Operation-owned droids
		// intentionally do not participate in this native Invasion objective.
		const int Remaining = max(0, m_EnemiesLeft) + CountBotsAlive();
		m_QuestProgressCounter = Remaining;
		if(Remaining == 0)
			CompleteCurrentQuest();
	}

		if (m_Quest == QUEST_KILL_BOSS)
		{
			m_BossesLeft = CountBossesAlive();
			m_QuestProgressCounter = m_BossesLeft;
			if (m_BossesLeft <= 0)
			{
				if(GameServer()->m_pPveDirector)
					GameServer()->m_pPveDirector->OnBossKilled();
				m_EnemiesLeft = 0;
				CompleteCurrentQuest();
			}
		}

		if (m_Quest == QUEST_DEFEND)
		{
			if (m_DefendPrepEndTick)
			{
				m_QuestProgressCounter = max(0, (m_DefendPrepEndTick - Server()->Tick()) / Server()->TickSpeed());
				if (m_DefendPrepEndTick <= Server()->Tick())
				{
					m_DefendPrepEndTick = 0;
					m_DefendEndTick = Server()->Tick() + Server()->TickSpeed() * (40 + g_Config.m_SvMapGenLevel);
					SpawnNewWave();
					GameServer()->SendBroadcast("Defend the reactor", -1);
				}
			}
			else
				m_QuestProgressCounter = max(0, (m_DefendEndTick - Server()->Tick()) / Server()->TickSpeed());

			if (!ReactorsLeft())
			{
				GameServer()->SendBroadcast("Reactor destroyed", -1);
				DeathMessage();
				m_RoundOverTick = Server()->Tick();
				SetReactorDefenseActive(false);
				m_DefendPrepEndTick = 0;
				m_Quest = QUEST_NONE;
			}
			else if (!m_DefendPrepEndTick && m_DefendEndTick && m_DefendEndTick <= Server()->Tick())
			{
				m_EnemiesLeft = 0;
				CompleteCurrentQuest();
			}
			else if (!m_DefendPrepEndTick && m_BotSpawnTick < Server()->Tick())
			{
				m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * max(0.22f, 0.7f - g_Config.m_SvMapGenLevel*0.012f);
				if (CountBots() < m_QuestWaveSize)
				{
					RandomGroupSpawnPos();
					GameServer()->AddBot();
				}
			}
		}

		if (m_Quest == QUEST_ACTIVATE_SWITCHES)
			m_QuestProgressCounter = max(0, m_SwitchesRequired - m_SwitchesActivated);
		}

		if (m_Quest == QUEST_FIND_SWITCH)
			m_QuestProgressCounter = max(0, 1 - m_SwitchesActivated);

		// After the switch: keep refreshing enemies until players reach the door.
		if (m_EscapeSpawnActive && m_Quest == QUEST_REACHDOOR && !m_RoundWin)
		{
			if (m_BotSpawnTick < Server()->Tick())
			{
				m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * max(0.35f, 1.1f - g_Config.m_SvMapGenLevel*0.015f);
				if (CountBots() < m_QuestWaveSize)
				{
					RandomGroupSpawnPos();
					GameServer()->AddBot();
					if (m_EnemiesLeft > 0 && m_EnemiesLeft < 9000)
						m_EnemiesLeft--;
				}
			}
		}
		}

	if (m_GameState == STATE_STARTING)
	{
		if (CountPlayers(0) > 0)
		{
			if(!m_RogueliteWaitTick)
				m_RogueliteWaitTick = Server()->Tick() + Server()->TickSpeed() * 2;
			if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->Enabled() &&
				!GameServer()->m_pPveDirector->ProgressReady() && Server()->Tick() < m_RogueliteWaitTick)
				return;
			if(!m_CheckpointApplied)
			{
				m_CheckpointApplied = true;
				if(GameServer()->m_pPveDirector && g_Config.m_SvMapGenLevel == 1)
				{
					if(m_ForceFloorOne)
						m_ForceFloorOne = false;
					else
					{
						const int Checkpoint = GameServer()->m_pPveDirector->TeamCheckpoint();
						if(Checkpoint > 1)
						{
							g_Config.m_SvMapGenLevel = Checkpoint;
							RegenerateMapFromTemplate();
							return;
						}
					}
				}
			}
			if(!m_RogueliteOpeningStarted)
			{
				m_RogueliteOpeningStarted = true;
				if(GameServer()->m_pPveDirector)
				{
					// Floor 1 remains a normal Invasion onboarding floor. Operations
					// start on Floor 2+. Acid-escape floors never vote: rising acid
					// and a parallel route would softlock the run.
					// At most one vote per floor: Contract on Level%3==0, else Operation.
					const bool AcidEscape = InvasionThemeFromLevel(g_Config.m_SvMapGenLevel) == INVASION_THEME_ACID_ESCAPE;
					const bool ContractVote = g_Config.m_SvMapGenLevel % 3 == 0;
					const bool OperationVote = !ContractVote && g_Config.m_SvMapGenLevel > 1 && !AcidEscape;
					GameServer()->m_pPveDirector->StartIntermission(ContractVote, true, OperationVote);
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
			if (!m_ProgressSynced)
			{
				SetupLevelTheme();
				m_ProgressSynced = true;
			}

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "start round theme=%d enemies='%u'", m_LevelTheme, m_Deaths);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "inv", aBuf);

			if (!m_StartBriefingSent)
			{
				m_StartBriefingSent = true;
				if(g_Config.m_SvInvFails > 0 && g_Config.m_SvInvFails < 5 && g_Config.m_SvInvFails != INV_FORCE_FLOOR_ONE)
					GameServer()->SendBroadcastFormat(-1, false, "Level %d - %s · Attempt %d/5",
						g_Config.m_SvMapGenLevel, GetThemeDisplayName(m_LevelTheme), g_Config.m_SvInvFails + 1);
				else
					GameServer()->SendBroadcastFormat(-1, false, "Level %d - %s", g_Config.m_SvMapGenLevel, GetThemeDisplayName(m_LevelTheme));
			}

			m_TriggerTick = 0;
			m_AutoRestart = true;

			m_GameState = STATE_GAME;
			if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_ELITE_HUNT)
			{
				vec2 Pos;
				if(!GetBossSpawnPos(&Pos))
					Pos = vec2(4000, 4000);
				const int BonusLevel = GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_ELITE_HUNT ? 2 : 1;
				CDroid *pBoss = SpawnBoss(&GameServer()->m_World, Pos, g_Config.m_SvMapGenLevel + BonusLevel);
				GameServer()->m_pPveDirector->RegisterEliteContractBoss(pBoss);
				m_EliteContractSpawned = true;
			}
			if(GameServer()->m_pPveDirector)
				m_EnemiesLeft = (int)(m_EnemiesLeft * GameServer()->m_pPveDirector->EnemyCountMultiplier() + 0.5f);
			for (int i = 0; i < m_EnemiesLeft && CountBots() < 32; i++)
				GameServer()->AddBot();
		}
		else if ((m_AutoRestart || g_Config.m_SvMapGenLevel > 1) && Server()->Tick() > Server()->TickSpeed()*60.0f)
		{
			m_AutoRestart = false;
			
			if (g_Config.m_SvMapGenRandSeed)
				g_Config.m_SvMapGenSeed = rand()%32767;
			
			FirstMap();
		}
	}
	else
	{
		if (g_Config.m_SvMapGenLevel > 1)
			m_AutoRestart = true;
			
		if (m_RoundOverTick && m_RoundOverTick < Server()->Tick() - Server()->TickSpeed()*2.0f)
		{
			m_RoundOverTick = 0;
			if(g_Config.m_SvInvFails == INV_FINAL_ATTEMPT)
				StartRetryResult(PVE_INVASION_RETRY_RESULT_FINAL_FAILURE);
			else if(++g_Config.m_SvInvFails >= 5)
			{
				g_Config.m_SvInvFails = 5;
				StartRetryVote();
			}
			else
			{
				GameServer()->SendBroadcastFormat(-1, false, "Failure %d/5 — retrying this floor", g_Config.m_SvInvFails);
				GameServer()->ReloadMap();
			}
		}
	}
	
	GameServer()->UpdateAI();
	
	if (m_TriggerTick < Server()->Tick())
	{
		Trigger(true);
		m_TriggerTick = Server()->Tick() + Server()->TickSpeed()*4;
	}
	
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer)
			continue;
			
		if (pPlayer->m_IsBot && pPlayer->m_ToBeKicked)
			GameServer()->KickBot(pPlayer->GetCID());
	}
	
	if (m_RoundWin)
	{
		if (m_RoundWinTick < Server()->Tick())
		{
			const int CompletedLevel = g_Config.m_SvMapGenLevel;
			if(!m_RogueliteCompletionStarted)
			{
				m_RogueliteCompletionStarted = true;
				if(GameServer()->m_pPveDirector)
				{
					GameServer()->m_pPveDirector->OnStageComplete(true);
					GameServer()->m_pPveDirector->RewardResearch(CompletedLevel % 2 == 0 ? 1 : 0, PVE_REWARD_INVASION_DEPTH, CompletedLevel);
				}

				for (int i = 0; i < MAX_CLIENTS; i++)
				{
					CPlayer *pPlayer = GameServer()->m_apPlayers[i];
					if (!pPlayer || pPlayer->m_IsBot)
						continue;
					pPlayer->IncreaseGold(10 + CompletedLevel/3);
				}

				// The next floor offers its perk after the new map and client state
				// are ready, avoiding a selection crossing the map-load boundary.
			}

			m_RoundWin = false;
			m_RoundWinTick = 0;
			m_RunBuffActive = false;
			g_Config.m_SvMapGenLevel++;
			g_Config.m_SvInvFails = 0;

			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if(pPlayer)
					pPlayer->SaveData();
			}
			
			EndRound();
		}
	}
}


void CGameControllerInvasion::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	pGameDataObj->m_TeamscoreRed = m_Quest;
	pGameDataObj->m_TeamscoreBlue = m_QuestProgressCounter;
	// Coop HUD packs level/theme/wave/quest progress (flag carriers unused in coop).
	// Bits 16..23 carry the active route target type so the Quest HUD can show
	// timed-hold countdowns without the Operation overlay.
	pGameDataObj->m_FlagCarrierRed = g_Config.m_SvMapGenLevel;
	pGameDataObj->m_FlagCarrierBlue = m_LevelTheme | (m_QuestWaveType << 4) | (m_QuestsCompleted << 8) | (m_LevelQuestsLeft << 12) |
		((m_Quest == QUEST_ROUTE ? m_RouteTargetType : 0) << 16);
}
