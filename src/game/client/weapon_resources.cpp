#include "weapon_resources.h"

#include <base/system.h>
#include <engine/graphics.h>
#include <engine/sound.h>
#include <engine/storage.h>
#include <engine/console.h>
#include <engine/shared/content_package_index.h>
#include <game/localization.h>
#include <game/version.h>

CWeaponResources g_WeaponResources;

namespace
{
bool ResolveLocator(const CContentPackageIndex &Index, const char *pLocator, char *pPath, int PathSize)
{
	if(!pLocator || str_comp_num(pLocator, "workshop:", 9) != 0)
		return false;
	const char *pId = pLocator + 9;
	const char *pSeparator = str_find(pId, ":");
	if(!pSeparator || pSeparator == pId || !pSeparator[1])
		return false;
	char aId[32];
	const int Length = (int)(pSeparator - pId);
	if(Length >= (int)sizeof(aId))
		return false;
	mem_copy(aId, pId, Length);
	aId[Length] = 0;
	const CContentPackageIndex::CEntry *pPackage = Index.Find(aId);
	if(!pPackage)
		return false;
	str_format(pPath, PathSize, "%s/%s", pPackage->m_aRoot, pSeparator + 1);
	return true;
}

int LoadTexture(IGraphics *pGraphics, const CContentPackageIndex &Index, const char *pLocator)
{
	char aPath[1400];
	return ResolveLocator(Index, pLocator, aPath, sizeof(aPath))
			   ? pGraphics->LoadTexture(
					 aPath, IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, IGraphics::TEXLOAD_NOMIPMAPS)
			   : -1;
}

int LoadSound(ISound *pSound, const CContentPackageIndex &Index, const char *pLocator)
{
	char aPath[1400];
	return ResolveLocator(Index, pLocator, aPath, sizeof(aPath)) ? pSound->LoadWV(aPath) : -1;
}

template <typename TLoader>
bool LoadDeclaredResources(
	const char *const *ppLocators, int *const *ppHandles, int Count, bool ZeroIsInvalid, TLoader Loader)
{
	for(int i = 0; i < Count; ++i)
	{
		*ppHandles[i] = Loader(ppLocators[i]);
		if(ppLocators[i][0] && (ZeroIsInvalid ? *ppHandles[i] <= 0 : *ppHandles[i] < 0))
			return false;
	}
	return true;
}

bool HasLocalizedPackage(char aaPackages[][32], int Count, const char *pPackageId)
{
	for(int i = 0; i < Count; ++i)
		if(str_comp(aaPackages[i], pPackageId) == 0)
			return true;
	return false;
}
} // namespace

CWeaponResources::CWeaponResources() : m_Count(0)
{
	mem_zero(m_aEntries, sizeof(m_aEntries));
}

const CWeaponResources::CEntry *CWeaponResources::Find(WeaponDefinitionId Id) const
{
	for(int i = 0; i < m_Count; ++i)
		if(m_aEntries[i].m_Id == Id)
			return &m_aEntries[i];
	return 0;
}

