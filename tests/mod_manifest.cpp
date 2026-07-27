#include <engine/shared/mod_manifest.h>

#include <assert.h>
#include <string.h>

int main()
{
	const char *pGood = "{\"published_file_id\":\"42\",\"name\":\"Safe\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"content_rating\":\"everyone\",\"api_version\":1,\"capabilities\":[\"resources\",\"client_theme\"],\"dependencies\":[{\"published_file_id\":\"7\",\"version\":\"2\",\"content_hash\":\"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\"}],\"maps\":[\"maps/a.map\"],\"scripts\":[\"rules/main.lua\"]}";
	char aError[128];
	assert(ModManifestValidateText(pGood, (int)strlen(pGood), "test", aError, sizeof(aError)));
	assert(!ModManifestIsSafeRelativePath("../server.cfg"));
	assert(!ModManifestIsSafeRelativePath("bin/evil.dll"));
	assert(!ModManifestIsSafeRelativePath("C:/escape"));
	assert(!ModManifestValidateText(pGood, (int)strlen(pGood), "other", aError, sizeof(aError)));
	CModApiDescriptor Descriptor;
	assert(ModManifestReadApiDescriptor(pGood, (int)strlen(pGood), &Descriptor, aError, sizeof(aError)));
	CModManifest Manifest;
	assert(ModManifestParse(pGood, (int)strlen(pGood), "test", &Manifest, aError, sizeof(aError)));
	assert(strcmp(Manifest.m_aContentRating, "everyone") == 0);
	assert(Manifest.m_DependencyCount == 1 && Manifest.m_FileCount == 2);
	const char *pMissingRating = "{\"published_file_id\":\"42\",\"name\":\"Safe\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"api_version\":1,\"capabilities\":[],\"maps\":[],\"resources\":[],\"scripts\":[]}";
	assert(!ModManifestValidateText(pMissingRating, (int)strlen(pMissingRating), "test", aError, sizeof(aError)));
	const char *pInvalidRating = "{\"published_file_id\":\"42\",\"name\":\"Safe\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"content_rating\":\"unrated\",\"api_version\":1,\"capabilities\":[],\"maps\":[],\"resources\":[],\"scripts\":[]}";
	assert(!ModManifestValidateText(pInvalidRating, (int)strlen(pInvalidRating), "test", aError, sizeof(aError)));
	assert(!ModManifestValidateText("{\"published_file_id\":\"x\"}", 25, "test", aError, sizeof(aError)));
	assert(ModApiCanActivate(Descriptor) == MOD_ACTIVATION_OK);
	Descriptor.m_Capabilities = MOD_CAPABILITY_GAMEPLAY_RULES;
#if defined(CONF_LUA_MOD_API)
	assert(ModApiCanActivate(Descriptor) == MOD_ACTIVATION_OK);
	Descriptor.m_Capabilities |= MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_ITEMS;
	assert(ModApiCanActivate(Descriptor) == MOD_ACTIVATION_OK);
#else
	assert(ModApiCanActivate(Descriptor) == MOD_ACTIVATION_UNSUPPORTED_CAPABILITY);
#endif
	return 0;
}
