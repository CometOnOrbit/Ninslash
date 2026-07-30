#include "cloud_profile.h"

#include <base/math.h>
#include <base/system.h>
#include <engine/external/json-parser/json.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <game/client/components/binds.h>

#include <stdarg.h>
#include <stdio.h>

namespace
{
enum
{
	CLOUD_PROFILE_SCHEMA = 1,
	CONTENT_BUFFER_SIZE = 60 * 1024
};

bool Append(char *pBuffer, int BufferSize, int *pOffset, const char *pFormat, ...)
{
	if(!pBuffer || !pOffset || *pOffset < 0 || *pOffset >= BufferSize)
		return false;
	va_list Args;
	va_start(Args, pFormat);
	const int Written = vsnprintf(pBuffer + *pOffset, BufferSize - *pOffset, pFormat, Args);
	va_end(Args);
	if(Written < 0 || Written >= BufferSize - *pOffset)
		return false;
	*pOffset += Written;
	return true;
}

bool AppendJsonString(char *pBuffer, int BufferSize, int *pOffset, const char *pText)
{
	if(!Append(pBuffer, BufferSize, pOffset, "\""))
		return false;
	for(const unsigned char *p = (const unsigned char *)(pText ? pText : ""); *p; ++p)
	{
		if(*p == '\"' || *p == '\\')
		{
			if(!Append(pBuffer, BufferSize, pOffset, "\\%c", *p))
				return false;
		}
		else if(*p == '\n')
		{
			if(!Append(pBuffer, BufferSize, pOffset, "\\n"))
				return false;
		}
		else if(*p == '\r')
		{
			if(!Append(pBuffer, BufferSize, pOffset, "\\r"))
				return false;
		}
		else if(*p == '\t')
		{
			if(!Append(pBuffer, BufferSize, pOffset, "\\t"))
				return false;
		}
		else if(*p >= 32)
		{
			if(!Append(pBuffer, BufferSize, pOffset, "%c", *p))
				return false;
		}
	}
	return Append(pBuffer, BufferSize, pOffset, "\"");
}

unsigned long long DocumentContentHash(const char *pData, int DataSize)
{
	const char *pProfile = pData ? strstr(pData, "\"profile\"") : 0;
	if(!pProfile || pProfile >= pData + DataSize)
		return 0;
	unsigned long long Hash = 1469598103934665603ULL;
	Hash ^= (unsigned char)'{';
	Hash *= 1099511628211ULL;
	for(const unsigned char *p = (const unsigned char *)pProfile; p < (const unsigned char *)pData + DataSize; ++p)
	{
		Hash ^= *p;
		Hash *= 1099511628211ULL;
	}
	return Hash;
}

bool ReadMetadata(const json_value &Root, CCloudProfileSummary *pSummary)
{
	const json_value &Schema = Root["schema_version"];
	const json_value &Revision = Root["revision"];
	const json_value &Modified = Root["modified_at"];
	const json_value &Hash = Root["content_hash"];
	if(Schema.type != json_integer || Revision.type != json_integer || Modified.type != json_integer ||
	   Hash.type != json_string)
		return false;
	char Trailing = 0;
	unsigned long long ParsedHash = 0;
	if(sscanf((const char *)Hash, "%llx%c", &ParsedHash, &Trailing) != 1)
		return false;
	if(pSummary)
	{
		mem_zero(pSummary, sizeof(*pSummary));
		pSummary->m_SchemaVersion = (int)Schema.u.integer;
		pSummary->m_Revision = max(0, (int)Revision.u.integer);
		pSummary->m_ModifiedAt = (long long)Modified.u.integer;
		pSummary->m_ContentHash = ParsedHash;
	}
	return true;
}

ECloudProfileReadResult Parse(const char *pData, int DataSize, json_value **ppRoot, CCloudProfileSummary *pSummary)
{
	if(ppRoot)
		*ppRoot = 0;
	if(!pData || DataSize <= 0 || DataSize > 64 * 1024)
		return CLOUD_PROFILE_CORRUPT;
	json_settings Settings;
	mem_zero(&Settings, sizeof(Settings));
	char aError[128];
	json_value *pRoot = json_parse_ex(&Settings, pData, DataSize, aError);
	if(!pRoot || pRoot->type != json_object || !ReadMetadata(*pRoot, pSummary))
	{
		if(pRoot)
			json_value_free(pRoot);
		return CLOUD_PROFILE_CORRUPT;
	}
	if(pSummary && DocumentContentHash(pData, DataSize) != pSummary->m_ContentHash)
	{
		json_value_free(pRoot);
		return CLOUD_PROFILE_CORRUPT;
	}
	if(pSummary && pSummary->m_SchemaVersion > CLOUD_PROFILE_SCHEMA)
	{
		json_value_free(pRoot);
		return CLOUD_PROFILE_FUTURE_VERSION;
	}
	if(pSummary && pSummary->m_SchemaVersion <= 0)
	{
		json_value_free(pRoot);
		return CLOUD_PROFILE_CORRUPT;
	}
	const json_value &Profile = (*pRoot)["profile"];
	if(Profile.type != json_object)
	{
		json_value_free(pRoot);
		return CLOUD_PROFILE_CORRUPT;
	}
	if(pSummary)
	{
		const json_value &Points = Profile["cl_pve_research_points"];
		const json_value &Highest = Profile["cl_pve_highest_invasion"];
		const json_value &Tutorial = Profile["cl_tutorial_completed_mask"];
		pSummary->m_ResearchPoints = Points.type == json_integer ? (int)Points.u.integer : 0;
		pSummary->m_HighestInvasion = Highest.type == json_integer ? (int)Highest.u.integer : 0;
		pSummary->m_TutorialCompletedMask = Tutorial.type == json_integer ? (int)Tutorial.u.integer : 0;
	}
	if(ppRoot)
		*ppRoot = pRoot;
	else
		json_value_free(pRoot);
	return CLOUD_PROFILE_OK;
}
} // namespace

