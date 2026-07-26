#include <engine/shared/mod_manifest.h>

#include <assert.h>
#include <string.h>

int main()
{
	const char *pGood = "{\"published_file_id\":\"42\",\"name\":\"Safe\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"api_version\":1,\"capabilities\":[\"resources\",\"client_theme\"],\"maps\":[\"maps/a.map\"],\"scripts\":[\"rules/main.lua\"]}";
	char aError[128];
	assert(ModManifestValidateText(pGood, (int)strlen(pGood), "test", aError, sizeof(aError)));
	assert(!ModManifestIsSafeRelativePath("../server.cfg"));
	assert(!ModManifestIsSafeRelativePath("bin/evil.dll"));
	assert(!ModManifestIsSafeRelativePath("C:/escape"));
	assert(!ModManifestValidateText(pGood, (int)strlen(pGood), "other", aError, sizeof(aError)));
	CModApiDescriptor Descriptor;
	assert(ModManifestReadApiDescriptor(pGood, (int)strlen(pGood), &Descriptor, aError, sizeof(aError)));
	assert(ModApiCanActivate(Descriptor) == MOD_ACTIVATION_OK);
	Descriptor.m_Capabilities = MOD_CAPABILITY_GAMEPLAY_RULES;
	assert(ModApiCanActivate(Descriptor) == MOD_ACTIVATION_UNSUPPORTED_CAPABILITY);
	return 0;
}
