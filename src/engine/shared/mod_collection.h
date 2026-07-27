#ifndef ENGINE_SHARED_MOD_COLLECTION_H
#define ENGINE_SHARED_MOD_COLLECTION_H

#include "mod_manifest.h"

class CModCollection
{
public:
	enum { MAX_MODS = 64 };
	struct CInstalledMod { CModManifest m_Manifest; char m_aRoot[1024]; };

private:
	CInstalledMod m_aMods[MAX_MODS];
	int m_ModCount;
	int Find(const char *pPublishedFileID) const;
	bool Visit(int Index, int *pState, int *pOrder, int *pOrderCount, char *pError, int ErrorSize) const;

public:
	CModCollection();
	void Clear();
	int Count() const { return m_ModCount; }
	int FindIndex(const char *pPublishedFileID) const { return Find(pPublishedFileID); }
	const CInstalledMod *Get(int Index) const { return Index >= 0 && Index < m_ModCount ? &m_aMods[Index] : 0; }
	bool AddManifest(const CModManifest &Manifest, const char *pRoot, char *pError, int ErrorSize);
	bool AddValidatedPackage(const char *pRoot, const char *pPublishedFileID, const char *pProtocol, char *pError, int ErrorSize);
	bool Resolve(const char *const *ppRootIDs, int RootCount, int *pOrder, int *pOrderCount, char aCollectionHash[65], char *pError, int ErrorSize) const;
};

#endif
