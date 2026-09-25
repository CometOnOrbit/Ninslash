#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/platform_events.h>

#include <game/mapitems.h>
#include <base/deterministic_random.h>
#include <game/pve/invasion_rules.h>
#include <game/pve/questinfo.h>
#include <game/pve/pve_environment.h>
#include <game/pve/pve_roguelite.h>
#include <game/weapons/weapons.h>

#include <game/server/entities/character.h>
#include <game/server/entities/building.h>
#include <game/server/entities/droid.h>
#include <game/server/bosspool.h>
#include <game/server/entities/radar.h>
#include <game/server/entities/turret.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>
#include <game/server/pve_director.h>
#include <game/server/tutorial_director.h>

#include "invasion.h"

#include <game/server/playerdata.h>
#include <game/server/ai.h>
#include <game/server/ai/inv/invasion_ai.h>
#include <game/server/ai/inv/invasion_profile.h>

static const float INV_QUEST_QUEUE_TIME = 1.5f;
static const float INV_QUEST_DOOR_TIME = 3.0f;
static const int INV_FINAL_ATTEMPT = 6;
static const int INV_FORCE_FLOOR_ONE = 7;
static const int INV_REACTOR_DEFEND_MIN_SECONDS = 10;
static const int INV_REACTOR_DEFEND_MAX_SECONDS = 60;

enum EInvasionMapTemplate
{
	INV_MAP_UNKNOWN = -1,
	INV_MAP_CITY1,
	INV_MAP_CITY2,
	INV_MAP_BLUEPLANET,
	INV_MAP_LARGE1,
	INV_MAP_LARGE2,
	INV_MAP_LARGE3,
	INV_MAP_SPACE,
};

static int InvasionMapTemplateForName(const char *pMap)
{
	if(!pMap)
		return INV_MAP_UNKNOWN;
	if(str_comp(pMap, "generate_city1") == 0)
		return INV_MAP_CITY1;
	if(str_comp(pMap, "generate_city2") == 0)
		return INV_MAP_CITY2;
	if(str_comp(pMap, "generate_blueplanet1") == 0)
		return INV_MAP_BLUEPLANET;
	if(str_comp(pMap, "generate_large1") == 0)
		return INV_MAP_LARGE1;
	if(str_comp(pMap, "generate_large2") == 0)
		return INV_MAP_LARGE2;
	if(str_comp(pMap, "generate_large3") == 0)
		return INV_MAP_LARGE3;
	if(str_comp(pMap, "generate_space1") == 0)
		return INV_MAP_SPACE;
	return INV_MAP_UNKNOWN;
}

static int InvasionBiomeForMapTemplate(int MapTemplate)
{
	if(MapTemplate == INV_MAP_BLUEPLANET)
		return PVE_BIOME_BLUE_PLANET;
	return PVE_BIOME_NONE;
}

static int InvasionRegionalBossType(int MapTemplate)
{
	switch(MapTemplate)
	{
		case INV_MAP_CITY1:
		case INV_MAP_LARGE3:
			return DROIDTYPE_BOSSCRAWLER;
		case INV_MAP_BLUEPLANET:
		case INV_MAP_LARGE1:
			return DROIDTYPE_BOSSSPLITTER;
		case INV_MAP_CITY2:
		case INV_MAP_LARGE2:
		case INV_MAP_SPACE:
			return DROIDTYPE_BOSSSTAR;
		default:
			return DROIDTYPE_BOSSCRAWLER;
	}
}

static int InvasionRegionalSupportType(int MapTemplate, int Phase)
{
	switch(MapTemplate)
	{
		case INV_MAP_CITY1: return DROIDTYPE_SIEGEBREAKERCRAWLER;
		case INV_MAP_CITY2: return DROIDTYPE_STALKERCRAWLER;
		case INV_MAP_BLUEPLANET: return DROIDTYPE_TEMPESTSTAR;
		case INV_MAP_LARGE1: return Phase == 1 ? DROIDTYPE_RAILSTAR : DROIDTYPE_SIEGEBREAKERCRAWLER;
		case INV_MAP_LARGE2: return Phase == 1 ? DROIDTYPE_TEMPESTSTAR : DROIDTYPE_CYCLONECRAWLER;
		case INV_MAP_LARGE3: return Phase == 1 ? DROIDTYPE_TESLASTAR : DROIDTYPE_RAILSTAR;
		case INV_MAP_SPACE: return Phase == 1 ? DROIDTYPE_TEMPESTSTAR : DROIDTYPE_RAILSTAR;
		default: return DROIDTYPE_SIEGEBREAKERCRAWLER;
	}
}

static const char *InvasionMapTemplateName(int MapTemplate)
{
	switch(MapTemplate)
	{
		case INV_MAP_CITY1: return "City I";
		case INV_MAP_CITY2: return "City II";
		case INV_MAP_BLUEPLANET: return "Blueplanet";
		case INV_MAP_LARGE1: return "Large I";
		case INV_MAP_LARGE2: return "Large II";
		case INV_MAP_LARGE3: return "Large III";
		case INV_MAP_SPACE: return "Space";
		default: return "Invasion";
	}
}

static constexpr int InvasionReactorDefenseSeconds(int Level)
{
	return Level < INV_REACTOR_DEFEND_MIN_SECONDS
			   ? INV_REACTOR_DEFEND_MIN_SECONDS
			   : (Level > INV_REACTOR_DEFEND_MAX_SECONDS ? INV_REACTOR_DEFEND_MAX_SECONDS : Level);
}

static_assert(InvasionReactorDefenseSeconds(0) == 10, "reactor defense minimum duration changed");
static_assert(InvasionReactorDefenseSeconds(4) == 10, "reactor defense first-floor duration changed");
static_assert(InvasionReactorDefenseSeconds(30) == 30, "reactor defense scaling changed");
static_assert(InvasionReactorDefenseSeconds(60) == 60, "reactor defense maximum duration changed");
static_assert(InvasionReactorDefenseSeconds(61) == 60, "reactor defense duration must stay capped");

static int InvasionDepthQuests(int Level)
{
	if(Level >= 21)
		return min(2 + Level / 12, 3);
	return 2;
}

static float InvasionCountScale(CGameContext *pGameServer)
{
	if(pGameServer && pGameServer->m_pPveDirector)
		return pGameServer->m_pPveDirector->EnemyCountMultiplier();
	return 1.0f;
}

CGameControllerInvasion::CGameControllerInvasion(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "Invasion";
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
	m_MapTemplate = InvasionMapTemplateForName(Server()->m_aMapInUse);
	if(m_MapTemplate == INV_MAP_UNKNOWN)
		m_MapTemplate = InvasionMapTemplateForName(g_Config.m_SvMap);
	m_MapBiome = InvasionBiomeForMapTemplate(m_MapTemplate);
	g_Config.m_SvPveBiome = m_MapBiome;

	if(g_Config.m_SvMapGenRandSeed)
	{
		g_Config.m_SvMapGenSeed = (int)((unsigned long long)time_get() % 0x7FFFFFFFull);
		if(g_Config.m_SvMapGenSeed <= 0)
			g_Config.m_SvMapGenSeed = 1;
		g_Config.m_SvMapGenRandSeed = 0;
	}

	for(int i = 0; i < MAX_ENEMIES; i++)
		m_aEnemySpawnPos[i] = vec2(0, 0);
	for(int i = 0; i < INV_MAX_PUSH_POINTS; i++)
	{
		m_aPushPoints[i] = vec2(0, 0);
		m_aRegionalBossPoints[i] = vec2(0, 0);
	}

	m_RoundOverTick = 0;
	m_RoundWinTick = 0;
	m_RoundWin = false;
	m_QuestsCompleted = 0;

	m_QuestWaveSize = 0;
	m_QuestWaveEndTick = 0;
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
	m_PushPointCount = 0;
	m_PushCompletedMask = 0;
	m_PushActiveMask = 0;
	m_PushPointEndTick = 0;
	m_PushForwardActive = false;
	m_PushParallel = false;
	m_RegionalBossPointCount = 0;
	m_pRegionalBoss = 0;
	m_RegionalBossPhase = -1;
	m_RegionalBossSupportSpawned = 0;
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
	m_EscapeLevel = false;
	m_LevelTheme = 0;
	m_LevelQuestsLeft = 0;
	m_EnemiesLeft = 0;

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

	if(g_Config.m_SvEnableBuilding)
		m_GameFlags |= GAMEFLAG_BUILD;

	if(g_Config.m_SvSurvivalMode)
		m_GameFlags |= GAMEFLAG_SURVIVAL;

	m_GameFlags |= GAMEFLAG_ACID;

	for(int i = 0; i < MAX_CLIENTS; i++)
		new CServerRadar(&GameServer()->m_World, RADAR_HUMAN, i);

	m_pDoor = new CServerRadar(&GameServer()->m_World, RADAR_DOOR);
	m_pEnemySpawn = new CServerRadar(&GameServer()->m_World, RADAR_ENEMY);
	m_pReactor = new CServerRadar(&GameServer()->m_World, RADAR_REACTOR);
	m_pPushRadar = new CServerRadar(&GameServer()->m_World, RADAR_REACTOR);
	m_NumSwitchRadars = 0;
	for(int i = 0; i < 8; i++)
		m_apSwitchRadar[i] = 0;
}

