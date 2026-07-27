#include <engine/shared/mod_package.h>
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
	char aRoot[256]; str_format(aRoot, sizeof(aRoot), "/tmp/ninslash-mod-package-%lld", (long long)time_get());
	assert(fs_makedir(aRoot) == 0);
	char aMaps[300], aScripts[300], aManifestPath[300], aMapPath[300], aScriptPath[300], aExtraPath[300];
	str_format(aMaps,sizeof(aMaps),"%s/maps",aRoot); str_format(aScripts,sizeof(aScripts),"%s/rules",aRoot);
	assert(fs_makedir(aMaps)==0 && fs_makedir(aScripts)==0);
	str_format(aMapPath,sizeof(aMapPath),"%s/maps/a.map",aRoot); str_format(aScriptPath,sizeof(aScriptPath),"%s/rules/main.lua",aRoot);
	str_format(aManifestPath,sizeof(aManifestPath),"%s/ninslash_mod.json",aRoot); str_format(aExtraPath,sizeof(aExtraPath),"%s/extra.txt",aRoot);
	WriteFile(aMapPath,"map"); WriteFile(aScriptPath,"function on_event() end");
	const char *pTemplate="{\"published_file_id\":\"42\",\"name\":\"Safe\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":\"%s\",\"content_rating\":\"everyone\",\"api_version\":1,\"capabilities\":[\"resources\"],\"maps\":[\"maps/a.map\"],\"scripts\":[\"rules/main.lua\"]}";
	char aJson[2048], aError[256]; str_format(aJson,sizeof(aJson),pTemplate,"0000000000000000000000000000000000000000000000000000000000000000");
	CModManifest Manifest; assert(ModManifestParse(aJson,(int)strlen(aJson),"test",&Manifest,aError,sizeof(aError)));
	char aHash[65]; assert(ModPackageComputeHash(aRoot,Manifest,aHash,aError,sizeof(aError)));
	str_format(aJson,sizeof(aJson),pTemplate,aHash); WriteFile(aManifestPath,aJson);
	assert(ModPackageValidate(aRoot,"42","test",&Manifest,aError,sizeof(aError)));
	char aWorkshop[300], aStaged[400]; str_format(aWorkshop,sizeof(aWorkshop),"%s/workshop",aRoot);
	assert(ModPackageStage(aRoot,aWorkshop,"42","test",&Manifest,aStaged,sizeof(aStaged),aError,sizeof(aError)));
	assert(str_length(aStaged)>=3 && str_comp(aStaged+str_length(aStaged)-3,"/42")==0);
	assert(ModPackageValidate(aStaged,"42","test",&Manifest,aError,sizeof(aError)));
	assert(!ModPackageValidate(aRoot,"43","test",&Manifest,aError,sizeof(aError)));
	WriteFile(aExtraPath,"undeclared");
	assert(!ModPackageValidate(aRoot,"42","test",&Manifest,aError,sizeof(aError)));
#if defined(CONF_FAMILY_UNIX)
	assert(fs_remove(aExtraPath)==0 && fs_remove(aScriptPath)==0);
	assert(symlink(aMapPath,aScriptPath)==0);
	assert(!ModPackageValidate(aRoot,"42","test",&Manifest,aError,sizeof(aError)));
	assert(strstr(aError,"symbolic")!=0);
#endif
	return 0;
}
