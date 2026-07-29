#include <engine/shared/platform_server_metadata.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

int main()
{
	CPlatformServerMetadata Metadata;
	assert(PlatformServerMetadataParse("official=1,modded=0,modhash=none,auth=2", &Metadata));
	assert(Metadata.m_Official && !Metadata.m_Modded && Metadata.m_AuthPolicy == 2 && strcmp(Metadata.m_aModHash, "none") == 0);
	const char *pHash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	char aTags[160]; snprintf(aTags, sizeof(aTags), "modhash=%s,auth=1,official=0,modded=1", pHash);
	assert(PlatformServerMetadataParse(aTags, &Metadata) && !Metadata.m_Official && Metadata.m_Modded && Metadata.m_AuthPolicy == 1);
	assert(!PlatformServerMetadataParse("not_official=1,modded=0,modhash=none,auth=0", &Metadata));
	assert(!PlatformServerMetadataParse("official=1,modded=1,modhash=none,auth=2", &Metadata));
	assert(!PlatformServerMetadataParse("official=1,modded=1,modhash=../bad,auth=2", &Metadata));
	assert(!PlatformServerMetadataParse("official=0,modded=0,modhash=none", &Metadata));
	assert(!PlatformServerMetadataParse("official=0,modded=0,modhash=none,auth=3", &Metadata));
	return 0;
}
