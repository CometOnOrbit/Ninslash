#include "mod_collection.h"
#include "mod_package.h"
#include "sha256.h"

#include <base/system.h>

namespace { bool Fail(char *pError,int ErrorSize,const char *pText){if(pError&&ErrorSize>0)str_copy(pError,pText,ErrorSize);return false;} }

CModCollection::CModCollection() { Clear(); }
void CModCollection::Clear() { m_ModCount=0; }
int CModCollection::Find(const char *pID) const { for(int i=0;i<m_ModCount;i++)if(str_comp(m_aMods[i].m_Manifest.m_aPublishedFileID,pID)==0)return i;return -1; }

bool CModCollection::AddManifest(const CModManifest &Manifest,const char *pRoot,char *pError,int ErrorSize)
{
	if(m_ModCount>=MAX_MODS||Find(Manifest.m_aPublishedFileID)>=0)return Fail(pError,ErrorSize,"duplicate or excessive installed Mod");
	m_aMods[m_ModCount].m_Manifest=Manifest;
	str_copy(m_aMods[m_ModCount].m_aRoot,pRoot?pRoot:"",sizeof(m_aMods[m_ModCount].m_aRoot));
	m_ModCount++;
	return true;
}

bool CModCollection::AddValidatedPackage(const char *pRoot,const char *pID,const char *pProtocol,char *pError,int ErrorSize)
{
	CModManifest Manifest;
	return ModPackageValidate(pRoot,pID,pProtocol,&Manifest,pError,ErrorSize)&&AddManifest(Manifest,pRoot,pError,ErrorSize);
}

bool CModCollection::Visit(int Index,int *pState,int *pOrder,int *pOrderCount,char *pError,int ErrorSize) const
{
	if(pState[Index]==2)return true;
	if(pState[Index]==1)return Fail(pError,ErrorSize,"cyclic Mod dependency");
	pState[Index]=1;
	const CModManifest &Manifest=m_aMods[Index].m_Manifest;
	for(int i=0;i<Manifest.m_DependencyCount;i++)
	{
		const CModDependency &Dependency=Manifest.m_aDependencies[i];
		const int DependencyIndex=Find(Dependency.m_aPublishedFileID);
		if(DependencyIndex<0)return Fail(pError,ErrorSize,"missing Mod dependency");
		const CModManifest &Installed=m_aMods[DependencyIndex].m_Manifest;
		if(str_comp(Installed.m_aVersion,Dependency.m_aVersion)!=0||str_comp_nocase(Installed.m_aContentHash,Dependency.m_aContentHash)!=0)return Fail(pError,ErrorSize,"Mod dependency version or hash mismatch");
		if(!Visit(DependencyIndex,pState,pOrder,pOrderCount,pError,ErrorSize))return false;
	}
	pState[Index]=2;
	pOrder[(*pOrderCount)++]=Index;
	return true;
}

bool CModCollection::Resolve(const char *const *ppRootIDs,int RootCount,int *pOrder,int *pOrderCount,char aHash[65],char *pError,int ErrorSize) const
{
	if(!pOrder||!pOrderCount||!aHash||RootCount<0||RootCount>MAX_MODS)return Fail(pError,ErrorSize,"invalid Mod collection request");
	int aState[MAX_MODS];mem_zero(aState,sizeof(aState));*pOrderCount=0;
	for(int i=0;i<RootCount;i++){const int Index=Find(ppRootIDs[i]);if(Index<0)return Fail(pError,ErrorSize,"requested Mod is not installed");if(!Visit(Index,aState,pOrder,pOrderCount,pError,ErrorSize))return false;}
	CSha256 Hash;
	for(int i=0;i<*pOrderCount;i++)
	{
		const CModManifest &Manifest=m_aMods[pOrder[i]].m_Manifest;
		Hash.Update(Manifest.m_aPublishedFileID,str_length(Manifest.m_aPublishedFileID)+1);
		Hash.Update(Manifest.m_aVersion,str_length(Manifest.m_aVersion)+1);
		Hash.Update(Manifest.m_aContentHash,str_length(Manifest.m_aContentHash)+1);
	}
	unsigned char aDigest[32];Hash.Finish(aDigest);CSha256::ToHex(aDigest,aHash);return true;
}
