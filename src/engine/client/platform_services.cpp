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
	virtual bool ExitRequested() const { return false; }
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
	virtual bool CreateLobby(EPlatformLobbyVisibility Visibility, int MaxMembers) { (void)Visibility; (void)MaxMembers; return false; }
	virtual bool JoinLobby(unsigned long long LobbyID) { (void)LobbyID; return false; }
	virtual void LeaveLobby() {}
	virtual unsigned long long CurrentLobbyID() const { return 0; }
	virtual bool SetLobbyData(const char *pKey, const char *pValue) { (void)pKey; (void)pValue; return false; }
	virtual bool ConsumeLobbyJoin(unsigned long long *pLobbyID) { if(pLobbyID) *pLobbyID = 0; return false; }
	virtual bool OpenLobbyInviteDialog() { return false; }
	virtual bool SubscribeWorkshopItem(unsigned long long PublishedFileID) { (void)PublishedFileID; return false; }
	virtual bool WorkshopDownloadProgress(unsigned long long PublishedFileID, unsigned long long *pDownloaded, unsigned long long *pTotal) const { (void)PublishedFileID; if(pDownloaded) *pDownloaded = 0; if(pTotal) *pTotal = 0; return false; }
	virtual bool UnlockAchievement(const char *pAchievement) { (void)pAchievement; return false; }
	virtual bool SteamInputActive() const { return false; }
};

#if defined(CONF_STEAMWORKS)
class CSteamPlatformServices : public IPlatformServices
{
	bool m_Initialized;
	bool m_ExitRequested;
	bool m_SteamInputInitialized;
	char m_aPendingJoin[256];
	unsigned long long m_CurrentLobbyID;
	unsigned long long m_PendingLobbyJoinID;
	void OnJoinRequested(GameRichPresenceJoinRequested_t *pRequest);
	void OnLobbyCreated(LobbyCreated_t *pResult, bool IOError);
	void OnLobbyEntered(LobbyEnter_t *pResult, bool IOError);
	CCallback<CSteamPlatformServices, GameRichPresenceJoinRequested_t> m_JoinRequestedCallback;
	CCallResult<CSteamPlatformServices, LobbyCreated_t> m_LobbyCreatedCall;
	CCallResult<CSteamPlatformServices, LobbyEnter_t> m_LobbyEnteredCall;

public:
	CSteamPlatformServices() :
		m_Initialized(false), m_ExitRequested(false), m_SteamInputInitialized(false), m_CurrentLobbyID(0), m_PendingLobbyJoinID(0),
		m_JoinRequestedCallback(this, &CSteamPlatformServices::OnJoinRequested)
	{
		m_aPendingJoin[0] = 0;
	}

	virtual bool Init()
	{
		if(m_Initialized)
			return true;
		if(SteamAPI_RestartAppIfNecessary((AppId_t)STEAM_APP_ID))
		{
			m_ExitRequested = true;
			return false;
		}
		m_Initialized = SteamAPI_Init();
		if(!m_Initialized)
		{
			dbg_msg("steam", "SteamAPI_Init failed; Steam features are unavailable");
			return false;
		}
		dbg_msg("steam", "initialized for user %llu", LocalUserID());
		m_SteamInputInitialized = SteamInput() && SteamInput()->Init(false);
		// Recent Steamworks SDKs populate local-user stats automatically; older
		// RequestCurrentStats was removed from the public interface.
		return true;
	}

	virtual void Shutdown()
	{
		if(!m_Initialized)
			return;
		SteamFriends()->ClearRichPresence();
		if(m_CurrentLobbyID && SteamMatchmaking())
			SteamMatchmaking()->LeaveLobby(CSteamID(m_CurrentLobbyID));
		if(m_SteamInputInitialized && SteamInput())
			SteamInput()->Shutdown();
		m_SteamInputInitialized = false;
		SteamAPI_Shutdown();
		m_Initialized = false;
	}

	virtual void RunCallbacks()
	{
		if(m_Initialized)
		{
			SteamAPI_RunCallbacks();
			if(m_SteamInputInitialized && SteamInput())
				SteamInput()->RunFrame();
		}
	}

	virtual bool Available() const { return m_Initialized; }
	virtual bool ExitRequested() const { return m_ExitRequested; }
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

