#ifndef ENGINE_SHARED_CONTENT_PACKAGE_INDEX_H
#define ENGINE_SHARED_CONTENT_PACKAGE_INDEX_H

#include "content_collection.h"
#include "content_manifest.h"

#include <memory>

class CContentPackageIndex
{
  public:
	enum
	{
		MAX_ENTRIES = 256
	};
	struct CEntry
	{
		CContentManifest m_Manifest;
		char m_aDirectory[256];
		char m_aRoot[1024];
		char m_aError[256];
		bool m_Valid;
	};

  private:
	std::unique_ptr<CEntry[]> m_pEntries;
	int m_EntryCount;

  public:
	CContentPackageIndex();
	bool Scan(const char *pWorkshopRoot, const char *pProtocol, char *pError, int ErrorSize);
	int Count() const { return m_EntryCount; }
	const CEntry *Get(int Index) const { return Index >= 0 && Index < m_EntryCount ? &m_pEntries[Index] : 0; }
	const CEntry *Find(const char *pPublishedFileID) const;
	bool AddRecursive(CContentCollection *pCollection,
					  const char *pPublishedFileID,
					  int RequiredContentType,
					  char *pError,
					  int ErrorSize) const;
};

#endif
