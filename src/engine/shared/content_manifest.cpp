#include "content_manifest.h"

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
		if(!((pHash[i] >= '0' && pHash[i] <= '9') || (pHash[i] >= 'a' && pHash[i] <= 'f') ||
			 (pHash[i] >= 'A' && pHash[i] <= 'F')))
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
		if(!IsString(Value[i]) || !ContentManifestIsSafeRelativePath((const char *)Value[i]))
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
	{
		if(Value[i].type != json_object || !IsString(Value[i]["published_file_id"]) || !IsString(Value[i]["version"]) ||
		   !IsString(Value[i]["content_hash"]) || !IsHash((const char *)Value[i]["content_hash"]))
			return false;
		for(unsigned int j = 0; j < i; j++)
			if(str_comp((const char *)Value[i]["published_file_id"], (const char *)Value[j]["published_file_id"]) == 0)
				return false;
	}
	return true;
}

bool IsPublishedFileID(const char *pID)
{
	if(!pID || !pID[0] || str_length(pID) >= 32)
		return false;
	for(const char *p = pID; *p; p++)
		if(*p < '0' || *p > '9')
			return false;
	return str_comp(pID, "0") != 0;
}

const char *ContentTypeNameInternal(int Type)
{
	static const char *s_apNames[] = {"mod", "map", "room_preset", "challenge"};
	return Type >= 0 && Type < NUM_CONTENT_TYPES ? s_apNames[Type] : "unknown";
}

bool ContentTypeFromNameInternal(const char *pName, int *pType)
{
	if(!pName || !pType)
		return false;
	for(int i = 0; i < NUM_CONTENT_TYPES; i++)
		if(str_comp(pName, ContentTypeNameInternal(i)) == 0)
		{
			*pType = i;
			return true;
		}
	return false;
}

bool IsContentRating(const char *pRating)
{
	return pRating &&
		   (str_comp(pRating, "everyone") == 0 || str_comp(pRating, "teen") == 0 || str_comp(pRating, "mature") == 0);
}

bool AddFiles(const json_value &Array, int Type, CContentManifest *pManifest)
{
	if(Array.type == json_none)
		return true;
	if(Array.type != json_array)
		return false;
	for(unsigned int i = 0; i < Array.u.array.length; i++)
	{
		if(!IsString(Array[i]) || !ContentManifestIsSafeRelativePath((const char *)Array[i]) ||
		   str_length((const char *)Array[i]) >= 256 || pManifest->m_FileCount >= CContentManifest::MAX_FILES)
			return false;
		for(int j = 0; j < pManifest->m_FileCount; j++)
			if(str_comp(pManifest->m_aFiles[j].m_aPath, (const char *)Array[i]) == 0)
				return false;
		CContentDeclaredFile &File = pManifest->m_aFiles[pManifest->m_FileCount++];
		str_copy(File.m_aPath, (const char *)Array[i], sizeof(File.m_aPath));
		File.m_Type = Type;
	}
	return true;
}

int CapabilityFromName(const char *pName)
{
	if(str_comp(pName, "resources") == 0)
		return MOD_CAPABILITY_RESOURCES;
	if(str_comp(pName, "client_theme") == 0)
		return MOD_CAPABILITY_CLIENT_THEME;
	if(str_comp(pName, "gameplay_rules") == 0)
		return MOD_CAPABILITY_GAMEPLAY_RULES;
	if(str_comp(pName, "weapons") == 0)
		return MOD_CAPABILITY_WEAPONS;
	if(str_comp(pName, "items") == 0)
		return MOD_CAPABILITY_ITEMS;
	if(str_comp(pName, "weapon_modules") == 0)
		return MOD_CAPABILITY_WEAPON_MODULES;
	if(str_comp(pName, "forge_recipes") == 0)
		return MOD_CAPABILITY_FORGE_RECIPES;
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
} // namespace

const char *ContentTypeName(int Type)
{
	return ContentTypeNameInternal(Type);
}
bool ContentTypeFromName(const char *pName, int *pType)
{
	return ContentTypeFromNameInternal(pName, pType);
}

bool ContentManifestIsSafeRelativePath(const char *pPath)
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

bool ContentManifestValidateText(
	const char *pJson, int JsonLength, const char *pExpectedProtocol, char *pError, int ErrorSize)
{
	CContentManifest Manifest;
	return ContentManifestParse(pJson, JsonLength, pExpectedProtocol, &Manifest, pError, ErrorSize);
}