	virtual bool CreateLobby(EPlatformLobbyVisibility Visibility, int MaxMembers)
	{
		if(!m_Initialized || !SteamMatchmaking() || MaxMembers < 1 || MaxMembers > 64)
			return false;
		ELobbyType Type = k_ELobbyTypeFriendsOnly;
		if(Visibility == PLATFORM_LOBBY_INVITE_ONLY)
			Type = k_ELobbyTypePrivate;
		else if(Visibility == PLATFORM_LOBBY_PUBLIC)
			Type = k_ELobbyTypePublic;
		m_LobbyCreatedCall.Set(SteamMatchmaking()->CreateLobby(Type, MaxMembers), this, &CSteamPlatformServices::OnLobbyCreated);
		return true;
	}
	virtual bool JoinLobby(unsigned long long LobbyID)
	{
		if(!m_Initialized || !SteamMatchmaking() || !LobbyID)
			return false;
		m_LobbyEnteredCall.Set(SteamMatchmaking()->JoinLobby(CSteamID(LobbyID)), this, &CSteamPlatformServices::OnLobbyEntered);
		return true;
	}
	virtual void LeaveLobby()
	{
		if(m_Initialized && m_CurrentLobbyID && SteamMatchmaking())
			SteamMatchmaking()->LeaveLobby(CSteamID(m_CurrentLobbyID));
		m_CurrentLobbyID = 0;
	}
	virtual unsigned long long CurrentLobbyID() const { return m_CurrentLobbyID; }
	virtual bool SetLobbyData(const char *pKey, const char *pValue)
	{
		return m_Initialized && m_CurrentLobbyID && pKey && pValue && SteamMatchmaking() && SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), pKey, pValue);
	}
	virtual bool ConsumeLobbyJoin(unsigned long long *pLobbyID)
	{
		if(!pLobbyID || !m_PendingLobbyJoinID)
			return false;
		*pLobbyID = m_PendingLobbyJoinID;
		m_PendingLobbyJoinID = 0;
		return true;
	}
	virtual bool OpenLobbyInviteDialog()
	{
		if(!m_Initialized || !m_CurrentLobbyID || !SteamFriends())
			return false;
		SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(m_CurrentLobbyID));
		return true;
	}
	virtual bool SubscribeWorkshopItem(unsigned long long PublishedFileID)
	{
		return m_Initialized && PublishedFileID && SteamUGC() && SteamUGC()->SubscribeItem((PublishedFileId_t)PublishedFileID) != k_uAPICallInvalid;
	}
	virtual bool WorkshopDownloadProgress(unsigned long long PublishedFileID, unsigned long long *pDownloaded, unsigned long long *pTotal) const
	{
		uint64 Downloaded = 0, Total = 0;
		const bool Result = m_Initialized && PublishedFileID && SteamUGC() && SteamUGC()->GetItemDownloadInfo((PublishedFileId_t)PublishedFileID, &Downloaded, &Total);
		if(pDownloaded) *pDownloaded = Downloaded;
		if(pTotal) *pTotal = Total;
		return Result;
	}
	virtual bool UnlockAchievement(const char *pAchievement)
	{
		return m_Initialized && pAchievement && pAchievement[0] && SteamUserStats() && SteamUserStats()->SetAchievement(pAchievement) && SteamUserStats()->StoreStats();
	}
	virtual bool SteamInputActive() const { return m_SteamInputInitialized; }
};

void CSteamPlatformServices::OnJoinRequested(GameRichPresenceJoinRequested_t *pRequest)
{
	str_copy(m_aPendingJoin, pRequest->m_rgchConnect, sizeof(m_aPendingJoin));
}

void CSteamPlatformServices::OnLobbyCreated(LobbyCreated_t *pResult, bool IOError)
{
	if(IOError || !pResult || pResult->m_eResult != k_EResultOK)
		return;
	m_CurrentLobbyID = pResult->m_ulSteamIDLobby;
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "protocol", "3");
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "room_type", "steam_listen");
}

void CSteamPlatformServices::OnLobbyEntered(LobbyEnter_t *pResult, bool IOError)
{
	if(IOError || !pResult || pResult->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess)
		return;
	m_CurrentLobbyID = pResult->m_ulSteamIDLobby;
	m_PendingLobbyJoinID = m_CurrentLobbyID;
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
