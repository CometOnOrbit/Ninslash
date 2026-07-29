#include <engine/server/platform_gameserver.h>

#include <assert.h>

int main()
{
	assert(PlatformAuthResultAllowsJoin(PLATFORM_AUTH_OK));
	assert(!PlatformAuthResultAllowsJoin(PLATFORM_AUTH_PENDING));
	assert(!PlatformAuthResultAllowsJoin(PLATFORM_AUTH_INVALID_TICKET));
	assert(!PlatformAuthResultAllowsJoin(PLATFORM_AUTH_REPLAYED_TICKET));
	assert(!PlatformAuthResultAllowsJoin(PLATFORM_AUTH_UNAVAILABLE));
	assert(PlatformAuthResultIsTerminalFailure(PLATFORM_AUTH_INVALID_TICKET));
	assert(PlatformAuthResultIsTerminalFailure(PLATFORM_AUTH_REPLAYED_TICKET));
	assert(PlatformAuthResultIsTerminalFailure(PLATFORM_AUTH_UNAVAILABLE));
	assert(!PlatformAuthResultIsTerminalFailure(PLATFORM_AUTH_PENDING));
	assert(!PlatformConnectionUsesRelay(false, false));
	assert(!PlatformConnectionUsesRelay(false, true));
	assert(!PlatformConnectionUsesRelay(true, false)); // Listen-server host over loopback.
	assert(PlatformConnectionUsesRelay(true, true)); // Remote Steam peer.

	assert(PlatformEffectiveAuthPolicy(0, false, false) == 0);
	assert(PlatformEffectiveAuthPolicy(1, false, false) == 1);
	assert(PlatformEffectiveAuthPolicy(1, true, false) == 2);
	assert(PlatformEffectiveAuthPolicy(0, false, true) == 2);
	assert(PlatformConnectionAuthPolicy(1, false, false, false) == 1); // Community dedicated server.
	assert(PlatformConnectionAuthPolicy(2, true, false, false) == 2); // Official dedicated server.
	assert(PlatformConnectionAuthPolicy(2, false, true, false) == 0); // Listen-server host over loopback.
	assert(PlatformConnectionAuthPolicy(0, false, true, true) == 2); // Remote Relay peer.
	assert(!PlatformClientUsesSteamIdentity(0, false, true, true));
	assert(!PlatformClientUsesSteamIdentity(1, false, true, true)); // Optional auth never blocks loopback.
	assert(PlatformClientUsesSteamIdentity(2, false, true, true));
	assert(PlatformClientUsesSteamIdentity(1, false, false, true));
	assert(PlatformClientUsesSteamIdentity(1, true, true, true));
	assert(!PlatformClientUsesSteamIdentity(2, true, false, false));
	assert(PlatformAuthTimeoutAllowsAnonymous(0, false));
	assert(PlatformAuthTimeoutAllowsAnonymous(1, false));
	assert(!PlatformAuthTimeoutAllowsAnonymous(2, false));
	assert(!PlatformAuthTimeoutAllowsAnonymous(0, true));

	assert(PlatformJoinDecision(PLATFORM_IDENTITY_ANONYMOUS, 0, false, PLATFORM_AUTH_UNAVAILABLE) == PLATFORM_JOIN_ANONYMOUS);
	assert(PlatformJoinDecision(PLATFORM_IDENTITY_ANONYMOUS, 1, false, PLATFORM_AUTH_UNAVAILABLE) == PLATFORM_JOIN_ANONYMOUS);
	assert(PlatformJoinDecision(PLATFORM_IDENTITY_ANONYMOUS, 2, false, PLATFORM_AUTH_UNAVAILABLE) == PLATFORM_JOIN_REJECT);
	assert(PlatformJoinDecision(PLATFORM_IDENTITY_ANONYMOUS, 1, true, PLATFORM_AUTH_UNAVAILABLE) == PLATFORM_JOIN_REJECT);
	assert(PlatformJoinDecision(PLATFORM_IDENTITY_STEAM, 1, false, PLATFORM_AUTH_OK) == PLATFORM_JOIN_VERIFIED);
	assert(PlatformJoinDecision(PLATFORM_IDENTITY_STEAM, 1, false, PLATFORM_AUTH_PENDING) == PLATFORM_JOIN_PENDING);
	assert(PlatformJoinDecision(PLATFORM_IDENTITY_STEAM, 1, false, PLATFORM_AUTH_UNAVAILABLE) == PLATFORM_JOIN_REJECT);
	assert(PlatformJoinDecision(PLATFORM_IDENTITY_STEAM, 1, false, PLATFORM_AUTH_INVALID_TICKET) == PLATFORM_JOIN_REJECT);
	assert(PlatformJoinDecision(PLATFORM_IDENTITY_STEAM, 1, false, PLATFORM_AUTH_REPLAYED_TICKET) == PLATFORM_JOIN_REJECT);
	assert(PlatformJoinDecision(99, 1, false, PLATFORM_AUTH_OK) == PLATFORM_JOIN_REJECT);
	return 0;
}
