#ifndef GAME_SERVER_GAMEMODES_INVASION_H
#define GAME_SERVER_GAMEMODES_INVASION_H
#include <game/server/gamecontroller.h>
#include <game/pve/questinfo.h>

#define MAX_ENEMIES 512
#define INV_MAX_PUSH_POINTS 3

class CBuilding;

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
	int m_QuestWaveSize;

	bool m_EliteWave;
	int m_DefendEndTick;
	int m_DefendPrepEndTick;
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
	void ClearPushForward();
	bool BuildPushForwardRoute();
	void ActivatePushForwardGroup();
	void TickPushForward();
	int CountBossesAlive() const;
	int CountBuildingsOfType(int Type) const;
	int ReactorsLeft();
	int SwitchesAvailable() const;
	void SetSwitchesActive(bool Active);
	void SetReactorDefenseActive(bool Active);
	void BuildRegionalBossArena();
	class CDroid *RegionalBoss();
	void ApplyRegionalBossPhase(int Phase);
	void TickRegionalBoss();
	int CountHumansAlive(int ExcludeCID = -1) const;
	void RewardQuestGold();

	vec2 m_aEnemySpawnPos[MAX_ENEMIES];

	bool m_RoundWin;
	int m_RoundWinTick;
	int m_RoundOverTick;
	int m_ElevatorEndTick;
	bool m_ElevatorDoorOpen;
	vec2 m_ElevatorDoorPos;
	bool m_aElevatorSupplied[MAX_CLIENTS];

	// enemy grouping
	vec2 m_GroupSpawnPos;

	void SpawnNewWave(bool AddBots = true);

	vec2 GetBotSpawnPos();
	bool GetBossSpawnPos(vec2 *pOutPos);
	void RandomGroupSpawnPos();
	int m_BotSpawnTick;

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
	int m_ReactorCountCheckTick;
	int m_CachedReactorsLeft;

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
	bool m_ForceFloorOne;
	int m_RetryVoteNonce;
	int m_RetryVoteEndTick;
	int m_RetryVoteLastSyncTick;
	int m_aRetryVotes[MAX_CLIENTS];
	int m_RetryResult;
	int m_RetryResultEndTick;
	int m_RetryResultLastSyncTick;
	char m_aRetryPlayerName[MAX_NAME_LENGTH];

	bool m_AutoRestart;
	bool IsRetryVoter(int ClientID) const;
	int RetryVoterCount() const;
	void CountRetryVotes(int *pRetry, int *pReset, int *pVoted = 0) const;
	void StartRetryVote();
	void SendRetryVote(int ClientID = -1);
	void TickRetryVote();
	void FinishRetryVote();
	void StartRetryResult(int Result);
	void SendRetryResult(int ClientID = -1);
	void TickRetryResult();
	void FinishRetryResult();
	void RegenerateMapFromTemplate();
	void BeginPostRoundTransition() override;

	void Trigger(bool IncreaseLevel);

	class CServerRadar *m_pDoor;
	class CServerRadar *m_pEnemySpawn;
	class CServerRadar *m_pReactor;
	class CServerRadar *m_pPushRadar;
	class CServerRadar *m_apSwitchRadar[8];
	int m_NumSwitchRadars;

	int m_MapTemplate;
	int m_MapBiome;
	vec2 m_aPushPoints[INV_MAX_PUSH_POINTS];
	int m_PushPointCount;
	int m_PushCompletedMask;
	int m_PushActiveMask;
	int m_PushPointEndTick;
	bool m_PushForwardActive;
	bool m_PushParallel;

	vec2 m_aRegionalBossPoints[INV_MAX_PUSH_POINTS];
	int m_RegionalBossPointCount;
	class CDroid *m_pRegionalBoss;
	int m_RegionalBossPhase;
	int m_RegionalBossSupportSpawned;

	void ClearSwitchRadars();
	void RefreshSwitchRadars();
	void ShowCartographerObjectives();
	bool AnyCartographer() const;

  public:
#if defined(CONF_DEBUG)
	void DebugBossEncounter() { ChangeQuest(QUEST_KILL_BOSS, 0.1f); }
	void DebugBossState(char *pBuffer, int Size);
#endif
	CGameControllerInvasion(class CGameContext *pGameServer);
	virtual ~CGameControllerInvasion();

	virtual bool OnEntity(int Index, vec2 Pos);
	void OnCharacterSpawn(class CCharacter *pChr, bool RequestAI = false);
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source);
	bool CanSpawn(int Team, vec2 *pPos, bool IsBot = false);
	void NextLevel(int CID = -1);
	bool GetSpawnPos(int Team, vec2 *pOutPos);
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual void OnSwitchTriggered();
	void OnRetryVote(int ClientID, int Nonce, int Choice);

	void DisplayExit(vec2 Pos);

	bool RunBuffActive() const { return m_RunBuffActive; }
	bool IsObjectiveTarget(bool Boss) const;
	virtual bool IsReactorDefenseActive() const;
	bool IsFinalObjective() const { return m_LevelQuestsLeft > 0 && m_QuestsCompleted >= m_LevelQuestsLeft - 1; }

	enum GameState
	{
		STATE_STARTING,
		STATE_GAME,
		STATE_RETRY_VOTE,
		STATE_RETRY_RESULT,
		STATE_FAIL,
	};
};
#endif
