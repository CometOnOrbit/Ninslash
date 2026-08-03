#include <engine/shared/content_package.h>
#include <base/system.h>

#include <assert.h>
#include <string.h>
#if defined(CONF_FAMILY_UNIX)
#include <unistd.h>
#endif

static void WriteFile(const char *pPath, const char *pData)
{
	IOHANDLE File = io_open(pPath, IOFLAG_WRITE);
	assert(File);
	io_write(File, pData, str_length(pData));
	io_close(File);
}

int main()
{
	char aRoot[256];
	str_format(aRoot, sizeof(aRoot), "/tmp/ninslash-mod-package-%lld", (long long)time_get());
	assert(fs_makedir(aRoot) == 0);
	char aMaps[300], aScripts[300], aManifestPath[300], aMapPath[300], aScriptPath[300], aExtraPath[300];
	str_format(aMaps, sizeof(aMaps), "%s/maps", aRoot);
	str_format(aScripts, sizeof(aScripts), "%s/rules", aRoot);
	assert(fs_makedir(aMaps) == 0 && fs_makedir(aScripts) == 0);
	str_format(aMapPath, sizeof(aMapPath), "%s/maps/a.map", aRoot);
	str_format(aScriptPath, sizeof(aScriptPath), "%s/rules/main.lua", aRoot);
	str_format(aManifestPath, sizeof(aManifestPath), "%s/ninslash_content.json", aRoot);
	str_format(aExtraPath, sizeof(aExtraPath), "%s/extra.txt", aRoot);
	WriteFile(aMapPath, "map");
	WriteFile(aScriptPath, "function on_event() end");
	const char *pTemplate = "{\"schema_version\":1,\"content_type\":\"mod\",\"published_file_id\":\"42\",\"name\":"
							"\"Safe\",\"description\":\"Test "
							"content\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_"
							"hash\":\"%s\",\"content_rating\":\"everyone\",\"api_version\":1,\"capabilities\":["
							"\"resources\"],\"maps\":[\"maps/a.map\"],\"scripts\":[\"rules/main.lua\"]}";
	char aJson[2048], aError[256];
	str_format(aJson, sizeof(aJson), pTemplate, "0000000000000000000000000000000000000000000000000000000000000000");
	CContentManifest Manifest;
	assert(ContentManifestParse(aJson, (int)strlen(aJson), "test", &Manifest, aError, sizeof(aError)));
	char aHash[65];
	assert(ContentPackageComputeHash(aRoot, Manifest, aHash, aError, sizeof(aError)));
	char aChangedJson[2048], aChangedHash[65];
	str_copy(aChangedJson, aJson, sizeof(aChangedJson));
	char *pDescription = strstr(aChangedJson, "Test content");
	assert(pDescription);
	pDescription[0] = 'B';
	CContentManifest Changed;
	assert(ContentManifestParse(aChangedJson, (int)strlen(aChangedJson), "test", &Changed, aError, sizeof(aError)));
	assert(ContentPackageComputeHash(aRoot, Changed, aChangedHash, aError, sizeof(aError)));
	assert(str_comp(aHash, aChangedHash) != 0);
	str_format(aJson, sizeof(aJson), pTemplate, aHash);
	WriteFile(aManifestPath, aJson);
	assert(ContentPackageValidate(aRoot, "42", "test", &Manifest, aError, sizeof(aError)));
	char aWorkshop[300], aStaged[400];
	// Stage outside aRoot: ContentPackageValidate(aRoot) scans recursively and
	// would otherwise hit the staged copy as undeclared files (or mask the
	// symlink error below).
	str_format(aWorkshop, sizeof(aWorkshop), "/tmp/ninslash-workshop-%lld", (long long)time_get());
	assert(ContentPackageStage(
		aRoot, aWorkshop, "42", "test", &Manifest, aStaged, sizeof(aStaged), aError, sizeof(aError)));
	assert(str_length(aStaged) >= 3 && str_comp(aStaged + str_length(aStaged) - 3, "/42") == 0);
	assert(ContentPackageValidate(aStaged, "42", "test", &Manifest, aError, sizeof(aError)));
	assert(!ContentPackageValidate(aRoot, "43", "test", &Manifest, aError, sizeof(aError)));
	WriteFile(aExtraPath, "undeclared");
	assert(!ContentPackageValidate(aRoot, "42", "test", &Manifest, aError, sizeof(aError)));
#if defined(CONF_FAMILY_UNIX)
	assert(fs_remove(aExtraPath) == 0 && fs_remove(aScriptPath) == 0);
	assert(symlink(aMapPath, aScriptPath) == 0);
	assert(!ContentPackageValidate(aRoot, "42", "test", &Manifest, aError, sizeof(aError)));
	assert(strstr(aError, "symbolic") != 0);
#endif
	char aMapSource[256];
	char aMapDirectory[300];
	char aMapFile[340];
	char aMapManifestPath[340];
	str_format(aMapSource, sizeof(aMapSource), "/tmp/ninslash-map-package-%lld", (long long)time_get());
	assert(fs_makedir(aMapSource) == 0);
	str_format(aMapDirectory, sizeof(aMapDirectory), "%s/maps", aMapSource);
	assert(fs_makedir(aMapDirectory) == 0);
	str_format(aMapFile, sizeof(aMapFile), "%s/maps/friendly.map", aMapSource);
	WriteFile(aMapFile, "map");
	const char *pMapTemplate =
		"{\"schema_version\":1,\"content_type\":\"map\",\"published_file_id\":\"77\",\"name\":\"Friendly "
		"Map\",\"description\":\"Map\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_"
		"hash\":\"%s\",\"content_rating\":\"everyone\",\"maps\":[\"maps/friendly.map\"]}";
	str_format(aJson, sizeof(aJson), pMapTemplate, "0000000000000000000000000000000000000000000000000000000000000000");
	assert(ContentManifestParse(aJson, (int)strlen(aJson), "test", &Manifest, aError, sizeof(aError)));
	assert(ContentPackageComputeHash(aMapSource, Manifest, aHash, aError, sizeof(aError)));
	str_format(aJson, sizeof(aJson), pMapTemplate, aHash);
	str_format(aMapManifestPath, sizeof(aMapManifestPath), "%s/ninslash_content.json", aMapSource);
	WriteFile(aMapManifestPath, aJson);
	char aMapLibrary[300];
	char aMapStaged[400];
	char aFriendlyMap[400];
	char aResolvedMap[512];
	str_format(aMapLibrary, sizeof(aMapLibrary), "%s/library", aMapSource);
	assert(ContentPackageStage(
		aMapSource, aMapLibrary, "77", "test", &Manifest, aMapStaged, sizeof(aMapStaged), aError, sizeof(aError)));
	str_format(aFriendlyMap, sizeof(aFriendlyMap), "%s/friendly-map-folder", aMapLibrary);
	assert(fs_rename(aMapStaged, aFriendlyMap) == 0);
	assert(ContentPackageResolveMapLocator(aMapLibrary,
										   "workshop:77:maps/friendly.map",
										   "test",
										   aResolvedMap,
										   sizeof(aResolvedMap),
										   aError,
										   sizeof(aError)));
	assert(strstr(aResolvedMap, "/friendly-map-folder/maps/friendly.map") != 0);
	return 0;
}
