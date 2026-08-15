#include "expedition_save.h"

#include <base/math.h>
#include <base/system.h>
#include <engine/external/json-parser/json.h>
#include <engine/storage.h>

namespace
{
bool ReadInt(const json_value &Object, const char *pName, int *pValue)
{
	const json_value &Value = Object[pName];
	if(Value.type != json_integer)
		return false;
	*pValue = (int)Value.u.integer;
	return true;
}

bool FileExists(IStorage *pStorage, const char *pFilename)
{
	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	io_close(File);
	return true;
}

void SanitizeName(char *pName, int Size)
{
	if(!pName || Size <= 0)
		return;
	pName[Size - 1] = 0;
	for(int i = 0; pName[i]; i++)
		if(pName[i] == '"' || pName[i] == '\\' || pName[i] < 32)
			pName[i] = '?';
}

void ResetPlayer(CExpeditionPlayer *pPlayer)
{
	mem_zero(pPlayer, sizeof(*pPlayer));
	pPlayer->m_PveLegendaryCard = -1;
}

bool ParsePlayer(const json_value &Object, CExpeditionPlayer *pPlayer)
{
	if(Object.type != json_object)
		return false;
	ResetPlayer(pPlayer);
	const json_value &Name = Object["name"];
	if(Name.type != json_string)
		return false;
	str_copy(pPlayer->m_aName, (const char *)Name, sizeof(pPlayer->m_aName));
	SanitizeName(pPlayer->m_aName, sizeof(pPlayer->m_aName));
	if(!pPlayer->m_aName[0] || !ReadInt(Object, "color", &pPlayer->m_ColorID))
		return false;
	ReadInt(Object, "kits", &pPlayer->m_Kits);
	ReadInt(Object, "armor", &pPlayer->m_Armor);
	ReadInt(Object, "score", &pPlayer->m_Score);
	ReadInt(Object, "gold", &pPlayer->m_Gold);
	ReadInt(Object, "choices", &pPlayer->m_PveChoices);
	ReadInt(Object, "used_contracts", &pPlayer->m_PveUsedContracts);
	ReadInt(Object, "invasion_floors", &pPlayer->m_PveInvasionFloors);
	ReadInt(Object, "pending_armor", &pPlayer->m_PvePendingArmor);
	ReadInt(Object, "pending_kits", &pPlayer->m_PvePendingKits);
	int PendingAmmo = 0;
	ReadInt(Object, "pending_ammo", &PendingAmmo);
	pPlayer->m_PvePendingAmmo = PendingAmmo != 0;
	ReadInt(Object, "legendary", &pPlayer->m_PveLegendaryCard);
	ReadInt(Object, "barrier", &pPlayer->m_PveBarrier);
	ReadInt(Object, "drone", &pPlayer->m_PveDroneModule);
	ReadInt(Object, "deathless", &pPlayer->m_PveDeathlessFloors);

	const json_value &Weapons = Object["weapons"];
	if(Weapons.type == json_array)
	{
		const int Count = min((int)Weapons.u.array.length, (int)EXPEDITION_WEAPON_SLOTS);
		for(int i = 0; i < Count; i++)
		{
			const json_value &Weapon = Weapons[i];
			if(Weapon.type != json_array || Weapon.u.array.length < 3)
				continue;
			if(Weapon[0].type == json_integer)
				pPlayer->m_aWeaponDefinitionId[i] = (int)Weapon[0].u.integer;
			if(Weapon[1].type == json_integer)
				pPlayer->m_aWeaponLevel[i] = (int)Weapon[1].u.integer;
			if(Weapon[2].type == json_integer)
				pPlayer->m_aWeaponAmmo[i] = (int)Weapon[2].u.integer;
		}
	}

	const json_value &Resources = Object["weapon_resources"];
	if(Resources.type == json_array)
	{
		const int Count = min((int)Resources.u.array.length, 4); // 4 weapon resource tracks
		for(int i = 0; i < Count; i++)
			if(Resources[i].type == json_integer)
				pPlayer->m_aPveWeaponResources[i] = (int)Resources[i].u.integer;
	}

	const json_value &Perks = Object["perks"];
	if(Perks.type == json_array)
	{
		for(unsigned i = 0; i < Perks.u.array.length; i++)
		{
			const json_value &Perk = Perks[i];
			if(Perk.type != json_array || Perk.u.array.length < 2)
				continue;
			if(Perk[0].type != json_integer || Perk[1].type != json_integer)
				continue;
			const int ID = (int)Perk[0].u.integer;
			if(ID < 0 || ID >= NUM_PVE_CARDS)
				continue;
			pPlayer->m_aPvePerks[ID] = (int)Perk[1].u.integer;
		}
	}
	return true;
}

int Append(char *pBuf, int Size, int Offset, const char *pText)
{
	if(Offset < 0 || Offset >= Size)
		return -1;
	const int Left = Size - Offset;
	str_copy(pBuf + Offset, pText, Left);
	const int Added = str_length(pBuf + Offset);
	if(Added >= Left - 1)
		return -1;
	return Offset + Added;
}
} // namespace

