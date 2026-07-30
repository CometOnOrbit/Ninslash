#include "mod_server.h"

#include <base/system.h>
#include <game/weapon_packages.h>
#include <game/weapon_catalog.h>
#include <game/weapon_script_runtime.h>
#include <game/weapon_presentation_runtime.h>

#if defined(CONF_LUA_MODS)
#include <engine/shared/content_collection.h>
#include <engine/shared/content_package.h>
#include <engine/shared/content_package_index.h>
#include <engine/shared/mod_runtime.h>

struct CModServerRuntime::CImpl
{
	CContentCollection m_Collection;
	CContentPackageIndex m_Index;
	ILuaModRuntime *m_apRuntimes[CContentCollection::MAX_CONTENT];
	int m_RuntimeCount;

	CImpl() : m_RuntimeCount(0) { mem_zero(m_apRuntimes, sizeof(m_apRuntimes)); }

	~CImpl()
	{
		for(int i = 0; i < m_RuntimeCount; i++)
		{
			delete m_apRuntimes[i];
		}
	}

	bool AddRecursive(const char *pID, char *pError, int ErrorSize)
	{
		return m_Index.AddRecursive(&m_Collection, pID, CONTENT_TYPE_MOD, pError, ErrorSize);
	}
};

static bool IsWeaponDedicatedScript(const char *pPath)
{
	const char *apSuffixes[] = {".weapon_runtime.lua", ".weapon_presentation.lua"};
	const int Length = str_length(pPath);
	for(const char *pSuffix : apSuffixes)
	{
		const int SuffixLength = str_length(pSuffix);
		if(Length >= SuffixLength && str_comp(pPath + Length - SuffixLength, pSuffix) == 0)
			return true;
	}
	return false;
}
#else
struct CModServerRuntime::CImpl
{
};
#endif

CModServerRuntime::CModServerRuntime() : m_pImpl(0)
{
}

CModServerRuntime::~CModServerRuntime()
{
	Unload();
}

void CModServerRuntime::Unload()
{
	delete m_pImpl;
	m_pImpl = 0;
	CWeaponCatalog::ResetCustomDefinitions();
	CWeaponScriptRuntime::Reset();
	CWeaponPresentationRuntime::Reset();
}

bool CModServerRuntime::Active() const
{
#if defined(CONF_LUA_MODS)
	if(!m_pImpl || m_pImpl->m_RuntimeCount <= 0)
		return false;
	for(int i = 0; i < m_pImpl->m_RuntimeCount; i++)
	{
		if(!m_pImpl->m_apRuntimes[i]->Active())
			return false;
	}
	return true;
#else
	return false;
#endif
}

