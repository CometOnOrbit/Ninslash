#include <engine/shared/mod_manifest.h>

#include <assert.h>
#include <string.h>

int main()
{
	const char *pGood = "{\"published_file_id\":\"42\",\"name\":\"Safe\",\"version\":\"1\",\"author\":\"A\",\"target_protocol\":\"test\",\"content_hash\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"maps\":[\"maps/a.map\"],\"scripts\":[\"rules/main.lua\"]}";
	char aError[128];
	assert(ModManifestValidateText(pGood, (int)strlen(pGood), "test", aError, sizeof(aError)));
	assert(!ModManifestIsSafeRelativePath("../server.cfg"));
	assert(!ModManifestIsSafeRelativePath("bin/evil.dll"));
	assert(!ModManifestIsSafeRelativePath("C:/escape"));
	assert(!ModManifestValidateText(pGood, (int)strlen(pGood), "other", aError, sizeof(aError)));
	return 0;
}
