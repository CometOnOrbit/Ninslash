#ifndef GAME_WEAPON_PACKAGES_H
#define GAME_WEAPON_PACKAGES_H

// Loads only manifest-declared *.weapon.lua definition files. Gameplay scripts
// remain owned by the existing server Mod runtime.
bool WeaponPackagesLoadCollection(const char *pWorkshopRoot,
								  const char *pRootIds,
								  const char *pProtocol,
								  const char *pExpectedHash,
								  char *pError,
								  int ErrorSize);

// Resolves root IDs and dependencies in deterministic load order and computes
// the collection hash. An empty root list resolves to the unmodded collection.
bool WeaponPackagesResolveCollectionHash(const char *pWorkshopRoot,
										 const char *pRootIds,
										 const char *pProtocol,
										 char *pHash,
										 int HashSize,
										 char *pError,
										 int ErrorSize);

#endif
