#ifndef ENGINE_SHARED_MOD_API_H
#define ENGINE_SHARED_MOD_API_H

/*
 * This is intentionally runtime-neutral. Lua is not loaded in the current
 * release; a future sandbox is required to implement IModRuntime and may not
 * bypass this capability and event boundary.
 */
enum EModCapability
{
	MOD_CAPABILITY_RESOURCES = 1 << 0,
	MOD_CAPABILITY_CLIENT_THEME = 1 << 1,
	MOD_CAPABILITY_GAMEPLAY_RULES = 1 << 2,
	MOD_CAPABILITY_WEAPONS = 1 << 3,
	MOD_CAPABILITY_ITEMS = 1 << 4,
};

enum EModEvent
{
	MOD_EVENT_ROUND_START,
	MOD_EVENT_ROUND_END,
	MOD_EVENT_ENTITY_CREATED,
	MOD_EVENT_ENTITY_DESTROYED,
	MOD_EVENT_DAMAGE,
	MOD_EVENT_PICKUP,
	MOD_EVENT_BUILD,
	MOD_EVENT_FORGE,
	MOD_EVENT_PVE_FLOOR_COMPLETE,
};

enum EModActivationResult
{
	MOD_ACTIVATION_OK,
	MOD_ACTIVATION_API_VERSION_MISMATCH,
	MOD_ACTIVATION_UNSUPPORTED_CAPABILITY,
};

struct CModApiDescriptor
{
	int m_ApiVersion;
	int m_Capabilities;
};

class IModEventSink
{
public:
	virtual ~IModEventSink() {}
	virtual void OnModEvent(EModEvent Event, int ClientID, int Value) = 0;
};

class IModRuntime
{
public:
	virtual ~IModRuntime() {}
	virtual EModActivationResult Activate(const CModApiDescriptor &Descriptor) = 0;
	virtual void Deactivate() = 0;
	virtual bool Active() const = 0;
};

int ModApiCurrentVersion();
int ModApiSupportedCapabilities();
EModActivationResult ModApiCanActivate(const CModApiDescriptor &Descriptor);
const char *ModActivationResultName(EModActivationResult Result);

#endif
