#ifndef ENGINE_SERVER_SERVER_MOD_MANAGER_H
#define ENGINE_SERVER_SERVER_MOD_MANAGER_H

#include <engine/shared/content_package_import.h>

#include <string>
#include <vector>

class CServerModManager
{
  public:
	struct CProfile
	{
		std::string m_Name;
		std::vector<std::string> m_RootIds;
	};

	enum EImportState
	{
		IMPORT_IDLE,
		IMPORT_RUNNING,
		IMPORT_FINISHED,
	};

  private:
	std::string m_SaveRoot;
	std::string m_WorkshopRoot;
	std::string m_Protocol;
	std::vector<CProfile> m_Profiles;
	std::string m_SelectedProfile;
	std::string m_AppliedProfile;
	std::string m_AppliedIds;
	std::string m_AppliedHash;

	struct CImportTask;
	CImportTask *m_pImport;
	void *m_pImportThread;
	CContentPackageImportResult m_LastImport;
	std::string m_LastImportError;
	bool m_LastImportSucceeded;
	bool m_HasLastImport;
	bool m_PendingImportTransaction;

	CProfile *FindProfile(const char *pName);
	const CProfile *FindProfile(const char *pName) const;
	bool Save(char *pError, int ErrorSize) const;
	bool SaveTransaction(const CContentPackageImportResult &Result, char *pError, int ErrorSize) const;
	bool LoadTransaction(char *pError, int ErrorSize);
	static void ImportThread(void *pUser);

  public:
	CServerModManager();
	~CServerModManager();

	bool Init(const char *pSaveRoot, const char *pWorkshopRoot, const char *pProtocol, char *pError, int ErrorSize);
	static bool ValidProfileName(const char *pName);
	const std::vector<CProfile> &Profiles() const { return m_Profiles; }
	const char *SelectedProfile() const { return m_SelectedProfile.c_str(); }
	const char *AppliedProfile() const { return m_AppliedProfile.c_str(); }
	const char *AppliedIds() const { return m_AppliedIds.c_str(); }
	const char *AppliedHash() const { return m_AppliedHash.c_str(); }
	bool PendingImportTransaction() const { return m_PendingImportTransaction; }

	bool CreateProfile(const char *pName, const char *pCopyFrom, char *pError, int ErrorSize);
	bool DeleteProfile(const char *pName, char *pError, int ErrorSize);
	bool SelectProfile(const char *pName, char *pError, int ErrorSize);
	bool Enable(const char *pPublishedFileID, const char *pProfile, char *pError, int ErrorSize);
	bool Disable(const char *pPublishedFileID, const char *pProfile, char *pError, int ErrorSize);
	bool Move(const char *pPublishedFileID, int Index, const char *pProfile, char *pError, int ErrorSize);
	bool ResolveSelected(char *pIds, int IdsSize, char *pHash, int HashSize, char *pError, int ErrorSize) const;
	void SetApplied(const char *pProfile, const char *pIds, const char *pHash);

	bool StartImport(const char *pArchiveName, bool Replace, char *pError, int ErrorSize);
	EImportState PollImport();
	bool HasLastImport() const { return m_HasLastImport; }
	bool LastImportSucceeded() const { return m_LastImportSucceeded; }
	const CContentPackageImportResult &LastImport() const { return m_LastImport; }
	const char *LastImportError() const { return m_LastImportError.c_str(); }
	bool FinalizeImport(char *pError, int ErrorSize);
	bool RollbackImport(char *pError, int ErrorSize);
	bool Remove(const char *pPublishedFileID, char *pError, int ErrorSize);
};

#endif
