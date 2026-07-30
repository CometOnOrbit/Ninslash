#include "content_package_import.h"

#include "content_manifest.h"
#include "content_package.h"
#include "content_package_index.h"

#include <base/system.h>
#include <base/math.h>

#include <engine/external/minizip/unzip.h>

#include <ctype.h>
#include <memory>

namespace
{
enum
{
	MAX_ARCHIVE_ENTRIES = CContentManifest::MAX_FILES + 64,
	MAX_ARCHIVE_PATH = 512,
};

struct CArchiveEntry
{
	unz64_file_pos m_Position;
	unz_file_info64 m_Info;
	char m_aPath[MAX_ARCHIVE_PATH];
	bool m_Directory;
};

bool Fail(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText, ErrorSize);
	return false;
}

bool MakeParents(char *pPath)
{
	for(char *p = pPath + 1; *p; ++p)
	{
		if(*p != '/')
			continue;
		*p = 0;
		const bool Ok = fs_makedir(pPath) == 0;
		*p = '/';
		if(!Ok)
			return false;
	}
	return true;
}

struct CRemoveState
{
	char m_aRoot[1536];
	bool m_Ok;
};

int RemoveCallback(const char *pName, int IsDir, int DirType, void *pUser)
{
	(void)DirType;
	if(str_comp(pName, ".") == 0 || str_comp(pName, "..") == 0)
		return 0;
	CRemoveState *pState = static_cast<CRemoveState *>(pUser);
	char aPath[1800];
	str_format(aPath, sizeof(aPath), "%s/%s", pState->m_aRoot, pName);
	if(fs_is_symlink(aPath) || !IsDir)
		pState->m_Ok = fs_remove(aPath) == 0 && pState->m_Ok;
	else
	{
		CRemoveState Child;
		str_copy(Child.m_aRoot, aPath, sizeof(Child.m_aRoot));
		Child.m_Ok = true;
		fs_listdir(aPath, RemoveCallback, 0, &Child);
		pState->m_Ok = Child.m_Ok && fs_removedir(aPath) == 0 && pState->m_Ok;
	}
	return 0;
}

bool RemoveTree(const char *pRoot)
{
	if(!pRoot || !pRoot[0])
		return false;
	if(!fs_is_dir(pRoot))
		return fs_is_symlink(pRoot) ? fs_remove(pRoot) == 0 : true;
	CRemoveState State;
	str_copy(State.m_aRoot, pRoot, sizeof(State.m_aRoot));
	State.m_Ok = true;
	fs_listdir(pRoot, RemoveCallback, 0, &State);
	return State.m_Ok && fs_removedir(pRoot) == 0;
}

bool IsDeclared(const CContentManifest &Manifest, const char *pPath)
{
	for(int i = 0; i < Manifest.m_FileCount; ++i)
		if(str_comp(Manifest.m_aFiles[i].m_aPath, pPath) == 0)
			return true;
	return false;
}

bool ReadCurrentFile(
	unzFile Archive, const CArchiveEntry &Entry, void *pBuffer, unsigned BufferSize, char *pError, int ErrorSize)
{
	if(Entry.m_Info.uncompressed_size != BufferSize || unzGoToFilePos64(Archive, &Entry.m_Position) != UNZ_OK ||
	   unzOpenCurrentFile(Archive) != UNZ_OK)
		return Fail(pError, ErrorSize, "unable to open ZIP entry");
	unsigned char *pBytes = static_cast<unsigned char *>(pBuffer);
	unsigned Offset = 0;
	while(Offset < BufferSize)
	{
		const unsigned Want = min((unsigned)16384, BufferSize - Offset);
		const int Read = unzReadCurrentFile(Archive, pBytes + Offset, Want);
		if(Read <= 0)
		{
			unzCloseCurrentFile(Archive);
			return Fail(pError, ErrorSize, "unable to read ZIP entry");
		}
		Offset += (unsigned)Read;
	}
	if(unzCloseCurrentFile(Archive) != UNZ_OK)
		return Fail(pError, ErrorSize, "ZIP entry checksum mismatch");
	return true;
}

