#include "weapon_packages.h"

#include "weapon_catalog.h"
#include "weapon_script_runtime.h"
#include "weapon_presentation_runtime.h"

#include <base/system.h>
#include <engine/shared/content_collection.h>
#include <engine/shared/content_package.h>
#include <engine/shared/content_package_index.h>

#include <algorithm>
#include <memory>

namespace
{
bool Fail(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText, ErrorSize);
	return false;
}

void ResetWeaponPackageState()
{
	CWeaponCatalog::ResetCustomDefinitions();
	CWeaponScriptRuntime::Reset();
	CWeaponPresentationRuntime::Reset();
}

void BeginWeaponPackageReload()
{
	CWeaponCatalog::BeginCustomDefinitionReload();
	CWeaponScriptRuntime::BeginReload();
	CWeaponPresentationRuntime::BeginReload();
}

void CommitWeaponPackageReload()
{
	CWeaponPresentationRuntime::CommitReload();
	CWeaponScriptRuntime::CommitReload();
	CWeaponCatalog::CommitCustomDefinitionReload();
}

void RollbackWeaponPackageReload()
{
	CWeaponPresentationRuntime::RollbackReload();
	CWeaponScriptRuntime::RollbackReload();
	CWeaponCatalog::RollbackCustomDefinitionReload();
}

bool EndsWith(const char *pText, const char *pSuffix)
{
	const int TextLength = str_length(pText);
	const int SuffixLength = str_length(pSuffix);
	return TextLength >= SuffixLength && str_comp(pText + TextLength - SuffixLength, pSuffix) == 0;
}

bool DeclaredResource(const CContentManifest &Manifest, const char *pPath)
{
	for(int i = 0; i < Manifest.m_FileCount; ++i)
		if(Manifest.m_aFiles[i].m_Type == CONTENT_FILE_RESOURCE && str_comp(Manifest.m_aFiles[i].m_aPath, pPath) == 0)
			return true;
	return false;
}

bool ValidateAssetLocator(
	const CContentManifest &Manifest, const char *pLocator, bool Image, char *pError, int ErrorSize)
{
	if(!pLocator[0])
		return true;
	char aPrefix[48];
	str_format(aPrefix, sizeof(aPrefix), "workshop:%s:", Manifest.m_aPublishedFileID);
	if(str_comp_num(pLocator, aPrefix, str_length(aPrefix)) != 0)
		return true; // Inherited resource owned and validated by a dependency.
	const char *pPath = pLocator + str_length(aPrefix);
	if(!DeclaredResource(Manifest, pPath))
		return Fail(pError, ErrorSize, "weapon asset is not declared as a package resource");
	if(Image ? !EndsWith(pPath, ".png") : !EndsWith(pPath, ".wv"))
		return Fail(pError, ErrorSize, Image ? "weapon images must use PNG" : "weapon sounds must use WV");
	return true;
}

bool ValidatePackageAssets(const CContentManifest &Manifest, char *pError, int ErrorSize)
{
	for(int Index = 0; Index < CWeaponCatalog::DefinitionCount(); ++Index)
	{
		CWeaponDefinition Definition;
		if(!CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition) ||
		   str_comp(Definition.m_aPackageId, Manifest.m_aPublishedFileID) != 0)
			continue;
		const char *apImages[] = {Definition.m_aHeldImage, Definition.m_aProjectileImage, Definition.m_aMuzzleImage};
		const char *apSounds[] = {Definition.m_aFireSound, Definition.m_aFireSound2, Definition.m_aExplosionSound};
		for(const char *pImage : apImages)
			if(!ValidateAssetLocator(Manifest, pImage, true, pError, ErrorSize))
				return false;
		for(const char *pSound : apSounds)
			if(!ValidateAssetLocator(Manifest, pSound, false, pError, ErrorSize))
				return false;
	}
	return true;
}

bool ReadPackageFile(const CContentCollection::CInstalledContent &Content,
					 const CContentDeclaredFile &File,
					 const char *pFailure,
					 char **ppSource,
					 int *pSize,
					 char *pError,
					 int ErrorSize)
{
	char aPath[1400];
	str_format(aPath, sizeof(aPath), "%s/%s", Content.m_aRoot, File.m_aPath);
	IOHANDLE Handle = io_open(aPath, IOFLAG_READ);
	if(!Handle)
		return Fail(pError, ErrorSize, pFailure);
	const long Size = io_length(Handle);
	if(Size <= 0 || Size > 1024 * 1024)
	{
		io_close(Handle);
		return Fail(pError, ErrorSize, "invalid weapon script or definition size");
	}
	char *pSource = static_cast<char *>(mem_alloc((unsigned)Size, 1));
	const unsigned Read = io_read(Handle, pSource, (unsigned)Size);
	io_close(Handle);
	if(Read != (unsigned)Size)
	{
		mem_free(pSource);
		return Fail(pError, ErrorSize, pFailure);
	}
	*ppSource = pSource;
	*pSize = (int)Size;
	return true;
}