bool CWeaponResources::Load(IGraphics *pGraphics,
							ISound *pSound,
							IStorage *pStorage,
							IConsole *pConsole,
							const char *pWorkshopRoot,
							const char *pLanguageFile,
							char *pError,
							int ErrorSize)
{
	m_Count = 0;
	CContentPackageIndex PackageIndex;
	if(!PackageIndex.Scan(pWorkshopRoot, GAME_NETVERSION, pError, ErrorSize))
		return false;
	char aaLocalizedPackages[1024][32];
	int LocalizedPackageCount = 0;
	const char *pLanguageName = pLanguageFile ? pLanguageFile : "";
	for(const char *p = pLanguageName; *p; ++p)
		if(*p == '/' || *p == '\\')
			pLanguageName = p + 1;
	for(int Index = 0; Index < CWeaponCatalog::DefinitionCount(); ++Index)
	{
		CWeaponDefinition Definition;
		if(!CWeaponCatalog::TryGetDefinitionByIndex(Index, &Definition) || !Definition.m_Custom)
			continue;
		if(m_Count >= (int)(sizeof(m_aEntries) / sizeof(m_aEntries[0])))
		{
			if(pError && ErrorSize > 0)
				str_copy(pError, "custom weapon resource capacity exceeded", ErrorSize);
			return false;
		}
		CEntry &Entry = m_aEntries[m_Count++];
		Entry.m_Id = Definition.m_Id;
		const char *apTextureLocators[] = {
			Definition.m_aHeldImage, Definition.m_aProjectileImage, Definition.m_aMuzzleImage};
		int *apTextureHandles[] = {&Entry.m_HeldTexture, &Entry.m_ProjectileTexture, &Entry.m_MuzzleTexture};
		const char *apSoundLocators[] = {
			Definition.m_aFireSound, Definition.m_aFireSound2, Definition.m_aExplosionSound};
		int *apSoundHandles[] = {&Entry.m_FireSound, &Entry.m_FireSound2, &Entry.m_ExplosionSound};
		const bool TexturesLoaded =
			LoadDeclaredResources(apTextureLocators,
								  apTextureHandles,
								  3,
								  true,
								  [&](const char *pLocator) { return LoadTexture(pGraphics, PackageIndex, pLocator); });
		const bool SoundsLoaded =
			TexturesLoaded &&
			LoadDeclaredResources(apSoundLocators,
								  apSoundHandles,
								  3,
								  false,
								  [&](const char *pLocator) { return LoadSound(pSound, PackageIndex, pLocator); });
		if(!SoundsLoaded)
		{
			if(pError && ErrorSize > 0)
				str_copy(pError, "unable to decode a declared weapon PNG/WV resource", ErrorSize);
			return false;
		}
		if(!HasLocalizedPackage(aaLocalizedPackages, LocalizedPackageCount, Definition.m_aPackageId) &&
		   pLanguageName[0] && LocalizedPackageCount < 1024)
		{
			const CContentPackageIndex::CEntry *pPackage = PackageIndex.Find(Definition.m_aPackageId);
			if(!pPackage)
			{
				if(pError && ErrorSize > 0)
					str_copy(pError, "unable to resolve weapon package localization root", ErrorSize);
				return false;
			}
			char aLocalizationPath[1400];
			str_format(
				aLocalizationPath, sizeof(aLocalizationPath), "%s/localization/%s", pPackage->m_aRoot, pLanguageName);
			g_Localization.LoadOverlay(aLocalizationPath, pStorage, pConsole);
			str_copy(
				aaLocalizedPackages[LocalizedPackageCount++], Definition.m_aPackageId, sizeof(aaLocalizedPackages[0]));
		}
	}
	return true;
}

int CWeaponResources::HeldTexture(const CWeaponSpec &Spec) const
{
	const CEntry *pEntry = Find(Spec.m_DefinitionId);
	return pEntry ? pEntry->m_HeldTexture : -1;
}

int CWeaponResources::ProjectileTexture(const CWeaponSpec &Spec) const
{
	const CEntry *pEntry = Find(Spec.m_DefinitionId);
	return pEntry ? pEntry->m_ProjectileTexture : -1;
}

int CWeaponResources::MuzzleTexture(const CWeaponSpec &Spec) const
{
	const CEntry *pEntry = Find(Spec.m_DefinitionId);
	return pEntry ? pEntry->m_MuzzleTexture : -1;
}

int CWeaponResources::SoundSample(const CWeaponSpec &Spec, int Slot) const
{
	const CEntry *pEntry = Find(Spec.m_DefinitionId);
	if(!pEntry)
		return -1;
	return Slot == 0   ? pEntry->m_FireSound
		   : Slot == 1 ? pEntry->m_FireSound2
		   : Slot == 2 ? pEntry->m_ExplosionSound
					   : -1;
}
