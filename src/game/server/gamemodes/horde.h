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

	void NextWave();
	int EnemyLevel() const;

public:
	CGameControllerHorde(class CGameContext *pGameServer);

	virtual bool OnEntity(int Index, vec2 Pos);
	void OnCharacterSpawn(class CCharacter *pChr, bool RequestAI = false);
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	bool GetSpawnPos(int Team, vec2 *pOutPos);
	virtual bool CanSpawn(int Team, vec2 *pPos, bool IsBot = false);
	virtual void Tick();
	virtual void Snap(int SnappingClient);

	enum GameState
	{
		STATE_STARTING,
		STATE_GAME,
	};
};
#endif
