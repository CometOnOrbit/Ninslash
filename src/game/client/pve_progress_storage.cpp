#include "pve_progress_storage.h"

#include <base/math.h>
#include <base/system.h>
#include <engine/external/json-parser/json.h>
#include <engine/storage.h>

namespace
{
const char *PROGRESS_FILENAME = "pve_progress.json";
const char *PROGRESS_TEMP_FILENAME = "pve_progress.json.tmp";
const char *PROGRESS_BACKUP_FILENAME = "pve_progress.json.bak";

bool IsHexMask(const char *pMask)
{
	if(str_length(pMask) != 32)
		return false;
	for(int i = 0; i < 32; i++)
	{
		const char c = pMask[i];
		if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
			return false;
	}
	return true;
}

bool ReadInt(const json_value &Object, const char *pName, int *pValue)
{
	const json_value &Value = Object[pName];
	if(Value.type != json_integer)
		return false;
	*pValue = (int)Value.u.integer;
	return true;
}

bool FileExists(IStorage *pStorage, const char *pFilename)
{
	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	io_close(File);
	return true;
}
}

CPveProgressData::CPveProgressData()
{
	m_SchemaVersion = CURRENT_SCHEMA_VERSION;
	m_ProgressVersion = 2;
	m_ResearchPoints = 0;
	str_copy(m_aResearchMask, "00000000000000000000000000000000", sizeof(m_aResearchMask));
	m_HighestInvasion = 0;
	m_PreferredCheckpoint = 1;
	m_DroneTutorialSeen = false;
}

void CPveProgressData::Sanitize()
{
	m_SchemaVersion = CURRENT_SCHEMA_VERSION;
	m_ProgressVersion = clamp(m_ProgressVersion, 0, 999);
	m_ResearchPoints = clamp(m_ResearchPoints, 0, 999);
	if(!IsHexMask(m_aResearchMask))
		str_copy(m_aResearchMask, "00000000000000000000000000000000", sizeof(m_aResearchMask));
	m_HighestInvasion = clamp(m_HighestInvasion, 0, 9999);
	const int MaxCheckpoint = m_HighestInvasion >= 10 ? (m_HighestInvasion / 10) * 10 + 1 : 1;
	m_PreferredCheckpoint = clamp(m_PreferredCheckpoint, 1, MaxCheckpoint);
}

const char *CPveProgressStorage::Filename()
{
	return PROGRESS_FILENAME;
}

EPveProgressLoadResult CPveProgressStorage::Load(IStorage *pStorage, CPveProgressData *pData, const char *pFilename)
{
	if(!pStorage || !pData)
		return PVE_PROGRESS_LOAD_CORRUPT;
	if(!pFilename)
		pFilename = PROGRESS_FILENAME;

	IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return PVE_PROGRESS_LOAD_MISSING;
	const long Length = io_length(File);
	if(Length <= 0 || Length > 64 * 1024)
	{
		io_close(File);
		return PVE_PROGRESS_LOAD_CORRUPT;
	}
	char *pBuffer = (char *)mem_alloc((unsigned)Length + 1, 1);
	const unsigned Read = io_read(File, pBuffer, (unsigned)Length);
	io_close(File);
	if(Read != (unsigned)Length)
	{
		mem_free(pBuffer);
		return PVE_PROGRESS_LOAD_CORRUPT;
	}
	pBuffer[Length] = 0;

	json_settings Settings;
	mem_zero(&Settings, sizeof(Settings));
	char aError[256];
	json_value *pJson = json_parse_ex(&Settings, pBuffer, (size_t)Length, aError);
	mem_free(pBuffer);
	if(!pJson || pJson->type != json_object)
	{
		if(pJson)
			json_value_free(pJson);
		return PVE_PROGRESS_LOAD_CORRUPT;
	}

	CPveProgressData Loaded;
	int TutorialSeen = 0;
	const json_value &Mask = (*pJson)["research_mask"];
	const bool Valid =
		ReadInt(*pJson, "schema_version", &Loaded.m_SchemaVersion) &&
		ReadInt(*pJson, "progress_version", &Loaded.m_ProgressVersion) &&
		ReadInt(*pJson, "research_points", &Loaded.m_ResearchPoints) &&
		Mask.type == json_string &&
		ReadInt(*pJson, "highest_invasion", &Loaded.m_HighestInvasion) &&
		ReadInt(*pJson, "preferred_checkpoint", &Loaded.m_PreferredCheckpoint) &&
		ReadInt(*pJson, "drone_tutorial_seen", &TutorialSeen);
	if(Valid)
		str_copy(Loaded.m_aResearchMask, (const char *)Mask, sizeof(Loaded.m_aResearchMask));
	json_value_free(pJson);

	if(!Valid || !IsHexMask(Loaded.m_aResearchMask) || (TutorialSeen != 0 && TutorialSeen != 1))
		return PVE_PROGRESS_LOAD_CORRUPT;
	if(Loaded.m_SchemaVersion > CPveProgressData::CURRENT_SCHEMA_VERSION)
		return PVE_PROGRESS_LOAD_FUTURE_VERSION;
	if(Loaded.m_SchemaVersion <= 0)
		return PVE_PROGRESS_LOAD_CORRUPT;
	Loaded.m_DroneTutorialSeen = TutorialSeen != 0;
	Loaded.Sanitize();
	*pData = Loaded;
	return PVE_PROGRESS_LOAD_OK;
}

