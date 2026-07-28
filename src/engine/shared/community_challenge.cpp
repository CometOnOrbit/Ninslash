#include "community_challenge.h"

#include <base/system.h>
#include <engine/external/json-parser/json.h>
#include <stdio.h>

namespace
{
bool Fail(char *pError, int ErrorSize, const char *pText) { if(pError && ErrorSize > 0) str_copy(pError, pText, ErrorSize); return false; }
bool Hash(const char *pHash) { if(!pHash || str_length(pHash) != 64) return false; for(int i = 0; pHash[i]; i++) if(!((pHash[i] >= '0' && pHash[i] <= '9') || (pHash[i] >= 'a' && pHash[i] <= 'f') || (pHash[i] >= 'A' && pHash[i] <= 'F'))) return false; return true; }
}

bool CommunityChallengeParse(const char *pJson, int JsonLength, const CContentManifest &Manifest, CCommunityChallengeDescriptor *pDescriptor, char *pError, int ErrorSize)
{
	if(!pJson || JsonLength <= 0 || JsonLength > 64 * 1024 || !pDescriptor || Manifest.m_ContentType != CONTENT_TYPE_CHALLENGE) return Fail(pError, ErrorSize, "invalid challenge definition");
	json_settings Settings; mem_zero(&Settings, sizeof(Settings)); char aJsonError[128]; json_value *pRoot = json_parse_ex(&Settings, pJson, JsonLength, aJsonError);
	if(!pRoot || pRoot->type != json_object) { if(pRoot) json_value_free(pRoot); return Fail(pError, ErrorSize, "challenge definition is not an object"); }
	const json_value &Revision = (*pRoot)["revision"], &Seed = (*pRoot)["fixed_seed"], &Metric = (*pRoot)["metric"], &RulesHash = (*pRoot)["rules_hash"], &RulesLocked = (*pRoot)["rules_locked"];
	const char *pMetric = Metric.type == json_string ? (const char *)Metric : "";
	const int MetricValue = str_comp(pMetric, "clear_time_ms") == 0 ? COMMUNITY_CHALLENGE_CLEAR_TIME_MS : str_comp(pMetric, "highest_floor") == 0 ? COMMUNITY_CHALLENGE_HIGHEST_FLOOR : -1;
	const bool Valid = Revision.type == json_integer && Revision.u.integer > 0 && Seed.type == json_integer && Seed.u.integer >= 0 && MetricValue >= 0 && RulesHash.type == json_string && Hash((const char *)RulesHash) && RulesLocked.type == json_boolean && RulesLocked.u.boolean;
	if(!Valid) { json_value_free(pRoot); return Fail(pError, ErrorSize, "challenge must lock rules, fixed seed, revision, rules hash, and supported metric"); }
	mem_zero(pDescriptor, sizeof(*pDescriptor));
	sscanf(Manifest.m_aPublishedFileID, "%llu", &pDescriptor->m_PublishedFileID);
	pDescriptor->m_Revision = (int)Revision.u.integer; pDescriptor->m_Metric = MetricValue; pDescriptor->m_FixedSeed = (unsigned long long)Seed.u.integer;
	str_copy(pDescriptor->m_aRulesHash, (const char *)RulesHash, sizeof(pDescriptor->m_aRulesHash)); str_copy(pDescriptor->m_aContentHash, Manifest.m_aContentHash, sizeof(pDescriptor->m_aContentHash));
	json_value_free(pRoot); return true;
}

bool CommunityChallengeStillEligible(const CCommunityChallengeDescriptor &Run, const CCommunityChallengeDescriptor &Current)
{
	return Run.m_PublishedFileID == Current.m_PublishedFileID && Run.m_Revision == Current.m_Revision && Run.m_Metric == Current.m_Metric && Run.m_FixedSeed == Current.m_FixedSeed && str_comp_nocase(Run.m_aRulesHash, Current.m_aRulesHash) == 0 && str_comp_nocase(Run.m_aContentHash, Current.m_aContentHash) == 0;
}

bool CommunityChallengeLeaderboardName(const CCommunityChallengeDescriptor &Descriptor, char *pBuffer, int BufferSize)
{
	if(!pBuffer || BufferSize <= 0 || !Descriptor.m_PublishedFileID || Descriptor.m_Revision <= 0 || Descriptor.m_Metric < 0 || Descriptor.m_Metric > 1) return false;
	str_format(pBuffer, BufferSize, "community_challenge_%llu_%d_%s", Descriptor.m_PublishedFileID, Descriptor.m_Revision, Descriptor.m_Metric == COMMUNITY_CHALLENGE_CLEAR_TIME_MS ? "clear_time_ms" : "highest_floor"); return true;
}
