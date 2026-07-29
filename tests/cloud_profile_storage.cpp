#include <game/client/cloud_profile.h>
#include <game/client/components/binds.h>
#include <engine/shared/config.h>

#include <assert.h>

const char *CBinds::Get(int KeyID) { (void)KeyID; return ""; }
void CBinds::Bind(int KeyID, const char *pStr) { (void)KeyID; (void)pStr; }
void CBinds::UnbindAll() {}

int main()
{
	mem_zero(&g_Config, sizeof(g_Config));
	g_Config.m_ClPveResearchPoints = 17;
	g_Config.m_ClPveHighestInvasion = 42;
	g_Config.m_ClTutorialCompletedMask = 7;
	str_copy(g_Config.m_PlayerName, "CloudTest", sizeof(g_Config.m_PlayerName));
	str_copy(g_Config.m_ClPveResearchMask, "0000000000000000000000000000000f", sizeof(g_Config.m_ClPveResearchMask));

	char aProfile[64 * 1024];
	CCloudProfileSummary Built;
	assert(CloudProfileBuild(0, 3, 123456, aProfile, sizeof(aProfile), &Built));
	assert(Built.m_Revision == 3);
	assert(Built.m_ResearchPoints == 17);
	assert(Built.m_HighestInvasion == 42);

	CCloudProfileSummary Inspected;
	assert(CloudProfileInspect(aProfile, str_length(aProfile), &Inspected) == CLOUD_PROFILE_OK);
	assert(Inspected.m_ContentHash == Built.m_ContentHash);

	g_Config.m_ClPveResearchPoints = 0;
	g_Config.m_ClPveHighestInvasion = 0;
	g_Config.m_PlayerName[0] = 0;
	assert(CloudProfileApply(aProfile, str_length(aProfile), 0, &Inspected) == CLOUD_PROFILE_OK);
	assert(g_Config.m_ClPveResearchPoints == 17);
	assert(g_Config.m_ClPveHighestInvasion == 42);
	assert(str_comp(g_Config.m_PlayerName, "CloudTest") == 0);

	char aTampered[64 * 1024];
	str_copy(aTampered, aProfile, sizeof(aTampered));
	char *pPoints = (char *)str_find(aTampered, "\"cl_pve_research_points\":17");
	assert(pPoints);
	pPoints[str_length("\"cl_pve_research_points\":1")] = '8';
	assert(CloudProfileInspect(aTampered, str_length(aTampered), &Inspected) == CLOUD_PROFILE_CORRUPT);

	str_copy(aTampered, aProfile, sizeof(aTampered));
	char *pSchema = (char *)str_find(aTampered, "\"schema_version\":1");
	assert(pSchema);
	pSchema[str_length("\"schema_version\":")] = '2';
	assert(CloudProfileInspect(aTampered, str_length(aTampered), &Inspected) == CLOUD_PROFILE_FUTURE_VERSION);
	return 0;
}
