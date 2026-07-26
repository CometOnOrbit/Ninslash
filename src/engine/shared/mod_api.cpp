#include "mod_api.h"

int ModApiCurrentVersion()
{
	return 1;
}

int ModApiSupportedCapabilities()
{
	// This release deliberately exposes resource/theme packs only. Gameplay
	// capabilities remain declared in manifests for forward compatibility.
	return MOD_CAPABILITY_RESOURCES | MOD_CAPABILITY_CLIENT_THEME;
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
	return "unsupported_capability";
}
