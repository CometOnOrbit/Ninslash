#ifndef ENGINE_SHARED_PLATFORM_SERVER_METADATA_H
#define ENGINE_SHARED_PLATFORM_SERVER_METADATA_H

struct CPlatformServerMetadata
{
	bool m_Official;
	bool m_Modded;
	int m_AuthPolicy;
	char m_aModHash[65];
};

bool PlatformServerMetadataParse(const char *pTags, CPlatformServerMetadata *pMetadata);

#endif
