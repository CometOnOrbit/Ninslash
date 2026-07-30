#ifndef GAME_CLIENT_WEAPON_RESOURCES_H
#define GAME_CLIENT_WEAPON_RESOURCES_H

#include <game/weapon_catalog.h>

class IGraphics;
class ISound;
class IStorage;
class IConsole;

class CWeaponResources
{
	struct CEntry
	{
		WeaponDefinitionId m_Id;
		int m_HeldTexture;
		int m_ProjectileTexture;
		int m_MuzzleTexture;
		int m_FireSound;
		int m_FireSound2;
		int m_ExplosionSound;
	};
	CEntry m_aEntries[1024];
	int m_Count;

	const CEntry *Find(WeaponDefinitionId Id) const;

  public:
	CWeaponResources();
	bool Load(IGraphics *pGraphics,
			  ISound *pSound,
			  IStorage *pStorage,
			  IConsole *pConsole,
			  const char *pWorkshopRoot,
			  const char *pLanguageFile,
			  char *pError,
			  int ErrorSize);
	int HeldTexture(const CWeaponSpec &Spec) const;
	int ProjectileTexture(const CWeaponSpec &Spec) const;
	int MuzzleTexture(const CWeaponSpec &Spec) const;
	int SoundSample(const CWeaponSpec &Spec, int Slot) const;
};

extern CWeaponResources g_WeaponResources;

#endif