CGameControllerInvasion::~CGameControllerInvasion()
{
}

void CGameControllerInvasion::SetupLevelTheme()
{
	int Level = g_Config.m_SvMapGenLevel;
	m_LevelTheme = InvasionThemeFromLevel(Level);
	m_MapBiome = PveSanitizeBiome(m_MapBiome == PVE_BIOME_BLUE_PLANET ? m_MapBiome : g_Config.m_SvPveBiome);
	m_EscapeLevel = (m_LevelTheme == INVASION_THEME_ACID_ESCAPE);
	m_DefendLevel = (m_LevelTheme == INVASION_THEME_REACTOR_DEFEND);
	m_SwitchCoopLevel = (m_LevelTheme == INVASION_THEME_DUAL_SWITCHES);
	m_EliteWave = (m_LevelTheme == INVASION_THEME_ELITE_WAVE);
	m_SwitchesRequired = m_SwitchCoopLevel ? 2 : (m_EscapeLevel ? 1 : 0);
	m_SwitchesActivated = 0;

	const int DepthQuests = InvasionDepthQuests(Level);
	m_LevelQuestsLeft = m_EscapeLevel ? 0 : max(2, DepthQuests);
	if(m_LevelTheme == INVASION_THEME_BOSS_ASSAULT || Level == 30)
		m_BossesLeft = 1;

	if(g_Config.m_SvInvFails != INV_FORCE_FLOOR_ONE && g_Config.m_SvInvFails >= 2)
	{
		m_RunBuffActive = true;
		m_WaveSizeNerf = 1;
	}
	else if(g_Config.m_SvInvFails != INV_FORCE_FLOOR_ONE && g_Config.m_SvInvFails >= 1)
		m_WaveSizeNerf = 1;

	const int Players = max(1, CountHumans());
	const float Scale = InvasionCountScale(GameServer());
	m_QuestWaveEndTick = 0;
	m_QuestWaveSize = InvasionConcurrentCap(Level, Players, m_WaveSizeNerf);
	if(m_EscapeLevel)
		m_EnemiesLeft = InvasionEnemyBudget(INV_BUDGET_ACID, Level, Players, m_LevelTheme, Scale);
	else if(m_LevelTheme == INVASION_THEME_TIMED_SURVIVE)
		m_EnemiesLeft = 0;
	else
		m_EnemiesLeft = InvasionEnemyBudget(INV_BUDGET_OPENING, Level, Players, m_LevelTheme, Scale);
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
	if(IGameController::OnEntity(Index, Pos))
		return true;

	if(Index == ENTITY_ENEMYSPAWN && m_NumEnemySpawnPos < MAX_ENEMIES)
	{
		m_aEnemySpawnPos[m_NumEnemySpawnPos++] = Pos;
		return true;
	}

	return false;
}

bool CGameControllerInvasion::GetSpawnPos(int Team, vec2 *pOutPos)
{
	if(!pOutPos || !m_NumEnemySpawnPos)
		return false;

	m_SpawnPosRotation++;
	m_SpawnPosRotation = m_SpawnPosRotation % m_NumEnemySpawnPos;

	*pOutPos = m_aEnemySpawnPos[m_SpawnPosRotation];
	return true;
}

bool CGameControllerInvasion::GetBossSpawnPos(vec2 *pOutPos)
{
	return FindBossSpawnPosition(
		&GameServer()->m_World, m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, pOutPos);
}

vec2 CGameControllerInvasion::GetBotSpawnPos()
{
	if(m_GroupSpawnPos.x < 1.0f)
	{
		vec2 Pos(0, 0);
		GetSpawnPos(0, &Pos);
		return Pos;
	}

	vec2 Pos = m_GroupSpawnPos;

	for(int i = 0; i < 99; i++)
	{
		Pos = m_GroupSpawnPos + vec2(frandom() - frandom(), frandom() - frandom()) * 400;
		if(!GameServer()->Collision()->TestBox(Pos, vec2(32.0f, 74.0f)))
			return Pos;
	}

	return m_GroupSpawnPos;
}

void CGameControllerInvasion::RandomGroupSpawnPos()
{
	if(!m_NumEnemySpawnPos)
		return;
	m_GroupSpawnPos = m_aEnemySpawnPos[irandom(m_NumEnemySpawnPos)];
	m_pEnemySpawn->Activate(m_GroupSpawnPos, Server()->Tick() + Server()->TickSpeed() * 5);
}

bool CGameControllerInvasion::CanSpawn(int Team, vec2 *pOutPos, bool IsBot)
{
	CSpawnEval Eval;

	if(Team == TEAM_SPECTATORS)
		return false;

	if(IsBot)
	{
		if(m_EnemiesLeft <= 0)
			return false;

		if(m_BotSpawnTick > Server()->Tick())
			return false;

		if(m_GroupSpawnPos.x < 1.0f)
		{
			if(GetSpawnPos(1, pOutPos))
				return true;
			EvaluateSpawnType(&Eval, 0);
			if(!Eval.m_Got)
				return false;
			*pOutPos = Eval.m_Pos;
			return true;
		}

		vec2 Pos = GetBotSpawnPos();
		*pOutPos = Pos;

		const int Level = max(0, g_Config.m_SvMapGenLevel);
		const int EarlyLevel = min(Level, 20);
		const int LateLevel = max(0, Level - 20);
		float SpawnDelay = max(0.25f, 0.5f - EarlyLevel * 0.01f - LateLevel * 0.0025f);
		m_BotSpawnTick = Server()->Tick() + Server()->TickSpeed() * SpawnDelay;

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
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == ExcludeCID)
			continue;
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!IsHumanCoopPlayer(pPlayer) || pPlayer->GetTeam() == TEAM_SPECTATORS)
			continue;
		if(pPlayer->GetCharacter() && pPlayer->GetCharacter()->IsAlive())
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
	   !IsRetryVoter(ClientID) || Choice < PVE_INVASION_RETRY || Choice > PVE_INVASION_RESET ||
	   m_aRetryVotes[ClientID] != -1)
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
	g_Config.m_SvMapGenSeed = (int)((unsigned long long)time_get() % 0x7FFFFFFFull);
	if(g_Config.m_SvMapGenSeed <= 0)
		g_Config.m_SvMapGenSeed = 1;
	GameServer()->WriteExpeditionSave(false);
	RegenerateMapFromTemplate();
}

void CGameControllerInvasion::RegenerateMapFromTemplate()
{
	char aTemplate[128];
	int Biome = 0;
	if(Server()->FindInvasionMapForLevel(g_Config.m_SvMapGenLevel, aTemplate, sizeof(aTemplate), &Biome))
	{
		g_Config.m_SvPveBiome = Biome;
		str_copy(g_Config.m_SvMap, aTemplate, sizeof(g_Config.m_SvMap));
		str_copy(g_Config.m_SvInvMap, aTemplate, sizeof(g_Config.m_SvInvMap));
		str_copy(Server()->m_aMapInUse, aTemplate, sizeof(Server()->m_aMapInUse));
		Server()->m_MapGenerated = false;
	}
	else if(g_Config.m_SvMapGen && g_Config.m_SvInvMap[0] && str_comp(g_Config.m_SvInvMap, "generated") != 0)
	{
		str_copy(g_Config.m_SvMap, g_Config.m_SvInvMap, sizeof(g_Config.m_SvMap));
		Server()->m_MapGenerated = false;
	}
	GameServer()->ReloadMap();
}

