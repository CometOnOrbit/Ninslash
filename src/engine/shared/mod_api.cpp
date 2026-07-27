#include "mod_api.h"

int ModApiCurrentVersion()
{
	return 1;
}

int ModApiSupportedCapabilities()
{
	int Capabilities = MOD_CAPABILITY_RESOURCES | MOD_CAPABILITY_CLIENT_THEME;
#if defined(CONF_LUA_MOD_API)
	Capabilities |= MOD_CAPABILITY_GAMEPLAY_RULES | MOD_CAPABILITY_WEAPONS | MOD_CAPABILITY_ITEMS;
#endif
	return Capabilities;
}

EModActivationResult ModApiCanActivate(const CModApiDescriptor &Descriptor)
{
	if(Descriptor.m_ApiVersion != ModApiCurrentVersion())
		return MOD_ACTIVATION_API_VERSION_MISMATCH;
	if(Descriptor.m_Capabilities & ~ModApiSupportedCapabilities())
		return MOD_ACTIVATION_UNSUPPORTED_CAPABILITY;
	return MOD_ACTIVATION_OK;
}

const char *ModActivationResultName(EModActivationResult Result)
{
	if(Result == MOD_ACTIVATION_OK)
		return "ok";
	if(Result == MOD_ACTIVATION_API_VERSION_MISMATCH)
		return "api_version_mismatch";
	if(Result == MOD_ACTIVATION_UNSUPPORTED_CAPABILITY)
		return "unsupported_capability";
	return "runtime_unavailable";
}