bool CModServerRuntime::Load(
	const char *pRoot, const char *pIDs, const char *pProtocol, const char *pExpectedHash, char *pError, int ErrorSize)
{
	Unload();
	if(!pExpectedHash || !pExpectedHash[0])
		return true;
	if(!WeaponPackagesLoadCollection(pRoot, pIDs, pProtocol, pExpectedHash, pError, ErrorSize))
		return false;
#if !defined(CONF_LUA_MODS)
	if(pError && ErrorSize > 0)
		str_copy(pError, "server was built without Lua Mod support", ErrorSize);
	return false;
#else
	m_pImpl = new CImpl();
	if(!m_pImpl->m_Index.Scan(pRoot ? pRoot : "workshop", pProtocol ? pProtocol : "", pError, ErrorSize))
	{
		Unload();
		return false;
	}

	const char *apRoots[64];
	char aaRoots[64][32];
	int RootCount = 0;
	const char *pCurrent = pIDs ? pIDs : "";
	while(*pCurrent && RootCount < 64)
	{
		int Length = 0;
		while(pCurrent[Length] && pCurrent[Length] != ',' && Length < 31)
		{
			aaRoots[RootCount][Length] = pCurrent[Length];
			Length++;
		}
		aaRoots[RootCount][Length] = 0;
		if(!Length)
		{
			Unload();
			if(pError && ErrorSize > 0)
				str_copy(pError, "invalid root Mod ID list", ErrorSize);
			return false;
		}
		apRoots[RootCount] = aaRoots[RootCount];
		RootCount++;
		pCurrent += Length;
		if(*pCurrent == ',')
			pCurrent++;
		else if(*pCurrent)
		{
			Unload();
			return false;
		}
	}
	if(!RootCount)
	{
		Unload();
		if(pError && ErrorSize > 0)
			str_copy(pError, "Mod hash requires at least one root Mod ID", ErrorSize);
		return false;
	}
	for(int i = 0; i < RootCount; i++)
	{
		if(!m_pImpl->AddRecursive(apRoots[i], pError, ErrorSize))
		{
			Unload();
			return false;
		}
	}

	int aOrder[64];
	int OrderCount = 0;
	char aHash[65];
	if(!m_pImpl->m_Collection.Resolve(apRoots, RootCount, aOrder, &OrderCount, aHash, pError, ErrorSize))
	{
		Unload();
		return false;
	}
	if(str_comp_nocase(aHash, pExpectedHash) != 0)
	{
		Unload();
		if(pError && ErrorSize > 0)
			str_copy(pError, "server Mod collection hash mismatch", ErrorSize);
		return false;
	}
	for(int i = 0; i < OrderCount; i++)
	{
		const CContentCollection::CInstalledContent *pMod = m_pImpl->m_Collection.Get(aOrder[i]);
		if(!pMod)
			continue;
		bool HasScript = false;
		for(int FileIndex = 0; FileIndex < pMod->m_Manifest.m_FileCount; FileIndex++)
		{
			if(pMod->m_Manifest.m_aFiles[FileIndex].m_Type == CONTENT_FILE_SCRIPT &&
			   !IsWeaponDedicatedScript(pMod->m_Manifest.m_aFiles[FileIndex].m_aPath))
				HasScript = true;
		}
		if(!HasScript)
			continue;

		ILuaModRuntime *pRuntime = CreateLuaModRuntime();
		if(!pRuntime || pRuntime->Activate(pMod->m_Manifest.m_Api) != MOD_ACTIVATION_OK)
		{
			delete pRuntime;
			Unload();
			if(pError && ErrorSize > 0)
				str_copy(pError, "unable to activate Lua Mod", ErrorSize);
			return false;
		}
		pRuntime->SetRandomSeed((unsigned)str_toint(pMod->m_Manifest.m_aPublishedFileID));
		for(int FileIndex = 0; FileIndex < pMod->m_Manifest.m_FileCount; FileIndex++)
		{
			const CContentDeclaredFile &FileEntry = pMod->m_Manifest.m_aFiles[FileIndex];
			if(FileEntry.m_Type != CONTENT_FILE_SCRIPT || IsWeaponDedicatedScript(FileEntry.m_aPath))
				continue;

			char aPath[1400];
			str_format(aPath, sizeof(aPath), "%s/%s", pMod->m_aRoot, FileEntry.m_aPath);
			IOHANDLE File = io_open(aPath, IOFLAG_READ);
			if(!File)
			{
				delete pRuntime;
				Unload();
				return false;
			}
			long Size = io_length(File);
			if(Size <= 0 || Size > 1024 * 1024)
			{
				io_close(File);
				delete pRuntime;
				Unload();
				return false;
			}
			char *pData = (char *)mem_alloc((unsigned)Size, 1);
			unsigned Read = io_read(File, pData, (unsigned)Size);
			io_close(File);
			bool Loaded =
				Read == (unsigned)Size && pRuntime->LoadScript(FileEntry.m_aPath, pData, (int)Size, pError, ErrorSize);
			mem_free(pData);
			if(!Loaded)
			{
				delete pRuntime;
				Unload();
				return false;
			}
		}
		m_pImpl->m_apRuntimes[m_pImpl->m_RuntimeCount++] = pRuntime;
	}
	return true;
#endif
}

void CModServerRuntime::Dispatch(EModEvent Event, int ClientID, int Value)
{
#if defined(CONF_LUA_MODS)
	if(!m_pImpl)
		return;
	for(int i = 0; i < m_pImpl->m_RuntimeCount; i++)
	{
		if(m_pImpl->m_apRuntimes[i]->Active())
			m_pImpl->m_apRuntimes[i]->OnModEvent(Event, ClientID, Value);
	}
#else
	(void)Event;
	(void)ClientID;
	(void)Value;
#endif
}
