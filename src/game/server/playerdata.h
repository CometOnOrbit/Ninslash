#ifndef GAME_SERVER_PLAYERDATA_H
#define GAME_SERVER_PLAYERDATA_H

#include <game/pve/pve_roguelite.h>

enum
{
	WEAPON_DATA_VERSION = 3,
};

// stored player data for switching between levels
class CPlayerData
{
  private:
	CPlayerData *m_pChild1;
	CPlayerData *m_pChild2;

  public:
	CPlayerData(const char *pName, int ColorID);
	void Die();
	void Reset();
	void ClearPveRun();
	void ResetWeapons();

	int m_WeaponDataVersion;
	int m_aWeaponDefinitionId[99];
	int m_aWeaponLevel[99];
	int m_aWeaponAmmo[99];

	int m_Armor;
	int m_Kits;
	int m_Score;
	int m_Gold;
	int m_aPvePerks[NUM_PVE_CARDS];
	int m_PveChoices;
	int m_PveRunMode;
	int m_PveUsedContracts;
	int m_PveInvasionFloors;
	bool m_PveStageSuppliesApplied;
	bool m_PveLastStandUsed;
	bool m_PveEmergencyPlatingUsed;
	bool m_PveContractParticipant;
	int m_PveContractNonce;
	int m_PvePendingArmor;
	int m_PvePendingKits;
	bool m_PvePendingAmmo;
	int m_PveLegendaryCard;
	int m_aPveWeaponResources[4];
	int m_PveBarrier;
	int m_PveDroneModule;
	int m_PveDroneSwitchReadyTick;
	int m_PveDeathlessFloors;

	int m_ColorID;

	char m_aName[16];

	void Add(CPlayerData *pPlayerData);
	CPlayerData *Get(const char *pName, int ColorID);

	int GetHighScore(int Score);
	int GetPlayerCount(int Score);
};

#endif
