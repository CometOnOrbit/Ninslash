#include "platform_gameserver.h"

#if defined(CONF_STEAMWORKS_GAMESERVER)
#include <steam_gameserver.h>
#endif

namespace
{
class CNullPlatformGameServer : public IPlatformGameServer
{
public:
	bool Init(unsigned short Port) { (void)Port; return true; }
	void Shutdown() {}
	void RunCallbacks() {}
	bool Available() const { return false; }
	EPlatformAuthResult Authenticate(unsigned long long SteamID, const void *pTicket, int TicketSize)
	{
		(void)SteamID;
		(void)pTicket;
		(void)TicketSize;
		return PLATFORM_AUTH_UNAVAILABLE;
	}
	void EndAuthentication(unsigned long long SteamID) { (void)SteamID; }
};

#if defined(CONF_STEAMWORKS_GAMESERVER)
class CSteamPlatformGameServer : public IPlatformGameServer
{
	bool m_Initialized;
public:
	CSteamPlatformGameServer() : m_Initialized(false) {}
	bool Init(unsigned short Port)
	{
		if(m_Initialized)
			return true;
		m_Initialized = SteamGameServer_Init(0, Port, Port + 1, eServerModeAuthenticationAndSecure, "1.0.0.0");
		if(m_Initialized)
		{
			SteamGameServer()->SetProduct("ninslash");
			SteamGameServer()->SetModDir("ninslash");
			SteamGameServer()->LogOnAnonymous();
		}
		return m_Initialized;
	}
	void Shutdown()
	{
		if(m_Initialized)
			SteamGameServer_Shutdown();
		m_Initialized = false;
	}
	void RunCallbacks() { if(m_Initialized) SteamGameServer_RunCallbacks(); }
	bool Available() const { return m_Initialized; }
	EPlatformAuthResult Authenticate(unsigned long long SteamID, const void *pTicket, int TicketSize)
	{
		if(!m_Initialized)
			return PLATFORM_AUTH_UNAVAILABLE;
		if(!SteamID || !pTicket || TicketSize <= 0 || TicketSize > 2048)
			return PLATFORM_AUTH_INVALID_TICKET;
		const EBeginAuthSessionResult Result = SteamGameServer()->BeginAuthSession(pTicket, TicketSize, CSteamID(SteamID));
		if(Result == k_EBeginAuthSessionResultOK)
			return PLATFORM_AUTH_OK;
		if(Result == k_EBeginAuthSessionResultDuplicateRequest)
			return PLATFORM_AUTH_REPLAYED_TICKET;
		return PLATFORM_AUTH_INVALID_TICKET;
	}
	void EndAuthentication(unsigned long long SteamID)
	{
		if(m_Initialized && SteamID)
			SteamGameServer()->EndAuthSession(CSteamID(SteamID));
	}
};
#endif
}

IPlatformGameServer *CreatePlatformGameServer()
{
#if defined(CONF_STEAMWORKS_GAMESERVER)
	return new CSteamPlatformGameServer();
#else
	return new CNullPlatformGameServer();
#endif
}
