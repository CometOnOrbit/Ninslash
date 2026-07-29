#ifndef ENGINE_SHARED_CONTENT_MANIFEST_H
#define ENGINE_SHARED_CONTENT_MANIFEST_H

#include "mod_api.h"

enum EContentType
{
	CONTENT_TYPE_MOD,
	CONTENT_TYPE_MAP,
	CONTENT_TYPE_ROOM_PRESET,
	CONTENT_TYPE_CHALLENGE,
	NUM_CONTENT_TYPES,
};

enum EContentFileType { CONTENT_FILE_MAP, CONTENT_FILE_RESOURCE, CONTENT_FILE_SCRIPT, CONTENT_FILE_DEFINITION };

struct CContentDependency
{
	char m_aPublishedFileID[32];
	char m_aVersion[32];
	char m_aContentHash[65];
};

struct CContentDeclaredFile
{
	char m_aPath[256];
	int m_Type;
};

struct CContentManifest
{
	enum { MAX_DEPENDENCIES = 32, MAX_FILES = 256 };
	int m_SchemaVersion;
	int m_ContentType;
	char m_aPublishedFileID[32];
	char m_aName[128];
	char m_aDescription[1024];
	char m_aVersion[32];
	char m_aAuthor[128];
	char m_aTargetProtocol[128];
	char m_aContentHash[65];
	char m_aContentRating[16];
	CModApiDescriptor m_Api;
	CContentDependency m_aDependencies[MAX_DEPENDENCIES];
	int m_DependencyCount;
	CContentDeclaredFile m_aFiles[MAX_FILES];
	int m_FileCount;
};

/* Validation is intentionally independent of Steam UGC. Both Workshop and
 * manually installed community content must pass the same trust boundary. */
bool ContentManifestIsSafeRelativePath(const char *pPath);
bool ContentManifestValidateText(const char *pJson, int JsonLength, const char *pExpectedProtocol, char *pError, int ErrorSize);
bool ContentManifestReadApiDescriptor(const char *pJson, int JsonLength, CModApiDescriptor *pDescriptor, char *pError, int ErrorSize);
bool ContentManifestParse(const char *pJson, int JsonLength, const char *pExpectedProtocol, CContentManifest *pManifest, char *pError, int ErrorSize);
const char *ContentTypeName(int Type);
bool ContentTypeFromName(const char *pName, int *pType);

#endif