void CGameControllerInvasion::BeginPostRoundTransition()
{
	// A cleared floor always continues the same expedition. The generic game
	// vote would allow a successful run to switch modes between generated maps.
	RegenerateMapFromTemplate();
}

void CGameControllerInvasion::RewardQuestGold()
{
	int Gold = 5 + g_Config.m_SvMapGenLevel / 5;
	if(m_RunBuffActive)
		Gold += 3;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer || pPlayer->m_IsBot)
			continue;
		pPlayer->IncreaseGold(Gold);
	}
}

void CGameControllerInvasion::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);

	if(!RequestAI)
	{
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnPlayerSpawn(pChr->GetPlayer()->GetCID());
		if(m_DefendLevel)
			pChr->m_Kits = max(pChr->m_Kits, 10);
		return;
	}

	{
		bool Found = false;

		if(m_EnemiesLeft > 0)
		{
			m_EnemiesLeft--;
			Found = true;

			int Level = 0;

			for(int i = 0; i < 9; i++)
				if(m_EnemiesLeft < 1 - i * 3 + g_Config.m_SvMapGenLevel / 2)
					Level++;

			if(frandom() < 0.7f && Level > 2)
				Level = irandom(Level - 1);

			GameServer()->GetAISkin(&pChr->m_AISkin, false, max(1, Level), m_QuestWaveType);
			pChr->SetAISkin();
			pChr->m_IsBot = true;

			bool UseElite = m_EliteWave && frandom() < 0.45f;
			if(!UseElite && g_Config.m_SvMapGenLevel > 15 && frandom() < 0.15f)
				UseElite = frandom() < 0.45f;
			const EInvasionSkinId Profile = InvasionSkinForWave(m_QuestWaveType, Level, UseElite);
			pChr->m_pAI = new CInvasionAI(GameServer(), pChr, Level, Profile);

			pChr->m_IsBot = true;
			pChr->m_TeeInfos.m_IsBot = true;

			pChr->m_SkipPickups = 999;
			Trigger(false);
		}

		if(!Found)
		{
			pChr->m_pAI = new CInvasionAI(GameServer(), pChr, g_Config.m_SvMapGenLevel, INVASION_SKIN_ALIEN1);
			pChr->m_IsBot = true;
			pChr->m_TeeInfos.m_IsBot = true;
			pChr->MarkToBeKicked();
			Trigger(false);
		}
	}
}

void CGameControllerInvasion::Trigger(bool IncreaseLevel)
{
	if(IncreaseLevel)
		m_TriggerLevel++;

	GameServer()->TriggerBotAI(m_TriggerLevel);
}

void CGameControllerInvasion::SpawnNewWave(bool AddBots)
{
	const int Level = g_Config.m_SvMapGenLevel;
	const int Players = max(1, CountHumans());
	const int WaveCap = InvasionConcurrentCap(Level, Players, m_WaveSizeNerf);
	const int EnvironmentPhase =
		GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->EnvironmentPhase() : PVE_ENV_PHASE_CALM;
	m_QuestWaveType = InvasionWaveType(m_ForcedWaveType,
									   m_LevelTheme,
									   m_MapBiome,
									   Level,
									   EnvironmentPhase,
									   (unsigned long long)g_Config.m_SvMapGenSeed);

	int BudgetKind = INV_BUDGET_OPENING;
	if(m_Quest == QUEST_SURVIVEWAVETIME || (m_Quest == QUEST_NONE && m_LevelTheme == INVASION_THEME_TIMED_SURVIVE) ||
	   (m_LevelTheme == INVASION_THEME_TRAP_RUN && m_QuestsCompleted >= 1))
		BudgetKind = INV_BUDGET_TIMED;
	else if(m_Quest == QUEST_SURVIVEWAVE || m_Quest == QUEST_DEFEND)
		BudgetKind = INV_BUDGET_WAVE;
	else if(m_Quest == QUEST_PUSH_FORWARD)
		BudgetKind = INV_BUDGET_PUSH;
	else if(m_Quest == QUEST_KILLREMAININGENEMIES)
		BudgetKind = INV_BUDGET_PURGE;
	else if(m_Quest == QUEST_KILL_BOSS)
		BudgetKind = INV_BUDGET_BOSS_MINIONS;

	m_QuestWaveEndTick = 0;
	if(BudgetKind == INV_BUDGET_TIMED)
	{
		int TimedSecs = 35 + min(20, Level / 2);
		if(m_LevelTheme == INVASION_THEME_TRAP_RUN)
			TimedSecs = 30 + Level / 3;
		m_QuestWaveEndTick = Server()->Tick() + Server()->TickSpeed() * TimedSecs;
	}
	m_QuestWaveSize = WaveCap;
	m_EnemiesLeft = InvasionEnemyBudget(BudgetKind, Level, Players, m_LevelTheme, InvasionCountScale(GameServer()));

	if(AddBots)
	{
		RandomGroupSpawnPos();
		int ThreatDivisor = m_LevelTheme == INVASION_THEME_ELITE_WAVE ? (Level > 20 ? 5 : 3) :
			(Level > 20 ? 8 : 6);
		const SThreatBudgetResult ThreatReplacement = SpawnThreatBudgetSpecialists(&GameServer()->m_World,
																				   m_aEnemySpawnPos,
																				   m_NumEnemySpawnPos,
																				   &m_SpawnPosRotation,
																				   Level,
																				   m_EnemiesLeft,
																				   m_QuestWaveSize,
																				   ThreatDivisor);
		m_EnemiesLeft -= ThreatReplacement.m_ThreatSpent;
		const int BotCap = max(0, m_QuestWaveSize - ThreatReplacement.m_EntitiesSpawned);
		const int SpawnCount = min(m_EnemiesLeft, max(0, BotCap - CountBots()));
		for(int i = 0; i < SpawnCount; i++)
			GameServer()->AddBot();
	}

}

void CGameControllerInvasion::DisplayExit(vec2 Pos)
{
	m_pDoor->Activate(Pos);
}

void CGameControllerInvasion::SpawnBosses(int Count)
{
	m_pRegionalBoss = 0;
	m_RegionalBossPhase = -1;
	m_RegionalBossSupportSpawned = 0;
	BuildRegionalBossArena();
	int Spawned = 0;
	for(int i = 0; i < Count; i++)
	{
		vec2 p;
		if(m_RegionalBossPointCount > 0)
			p = m_aRegionalBossPoints[i % m_RegionalBossPointCount];
		else if(!GetBossSpawnPos(&p))
		{
			dbg_msg("inv", "boss room unavailable; skipping unsafe regional boss spawn");
			continue;
		}
		CDroid *pBoss = SpawnBoss(
			&GameServer()->m_World,
			p,
			g_Config.m_SvMapGenLevel,
			m_MapTemplate == INV_MAP_UNKNOWN ? -1 : InvasionRegionalBossType(m_MapTemplate));
		if(!pBoss)
			continue;
		Spawned++;
		if(!m_pRegionalBoss)
			m_pRegionalBoss = pBoss;
	}
	m_BossesLeft = Spawned;
}

int CGameControllerInvasion::CountBossesAlive() const
{
	return CountAliveBosses(&GameServer()->m_World);
}

bool CGameControllerInvasion::IsObjectiveTarget(bool Boss) const
{
	return Boss;
}

int CGameControllerInvasion::CountBuildingsOfType(int Type) const
{
	CBuilding *apEnts[256];
	int Num =
		GameServer()->m_World.FindEntities(vec2(0, 0), 0.0f, (CEntity **)apEnts, 256, CGameWorld::ENTTYPE_BUILDING);
	int Count = 0;
	for(int i = 0; i < Num; i++)
	{
		if(apEnts[i] && apEnts[i]->m_Type == Type)
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
	for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding;
		pBuilding = (CBuilding *)pBuilding->TypeNext())
	{
		if(pBuilding->m_Type != BUILDING_SWITCH || !pBuilding->m_PveSwitchActive || pBuilding->m_aStatus[BSTATUS_ON])
			continue;
		if(m_NumSwitchRadars >= 8)
			break;
		CServerRadar *pRadar = new CServerRadar(&GameServer()->m_World, RADAR_REACTOR);
		pRadar->Activate(pBuilding->m_Pos);
		m_apSwitchRadar[m_NumSwitchRadars++] = pRadar;
	}
}

