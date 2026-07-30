#include <engine/shared/content_package.h>
#include <engine/shared/content_package_import.h>

#include <base/system.h>

#include <assert.h>
#include <string.h>

int main(int argc, char **argv)
{
	assert(argc == 2);
	const char *pFixtures = argv[1];
	const char *pProtocol = "0.5.1 abc123-luaweapons5";
	char aRoot[512], aArchive[512], aError[256];
	str_format(aRoot, sizeof(aRoot), "/tmp/ninslash-content-import-%lld", (long long)time_get());
	assert(fs_makedir(aRoot) == 0);

	CContentPackageImportResult Result;
	str_format(aArchive, sizeof(aArchive), "%s/friendly.zip", pFixtures);
	assert(ContentPackageImportZip(aArchive, aRoot, pProtocol, false, &Result, aError, sizeof(aError)));
	assert(Result.m_Status == CONTENT_IMPORT_INSTALLED);
	assert(str_comp(Result.m_aPublishedFileID, "9000000001") == 0);
	assert(strstr(Result.m_aTargetRoot, "/plasma-carbine-example") != 0);
	CContentManifest Manifest;
	assert(ContentPackageValidate(Result.m_aTargetRoot, "9000000001", pProtocol, &Manifest, aError, sizeof(aError)));
	assert(ContentPackageFinalizeImport(&Result, aError, sizeof(aError)));

	assert(ContentPackageImportZip(aArchive, aRoot, pProtocol, false, &Result, aError, sizeof(aError)));
	assert(Result.m_Status == CONTENT_IMPORT_ALREADY_INSTALLED);
	str_format(aArchive, sizeof(aArchive), "%s/updated.zip", pFixtures);
	assert(ContentPackageImportZip(aArchive, aRoot, pProtocol, false, &Result, aError, sizeof(aError)));
	assert(Result.m_Status == CONTENT_IMPORT_REPLACE_REQUIRED);
	assert(str_comp(Result.m_aPreviousVersion, "1") == 0 && str_comp(Result.m_aVersion, "2") == 0);
	assert(ContentPackageImportZip(aArchive, aRoot, pProtocol, true, &Result, aError, sizeof(aError)));
	assert(Result.m_Status == CONTENT_IMPORT_INSTALLED && Result.m_Replaced);
	assert(ContentPackageRollbackImport(&Result, aError, sizeof(aError)));
	assert(ContentPackageValidate(Result.m_aTargetRoot, "9000000001", pProtocol, &Manifest, aError, sizeof(aError)));
	assert(str_comp(Manifest.m_aVersion, "1") == 0);
	assert(ContentPackageImportZip(aArchive, aRoot, pProtocol, true, &Result, aError, sizeof(aError)));
	assert(ContentPackageFinalizeImport(&Result, aError, sizeof(aError)));
	assert(ContentPackageValidate(Result.m_aTargetRoot, "9000000001", pProtocol, &Manifest, aError, sizeof(aError)));
	assert(str_comp(Manifest.m_aVersion, "2") == 0);

	char aSecondRoot[512];
	str_format(aSecondRoot, sizeof(aSecondRoot), "%s-root", aRoot);
	assert(fs_makedir(aSecondRoot) == 0);
	str_format(aArchive, sizeof(aArchive), "%s/root.zip", pFixtures);
	assert(ContentPackageImportZip(aArchive, aSecondRoot, pProtocol, false, &Result, aError, sizeof(aError)));
	assert(Result.m_Status == CONTENT_IMPORT_INSTALLED);
	assert(ContentPackageFinalizeImport(&Result, aError, sizeof(aError)));

	char aRejectedRoot[512];
	str_format(aRejectedRoot, sizeof(aRejectedRoot), "%s-rejected", aRoot);
	assert(fs_makedir(aRejectedRoot) == 0);
	str_format(aArchive, sizeof(aArchive), "%s/traversal.zip", pFixtures);
	assert(!ContentPackageImportZip(aArchive, aRejectedRoot, pProtocol, false, &Result, aError, sizeof(aError)));
	assert(strstr(aError, "unsafe") != 0 || strstr(aError, "package root") != 0);
	str_format(aArchive, sizeof(aArchive), "%s/undeclared.zip", pFixtures);
	assert(!ContentPackageImportZip(aArchive, aRejectedRoot, pProtocol, false, &Result, aError, sizeof(aError)));
	assert(strstr(aError, "undeclared") != 0);
	return 0;
}
