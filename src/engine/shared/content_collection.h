#ifndef ENGINE_SHARED_CONTENT_COLLECTION_H
#define ENGINE_SHARED_CONTENT_COLLECTION_H

#include "content_manifest.h"

class CContentCollection
{
public:
	enum { MAX_CONTENT = 64 };
	struct CInstalledContent { CContentManifest m_Manifest; char m_aRoot[1024]; };

private:
	CInstalledContent m_aContent[MAX_CONTENT];
	int m_ContentCount;
	int Find(const char *pPublishedFileID) const;
	bool Visit(int Index, int *pState, int *pOrder, int *pOrderCount, char *pError, int ErrorSize) const;

public:
	CContentCollection();
	void Clear();
	int Count() const { return m_ContentCount; }
	int FindIndex(const char *pPublishedFileID) const { return Find(pPublishedFileID); }
	const CInstalledContent *Get(int Index) const { return Index >= 0 && Index < m_ContentCount ? &m_aContent[Index] : 0; }
	bool AddManifest(const CContentManifest &Manifest, const char *pRoot, char *pError, int ErrorSize);
	bool AddValidatedPackage(const char *pRoot, const char *pPublishedFileID, const char *pProtocol, char *pError, int ErrorSize);
	bool Resolve(const char *const *ppRootIDs, int RootCount, int *pOrder, int *pOrderCount, char aCollectionHash[65], char *pError, int ErrorSize) const;
};

#endif
