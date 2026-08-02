#ifndef GAME_SERVER_GAMEMODES_EXTRACT_H
#define GAME_SERVER_GAMEMODES_EXTRACT_H
#include <game/server/gamecontroller.h>

#define MAX_ENEMIES 512
#define MAX_EXTRACTION_LOOT 32
#define MAX_EXTRACTION_OUTPOSTS 4

class CGameControllerExtract : public IGameController
{
  private:
	vec2 m_aEnemySpawnPos[MAX_ENEMIES];

	int m_Phase; // briefing, scavenge, called, boarding, result
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
	bool m_RogueliteStarted;
	int m_RogueliteWaitTick;
	bool m_RogueliteStageStarted;
	bool m_MidBossPerkOffered;
	bool m_DoorChoicePending;
	bool m_DoorChoiceStarted;
	bool m_EliteContractSpawned;
	class CDroid *m_pMidBoss;
	class CServerRadar *m_pDoor;
	class CExtractionObject *m_apLoot[MAX_EXTRACTION_LOOT];
	class CExtractionObject *m_apOutposts[MAX_EXTRACTION_OUTPOSTS];
	class CExtractionObject *m_apRevive[MAX_CLIENTS];
	vec2 m_aLootCandidate[MAX_EXTRACTION_LOOT];
	vec2 m_aOutpostPos[MAX_EXTRACTION_OUTPOSTS];
	vec2 m_aGuardSpawnPos[MAX_ENEMIES];
	vec2 m_EvacPos;
	class CExtractionObject *m_pEvacObject;
	int m_NumLootCandidates;
	int m_NumOutposts;
	int m_NumGuardSpawnPos;
	int m_Quota;
	int m_DepositedValue;
	int m_AlertLevel;
	int m_PhaseEndTick;
	int m_aCarriedValue[MAX_CLIENTS];
	int m_aInteractionTarget[MAX_CLIENTS];
	int m_aInteractionTicks[MAX_CLIENTS];
	bool m_aInteractionHeld[MAX_CLIENTS];
	bool m_aBoarded[MAX_CLIENTS];
	bool m_aEliminated[MAX_CLIENTS];
	int m_aDownCount[MAX_CLIENTS];
	int m_aBleedoutTick[MAX_CLIENTS];

	void SpawnInitialEnemies();
	void SpawnMidBoss();
	void SpawnEscapePressure();
	void BeginEvacuation();
	void BeginBoarding();
	void FinishExtraction();
	void SpawnLoot();
	void TickInteractions();
	void SendExtractionState(int ClientID);
	void TriggerOutpost(class CExtractionObject *pOutpost);
	void DropCarriedLoot(int ClientID, vec2 Pos);
	class CExtractionObject *FindExtractionObject(int ID) const;
	int CountHumanPlayersLocal() const;
	int CountHumansAliveLocal() const;
	int EnemyLevel() const;
	bool GetBossSpawnPos(vec2 *pOutPos);

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
	void OnInteract(int ClientID, int Target, bool Pressed);
	void OnClientDrop(int ClientID);
	bool WantsInventoryInteraction(int ClientID) const;
	int CarriedValue(int ClientID) const { return ClientID >= 0 && ClientID < MAX_CLIENTS ? m_aCarriedValue[ClientID] : 0; }
	bool Evacuating() const { return (m_Phase == 2 || m_Phase == 3) && !m_RoundOverTick; }

	enum GameState
	{
		STATE_STARTING,
		STATE_GAME,
	};
};
#endif
