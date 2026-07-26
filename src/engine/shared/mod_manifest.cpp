#include "mod_manifest.h"

#include <base/system.h>
#include <engine/external/json-parser/json.h>

namespace
{
bool SetError(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText, ErrorSize);
	return false;
}

bool IsString(const json_value &Value)
{
	return Value.type == json_string && ((const char *)Value)[0] != 0;
}

bool IsHash(const char *pHash)
{
	if(str_length(pHash) != 64)
		return false;
	for(int i = 0; pHash[i]; i++)
		if(!((pHash[i] >= '0' && pHash[i] <= '9') || (pHash[i] >= 'a' && pHash[i] <= 'f') || (pHash[i] >= 'A' && pHash[i] <= 'F')))
			return false;
	return true;
}

bool HasUnsafeExtension(const char *pPath)
{
	const char *apExtensions[] = {".dll", ".so", ".dylib", ".exe", ".com", ".bat", ".cmd", ".msi"};
	const int Length = str_length(pPath);
	for(unsigned i = 0; i < sizeof(apExtensions) / sizeof(apExtensions[0]); i++)
	{
		const int ExtensionLength = str_length(apExtensions[i]);
		if(Length >= ExtensionLength && str_comp_nocase(pPath + Length - ExtensionLength, apExtensions[i]) == 0)
			return true;
	}
	return false;
}

bool ValidatePathArray(const json_value &Value)
{
	if(Value.type == json_none)
		return true;
	if(Value.type != json_array)
		return false;
	for(unsigned int i = 0; i < Value.u.array.length; i++)
		if(!IsString(Value[i]) || !ModManifestIsSafeRelativePath((const char *)Value[i]))
			return false;
	return true;
}

bool ValidateDependencyArray(const json_value &Value)
{
	if(Value.type == json_none)
		return true;
	if(Value.type != json_array)
		return false;
	for(unsigned int i = 0; i < Value.u.array.length; i++)
		if(!IsString(Value[i]) || str_length((const char *)Value[i]) > 128)
			return false;
	return true;
}

int CapabilityFromName(const char *pName)
{
	if(str_comp(pName, "resources") == 0) return MOD_CAPABILITY_RESOURCES;
	if(str_comp(pName, "client_theme") == 0) return MOD_CAPABILITY_CLIENT_THEME;
	if(str_comp(pName, "gameplay_rules") == 0) return MOD_CAPABILITY_GAMEPLAY_RULES;
	if(str_comp(pName, "weapons") == 0) return MOD_CAPABILITY_WEAPONS;
	if(str_comp(pName, "items") == 0) return MOD_CAPABILITY_ITEMS;
	return 0;
}

bool ReadApiDescriptor(const json_value &Root, CModApiDescriptor *pDescriptor)
{
	const json_value &ApiVersion = Root["api_version"];
	if(ApiVersion.type != json_integer || ApiVersion.u.integer <= 0 || ApiVersion.u.integer > 999)
		return false;
	const json_value &Capabilities = Root["capabilities"];
	if(Capabilities.type != json_array)
		return false;
	int Mask = 0;
	for(unsigned int i = 0; i < Capabilities.u.array.length; i++)
	{
		if(!IsString(Capabilities[i]))
			return false;
		const int Capability = CapabilityFromName((const char *)Capabilities[i]);
		if(!Capability || (Mask & Capability))
			return false;
		Mask |= Capability;
	}
	pDescriptor->m_ApiVersion = (int)ApiVersion.u.integer;
	pDescriptor->m_Capabilities = Mask;
	return true;
}
}

bool ModManifestIsSafeRelativePath(const char *pPath)
{
	if(!pPath || !pPath[0] || pPath[0] == '/' || pPath[0] == '\\' || (pPath[0] && pPath[1] == ':'))
		return false;
	for(const char *p = pPath; *p; p++)
	{
		if((unsigned char)*p < 32 || *p == '\\')
			return false;
	}
	if(str_find(pPath, "../") || str_find(pPath, "/..") || str_comp(pPath, "..") == 0 || HasUnsafeExtension(pPath))
		return false;
	return true;
}

bool ModManifestValidateText(const char *pJson, int JsonLength, const char *pExpectedProtocol, char *pError, int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = 0;
	if(!pJson || JsonLength <= 0 || JsonLength > 64 * 1024)
		return SetError(pError, ErrorSize, "invalid manifest size");
	json_settings Settings;
	mem_zero(&Settings, sizeof(Settings));
	char aJsonError[128];
	json_value *pRoot = json_parse_ex(&Settings, pJson, JsonLength, aJsonError);
	if(!pRoot || pRoot->type != json_object)
	{
		if(pRoot)
			json_value_free(pRoot);
		return SetError(pError, ErrorSize, "manifest is not a JSON object");
	}
	const json_value &Id = (*pRoot)["published_file_id"];
	const json_value &Name = (*pRoot)["name"];
	const json_value &Version = (*pRoot)["version"];
	const json_value &Author = (*pRoot)["author"];
	const json_value &Protocol = (*pRoot)["target_protocol"];
	const json_value &Hash = (*pRoot)["content_hash"];
	bool Valid = IsString(Id) && IsString(Name) && IsString(Version) && IsString(Author) && IsString(Protocol) && IsString(Hash);
	if(Valid && pExpectedProtocol && pExpectedProtocol[0])
		Valid = str_comp((const char *)Protocol, pExpectedProtocol) == 0;
	CModApiDescriptor Descriptor;
	if(Valid)
		Valid = IsHash((const char *)Hash) && ReadApiDescriptor(*pRoot, &Descriptor) && ValidateDependencyArray((*pRoot)["dependencies"]) && ValidatePathArray((*pRoot)["maps"]) && ValidatePathArray((*pRoot)["resources"]) && ValidatePathArray((*pRoot)["scripts"]);
	json_value_free(pRoot);
	return Valid ? true : SetError(pError, ErrorSize, "missing, unsafe, or incompatible manifest field");
}

bool ModManifestReadApiDescriptor(const char *pJson, int JsonLength, CModApiDescriptor *pDescriptor, char *pError, int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = 0;
	if(!pJson || !pDescriptor || JsonLength <= 0 || JsonLength > 64 * 1024)
		return SetError(pError, ErrorSize, "invalid manifest input");
	json_settings Settings;
	mem_zero(&Settings, sizeof(Settings));
	char aJsonError[128];
	json_value *pRoot = json_parse_ex(&Settings, pJson, JsonLength, aJsonError);
	const bool Valid = pRoot && pRoot->type == json_object && ReadApiDescriptor(*pRoot, pDescriptor);
	if(pRoot)
		json_value_free(pRoot);
	return Valid ? true : SetError(pError, ErrorSize, "invalid mod api descriptor");
}
