#ifndef GAME_EXPEDITION_SAVE_H
#define GAME_EXPEDITION_SAVE_H

#include <game/pve_roguelite.h>

class IStorage;

enum
{
	EXPEDITION_SLOTS = 3,
	EXPEDITION_MAX_PLAYERS = 16,
	EXPEDITION_WEAPON_SLOTS = 12,
};

struct CExpeditionPlayer
{
	char m_aName[16];
	int m_ColorID;
	int m_Kits;
	int m_Armor;
	int m_Score;
	int m_Gold;
	int m_aWeaponDefinitionId[EXPEDITION_WEAPON_SLOTS];
	int m_aWeaponLevel[EXPEDITION_WEAPON_SLOTS];
	int m_aWeaponAmmo[EXPEDITION_WEAPON_SLOTS];
	int m_aPvePerks[NUM_PVE_CARDS];
	int m_PveChoices;
	int m_PveUsedContracts;
	int m_PveInvasionFloors;
	int m_PvePendingArmor;
	int m_PvePendingKits;
	int m_PvePendingAmmo;
	int m_PveLegendaryCard;
	int m_aPveWeaponResources[4];
	int m_PveBarrier;
	int m_PveDroneModule;
	int m_PveDeathlessFloors;
};

struct CExpeditionSave
{
	enum
	{
		CURRENT_SCHEMA_VERSION = 1,
	};

	int m_SchemaVersion;
	int m_Floor;
	int m_Seed;
	int m_UsedContracts;
	int m_NumPlayers;
	CExpeditionPlayer m_aPlayers[EXPEDITION_MAX_PLAYERS];

	CExpeditionSave();
	void Reset();
	void Sanitize();
	int FindPlayer(const char *pName, int ColorID) const;
	int UpsertPlayer(const CExpeditionPlayer &Player);
};

enum EExpeditionLoadResult
{
	EXPEDITION_LOAD_OK = 0,
	EXPEDITION_LOAD_MISSING,
	EXPEDITION_LOAD_CORRUPT,
	EXPEDITION_LOAD_FUTURE_VERSION,
};

class CExpeditionSaveStorage
{
public:
	static bool SlotValid(int Slot);
	static void Filename(int Slot, char *pBuf, int Size);
	static EExpeditionLoadResult Load(IStorage *pStorage, int Slot, CExpeditionSave *pData);
	static bool Save(IStorage *pStorage, int Slot, const CExpeditionSave &Data);
	static bool Remove(IStorage *pStorage, int Slot);
};

#endif
