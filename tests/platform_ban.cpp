#include <engine/server/platform_ban.h>
#include <engine/storage.h>

#include <assert.h>

int main(int argc, const char **argv)
{
	IStorage *pStorage = CreateStorage("NinslashPlatformBanTest", IStorage::STORAGETYPE_SERVER, argc, argv); assert(pStorage);
	pStorage->RemoveFile("steam_bans.cfg", IStorage::TYPE_SAVE);
	CPlatformBanList Bans; Bans.Init(pStorage); char aReason[128];
	assert(Bans.Ban(76561197960265728ULL, 0, "test ban")); assert(Bans.IsBanned(76561197960265728ULL, aReason, sizeof(aReason)));
	assert(Bans.Count() == 1); CPlatformBanList Reloaded; Reloaded.Init(pStorage); assert(Reloaded.IsBanned(76561197960265728ULL, aReason, sizeof(aReason)));
	assert(Reloaded.Unban(76561197960265728ULL)); assert(!Reloaded.IsBanned(76561197960265728ULL, aReason, sizeof(aReason)));
	delete pStorage; return 0;
}
