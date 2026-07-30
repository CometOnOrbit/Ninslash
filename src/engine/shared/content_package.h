#ifndef ENGINE_SHARED_CONTENT_PACKAGE_H
#define ENGINE_SHARED_CONTENT_PACKAGE_H

#include "content_manifest.h"

bool ContentPackageComputeHash(
	const char *pRoot, const CContentManifest &Manifest, char aHash[65], char *pError, int ErrorSize);
bool ContentPackageValidate(const char *pRoot,
							const char *pExpectedPublishedFileID,
							const char *pExpectedProtocol,
							CContentManifest *pManifest,
							char *pError,
							int ErrorSize);
bool ContentPackageStage(const char *pSourceRoot,
						 const char *pWorkshopRoot,
						 const char *pExpectedPublishedFileID,
						 const char *pExpectedProtocol,
						 CContentManifest *pManifest,
						 char *pStagedRoot,
						 int StagedRootSize,
						 char *pError,
						 int ErrorSize);
// Resolves workshop:<PublishedFileID>:<entry> without allowing an official map
// name or a path outside the validated package to be substituted.
bool ContentPackageResolveMapLocator(const char *pWorkshopRoot,
									 const char *pLocator,
									 const char *pExpectedProtocol,
									 char *pPath,
									 int PathSize,
									 char *pError,
									 int ErrorSize);

#endif