CExpeditionSave::CExpeditionSave()
{
	Reset();
}

void CExpeditionSave::Reset()
{
	mem_zero(this, sizeof(*this));
	m_SchemaVersion = CURRENT_SCHEMA_VERSION;
	m_Floor = 1;
	m_Seed = 1;
	for(int i = 0; i < EXPEDITION_MAX_PLAYERS; i++)
		m_aPlayers[i].m_PveLegendaryCard = -1;
}

void CExpeditionSave::Sanitize()
{
	m_SchemaVersion = CURRENT_SCHEMA_VERSION;
	m_Floor = clamp(m_Floor, 1, 9999);
	if(m_Seed <= 0)
		m_Seed = 1;
	m_UsedContracts = max(0, m_UsedContracts);
	m_NumPlayers = clamp(m_NumPlayers, 0, (int)EXPEDITION_MAX_PLAYERS);
	for(int i = 0; i < m_NumPlayers; i++)
	{
		CExpeditionPlayer *pPlayer = &m_aPlayers[i];
		SanitizeName(pPlayer->m_aName, sizeof(pPlayer->m_aName));
		pPlayer->m_Kits = clamp(pPlayer->m_Kits, 0, 99);
		pPlayer->m_Armor = clamp(pPlayer->m_Armor, 0, 100);
		pPlayer->m_Score = max(0, pPlayer->m_Score);
		pPlayer->m_Gold = max(0, pPlayer->m_Gold);
		pPlayer->m_PveChoices = max(0, pPlayer->m_PveChoices);
		pPlayer->m_PveInvasionFloors = max(0, pPlayer->m_PveInvasionFloors);
		pPlayer->m_PvePendingArmor = max(0, pPlayer->m_PvePendingArmor);
		pPlayer->m_PvePendingKits = max(0, pPlayer->m_PvePendingKits);
		pPlayer->m_PveLegendaryCard = clamp(pPlayer->m_PveLegendaryCard, -1, NUM_PVE_CARDS - 1);
		pPlayer->m_PveBarrier = clamp(pPlayer->m_PveBarrier, 0, 30);
		pPlayer->m_PveDroneModule = clamp(pPlayer->m_PveDroneModule, (int)PVE_DRONE_NONE, (int)PVE_DRONE_REPAIR);
		pPlayer->m_PveDeathlessFloors = clamp(pPlayer->m_PveDeathlessFloors, 0, 5);
		for(int w = 0; w < EXPEDITION_WEAPON_SLOTS; w++)
		{
			pPlayer->m_aWeaponDefinitionId[w] = max(0, pPlayer->m_aWeaponDefinitionId[w]);
			pPlayer->m_aWeaponLevel[w] = max(0, pPlayer->m_aWeaponLevel[w]);
			pPlayer->m_aWeaponAmmo[w] = max(0, pPlayer->m_aWeaponAmmo[w]);
		}
		for(int r = 0; r < 4; r++)
			pPlayer->m_aPveWeaponResources[r] = clamp(pPlayer->m_aPveWeaponResources[r], 0, 10);
		for(int c = 0; c < NUM_PVE_CARDS; c++)
			pPlayer->m_aPvePerks[c] = max(0, pPlayer->m_aPvePerks[c]);
	}
}

int CExpeditionSave::FindPlayer(const char *pName, int ColorID) const
{
	if(!pName || !pName[0])
		return -1;
	for(int i = 0; i < m_NumPlayers; i++)
		if(m_aPlayers[i].m_ColorID == ColorID && str_comp(m_aPlayers[i].m_aName, pName) == 0)
			return i;
	return -1;
}

int CExpeditionSave::UpsertPlayer(const CExpeditionPlayer &Player)
{
	if(!Player.m_aName[0])
		return -1;
	int Index = FindPlayer(Player.m_aName, Player.m_ColorID);
	if(Index < 0)
	{
		if(m_NumPlayers >= EXPEDITION_MAX_PLAYERS)
			return -1;
		Index = m_NumPlayers++;
	}
	m_aPlayers[Index] = Player;
	return Index;
}

bool CExpeditionSaveStorage::SlotValid(int Slot)
{
	return Slot >= 1 && Slot <= EXPEDITION_SLOTS;
}

