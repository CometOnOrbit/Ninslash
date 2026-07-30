#include "server_mod_manager.h"

#include <base/system.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/content_collection.h>
#include <engine/shared/content_package_index.h>
#include <game/weapon_packages.h>

#include <algorithm>
#include <memory>

namespace
{
bool Fail(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText, ErrorSize);
	return false;
}

bool IsString(const json_value &Value)
{
	return Value.type == json_string && Value.u.string.ptr && str_utf8_check(Value.u.string.ptr);
}

bool IsPublishedId(const char *pId)
{
	if(!pId || !pId[0] || str_length(pId) >= 32)
		return false;
	for(const char *p = pId; *p; ++p)
		if(*p < '0' || *p > '9')
			return false;
	return true;
}

bool IsHash(const char *pHash)
{
	if(!pHash || str_length(pHash) != 64)
		return false;
	for(const char *p = pHash; *p; ++p)
		if(!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
			return false;
	return true;
}

bool ReadFile(const char *pPath, std::string &Data)
{
	IOHANDLE File = io_open(pPath, IOFLAG_READ);
	if(!File)
		return false;
	const long Size = io_length(File);
	if(Size < 0 || Size > 1024 * 1024)
	{
		io_close(File);
		return false;
	}
	Data.resize((size_t)Size);
	const bool Ok = Size == 0 || io_read(File, &Data[0], (unsigned)Size) == (unsigned)Size;
	io_close(File);
	return Ok;
}

bool AtomicWrite(const char *pPath, const std::string &Data, char *pError, int ErrorSize)
{
	char aTemporary[1200];
	str_format(aTemporary, sizeof(aTemporary), "%s.tmp", pPath);
	IOHANDLE File = io_open(aTemporary, IOFLAG_WRITE);
	if(!File)
		return Fail(pError, ErrorSize, "unable to open temporary state file");
	const bool Written = io_write(File, Data.data(), (unsigned)Data.size()) == Data.size() && io_sync(File) == 0;
	io_close(File);
	if(!Written)
	{
		fs_remove(aTemporary);
		return Fail(pError, ErrorSize, "unable to synchronize state file");
	}
	if(fs_rename(aTemporary, pPath) != 0)
	{
		// Windows cannot replace an existing file with rename. Preserve the old
		// file until the fully written temporary file is ready.
		char aOld[1200];
		str_format(aOld, sizeof(aOld), "%s.old", pPath);
		fs_remove(aOld);
		if(fs_rename(pPath, aOld) != 0 || fs_rename(aTemporary, pPath) != 0)
		{
			fs_rename(aOld, pPath);
			fs_remove(aTemporary);
			return Fail(pError, ErrorSize, "unable to atomically replace state file");
		}
		fs_remove(aOld);
	}
	return true;
}

bool RemoveTree(const char *pRoot);
struct CRemoveState
{
	const char *m_pRoot;
	bool m_Ok;
};
int RemoveEntry(const char *pName, int IsDir, int, void *pUser)
{
	CRemoveState *pState = static_cast<CRemoveState *>(pUser);
	if(str_comp(pName, ".") == 0 || str_comp(pName, "..") == 0)
		return 0;
	char aPath[1400];
	str_format(aPath, sizeof(aPath), "%s/%s", pState->m_pRoot, pName);
	if(IsDir && !fs_is_symlink(aPath))
		pState->m_Ok = RemoveTree(aPath) && pState->m_Ok;
	else
		pState->m_Ok = fs_remove(aPath) == 0 && pState->m_Ok;
	return 0;
}
bool RemoveTree(const char *pRoot)
{
	if(!fs_is_dir(pRoot))
		return fs_is_symlink(pRoot) ? fs_remove(pRoot) == 0 : true;
	CRemoveState State = {pRoot, true};
	fs_listdir(pRoot, RemoveEntry, 0, &State);
	return State.m_Ok && fs_removedir(pRoot) == 0;
}
} // namespace