bool ContentManifestParse(const char *pJson,
						  int JsonLength,
						  const char *pExpectedProtocol,
						  CContentManifest *pManifest,
						  char *pError,
						  int ErrorSize)
{
	if(pError && ErrorSize > 0)
		pError[0] = 0;
	if(!pJson || !pManifest || JsonLength <= 0 || JsonLength > 64 * 1024)
		return SetError(pError, ErrorSize, "invalid manifest size");
	mem_zero(pManifest, sizeof(*pManifest));
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
	const json_value &Schema = (*pRoot)["schema_version"];
	const json_value &ContentType = (*pRoot)["content_type"];
	const json_value &Name = (*pRoot)["name"];
	const json_value &Description = (*pRoot)["description"];
	const json_value &Version = (*pRoot)["version"];
	const json_value &Author = (*pRoot)["author"];
	const json_value &Protocol = (*pRoot)["target_protocol"];
	const json_value &Hash = (*pRoot)["content_hash"];
	const json_value &Rating = (*pRoot)["content_rating"];
	int Type = -1;
	bool Valid = Schema.type == json_integer && Schema.u.integer == 1 && IsString(ContentType) &&
				 ContentTypeFromName((const char *)ContentType, &Type) && IsString(Id) &&
				 IsPublishedFileID((const char *)Id) && IsString(Name) && Description.type == json_string &&
				 IsString(Version) && IsString(Author) && IsString(Protocol) && IsString(Hash) && IsString(Rating) &&
				 IsContentRating((const char *)Rating);
	if(Valid && pExpectedProtocol && pExpectedProtocol[0])
		Valid = str_comp((const char *)Protocol, pExpectedProtocol) == 0;
	if(Valid)
		Valid = IsHash((const char *)Hash) && ValidateDependencyArray((*pRoot)["dependencies"]) &&
				ValidatePathArray((*pRoot)["maps"]) && ValidatePathArray((*pRoot)["resources"]) &&
				ValidatePathArray((*pRoot)["scripts"]) && ValidatePathArray((*pRoot)["definitions"]);
	if(Valid && Type == CONTENT_TYPE_MOD)
		Valid = ReadApiDescriptor(*pRoot, &pManifest->m_Api);
	if(Valid && Type != CONTENT_TYPE_MOD && (*pRoot)["scripts"].type != json_none &&
	   (*pRoot)["scripts"].u.array.length != 0)
		Valid = false;
	if(Valid)
	{
		pManifest->m_SchemaVersion = 1;
		pManifest->m_ContentType = Type;
		str_copy(pManifest->m_aPublishedFileID, (const char *)Id, sizeof(pManifest->m_aPublishedFileID));
		str_copy(pManifest->m_aName, (const char *)Name, sizeof(pManifest->m_aName));
		str_copy(pManifest->m_aDescription, (const char *)Description, sizeof(pManifest->m_aDescription));
		str_copy(pManifest->m_aVersion, (const char *)Version, sizeof(pManifest->m_aVersion));
		str_copy(pManifest->m_aAuthor, (const char *)Author, sizeof(pManifest->m_aAuthor));
		str_copy(pManifest->m_aTargetProtocol, (const char *)Protocol, sizeof(pManifest->m_aTargetProtocol));
		str_copy(pManifest->m_aContentHash, (const char *)Hash, sizeof(pManifest->m_aContentHash));
		str_copy(pManifest->m_aContentRating, (const char *)Rating, sizeof(pManifest->m_aContentRating));
		const json_value &Dependencies = (*pRoot)["dependencies"];
		if(Dependencies.type == json_array)
			for(unsigned int i = 0; Valid && i < Dependencies.u.array.length; i++)
			{
				if(pManifest->m_DependencyCount >= CContentManifest::MAX_DEPENDENCIES)
				{
					Valid = false;
					break;
				}
				CContentDependency &Dependency = pManifest->m_aDependencies[pManifest->m_DependencyCount++];
				const char *pID = (const char *)Dependencies[i]["published_file_id"];
				const char *pVersion = (const char *)Dependencies[i]["version"];
				if(!IsPublishedFileID(pID) || str_length(pVersion) >= (int)sizeof(Dependency.m_aVersion))
				{
					Valid = false;
					break;
				}
				str_copy(Dependency.m_aPublishedFileID, pID, sizeof(Dependency.m_aPublishedFileID));
				str_copy(Dependency.m_aVersion, pVersion, sizeof(Dependency.m_aVersion));
				str_copy(Dependency.m_aContentHash,
						 (const char *)Dependencies[i]["content_hash"],
						 sizeof(Dependency.m_aContentHash));
			}
		Valid = Valid && AddFiles((*pRoot)["maps"], CONTENT_FILE_MAP, pManifest) &&
				AddFiles((*pRoot)["resources"], CONTENT_FILE_RESOURCE, pManifest) &&
				AddFiles((*pRoot)["scripts"], CONTENT_FILE_SCRIPT, pManifest) &&
				AddFiles((*pRoot)["definitions"], CONTENT_FILE_DEFINITION, pManifest);
		int Maps = 0, Definitions = 0;
		for(int i = 0; i < pManifest->m_FileCount; i++)
		{
			Maps += pManifest->m_aFiles[i].m_Type == CONTENT_FILE_MAP;
			Definitions += pManifest->m_aFiles[i].m_Type == CONTENT_FILE_DEFINITION;
		}
		if(Type == CONTENT_TYPE_MAP)
			Valid = Maps == 1;
		if(Type == CONTENT_TYPE_ROOM_PRESET || Type == CONTENT_TYPE_CHALLENGE)
			Valid = Definitions == 1;
	}
	json_value_free(pRoot);
	return Valid ? true : SetError(pError, ErrorSize, "missing, unsafe, or incompatible manifest field");
}

bool ContentManifestReadApiDescriptor(
	const char *pJson, int JsonLength, CModApiDescriptor *pDescriptor, char *pError, int ErrorSize)
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
