#ifndef GAME_CLIENT_CLOUD_PROFILE_H
#define GAME_CLIENT_CLOUD_PROFILE_H

class CBinds;

enum ECloudProfileReadResult
{
	CLOUD_PROFILE_OK,
	CLOUD_PROFILE_CORRUPT,
	CLOUD_PROFILE_FUTURE_VERSION,
};

enum ECloudProfileSyncDecision
{
	CLOUD_SYNC_CURRENT,
	CLOUD_SYNC_UPLOAD_LOCAL,
	CLOUD_SYNC_APPLY_REMOTE,
	CLOUD_SYNC_CONFLICT,
};

inline ECloudProfileSyncDecision CloudProfileDecide(unsigned long long LocalHash, unsigned long long RemoteHash, unsigned long long SyncedHash, bool LocalIsDefault)
{
	if(LocalHash == RemoteHash)
		return CLOUD_SYNC_CURRENT;
	if(SyncedHash && LocalHash == SyncedHash)
		return CLOUD_SYNC_APPLY_REMOTE;
	if(SyncedHash && RemoteHash == SyncedHash)
		return CLOUD_SYNC_UPLOAD_LOCAL;
	if(!SyncedHash && LocalIsDefault)
		return CLOUD_SYNC_APPLY_REMOTE;
	return CLOUD_SYNC_CONFLICT;
}

struct CCloudProfileSummary
{
	int m_SchemaVersion;
	int m_Revision;
	long long m_ModifiedAt;
	unsigned long long m_ContentHash;
	int m_ResearchPoints;
	int m_HighestInvasion;
	int m_TutorialCompletedMask;
};

unsigned long long CloudProfileHash(const void *pData, int Size);
bool CloudProfileBuild(CBinds *pBinds, int Revision, long long ModifiedAt, char *pBuffer, int BufferSize, CCloudProfileSummary *pSummary);
ECloudProfileReadResult CloudProfileInspect(const char *pData, int DataSize, CCloudProfileSummary *pSummary);
ECloudProfileReadResult CloudProfileApply(const char *pData, int DataSize, CBinds *pBinds, CCloudProfileSummary *pSummary);

#endif