void CGameControllerInvasion::ShowCartographerObjectives()
{
	if(!AnyCartographer())
		return;
	RefreshSwitchRadars();
	if(m_pRegionalBoss && m_pRegionalBoss->m_Health > 0 && m_pReactor)
		m_pReactor->Activate(m_pRegionalBoss->m_Pos);
	else if(m_pReactor)
	{
		for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding;
			pBuilding = (CBuilding *)pBuilding->TypeNext())
			if(pBuilding->m_Type == BUILDING_REACTOR)
			{
				m_pReactor->Activate(pBuilding->m_Pos);
				break;
			}
	}
	if(!m_pPushRadar || m_PushPointCount <= 0)
		return;
	for(int i = 0; i < m_PushPointCount; i++)
		if(!(m_PushCompletedMask & (1 << i)))
		{
			m_pPushRadar->Activate(m_aPushPoints[i]);
			break;
		}
}

void CGameControllerInvasion::SetSwitchesActive(bool Active)
{
	for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding;
		pBuilding = (CBuilding *)pBuilding->TypeNext())
		if(pBuilding->m_Type == BUILDING_SWITCH)
			pBuilding->SetPveSwitchActive(Active);
	if(Active)
		RefreshSwitchRadars();
	else
		ClearSwitchRadars();
}

void CGameControllerInvasion::ClearPushForward()
{
	m_PushForwardActive = false;
	m_PushPointCount = 0;
	m_PushCompletedMask = 0;
	m_PushActiveMask = 0;
	m_PushPointEndTick = 0;
	m_PushParallel = false;
	if(m_pPushRadar)
		m_pPushRadar->Deactivate();
}

static bool InvasionPushPointUsable(CGameContext *pGameServer, vec2 Raw, const vec2 *pPoints, int Count, vec2 *pOut)
{
	if(!pGameServer || !pOut || (Raw.x == 0.0f && Raw.y == 0.0f))
		return false;
	CCollision *pCollision = pGameServer->Collision();
	const vec2 Candidate = pCollision->SnapToStandPos(Raw);
	if(!pCollision->IsSafeStandPos(Candidate))
		return false;
	for(int i = 0; i < Count; i++)
		if(distance(Candidate, pPoints[i]) < 280.0f)
			return false;
	*pOut = Candidate;
	return true;
}

bool CGameControllerInvasion::BuildPushForwardRoute()
{
	ClearPushForward();
	const int Desired = m_MapTemplate == INV_MAP_LARGE1 ? 3 : 2;
	const int Seed = max(0, g_Config.m_SvMapGenSeed) + max(0, g_Config.m_SvMapGenLevel) * 17 +
		m_MapTemplate * 31;
	const int Stride = max(1, m_NumEnemySpawnPos / max(1, Desired));

	// ponytail: probe fixed candidate caps; authored route metadata can replace
	// this if hand-built maps outgrow the bounded fallback.
	const int SpawnChecks = min(m_NumEnemySpawnPos, 64);
	for(int i = 0; i < SpawnChecks && m_PushPointCount < Desired; i++)
	{
		const int Index = (Seed + i * Stride) % m_NumEnemySpawnPos;
		vec2 Candidate;
		if(InvasionPushPointUsable(
			   GameServer(), m_aEnemySpawnPos[Index], m_aPushPoints, m_PushPointCount, &Candidate))
			m_aPushPoints[m_PushPointCount++] = Candidate;
	}

	// Generated maps normally provide enemy markers. Keep a deterministic
	// waypoint fallback for hand-authored maps without enough markers.
	const int WaypointChecks = min(GameServer()->Collision()->WaypointCount(), 128);
	for(int i = 0; i < WaypointChecks && m_PushPointCount < Desired; i++)
	{
		vec2 Candidate;
		if(InvasionPushPointUsable(
			   GameServer(), GameServer()->Collision()->GetWaypointPos(i), m_aPushPoints, m_PushPointCount, &Candidate))
			m_aPushPoints[m_PushPointCount++] = Candidate;
	}

	if(m_PushPointCount < 2)
	{
		ClearPushForward();
		return false;
	}

	m_PushForwardActive = true;
	m_PushParallel = m_MapTemplate == INV_MAP_LARGE1 && CountHumansAlive() > 1 && m_PushPointCount > 2;
	m_PushCompletedMask = 0;
	ActivatePushForwardGroup();
	return true;
}

void CGameControllerInvasion::ActivatePushForwardGroup()
{
	const int AllMask = (1 << m_PushPointCount) - 1;
	const int Remaining = AllMask & ~m_PushCompletedMask;
	if(!Remaining)
	{
		m_PushActiveMask = 0;
		return;
	}

	if(m_PushParallel && !m_PushCompletedMask && m_PushPointCount > 1)
		m_PushActiveMask = (1 << 0) | (1 << 1);
	else
	{
		m_PushActiveMask = 0;
		for(int i = 0; i < m_PushPointCount; i++)
			if(Remaining & (1 << i))
			{
				m_PushActiveMask = 1 << i;
				break;
			}
	}

	const int Level = max(1, g_Config.m_SvMapGenLevel);
	int WindowSeconds = 12 + min(8, Level / 10);
	if(m_MapBiome == PVE_BIOME_BLUE_PLANET && GameServer()->m_pPveDirector)
	{
		const int Phase = GameServer()->m_pPveDirector->EnvironmentPhase();
		if(Phase == PVE_ENV_PHASE_RECOVERY)
			WindowSeconds += 3;
	}
	m_PushPointEndTick = Server()->Tick() + Server()->TickSpeed() * WindowSeconds;
	m_QuestProgressCounter = 0;
	for(int i = 0; i < m_PushPointCount; i++)
		if(!(m_PushCompletedMask & (1 << i)))
			m_QuestProgressCounter++;

	if(m_pPushRadar)
	{
		int ActivePoint = -1;
		for(int i = 0; i < m_PushPointCount; i++)
			if(m_PushActiveMask & (1 << i))
			{
				m_pPushRadar->Activate(m_aPushPoints[i]);
				ActivePoint = i;
				break;
			}
		if(ActivePoint >= 0)
		{
			GameServer()->CreateEffect(FX_ELECTRIC, m_aPushPoints[ActivePoint]);
			GameServer()->CreateSound(m_aPushPoints[ActivePoint], SOUND_WEAPON_SPAWN);
		}
	}
}

void CGameControllerInvasion::TickPushForward()
{
	if(!m_PushForwardActive)
		return;
	if(m_PushPointEndTick <= Server()->Tick())
	{
		GameServer()->SendBroadcast("The front line stalled — retreat failed", -1);
		ClearPushForward();
		m_RoundOverTick = Server()->Tick();
		return;
	}

	for(int Point = 0; Point < m_PushPointCount; Point++)
	{
		if(!(m_PushActiveMask & (1 << Point)) || (m_PushCompletedMask & (1 << Point)))
			continue;
		bool Reached = false;
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
		{
			CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
			if(!IsHumanCoopPlayer(pPlayer) || pPlayer->GetTeam() == TEAM_SPECTATORS)
				continue;
			CCharacter *pChr = pPlayer->GetCharacter();
			if(pChr && pChr->IsAlive() && distance(pChr->m_Pos, m_aPushPoints[Point]) < 190.0f)
			{
				Reached = true;
				break;
			}
		}
		if(Reached)
		{
			m_PushCompletedMask |= 1 << Point;
			m_PushActiveMask &= ~(1 << Point);
			GameServer()->CreateEffect(FX_GREEN_EXPLOSION, m_aPushPoints[Point]);
			GameServer()->CreateSound(m_aPushPoints[Point], SOUND_PICKUP_ARMOR);
		}
	}

	const int AllMask = (1 << m_PushPointCount) - 1;
	if(m_PushCompletedMask == AllMask)
	{
		m_QuestProgressCounter = 0;
		CompleteCurrentQuest();
		return;
	}
	if(!m_PushActiveMask)
		ActivatePushForwardGroup();
	else
	{
		m_QuestProgressCounter = 0;
		for(int i = 0; i < m_PushPointCount; i++)
			if(!(m_PushCompletedMask & (1 << i)))
				m_QuestProgressCounter++;
		if(m_pPushRadar)
			for(int i = 0; i < m_PushPointCount; i++)
				if(m_PushActiveMask & (1 << i))
				{
					m_pPushRadar->Activate(m_aPushPoints[i]);
					break;
				}
	}
}

