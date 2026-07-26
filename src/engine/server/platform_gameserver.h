#ifndef ENGINE_SERVER_PLATFORM_GAMESERVER_H
#define ENGINE_SERVER_PLATFORM_GAMESERVER_H

#include <base/system.h>

enum EPlatformAuthResult
{
	PLATFORM_AUTH_OK,
	PLATFORM_AUTH_UNAVAILABLE,
	PLATFORM_AUTH_INVALID_TICKET,
	PLATFORM_AUTH_REPLAYED_TICKET,
};

class IPlatformGameServer
{
public:
	virtual ~IPlatformGameServer() {}
	virtual bool Init(unsigned short Port) = 0;
	virtual void Shutdown() = 0;
	virtual void RunCallbacks() = 0;
	virtual bool Available() const = 0;
	virtual void UpdateMetadata(const char *pName, const char *pMap, int Players, int MaxPlayers, bool Official, const char *pModHash) = 0;
	virtual EPlatformAuthResult Authenticate(unsigned long long SteamID, const void *pTicket, int TicketSize) = 0;
	virtual void EndAuthentication(unsigned long long SteamID) = 0;
};

IPlatformGameServer *CreatePlatformGameServer();

#endif
