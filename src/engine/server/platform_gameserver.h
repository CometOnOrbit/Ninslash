#ifndef ENGINE_SERVER_PLATFORM_GAMESERVER_H
#define ENGINE_SERVER_PLATFORM_GAMESERVER_H

#include <base/system.h>

#include <engine/platform_auth.h>

inline bool PlatformAuthResultAllowsJoin(EPlatformAuthResult Result) { return Result == PLATFORM_AUTH_OK; }
inline bool PlatformAuthResultIsTerminalFailure(EPlatformAuthResult Result) { return Result == PLATFORM_AUTH_INVALID_TICKET || Result == PLATFORM_AUTH_REPLAYED_TICKET || Result == PLATFORM_AUTH_UNAVAILABLE; }

class IPlatformGameServer
{
public:
	virtual ~IPlatformGameServer() {}
	virtual bool Init(unsigned short Port) = 0;
	virtual void Shutdown() = 0;
	virtual void RunCallbacks() = 0;
	virtual bool Available() const = 0;
	virtual void SetAdvertiseServerActive(bool Active) = 0;
	virtual void UpdateMetadata(const char *pName, const char *pMap, int Players, int MaxPlayers, bool PasswordProtected, bool Official, int AuthPolicy, const char *pModHash) = 0;
	virtual void UpdateUserData(unsigned long long SteamID, const char *pName, int Score) = 0;
	virtual EPlatformAuthResult Authenticate(unsigned long long SteamID, const void *pTicket, int TicketSize) = 0;
	virtual EPlatformAuthResult AuthenticationResult(unsigned long long SteamID) const = 0;
	virtual void EndAuthentication(unsigned long long SteamID) = 0;
};

IPlatformGameServer *CreatePlatformGameServer();

#endif