void CGameControllerInvasion::BuildRegionalBossArena()
{
	m_RegionalBossPointCount = 0;
	if(!BuildPushForwardRoute())
		return;
	for(int i = 0; i < m_PushPointCount && m_RegionalBossPointCount < INV_MAX_PUSH_POINTS; i++)
	{
		vec2 SafePoint;
		int Rotation = -1;
		if(!FindBossSpawnPosition(&GameServer()->m_World, &m_aPushPoints[i], 1, &Rotation, &SafePoint))
			continue;
		bool Duplicate = false;
		for(int j = 0; j < m_RegionalBossPointCount; j++)
			if(distance(SafePoint, m_aRegionalBossPoints[j]) < 192.0f)
			{
				Duplicate = true;
				break;
			}
		if(!Duplicate)
			m_aRegionalBossPoints[m_RegionalBossPointCount++] = SafePoint;
	}
	if(m_RegionalBossPointCount == 0)
	{
		vec2 SafePoint;
		if(FindBossSpawnPosition(
			   &GameServer()->m_World, m_aEnemySpawnPos, m_NumEnemySpawnPos, &m_SpawnPosRotation, &SafePoint))
			m_aRegionalBossPoints[m_RegionalBossPointCount++] = SafePoint;
	}
	ClearPushForward();
}

void CGameControllerInvasion::ApplyRegionalBossPhase(int Phase)
{
	if(!m_pRegionalBoss || m_pRegionalBoss->m_Health <= 0 || Phase <= m_RegionalBossPhase)
		return;
	m_RegionalBossPhase = Phase;
	if(m_RegionalBossPointCount > 0)
	{
		const int Point = min(Phase, m_RegionalBossPointCount - 1);
		m_pRegionalBoss->m_Pos = m_aRegionalBossPoints[Point];
		if(m_pReactor)
			m_pReactor->Activate(m_pRegionalBoss->m_Pos);
	}

	if(Phase > 0 && m_RegionalBossSupportSpawned < 2 && m_EnemiesLeft > 0)
	{
		const int Type = InvasionRegionalSupportType(m_MapTemplate, Phase);
		const int Cost = DroidThreatCost(Type);
		if(m_EnemiesLeft >= Cost)
		{
			vec2 Pos = m_pRegionalBoss->m_Pos + vec2(Phase == 1 ? -220.0f : 220.0f, -100.0f);
			if(SpawnSpecialist(&GameServer()->m_World, Pos, Type))
			{
				m_EnemiesLeft -= Cost;
				m_RegionalBossSupportSpawned++;
			}
		}
	}

	if(Phase > 0)
		GameServer()->SendBroadcastFormat(
			-1, false, "%s enters phase %d", InvasionMapTemplateName(m_MapTemplate), Phase + 1);
}

void CGameControllerInvasion::TickRegionalBoss()
{
	if(!m_pRegionalBoss || m_pRegionalBoss->m_Health <= 0)
		return;
	int Phase = 0;
	if(m_pRegionalBoss->m_MaxHealth > 0)
	{
		if(m_pRegionalBoss->m_Health * 3 <= m_pRegionalBoss->m_MaxHealth * 2)
			Phase = 1;
		if(m_pRegionalBoss->m_Health * 3 <= m_pRegionalBoss->m_MaxHealth)
			Phase = 2;
	}
	ApplyRegionalBossPhase(Phase);
}

bool CGameControllerInvasion::IsReactorDefenseActive() const
{
	return m_GameState == STATE_GAME && m_Quest == QUEST_DEFEND && !m_RoundOverTick;
}

void CGameControllerInvasion::SetReactorDefenseActive(bool Active)
{
	const int ReactorLife = min(1200, 600 + max(1, g_Config.m_SvMapGenLevel) * 20);
	bool RadarActivated = false;
	for(CBuilding *pBuilding = (CBuilding *)GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_BUILDING); pBuilding;
		pBuilding = (CBuilding *)pBuilding->TypeNext())
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

