#include "mod_package.h"
#include "sha256.h"

#include <base/system.h>

namespace
{
bool Error(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0) str_copy(pError, pText, ErrorSize);
	return false;
}

bool Declared(const CModManifest &Manifest, const char *pPath)
{
	for(int i = 0; i < Manifest.m_FileCount; i++) if(str_comp(Manifest.m_aFiles[i].m_aPath, pPath) == 0) return true;
	return false;
}

bool CopyFile(const char *pSource, const char *pDestination)
{
	IOHANDLE Source = io_open(pSource, IOFLAG_READ);
	if(!Source) return false;
	IOHANDLE Destination = io_open(pDestination, IOFLAG_WRITE);
	if(!Destination) { io_close(Source); return false; }
	unsigned char aBuffer[16384];
	bool Result = true;
	for(;;)
	{
		const unsigned Read = io_read(Source, aBuffer, sizeof(aBuffer));
		if(Read == 0) break;
		if(io_write(Destination, aBuffer, Read) != Read) { Result = false; break; }
	}
	io_close(Destination);
	io_close(Source);
	return Result;
}

bool MakeParents(char *pPath)
{
	for(char *p = pPath + 1; *p; p++)
	{
		if(*p != '/') continue;
		*p = 0;
		const bool Result = fs_makedir(pPath) == 0;
		*p = '/';
		if(!Result) return false;
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
	if(str_comp(pName, ".") == 0 || str_comp(pName, "..") == 0) return 0;
	CRemoveState *pState = static_cast<CRemoveState *>(pUser);
	char aPath[1800]; str_format(aPath, sizeof(aPath), "%s/%s", pState->m_aRoot, pName);
	if(fs_is_symlink(aPath) || !IsDir)
		pState->m_Ok = fs_remove(aPath) == 0 && pState->m_Ok;
	else
	{
		CRemoveState Child; str_copy(Child.m_aRoot, aPath, sizeof(Child.m_aRoot)); Child.m_Ok = true;
		fs_listdir(aPath, RemoveCallback, 0, &Child);
		pState->m_Ok = Child.m_Ok && fs_removedir(aPath) == 0 && pState->m_Ok;
	}
	return 0;
}

bool RemoveTree(const char *pRoot)
{
	if(!fs_is_dir(pRoot)) return fs_is_symlink(pRoot) ? fs_remove(pRoot) == 0 : true;
	CRemoveState State; str_copy(State.m_aRoot, pRoot, sizeof(State.m_aRoot)); State.m_Ok = true;
	fs_listdir(pRoot, RemoveCallback, 0, &State);
	return State.m_Ok && fs_removedir(pRoot) == 0;
}

struct CScanState
{
	const CModManifest *m_pManifest;
	char m_aRoot[1024];
	char m_aRelative[512];
	char *m_pError;
	int m_ErrorSize;
	bool m_Valid;
};

int ScanCallback(const char *pName, int IsDir, int DirType, void *pUser)
{
	(void)DirType;
	CScanState *pState = static_cast<CScanState *>(pUser);
	if(!pState->m_Valid || str_comp(pName, ".") == 0 || str_comp(pName, "..") == 0) return 0;
	char aRelative[512], aFull[1536];
	str_format(aRelative, sizeof(aRelative), "%s%s%s", pState->m_aRelative, pState->m_aRelative[0] ? "/" : "", pName);
	if(!ModManifestIsSafeRelativePath(aRelative)) { pState->m_Valid = Error(pState->m_pError, pState->m_ErrorSize, "unsafe file in mod package"); return 1; }
	str_format(aFull, sizeof(aFull), "%s/%s", pState->m_aRoot, aRelative);
	if(fs_is_symlink(aFull)) { pState->m_Valid = Error(pState->m_pError, pState->m_ErrorSize, "symbolic links are forbidden in mod packages"); return 1; }
	if(IsDir)
	{
		CScanState Child = *pState;
		str_copy(Child.m_aRelative, aRelative, sizeof(Child.m_aRelative));
		fs_listdir(aFull, ScanCallback, 0, &Child);
		pState->m_Valid = Child.m_Valid;
	}
	else if(str_comp(aRelative, "ninslash_mod.json") != 0 && !Declared(*pState->m_pManifest, aRelative))
		pState->m_Valid = Error(pState->m_pError, pState->m_ErrorSize, "undeclared file in mod package");
	return pState->m_Valid ? 0 : 1;
}
}

