#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/questinfo.h>
#include <game/weapons.h>

#include <game/server/entities/character.h>
#include <game/server/entities/building.h>
#include <game/server/entities/droid.h>
#include <game/server/bosspool.h>
#include <game/server/entities/radar.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/gameworld.h>

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

static int InvasionDepthQuests(int Level)
{
	if (Level >= 21)
		return min(2 + Level/12, 3);
	return 2;
}

static int InvasionOpeningEnemies(int Level)
{
	return min(18, 6 + Level);
}

static int InvasionWaveCap(int Level, int Players)
{
	return min(28, 10 + Level/2 + Players);
}

CGameControllerInvasion::CGameControllerInvasion(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pGameType = "INV";
	m_GameFlags = GAMEFLAG_COOP;
	m_GameState = STATE_STARTING;
	
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
	m_SwitchesRequired = 0;
	m_SwitchesActivated = 0;
	m_BossesLeft = 0;
	m_DefendLevel = false;
	m_SwitchCoopLevel = false;
	m_ForcedWaveType = WAVE_NONE;
	m_WaveSizeNerf = 0;
	m_RunBuffActive = false;
	m_ProgressSynced = false;
	m_StartBriefingSent = false;
	
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
	}
	else
	{
		SpawnNewWave(false);
		m_EnemiesLeft = InvasionOpeningEnemies(Level);
		m_QuestWaveSize = InvasionWaveCap(Level, max(1, CountPlayers(0)));
		m_Deaths = m_QuestWaveSize;
	}

	if (g_Config.m_SvInvFails >= 2)
	{
		m_RunBuffActive = true;
		m_WaveSizeNerf = 1;
	}
	else if (g_Config.m_SvInvFails >= 1)
		m_WaveSizeNerf = 1;
}


bool CGameControllerInvasion::OnEntity(int Index, vec2 Pos)
{
	if (IGameController::OnEntity(Index, Pos))
		return true;

	if (Index == ENTITY_ENEMYSPAWN && m_NumEnemySpawnPos < MAX_ENEMIES)
	{
		m_aEnemySpawnPos[m_NumEnemySpawnPos++] = Pos;
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


void CGameControllerInvasion::ApplyMetaUnlocks(CCharacter *pChr)
{
	if (!pChr || pChr->m_IsBot)
		return;

	CPlayer *pPlayer = pChr->GetPlayer();
	if (!pPlayer)
		return;

	CPlayerData *pData = GameServer()->Server()->GetPlayerData(pPlayer->GetCID(), pPlayer->GetColorID());
	if (!pData)
		return;

	if (pData->m_UnlockFlags & UNLOCK_EXTRA_KITS)
		pChr->m_Kits = max(pChr->m_Kits, 8);
	if ((pData->m_UnlockFlags & UNLOCK_DEFEND_BONUS) && m_DefendLevel)
		pChr->m_Kits = max(pChr->m_Kits, 15);
	if (pData->m_UnlockFlags & UNLOCK_WEAPON_TIER1)
		pChr->SetArmor(max(pChr->GetArmor(), 5));
	if (pData->m_UnlockFlags & UNLOCK_WEAPON_TIER2)
		pChr->SetArmor(max(pChr->GetArmor(), 10));
	if (m_RunBuffActive)
		pChr->m_Kits = max(pChr->m_Kits, 6);
}


void CGameControllerInvasion::SendUnlockBroadcast(CPlayerData *pData, int NewFlags)
{
	if (!pData)
		return;
	if (NewFlags & UNLOCK_EXTRA_KITS)
		GameServer()->SendBroadcast("Milestone: extra kits unlocked", -1);
	if (NewFlags & UNLOCK_WEAPON_TIER1)
		GameServer()->SendBroadcast("Milestone: armor tier 1 unlocked", -1);
	if (NewFlags & UNLOCK_DEFEND_BONUS)
		GameServer()->SendBroadcast("Milestone: defend bonus unlocked", -1);
	if (NewFlags & UNLOCK_WEAPON_TIER2)
		GameServer()->SendBroadcast("Milestone: armor tier 2 unlocked", -1);
	if (NewFlags & UNLOCK_GOLD_BONUS)
		GameServer()->SendBroadcast("Milestone: gold bonus unlocked", -1);
	if (NewFlags & UNLOCK_SHOP_TIER)
		GameServer()->SendBroadcast("Milestone: shop tier unlocked", -1);
}


void CGameControllerInvasion::SyncProgressLevel()
{
	int TotalLevel = 0;
	int PlayerCount = 0;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if (!pPlayer || pPlayer->m_IsBot)
			continue;
		CPlayerData *pData = GameServer()->Server()->GetPlayerData(i, pPlayer->GetColorID());
		if (!pData)
			continue;
		TotalLevel += max(1, pData->m_HighestLevel);
		PlayerCount++;
	}
	if (PlayerCount > 0)
	{
		int Suggested = max(1, TotalLevel / PlayerCount);
		if (g_Config.m_SvMapGenLevel < Suggested)
			g_Config.m_SvMapGenLevel = Suggested;
	}
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
		CPlayerData *pData = GameServer()->Server()->GetPlayerData(i, pPlayer->GetColorID());
		int Bonus = 0;
		if (pData && (pData->m_UnlockFlags & UNLOCK_GOLD_BONUS))
			Bonus = Gold/2;
		pPlayer->IncreaseGold(Gold + Bonus);
	}
}


