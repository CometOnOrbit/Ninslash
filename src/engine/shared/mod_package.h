#ifndef ENGINE_SHARED_MOD_PACKAGE_H
#define ENGINE_SHARED_MOD_PACKAGE_H

#include "mod_manifest.h"

bool ModPackageComputeHash(const char *pRoot, const CModManifest &Manifest, char aHash[65], char *pError, int ErrorSize);
bool ModPackageValidate(const char *pRoot, const char *pExpectedPublishedFileID, const char *pExpectedProtocol, CModManifest *pManifest, char *pError, int ErrorSize);
bool ModPackageStage(const char *pSourceRoot, const char *pWorkshopRoot, const char *pExpectedPublishedFileID, const char *pExpectedProtocol, CModManifest *pManifest, char *pStagedRoot, int StagedRootSize, char *pError, int ErrorSize);

#endif
