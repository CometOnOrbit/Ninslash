#ifndef ENGINE_PLATFORM_AUTH_H
#define ENGINE_PLATFORM_AUTH_H

enum EPlatformAuthResult
{
	PLATFORM_AUTH_OK,
	PLATFORM_AUTH_PENDING,
	PLATFORM_AUTH_UNAVAILABLE,
	PLATFORM_AUTH_INVALID_TICKET,
	PLATFORM_AUTH_REPLAYED_TICKET,
};

enum EPlatformIdentityKind
{
	PLATFORM_IDENTITY_ANONYMOUS = 0,
	PLATFORM_IDENTITY_STEAM = 1,
};

enum EPlatformJoinDecision
{
	PLATFORM_JOIN_REJECT,
	PLATFORM_JOIN_ANONYMOUS,
	PLATFORM_JOIN_PENDING,
	PLATFORM_JOIN_VERIFIED,
};

inline bool PlatformConnectionUsesRelay(bool ServerHasRelayListener, bool PeerUsesSteamTransport)
{
	return ServerHasRelayListener && PeerUsesSteamTransport;
}

inline int PlatformEffectiveAuthPolicy(int ConfiguredPolicy, bool Official, bool Relay)
{
	if(Official || Relay)
		return 2;
	return ConfiguredPolicy < 0 ? 0 : ConfiguredPolicy > 2 ? 2 : ConfiguredPolicy;
}

inline int PlatformConnectionAuthPolicy(int ConfiguredPolicy,
										bool Official,
										bool ServerHasRelayListener,
										bool PeerUsesSteamTransport)
{
	const bool Relay = PlatformConnectionUsesRelay(ServerHasRelayListener, PeerUsesSteamTransport);
	// The listen-server owner joins over loopback. Steam Relay already proves
	// remote peer identity, while the local host must not depend on a separate
	// Steam GameServer authentication service inside the same process.
	if(ServerHasRelayListener && !Relay)
		return 0;
	return PlatformEffectiveAuthPolicy(ConfiguredPolicy, Official, Relay);
}

inline bool PlatformClientUsesSteamIdentity(int AuthPolicy, bool RelayRequired, bool Loopback, bool SteamAvailable)
{
	if(!SteamAvailable || AuthPolicy <= 0)
		return false;
	// Optional authentication must never make a local connection depend on a
	// Steam GameServer callback. Required and Relay connections still use Steam.
	return !Loopback || AuthPolicy >= 2 || RelayRequired;
}

inline bool PlatformAuthTimeoutAllowsAnonymous(int AuthPolicy, bool Relay)
{
	return AuthPolicy < 2 && !Relay;
}

inline EPlatformJoinDecision
PlatformJoinDecision(int IdentityKind, int AuthPolicy, bool Relay, EPlatformAuthResult AuthResult)
{
	if(IdentityKind == PLATFORM_IDENTITY_ANONYMOUS)
		return AuthPolicy < 2 && !Relay ? PLATFORM_JOIN_ANONYMOUS : PLATFORM_JOIN_REJECT;
	if(IdentityKind != PLATFORM_IDENTITY_STEAM)
		return PLATFORM_JOIN_REJECT;
	if(AuthResult == PLATFORM_AUTH_OK)
		return PLATFORM_JOIN_VERIFIED;
	if(AuthResult == PLATFORM_AUTH_PENDING)
		return PLATFORM_JOIN_PENDING;
	return PLATFORM_JOIN_REJECT;
}

#endif
