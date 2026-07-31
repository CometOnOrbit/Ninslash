#ifndef GAME_SERVER_GAMEMODES_ROAM_H
#define GAME_SERVER_GAMEMODES_ROAM_H
#include <game/server/gamecontroller.h>

class CGameControllerRoam : public IGameController
{
	enum
	{
		MAX_ROAM_SPAWNS = 64,
		MAX_RACE_MARKERS = 64,
	};

  public:
	CGameControllerRoam(class CGameContext *pGameServer);

	void OnCharacterSpawn(class CCharacter *pChr, bool RequestAI = false);
	bool CanSpawn(int Team, vec2 *pPos, bool IsBot = false);
	void Tick() override;
	void Snap(int SnappingClient) override;
	void DoWincheck() override;
	bool OnEntity(int Index, vec2 Pos);
	void AddEnemy(vec2 Pos);
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source);

  private:
	struct CRaceState
	{
		int m_StartTick;
		int m_NextCheckpoint;
		int m_FinishTick;
		bool m_Active;
	};

	void AddRaceMarker(vec2 *pMarkers, int &Count, vec2 Pos);
	void TickRace();

	vec2 m_aBotSpawn[99];
	int m_BotSpawnNum;
	vec2 m_aSpawnPoints[MAX_ROAM_SPAWNS];
	int m_NumSpawnPoints;
	vec2 m_aCheckpoints[MAX_RACE_MARKERS];
	vec2 m_aFinishes[MAX_RACE_MARKERS];
	int m_NumCheckpoints;
	int m_NumFinishes;
	CRaceState m_aRace[MAX_CLIENTS];
};
#endif
