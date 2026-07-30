#ifndef GAME_SERVER_GAMEMODES_TUTORIAL_H
#define GAME_SERVER_GAMEMODES_TUTORIAL_H

#include <engine/shared/protocol.h>
#include <game/weapon_catalog.h>
#include <game/server/gamecontroller.h>

class CGameControllerTutorial : public IGameController
{
	static const int MAX_TUTORIAL_TARGET_SLOTS = 32;
	vec2 m_aTargetSpawnPoints[MAX_TUTORIAL_TARGET_SLOTS];
	int m_NumTargetSpawnPoints;
	int m_TargetSpawnRotation;
	bool m_TargetSlotsReported;
	vec2 m_CurrentTargetPos;
	bool m_aRespawnNearTarget[MAX_CLIENTS];
	class CServerRadar *m_apObjectiveRadars[4];
	int m_NumObjectiveRadars;

	int DesiredBots() const;
	void UpdateControlledBots();
	void AddTargetSpawn(vec2 Pos);
	bool GetRespawnNearTarget(vec2 *pOutPos) const;
	void ClearObjectiveRadars();
	void RefreshObjectiveRadars();

  public:
	explicit CGameControllerTutorial(class CGameContext *pGameServer);
	bool OnEntity(int Index, vec2 Pos) override;
	void AddEnemy(vec2 Pos) override;
	bool GetSpawnPos(int Team, vec2 *pOutPos) override;
	bool CanSpawn(int Team, vec2 *pOutPos, bool IsBot = false) override;
	void OnCharacterSpawn(class CCharacter *pChr, bool RequestAI = false) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, const CAttackSource &Source) override;
	void OnSwitchTriggered() override;
	void Tick() override;
};

#endif