struct CServerModManager::CImportTask
{
	volatile int m_Done;
	bool m_Ok;
	bool m_Replace;
	char m_aArchive[1200];
	char m_aWorkshop[1200];
	char m_aProtocol[64];
	char m_aError[256];
	CContentPackageImportResult m_Result;
};

CServerModManager::CServerModManager()
	: m_pImport(0), m_pImportThread(0), m_LastImportSucceeded(false), m_HasLastImport(false),
	  m_PendingImportTransaction(false)
{
	mem_zero(&m_LastImport, sizeof(m_LastImport));
}
CServerModManager::~CServerModManager()
{
	if(m_pImportThread)
		thread_wait(m_pImportThread);
	delete m_pImport;
}

bool CServerModManager::ValidProfileName(const char *pName)
{
	const int Length = pName ? str_length(pName) : 0;
	if(Length < 1 || Length > 32)
		return false;
	for(int i = 0; i < Length; ++i)
		if(!((pName[i] >= 'a' && pName[i] <= 'z') || (pName[i] >= '0' && pName[i] <= '9') || pName[i] == '_' ||
			 pName[i] == '-'))
			return false;
	return true;
}

CServerModManager::CProfile *CServerModManager::FindProfile(const char *pName)
{
	for(CProfile &Profile : m_Profiles)
		if(Profile.m_Name == pName)
			return &Profile;
	return 0;
}
const CServerModManager::CProfile *CServerModManager::FindProfile(const char *pName) const
{
	for(const CProfile &Profile : m_Profiles)
		if(Profile.m_Name == pName)
			return &Profile;
	return 0;
}

bool CServerModManager::Save(char *pError, int ErrorSize) const
{
	std::string Json =
		"{\n  \"schema_version\": 1,\n  \"selected_profile\": \"" + m_SelectedProfile + "\",\n  \"profiles\": [\n";
	for(size_t i = 0; i < m_Profiles.size(); ++i)
	{
		Json += "    {\"name\": \"" + m_Profiles[i].m_Name + "\", \"root_ids\": [";
		for(size_t j = 0; j < m_Profiles[i].m_RootIds.size(); ++j)
		{
			if(j)
				Json += ", ";
			Json += "\"" + m_Profiles[i].m_RootIds[j] + "\"";
		}
		Json += "]}";
		if(i + 1 != m_Profiles.size())
			Json += ",";
		Json += "\n";
	}
	Json += "  ]\n}\n";
	char aPath[1200];
	str_format(aPath, sizeof(aPath), "%s/server_mods.json", m_SaveRoot.c_str());
	return AtomicWrite(aPath, Json, pError, ErrorSize);
}

