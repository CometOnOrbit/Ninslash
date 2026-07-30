#include <engine/server/server_mod_manager.h>

#include <base/system.h>
#include <game/version.h>

#include <assert.h>
#include <string.h>

static void WriteFile(const char *pPath, const char *pText)
{
	IOHANDLE File = io_open(pPath, IOFLAG_WRITE);
	assert(File);
	assert(io_write(File, pText, str_length(pText)) == (unsigned)str_length(pText));
	assert(io_sync(File) == 0);
	io_close(File);
}

static void CopyFile(const char *pSource, const char *pTarget)
{
	IOHANDLE Source = io_open(pSource, IOFLAG_READ);
	assert(Source);
	const long Size = io_length(Source);
	assert(Size > 0);
	char *pData = (char *)mem_alloc((unsigned)Size, 1);
	assert(io_read(Source, pData, (unsigned)Size) == (unsigned)Size);
	io_close(Source);

	IOHANDLE Target = io_open(pTarget, IOFLAG_WRITE);
	assert(Target);
	assert(io_write(Target, pData, (unsigned)Size) == (unsigned)Size);
	assert(io_sync(Target) == 0);
	io_close(Target);
	mem_free(pData);
}

int main(int argc, char **argv)
{
	char aRoot[256], aWorkshop[320], aState[320], aError[256];
	str_format(aRoot, sizeof(aRoot), "/tmp/ninslash-server-mod-manager-%lld", (long long)time_get());
	assert(fs_makedir(aRoot) == 0);
	str_format(aWorkshop, sizeof(aWorkshop), "%s/workshop", aRoot);
	assert(fs_makedir(aWorkshop) == 0);
	str_format(aState, sizeof(aState), "%s/server_mods.json", aRoot);

	CServerModManager Manager;
	assert(Manager.Init(aRoot, aWorkshop, "test", aError, sizeof(aError)));
	assert(str_comp(Manager.SelectedProfile(), "default") == 0);
	assert(Manager.Profiles().size() == 1 && Manager.Profiles()[0].m_RootIds.empty());
	assert(!CServerModManager::ValidProfileName("Upper"));
	assert(!CServerModManager::ValidProfileName("has space"));
	assert(CServerModManager::ValidProfileName("coop-hard_2"));
	assert(Manager.CreateProfile("ordered", "default", aError, sizeof(aError)));
	assert(!Manager.CreateProfile("ordered", 0, aError, sizeof(aError)));
	assert(Manager.SelectProfile("ordered", aError, sizeof(aError)));
	assert(!Manager.DeleteProfile("ordered", aError, sizeof(aError)));
	assert(Manager.SelectProfile("default", aError, sizeof(aError)));
	assert(Manager.DeleteProfile("ordered", aError, sizeof(aError)));

	CServerModManager Reloaded;
	assert(Reloaded.Init(aRoot, aWorkshop, "test", aError, sizeof(aError)));
	assert(str_comp(Reloaded.SelectedProfile(), "default") == 0 && Reloaded.Profiles().size() == 1);

	WriteFile(aState,
			  "{\"schema_version\":2,\"selected_profile\":\"default\",\"profiles\":[{\"name\":\"default\",\"root_ids\":"
			  "[]}]}\n");
	CServerModManager UnknownSchema;
	assert(!UnknownSchema.Init(aRoot, aWorkshop, "test", aError, sizeof(aError)));
	WriteFile(aState, "{broken\n");
	CServerModManager Corrupt;
	assert(!Corrupt.Init(aRoot, aWorkshop, "test", aError, sizeof(aError)));
	WriteFile(aState,
			  "{\"schema_version\":1,\"selected_profile\":\"default\",\"profiles\":[{\"name\":\"default\",\"root_ids\":"
			  "[]},{\"name\":\"default\",\"root_ids\":[]}]}\n");
	CServerModManager Duplicate;
	assert(!Duplicate.Init(aRoot, aWorkshop, "test", aError, sizeof(aError)));

	assert(argc == 2);
	char aImportRoot[256], aImportWorkshop[320], aInbox[320], aArchive[384];
	str_format(aImportRoot, sizeof(aImportRoot), "/tmp/ninslash-server-mod-import-%lld", (long long)time_get());
	assert(fs_makedir(aImportRoot) == 0);
	str_format(aImportWorkshop, sizeof(aImportWorkshop), "%s/workshop", aImportRoot);
	assert(fs_makedir(aImportWorkshop) == 0);
	str_format(aInbox, sizeof(aInbox), "%s/mod_inbox", aImportRoot);
	assert(fs_makedir(aInbox) == 0);
	str_format(aArchive, sizeof(aArchive), "%s/friendly.zip", aInbox);
	char aFixture[512];
	str_format(aFixture, sizeof(aFixture), "%s/friendly.zip", argv[1]);
	CopyFile(aFixture, aArchive);
	CServerModManager Importer;
	assert(Importer.Init(aImportRoot, aImportWorkshop, GAME_NETVERSION, aError, sizeof(aError)));
	assert(!Importer.StartImport("../friendly.zip", false, aError, sizeof(aError)));
	assert(Importer.StartImport("friendly.zip", false, aError, sizeof(aError)));
	assert(!Importer.StartImport("friendly.zip", false, aError, sizeof(aError)));
	while(Importer.PollImport() == CServerModManager::IMPORT_RUNNING)
		thread_sleep(1);
	assert(Importer.LastImportSucceeded() && Importer.LastImport().m_Status == CONTENT_IMPORT_INSTALLED);
	assert(Importer.Profiles()[0].m_RootIds.empty()); // Import never implicitly enables a Mod.
	assert(Importer.Enable("9000000001", 0, aError, sizeof(aError)));
	char aIds[1024], aHash[65];
	assert(Importer.ResolveSelected(aIds, sizeof(aIds), aHash, sizeof(aHash), aError, sizeof(aError)));
	assert(str_comp(aIds, "9000000001") == 0 && str_length(aHash) == 64);
	return 0;
}