void CExpeditionSaveStorage::Filename(int Slot, char *pBuf, int Size)
{
	str_format(pBuf, Size, "expedition_%d.json", Slot);
}

EExpeditionLoadResult CExpeditionSaveStorage::Load(IStorage *pStorage, int Slot, CExpeditionSave *pData)
{
	if(!pStorage || !pData || !SlotValid(Slot))
		return EXPEDITION_LOAD_CORRUPT;
	char aFilename[64];
	Filename(Slot, aFilename, sizeof(aFilename));
	IOHANDLE File = pStorage->OpenFile(aFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return EXPEDITION_LOAD_MISSING;
	const long Length = io_length(File);
	if(Length <= 0 || Length > 64 * 1024)
	{
		io_close(File);
		return EXPEDITION_LOAD_CORRUPT;
	}
	char *pBuffer = (char *)mem_alloc((unsigned)Length + 1, 1);
	const unsigned Read = io_read(File, pBuffer, (unsigned)Length);
	io_close(File);
	if(Read != (unsigned)Length)
	{
		mem_free(pBuffer);
		return EXPEDITION_LOAD_CORRUPT;
	}
	pBuffer[Length] = 0;

	json_settings Settings;
	mem_zero(&Settings, sizeof(Settings));
	char aError[256];
	json_value *pJson = json_parse_ex(&Settings, pBuffer, (size_t)Length, aError);
	mem_free(pBuffer);
	if(!pJson || pJson->type != json_object)
	{
		if(pJson)
			json_value_free(pJson);
		return EXPEDITION_LOAD_CORRUPT;
	}

	CExpeditionSave Loaded;
	const bool Valid = ReadInt(*pJson, "schema_version", &Loaded.m_SchemaVersion) &&
					   ReadInt(*pJson, "floor", &Loaded.m_Floor) && ReadInt(*pJson, "seed", &Loaded.m_Seed);
	ReadInt(*pJson, "used_contracts", &Loaded.m_UsedContracts);
	if(!Valid)
	{
		json_value_free(pJson);
		return EXPEDITION_LOAD_CORRUPT;
	}
	if(Loaded.m_SchemaVersion > CExpeditionSave::CURRENT_SCHEMA_VERSION)
	{
		json_value_free(pJson);
		return EXPEDITION_LOAD_FUTURE_VERSION;
	}
	if(Loaded.m_SchemaVersion <= 0)
	{
		json_value_free(pJson);
		return EXPEDITION_LOAD_CORRUPT;
	}

	const json_value &Players = (*pJson)["players"];
	if(Players.type == json_array)
	{
		for(unsigned i = 0; i < Players.u.array.length && Loaded.m_NumPlayers < EXPEDITION_MAX_PLAYERS; i++)
		{
			if(!ParsePlayer(Players[i], &Loaded.m_aPlayers[Loaded.m_NumPlayers]))
			{
				json_value_free(pJson);
				return EXPEDITION_LOAD_CORRUPT;
			}
			Loaded.m_NumPlayers++;
		}
	}
	json_value_free(pJson);
	Loaded.Sanitize();
	*pData = Loaded;
	return EXPEDITION_LOAD_OK;
}

bool CExpeditionSaveStorage::Save(IStorage *pStorage, int Slot, const CExpeditionSave &Source)
{
	if(!pStorage || !SlotValid(Slot))
		return false;
	CExpeditionSave Data = Source;
	Data.Sanitize();

	char aJson[64 * 1024];
	int Offset = 0;
	char aLine[1024];
	str_format(aLine,
			   sizeof(aLine),
			   "{\n  \"schema_version\": %d,\n  \"floor\": %d,\n  \"seed\": %d,\n  \"used_contracts\": %d,\n  \"players\": [\n",
			   Data.m_SchemaVersion,
			   Data.m_Floor,
			   Data.m_Seed,
			   Data.m_UsedContracts);
	Offset = Append(aJson, sizeof(aJson), Offset, aLine);
	for(int i = 0; i < Data.m_NumPlayers && Offset >= 0; i++)
	{
		const CExpeditionPlayer &Player = Data.m_aPlayers[i];
		str_format(aLine,
				   sizeof(aLine),
				   "%s    {\n      \"name\": \"%s\",\n      \"color\": %d,\n      \"kits\": %d,\n      \"armor\": %d,\n"
				   "      \"score\": %d,\n      \"gold\": %d,\n      \"choices\": %d,\n      \"used_contracts\": %d,\n"
				   "      \"invasion_floors\": %d,\n      \"pending_armor\": %d,\n      \"pending_kits\": %d,\n"
				   "      \"pending_ammo\": %d,\n      \"legendary\": %d,\n      \"barrier\": %d,\n      \"drone\": %d,\n"
				   "      \"deathless\": %d,\n      \"weapon_resources\": [%d, %d, %d, %d],\n      \"weapons\": [",
				   i ? ",\n" : "",
				   Player.m_aName,
				   Player.m_ColorID,
				   Player.m_Kits,
				   Player.m_Armor,
				   Player.m_Score,
				   Player.m_Gold,
				   Player.m_PveChoices,
				   Player.m_PveUsedContracts,
				   Player.m_PveInvasionFloors,
				   Player.m_PvePendingArmor,
				   Player.m_PvePendingKits,
				   Player.m_PvePendingAmmo ? 1 : 0,
				   Player.m_PveLegendaryCard,
				   Player.m_PveBarrier,
				   Player.m_PveDroneModule,
				   Player.m_PveDeathlessFloors,
				   Player.m_aPveWeaponResources[0],
				   Player.m_aPveWeaponResources[1],
				   Player.m_aPveWeaponResources[2],
				   Player.m_aPveWeaponResources[3]);
		if(str_length(aLine) >= (int)sizeof(aLine) - 1)
			return false;
		Offset = Append(aJson, sizeof(aJson), Offset, aLine);
		for(int w = 0; w < EXPEDITION_WEAPON_SLOTS && Offset >= 0; w++)
		{
			str_format(aLine,
					   sizeof(aLine),
					   "%s[%d, %d, %d]",
					   w ? ", " : "",
					   Player.m_aWeaponDefinitionId[w],
					   Player.m_aWeaponLevel[w],
					   Player.m_aWeaponAmmo[w]);
			Offset = Append(aJson, sizeof(aJson), Offset, aLine);
		}
		Offset = Append(aJson, sizeof(aJson), Offset, "],\n      \"perks\": [");
		bool FirstPerk = true;
		for(int c = 0; c < NUM_PVE_CARDS && Offset >= 0; c++)
		{
			if(Player.m_aPvePerks[c] <= 0)
				continue;
			str_format(aLine, sizeof(aLine), "%s[%d, %d]", FirstPerk ? "" : ", ", c, Player.m_aPvePerks[c]);
			Offset = Append(aJson, sizeof(aJson), Offset, aLine);
			FirstPerk = false;
		}
		Offset = Append(aJson, sizeof(aJson), Offset, "]\n    }");
	}
	Offset = Append(aJson, sizeof(aJson), Offset, "\n  ]\n}\n");
	if(Offset < 0)
		return false;

	char aFilename[64];
	char aTemp[80];
	char aBackup[80];
	Filename(Slot, aFilename, sizeof(aFilename));
	str_format(aTemp, sizeof(aTemp), "%s.tmp", aFilename);
	str_format(aBackup, sizeof(aBackup), "%s.bak", aFilename);
	pStorage->RemoveFile(aTemp, IStorage::TYPE_SAVE);
	IOHANDLE File = pStorage->OpenFile(aTemp, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	const unsigned Length = (unsigned)str_length(aJson);
	const bool Written = io_write(File, aJson, Length) == Length && io_flush(File) == 0;
	io_close(File);
	if(!Written)
	{
		pStorage->RemoveFile(aTemp, IStorage::TYPE_SAVE);
		return false;
	}
	pStorage->RemoveFile(aBackup, IStorage::TYPE_SAVE);
	const bool HadCurrent = FileExists(pStorage, aFilename);
	if(HadCurrent && !pStorage->RenameFile(aFilename, aBackup, IStorage::TYPE_SAVE))
	{
		pStorage->RemoveFile(aTemp, IStorage::TYPE_SAVE);
		return false;
	}
	if(!pStorage->RenameFile(aTemp, aFilename, IStorage::TYPE_SAVE))
	{
		if(HadCurrent)
			pStorage->RenameFile(aBackup, aFilename, IStorage::TYPE_SAVE);
		pStorage->RemoveFile(aTemp, IStorage::TYPE_SAVE);
		return false;
	}
	return true;
}

bool CExpeditionSaveStorage::Remove(IStorage *pStorage, int Slot)
{
	if(!pStorage || !SlotValid(Slot))
		return false;
	char aFilename[64];
	char aTemp[80];
	char aBackup[80];
	Filename(Slot, aFilename, sizeof(aFilename));
	str_format(aTemp, sizeof(aTemp), "%s.tmp", aFilename);
	str_format(aBackup, sizeof(aBackup), "%s.bak", aFilename);
	pStorage->RemoveFile(aTemp, IStorage::TYPE_SAVE);
	pStorage->RemoveFile(aBackup, IStorage::TYPE_SAVE);
	return pStorage->RemoveFile(aFilename, IStorage::TYPE_SAVE);
}