bool CServerModManager::Init(
	const char *pSaveRoot, const char *pWorkshopRoot, const char *pProtocol, char *pError, int ErrorSize)
{
	m_SaveRoot = pSaveRoot ? pSaveRoot : "";
	m_WorkshopRoot = pWorkshopRoot ? pWorkshopRoot : "";
	m_Protocol = pProtocol ? pProtocol : "";
	char aPath[1200];
	str_format(aPath, sizeof(aPath), "%s/server_mods.json", m_SaveRoot.c_str());
	std::string Data;
	if(!ReadFile(aPath, Data))
	{
		m_Profiles.clear();
		CProfile Default;
		Default.m_Name = "default";
		m_Profiles.push_back(Default);
		m_SelectedProfile = "default";
		if(!Save(pError, ErrorSize))
			return false;
	}
	else
	{
		json_settings Settings;
		mem_zero(&Settings, sizeof(Settings));
		char aJsonError[128];
		json_value *pRoot = json_parse_ex(&Settings, Data.data(), Data.size(), aJsonError);
		if(!pRoot || pRoot->type != json_object)
		{
			if(pRoot)
				json_value_free(pRoot);
			return Fail(pError, ErrorSize, "server_mods.json is not valid JSON object");
		}
		const json_value &Schema = (*pRoot)["schema_version"], &Selected = (*pRoot)["selected_profile"],
						 &Profiles = (*pRoot)["profiles"];
		if(Schema.type != json_integer || Schema.u.integer != 1 || !IsString(Selected) || Profiles.type != json_array ||
		   Profiles.u.array.length < 1 || Profiles.u.array.length > 32)
		{
			json_value_free(pRoot);
			return Fail(pError, ErrorSize, "server_mods.json has an unsupported schema or invalid fields");
		}
		m_Profiles.clear();
		m_SelectedProfile = (const char *)Selected;
		for(unsigned i = 0; i < Profiles.u.array.length; ++i)
		{
			const json_value &Object = Profiles[i], &Name = Object["name"], &Ids = Object["root_ids"];
			if(Object.type != json_object || !IsString(Name) || !ValidProfileName((const char *)Name) ||
			   Ids.type != json_array || Ids.u.array.length > 64 || FindProfile((const char *)Name))
			{
				json_value_free(pRoot);
				return Fail(pError, ErrorSize, "server_mods.json contains an invalid or duplicate profile");
			}
			CProfile Profile;
			Profile.m_Name = (const char *)Name;
			for(unsigned j = 0; j < Ids.u.array.length; ++j)
			{
				if(!IsString(Ids[j]) || !IsPublishedId((const char *)Ids[j]) ||
				   std::find(Profile.m_RootIds.begin(), Profile.m_RootIds.end(), (const char *)Ids[j]) !=
					   Profile.m_RootIds.end())
				{
					json_value_free(pRoot);
					return Fail(pError, ErrorSize, "server_mods.json contains an invalid or duplicate root ID");
				}
				Profile.m_RootIds.push_back((const char *)Ids[j]);
			}
			m_Profiles.push_back(Profile);
		}
		json_value_free(pRoot);
		if(!ValidProfileName(m_SelectedProfile.c_str()) || !FindProfile(m_SelectedProfile.c_str()))
			return Fail(pError, ErrorSize, "server_mods.json selects a missing profile");
	}
	return LoadTransaction(pError, ErrorSize);
}

bool CServerModManager::CreateProfile(const char *pName, const char *pCopyFrom, char *pError, int ErrorSize)
{
	if(!ValidProfileName(pName) || FindProfile(pName) || m_Profiles.size() >= 32)
		return Fail(pError, ErrorSize, "invalid, duplicate, or excessive profile name");
	CProfile Profile;
	Profile.m_Name = pName;
	if(pCopyFrom && pCopyFrom[0])
	{
		const CProfile *pSource = FindProfile(pCopyFrom);
		if(!pSource)
			return Fail(pError, ErrorSize, "copy source profile does not exist");
		Profile.m_RootIds = pSource->m_RootIds;
	}
	m_Profiles.push_back(Profile);
	if(!Save(pError, ErrorSize))
	{
		m_Profiles.pop_back();
		return false;
	}
	return true;
}
bool CServerModManager::DeleteProfile(const char *pName, char *pError, int ErrorSize)
{
	if(!pName || m_SelectedProfile == pName || (!m_AppliedProfile.empty() && m_AppliedProfile == pName))
		return Fail(pError, ErrorSize, "cannot delete the selected or applied profile");
	for(size_t i = 0; i < m_Profiles.size(); ++i)
	{
		if(m_Profiles[i].m_Name != pName)
			continue;
		CProfile Old = m_Profiles[i];
		m_Profiles.erase(m_Profiles.begin() + i);
		if(!Save(pError, ErrorSize))
		{
			m_Profiles.insert(m_Profiles.begin() + i, Old);
			return false;
		}
		return true;
	}
	return Fail(pError, ErrorSize, "profile does not exist");
}
bool CServerModManager::SelectProfile(const char *pName, char *pError, int ErrorSize)
{
	if(!FindProfile(pName))
		return Fail(pError, ErrorSize, "profile does not exist");
	std::string Old = m_SelectedProfile;
	m_SelectedProfile = pName;
	if(!Save(pError, ErrorSize))
	{
		m_SelectedProfile = Old;
		return false;
	}
	return true;
}

