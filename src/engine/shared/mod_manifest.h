#ifndef ENGINE_SHARED_MOD_MANIFEST_H
#define ENGINE_SHARED_MOD_MANIFEST_H

#include "mod_api.h"

/* Validation is intentionally independent of Steam UGC. Both Workshop and
 * manually installed community content must pass the same trust boundary. */
bool ModManifestIsSafeRelativePath(const char *pPath);
bool ModManifestValidateText(const char *pJson, int JsonLength, const char *pExpectedProtocol, char *pError, int ErrorSize);
bool ModManifestReadApiDescriptor(const char *pJson, int JsonLength, CModApiDescriptor *pDescriptor, char *pError, int ErrorSize);

#endif