template <typename TLoader>
bool LoadPackageFiles(const CContentCollection::CInstalledContent &Content,
					  int FileType,
					  const char *pSuffix,
					  const char *pReadFailure,
					  TLoader Loader,
					  char *pError,
					  int ErrorSize)
{
	const CContentDeclaredFile *apFiles[CContentManifest::MAX_FILES];
	int FileCount = 0;
	for(int FileIndex = 0; FileIndex < Content.m_Manifest.m_FileCount; ++FileIndex)
	{
		const CContentDeclaredFile &File = Content.m_Manifest.m_aFiles[FileIndex];
		if(File.m_Type != FileType || !EndsWith(File.m_aPath, pSuffix))
			continue;
		apFiles[FileCount++] = &File;
	}
	std::sort(apFiles,
			  apFiles + FileCount,
			  [](const CContentDeclaredFile *pLeft, const CContentDeclaredFile *pRight)
			  { return str_comp(pLeft->m_aPath, pRight->m_aPath) < 0; });
	for(int FileIndex = 0; FileIndex < FileCount; ++FileIndex)
	{
		const CContentDeclaredFile &File = *apFiles[FileIndex];
		char *pSource = 0;
		int Size = 0;
		if(!ReadPackageFile(Content, File, pReadFailure, &pSource, &Size, pError, ErrorSize))
			return false;
		const bool Loaded = Loader(File, pSource, Size);
		mem_free(pSource);
		if(!Loaded)
			return false;
	}
	return true;
}

struct CResolver
{
	CContentCollection m_Collection;
	CContentPackageIndex m_Index;

	bool AddRecursive(const char *pId, char *pError, int ErrorSize)
	{
		return m_Index.AddRecursive(&m_Collection, pId, CONTENT_TYPE_MOD, pError, ErrorSize);
	}
};

bool ParseRootIds(
	const char *pRootIds, const char **ppRoots, char aaRoots[][32], int *pRootCount, char *pError, int ErrorSize)
{
	*pRootCount = 0;
	const char *pCursor = pRootIds ? pRootIds : "";
	while(*pCursor && *pRootCount < 64)
	{
		int Length = 0;
		while(pCursor[Length] && pCursor[Length] != ',' && Length < 31)
		{
			aaRoots[*pRootCount][Length] = pCursor[Length];
			++Length;
		}
		aaRoots[*pRootCount][Length] = 0;
		if(!Length)
			return Fail(pError, ErrorSize, "invalid root Mod ID list");
		ppRoots[*pRootCount] = aaRoots[*pRootCount];
		++*pRootCount;
		pCursor += Length;
		if(*pCursor == ',')
			++pCursor;
		else if(*pCursor)
			return Fail(pError, ErrorSize, "invalid root Mod ID list");
	}
	if(*pCursor)
		return Fail(pError, ErrorSize, "too many root Mod IDs");
	return true;
}
} // namespace

bool WeaponPackagesResolveCollectionHash(const char *pWorkshopRoot,
										 const char *pRootIds,
										 const char *pProtocol,
										 char *pHash,
										 int HashSize,
										 char *pError,
										 int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = 0;
	if(!pHash || HashSize < 65)
		return Fail(pError, ErrorSize, "collection hash output buffer is too small");
	pHash[0] = 0;
	const char *apRoots[64];
	char aaRoots[64][32];
	int RootCount = 0;
	if(!ParseRootIds(pRootIds, apRoots, aaRoots, &RootCount, pError, ErrorSize))
		return false;
	if(!RootCount)
		return true;
	std::unique_ptr<CResolver> pResolver(new CResolver);
	if(!pResolver->m_Index.Scan(
		   pWorkshopRoot ? pWorkshopRoot : "workshop", pProtocol ? pProtocol : "", pError, ErrorSize))
		return false;
	for(int i = 0; i < RootCount; ++i)
		if(!pResolver->AddRecursive(apRoots[i], pError, ErrorSize))
			return false;
	int aOrder[CContentCollection::MAX_CONTENT];
	int OrderCount = 0;
	return pResolver->m_Collection.Resolve(apRoots, RootCount, aOrder, &OrderCount, pHash, pError, ErrorSize);
}

