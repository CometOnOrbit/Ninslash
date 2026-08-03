#ifndef GAME_SERVER_GAMEMODES_EXTRACT_H
#define GAME_SERVER_GAMEMODES_EXTRACT_H
#include <game/server/gamecontroller.h>

#define MAX_ENEMIES 512

class CGameControllerExtract : public IGameController
{
  private:
	vec2 m_aEnemySpawnPos[MAX_ENEMIES];

	int m_Phase; // 0 fight/switches, 1 evacuate, 2 won/lost
	int m_SwitchesRequired;
	int m_SwitchesActivated;
	int m_AvailableSwitches;
	int m_Evacuated;
	int m_EvacNeeded;
	bool m_DoorOpen;
	bool m_MidBossSpawned;
	int m_RoundOverTick;
	int m_DeadlineTick;
	int m_StartTick;
	int m_EnemiesLeft;
	int m_NumEnemySpawnPos;
	int m_SpawnPosRotation;
	int m_BotSpawnTick;
	int m_TriggerTick;
	int m_TriggerLevel;
	bool m_Win;
	bool m_EscapePressure;
	bool m_HadHumanAlive;
	int m_HumanDeaths;
	// Task completion tracked for the exit rating; populated by the task pool
	// (Step 2). Until then total == 0 means "all tasks done".
	int m_TasksCompleted;
	int m_TasksTotal;
	bool m_RogueliteStarted;
	int m_RogueliteWaitTick;
	bool m_RogueliteStageStarted;
	bool m_MidBossPerkOffered;
	bool m_DoorChoicePending;
	bool m_DoorChoiceStarted;
	bool m_EliteContractSpawned;
	class CDroid *m_pMidBoss;
	class CServerRadar *m_pDoor;

	void SpawnInitialEnemies();
	void SpawnMidBoss();
	void SpawnEscapePressure();
	void BeginEvacuation();
	int CountHumanPlayersLocal() const;
	int CountHumansAliveLocal() const;
	int EnemyLevel() const;
	bool GetBossSpawnPos(vec2 *pOutPos);
	// Exit rating: 0=C, 1=B, 2=A, 3=S (see docs/extraction_redesign.md §7.1).
	int ComputeRating() const;

	enum
	{
		MAX_EXTRACT_TASKS = 3,
	};
	enum EExtractTaskType
	{
		EXTRACT_TASK_NONE = 0,
		EXTRACT_TASK_SWITCHES,
		EXTRACT_TASK_ELIMINATE,
		EXTRACT_TASK_DEFEND,
		EXTRACT_TASK_COLLECT,
		EXTRACT_TASK_TIMED_CLEAR,
	};
	int m_ActiveTask; // -1 = none
	int m_TaskCount;
	int m_aTaskType[MAX_EXTRACT_TASKS];
	int m_aTaskProgress[MAX_EXTRACT_TASKS];
	int m_aTaskTarget[MAX_EXTRACT_TASKS];
	vec2 m_aTaskZone[MAX_EXTRACT_TASKS];
	int m_aTaskZones[MAX_EXTRACT_TASKS]; // index into m_aTaskZone, -1 = none
	int m_TaskZonesCollected;
	int m_TaskTimerTick;
	enum EExtractEvent
	{
		EXTRACT_EVT_NONE = 0,
		EXTRACT_EVT_REINFORCEMENTS,
		EXTRACT_EVT_ELITE_AMBUSH,
		EXTRACT_EVT_SUPPLY_DROP,
		EXTRACT_EVT_TRAP_ZONE,
		EXTRACT_EVT_BOSS_RUSH,
	};
	int m_ActiveEvent;
	int m_EventActionTick;
	int m_LastEventTask;
	// Radar guiding players to the active task's zone anchor.
	class CServerRadar *m_pTaskRadar;
	// Evacuation-stage pressure (docs/extraction_redesign.md §6.1/§6.2).
	int m_EscapeWave;
	int m_EscapeWaveTick;
	int m_ReinforceTick;
	bool PlayerInRadius(const vec2 &Center, float Radius) const;
	void PickTasks();
	void StartTask();
	void CompleteTask(int Index);
	void UpdateTask();
	void PickEvent();
	void RunEvent();

  public:
	CGameControllerExtract(class CGameContext *pGameServer);
	virtual ~CGameControllerExtract();

	virtual bool OnEntity(int Index, vec2 Pos);
	void OnCharacterSpawn(class CCharacter *pChr, bool RequestAI = false);
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source);
	bool GetSpawnPos(int Team, vec2 *pOutPos);
	virtual bool CanSpawn(int Team, vec2 *pPos, bool IsBot = false);
	virtual void OnSwitchTriggered();
	void OnDroidKilled(class CDroid *pDroid);
	virtual void NextLevel(int CID = -1);
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void DisplayExit(vec2 Pos);
	bool Evacuating() const { return m_Phase == 1 && !m_RoundOverTick; }

	enum GameState
	{
		STATE_STARTING,
		STATE_GAME,
	};
};
#endif
