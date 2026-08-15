#include <base/system.h>
#include <engine/storage.h>
#include <game/expedition_save.h>

namespace
{
int Fail(const char *pMessage)
{
	dbg_msg("test", "%s", pMessage);
	return 1;
}

bool WriteText(IStorage *pStorage, const char *pFilename, const char *pText)
{
	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	const unsigned Length = (unsigned)str_length(pText);
	const bool Result = io_write(File, pText, Length) == Length;
	io_close(File);
	return Result;
}
} // namespace

int main(int argc, const char **argv)
{
	IStorage *pStorage = CreateStorage("NinslashExpeditionTest", IStorage::STORAGETYPE_CLIENT, argc, argv);
	if(!pStorage)
		return Fail("storage initialization failed");
	CExpeditionSaveStorage::Remove(pStorage, 1);

	CExpeditionSave First;
	First.m_Floor = 12;
	First.m_Seed = 4242;
	First.m_UsedContracts = 3;
	CExpeditionPlayer Player;
	mem_zero(&Player, sizeof(Player));
	str_copy(Player.m_aName, "Host", sizeof(Player.m_aName));
	Player.m_ColorID = 7;
	Player.m_Gold = 80;
	Player.m_Kits = 4;
	Player.m_aWeaponDefinitionId[0] = 3;
	Player.m_aWeaponLevel[0] = 2;
	Player.m_aWeaponAmmo[0] = 11;
	Player.m_aPvePerks[5] = 2;
	Player.m_PveLegendaryCard = -1;
	if(First.UpsertPlayer(Player) < 0)
		return Fail("upsert host failed");
	if(!CExpeditionSaveStorage::Save(pStorage, 1, First))
		return Fail("first save failed");

	CExpeditionPlayer Guest;
	mem_zero(&Guest, sizeof(Guest));
	str_copy(Guest.m_aName, "Guest", sizeof(Guest.m_aName));
	Guest.m_ColorID = 9;
	Guest.m_Gold = 15;
	Guest.m_PveLegendaryCard = -1;
	if(First.UpsertPlayer(Guest) < 0)
		return Fail("upsert guest failed");
	Player.m_Gold = 90;
	if(First.UpsertPlayer(Player) < 0)
		return Fail("update host failed");
	if(First.m_NumPlayers != 2 || First.FindPlayer("Host", 7) != 0)
		return Fail("player merge failed");
	if(!CExpeditionSaveStorage::Save(pStorage, 1, First))
		return Fail("second save failed");

	CExpeditionSave Loaded;
	if(CExpeditionSaveStorage::Load(pStorage, 1, &Loaded) != EXPEDITION_LOAD_OK || Loaded.m_Floor != 12 ||
	   Loaded.m_Seed != 4242 || Loaded.m_NumPlayers != 2)
		return Fail("round-trip mismatch");
	const int Host = Loaded.FindPlayer("Host", 7);
	const int Other = Loaded.FindPlayer("Guest", 9);
	if(Host < 0 || Other < 0 || Loaded.m_aPlayers[Host].m_Gold != 90 || Loaded.m_aPlayers[Host].m_aPvePerks[5] != 2 ||
	   Loaded.m_aPlayers[Host].m_aWeaponDefinitionId[0] != 3)
		return Fail("player payload mismatch");

	if(!WriteText(pStorage, "expedition_1.json", "{broken"))
		return Fail("could not create corrupt fixture");
	if(CExpeditionSaveStorage::Load(pStorage, 1, &Loaded) != EXPEDITION_LOAD_CORRUPT)
		return Fail("corrupt save was accepted");

	if(!WriteText(pStorage, "expedition_1.json", "{\"schema_version\":99,\"floor\":1,\"seed\":1}"))
		return Fail("could not create future-version fixture");
	if(CExpeditionSaveStorage::Load(pStorage, 1, &Loaded) != EXPEDITION_LOAD_FUTURE_VERSION)
		return Fail("future save version was accepted");

	if(CExpeditionSaveStorage::Load(pStorage, 2, &Loaded) != EXPEDITION_LOAD_MISSING)
		return Fail("empty slot was not missing");
	if(CExpeditionSaveStorage::SlotValid(0) || CExpeditionSaveStorage::SlotValid(4))
		return Fail("slot bounds failed");

	dbg_msg("test", "expedition save: PASS");
	delete pStorage;
	return 0;
}
