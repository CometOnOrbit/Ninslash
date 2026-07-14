#ifndef GAME_SERVER_GAMEMODES_INVASION_H
#define GAME_SERVER_GAMEMODES_INVASION_H
#include <game/server/gamecontroller.h>

#define MAX_ENEMIES 512

class CGameControllerInvasion : public IGameController
{
private:
	int m_LevelQuestsLeft;
	int m_QuestsCompleted;
	int m_LevelTheme;

	int m_Quest;
	int m_NextQuest;
	int m_QuestChangeTick;
	int m_QuestProgressCounter;
	
	int m_QuestWaveType;
	int m_QuestWaveEndTick;
	int m_QuestWaveEnemiesLeft;
	int m_QuestWaveSize;

	bool m_EliteWave;
	int m_DefendEndTick;
	int m_SwitchesRequired;
	int m_SwitchesActivated;
	
	void ChangeQuest(int NextQuest, float QueueTimeInSeconds);
	void SendQuestStartMessage(int Quest);
	void SendQuestCompletedMessage(int Quest);
	void CompleteCurrentQuest();
	void SetupLevelTheme();
	void StartThemeQuest();
	void QueueNextObjectiveQuest();
	void SpawnEliteContractGuard();
	void SpawnBosses(int Count);
	int CountBossesAlive() const;
	int CountBuildingsOfType(int Type) const;
	int ReactorsLeft() const;
	int SwitchesAvailable() const;
	void SetSwitchesActive(bool Active);
	int CountHumansAlive(int ExcludeCID = -1) const;
	void RewardQuestGold();

	vec2 m_aEnemySpawnPos[MAX_ENEMIES];
	
	int m_Deaths;
	bool m_RoundWin;
	int m_RoundWinTick;
	int m_RoundOverTick;
	
	// enemy grouping
	vec2 m_GroupSpawnPos;
	
	void SpawnNewWave(bool AddBots = true);
	
	vec2 GetBotSpawnPos();
	void RandomGroupSpawnPos();
	int m_BotSpawnTick;
	
	// hordes of enemies
	int m_EnemyCount;
	int m_EnemiesLeft;
	
	int m_BossesLeft;
	
	int m_NumEnemySpawnPos;
	int m_SpawnPosRotation;
	
	int m_TriggerLevel;
	int m_TriggerTick;

	bool m_EscapeLevel;
	bool m_EscapeSpawnActive;
	bool m_DefendLevel;
	bool m_SwitchCoopLevel;
	
	int m_ForcedWaveType;
	int m_WaveSizeNerf;
	bool m_RunBuffActive;
	bool m_ProgressSynced;
	int m_RogueliteWaitTick;
	bool m_StartBriefingSent;
	bool m_RogueliteOpeningStarted;
	bool m_RogueliteStageStarted;
	bool m_RogueliteCompletionStarted;
	bool m_EliteContractSpawned;
	bool m_CheckpointApplied;
	
	bool m_AutoRestart;
	
	void Trigger(bool IncreaseLevel);
	
	class CRadar *m_pDoor;
	class CRadar *m_pEnemySpawn;
	
public:
	CGameControllerInvasion(class CGameContext *pGameServer);
	
	virtual bool OnEntity(int Index, vec2 Pos);
	void OnCharacterSpawn(class CCharacter *pChr, bool RequestAI = false);
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	bool CanSpawn(int Team, vec2 *pPos, bool IsBot = false);
	void NextLevel(int CID = -1);
	bool GetSpawnPos(int Team, vec2 *pOutPos);
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void OnSwitchTriggered();
	
	void DisplayExit(vec2 Pos);
	
	bool RunBuffActive() const { return m_RunBuffActive; }
	bool IsObjectiveTarget(bool Boss) const;
	bool IsFinalObjective() const { return m_LevelQuestsLeft > 0 && m_QuestsCompleted >= m_LevelQuestsLeft - 1; }
	
	enum GameState
	{
		STATE_STARTING,
		STATE_GAME,
		STATE_FAIL,
	};
};
#endif