void CGameControllerInvasion::GrantMetaUnlocks()
{
	int Level = g_Config.m_SvMapGenLevel;
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if (!pPlayer || pPlayer->m_IsBot)
			continue;

		CPlayerData *pData = GameServer()->Server()->GetPlayerData(i, pPlayer->GetColorID());
		if (!pData)
			continue;

		if (Level > pData->m_HighestLevel)
		{
			pData->m_HighestLevel = Level;
			pData->m_HighestLevelSeed = g_Config.m_SvMapGenSeed;
		}

		int OldFlags = pData->m_UnlockFlags;
		if (pData->m_HighestLevel >= 10)
			pData->m_UnlockFlags |= UNLOCK_EXTRA_KITS;
		if (pData->m_HighestLevel >= 20)
			pData->m_UnlockFlags |= UNLOCK_WEAPON_TIER1;
		if (pData->m_HighestLevel >= 30)
			pData->m_UnlockFlags |= UNLOCK_DEFEND_BONUS;
		if (pData->m_HighestLevel >= 40)
			pData->m_UnlockFlags |= UNLOCK_WEAPON_TIER2;
		if (pData->m_HighestLevel >= 50)
			pData->m_UnlockFlags |= UNLOCK_GOLD_BONUS;
		if (pData->m_HighestLevel >= 60)
			pData->m_UnlockFlags |= UNLOCK_SHOP_TIER;

		int NewFlags = pData->m_UnlockFlags & ~OldFlags;
		if (NewFlags)
			SendUnlockBroadcast(pData, NewFlags);
	}
}


void CGameControllerInvasion::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);

	if (!RequestAI)
	{
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
	if (m_LevelTheme == INVASION_THEME_ELITE_WAVE && m_QuestWaveType < WAVE_CYBORGS && frandom() < 0.45f)
		m_QuestWaveType = WAVE_CYBORGS;
	
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
		
		for (int i = 0; i < m_EnemiesLeft && CountBots() < m_QuestWaveSize; i++)
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
		if (!GetSpawnPos(0, &p))
			p = vec2(4000, 4000);
		SpawnBoss(&GameServer()->m_World, p+vec2(0, -100), g_Config.m_SvMapGenLevel);
	}
	m_BossesLeft = Count;
}


int CGameControllerInvasion::CountBossesAlive() const
{
	return CountAliveBosses(&GameServer()->m_World);
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


int CGameControllerInvasion::ReactorsLeft() const
{
	return CountBuildingsOfType(BUILDING_REACTOR);
}


int CGameControllerInvasion::SwitchesAvailable() const
{
	return CountBuildingsOfType(BUILDING_SWITCH);
}


int CGameControllerInvasion::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);

	if (pVictim->m_IsBot && !pVictim->GetPlayer()->m_ToBeKicked)
	{
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
			m_RoundOverTick = Server()->Tick();
		}
	}

	return 0;
}



void CGameControllerInvasion::NextLevel(int CID)
{
	if (!m_RoundWin)
	{
		m_RoundWin = true;
		m_RoundWinTick = Server()->Tick() + Server()->TickSpeed()*CountHumans()*1;
		
		if (CountHumans() > 1)
			GameServer()->SendBroadcastFormat(-1, false, "%s reached the door", Server()->ClientName(CID));
	}
	
	
	CPlayer *pPlayer = GameServer()->m_apPlayers[CID];
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
		GameServer()->SendBroadcast("Defend the reactor", -1);
	else
		GameServer()->SendBroadcast(GetQuestStartMessage(Quest, m_QuestWaveType), -1);
}


void CGameControllerInvasion::SendQuestCompletedMessage(int Quest)
{
	GameServer()->SendBroadcast(GetQuestCompletedMessage(Quest, m_QuestWaveType), -1);
}


