#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/questinfo.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include <game/server/bosspool.h>
#include <game/server/entities/droid_crawler.h>
#include <game/server/ai/base_ai.h>
#include <game/server/pve_bots.h>
#include <game/weapons.h>

#include "horde.h"

CGameControllerHorde::CGameControllerHorde(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
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

	g_Config.m_SvOneHitKill = 0;
	g_Config.m_SvWarmup = 0;
	g_Config.m_SvScorelimit = 0;
	g_Config.m_SvEnableBuilding = 1;
	g_Config.m_SvDisablePVP = 1;
	g_Config.m_SvSurvivalTime = 0;
	g_Config.m_SvSurvivalAcid = 0;
	// keep map as-is; optional one-shot mapgen via cfg, never level++

	if(g_Config.m_SvEnableBuilding)
		m_GameFlags |= GAMEFLAG_BUILD;
	if(g_Config.m_SvSurvivalMode)
		m_GameFlags |= GAMEFLAG_SURVIVAL;
}

bool CGameControllerHorde::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_ENEMYSPAWN)
	{
		if(m_NumEnemySpawnPos < MAX_ENEMIES)
			m_aEnemySpawnPos[m_NumEnemySpawnPos++] = Pos;
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

void CGameControllerHorde::OnCharacterSpawn(CCharacter *pChr, bool RequestAI)
{
	IGameController::OnCharacterSpawn(pChr);

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

int CGameControllerHorde::EnemyLevel() const
{
	// Wave 1 ~3–4, then climbs; caps so late waves stay tough but readable
	return min(14, max(3, 2 + m_Wave + (m_Wave / 3)));
}

int CGameControllerHorde::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);

	if(pVictim->m_IsBot)
	{
		if(!pVictim->GetPlayer()->m_ToBeKicked)
			m_Deaths = max(0, m_Deaths - 1);
		if(pKiller && !pKiller->m_IsBot)
			m_Kills++;
		pVictim->GetPlayer()->m_ToBeKicked = true;
	}
	else if(g_Config.m_SvSurvivalMode >= 2 && CountPlayersAlive(-1, true) <= 0)
	{
		DeathMessage();
		m_RoundOverTick = Server()->Tick();
	}
	else if(!pVictim->m_IsBot)
		pVictim->GetPlayer()->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvRespawnDelay;

	return 0;
}

void CGameControllerHorde::NextWave()
{
	m_WaveStartTick = 0;
	m_Wave++;
	m_TriggerLevel = 8 + m_Wave;

	GameServer()->SendBroadcastFormat(-1, false, "Wave %d", m_Wave);

	m_EnemiesLeft = min(10 + m_Wave * 3, 48);
	m_Deaths = m_EnemiesLeft;

	if(m_Wave % 2 == 0)
	{
		vec2 p;
		if(GetSpawnPos(0, &p))
			new CCrawler(&GameServer()->m_World, p + vec2(0, -100));
		if(m_Wave >= 6 && GetSpawnPos(0, &p))
			new CCrawler(&GameServer()->m_World, p + vec2(0, -100));
	}

	if(m_Wave > 0 && m_Wave % 4 == 0)
	{
		vec2 p;
		if(!GetSpawnPos(0, &p))
			p = vec2(4000, 4000);
		SpawnBoss(&GameServer()->m_World, p + vec2(0, -100), max(5, m_Wave));
		GameServer()->SendBroadcast("Boss incoming!", -1);
	}

	const int Cap = min(18, 12 + m_Wave / 2);
	for(int i = 0; i < m_EnemiesLeft && CountBots() < Cap; i++)
		GameServer()->AddBot();

	TriggerAllBotAI(GameServer(), m_TriggerLevel);
}

void CGameControllerHorde::Tick()
{
	IGameController::Tick();

	if(m_Wave > 0 && !m_NoPlayersTick && CountHumans() <= 0)
		m_NoPlayersTick = Server()->Tick() + Server()->TickSpeed() * 10.0f;

	if(m_NoPlayersTick && m_NoPlayersTick < Server()->Tick())
	{
		m_NoPlayersTick = 0;
		m_Wave = 0;
		EndRound();
	}

	if(m_GameState == STATE_STARTING)
	{
		if(CountPlayers(0) > 0 && !m_WaveStartTick)
		{
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

		if(!m_RoundOverTick && m_Deaths <= 0 && !CountBotsAlive() && CountAliveBosses(&GameServer()->m_World) <= 0
			&& CountPlayersAlive(-1, true) > 0 && !m_WaveStartTick)
		{
			if(g_Config.m_SvScorelimit > 0 && m_Wave >= g_Config.m_SvScorelimit)
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
	pGameDataObj->m_TeamscoreBlue = m_Deaths + CountAliveBosses(&GameServer()->m_World);
	pGameDataObj->m_FlagCarrierRed = m_Wave;
	pGameDataObj->m_FlagCarrierBlue = m_Kills;
}
