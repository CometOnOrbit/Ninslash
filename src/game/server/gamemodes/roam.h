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
	bool CanCharacterSpawn(int ClientID) override;
	bool CanSpawn(int Team, vec2 *pPos, bool IsBot = false);
	void Tick() override;
	void Snap(int SnappingClient) override;
	void DoWincheck() override;
	bool OnEntity(int Index, vec2 Pos);
	bool OnCourseEntity(int Index, vec2 Pos, int CourseOrdinal, vec2 RespawnPos);
	bool RegisterRaceGate(int Index, vec2 Min, vec2 Max, int CourseOrdinal, vec2 RespawnPos);
	bool GetRaceTarget(int ClientID, vec2 *pTargetPos, int *pCourseOrdinal = 0) const;
	void ResetRace(int ClientID);
	void FinalizeCourse(bool HasModularSpawn);
	void FreezeCourse();
	void AddEnemy(vec2 Pos);
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source);

  private:
	struct CRaceState
	{
		int m_StartTick;
		int m_NextCheckpoint;
		int m_FinishTick;
		bool m_Active;
		bool m_HasRespawn;
		vec2 m_RespawnPos;
		bool m_HasPreviousPos;
		vec2 m_PreviousPos;
	};

	struct CRaceGate
	{
		vec2 m_Min;
		vec2 m_Max;
		vec2 m_RespawnPos;
		int m_CourseOrdinal;
	};

	void AddRaceGate(CRaceGate *pGates, int &Count, vec2 Min, vec2 Max, int CourseOrdinal, vec2 RespawnPos);
	void TickRace();

	vec2 m_aBotSpawn[99];
	int m_BotSpawnNum;
	vec2 m_aSpawnPoints[MAX_ROAM_SPAWNS];
	int m_NumSpawnPoints;
	CRaceGate m_aCheckpoints[MAX_RACE_MARKERS];
	CRaceGate m_aFinishes[MAX_RACE_MARKERS];
	int m_NumCheckpoints;
	int m_NumFinishes;
	bool m_CourseValid;
	bool m_CourseFrozen;
	int m_SpawningClient;
	CRaceState m_aRace[MAX_CLIENTS];
};
#endif