bool ModPackageComputeHash(const char *pRoot, const CModManifest &Manifest, char aHash[65], char *pError, int ErrorSize)
{
	if(!pRoot || !pRoot[0] || !aHash) return Error(pError, ErrorSize, "invalid mod package root");
	int aOrder[CModManifest::MAX_FILES];
	for(int i = 0; i < Manifest.m_FileCount; i++) aOrder[i] = i;
	for(int i = 0; i < Manifest.m_FileCount; i++) for(int j = i + 1; j < Manifest.m_FileCount; j++)
		if(str_comp(Manifest.m_aFiles[aOrder[i]].m_aPath, Manifest.m_aFiles[aOrder[j]].m_aPath) > 0) { int T=aOrder[i]; aOrder[i]=aOrder[j]; aOrder[j]=T; }
	CSha256 Hash;
	unsigned long long TotalSize = 0;
	for(int i = 0; i < Manifest.m_FileCount; i++)
	{
		const char *pPath = Manifest.m_aFiles[aOrder[i]].m_aPath;
		char aFull[1536]; str_format(aFull, sizeof(aFull), "%s/%s", pRoot, pPath);
		IOHANDLE File = io_open(aFull, IOFLAG_READ);
		if(!File) return Error(pError, ErrorSize, "declared mod file is missing");
		const long Size = io_length(File);
		if(Size < 0 || Size > 64 * 1024 * 1024 || TotalSize + (unsigned long long)Size > 256 * 1024 * 1024) { io_close(File); return Error(pError, ErrorSize, "mod package size limit exceeded"); }
		TotalSize += Size;
		Hash.Update(pPath, str_length(pPath) + 1);
		unsigned char aSize[8]; for(int Byte=0;Byte<8;Byte++) aSize[7-Byte]=(unsigned char)((unsigned long long)Size>>(Byte*8));
		Hash.Update(aSize, sizeof(aSize));
		unsigned char aBuffer[16384]; long Remaining=Size;
		while(Remaining > 0)
		{
			const unsigned Want = Remaining < (long)sizeof(aBuffer) ? (unsigned)Remaining : sizeof(aBuffer);
			const unsigned Read = io_read(File, aBuffer, Want);
			if(Read != Want) { io_close(File); return Error(pError, ErrorSize, "failed to read declared mod file"); }
			Hash.Update(aBuffer, Read); Remaining -= Read;
		}
		io_close(File);
	}
	unsigned char aDigest[32]; Hash.Finish(aDigest); CSha256::ToHex(aDigest, aHash);
	return true;
}

bool ModPackageValidate(const char *pRoot, const char *pExpectedPublishedFileID, const char *pExpectedProtocol, CModManifest *pManifest, char *pError, int ErrorSize)
{
	if(!pRoot || !pManifest) return Error(pError, ErrorSize, "invalid mod package input");
	if(fs_is_symlink(pRoot)) return Error(pError, ErrorSize, "symbolic links are forbidden in mod packages");
	char aManifestPath[1536]; str_format(aManifestPath, sizeof(aManifestPath), "%s/ninslash_mod.json", pRoot);
	IOHANDLE File = io_open(aManifestPath, IOFLAG_READ);
	if(!File) return Error(pError, ErrorSize, "missing ninslash_mod.json");
	const long Size = io_length(File);
	if(Size <= 0 || Size > 64 * 1024) { io_close(File); return Error(pError, ErrorSize, "invalid manifest size"); }
	char *pData = static_cast<char *>(mem_alloc((unsigned)Size + 1, 1));
	const unsigned Read = io_read(File, pData, (unsigned)Size); io_close(File); pData[Read] = 0;
	const bool Parsed = Read == (unsigned)Size && ModManifestParse(pData, (int)Size, pExpectedProtocol, pManifest, pError, ErrorSize);
	mem_free(pData);
	if(!Parsed) return false;
	if(pExpectedPublishedFileID && pExpectedPublishedFileID[0] && str_comp(pManifest->m_aPublishedFileID, pExpectedPublishedFileID) != 0) return Error(pError, ErrorSize, "PublishedFileID does not match install directory");
	CScanState State; State.m_pManifest=pManifest; str_copy(State.m_aRoot,pRoot,sizeof(State.m_aRoot)); State.m_aRelative[0]=0; State.m_pError=pError; State.m_ErrorSize=ErrorSize; State.m_Valid=true;
	fs_listdir(pRoot, ScanCallback, 0, &State);
	if(!State.m_Valid) return false;
	char aActualHash[65];
	if(!ModPackageComputeHash(pRoot, *pManifest, aActualHash, pError, ErrorSize)) return false;
	if(str_comp_nocase(aActualHash, pManifest->m_aContentHash) != 0) return Error(pError, ErrorSize, "mod package content hash mismatch");
	return true;
}

