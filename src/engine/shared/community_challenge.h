#ifndef ENGINE_SHARED_COMMUNITY_CHALLENGE_H
#define ENGINE_SHARED_COMMUNITY_CHALLENGE_H

#include "content_manifest.h"

enum ECommunityChallengeMetric
{
	COMMUNITY_CHALLENGE_CLEAR_TIME_MS,
	COMMUNITY_CHALLENGE_HIGHEST_FLOOR
};

struct CCommunityChallengeDescriptor
{
	unsigned long long m_PublishedFileID;
	int m_Revision;
	int m_Metric;
	unsigned long long m_FixedSeed;
	char m_aRulesHash[65];
	char m_aContentHash[65];
};

bool CommunityChallengeParse(const char *pJson,
							 int JsonLength,
							 const CContentManifest &Manifest,
							 CCommunityChallengeDescriptor *pDescriptor,
							 char *pError,
							 int ErrorSize);
bool CommunityChallengeStillEligible(const CCommunityChallengeDescriptor &Run,
									 const CCommunityChallengeDescriptor &Current);
bool CommunityChallengeLeaderboardName(const CCommunityChallengeDescriptor &Descriptor, char *pBuffer, int BufferSize);

#endif