bool CServerModManager::Enable(const char *pId, const char *pProfile, char *pError, int ErrorSize)
{
	if(!IsPublishedId(pId))
		return Fail(pError, ErrorSize, "invalid PublishedFileID");
	CContentPackageIndex Index;
	if(!Index.Scan(m_WorkshopRoot.c_str(), m_Protocol.c_str(), pError, ErrorSize) || !Index.Find(pId))
		return Fail(pError, ErrorSize, "Mod is not installed or valid");
	CProfile *pTarget = FindProfile(pProfile && pProfile[0] ? pProfile : m_SelectedProfile.c_str());
	if(!pTarget)
		return Fail(pError, ErrorSize, "profile does not exist");
	if(std::find(pTarget->m_RootIds.begin(), pTarget->m_RootIds.end(), pId) != pTarget->m_RootIds.end())
		return true;
	pTarget->m_RootIds.push_back(pId);
	if(!Save(pError, ErrorSize))
	{
		pTarget->m_RootIds.pop_back();
		return false;
	}
	return true;
}
bool CServerModManager::Disable(const char *pId, const char *pProfile, char *pError, int ErrorSize)
{
	CProfile *pTarget = FindProfile(pProfile && pProfile[0] ? pProfile : m_SelectedProfile.c_str());
	if(!pTarget)
		return Fail(pError, ErrorSize, "profile does not exist");
	auto It = std::find(pTarget->m_RootIds.begin(), pTarget->m_RootIds.end(), pId ? pId : "");
	if(It == pTarget->m_RootIds.end())
		return Fail(pError, ErrorSize, "Mod is not enabled in profile");
	const size_t Pos = It - pTarget->m_RootIds.begin();
	std::string Old = *It;
	pTarget->m_RootIds.erase(It);
	if(!Save(pError, ErrorSize))
	{
		pTarget->m_RootIds.insert(pTarget->m_RootIds.begin() + Pos, Old);
		return false;
	}
	return true;
}
bool CServerModManager::Move(const char *pId, int Index, const char *pProfile, char *pError, int ErrorSize)
{
	CProfile *pTarget = FindProfile(pProfile && pProfile[0] ? pProfile : m_SelectedProfile.c_str());
	if(!pTarget)
		return Fail(pError, ErrorSize, "profile does not exist");
	auto It = std::find(pTarget->m_RootIds.begin(), pTarget->m_RootIds.end(), pId ? pId : "");
	if(It == pTarget->m_RootIds.end() || Index < 0 || Index >= (int)pTarget->m_RootIds.size())
		return Fail(pError, ErrorSize, "invalid Mod or target index");
	const int OldIndex = (int)(It - pTarget->m_RootIds.begin());
	std::string Id = *It;
	pTarget->m_RootIds.erase(It);
	pTarget->m_RootIds.insert(pTarget->m_RootIds.begin() + Index, Id);
	if(!Save(pError, ErrorSize))
	{
		pTarget->m_RootIds.erase(pTarget->m_RootIds.begin() + Index);
		pTarget->m_RootIds.insert(pTarget->m_RootIds.begin() + OldIndex, Id);
		return false;
	}
	return true;
}

bool CServerModManager::ResolveSelected(
	char *pIds, int IdsSize, char *pHash, int HashSize, char *pError, int ErrorSize) const
{
	const CProfile *pProfile = FindProfile(m_SelectedProfile.c_str());
	if(!pProfile)
		return Fail(pError, ErrorSize, "selected profile does not exist");
	std::string Ids;
	for(size_t i = 0; i < pProfile->m_RootIds.size(); ++i)
	{
		if(i)
			Ids += ',';
		Ids += pProfile->m_RootIds[i];
	}
	if((int)Ids.size() >= IdsSize)
		return Fail(pError, ErrorSize, "selected profile ID list is too long");
	str_copy(pIds, Ids.c_str(), IdsSize);
	return WeaponPackagesResolveCollectionHash(
		m_WorkshopRoot.c_str(), pIds, m_Protocol.c_str(), pHash, HashSize, pError, ErrorSize);
}
void CServerModManager::SetApplied(const char *pProfile, const char *pIds, const char *pHash)
{
	m_AppliedProfile = pProfile ? pProfile : "";
	m_AppliedIds = pIds ? pIds : "";
	m_AppliedHash = pHash ? pHash : "";
}

