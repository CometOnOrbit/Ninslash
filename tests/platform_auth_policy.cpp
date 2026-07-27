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