bool ModPackageStage(const char *pSourceRoot, const char *pWorkshopRoot, const char *pExpectedPublishedFileID, const char *pExpectedProtocol, CModManifest *pManifest, char *pStagedRoot, int StagedRootSize, char *pError, int ErrorSize)
{
	CModManifest Manifest;
	if(!pSourceRoot || !pWorkshopRoot || !pExpectedPublishedFileID || !pExpectedPublishedFileID[0] ||
		!ModPackageValidate(pSourceRoot, pExpectedPublishedFileID, pExpectedProtocol, &Manifest, pError, ErrorSize)) return false;
	if(fs_makedir(pWorkshopRoot) != 0) return Error(pError, ErrorSize, "unable to create Workshop storage directory");

	char aTemporary[1536], aTarget[1536], aBackup[1536];
	str_format(aTemporary, sizeof(aTemporary), "%s/.%s-%s.tmp", pWorkshopRoot, pExpectedPublishedFileID, Manifest.m_aContentHash);
	str_format(aTarget, sizeof(aTarget), "%s/%s", pWorkshopRoot, pExpectedPublishedFileID);
	str_format(aBackup, sizeof(aBackup), "%s/.%s.backup", pWorkshopRoot, pExpectedPublishedFileID);
	RemoveTree(aTemporary);
	if(fs_makedir(aTemporary) != 0) return Error(pError, ErrorSize, "unable to create Workshop staging directory");

	char aSource[1800], aDestination[1800];
	str_format(aSource, sizeof(aSource), "%s/ninslash_mod.json", pSourceRoot);
	str_format(aDestination, sizeof(aDestination), "%s/ninslash_mod.json", aTemporary);
	bool Copied = CopyFile(aSource, aDestination);
	for(int i = 0; Copied && i < Manifest.m_FileCount; i++)
	{
		str_format(aSource, sizeof(aSource), "%s/%s", pSourceRoot, Manifest.m_aFiles[i].m_aPath);
		str_format(aDestination, sizeof(aDestination), "%s/%s", aTemporary, Manifest.m_aFiles[i].m_aPath);
		Copied = MakeParents(aDestination) && CopyFile(aSource, aDestination);
	}
	CModManifest StagedManifest;
	if(!Copied || !ModPackageValidate(aTemporary, pExpectedPublishedFileID, pExpectedProtocol, &StagedManifest, pError, ErrorSize))
	{
		RemoveTree(aTemporary);
		return Copied ? false : Error(pError, ErrorSize, "failed to copy Workshop package into staging");
	}

	RemoveTree(aBackup);
	const bool HadTarget = fs_is_dir(aTarget) || fs_is_symlink(aTarget);
	if(HadTarget && fs_rename(aTarget, aBackup) != 0) { RemoveTree(aTemporary); return Error(pError, ErrorSize, "unable to replace installed Workshop package"); }
	if(fs_rename(aTemporary, aTarget) != 0)
	{
		if(HadTarget) fs_rename(aBackup, aTarget);
		RemoveTree(aTemporary);
		return Error(pError, ErrorSize, "unable to activate staged Workshop package");
	}
	if(HadTarget) RemoveTree(aBackup);
	if(pManifest) *pManifest = StagedManifest;
	if(pStagedRoot && StagedRootSize > 0) str_copy(pStagedRoot, aTarget, StagedRootSize);
	return true;
}
