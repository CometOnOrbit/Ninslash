#ifndef GAME_SERVER_GAMEMODES_HORDE_H
#define GAME_SERVER_GAMEMODES_HORDE_H
#include <game/server/gamecontroller.h>

#define MAX_ENEMIES 512

class CGameControllerHorde : public IGameController
{
  private:
	vec2 m_aEnemySpawnPos[MAX_ENEMIES];

	int m_Wave;
	int m_WaveStartTick;
	int m_Deaths;
	int m_Kills;
	int m_RoundOverTick;
	int m_NoPlayersTick;
	bool m_GameOverBroadcast;

	int m_EnemyCount;
	int m_EnemiesLeft;
	int m_NumEnemySpawnPos;
	int m_SpawnPosRotation;
	int m_TriggerTick;
	int m_TriggerLevel;
	bool m_RogueliteStarted;
	int m_RogueliteWaitTick;
	int m_LastIntermissionWave;
	int m_LastContractProgressWave;
	bool m_EliteContractSpawned;
	int m_BossCountCacheTick;
	int m_BossCountCache;
	vec2 m_DefenseAreaCenter;
	bool m_DefenseAreaReady;

	enum EHordeEvent
	{
		HORDE_EVENT_NONE = 0,
		HORDE_EVENT_CRAWLER_PACK,
		HORDE_EVENT_SUPPLY_DROP,
		HORDE_EVENT_BREATHER,
		HORDE_EVENT_RABBLE,
		HORDE_EVENT_REINFORCEMENTS,
		HORDE_EVENT_ROGUE_BOSS,
		HORDE_EVENT_FIELD_RATIONS,
		HORDE_EVENT_FORTIFY_DROP,
		HORDE_EVENT_HARDENED_SQUAD,
		HORDE_EVENT_CRAWLER_NEST,
	};

	int m_ActiveEvent;
	int m_LastEventWave;
	int m_EventActionTick;
	int m_EventWaveBudget;
	float m_EventCountMod;
	int m_EventLevelMod;

	void NextWave();
	int EnemyLevel() const;
	int AliveBossCount();
	int CountHumansAlive(int ExcludeCID = -1) const;
	bool GetBossSpawnPos(vec2 *pOutPos);
	void EnsureDefenseArea(vec2 FallbackPos);
	void ClearWaveEvent();
	void RollWaveEvent();
	void ApplyWaveEventSpawns();
	void TickWaveEvent();
	bool HeavyContractActive() const;
	bool BossContractActive() const;

  public:
	CGameControllerHorde(class CGameContext *pGameServer);
	virtual ~CGameControllerHorde();

	virtual bool OnEntity(int Index, vec2 Pos);
	void OnCharacterSpawn(class CCharacter *pChr, bool RequestAI = false);
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source);
	bool GetSpawnPos(int Team, vec2 *pOutPos);
	virtual bool CanSpawn(int Team, vec2 *pPos, bool IsBot = false);
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	bool InDefenseArea(vec2 Pos) const;

	enum GameState
	{
		STATE_STARTING,
		STATE_GAME,
	};
};
#endif