void CGameControllerInvasion::CompleteCurrentQuest()
{
	SendQuestCompletedMessage(m_Quest);
	RewardQuestGold();
	m_Quest = QUEST_NONE;
	m_NextQuest = QUEST_NONE;
	m_QuestsCompleted++;
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


void CGameControllerInvasion::OnSwitchTriggered()
{
	m_SwitchesActivated++;

	if (m_SwitchCoopLevel)
	{
		m_QuestProgressCounter = max(0, m_SwitchesRequired - m_SwitchesActivated);
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
	
	if (m_GameState == STATE_FAIL)
		return;
	
	if (m_GameState == STATE_GAME)
	{
		// Wipe only after someone has already died this round (SURVIVAL_NOCANDO).
		// At join/spawn, humans exist but aren't alive yet — don't end the round.
		if (g_Config.m_SvSurvivalMode && !m_RoundOverTick
			&& m_SurvivalStatus == SURVIVAL_NOCANDO
			&& CountHumans() > 0 && CountHumansAlive() <= 0)
		{
			DeathMessage();
			m_RoundOverTick = Server()->Tick();
		}

		if (m_QuestChangeTick && m_QuestChangeTick <= Server()->Tick())
		{
			m_Quest = m_NextQuest;
			m_NextQuest = QUEST_NONE;
			m_QuestChangeTick = 0;
			m_QuestProgressCounter = 0;
			
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
				SpawnNewWave();

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
					CompleteCurrentQuest();
				}
				else
					m_DefendEndTick = Server()->Tick() + Server()->TickSpeed() * (40 + g_Config.m_SvMapGenLevel);
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
			if (m_Quest == QUEST_SURVIVEWAVETIME)
				m_QuestProgressCounter = int((m_QuestWaveEndTick - Server()->Tick()) / Server()->TickSpeed());
			else
				m_QuestProgressCounter = m_EnemiesLeft + CountBotsAlive();
			
			if ((m_QuestWaveEndTick && m_QuestWaveEndTick <= Server()->Tick()) || (m_EnemiesLeft <= 0 && CountBotsAlive() <= 0))
			{
				m_EnemiesLeft = 0;
				m_QuestWaveEndTick = 0;
				int CompletedQuest = m_Quest;
				CompleteCurrentQuest();
				
				if (CompletedQuest == QUEST_SURVIVEWAVETIME && CountBotsAlive() > 4)
					ChangeQuest(QUEST_KILLREMAININGENEMIES, INV_QUEST_QUEUE_TIME);
			}
		}
		
		if (m_Quest == QUEST_KILLREMAININGENEMIES)
		{
			m_QuestProgressCounter = CountBotsAlive();
			if (m_QuestProgressCounter <= 0)
				CompleteCurrentQuest();
		}

		if (m_Quest == QUEST_KILL_BOSS)
		{
			m_BossesLeft = CountBossesAlive();
			m_QuestProgressCounter = m_BossesLeft;
			if (m_BossesLeft <= 0)
			{
				m_EnemiesLeft = 0;
				CompleteCurrentQuest();
			}
		}

		if (m_Quest == QUEST_DEFEND)
		{
			m_QuestProgressCounter = max(0, (m_DefendEndTick - Server()->Tick()) / Server()->TickSpeed());

			if (!ReactorsLeft())
			{
				GameServer()->SendBroadcast("Reactor destroyed", -1);
				DeathMessage();
				m_RoundOverTick = Server()->Tick();
				m_Quest = QUEST_NONE;
			}
			else if (m_DefendEndTick && m_DefendEndTick <= Server()->Tick())
			{
				m_EnemiesLeft = 0;
				CompleteCurrentQuest();
			}
			else if (m_BotSpawnTick < Server()->Tick())
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
			if (!m_ProgressSynced)
			{
				SyncProgressLevel();
				SetupLevelTheme();
				m_ProgressSynced = true;
			}

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "start round theme=%d enemies='%u'", m_LevelTheme, m_Deaths);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "inv", aBuf);

			if (!m_StartBriefingSent)
			{
				m_StartBriefingSent = true;
				GameServer()->SendBroadcastFormat(-1, false, "Level %d - %s", g_Config.m_SvMapGenLevel, GetThemeDisplayName(m_LevelTheme));
			}

			m_TriggerTick = 0;
			m_AutoRestart = true;

			m_GameState = STATE_GAME;
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
			if (++g_Config.m_SvInvFails >= 3)
			{
				g_Config.m_SvInvFails = 0;
				m_RunBuffActive = true;
				m_WaveSizeNerf = 1;
				
				if (--g_Config.m_SvMapGenLevel < 1)
					g_Config.m_SvMapGenLevel = 1;
				
				g_Config.m_SvMapGenSeed = rand()%32767;
				g_Config.m_SvInvFails = 0;
				m_GameState = STATE_FAIL;
				EndRound();
			}
			else
				GameServer()->ReloadMap();
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
			m_RoundWin = false;
			m_RoundWinTick = 0;
			m_RunBuffActive = false;
			const int CompletedLevel = g_Config.m_SvMapGenLevel;
			g_Config.m_SvMapGenLevel++;
			g_Config.m_SvInvFails = 0;

			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if (!pPlayer || pPlayer->m_IsBot)
					continue;
				pPlayer->IncreaseGold(10 + CompletedLevel/3);
			}

			GrantMetaUnlocks();
			
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
	pGameDataObj->m_FlagCarrierRed = g_Config.m_SvMapGenLevel;
	pGameDataObj->m_FlagCarrierBlue = m_LevelTheme | (m_QuestWaveType << 4) | (m_QuestsCompleted << 8) | (m_LevelQuestsLeft << 12);
}
