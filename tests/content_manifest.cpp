#include <engine/shared/content_manifest.h>

#include <assert.h>
#include <string.h>

int main()
{
	const char *pGood =
		"{\"schema_version\":1,\"content_type\":\"mod\",\"published_file_id\":\"42\",\"name\":\"Safe\",\"description\":"
		"\"Test "
		"content\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":"
		"\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"content_rating\":\"everyone\",\"api_"
		"version\":1,\"capabilities\":[\"resources\",\"client_theme\"],\"dependencies\":[{\"schema_version\":1,"
		"\"content_type\":\"mod\",\"published_file_id\":\"7\",\"version\":\"2\",\"content_hash\":"
		"\"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\"}],\"maps\":[\"maps/"
		"a.map\"],\"scripts\":[\"rules/main.lua\"]}";
	char aError[128];
	assert(ContentManifestValidateText(pGood, (int)strlen(pGood), "test", aError, sizeof(aError)));
	assert(!ContentManifestIsSafeRelativePath("../server.cfg"));
	assert(!ContentManifestIsSafeRelativePath("bin/evil.dll"));
	assert(!ContentManifestIsSafeRelativePath("C:/escape"));
	assert(!ContentManifestValidateText(pGood, (int)strlen(pGood), "other", aError, sizeof(aError)));
	CModApiDescriptor Descriptor;
	assert(ContentManifestReadApiDescriptor(pGood, (int)strlen(pGood), &Descriptor, aError, sizeof(aError)));
	CContentManifest Manifest;
	assert(ContentManifestParse(pGood, (int)strlen(pGood), "test", &Manifest, aError, sizeof(aError)));
	assert(strcmp(Manifest.m_aContentRating, "everyone") == 0);
	assert(Manifest.m_DependencyCount == 1 && Manifest.m_FileCount == 2);
	assert(Manifest.m_SchemaVersion == 1 && Manifest.m_ContentType == CONTENT_TYPE_MOD);
	const char *pMap = "{\"schema_version\":1,\"content_type\":\"map\",\"published_file_id\":\"44\",\"name\":\"Map\","
					   "\"description\":\"A "
					   "map\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":"
					   "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"content_rating\":"
					   "\"everyone\",\"maps\":[\"maps/main.map\"],\"resources\":[]}";
	assert(ContentManifestParse(pMap, (int)strlen(pMap), "test", &Manifest, aError, sizeof(aError)) &&
		   Manifest.m_ContentType == CONTENT_TYPE_MAP);
	const char *pUnknown = "{\"schema_version\":1,\"content_type\":\"save_game\",\"published_file_id\":\"44\"}";
	assert(!ContentManifestValidateText(pUnknown, (int)strlen(pUnknown), "test", aError, sizeof(aError)));
	const char *pMissingRating = "{\"schema_version\":1,\"content_type\":\"mod\",\"published_file_id\":\"42\",\"name\":"
								 "\"Safe\",\"description\":\"Test "
								 "content\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_"
								 "hash\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"api_"
								 "version\":1,\"capabilities\":[],\"maps\":[],\"resources\":[],\"scripts\":[]}";
	assert(!ContentManifestValidateText(pMissingRating, (int)strlen(pMissingRating), "test", aError, sizeof(aError)));
	const char *pInvalidRating =
		"{\"schema_version\":1,\"content_type\":\"mod\",\"published_file_id\":\"42\",\"name\":\"Safe\",\"description\":"
		"\"Test "
		"content\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":"
		"\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"content_rating\":\"unrated\",\"api_"
		"version\":1,\"capabilities\":[],\"maps\":[],\"resources\":[],\"scripts\":[]}";
	assert(!ContentManifestValidateText(pInvalidRating, (int)strlen(pInvalidRating), "test", aError, sizeof(aError)));
	assert(!ContentManifestValidateText("{\"schema_version\":1,\"content_type\":\"mod\",\"published_file_id\":\"x\"}",
										25,
										"test",
										aError,
										sizeof(aError)));
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
