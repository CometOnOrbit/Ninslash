#include <base/system.h>
#include <engine/storage.h>
#include <game/client/pve_progress_storage.h>

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
	IStorage *pStorage = CreateStorage("NinslashProgressTest", IStorage::STORAGETYPE_CLIENT, argc, argv);
	if(!pStorage)
		return Fail("storage initialization failed");
	pStorage->RemoveFile("pve_progress.json", IStorage::TYPE_SAVE);
	pStorage->RemoveFile("pve_progress.json.bak", IStorage::TYPE_SAVE);
	pStorage->RemoveFile("pve_progress.json.tmp", IStorage::TYPE_SAVE);

	CPveProgressData First;
	First.m_ResearchPoints = 5;
	First.m_HighestInvasion = 20;
	First.m_PreferredCheckpoint = 21;
	First.m_DroneTutorialSeen = true;
	str_copy(First.m_aResearchMask, "0000000000000000000000000000000F", sizeof(First.m_aResearchMask));
	if(!CPveProgressStorage::Save(pStorage, First))
		return Fail("first save failed");

	CPveProgressData Second = First;
	Second.m_ResearchPoints = 10;
	if(!CPveProgressStorage::Save(pStorage, Second))
		return Fail("second save failed");

	CPveProgressData Loaded;
	if(CPveProgressStorage::Load(pStorage, &Loaded) != PVE_PROGRESS_LOAD_OK || Loaded.m_ResearchPoints != 10 ||
	   Loaded.m_PreferredCheckpoint != 21 || !Loaded.m_DroneTutorialSeen)
		return Fail("round-trip mismatch");
	if(CPveProgressStorage::Load(pStorage, &Loaded, "pve_progress.json.bak") != PVE_PROGRESS_LOAD_OK ||
	   Loaded.m_ResearchPoints != 5)
		return Fail("backup does not contain previous generation");

	if(!WriteText(pStorage, "pve_progress.json", "{broken"))
		return Fail("could not create corrupt fixture");
	if(CPveProgressStorage::Load(pStorage, &Loaded) != PVE_PROGRESS_LOAD_CORRUPT)
		return Fail("corrupt save was accepted");
	if(CPveProgressStorage::Load(pStorage, &Loaded, "pve_progress.json.bak") != PVE_PROGRESS_LOAD_OK ||
	   Loaded.m_ResearchPoints != 5)
		return Fail("backup recovery failed");

	const char *pFuture = "{\"schema_version\":999,\"progress_version\":2,\"research_points\":1,"
						  "\"research_mask\":\"00000000000000000000000000000000\","
						  "\"highest_invasion\":0,\"preferred_checkpoint\":1,\"drone_tutorial_seen\":0}";
	if(!WriteText(pStorage, "pve_progress.json", pFuture))
		return Fail("could not create future-version fixture");
	if(CPveProgressStorage::Load(pStorage, &Loaded) != PVE_PROGRESS_LOAD_FUTURE_VERSION)
		return Fail("future save version was accepted");

	Second.m_HighestInvasion = 0;
	Second.m_PreferredCheckpoint = 9999;
	Second.Sanitize();
	if(Second.m_PreferredCheckpoint != 1)
		return Fail("checkpoint sanitation failed");

	dbg_msg("test", "pve progress storage: PASS");
	delete pStorage;
	return 0;
}
