#include <engine/server/mod_server.h>
#include <engine/shared/mod_collection.h>
#include <engine/shared/mod_package.h>
#include <base/system.h>

#include <assert.h>
#include <string.h>

static void WriteFile(const char *pPath, const char *pData)
{
	IOHANDLE File = io_open(pPath, IOFLAG_WRITE); assert(File);
	assert(io_write(File, pData, str_length(pData)) == (unsigned)str_length(pData));
	io_close(File);
}

static void CreatePackage(const char *pWorkshop, const char *pID, const char *pScript, const char *pDependencyID, const char *pDependencyHash)
{
	char aRoot[512], aRules[512], aScript[512], aManifest[512];
	str_format(aRoot,sizeof(aRoot),"%s/%s",pWorkshop,pID); assert(fs_makedir(aRoot)==0);
	str_format(aRules,sizeof(aRules),"%s/rules",aRoot); assert(fs_makedir(aRules)==0);
	str_format(aScript,sizeof(aScript),"%s/main.lua",aRules); WriteFile(aScript,pScript);
	str_format(aManifest,sizeof(aManifest),"%s/ninslash_mod.json",aRoot);
	const char *pDependencies = pDependencyID ? "\"dependencies\":[{\"published_file_id\":\"%s\",\"version\":\"1\",\"content_hash\":\"%s\"}]," : "%s%s";
	char aDependency[512];
	if(pDependencyID) str_format(aDependency,sizeof(aDependency),pDependencies,pDependencyID,pDependencyHash);
	else aDependency[0]=0;
	const char *pTemplate="{\"published_file_id\":\"%s\",\"name\":\"Test\",\"version\":\"1\",\"author\":\"Test\",\"target_protocol\":\"test\",\"content_hash\":\"%s\",\"content_rating\":\"everyone\",\"api_version\":1,\"capabilities\":[\"gameplay_rules\"],%s\"scripts\":[\"rules/main.lua\"]}";
	char aJson[2048], aError[256];
	str_format(aJson,sizeof(aJson),pTemplate,pID,"0000000000000000000000000000000000000000000000000000000000000000",aDependency);
	CModManifest Manifest; assert(ModManifestParse(aJson,(int)strlen(aJson),"test",&Manifest,aError,sizeof(aError)));
	char aHash[65]; assert(ModPackageComputeHash(aRoot,Manifest,aHash,aError,sizeof(aError)));
	str_format(aJson,sizeof(aJson),pTemplate,pID,aHash,aDependency); WriteFile(aManifest,aJson);
}

static void PackageHash(const char *pWorkshop, const char *pID, char aHash[65])
{
	char aRoot[512], aError[256]; str_format(aRoot,sizeof(aRoot),"%s/%s",pWorkshop,pID);
	CModManifest Manifest; assert(ModPackageValidate(aRoot,pID,"test",&Manifest,aError,sizeof(aError)));
	str_copy(aHash,Manifest.m_aContentHash,65);
}

static void CollectionHash(const char *pWorkshop, const char *pRootID, char aHash[65])
{
	CModCollection Collection; char aRoot[512], aError[256];
	for(int ID=1;ID<=4;ID++)
	{
		char aID[8]; str_format(aID,sizeof(aID),"%d",ID); str_format(aRoot,sizeof(aRoot),"%s/%s",pWorkshop,aID);
		if(fs_is_dir(aRoot)) assert(Collection.AddValidatedPackage(aRoot,aID,"test",aError,sizeof(aError)));
	}
	const char *apRoots[]={pRootID}; int aOrder[64], Count=0;
	assert(Collection.Resolve(apRoots,1,aOrder,&Count,aHash,aError,sizeof(aError)));
}

int main()
{
	char aRoot[256], aWorkshop[300]; str_format(aRoot,sizeof(aRoot),"/tmp/ninslash-mod-server-%lld",(long long)time_get()); assert(fs_makedir(aRoot)==0);
	str_format(aWorkshop,sizeof(aWorkshop),"%s/workshop",aRoot); assert(fs_makedir(aWorkshop)==0);
	CreatePackage(aWorkshop,"2","function on_event() end",0,0);
	char aDependencyHash[65]; PackageHash(aWorkshop,"2",aDependencyHash);
	CreatePackage(aWorkshop,"1","function on_event() end","2",aDependencyHash);
	char aCollectionHash[65], aError[256]; CollectionHash(aWorkshop,"1",aCollectionHash);
	CModServerRuntime Runtime;
	assert(Runtime.Load(aWorkshop,"1","test",aCollectionHash,aError,sizeof(aError)) && Runtime.Active());
	assert(!Runtime.Load(aWorkshop,"1","test","0000000000000000000000000000000000000000000000000000000000000000",aError,sizeof(aError)));
	char aDependencyPath[512], aDependencyAway[512]; str_format(aDependencyPath,sizeof(aDependencyPath),"%s/2",aWorkshop); str_format(aDependencyAway,sizeof(aDependencyAway),"%s/2-away",aWorkshop);
	assert(fs_rename(aDependencyPath,aDependencyAway)==0); assert(!Runtime.Load(aWorkshop,"1","test",aCollectionHash,aError,sizeof(aError))); assert(fs_rename(aDependencyAway,aDependencyPath)==0);

	CreatePackage(aWorkshop,"3","function on_event() error('crash') end",0,0); CollectionHash(aWorkshop,"3",aCollectionHash);
	assert(Runtime.Load(aWorkshop,"3","test",aCollectionHash,aError,sizeof(aError)) && Runtime.Active()); Runtime.Dispatch(MOD_EVENT_ROUND_START,0,0); assert(!Runtime.Active());
	CreatePackage(aWorkshop,"4","function on_event() while true do end end",0,0); CollectionHash(aWorkshop,"4",aCollectionHash);
	assert(Runtime.Load(aWorkshop,"4","test",aCollectionHash,aError,sizeof(aError)) && Runtime.Active()); Runtime.Dispatch(MOD_EVENT_ROUND_START,0,0); assert(!Runtime.Active());
	return 0;
}