int CGameControllerInvasion::OnCharacterDeath(class CCharacter *pVictim,
											  class CPlayer *pKiller,
											  const CAttackSource &Source)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Source);
	if(!pVictim->m_IsBot && GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnPlayerDeath(pVictim->GetPlayer()->GetCID());

	if(pVictim->m_IsBot && !pVictim->ToBeKicked())
	{
		if(pKiller && !pKiller->m_IsBot && GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnEnemyKilled(Source, pVictim->m_Pos, pVictim);
		// Ordinary waves use the dead bot slot to consume m_EnemiesLeft on respawn.
		// Reactor defense has a separate reinforcement loop, so its bots are kicked.
		if(m_EnemiesLeft <= 0 || m_EscapeSpawnActive || m_Quest == QUEST_DEFEND)
			pVictim->MarkToBeKicked();

		if(pKiller)
		{
			Trigger(true);

			if(frandom() < 0.013f)
				GameServer()->m_pController->DropWeapon(pVictim->m_Pos,
														vec2(frandom() * 6.0 - frandom() * 6.0, 0 - frandom() * 14.0),
														GameServer()->NewWeapon(CWeaponCatalog::Static(SW_UPGRADE)));
			else if(frandom() < 0.013f)
				GameServer()->m_pController->DropWeapon(pVictim->m_Pos,
														vec2(frandom() * 6.0 - frandom() * 6.0, 0 - frandom() * 14.0),
														GameServer()->NewWeapon(CWeaponCatalog::Static(SW_RESPAWNER)));
		}
	}

	if(g_Config.m_SvSurvivalMode && IsHumanCoopPlayer(pVictim->GetPlayer()))
	{
		const int CID = pVictim->GetPlayer()->GetCID();
		// Dead until ally uses Respawn device or next floor; wipe when nobody left to revive.
		if(CountHumansAlive(CID) <= 0)
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
	if(!m_RoundWin)
	{
		m_RoundWin = true;
		m_RoundWinTick = Server()->Tick() + Server()->TickSpeed() * 2;

		if(CountHumans() > 1 && CID >= 0 && CID < MAX_CLIENTS)
			GameServer()->SendBroadcastFormat(-1, false, "%s reached the door", Server()->ClientName(CID));
	}

	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? GameServer()->m_apPlayers[CID] : 0;
	if(pPlayer && pPlayer->GetCharacter() && !pPlayer->GetCharacter()->IgnoreCollision())
		pPlayer->GetCharacter()->Warp();
}

void CGameControllerInvasion::ChangeQuest(int NextQuest, float QueueTimeInSeconds)
{
	if(m_NextQuest == NextQuest)
		return;

	m_NextQuest = NextQuest;
	m_QuestChangeTick = Server()->Tick() + Server()->TickSpeed() * QueueTimeInSeconds;
}

void CGameControllerInvasion::SendQuestStartMessage(int Quest)
{
	if(m_EscapeLevel && Quest == QUEST_REACHDOOR)
		GameServer()->SendBroadcast("Rising acid! Reach the exit", -1);
	else if(m_DefendLevel && Quest == QUEST_DEFEND)
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
	if(m_Quest == QUEST_DEFEND)
		SetReactorDefenseActive(false);
	if(m_Quest == QUEST_KILL_BOSS)
	{
		m_pRegionalBoss = 0;
		if(m_pReactor)
			m_pReactor->Deactivate();
	}
	m_DefendPrepEndTick = 0;
	if(m_Quest == QUEST_ACTIVATE_SWITCHES || m_Quest == QUEST_FIND_SWITCH)
		SetSwitchesActive(false);
	const bool FinalObjective = IsFinalObjective();
	if(m_Quest == QUEST_PUSH_FORWARD)
		ClearPushForward();
	SendQuestCompletedMessage(m_Quest);
	RewardQuestGold();
	m_Quest = QUEST_NONE;
	m_NextQuest = QUEST_NONE;
	m_QuestsCompleted++;
	if(GameServer()->m_pPveDirector)
	{
		if(FinalObjective)
			GameServer()->m_pPveDirector->GrantDeepSovereignBarrier();
		GameServer()->m_pPveDirector->OnObjectiveComplete();
	}
	if(GameServer()->m_pTutorialDirector)
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			if(GameServer()->m_apPlayers[ClientID] && !GameServer()->m_apPlayers[ClientID]->m_IsBot)
				GameServer()->m_pTutorialDirector->OnGameplayProgress(ClientID, TUTORIAL_EVENT_OBJECTIVE);
	m_ForcedWaveType = WAVE_NONE;
}

void CGameControllerInvasion::StartThemeQuest()
{
	if(m_LevelTheme == INVASION_THEME_TIMED_SURVIVE)
		ChangeQuest(QUEST_SURVIVEWAVETIME, INV_QUEST_QUEUE_TIME);
	else
		ChangeQuest(QUEST_KILLREMAININGENEMIES, INV_QUEST_QUEUE_TIME);
}

void CGameControllerInvasion::QueueNextObjectiveQuest()
{
	const int Done = m_QuestsCompleted;
	const int LastSlot = max(1, m_LevelQuestsLeft - 1);
	int Next = QUEST_SURVIVEWAVE;
	if(g_Config.m_SvMapGenLevel == 30 && Done >= LastSlot)
	{
		Next = QUEST_KILL_BOSS;
		ChangeQuest(Next, INV_QUEST_QUEUE_TIME);
		return;
	}
	const bool Deep = m_LevelQuestsLeft >= 3;
	const bool PushSlot = InvasionThemeAllowsPushForward(m_LevelTheme, Deep) &&
		(Done < LastSlot || (m_LevelTheme == INVASION_THEME_TRAP_RUN && !Deep && Done == LastSlot));
	if(PushSlot && BuildPushForwardRoute())
	{
		ChangeQuest(QUEST_PUSH_FORWARD, INV_QUEST_QUEUE_TIME);
		return;
	}

	Next = InvasionObjectiveQuest(m_LevelTheme, Done, LastSlot, g_Config.m_SvMapGenLevel);
	if(m_LevelTheme == INVASION_THEME_STANDARD_WAVE)
		m_ForcedWaveType = 1 + (g_Config.m_SvMapGenLevel / 7) % (NUM_WAVES - 1);
	if(Next == QUEST_ACTIVATE_SWITCHES)
	{
		const int Switches = SwitchesAvailable();
		if(Switches > 0)
		{
			m_SwitchesRequired = min(2, Switches);
			m_SwitchCoopLevel = true;
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
	else if(Next == QUEST_DEFEND && ReactorsLeft() <= 0)
	{
		dbg_msg("inv", "theme reactor-defend: no reactor on map, skip defend quest");
		GameServer()->SendBroadcast("Reactor missing — survive the wave instead", -1);
		Next = QUEST_SURVIVEWAVE;
	}
	else if(Next == QUEST_KILLREMAININGENEMIES)
	{
		const int Players = max(1, CountHumans());
		m_QuestWaveSize = InvasionConcurrentCap(g_Config.m_SvMapGenLevel, Players, m_WaveSizeNerf);
		m_EnemiesLeft = InvasionEnemyBudget(INV_BUDGET_PURGE, g_Config.m_SvMapGenLevel, Players, m_LevelTheme,
											InvasionCountScale(GameServer()));
		RandomGroupSpawnPos();
		const int SpawnCount = min(m_EnemiesLeft, max(0, m_QuestWaveSize - CountBots()));
		for(int i = 0; i < SpawnCount; i++)
			GameServer()->AddBot();
	}

	ChangeQuest(Next, INV_QUEST_QUEUE_TIME);
}

void CGameControllerInvasion::SpawnEliteContractGuard()
{
	if(!GameServer()->m_pPveDirector || GameServer()->m_pPveDirector->ActiveContract() != PVE_CONTRACT_ELITE_GUARD ||
	   m_Quest == QUEST_NONE || m_Quest == QUEST_REACHDOOR)
		return;
	vec2 Pos;
	if(!GetBossSpawnPos(&Pos))
		Pos = vec2(4000, 4000);
	CDroid *pGuard = SpawnBoss(&GameServer()->m_World, Pos, g_Config.m_SvMapGenLevel + 1);
	GameServer()->m_pPveDirector->RegisterEliteContractBoss(pGuard);
}

void CGameControllerInvasion::OnSwitchTriggered()
{
	if(m_Quest != QUEST_ACTIVATE_SWITCHES && m_Quest != QUEST_FIND_SWITCH)
		return;
	m_SwitchesActivated++;

	if(m_SwitchCoopLevel)
	{
		m_QuestProgressCounter = max(0, m_SwitchesRequired - m_SwitchesActivated);
		RefreshSwitchRadars();
		if(m_SwitchesActivated < m_SwitchesRequired)
		{
			GameServer()->SendBroadcastFormat(
				-1, false, "Switch %d/%d activated", m_SwitchesActivated, m_SwitchesRequired);
			return;
		}

		if(m_Quest == QUEST_ACTIVATE_SWITCHES || m_NextQuest == QUEST_ACTIVATE_SWITCHES)
		{
			m_QuestChangeTick = 0;
			m_NextQuest = QUEST_NONE;
			if(m_Quest == QUEST_ACTIVATE_SWITCHES)
				CompleteCurrentQuest();
			else
				m_Quest = QUEST_NONE;
		}
		// Door opens only when REACHDOOR starts after remaining objectives.
		return;
	}

	if(m_EscapeLevel)
	{
		BeginRisingAcid(50);
		m_EscapeSpawnActive = true;
		m_EnemiesLeft = InvasionEnemyBudget(INV_BUDGET_TIMED, g_Config.m_SvMapGenLevel, max(1, CountHumans()), m_LevelTheme, 1.0f);
		m_QuestWaveSize = InvasionConcurrentCap(g_Config.m_SvMapGenLevel, max(1, CountHumans()), m_WaveSizeNerf);
		m_BotSpawnTick = Server()->Tick();

		if(m_Quest == QUEST_FIND_SWITCH || m_NextQuest == QUEST_FIND_SWITCH)
		{
			m_QuestChangeTick = 0;
			m_NextQuest = QUEST_NONE;
			if(m_Quest == QUEST_FIND_SWITCH)
				CompleteCurrentQuest();
			else
				m_Quest = QUEST_NONE;
			ChangeQuest(QUEST_REACHDOOR, 0.5f);
		}
		else if(m_Quest != QUEST_REACHDOOR && m_NextQuest != QUEST_REACHDOOR)
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

	if(m_GameState == STATE_FAIL)
		return;

	if(m_GameState == STATE_GAME)
	{
		if(m_EliteContractSpawned && GameServer()->m_pPveDirector && CountBossesAlive() <= 0)
		{
			m_EliteContractSpawned = false;
			GameServer()->m_pPveDirector->OnBossKilled();
		}
		// Wipe only after someone has already died this round (SURVIVAL_NOCANDO).
		// At join/spawn, humans exist but aren't alive yet — don't end the round.
		if(g_Config.m_SvSurvivalMode && !m_RoundOverTick && m_SurvivalStatus == SURVIVAL_NOCANDO && CountHumans() > 0 &&
		   CountHumansAlive() <= 0)
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

		if(m_QuestChangeTick && m_QuestChangeTick <= Server()->Tick())
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

			if(m_Quest == QUEST_REACHDOOR && !m_EscapeLevel && !m_SwitchCoopLevel)
				TriggerEscape();
			else if(m_Quest == QUEST_REACHDOOR && m_SwitchCoopLevel)
			{
				// Open if switches done, or map had no usable switches (avoid softlock).
				if(m_SwitchesActivated >= m_SwitchesRequired || SwitchesAvailable() <= 0 || m_SwitchesRequired <= 0)
					TriggerEscape();
			}

			if(m_Quest == QUEST_SURVIVEWAVE || m_Quest == QUEST_SURVIVEWAVETIME)
				SpawnNewWave();
			else if(m_Quest == QUEST_PUSH_FORWARD)
			{
				if(!m_PushForwardActive || m_PushPointCount < 2)
				{
					dbg_msg("inv", "push-forward route disappeared, fallback to wave");
					m_Quest = QUEST_NONE;
					ChangeQuest(QUEST_SURVIVEWAVE, 0.5f);
				}
				else
				{
					ActivatePushForwardGroup();
					SpawnNewWave();
				}
			}
			else if(m_Quest == QUEST_DEFEND && ReactorsLeft() > 0)
			{
				// Prep window: radar on, no waves until players reach the reactor.
				m_DefendPrepEndTick = Server()->Tick() + Server()->TickSpeed() * 10;
				m_DefendEndTick = 0;
			}

			if(m_Quest == QUEST_KILL_BOSS)
			{
				SpawnBosses(max(1, m_BossesLeft));
				const int Level = max(0, g_Config.m_SvMapGenLevel);
				const int Players = max(1, CountHumans());
				m_QuestWaveSize = InvasionConcurrentCap(Level, Players, m_WaveSizeNerf);
				m_EnemiesLeft = InvasionEnemyBudget(INV_BUDGET_BOSS_MINIONS, Level, Players, m_LevelTheme,
													InvasionCountScale(GameServer()));
				RandomGroupSpawnPos();
				const int SpawnCount = min(m_EnemiesLeft, max(0, m_QuestWaveSize - CountBots()));
				for(int i = 0; i < SpawnCount; i++)
					GameServer()->AddBot();
			}

			if(m_Quest == QUEST_DEFEND)
			{
				if(!ReactorsLeft())
				{
					dbg_msg("inv", "defend started with no reactor, auto-complete");
					m_DefendPrepEndTick = 0;
					CompleteCurrentQuest();
				}
			}

			if(m_Quest == QUEST_ACTIVATE_SWITCHES)
			{
				const int Switches = SwitchesAvailable();
				if(Switches <= 0)
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

			if(m_Quest == QUEST_FIND_SWITCH && SwitchesAvailable() <= 0)
			{
				dbg_msg("inv", "escape switch missing, force acid climb");
				BeginRisingAcid(50);
				m_EscapeSpawnActive = true;
				CompleteCurrentQuest();
				ChangeQuest(QUEST_REACHDOOR, 0.5f);
				TriggerEscape();
			}

			if(m_Quest != QUEST_NONE)
			{
				ShowCartographerObjectives();
				SendQuestStartMessage(m_Quest);
			}
		}

		if(m_Quest == QUEST_NONE && m_NextQuest == QUEST_NONE)
		{
			if(m_EscapeLevel)
			{
				if(!m_EscapeSpawnActive)
					ChangeQuest(QUEST_FIND_SWITCH, 2.0f);
				else
					ChangeQuest(QUEST_REACHDOOR, 1.0f);
			}
			else if(m_QuestsCompleted >= m_LevelQuestsLeft)
				ChangeQuest(QUEST_REACHDOOR, INV_QUEST_DOOR_TIME);
			else if(m_QuestsCompleted == 0)
				StartThemeQuest();
			else
				QueueNextObjectiveQuest();
		}

		if(m_Quest == QUEST_SURVIVEWAVE || m_Quest == QUEST_SURVIVEWAVETIME)
		{
			const int AliveBots = CountBotsAlive() + CountAliveSpecialists(&GameServer()->m_World);
			if(m_Quest == QUEST_SURVIVEWAVETIME)
				m_QuestProgressCounter = int((m_QuestWaveEndTick - Server()->Tick()) / Server()->TickSpeed());
			else
				m_QuestProgressCounter = m_EnemiesLeft + AliveBots;

			if((m_QuestWaveEndTick && m_QuestWaveEndTick <= Server()->Tick()) || (m_EnemiesLeft <= 0 && AliveBots <= 0))
			{
				m_EnemiesLeft = 0;
				m_QuestWaveEndTick = 0;
				int CompletedQuest = m_Quest;
				CompleteCurrentQuest();

				if(CompletedQuest == QUEST_SURVIVEWAVETIME && AliveBots > 4)
					ChangeQuest(QUEST_KILLREMAININGENEMIES, INV_QUEST_QUEUE_TIME);
			}
		}

		if(m_Quest == QUEST_KILLREMAININGENEMIES)
		{
			// The old HUD counted only currently alive Bots, while the server could
			// still have enemies queued in m_EnemiesLeft. This let the counter show
			// zero before the purge encounter had reached a stable completion state.
			// Use one value for both rendering and completion.
			const int Remaining = max(0, m_EnemiesLeft) + CountBotsAlive() + CountAliveSpecialists(&GameServer()->m_World);
			m_QuestProgressCounter = Remaining;
			if(Remaining == 0)
				CompleteCurrentQuest();
		}

		if(m_Quest == QUEST_KILL_BOSS)
		{
			TickRegionalBoss();
			m_BossesLeft = CountBossesAlive();
			m_QuestProgressCounter = m_BossesLeft;
			if(m_BossesLeft <= 0)
			{
				if(GameServer()->m_pPveDirector)
					GameServer()->m_pPveDirector->OnBossKilled();
				m_EnemiesLeft = 0;
				CompleteCurrentQuest();
			}
		}

		if(m_Quest == QUEST_DEFEND)
		{
			if(m_DefendPrepEndTick)
			{
				m_QuestProgressCounter = max(0, (m_DefendPrepEndTick - Server()->Tick()) / Server()->TickSpeed());
				if(m_DefendPrepEndTick <= Server()->Tick())
				{
					m_DefendPrepEndTick = 0;
					m_DefendEndTick = Server()->Tick() +
									  Server()->TickSpeed() * InvasionReactorDefenseSeconds(g_Config.m_SvMapGenLevel);
					SpawnNewWave();
					// SpawnNewWave drains the enemy pool filling the concurrent cap.
					// Keep a reinforce budget so CanSpawn can admit replacements
					// for the rest of the defend timer.
					if(m_EnemiesLeft <= 0)
						m_EnemiesLeft = max(4, m_QuestWaveSize / 2);
					m_BotSpawnTick = Server()->Tick();
					GameServer()->SendBroadcast("Defend the reactor", -1);
				}
			}
			else
				m_QuestProgressCounter = max(0, (m_DefendEndTick - Server()->Tick()) / Server()->TickSpeed());

			if(!ReactorsLeft())
			{
				GameServer()->SendBroadcast("Reactor destroyed", -1);
				DeathMessage();
				m_RoundOverTick = Server()->Tick();
				SetReactorDefenseActive(false);
				m_DefendPrepEndTick = 0;
				m_Quest = QUEST_NONE;
			}
			else if(!m_DefendPrepEndTick && m_DefendEndTick && m_DefendEndTick <= Server()->Tick())
			{
				m_EnemiesLeft = 0;
				CompleteCurrentQuest();
			}
			else if(!m_DefendPrepEndTick && m_BotSpawnTick < Server()->Tick())
			{
				const int Level = max(0, g_Config.m_SvMapGenLevel);
				const int EarlyLevel = min(Level, 20);
				const int LateLevel = max(0, Level - 20);
				m_BotSpawnTick = Server()->Tick() +
					Server()->TickSpeed() * max(0.4f, 0.7f - EarlyLevel * 0.012f - LateLevel * 0.003f);
				if(CountBots() < m_QuestWaveSize)
				{
					// Infinite reinforce while the defend timer runs: CanSpawn
					// rejects bots when m_EnemiesLeft hits 0.
					if(m_EnemiesLeft <= 0)
						m_EnemiesLeft = 1;
					RandomGroupSpawnPos();
					GameServer()->AddBot();
				}
			}
		}

		if(m_Quest == QUEST_ACTIVATE_SWITCHES)
			m_QuestProgressCounter = max(0, m_SwitchesRequired - m_SwitchesActivated);

		if(m_Quest == QUEST_FIND_SWITCH)
			m_QuestProgressCounter = max(0, 1 - m_SwitchesActivated);

		if(m_Quest == QUEST_PUSH_FORWARD)
			TickPushForward();

		// After the switch: keep refreshing enemies until players reach the door.
		if(m_EscapeSpawnActive && m_Quest == QUEST_REACHDOOR && !m_RoundWin)
		{
			if(m_BotSpawnTick < Server()->Tick())
			{
				const int Level = max(0, g_Config.m_SvMapGenLevel);
				const int EarlyLevel = min(Level, 20);
				const int LateLevel = max(0, Level - 20);
				m_BotSpawnTick = Server()->Tick() +
					Server()->TickSpeed() * max(0.55f, 1.1f - EarlyLevel * 0.015f - LateLevel * 0.005f);
				if(CountBots() < m_QuestWaveSize)
				{
					RandomGroupSpawnPos();
					GameServer()->AddBot();
					if(m_EnemiesLeft > 0 && m_EnemiesLeft < 9000)
						m_EnemiesLeft--;
				}
			}
		}
	}

	if(m_GameState == STATE_STARTING)
	{
		if(CountHumans() > 0)
		{
			if(!m_RogueliteWaitTick)
				m_RogueliteWaitTick = Server()->Tick() + Server()->TickSpeed() * 2;
			if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->Enabled() &&
			   !GameServer()->m_pPveDirector->ProgressReady() && Server()->Tick() < m_RogueliteWaitTick)
				return;
			if(!m_CheckpointApplied)
			{
				m_CheckpointApplied = true;
				if(GameServer()->m_pPveDirector && GameServer()->m_pPveDirector->Enabled() &&
				   g_Config.m_SvInvasionUseCheckpoint && g_Config.m_SvMapGenLevel == 1)
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
					const bool ContractVote = g_Config.m_SvMapGenLevel % 3 == 0;
					GameServer()->m_pPveDirector->StartIntermission(ContractVote, true);
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
			if(!m_ProgressSynced)
			{
				SetupLevelTheme();
				m_ProgressSynced = true;
			}

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "start round theme=%d enemies='%d'", m_LevelTheme, m_EnemiesLeft);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "inv", aBuf);

			if(!m_StartBriefingSent)
			{
				m_StartBriefingSent = true;
				if(g_Config.m_SvInvFails > 0 && g_Config.m_SvInvFails < 5 &&
				   g_Config.m_SvInvFails != INV_FORCE_FLOOR_ONE)
					GameServer()->SendBroadcastFormat(-1,
													  false,
													  "Level %d - %s · Attempt %d/5",
													  g_Config.m_SvMapGenLevel,
													  GetThemeDisplayName(m_LevelTheme),
													  g_Config.m_SvInvFails + 1);
				else
					GameServer()->SendBroadcastFormat(
						-1, false, "Level %d - %s", g_Config.m_SvMapGenLevel, GetThemeDisplayName(m_LevelTheme));
			}

			m_TriggerTick = 0;
			m_AutoRestart = true;

			if(GameServer()->m_pPveDirector &&
			   GameServer()->m_pPveDirector->ActiveContract() == PVE_CONTRACT_ELITE_HUNT)
			{
				vec2 Pos;
				if(!GetBossSpawnPos(&Pos))
					Pos = vec2(4000, 4000);
				CDroid *pBoss = SpawnBoss(&GameServer()->m_World, Pos, g_Config.m_SvMapGenLevel + 2);
				GameServer()->m_pPveDirector->RegisterEliteContractBoss(pBoss);
				m_EliteContractSpawned = true;
			}
			if(m_LevelTheme != INVASION_THEME_TIMED_SURVIVE)
			{
				const int SpawnCount = min(m_EnemiesLeft, max(0, m_QuestWaveSize - CountBots()));
				for(int i = 0; i < SpawnCount; i++)
					GameServer()->AddBot();
			}
			ShowCartographerObjectives();
			m_GameState = STATE_GAME;
		}
		else if((m_AutoRestart || g_Config.m_SvMapGenLevel > 1) && Server()->Tick() > Server()->TickSpeed() * 60.0f)
		{
			m_AutoRestart = false;

			if(g_Config.m_SvMapGenRandSeed)
			{
				g_Config.m_SvMapGenSeed = (int)((unsigned long long)time_get() % 0x7FFFFFFFull);
				if(g_Config.m_SvMapGenSeed <= 0)
					g_Config.m_SvMapGenSeed = 1;
			}

			FirstMap();
		}
	}
	else
	{
		if(g_Config.m_SvMapGenLevel > 1)
			m_AutoRestart = true;

		if(m_RoundOverTick && m_RoundOverTick < Server()->Tick() - Server()->TickSpeed() * 2.0f)
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
				GameServer()->SendBroadcastFormat(
					-1, false, "Failure %d/5 — retrying this floor", g_Config.m_SvInvFails);
				GameServer()->ReloadMap();
			}
		}
	}

	GameServer()->UpdateAI();

	if(m_TriggerTick < Server()->Tick())
	{
		Trigger(true);
		m_TriggerTick = Server()->Tick() + Server()->TickSpeed() * 4;
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer)
			continue;

		if(pPlayer->m_IsBot && pPlayer->m_ToBeKicked)
			GameServer()->KickBot(pPlayer->GetCID());
	}

	if(m_RoundWin)
	{
		if(m_RoundWinTick < Server()->Tick())
		{
			const int CompletedLevel = g_Config.m_SvMapGenLevel;
			if(!m_RogueliteCompletionStarted)
			{
				m_RogueliteCompletionStarted = true;
				if(GameServer()->m_pPveDirector)
				{
					GameServer()->m_pPveDirector->OnStageComplete(true);
					GameServer()->m_pPveDirector->RewardResearch(
						CompletedLevel % 2 == 0 ? 1 : 0, PVE_REWARD_INVASION_DEPTH, CompletedLevel);
				}

				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					CPlayer *pPlayer = GameServer()->m_apPlayers[i];
					if(!pPlayer || pPlayer->m_IsBot)
						continue;
					Server()->SendPlatformEvent(i, PLATFORM_EVENT_FIRST_INVASION);
					if(CompletedLevel >= 10)
						Server()->SendPlatformEvent(i, PLATFORM_EVENT_INVASION_10);
					if(CompletedLevel >= 30)
						Server()->SendPlatformEvent(i, PLATFORM_EVENT_INVASION_30);
					if(CompletedLevel >= 60)
						Server()->SendPlatformEvent(i, PLATFORM_EVENT_INVASION_60);
					Server()->SendPlatformEvent(i, PLATFORM_EVENT_LB_INVASION_FLOOR, CompletedLevel);
					Server()->SendPlatformEvent(i, PLATFORM_EVENT_FIRST_COOP_COMPLETE);
					Server()->SendPlatformEvent(i, PLATFORM_EVENT_STAT_COOP_COMPLETIONS, 1);
					pPlayer->IncreaseGold(10 + CompletedLevel / 3);
				}
				Server()->DispatchModEvent(MOD_EVENT_PVE_FLOOR_COMPLETE, -1, CompletedLevel);
				GameServer()->DispatchChallengeEvent(EChallengeScriptEvent::FloorComplete, -1, CompletedLevel);

				// The next floor offers its perk after the new map and client state
				// are ready, avoiding a selection crossing the map-load boundary.
			}

			m_RoundWin = false;
			m_RoundWinTick = 0;
			m_RunBuffActive = false;
			g_Config.m_SvMapGenLevel++;
			g_Config.m_SvInvFails = 0;

			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if(pPlayer)
					pPlayer->SaveData();
			}
			GameServer()->WriteExpeditionSave();

			EndRound();
		}
	}
}

void CGameControllerInvasion::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameData *pGameDataObj =
		(CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
	if(!pGameDataObj)
		return;

	pGameDataObj->m_TeamscoreRed = m_Quest;
	pGameDataObj->m_TeamscoreBlue = m_QuestProgressCounter;
	// Coop HUD packs level/theme/wave/quest progress (flag carriers unused in coop).
	pGameDataObj->m_FlagCarrierRed = g_Config.m_SvMapGenLevel;
	pGameDataObj->m_FlagCarrierBlue =
		m_LevelTheme | (m_QuestWaveType << 4) | (m_QuestsCompleted << 8) | (m_LevelQuestsLeft << 12);
}
