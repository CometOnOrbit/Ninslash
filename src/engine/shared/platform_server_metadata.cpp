#include "platform_server_metadata.h"

#include <base/system.h>

namespace
{
bool IsHash(const char *pValue)
{
	if(str_comp(pValue, "none") == 0)
		return true;
	if(str_length(pValue) != 64)
		return false;
	for(const char *p = pValue; *p; p++)
		if(!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
			return false;
	return true;
}
} // namespace

bool PlatformServerMetadataParse(const char *pTags, CPlatformServerMetadata *pMetadata)
{
	if(!pMetadata)
		return false;
	mem_zero(pMetadata, sizeof(*pMetadata));
	str_copy(pMetadata->m_aModHash, "none", sizeof(pMetadata->m_aModHash));
	if(!pTags)
		return false;
	bool GotOfficial = false, GotModded = false, GotHash = false, GotAuth = false;
	const char *p = pTags;
	while(*p)
	{
		char aToken[128];
		int Length = 0;
		while(p[Length] && p[Length] != ',' && Length < (int)sizeof(aToken) - 1)
		{
			aToken[Length] = p[Length];
			Length++;
		}
		aToken[Length] = 0;
		if(str_comp(aToken, "official=0") == 0 || str_comp(aToken, "official=1") == 0)
		{
			pMetadata->m_Official = aToken[9] == '1';
			GotOfficial = true;
		}
		else if(str_comp(aToken, "modded=0") == 0 || str_comp(aToken, "modded=1") == 0)
		{
			pMetadata->m_Modded = aToken[7] == '1';
			GotModded = true;
		}
		else if(str_comp_num(aToken, "modhash=", 8) == 0 && IsHash(aToken + 8))
		{
			str_copy(pMetadata->m_aModHash, aToken + 8, sizeof(pMetadata->m_aModHash));
			GotHash = true;
		}
		else if(str_comp(aToken, "auth=0") == 0 || str_comp(aToken, "auth=1") == 0 || str_comp(aToken, "auth=2") == 0)
		{
			pMetadata->m_AuthPolicy = aToken[5] - '0';
			GotAuth = true;
		}
		p += Length;
		if(*p == ',')
			p++;
		else if(*p)
			return false;
	}
	return GotOfficial && GotModded && GotHash && GotAuth &&
		   pMetadata->m_Modded == (str_comp(pMetadata->m_aModHash, "none") != 0);
}
