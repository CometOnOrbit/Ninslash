#include "platform_gameserver.h"

#include <game/version.h>

#if defined(CONF_STEAMWORKS_GAMESERVER)
#include <steam_gameserver.h>
#include <stdlib.h>
#endif

namespace
{
class CNullPlatformGameServer : public IPlatformGameServer
{
  public:
	bool Init(unsigned short Port)
	{
		(void)Port;
		return true;
	}
	void Shutdown() {}
	void RunCallbacks() {}
	bool Available() const { return false; }
	void SetAdvertiseServerActive(bool Active) { (void)Active; }
	void UpdateMetadata(const char *pName,
						const char *pMap,
						int Players,
						int MaxPlayers,
						bool PasswordProtected,
						bool Official,
						int AuthPolicy,
						const char *pModHash)
	{
		(void)pName;
		(void)pMap;
		(void)Players;
		(void)MaxPlayers;
		(void)PasswordProtected;
		(void)Official;
		(void)AuthPolicy;
		(void)pModHash;
	}
	void UpdateUserData(unsigned long long SteamID, const char *pName, int Score)
	{
		(void)SteamID;
		(void)pName;
		(void)Score;
	}
	EPlatformAuthResult Authenticate(unsigned long long SteamID, const void *pTicket, int TicketSize)
	{
		(void)SteamID;
		(void)pTicket;
		(void)TicketSize;
		return PLATFORM_AUTH_UNAVAILABLE;
	}
	EPlatformAuthResult AuthenticationResult(unsigned long long SteamID) const
	{
		(void)SteamID;
		return PLATFORM_AUTH_UNAVAILABLE;
	}
	void EndAuthentication(unsigned long long SteamID) { (void)SteamID; }
};

#if defined(CONF_STEAMWORKS_GAMESERVER)
class CSteamPlatformGameServer : public IPlatformGameServer
{
	struct CAuthSession
	{
		unsigned long long m_SteamID;
		EPlatformAuthResult m_Result;
	};
	CAuthSession m_aAuthSessions[64];
	bool m_Initialized;
	bool m_Connected;
	bool m_AdvertiseRequested;
	CCallbackManual<CSteamPlatformGameServer, ValidateAuthTicketResponse_t, true> m_ValidateAuthCallback;
	CCallbackManual<CSteamPlatformGameServer, SteamServersConnected_t, true> m_ServersConnectedCallback;
	CCallbackManual<CSteamPlatformGameServer, SteamServerConnectFailure_t, true> m_ServerConnectFailureCallback;
	CCallbackManual<CSteamPlatformGameServer, SteamServersDisconnected_t, true> m_ServersDisconnectedCallback;

	void OnServersConnected(SteamServersConnected_t *pResponse)
	{
		(void)pResponse;
		m_Connected = true;
		if(SteamGameServer())
			SteamGameServer()->SetAdvertiseServerActive(m_AdvertiseRequested);
		dbg_msg("steam", "Steam GameServer connected");
	}

	void OnServerConnectFailure(SteamServerConnectFailure_t *pResponse)
	{
		m_Connected = false;
		if(SteamGameServer())
			SteamGameServer()->SetAdvertiseServerActive(false);
		dbg_msg("steam",
				"Steam GameServer connection failed: result=%d retrying=%d",
				pResponse ? (int)pResponse->m_eResult : -1,
				pResponse && pResponse->m_bStillRetrying ? 1 : 0);
	}

	void OnServersDisconnected(SteamServersDisconnected_t *pResponse)
	{
		m_Connected = false;
		if(SteamGameServer())
			SteamGameServer()->SetAdvertiseServerActive(false);
		dbg_msg("steam", "Steam GameServer disconnected: result=%d", pResponse ? (int)pResponse->m_eResult : -1);
	}

	int FindSession(unsigned long long SteamID) const
	{
		for(int i = 0; i < 64; i++)
			if(m_aAuthSessions[i].m_SteamID == SteamID)
				return i;
		return -1;
	}

