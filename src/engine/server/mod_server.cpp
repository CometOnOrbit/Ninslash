#include "mod_server.h"

#include <base/system.h>

#if defined(CONF_LUA_MODS)
#include <engine/shared/content_collection.h>
#include <engine/shared/content_package.h>
#include <engine/shared/mod_runtime.h>

struct CModServerRuntime::CImpl
{
	CContentCollection m_Collection;
	ILuaModRuntime *m_apRuntimes[CContentCollection::MAX_CONTENT];
	int m_RuntimeCount;
	char m_aRoot[1024];
	char m_aProtocol[128];
	CImpl():m_RuntimeCount(0){mem_zero(m_apRuntimes,sizeof(m_apRuntimes));}
	~CImpl(){for(int i=0;i<m_RuntimeCount;i++)delete m_apRuntimes[i];}
	bool AddRecursive(const char *pID,char *pError,int ErrorSize)
	{
		if(m_Collection.FindIndex(pID)>=0)return true;
		char aPath[1200];str_format(aPath,sizeof(aPath),"%s/%s",m_aRoot,pID);
		CContentManifest Manifest;if(!ContentPackageValidate(aPath,pID,m_aProtocol,&Manifest,pError,ErrorSize))return false;
		if(Manifest.m_ContentType != CONTENT_TYPE_MOD){if(pError&&ErrorSize>0)str_copy(pError,"Mod runtime only accepts content_type=mod",ErrorSize);return false;}
		if(!m_Collection.AddManifest(Manifest,aPath,pError,ErrorSize))return false;
		for(int i=0;i<Manifest.m_DependencyCount;i++)if(!AddRecursive(Manifest.m_aDependencies[i].m_aPublishedFileID,pError,ErrorSize))return false;
		return true;
	}
};
#else
struct CModServerRuntime::CImpl {};
#endif

CModServerRuntime::CModServerRuntime():m_pImpl(0){}
CModServerRuntime::~CModServerRuntime(){Unload();}

void CModServerRuntime::Unload(){delete m_pImpl;m_pImpl=0;}
bool CModServerRuntime::Active() const {
#if defined(CONF_LUA_MODS)
	if(!m_pImpl || m_pImpl->m_RuntimeCount <= 0) return false;
	for(int i = 0; i < m_pImpl->m_RuntimeCount; i++) if(!m_pImpl->m_apRuntimes[i]->Active()) return false;
	return true;
#else
	return false;
#endif
}

bool CModServerRuntime::Load(const char *pRoot,const char *pIDs,const char *pProtocol,const char *pExpectedHash,char *pError,int ErrorSize)
{
	Unload();
	if(!pExpectedHash||!pExpectedHash[0])return true;
#if !defined(CONF_LUA_MODS)
	if(pError&&ErrorSize>0)str_copy(pError,"server was built without Lua Mod support",ErrorSize);
	return false;
#else
	m_pImpl=new CImpl();str_copy(m_pImpl->m_aRoot,pRoot?pRoot:"workshop",sizeof(m_pImpl->m_aRoot));str_copy(m_pImpl->m_aProtocol,pProtocol?pProtocol:"",sizeof(m_pImpl->m_aProtocol));
	const char *apRoots[64];char aaRoots[64][32];int RootCount=0;const char *p=pIDs?pIDs:"";
	while(*p&&RootCount<64){int N=0;while(p[N]&&p[N]!=','&&N<31){aaRoots[RootCount][N]=p[N];N++;}aaRoots[RootCount][N]=0;if(!N){Unload();if(pError&&ErrorSize>0)str_copy(pError,"invalid root Mod ID list",ErrorSize);return false;}apRoots[RootCount]=aaRoots[RootCount];RootCount++;p+=N;if(*p==',')p++;else if(*p){Unload();return false;}}
	if(!RootCount){Unload();if(pError&&ErrorSize>0)str_copy(pError,"Mod hash requires at least one root Mod ID",ErrorSize);return false;}
	for(int i=0;i<RootCount;i++)if(!m_pImpl->AddRecursive(apRoots[i],pError,ErrorSize)){Unload();return false;}
	int aOrder[64],OrderCount=0;char aHash[65];
	if(!m_pImpl->m_Collection.Resolve(apRoots,RootCount,aOrder,&OrderCount,aHash,pError,ErrorSize)){Unload();return false;}
	if(str_comp_nocase(aHash,pExpectedHash)!=0){Unload();if(pError&&ErrorSize>0)str_copy(pError,"server Mod collection hash mismatch",ErrorSize);return false;}
	for(int i=0;i<OrderCount;i++)
	{
		const CContentCollection::CInstalledContent *pMod=m_pImpl->m_Collection.Get(aOrder[i]);if(!pMod)continue;
		bool HasScript=false;for(int f=0;f<pMod->m_Manifest.m_FileCount;f++)if(pMod->m_Manifest.m_aFiles[f].m_Type==CONTENT_FILE_SCRIPT)HasScript=true;
		if(!HasScript)continue;
		ILuaModRuntime *pRuntime=CreateLuaModRuntime();if(!pRuntime||pRuntime->Activate(pMod->m_Manifest.m_Api)!=MOD_ACTIVATION_OK){delete pRuntime;Unload();if(pError&&ErrorSize>0)str_copy(pError,"unable to activate Lua Mod",ErrorSize);return false;}
		pRuntime->SetRandomSeed((unsigned)str_toint(pMod->m_Manifest.m_aPublishedFileID));
		for(int f=0;f<pMod->m_Manifest.m_FileCount;f++)if(pMod->m_Manifest.m_aFiles[f].m_Type==CONTENT_FILE_SCRIPT)
		{
			char aPath[1400];str_format(aPath,sizeof(aPath),"%s/%s",pMod->m_aRoot,pMod->m_Manifest.m_aFiles[f].m_aPath);IOHANDLE File=io_open(aPath,IOFLAG_READ);if(!File){delete pRuntime;Unload();return false;}long Size=io_length(File);if(Size<=0||Size>1024*1024){io_close(File);delete pRuntime;Unload();return false;}char *pData=(char *)mem_alloc((unsigned)Size,1);unsigned Read=io_read(File,pData,(unsigned)Size);io_close(File);bool Loaded=Read==(unsigned)Size&&pRuntime->LoadScript(pMod->m_Manifest.m_aFiles[f].m_aPath,pData,(int)Size,pError,ErrorSize);mem_free(pData);if(!Loaded){delete pRuntime;Unload();return false;}
		}
		m_pImpl->m_apRuntimes[m_pImpl->m_RuntimeCount++]=pRuntime;
	}
	return true;
#endif
}

void CModServerRuntime::Dispatch(EModEvent Event,int ClientID,int Value)
{
#if defined(CONF_LUA_MODS)
	if(!m_pImpl)return;
	for(int i=0;i<m_pImpl->m_RuntimeCount;i++)if(m_pImpl->m_apRuntimes[i]->Active())m_pImpl->m_apRuntimes[i]->OnModEvent(Event,ClientID,Value);
#else
	(void)Event;(void)ClientID;(void)Value;
#endif
}
