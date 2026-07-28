#include "content_collection.h"
#include "content_package.h"
#include "sha256.h"

#include <base/system.h>

namespace { bool Fail(char *pError,int ErrorSize,const char *pText){if(pError&&ErrorSize>0)str_copy(pError,pText,ErrorSize);return false;} }

CContentCollection::CContentCollection() { Clear(); }
void CContentCollection::Clear() { m_ContentCount=0; }
int CContentCollection::Find(const char *pID) const { for(int i=0;i<m_ContentCount;i++)if(str_comp(m_aContent[i].m_Manifest.m_aPublishedFileID,pID)==0)return i;return -1; }

bool CContentCollection::AddManifest(const CContentManifest &Manifest,const char *pRoot,char *pError,int ErrorSize)
{
	if(m_ContentCount>=MAX_CONTENT||Find(Manifest.m_aPublishedFileID)>=0)return Fail(pError,ErrorSize,"duplicate or excessive installed content");
	m_aContent[m_ContentCount].m_Manifest=Manifest;
	str_copy(m_aContent[m_ContentCount].m_aRoot,pRoot?pRoot:"",sizeof(m_aContent[m_ContentCount].m_aRoot));
	m_ContentCount++;
	return true;
}

bool CContentCollection::AddValidatedPackage(const char *pRoot,const char *pID,const char *pProtocol,char *pError,int ErrorSize)
{
	CContentManifest Manifest;
	return ContentPackageValidate(pRoot,pID,pProtocol,&Manifest,pError,ErrorSize)&&AddManifest(Manifest,pRoot,pError,ErrorSize);
}

bool CContentCollection::Visit(int Index,int *pState,int *pOrder,int *pOrderCount,char *pError,int ErrorSize) const
{
	if(pState[Index]==2)return true;
	if(pState[Index]==1)return Fail(pError,ErrorSize,"cyclic content dependency");
	pState[Index]=1;
	const CContentManifest &Manifest=m_aContent[Index].m_Manifest;
	for(int i=0;i<Manifest.m_DependencyCount;i++)
	{
		const CContentDependency &Dependency=Manifest.m_aDependencies[i];
		const int DependencyIndex=Find(Dependency.m_aPublishedFileID);
		if(DependencyIndex<0)return Fail(pError,ErrorSize,"missing content dependency");
		const CContentManifest &Installed=m_aContent[DependencyIndex].m_Manifest;
		if(str_comp(Installed.m_aVersion,Dependency.m_aVersion)!=0||str_comp_nocase(Installed.m_aContentHash,Dependency.m_aContentHash)!=0)return Fail(pError,ErrorSize,"content dependency version or hash mismatch");
		if(!Visit(DependencyIndex,pState,pOrder,pOrderCount,pError,ErrorSize))return false;
	}
	pState[Index]=2;
	pOrder[(*pOrderCount)++]=Index;
	return true;
}

bool CContentCollection::Resolve(const char *const *ppRootIDs,int RootCount,int *pOrder,int *pOrderCount,char aHash[65],char *pError,int ErrorSize) const
{
	if(!pOrder||!pOrderCount||!aHash||RootCount<0||RootCount>MAX_CONTENT)return Fail(pError,ErrorSize,"invalid content collection request");
	int aState[MAX_CONTENT];mem_zero(aState,sizeof(aState));*pOrderCount=0;
	for(int i=0;i<RootCount;i++){const int Index=Find(ppRootIDs[i]);if(Index<0)return Fail(pError,ErrorSize,"requested content is not installed");if(!Visit(Index,aState,pOrder,pOrderCount,pError,ErrorSize))return false;}
	CSha256 Hash;
	for(int i=0;i<*pOrderCount;i++)
	{
		const CContentManifest &Manifest=m_aContent[pOrder[i]].m_Manifest;
		Hash.Update(Manifest.m_aPublishedFileID,str_length(Manifest.m_aPublishedFileID)+1);
		Hash.Update(Manifest.m_aVersion,str_length(Manifest.m_aVersion)+1);
		Hash.Update(Manifest.m_aContentHash,str_length(Manifest.m_aContentHash)+1);
	}
	unsigned char aDigest[32];Hash.Finish(aDigest);CSha256::ToHex(aDigest,aHash);return true;
}