bool WeaponPackagesLoadCollection(const char *pWorkshopRoot,
								  const char *pRootIds,
								  const char *pProtocol,
								  const char *pExpectedHash,
								  char *pError,
								  int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = 0;
	if(!CWeaponCatalog::Initialize(pError, ErrorSize))
		return false;
	if(!pExpectedHash || !pExpectedHash[0])
	{
		if(pRootIds && pRootIds[0])
			return Fail(pError, ErrorSize, "Mod IDs require a collection hash");
		ResetWeaponPackageState();
		return true;
	}
	std::unique_ptr<CResolver> pResolver(new CResolver);
	if(!pResolver->m_Index.Scan(
		   pWorkshopRoot ? pWorkshopRoot : "workshop", pProtocol ? pProtocol : "", pError, ErrorSize))
		return false;
	const char *apRoots[64];
	char aaRoots[64][32];
	int RootCount = 0;
	if(!ParseRootIds(pRootIds, apRoots, aaRoots, &RootCount, pError, ErrorSize))
		return false;
	if(!RootCount)
		return Fail(pError, ErrorSize, "Mod hash requires at least one root Mod ID");
	for(int i = 0; i < RootCount; ++i)
		if(!pResolver->AddRecursive(apRoots[i], pError, ErrorSize))
			return false;
	int aOrder[CContentCollection::MAX_CONTENT];
	int OrderCount = 0;
	char aHash[65];
	if(!pResolver->m_Collection.Resolve(apRoots, RootCount, aOrder, &OrderCount, aHash, pError, ErrorSize))
		return false;
	if(str_comp_nocase(aHash, pExpectedHash) != 0)
		return Fail(pError, ErrorSize, "weapon collection hash mismatch");
	BeginWeaponPackageReload();
	for(int Order = 0; Order < OrderCount; ++Order)
	{
		const CContentCollection::CInstalledContent *pContent = pResolver->m_Collection.Get(aOrder[Order]);
		if(!pContent)
			continue;
		const char *apDependencies[CContentManifest::MAX_DEPENDENCIES];
		for(int i = 0; i < pContent->m_Manifest.m_DependencyCount; ++i)
			apDependencies[i] = pContent->m_Manifest.m_aDependencies[i].m_aPublishedFileID;
		const bool DefinitionsLoaded = LoadPackageFiles(
			*pContent,
			CONTENT_FILE_DEFINITION,
			".weapon.lua",
			"unable to read weapon definition",
			[&](const CContentDeclaredFile &, const char *pSource, int Size)
			{
				return CWeaponCatalog::LoadLuaDefinitions(pContent->m_Manifest.m_aPublishedFileID,
														  pContent->m_Manifest.m_Api.m_Capabilities,
														  apDependencies,
														  pContent->m_Manifest.m_DependencyCount,
														  pSource,
														  Size,
														  pError,
														  ErrorSize);
			},
			pError,
			ErrorSize);
		if(!DefinitionsLoaded)
		{
			RollbackWeaponPackageReload();
			return false;
		}
	}
	if(!CWeaponCatalog::FinalizeLuaDefinitions(pError, ErrorSize))
	{
		RollbackWeaponPackageReload();
		return false;
	}
	for(int Order = 0; Order < OrderCount; ++Order)
	{
		const CContentCollection::CInstalledContent *pContent = pResolver->m_Collection.Get(aOrder[Order]);
		if(!pContent)
			continue;
		const bool RuntimeLoaded = LoadPackageFiles(
			*pContent,
			CONTENT_FILE_SCRIPT,
			".weapon_runtime.lua",
			"unable to read weapon runtime script",
			[&](const CContentDeclaredFile &File, const char *pSource, int Size)
			{
				return CWeaponScriptRuntime::LoadPackageScript(
					pContent->m_Manifest.m_aPublishedFileID, File.m_aPath, pSource, Size, pError, ErrorSize);
			},
			pError,
			ErrorSize);
		const bool PresentationLoaded =
			RuntimeLoaded &&
			LoadPackageFiles(
				*pContent,
				CONTENT_FILE_SCRIPT,
				".weapon_presentation.lua",
				"unable to read weapon presentation script",
				[&](const CContentDeclaredFile &File, const char *pSource, int Size)
				{
					return CWeaponPresentationRuntime::LoadPackageScript(
						pContent->m_Manifest.m_aPublishedFileID, File.m_aPath, pSource, Size, pError, ErrorSize);
				},
				pError,
				ErrorSize);
		if(!PresentationLoaded)
		{
			RollbackWeaponPackageReload();
			return false;
		}
		if(!ValidatePackageAssets(pContent->m_Manifest, pError, ErrorSize))
		{
			RollbackWeaponPackageReload();
			return false;
		}
	}
	CommitWeaponPackageReload();
	return true;
}