bool ExtractCurrentFile(
	unzFile Archive, const CArchiveEntry &Entry, const char *pDestination, char *pError, int ErrorSize)
{
	if(unzGoToFilePos64(Archive, &Entry.m_Position) != UNZ_OK || unzOpenCurrentFile(Archive) != UNZ_OK)
		return Fail(pError, ErrorSize, "unable to open ZIP entry");
	char aPath[1536];
	str_copy(aPath, pDestination, sizeof(aPath));
	if(!MakeParents(aPath))
	{
		unzCloseCurrentFile(Archive);
		return Fail(pError, ErrorSize, "unable to create imported package directory");
	}
	IOHANDLE File = io_open(pDestination, IOFLAG_WRITE);
	if(!File)
	{
		unzCloseCurrentFile(Archive);
		return Fail(pError, ErrorSize, "unable to create imported package file");
	}
	unsigned char aBuffer[16384];
	unsigned long long Written = 0;
	bool Ok = true;
	for(;;)
	{
		const int Read = unzReadCurrentFile(Archive, aBuffer, sizeof(aBuffer));
		if(Read < 0)
		{
			Ok = false;
			break;
		}
		if(Read == 0)
			break;
		if(Written + (unsigned)Read > Entry.m_Info.uncompressed_size)
		{
			Ok = false;
			break;
		}
		if(io_write(File, aBuffer, (unsigned)Read) != (unsigned)Read)
		{
			Ok = false;
			break;
		}
		Written += (unsigned)Read;
	}
	io_close(File);
	const int CloseResult = unzCloseCurrentFile(Archive);
	if(!Ok || Written != Entry.m_Info.uncompressed_size || CloseResult != UNZ_OK)
	{
		fs_remove(pDestination);
		return Fail(pError, ErrorSize, "failed to extract or verify ZIP entry");
	}
	return true;
}

void MakeInstallName(const char *pName, const char *pID, char *pBuffer, int BufferSize)
{
	int Out = 0;
	bool Separator = false;
	for(const unsigned char *p = reinterpret_cast<const unsigned char *>(pName); *p && Out < min(BufferSize - 1, 48);
		++p)
	{
		if(*p < 128 && isalnum(*p))
		{
			if(Separator && Out > 0 && Out < BufferSize - 1)
				pBuffer[Out++] = '-';
			pBuffer[Out++] = (char)tolower(*p);
			Separator = false;
		}
		else if(Out > 0)
			Separator = true;
	}
	while(Out > 0 && pBuffer[Out - 1] == '-')
		--Out;
	pBuffer[Out] = 0;
	if(!Out)
		str_format(pBuffer, BufferSize, "mod-%s", pID);
}
} // namespace