void CServerModManager::ImportThread(void *pUser)
{
	CImportTask *pTask = static_cast<CImportTask *>(pUser);
	pTask->m_Ok = ContentPackageImportZip(pTask->m_aArchive,
										  pTask->m_aWorkshop,
										  pTask->m_aProtocol,
										  pTask->m_Replace,
										  &pTask->m_Result,
										  pTask->m_aError,
										  sizeof(pTask->m_aError));
	pTask->m_Done = 1;
}
bool CServerModManager::StartImport(const char *pArchiveName, bool Replace, char *pError, int ErrorSize)
{
	if(m_pImport)
		return Fail(pError, ErrorSize, "another Mod import is already running or awaiting status collection");
	if(m_PendingImportTransaction)
		return Fail(pError, ErrorSize, "an imported Mod update is pending mod_apply or rollback");
	if(!pArchiveName || !pArchiveName[0] || str_length(pArchiveName) > 255 ||
	   str_comp_nocase(pArchiveName + std::max(0, str_length(pArchiveName) - 4), ".zip") != 0 ||
	   strchr(pArchiveName, '/') || strchr(pArchiveName, '\\') || str_comp(pArchiveName, ".") == 0 ||
	   str_comp(pArchiveName, "..") == 0)
		return Fail(pError, ErrorSize, "archive must be a plain .zip filename from mod_inbox");
	char aInbox[1200];
	str_format(aInbox, sizeof(aInbox), "%s/mod_inbox", m_SaveRoot.c_str());
	fs_makedir(aInbox);
	char aArchive[1500];
	str_format(aArchive, sizeof(aArchive), "%s/%s", aInbox, pArchiveName);
	if(!fs_is_file(aArchive))
		return Fail(pError, ErrorSize, "archive must be a regular file and not a symbolic link");
	IOHANDLE Check = io_open(aArchive, IOFLAG_READ);
	if(!Check)
		return Fail(pError, ErrorSize, "archive does not exist in mod_inbox");
	io_close(Check);
	m_pImport = new CImportTask;
	mem_zero(m_pImport, sizeof(*m_pImport));
	m_pImport->m_Replace = Replace;
	str_copy(m_pImport->m_aArchive, aArchive, sizeof(m_pImport->m_aArchive));
	str_copy(m_pImport->m_aWorkshop, m_WorkshopRoot.c_str(), sizeof(m_pImport->m_aWorkshop));
	str_copy(m_pImport->m_aProtocol, m_Protocol.c_str(), sizeof(m_pImport->m_aProtocol));
	m_pImportThread = thread_init(ImportThread, m_pImport);
	if(!m_pImportThread)
	{
		delete m_pImport;
		m_pImport = 0;
		return Fail(pError, ErrorSize, "unable to start Mod import worker");
	}
	return true;
}
CServerModManager::EImportState CServerModManager::PollImport()
{
	if(!m_pImport)
		return m_HasLastImport ? IMPORT_FINISHED : IMPORT_IDLE;
	if(!m_pImport->m_Done)
		return IMPORT_RUNNING;
	thread_wait(m_pImportThread);
	m_pImportThread = 0;
	m_LastImport = m_pImport->m_Result;
	m_LastImportSucceeded = m_pImport->m_Ok;
	m_LastImportError = m_pImport->m_aError;
	m_HasLastImport = true;
	if(m_LastImportSucceeded && m_LastImport.m_Status == CONTENT_IMPORT_INSTALLED)
	{
		bool Active = false;
		std::unique_ptr<CContentPackageIndex> pIndex(new CContentPackageIndex);
		char aError[256];
		std::unique_ptr<CContentCollection> pCollection(new CContentCollection);
		if(pIndex->Scan(m_WorkshopRoot.c_str(), m_Protocol.c_str(), aError, sizeof(aError)))
		{
			const CProfile *pApplied = FindProfile(m_AppliedProfile.c_str());
			if(pApplied)
				for(const std::string &Root : pApplied->m_RootIds)
					pIndex->AddRecursive(pCollection.get(), Root.c_str(), CONTENT_TYPE_MOD, aError, sizeof(aError));
			Active = pCollection->FindIndex(m_LastImport.m_aPublishedFileID) >= 0;
		}
		if(m_LastImport.m_Replaced && Active)
		{
			if(SaveTransaction(m_LastImport, aError, sizeof(aError)))
				m_PendingImportTransaction = true;
			else
			{
				m_LastImportSucceeded = false;
				m_LastImportError = aError;
				ContentPackageRollbackImport(&m_LastImport, aError, sizeof(aError));
			}
		}
		else if(!ContentPackageFinalizeImport(&m_LastImport, aError, sizeof(aError)))
		{
			m_LastImportSucceeded = false;
			m_LastImportError = aError;
		}
	}
	delete m_pImport;
	m_pImport = 0;
	return IMPORT_FINISHED;
}

