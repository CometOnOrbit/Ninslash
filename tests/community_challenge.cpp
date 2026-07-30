#include <engine/shared/community_challenge.h>
#include <base/system.h>
#include <assert.h>
#include <string.h>

int main()
{
	CContentManifest Manifest;
	mem_zero(&Manifest, sizeof(Manifest));
	Manifest.m_ContentType = CONTENT_TYPE_CHALLENGE;
	str_copy(Manifest.m_aPublishedFileID, "42", sizeof(Manifest.m_aPublishedFileID));
	str_copy(Manifest.m_aContentHash,
			 "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
			 sizeof(Manifest.m_aContentHash));
	const char *pJson = "{\"revision\":2,\"fixed_seed\":123,\"metric\":\"clear_time_ms\",\"rules_locked\":true,\"rules_"
						"hash\":\"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\"}";
	CCommunityChallengeDescriptor Run, Current;
	char aError[128], aName[128];
	assert(CommunityChallengeParse(pJson, strlen(pJson), Manifest, &Run, aError, sizeof(aError)));
	Current = Run;
	assert(CommunityChallengeStillEligible(Run, Current));
	Current.m_FixedSeed++;
	assert(!CommunityChallengeStillEligible(Run, Current));
	assert(CommunityChallengeLeaderboardName(Run, aName, sizeof(aName)));
	assert(strcmp(aName, "community_challenge_42_2_clear_time_ms") == 0);
	return 0;
}