bool ContentPackageImportZip(const char *pArchivePath,
							 const char *pWorkshopRoot,
							 const char *pProtocol,
							 bool ReplaceExisting,
							 CContentPackageImportResult *pResult,
							 char *pError,
							 int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = 0;
	if(pResult)
		mem_zero(pResult, sizeof(*pResult));
	if(!pArchivePath || !pArchivePath[0] || !pWorkshopRoot || !pWorkshopRoot[0] || !pResult)
		return Fail(pError, ErrorSize, "invalid ZIP import request");

	unzFile Archive = unzOpen64(pArchivePath);
	if(!Archive)
		return Fail(pError, ErrorSize, "unable to open ZIP archive");
	std::unique_ptr<CArchiveEntry[]> pEntries(new CArchiveEntry[MAX_ARCHIVE_ENTRIES]);
	mem_zero(pEntries.get(), sizeof(CArchiveEntry) * MAX_ARCHIVE_ENTRIES);
	int EntryCount = 0;
	unsigned long long TotalSize = 0;
	int Result = unzGoToFirstFile(Archive);
	while(Result == UNZ_OK)
	{
		if(EntryCount >= MAX_ARCHIVE_ENTRIES)
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "ZIP archive contains too many entries");
		}
		CArchiveEntry &Entry = pEntries[EntryCount];
		if(unzGetCurrentFileInfo64(Archive, &Entry.m_Info, Entry.m_aPath, sizeof(Entry.m_aPath), 0, 0, 0, 0) !=
			   UNZ_OK ||
		   Entry.m_Info.size_filename == 0 || Entry.m_Info.size_filename >= sizeof(Entry.m_aPath) ||
		   str_length(Entry.m_aPath) != (int)Entry.m_Info.size_filename)
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "invalid ZIP entry name");
		}
		Entry.m_Directory = Entry.m_aPath[str_length(Entry.m_aPath) - 1] == '/';
		const unsigned UnixMode = Entry.m_Info.external_fa >> 16;
		const unsigned UnixType = UnixMode & 0170000;
		if((Entry.m_Info.flag & 1) != 0 || Entry.m_Info.disk_num_start != 0 ||
		   (Entry.m_Info.compression_method != 0 && Entry.m_Info.compression_method != Z_DEFLATED) ||
		   (UnixType != 0 && UnixType != 0100000 && UnixType != 0040000) ||
		   (UnixType == 0040000) != Entry.m_Directory || str_find(Entry.m_aPath, "\\"))
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "unsupported or unsafe ZIP entry");
		}
		if(!Entry.m_Directory)
		{
			if(Entry.m_Info.uncompressed_size > 64ULL * 1024 * 1024 ||
			   TotalSize + Entry.m_Info.uncompressed_size > 256ULL * 1024 * 1024)
			{
				unzClose(Archive);
				return Fail(pError, ErrorSize, "ZIP archive size limit exceeded");
			}
			TotalSize += Entry.m_Info.uncompressed_size;
		}
		unzGetFilePos64(Archive, &Entry.m_Position);
		++EntryCount;
		Result = unzGoToNextFile(Archive);
	}
	if(Result != UNZ_END_OF_LIST_OF_FILE || EntryCount == 0)
	{
		unzClose(Archive);
		return Fail(pError, ErrorSize, "invalid ZIP archive directory");
	}

	int ManifestIndex = -1;
	char aPrefix[MAX_ARCHIVE_PATH] = "";
	for(int i = 0; i < EntryCount; ++i)
	{
		if(pEntries[i].m_Directory)
			continue;
		const char *pPath = pEntries[i].m_aPath;
		const char *pSlash = str_find(pPath, "/");
		const bool RootManifest = str_comp(pPath, "ninslash_content.json") == 0;
		const bool NestedManifest =
			pSlash && !str_find(pSlash + 1, "/") && str_comp(pSlash + 1, "ninslash_content.json") == 0;
		if(!RootManifest && !NestedManifest)
			continue;
		if(ManifestIndex >= 0)
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "ZIP archive contains multiple package manifests");
		}
		ManifestIndex = i;
		if(NestedManifest)
		{
			const int PrefixLength = (int)(pSlash - pPath) + 1;
			mem_copy(aPrefix, pPath, PrefixLength);
			aPrefix[PrefixLength] = 0;
		}
	}
	if(ManifestIndex < 0)
	{
		unzClose(Archive);
		return Fail(pError, ErrorSize, "ZIP archive is missing ninslash_content.json");
	}
	const int PrefixLength = str_length(aPrefix);
	for(int i = 0; i < EntryCount; ++i)
	{
		const char *pPath = pEntries[i].m_aPath;
		if(PrefixLength && str_comp_num(pPath, aPrefix, PrefixLength) != 0)
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "ZIP archive must contain one package root");
		}
		const char *pRelative = pPath + PrefixLength;
		if(!pRelative[0])
			continue;
		char aCheck[MAX_ARCHIVE_PATH];
		str_copy(aCheck, pRelative, sizeof(aCheck));
		if(pEntries[i].m_Directory)
			aCheck[str_length(aCheck) - 1] = 0;
		if(aCheck[0] && !ContentManifestIsSafeRelativePath(aCheck))
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "unsafe path in ZIP archive");
		}
		if(!pEntries[i].m_Directory)
			for(int j = 0; j < i; ++j)
				if(!pEntries[j].m_Directory && str_comp_nocase(pEntries[j].m_aPath + PrefixLength, pRelative) == 0)
				{
					unzClose(Archive);
					return Fail(pError, ErrorSize, "ZIP archive contains duplicate file paths");
				}
	}

	const CArchiveEntry &ManifestEntry = pEntries[ManifestIndex];
	if(ManifestEntry.m_Info.uncompressed_size == 0 || ManifestEntry.m_Info.uncompressed_size > 64 * 1024)
	{
		unzClose(Archive);
		return Fail(pError, ErrorSize, "invalid manifest size in ZIP archive");
	}
	std::unique_ptr<char[]> pManifestJson(new char[(unsigned)ManifestEntry.m_Info.uncompressed_size + 1]);
	if(!ReadCurrentFile(Archive,
						ManifestEntry,
						pManifestJson.get(),
						(unsigned)ManifestEntry.m_Info.uncompressed_size,
						pError,
						ErrorSize))
	{
		unzClose(Archive);
		return false;
	}
	pManifestJson[(unsigned)ManifestEntry.m_Info.uncompressed_size] = 0;
	CContentManifest Manifest;
	if(!ContentManifestParse(
		   pManifestJson.get(), (int)ManifestEntry.m_Info.uncompressed_size, pProtocol, &Manifest, pError, ErrorSize))
	{
		unzClose(Archive);
		return false;
	}
	bool aFound[CContentManifest::MAX_FILES];
	mem_zero(aFound, sizeof(aFound));
	for(int i = 0; i < EntryCount; ++i)
	{
		if(pEntries[i].m_Directory)
			continue;
		const char *pRelative = pEntries[i].m_aPath + PrefixLength;
		if(str_comp(pRelative, "ninslash_content.json") == 0)
			continue;
		int DeclaredIndex = -1;
		for(int FileIndex = 0; FileIndex < Manifest.m_FileCount; ++FileIndex)
			if(str_comp(Manifest.m_aFiles[FileIndex].m_aPath, pRelative) == 0)
			{
				DeclaredIndex = FileIndex;
				break;
			}
		if(DeclaredIndex < 0 || aFound[DeclaredIndex])
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "ZIP archive contains undeclared or duplicate files");
		}
		aFound[DeclaredIndex] = true;
	}
	for(int i = 0; i < Manifest.m_FileCount; ++i)
	{
		if(!aFound[i])
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "ZIP archive is missing a declared file");
		}
	}

	str_copy(pResult->m_aPublishedFileID, Manifest.m_aPublishedFileID, sizeof(pResult->m_aPublishedFileID));
	str_copy(pResult->m_aName, Manifest.m_aName, sizeof(pResult->m_aName));
	str_copy(pResult->m_aVersion, Manifest.m_aVersion, sizeof(pResult->m_aVersion));
	str_copy(pResult->m_aContentHash, Manifest.m_aContentHash, sizeof(pResult->m_aContentHash));
	if(fs_makedir(pWorkshopRoot) != 0)
	{
		unzClose(Archive);
		return Fail(pError, ErrorSize, "unable to create local content directory");
	}
	CContentPackageIndex Index;
	char aIndexError[256];
	if(!Index.Scan(pWorkshopRoot, pProtocol, aIndexError, sizeof(aIndexError)))
	{
		unzClose(Archive);
		return Fail(pError, ErrorSize, aIndexError);
	}
	const CContentPackageIndex::CEntry *pExisting = 0;
	int MatchingEntries = 0;
	for(int i = 0; i < Index.Count(); ++i)
	{
		const CContentPackageIndex::CEntry *pEntry = Index.Get(i);
		if(pEntry && str_comp(pEntry->m_Manifest.m_aPublishedFileID, Manifest.m_aPublishedFileID) == 0)
		{
			pExisting = pEntry;
			++MatchingEntries;
		}
	}
	if(MatchingEntries > 1)
	{
		unzClose(Archive);
		return Fail(pError, ErrorSize, "duplicate PublishedFileID must be resolved before importing");
	}
	if(pExisting)
	{
		str_copy(pResult->m_aPreviousVersion, pExisting->m_Manifest.m_aVersion, sizeof(pResult->m_aPreviousVersion));
		str_copy(pResult->m_aPreviousHash, pExisting->m_Manifest.m_aContentHash, sizeof(pResult->m_aPreviousHash));
		str_copy(pResult->m_aTargetRoot, pExisting->m_aRoot, sizeof(pResult->m_aTargetRoot));
		if(str_comp_nocase(Manifest.m_aContentHash, pExisting->m_Manifest.m_aContentHash) == 0)
		{
			pResult->m_Status = CONTENT_IMPORT_ALREADY_INSTALLED;
			unzClose(Archive);
			return true;
		}
		if(!ReplaceExisting)
		{
			pResult->m_Status = CONTENT_IMPORT_REPLACE_REQUIRED;
			unzClose(Archive);
			return true;
		}
	}
	else
	{
		char aInstallName[96];
		MakeInstallName(Manifest.m_aName, Manifest.m_aPublishedFileID, aInstallName, sizeof(aInstallName));
		str_format(pResult->m_aTargetRoot, sizeof(pResult->m_aTargetRoot), "%s/%s", pWorkshopRoot, aInstallName);
		if(fs_is_dir(pResult->m_aTargetRoot) || fs_is_symlink(pResult->m_aTargetRoot))
			str_format(pResult->m_aTargetRoot,
					   sizeof(pResult->m_aTargetRoot),
					   "%s/%s-%s",
					   pWorkshopRoot,
					   aInstallName,
					   Manifest.m_aPublishedFileID);
		if(fs_is_dir(pResult->m_aTargetRoot) || fs_is_symlink(pResult->m_aTargetRoot))
		{
			unzClose(Archive);
			return Fail(pError, ErrorSize, "unable to choose a unique local content directory");
		}
	}

	char aTemporary[1536];
	str_format(aTemporary,
			   sizeof(aTemporary),
			   "%s/.import-%s-%.16s.tmp",
			   pWorkshopRoot,
			   Manifest.m_aPublishedFileID,
			   Manifest.m_aContentHash);
	RemoveTree(aTemporary);
	if(fs_makedir(aTemporary) != 0)
	{
		unzClose(Archive);
		return Fail(pError, ErrorSize, "unable to create ZIP import staging directory");
	}
	for(int i = 0; i < EntryCount; ++i)
	{
		if(pEntries[i].m_Directory)
			continue;
		const char *pRelative = pEntries[i].m_aPath + PrefixLength;
		if(str_comp(pRelative, "ninslash_content.json") != 0 && !IsDeclared(Manifest, pRelative))
			continue;
		char aDestination[1800];
		str_format(aDestination, sizeof(aDestination), "%s/%s", aTemporary, pRelative);
		if(!ExtractCurrentFile(Archive, pEntries[i], aDestination, pError, ErrorSize))
		{
			unzClose(Archive);
			RemoveTree(aTemporary);
			return false;
		}
	}
	unzClose(Archive);
	CContentManifest StagedManifest;
	if(!ContentPackageValidate(aTemporary, Manifest.m_aPublishedFileID, pProtocol, &StagedManifest, pError, ErrorSize))
	{
		RemoveTree(aTemporary);
		return false;
	}

	str_format(pResult->m_aBackupRoot,
			   sizeof(pResult->m_aBackupRoot),
			   "%s/.import-%s.backup",
			   pWorkshopRoot,
			   Manifest.m_aPublishedFileID);
	RemoveTree(pResult->m_aBackupRoot);
	const bool HadTarget = fs_is_dir(pResult->m_aTargetRoot) || fs_is_symlink(pResult->m_aTargetRoot);
	if(HadTarget && fs_rename(pResult->m_aTargetRoot, pResult->m_aBackupRoot) != 0)
	{
		RemoveTree(aTemporary);
		return Fail(pError, ErrorSize, "unable to back up installed content package");
	}
	if(fs_rename(aTemporary, pResult->m_aTargetRoot) != 0)
	{
		if(HadTarget)
			fs_rename(pResult->m_aBackupRoot, pResult->m_aTargetRoot);
		RemoveTree(aTemporary);
		return Fail(pError, ErrorSize, "unable to activate imported content package");
	}
	pResult->m_Replaced = HadTarget;
	pResult->m_Status = CONTENT_IMPORT_INSTALLED;
	return true;
}

bool ContentPackageFinalizeImport(CContentPackageImportResult *pResult, char *pError, int ErrorSize)
{
	if(!pResult || pResult->m_Status != CONTENT_IMPORT_INSTALLED)
		return Fail(pError, ErrorSize, "no imported content package to finalize");
	if(pResult->m_aBackupRoot[0] && !RemoveTree(pResult->m_aBackupRoot))
		return Fail(pError, ErrorSize, "unable to remove content package backup");
	pResult->m_aBackupRoot[0] = 0;
	return true;
}

bool ContentPackageRollbackImport(CContentPackageImportResult *pResult, char *pError, int ErrorSize)
{
	if(!pResult || pResult->m_Status != CONTENT_IMPORT_INSTALLED || !pResult->m_aTargetRoot[0])
		return Fail(pError, ErrorSize, "no imported content package to roll back");
	if(!RemoveTree(pResult->m_aTargetRoot))
		return Fail(pError, ErrorSize, "unable to remove failed imported content package");
	if(pResult->m_Replaced && fs_rename(pResult->m_aBackupRoot, pResult->m_aTargetRoot) != 0)
		return Fail(pError, ErrorSize, "unable to restore previous content package");
	pResult->m_aBackupRoot[0] = 0;
	return true;
}