bool CPveProgressStorage::Save(IStorage *pStorage, const CPveProgressData &Source)
{
	if(!pStorage)
		return false;
	CPveProgressData Data = Source;
	Data.Sanitize();

	char aJson[768];
	str_format(aJson, sizeof(aJson),
		"{\n"
		"  \"schema_version\": %d,\n"
		"  \"progress_version\": %d,\n"
		"  \"research_points\": %d,\n"
		"  \"research_mask\": \"%s\",\n"
		"  \"highest_invasion\": %d,\n"
		"  \"preferred_checkpoint\": %d,\n"
		"  \"drone_tutorial_seen\": %d\n"
		"}\n",
		Data.m_SchemaVersion, Data.m_ProgressVersion, Data.m_ResearchPoints, Data.m_aResearchMask,
		Data.m_HighestInvasion, Data.m_PreferredCheckpoint, Data.m_DroneTutorialSeen ? 1 : 0);

	pStorage->RemoveFile(PROGRESS_TEMP_FILENAME, IStorage::TYPE_SAVE);
	IOHANDLE File = pStorage->OpenFile(PROGRESS_TEMP_FILENAME, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	const unsigned Length = (unsigned)str_length(aJson);
	const bool Written = io_write(File, aJson, Length) == Length && io_flush(File) == 0;
	io_close(File);
	if(!Written)
	{
		pStorage->RemoveFile(PROGRESS_TEMP_FILENAME, IStorage::TYPE_SAVE);
		return false;
	}

	pStorage->RemoveFile(PROGRESS_BACKUP_FILENAME, IStorage::TYPE_SAVE);
	const bool HadCurrent = FileExists(pStorage, PROGRESS_FILENAME);
	if(HadCurrent && !pStorage->RenameFile(PROGRESS_FILENAME, PROGRESS_BACKUP_FILENAME, IStorage::TYPE_SAVE))
	{
		pStorage->RemoveFile(PROGRESS_TEMP_FILENAME, IStorage::TYPE_SAVE);
		return false;
	}
	if(!pStorage->RenameFile(PROGRESS_TEMP_FILENAME, PROGRESS_FILENAME, IStorage::TYPE_SAVE))
	{
		if(HadCurrent)
			pStorage->RenameFile(PROGRESS_BACKUP_FILENAME, PROGRESS_FILENAME, IStorage::TYPE_SAVE);
		pStorage->RemoveFile(PROGRESS_TEMP_FILENAME, IStorage::TYPE_SAVE);
		return false;
	}
	return true;
}
