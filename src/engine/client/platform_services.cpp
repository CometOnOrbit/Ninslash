#include <base/system.h>
#include <engine/platform_services.h>

#if defined(CONF_STEAMWORKS)
#include <steam_api.h>
#endif

namespace
{
class CNullPlatformServices : public IPlatformServices
{
public:
	virtual bool Init() { return true; }
	virtual void Shutdown() {}
	virtual void RunCallbacks() {}
	virtual bool Available() const { return false; }
	virtual const char *PlatformName() const { return "standalone"; }
	virtual unsigned long long LocalUserID() const { return 0; }
	virtual int GetAuthSessionTicket(void *pBuffer, int BufferSize) { (void)pBuffer; (void)BufferSize; return 0; }
	virtual void SetRichPresence(const char *pStatus, const char *pConnect) { (void)pStatus; (void)pConnect; }
	virtual bool ConsumeJoinRequest(char *pBuffer, int BufferSize)
	{
		if(BufferSize > 0)
			pBuffer[0] = 0;
		return false;
	}
};

#if defined(CONF_STEAMWORKS)
class CSteamPlatformServices : public IPlatformServices
{
	bool m_Initialized;
	char m_aPendingJoin[256];
	void OnJoinRequested(GameRichPresenceJoinRequested_t *pRequest);
	CCallback<CSteamPlatformServices, GameRichPresenceJoinRequested_t> m_JoinRequestedCallback;

public:
	CSteamPlatformServices() :
		m_Initialized(false),
		m_JoinRequestedCallback(this, &CSteamPlatformServices::OnJoinRequested)
	{
		m_aPendingJoin[0] = 0;
	}

	virtual bool Init()
	{
		if(m_Initialized)
			return true;
		if(SteamAPI_RestartAppIfNecessary((AppId_t)STEAM_APP_ID))
			return false;
		m_Initialized = SteamAPI_Init();
		if(!m_Initialized)
		{
			dbg_msg("steam", "SteamAPI_Init failed; Steam features are unavailable");
			return false;
		}
		dbg_msg("steam", "initialized for user %llu", LocalUserID());
		return true;
	}

	virtual void Shutdown()
	{
		if(!m_Initialized)
			return;
		SteamFriends()->ClearRichPresence();
		SteamAPI_Shutdown();
		m_Initialized = false;
	}

	virtual void RunCallbacks()
	{
		if(m_Initialized)
			SteamAPI_RunCallbacks();
	}

	virtual bool Available() const { return m_Initialized; }
	virtual const char *PlatformName() const { return "steam"; }

	virtual unsigned long long LocalUserID() const
	{
		return m_Initialized && SteamUser() ? SteamUser()->GetSteamID().ConvertToUint64() : 0;
	}

	virtual int GetAuthSessionTicket(void *pBuffer, int BufferSize)
	{
		if(!m_Initialized || !SteamUser() || !pBuffer || BufferSize <= 0)
			return 0;
		uint32 TicketSize = 0;
		// Steamworks SDK 1.60+ requires an explicit remote identity. Passing
		// null keeps this as a ticket for a dedicated GameServer.
		SteamUser()->GetAuthSessionTicket(pBuffer, BufferSize, &TicketSize, 0);
		return TicketSize > (uint32)BufferSize ? 0 : (int)TicketSize;
	}

	virtual void SetRichPresence(const char *pStatus, const char *pConnect)
	{
		if(!m_Initialized || !SteamFriends())
			return;
		SteamFriends()->SetRichPresence("status", pStatus ? pStatus : "");
		SteamFriends()->SetRichPresence("connect", pConnect ? pConnect : "");
	}

	virtual bool ConsumeJoinRequest(char *pBuffer, int BufferSize)
	{
		if(!m_aPendingJoin[0] || BufferSize <= 0)
			return false;
		str_copy(pBuffer, m_aPendingJoin, BufferSize);
		m_aPendingJoin[0] = 0;
		return true;
	}
};

void CSteamPlatformServices::OnJoinRequested(GameRichPresenceJoinRequested_t *pRequest)
{
	str_copy(m_aPendingJoin, pRequest->m_rgchConnect, sizeof(m_aPendingJoin));
}
#endif
}

IPlatformServices *CreatePlatformServices()
{
#if defined(CONF_STEAMWORKS)
	return new CSteamPlatformServices();
#else
	return new CNullPlatformServices();
#endif
}
