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
