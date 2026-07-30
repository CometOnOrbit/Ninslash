#ifndef GAME_CLIENT_PVE_PROGRESS_STORAGE_H
#define GAME_CLIENT_PVE_PROGRESS_STORAGE_H

class IStorage;

struct CPveProgressData
{
	enum
	{
		CURRENT_SCHEMA_VERSION = 1,
	};

	int m_SchemaVersion;
	int m_ProgressVersion;
	int m_ResearchPoints;
	char m_aResearchMask[33];
	int m_HighestInvasion;
	int m_PreferredCheckpoint;
	bool m_DroneTutorialSeen;

	CPveProgressData();
	void Sanitize();
};

enum EPveProgressLoadResult
{
	PVE_PROGRESS_LOAD_OK = 0,
	PVE_PROGRESS_LOAD_MISSING,
	PVE_PROGRESS_LOAD_CORRUPT,
	PVE_PROGRESS_LOAD_FUTURE_VERSION,
};

class CPveProgressStorage
{
  public:
	static const char *Filename();
	static EPveProgressLoadResult Load(IStorage *pStorage, CPveProgressData *pData, const char *pFilename = 0);
	static bool Save(IStorage *pStorage, const CPveProgressData &Data);
};

#endif