bool CServerModManager::SaveTransaction(const CContentPackageImportResult &R, char *pError, int ErrorSize) const
{
	std::string Json = "{\"schema_version\":1,\"published_file_id\":\"" + std::string(R.m_aPublishedFileID) +
					   "\",\"target\":\"" + R.m_aTargetRoot + "\",\"backup\":\"" + R.m_aBackupRoot +
					   "\",\"old_hash\":\"" + R.m_aPreviousHash + "\",\"new_hash\":\"" + R.m_aContentHash + "\"}\n";
	char aPath[1200];
	str_format(aPath, sizeof(aPath), "%s/server_mod_import.json", m_SaveRoot.c_str());
	return AtomicWrite(aPath, Json, pError, ErrorSize);
}
bool CServerModManager::LoadTransaction(char *pError, int ErrorSize)
{
	char aPath[1200];
	str_format(aPath, sizeof(aPath), "%s/server_mod_import.json", m_SaveRoot.c_str());
	std::string Data;
	if(!ReadFile(aPath, Data))
		return true;
	json_value *pRoot = json_parse(Data.data(), Data.size());
	if(!pRoot || pRoot->type != json_object)
	{
		if(pRoot)
			json_value_free(pRoot);
		return Fail(pError, ErrorSize, "server Mod transaction log is corrupt");
	}
	const json_value &Id = (*pRoot)["published_file_id"], &Target = (*pRoot)["target"], &Backup = (*pRoot)["backup"],
					 &OldHash = (*pRoot)["old_hash"], &NewHash = (*pRoot)["new_hash"];
	char aExpectedBackup[1400], aRootPrefix[1300];
	str_format(aExpectedBackup,
			   sizeof(aExpectedBackup),
			   "%s/.import-%s.backup",
			   m_WorkshopRoot.c_str(),
			   IsString(Id) ? (const char *)Id : "");
	str_format(aRootPrefix, sizeof(aRootPrefix), "%s/", m_WorkshopRoot.c_str());
	const char *pTarget = IsString(Target) ? (const char *)Target : "";
	const char *pRelativeTarget = pTarget + std::min(str_length(pTarget), str_length(aRootPrefix));
	if(!IsString(Id) || !IsPublishedId((const char *)Id) || !IsString(Target) ||
	   str_comp_num(pTarget, aRootPrefix, str_length(aRootPrefix)) != 0 || !pRelativeTarget[0] ||
	   strchr(pRelativeTarget, '/') || strchr(pRelativeTarget, '\\') || !IsString(Backup) ||
	   str_comp((const char *)Backup, aExpectedBackup) != 0 || !IsString(OldHash) || !IsHash((const char *)OldHash) ||
	   !IsString(NewHash) || !IsHash((const char *)NewHash))
	{
		json_value_free(pRoot);
		return Fail(pError, ErrorSize, "server Mod transaction log has invalid fields");
	}
	mem_zero(&m_LastImport, sizeof(m_LastImport));
	m_LastImport.m_Status = CONTENT_IMPORT_INSTALLED;
	m_LastImport.m_Replaced = true;
	str_copy(m_LastImport.m_aPublishedFileID, (const char *)Id, sizeof(m_LastImport.m_aPublishedFileID));
	str_copy(m_LastImport.m_aTargetRoot, (const char *)Target, sizeof(m_LastImport.m_aTargetRoot));
	str_copy(m_LastImport.m_aBackupRoot, (const char *)Backup, sizeof(m_LastImport.m_aBackupRoot));
	str_copy(m_LastImport.m_aPreviousHash, (const char *)OldHash, sizeof(m_LastImport.m_aPreviousHash));
	str_copy(m_LastImport.m_aContentHash, (const char *)NewHash, sizeof(m_LastImport.m_aContentHash));
	json_value_free(pRoot);
	m_PendingImportTransaction = true;
	return true;
}
bool CServerModManager::FinalizeImport(char *pError, int ErrorSize)
{
	if(!m_PendingImportTransaction)
		return true;
	if(!ContentPackageFinalizeImport(&m_LastImport, pError, ErrorSize))
		return false;
	char aPath[1200];
	str_format(aPath, sizeof(aPath), "%s/server_mod_import.json", m_SaveRoot.c_str());
	if(fs_remove(aPath) != 0)
		return Fail(pError, ErrorSize, "unable to remove Mod transaction log");
	m_PendingImportTransaction = false;
	return true;
}
bool CServerModManager::RollbackImport(char *pError, int ErrorSize)
{
	if(!m_PendingImportTransaction)
		return true;
	if(!ContentPackageRollbackImport(&m_LastImport, pError, ErrorSize))
		return false;
	char aPath[1200];
	str_format(aPath, sizeof(aPath), "%s/server_mod_import.json", m_SaveRoot.c_str());
	fs_remove(aPath);
	m_PendingImportTransaction = false;
	return true;
}

