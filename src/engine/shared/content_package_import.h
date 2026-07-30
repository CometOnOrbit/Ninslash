#ifndef ENGINE_SHARED_CONTENT_PACKAGE_IMPORT_H
#define ENGINE_SHARED_CONTENT_PACKAGE_IMPORT_H

enum EContentPackageImportStatus
{
	CONTENT_IMPORT_INSTALLED = 1,
	CONTENT_IMPORT_ALREADY_INSTALLED,
	CONTENT_IMPORT_REPLACE_REQUIRED,
};

struct CContentPackageImportResult
{
	int m_Status;
	bool m_Replaced;
	char m_aPublishedFileID[32];
	char m_aName[128];
	char m_aVersion[32];
	char m_aContentHash[65];
	char m_aPreviousVersion[32];
	char m_aPreviousHash[65];
	char m_aTargetRoot[1024];
	char m_aBackupRoot[1024];
};

bool ContentPackageImportZip(const char *pArchivePath,
							 const char *pWorkshopRoot,
							 const char *pProtocol,
							 bool ReplaceExisting,
							 CContentPackageImportResult *pResult,
							 char *pError,
							 int ErrorSize);
bool ContentPackageFinalizeImport(CContentPackageImportResult *pResult, char *pError, int ErrorSize);
bool ContentPackageRollbackImport(CContentPackageImportResult *pResult, char *pError, int ErrorSize);

#endif