unsigned long long CloudProfileHash(const void *pData, int Size)
{
	const unsigned char *pBytes = (const unsigned char *)pData;
	unsigned long long Hash = 1469598103934665603ULL;
	for(int i = 0; i < Size; ++i)
	{
		Hash ^= pBytes[i];
		Hash *= 1099511628211ULL;
	}
	return Hash;
}

bool CloudProfileBuild(
	CBinds *pBinds, int Revision, long long ModifiedAt, char *pBuffer, int BufferSize, CCloudProfileSummary *pSummary)
{
	char aContent[CONTENT_BUFFER_SIZE];
	int Offset = 0;
	bool First = true;
	if(!Append(aContent, sizeof(aContent), &Offset, "{\"profile\":{"))
		return false;
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc)                                                 \
	if((Flags)&CFGFLAG_CLOUD)                                                                                          \
	{                                                                                                                  \
		if(!Append(                                                                                                    \
			   aContent, sizeof(aContent), &Offset, "%s\"%s\":%d", First ? "" : ",", #ScriptName, g_Config.m_##Name))  \
			return false;                                                                                              \
		First = false;                                                                                                 \
	}
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc)                                                      \
	if((Flags)&CFGFLAG_CLOUD)                                                                                          \
	{                                                                                                                  \
		if(!Append(aContent, sizeof(aContent), &Offset, "%s\"%s\":", First ? "" : ",", #ScriptName) ||                 \
		   !AppendJsonString(aContent, sizeof(aContent), &Offset, g_Config.m_##Name))                                  \
			return false;                                                                                              \
		First = false;                                                                                                 \
	}
#include <engine/shared/config_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_STR
	if(!Append(aContent, sizeof(aContent), &Offset, "},\"binds\":{"))
		return false;
	First = true;
	if(pBinds)
	{
		for(int i = 1; i < KEY_LAST; ++i)
		{
			const char *pBind = pBinds->Get(i);
			if(!pBind || !pBind[0])
				continue;
			if(!Append(aContent, sizeof(aContent), &Offset, "%s\"%d\":", First ? "" : ",", i) ||
			   !AppendJsonString(aContent, sizeof(aContent), &Offset, pBind))
				return false;
			First = false;
		}
	}
	if(!Append(aContent, sizeof(aContent), &Offset, "}}"))
		return false;
	const unsigned long long Hash = CloudProfileHash(aContent, Offset);
	int Out = 0;
	if(!Append(pBuffer,
			   BufferSize,
			   &Out,
			   "{\"schema_version\":%d,\"revision\":%d,\"modified_at\":%lld,\"content_hash\":\"%016llx\",",
			   CLOUD_PROFILE_SCHEMA,
			   max(0, Revision),
			   ModifiedAt,
			   Hash))
		return false;
	if(!Append(pBuffer, BufferSize, &Out, "%s", aContent + 1))
		return false;
	if(pSummary)
		CloudProfileInspect(pBuffer, Out, pSummary);
	return true;
}

ECloudProfileReadResult CloudProfileInspect(const char *pData, int DataSize, CCloudProfileSummary *pSummary)
{
	return Parse(pData, DataSize, 0, pSummary);
}

ECloudProfileReadResult
CloudProfileApply(const char *pData, int DataSize, CBinds *pBinds, CCloudProfileSummary *pSummary)
{
	json_value *pRoot = 0;
	const ECloudProfileReadResult Result = Parse(pData, DataSize, &pRoot, pSummary);
	if(Result != CLOUD_PROFILE_OK)
		return Result;
	const json_value &Profile = (*pRoot)["profile"];
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc)                                                 \
	if((Flags)&CFGFLAG_CLOUD)                                                                                          \
	{                                                                                                                  \
		const json_value &Value = Profile[#ScriptName];                                                                \
		if(Value.type == json_integer)                                                                                 \
			g_Config.m_##Name = clamp((int)Value.u.integer, (int)(Min), (int)(Max));                                   \
	}
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc)                                                      \
	if((Flags)&CFGFLAG_CLOUD)                                                                                          \
	{                                                                                                                  \
		const json_value &Value = Profile[#ScriptName];                                                                \
		if(Value.type == json_string)                                                                                  \
			str_copy(g_Config.m_##Name, (const char *)Value, sizeof(g_Config.m_##Name));                               \
	}
#include <engine/shared/config_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_STR
	const json_value &Binds = (*pRoot)["binds"];
	if(pBinds && Binds.type == json_object)
	{
		pBinds->UnbindAll();
		for(int i = 1; i < KEY_LAST; ++i)
		{
			char aKey[16];
			str_format(aKey, sizeof(aKey), "%d", i);
			const json_value &Value = Binds.operator[](aKey);
			if(Value.type == json_string)
				pBinds->Bind(i, (const char *)Value);
		}
	}
	json_value_free(pRoot);
	return CLOUD_PROFILE_OK;
}