bool CServerModManager::Remove(const char *pId, char *pError, int ErrorSize)
{
	if(!IsPublishedId(pId))
		return Fail(pError, ErrorSize, "invalid PublishedFileID");
	for(const CProfile &Profile : m_Profiles)
		if(std::find(Profile.m_RootIds.begin(), Profile.m_RootIds.end(), pId) != Profile.m_RootIds.end())
			return Fail(pError, ErrorSize, "Mod is referenced by a profile");
	CContentPackageIndex Index;
	if(!Index.Scan(m_WorkshopRoot.c_str(), m_Protocol.c_str(), pError, ErrorSize))
		return false;
	if(!m_AppliedIds.empty())
	{
		CContentCollection Applied;
		const CProfile *pProfile = FindProfile(m_AppliedProfile.c_str());
		if(pProfile)
			for(const std::string &Root : pProfile->m_RootIds)
				Index.AddRecursive(&Applied, Root.c_str(), CONTENT_TYPE_MOD, pError, ErrorSize);
		if(Applied.FindIndex(pId) >= 0)
			return Fail(pError, ErrorSize, "Mod is referenced by the current runtime dependency graph");
	}
	for(int i = 0; i < Index.Count(); ++i)
	{
		const CContentPackageIndex::CEntry *pEntry = Index.Get(i);
		if(!pEntry || !pEntry->m_Valid)
			continue;
		for(int d = 0; d < pEntry->m_Manifest.m_DependencyCount; ++d)
			if(str_comp(pEntry->m_Manifest.m_aDependencies[d].m_aPublishedFileID, pId) == 0)
				return Fail(pError, ErrorSize, "Mod is referenced by an installed dependency graph");
	}
	const CContentPackageIndex::CEntry *pEntry = Index.Find(pId);
	if(!pEntry)
		return Fail(pError, ErrorSize, "Mod is not installed or valid");
	if(!RemoveTree(pEntry->m_aRoot))
		return Fail(pError, ErrorSize, "unable to remove installed Mod");
	return true;
}