	int FreeSession() const
	{
		for(int i = 0; i < 64; i++)
			if(!m_aAuthSessions[i].m_SteamID)
				return i;
		return -1;
	}

	void OnValidateAuthTicket(ValidateAuthTicketResponse_t *pResponse)
	{
		if(!pResponse)
			return;
		const int Session = FindSession(pResponse->m_SteamID.ConvertToUint64());
		if(Session < 0)
			return;
		if(pResponse->m_eAuthSessionResponse == k_EAuthSessionResponseOK)
			m_aAuthSessions[Session].m_Result = PLATFORM_AUTH_OK;
		else if(pResponse->m_eAuthSessionResponse == k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed)
			m_aAuthSessions[Session].m_Result = PLATFORM_AUTH_REPLAYED_TICKET;
		else
			m_aAuthSessions[Session].m_Result = PLATFORM_AUTH_INVALID_TICKET;
	}

  public:
	CSteamPlatformGameServer() : m_Initialized(false), m_Connected(false), m_AdvertiseRequested(false)
	{
		mem_zero(m_aAuthSessions, sizeof(m_aAuthSessions));
	}
	bool Init(unsigned short Port)
	{
		if(m_Initialized)
			return true;
#if defined(CONF_FAMILY_WINDOWS)
		char aAppID[32];
		// The dedicated-server Tool AppID is only used to distribute the server.
		// Authentication tickets and the server browser belong to the base game.
		str_format(aAppID, sizeof(aAppID), "%d", STEAM_APP_ID);
		_putenv_s("SteamAppId", aAppID);
#else
		char aAppID[32];
		str_format(aAppID, sizeof(aAppID), "%d", STEAM_APP_ID);
		setenv("SteamAppId", aAppID, 0);
#endif
		const unsigned short QueryPort = Port == 65535 ? 65534 : (unsigned short)(Port + 1);
		m_Initialized = SteamGameServer_Init(0, Port, QueryPort, eServerModeAuthenticationAndSecure, GAME_VERSION);
		if(m_Initialized)
		{
			m_ValidateAuthCallback.Register(this, &CSteamPlatformGameServer::OnValidateAuthTicket);
			m_ServersConnectedCallback.Register(this, &CSteamPlatformGameServer::OnServersConnected);
			m_ServerConnectFailureCallback.Register(this, &CSteamPlatformGameServer::OnServerConnectFailure);
			m_ServersDisconnectedCallback.Register(this, &CSteamPlatformGameServer::OnServersDisconnected);
			SteamGameServer()->SetProduct("ninslash");
			SteamGameServer()->SetModDir("ninslash");
			SteamGameServer()->SetDedicatedServer(true);
			SteamGameServer()->LogOnAnonymous();
			SteamGameServer()->SetAdvertiseServerActive(false);
		}
		return m_Initialized;
	}
	void Shutdown()
	{
		if(m_Initialized)
		{
			for(int i = 0; i < 64; i++)
				if(m_aAuthSessions[i].m_SteamID)
					SteamGameServer()->EndAuthSession(CSteamID(m_aAuthSessions[i].m_SteamID));
			m_ValidateAuthCallback.Unregister();
			m_ServersConnectedCallback.Unregister();
			m_ServerConnectFailureCallback.Unregister();
			m_ServersDisconnectedCallback.Unregister();
			SteamGameServer_Shutdown();
		}
		mem_zero(m_aAuthSessions, sizeof(m_aAuthSessions));
		m_Initialized = false;
		m_Connected = false;
		m_AdvertiseRequested = false;
	}
	void RunCallbacks()
	{
		if(m_Initialized)
			SteamGameServer_RunCallbacks();
	}
	bool Available() const
	{
		return m_Initialized;
	}
	void SetAdvertiseServerActive(bool Active)
	{
		m_AdvertiseRequested = Active;
		if(m_Initialized && SteamGameServer())
			SteamGameServer()->SetAdvertiseServerActive(Active && m_Connected);
	}
	void UpdateMetadata(const char *pName,
						const char *pMap,
						int Players,
						int MaxPlayers,
						bool PasswordProtected,
						bool Official,
						int AuthPolicy,
						const char *pModHash)
	{
		if(!m_Initialized || !SteamGameServer())
			return;
		SteamGameServer()->SetServerName(pName ? pName : "Ninslash server");
		SteamGameServer()->SetMapName(pMap ? pMap : "");
		SteamGameServer()->SetBotPlayerCount(0);
		SteamGameServer()->SetMaxPlayerCount(MaxPlayers);
		SteamGameServer()->SetPasswordProtected(PasswordProtected);
		char aTags[256];
		str_format(aTags,
				   sizeof(aTags),
				   "official=%d,modded=%d,modhash=%s,auth=%d",
				   Official ? 1 : 0,
				   pModHash && pModHash[0] ? 1 : 0,
				   pModHash && pModHash[0] ? pModHash : "none",
				   PlatformEffectiveAuthPolicy(AuthPolicy, Official, false));
		SteamGameServer()->SetGameTags(aTags);
		(void)Players; // Steam derives player count from authenticated sessions.
	}
	void UpdateUserData(unsigned long long SteamID, const char *pName, int Score)
	{
		if(m_Initialized && m_Connected && SteamID && AuthenticationResult(SteamID) == PLATFORM_AUTH_OK)
			SteamGameServer()->BUpdateUserData(CSteamID(SteamID), pName ? pName : "", Score < 0 ? 0 : (uint32)Score);
	}
	EPlatformAuthResult Authenticate(unsigned long long SteamID, const void *pTicket, int TicketSize)
	{
		if(!m_Initialized || !m_Connected)
			return PLATFORM_AUTH_UNAVAILABLE;
		if(!SteamID || !pTicket || TicketSize <= 0 || TicketSize > 2048)
			return PLATFORM_AUTH_INVALID_TICKET;
		if(FindSession(SteamID) >= 0)
			return PLATFORM_AUTH_REPLAYED_TICKET;
		const int Session = FreeSession();
		if(Session < 0)
			return PLATFORM_AUTH_UNAVAILABLE;
		const EBeginAuthSessionResult Result =
			SteamGameServer()->BeginAuthSession(pTicket, TicketSize, CSteamID(SteamID));
		if(Result == k_EBeginAuthSessionResultOK)
		{
			m_aAuthSessions[Session].m_SteamID = SteamID;
			m_aAuthSessions[Session].m_Result = PLATFORM_AUTH_PENDING;
			return PLATFORM_AUTH_PENDING;
		}
		if(Result == k_EBeginAuthSessionResultDuplicateRequest)
			return PLATFORM_AUTH_REPLAYED_TICKET;
		return PLATFORM_AUTH_INVALID_TICKET;
	}
	EPlatformAuthResult AuthenticationResult(unsigned long long SteamID) const
	{
		const int Session = FindSession(SteamID);
		return Session >= 0 ? m_aAuthSessions[Session].m_Result : PLATFORM_AUTH_INVALID_TICKET;
	}
	void EndAuthentication(unsigned long long SteamID)
	{
		const int Session = FindSession(SteamID);
		if(m_Initialized && SteamID && Session >= 0)
			SteamGameServer()->EndAuthSession(CSteamID(SteamID));
		if(Session >= 0)
		{
			m_aAuthSessions[Session].m_SteamID = 0;
			m_aAuthSessions[Session].m_Result = PLATFORM_AUTH_UNAVAILABLE;
		}
	}
};
#endif
} // namespace

IPlatformGameServer *CreatePlatformGameServer()
{
#if defined(CONF_STEAMWORKS_GAMESERVER)
	return new CSteamPlatformGameServer();
#else
	return new CNullPlatformGameServer();
#endif
}
