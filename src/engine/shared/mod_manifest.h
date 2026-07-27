#ifndef ENGINE_SHARED_MOD_MANIFEST_H
#define ENGINE_SHARED_MOD_MANIFEST_H

#include "mod_api.h"

enum EModFileType { MOD_FILE_MAP, MOD_FILE_RESOURCE, MOD_FILE_SCRIPT };

struct CModDependency
{
	char m_aPublishedFileID[32];
	char m_aVersion[32];
	char m_aContentHash[65];
};

struct CModDeclaredFile
{
	char m_aPath[256];
	int m_Type;
};

struct CModManifest
{
	enum { MAX_DEPENDENCIES = 32, MAX_FILES = 256 };
	char m_aPublishedFileID[32];
	char m_aName[128];
	char m_aVersion[32];
	char m_aAuthor[128];
	char m_aTargetProtocol[128];
	char m_aContentHash[65];
	char m_aContentRating[16];
	CModApiDescriptor m_Api;
	CModDependency m_aDependencies[MAX_DEPENDENCIES];
	int m_DependencyCount;
	CModDeclaredFile m_aFiles[MAX_FILES];
	int m_FileCount;
};

/* Validation is intentionally independent of Steam UGC. Both Workshop and
 * manually installed community content must pass the same trust boundary. */
bool ModManifestIsSafeRelativePath(const char *pPath);
bool ModManifestValidateText(const char *pJson, int JsonLength, const char *pExpectedProtocol, char *pError, int ErrorSize);
bool ModManifestReadApiDescriptor(const char *pJson, int JsonLength, CModApiDescriptor *pDescriptor, char *pError, int ErrorSize);
bool ModManifestParse(const char *pJson, int JsonLength, const char *pExpectedProtocol, CModManifest *pManifest, char *pError, int ErrorSize);

#endif
