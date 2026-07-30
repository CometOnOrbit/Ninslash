#include "content_package_index.h"

#include "content_package.h"

#include <base/system.h>

namespace
{
bool Fail(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText, ErrorSize);
	return false;
}

struct CDirectoryList
{
	char m_aaNames[CContentPackageIndex::MAX_ENTRIES][256];
	int m_Count;
};

int CollectDirectory(const char *pName, int IsDir, int DirType, void *pUser)
{
	(void)DirType;
	CDirectoryList *pList = static_cast<CDirectoryList *>(pUser);
	if(!IsDir || !pName || pName[0] == '.' || pList->m_Count >= CContentPackageIndex::MAX_ENTRIES)
		return 0;
	str_copy(pList->m_aaNames[pList->m_Count++], pName, sizeof(pList->m_aaNames[0]));
	return 0;
}
} // namespace

CContentPackageIndex::CContentPackageIndex() : m_pEntries(new CEntry[MAX_ENTRIES]), m_EntryCount(0)
{
	mem_zero(m_pEntries.get(), sizeof(CEntry) * MAX_ENTRIES);
}

bool CContentPackageIndex::Scan(const char *pWorkshopRoot, const char *pProtocol, char *pError, int ErrorSize)
{
	m_EntryCount = 0;
	mem_zero(m_pEntries.get(), sizeof(CEntry) * MAX_ENTRIES);
	if(pError && ErrorSize > 0)
		pError[0] = 0;
	if(!pWorkshopRoot || !pWorkshopRoot[0])
		return Fail(pError, ErrorSize, "invalid content package index root");

	CDirectoryList List;
	mem_zero(&List, sizeof(List));
	fs_listdir(pWorkshopRoot, CollectDirectory, 0, &List);
	for(int i = 0; i < List.m_Count; ++i)
		for(int j = i + 1; j < List.m_Count; ++j)
			if(str_comp_filenames(List.m_aaNames[i], List.m_aaNames[j]) > 0)
			{
				char aSwap[256];
				str_copy(aSwap, List.m_aaNames[i], sizeof(aSwap));
				str_copy(List.m_aaNames[i], List.m_aaNames[j], sizeof(List.m_aaNames[i]));
				str_copy(List.m_aaNames[j], aSwap, sizeof(List.m_aaNames[j]));
			}

	for(int i = 0; i < List.m_Count; ++i)
	{
		CEntry &Entry = m_pEntries[m_EntryCount++];
		str_copy(Entry.m_aDirectory, List.m_aaNames[i], sizeof(Entry.m_aDirectory));
		str_format(Entry.m_aRoot, sizeof(Entry.m_aRoot), "%s/%s", pWorkshopRoot, Entry.m_aDirectory);
		Entry.m_Valid = ContentPackageValidate(
			Entry.m_aRoot, 0, pProtocol, &Entry.m_Manifest, Entry.m_aError, sizeof(Entry.m_aError));
	}

	for(int i = 0; i < m_EntryCount; ++i)
	{
		if(!m_pEntries[i].m_Valid)
			continue;
		for(int j = i + 1; j < m_EntryCount; ++j)
		{
			if(!m_pEntries[j].m_Valid ||
			   str_comp(m_pEntries[i].m_Manifest.m_aPublishedFileID, m_pEntries[j].m_Manifest.m_aPublishedFileID) != 0)
				continue;
			m_pEntries[i].m_Valid = false;
			m_pEntries[j].m_Valid = false;
			str_copy(m_pEntries[i].m_aError,
					 "duplicate PublishedFileID in local content library",
					 sizeof(m_pEntries[i].m_aError));
			str_copy(m_pEntries[j].m_aError,
					 "duplicate PublishedFileID in local content library",
					 sizeof(m_pEntries[j].m_aError));
		}
	}
	return true;
}

const CContentPackageIndex::CEntry *CContentPackageIndex::Find(const char *pPublishedFileID) const
{
	for(int i = 0; i < m_EntryCount; ++i)
		if(m_pEntries[i].m_Valid && str_comp(m_pEntries[i].m_Manifest.m_aPublishedFileID, pPublishedFileID) == 0)
			return &m_pEntries[i];
	return 0;
}

bool CContentPackageIndex::AddRecursive(CContentCollection *pCollection,
										const char *pPublishedFileID,
										int RequiredContentType,
										char *pError,
										int ErrorSize) const
{
	if(!pCollection || !pPublishedFileID || !pPublishedFileID[0])
		return Fail(pError, ErrorSize, "invalid content package request");
	if(pCollection->FindIndex(pPublishedFileID) >= 0)
		return true;
	const CEntry *pEntry = Find(pPublishedFileID);
	if(!pEntry)
	{
		for(int i = 0; i < m_EntryCount; ++i)
			if(m_pEntries[i].m_Manifest.m_aPublishedFileID[0] &&
			   str_comp(m_pEntries[i].m_Manifest.m_aPublishedFileID, pPublishedFileID) == 0)
				return Fail(pError,
							ErrorSize,
							m_pEntries[i].m_aError[0] ? m_pEntries[i].m_aError
													  : "requested content package is invalid");
		return Fail(pError, ErrorSize, "requested content is not installed");
	}
	if(RequiredContentType >= 0 && pEntry->m_Manifest.m_ContentType != RequiredContentType)
		return Fail(pError, ErrorSize, "content package has an incompatible content type");
	if(!pCollection->AddManifest(pEntry->m_Manifest, pEntry->m_aRoot, pError, ErrorSize))
		return false;
	for(int i = 0; i < pEntry->m_Manifest.m_DependencyCount; ++i)
		if(!AddRecursive(pCollection,
						 pEntry->m_Manifest.m_aDependencies[i].m_aPublishedFileID,
						 RequiredContentType,
						 pError,
						 ErrorSize))
			return false;
	return true;
}
