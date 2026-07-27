#include <base/system.h>
#include <base/math.h>
#include <engine/platform_services.h>
#include <engine/client.h>
#include <engine/platform_events.h>
#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/shared/platform_event_queue.h>
#include <engine/shared/platform_server_metadata.h>
#include <engine/shared/mod_collection.h>
#include <engine/shared/mod_package.h>
#include <engine/storage.h>
#include <engine/serverbrowser.h>
#include <game/version.h>

#if defined(CONF_STEAMWORKS)
#include <steam_api.h>
#endif

namespace
{
#if defined(CONF_STEAMWORKS)
class CSteamRelayTransport : public INetPacketTransport
{
	struct CPeer
	{
		HSteamNetConnection m_Connection;
		unsigned long long m_SteamID;
	};
	CPeer m_aPeers[64];
	HSteamListenSocket m_ListenSocket;
	LOCK m_Lock;
	CCallback<CSteamRelayTransport, SteamNetConnectionStatusChangedCallback_t> m_StatusCallback;
	int FindPeer(HSteamNetConnection Connection) const
	{
		for(int i = 0; i < 64; i++) if(m_aPeers[i].m_Connection == Connection) return i;
		return -1;
	}
	int FindPeer(unsigned long long SteamID) const
	{
		for(int i = 0; i < 64; i++) if(m_aPeers[i].m_Connection != k_HSteamNetConnection_Invalid && m_aPeers[i].m_SteamID == SteamID) return i;
		return -1;
	}
	int FreePeer() const
	{
		for(int i = 0; i < 64; i++) if(m_aPeers[i].m_Connection == k_HSteamNetConnection_Invalid) return i;
		return -1;
	}
	unsigned long long AddressSteamID(const NETADDR *pAddr) const
	{
		if(!pAddr || pAddr->type != NETTYPE_STEAM) return 0;
		unsigned long long SteamID = 0;
		for(int i = 0; i < 8; i++) SteamID |= (unsigned long long)pAddr->ip[i] << (i * 8);
		return SteamID;
	}

	void OnStatusChanged(SteamNetConnectionStatusChangedCallback_t *pStatus)
	{
		if(!pStatus || !SteamNetworkingSockets())
			return;
		lock_wait(m_Lock);
		if(m_ListenSocket != k_HSteamListenSocket_Invalid && pStatus->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting && pStatus->m_info.m_hListenSocket == m_ListenSocket)
		{
			const int Peer = FreePeer();
			if(Peer < 0 || SteamNetworkingSockets()->AcceptConnection(pStatus->m_hConn) != k_EResultOK)
			{
				SteamNetworkingSockets()->CloseConnection(pStatus->m_hConn, 0, "relay busy", false);
				lock_unlock(m_Lock);
				return;
			}
			m_aPeers[Peer].m_Connection = pStatus->m_hConn;
			m_aPeers[Peer].m_SteamID = pStatus->m_info.m_identityRemote.GetSteamID64();
		}
		else if(pStatus->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || pStatus->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
		{
			const int Peer = FindPeer(pStatus->m_hConn);
			if(Peer >= 0)
			{
				SteamNetworkingSockets()->CloseConnection(pStatus->m_hConn, 0, 0, false);
				m_aPeers[Peer].m_Connection = k_HSteamNetConnection_Invalid;
				m_aPeers[Peer].m_SteamID = 0;
			}
		}
		lock_unlock(m_Lock);
	}

	void FillAddress(NETADDR *pAddr, unsigned long long SteamID) const
	{
		mem_zero(pAddr, sizeof(*pAddr));
		pAddr->type = NETTYPE_STEAM;
		for(int i = 0; i < 8; i++)
			pAddr->ip[i] = (unsigned char)(SteamID >> (i * 8));
	}

public:
	CSteamRelayTransport() :
		m_ListenSocket(k_HSteamListenSocket_Invalid),
		m_Lock(lock_create()),
		m_StatusCallback(this, &CSteamRelayTransport::OnStatusChanged)
	{
		for(int i = 0; i < 64; i++) { m_aPeers[i].m_Connection = k_HSteamNetConnection_Invalid; m_aPeers[i].m_SteamID = 0; }
	}
	~CSteamRelayTransport()
	{
		ClosePeer();
		lock_wait(m_Lock);
		if(m_ListenSocket != k_HSteamListenSocket_Invalid && SteamNetworkingSockets()) SteamNetworkingSockets()->CloseListenSocket(m_ListenSocket);
		m_ListenSocket = k_HSteamListenSocket_Invalid;
		lock_unlock(m_Lock);
		lock_destroy(m_Lock);
	}

	bool ConnectPeer(unsigned long long PeerID)
	{
		if(!PeerID || !SteamNetworkingSockets())
			return false;
		lock_wait(m_Lock);
		if(FindPeer(PeerID) >= 0) { lock_unlock(m_Lock); return true; }
		const int Peer = FreePeer();
		if(Peer < 0) { lock_unlock(m_Lock); return false; }
		SteamNetworkingIdentity Identity;
		Identity.Clear();
		Identity.SetSteamID64(PeerID);
		m_aPeers[Peer].m_Connection = SteamNetworkingSockets()->ConnectP2P(Identity, 1, 0, 0);
		m_aPeers[Peer].m_SteamID = PeerID;
		const bool Result = m_aPeers[Peer].m_Connection != k_HSteamNetConnection_Invalid;
		lock_unlock(m_Lock);
		return Result;
	}
	bool Listen(int VirtualPort)
	{
		if(!SteamNetworkingSockets() || VirtualPort < 0)
			return false;
		lock_wait(m_Lock);
		if(m_ListenSocket != k_HSteamListenSocket_Invalid)
			SteamNetworkingSockets()->CloseListenSocket(m_ListenSocket);
		m_ListenSocket = SteamNetworkingSockets()->CreateListenSocketP2P(VirtualPort, 0, 0);
		const bool Result = m_ListenSocket != k_HSteamListenSocket_Invalid;
		lock_unlock(m_Lock);
		return Result;
	}
	void CloseListen()
	{
		lock_wait(m_Lock);
		if(m_ListenSocket != k_HSteamListenSocket_Invalid && SteamNetworkingSockets())
			SteamNetworkingSockets()->CloseListenSocket(m_ListenSocket);
		m_ListenSocket = k_HSteamListenSocket_Invalid;
		lock_unlock(m_Lock);
	}
	void ClosePeer()
	{
		lock_wait(m_Lock);
		for(int i = 0; i < 64; i++)
			if(m_aPeers[i].m_Connection != k_HSteamNetConnection_Invalid && SteamNetworkingSockets())
			{
				SteamNetworkingSockets()->CloseConnection(m_aPeers[i].m_Connection, 0, "closed", false);
				m_aPeers[i].m_Connection = k_HSteamNetConnection_Invalid;
				m_aPeers[i].m_SteamID = 0;
			}
		lock_unlock(m_Lock);
	}
	void Update() {}
	int RecvPacket(NETADDR *pAddr, void *pBuffer, int BufferSize)
	{
		if(!SteamNetworkingSockets() || !pBuffer || BufferSize <= 0)
			return 0;
		lock_wait(m_Lock);
		SteamNetworkingMessage_t *pMessage = 0;
		int Peer = -1;
		for(int i = 0; i < 64; i++)
			if(m_aPeers[i].m_Connection != k_HSteamNetConnection_Invalid && SteamNetworkingSockets()->ReceiveMessagesOnConnection(m_aPeers[i].m_Connection, &pMessage, 1) > 0) { Peer = i; break; }
		if(Peer < 0 || !pMessage) { lock_unlock(m_Lock); return 0; }
		const int Size = pMessage->m_cbSize <= BufferSize ? pMessage->m_cbSize : -1;
		if(Size > 0)
		{
			mem_copy(pBuffer, pMessage->m_pData, Size);
			FillAddress(pAddr, m_aPeers[Peer].m_SteamID);
		}
		pMessage->Release();
		lock_unlock(m_Lock);
		return Size;
	}
	bool SendPacket(const NETADDR *pAddr, CNetPacketConstruct *pPacket)
	{
		unsigned char aBuffer[NET_MAX_PACKETSIZE];
		const int Size = CNetBase::PackPacket(pPacket, aBuffer, sizeof(aBuffer));
		lock_wait(m_Lock);
		const int Peer = FindPeer(AddressSteamID(pAddr));
		const bool Result = Size > 0 && Peer >= 0 && SteamNetworkingSockets() && SteamNetworkingSockets()->SendMessageToConnection(m_aPeers[Peer].m_Connection, aBuffer, Size, k_nSteamNetworkingSend_UnreliableNoNagle, 0) == k_EResultOK;
		lock_unlock(m_Lock);
		return Result;
	}
	bool SendControl(const NETADDR *pAddr, int Ack, int ControlMsg, const void *pExtra, int ExtraSize)
	{
		unsigned char aBuffer[NET_MAX_PACKETSIZE];
		const int Size = CNetBase::PackControl(Ack, ControlMsg, pExtra, ExtraSize, aBuffer, sizeof(aBuffer));
		lock_wait(m_Lock);
		const int Peer = FindPeer(AddressSteamID(pAddr));
		const bool Result = Size > 0 && Peer >= 0 && SteamNetworkingSockets() && SteamNetworkingSockets()->SendMessageToConnection(m_aPeers[Peer].m_Connection, aBuffer, Size, k_nSteamNetworkingSend_UnreliableNoNagle, 0) == k_EResultOK;
		lock_unlock(m_Lock);
		return Result;
	}
};
#endif

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
	virtual void CancelAuthSessionTicket() {}
	virtual void SetRichPresence(const char *pStatus, const char *pConnect) { (void)pStatus; (void)pConnect; }
	virtual bool ConsumeJoinRequest(char *pBuffer, int BufferSize)
	{
		if(BufferSize > 0)
			pBuffer[0] = 0;
		return false;
	}
	virtual bool ConsumeJoinFailure(char *pBuffer, int BufferSize) { if(pBuffer && BufferSize > 0) pBuffer[0] = 0; return false; }
	virtual bool CreateLobby(EPlatformLobbyVisibility Visibility, int MaxMembers) { (void)Visibility; (void)MaxMembers; return false; }
	virtual bool JoinLobby(unsigned long long LobbyID) { (void)LobbyID; return false; }
	virtual void LeaveLobby() {}
	virtual unsigned long long CurrentLobbyID() const { return 0; }
	virtual bool SetLobbyData(const char *pKey, const char *pValue) { (void)pKey; (void)pValue; return false; }
	virtual bool ConsumeLobbyJoin(unsigned long long *pLobbyID) { if(pLobbyID) *pLobbyID = 0; return false; }
	virtual bool ConsumeListenServerStopRequest() { return false; }
	virtual bool OpenLobbyInviteDialog() { return false; }
	virtual bool RefreshLobbyList() { return false; }
	virtual bool RefreshDedicatedServerList() { return false; }
	virtual int LobbyCount() const { return 0; }
	virtual bool LobbyInfo(int Index, CPlatformLobbyInfo *pInfo) const { (void)Index; if(pInfo) mem_zero(pInfo,sizeof(*pInfo)); return false; }
	virtual void LobbyOperationStatus(CPlatformOperationStatus *pStatus) const { if(pStatus) mem_zero(pStatus,sizeof(*pStatus)); }
	virtual bool SubscribeWorkshopItem(unsigned long long PublishedFileID) { (void)PublishedFileID; return false; }
	virtual bool UnsubscribeWorkshopItem(unsigned long long PublishedFileID) { (void)PublishedFileID; return false; }
	virtual bool OpenWorkshopItemPage(unsigned long long PublishedFileID) { (void)PublishedFileID; return false; }
	virtual bool OpenWorkshopBrowsePage() { return false; }
	virtual bool WorkshopDownloadProgress(unsigned long long PublishedFileID, unsigned long long *pDownloaded, unsigned long long *pTotal) const { (void)PublishedFileID; if(pDownloaded) *pDownloaded = 0; if(pTotal) *pTotal = 0; return false; }
	virtual int RefreshWorkshopItems() { return 0; }
	virtual int WorkshopItemCount() const { return 0; }
	virtual bool WorkshopItem(int Index, CPlatformWorkshopItem *pItem) const { (void)Index; if(pItem) mem_zero(pItem,sizeof(*pItem)); return false; }
	virtual bool SetWorkshopItemDisabled(unsigned long long PublishedFileID, bool Disabled) { (void)PublishedFileID; (void)Disabled; return false; }
	virtual void WorkshopOperationStatus(CPlatformOperationStatus *pStatus) const { if(pStatus) mem_zero(pStatus,sizeof(*pStatus)); }
	virtual bool CreateWorkshopItem() { return false; }
	virtual bool UpdateWorkshopItem(unsigned long long PublishedFileID, const char *pContentRoot, const char *pPreviewFile) { (void)PublishedFileID;(void)pContentRoot;(void)pPreviewFile;return false; }
	virtual void WorkshopPublishStatus(CPlatformWorkshopPublishStatus *pStatus) const { if(pStatus)mem_zero(pStatus,sizeof(*pStatus)); }
	virtual bool UnlockAchievement(const char *pAchievement) { (void)pAchievement; return false; }
	virtual void ProcessServerEvent(int Event, int Value, bool LeaderboardEligible) { (void)Event; (void)Value; (void)LeaderboardEligible; }
	virtual bool SteamInputActive() const { return false; }
	virtual void SetInputActionSet(EPlatformInputActionSet ActionSet) { (void)ActionSet; }
	virtual bool ReadInputState(CPlatformInputState *pState) { if(pState) mem_zero(pState, sizeof(*pState)); return false; }
	virtual INetPacketTransport *RelayTransport() { return 0; }
	virtual INetPacketTransport *RelayListenTransport() { return 0; }
};

#if defined(CONF_STEAMWORKS)
class CSteamPlatformServices : public IPlatformServices, public ISteamMatchmakingServerListResponse
{
	bool m_Initialized;
	bool m_ExitRequested;
	bool m_SteamInputInitialized;
	char m_aPendingJoin[256];
	char m_aJoinFailure[256];
	unsigned long long m_CurrentLobbyID;
	unsigned long long m_PendingLobbyJoinID;
	unsigned long long m_HostedLobbyID;
	bool m_ListenServerStopRequested;
	bool m_LobbyCreatePending;
	bool m_LobbyRefreshPending;
	bool m_LobbyJoinPending;
	IStorage *m_pStorage;
	CPlatformEventQueue m_EventQueue;
	int m_ActiveLeaderboardEvent;
	int m_ActiveLeaderboardValue;
	int64 m_NextEventRetry;
	CPlatformWorkshopItem m_aWorkshopItems[256];
	int m_WorkshopItemCount;
	CPlatformLobbyInfo m_aLobbies[128];
	int m_LobbyCount;
	CPlatformWorkshopPublishStatus m_WorkshopPublish;
	UGCUpdateHandle_t m_WorkshopUpdateHandle;
	HServerListRequest m_DedicatedServerRequest;
	CModCollection m_ModCollection;
	EPlatformInputActionSet m_InputActionSet;
	InputActionSetHandle_t m_aInputActionSets[4];
	InputDigitalActionHandle_t m_aDigitalActions[NUM_PLATFORM_INPUT_ACTIONS];
	InputAnalogActionHandle_t m_MoveAction;
	InputAnalogActionHandle_t m_AimAction;
	CSteamRelayTransport m_RelayTransport;
	CSteamRelayTransport m_RelayListenTransport;
	unsigned char m_aAuthTicket[2048];
	int m_AuthTicketSize;
	HAuthTicket m_AuthTicketHandle;
	int m_AuthTicketState; // 0=idle, 1=pending, 2=ready, 3=failed
	void OnAuthTicketResponse(GetAuthSessionTicketResponse_t *pResponse)
	{
		if(!pResponse || pResponse->m_hAuthTicket != m_AuthTicketHandle)
			return;
		m_AuthTicketState = pResponse->m_eResult == k_EResultOK ? 2 : 3;
		if(m_AuthTicketState == 3)
			dbg_msg("steam", "authentication ticket registration failed: result=%d", (int)pResponse->m_eResult);
	}
	void OnJoinRequested(GameRichPresenceJoinRequested_t *pRequest);
	void OnLobbyJoinRequested(GameLobbyJoinRequested_t *pRequest);
	void OnLobbyMembersChanged(LobbyChatUpdate_t *pUpdate);
	void OnLobbyCreated(LobbyCreated_t *pResult, bool IOError);
	void OnLobbyEntered(LobbyEnter_t *pResult, bool IOError);
	void OnLobbyList(LobbyMatchList_t *pResult, bool IOError);
	void OnWorkshopDownloaded(DownloadItemResult_t *pResult) { if(pResult && pResult->m_unAppID == STEAM_APP_ID) RefreshWorkshopItems(); }
	void OnWorkshopCreated(CreateItemResult_t *pResult, bool IOError);
	void OnWorkshopSubmitted(SubmitItemUpdateResult_t *pResult, bool IOError);
	void ServerResponded(HServerListRequest Request, int ServerIndex)
	{
		if(Request != m_DedicatedServerRequest || !SteamMatchmakingServers()) return;
		gameserveritem_t *pServer = SteamMatchmakingServers()->GetServerDetails(Request, ServerIndex);
		if(!pServer || !pServer->m_bHadSuccessfulResponse || pServer->m_nAppID != STEAM_APP_ID || str_comp(pServer->m_szGameDir, "ninslash") != 0) return;
		NETADDR Address; if(net_addr_from_str(&Address, pServer->m_NetAdr.GetConnectionAddressString()) != 0) return;
		CPlatformServerMetadata Metadata; if(!PlatformServerMetadataParse(pServer->m_szGameTags, &Metadata)) return;
		IServerBrowser *pBrowser = Kernel()->RequestInterface<IServerBrowser>();
		if(pBrowser) pBrowser->AddDiscoveredServer(Address, IServerBrowser::DISCOVERY_STEAM, Metadata.m_Official, Metadata.m_Modded, Metadata.m_AuthPolicy, pServer->m_steamID.IsValid() ? pServer->m_steamID.ConvertToUint64() : 0);
	}
	void ServerFailedToRespond(HServerListRequest Request, int ServerIndex) { (void)Request; (void)ServerIndex; }
	void RefreshComplete(HServerListRequest Request, EMatchMakingServerResponse Response)
	{
		(void)Response;
		if(Request == m_DedicatedServerRequest && SteamMatchmakingServers()) SteamMatchmakingServers()->ReleaseRequest(Request);
		if(Request == m_DedicatedServerRequest) m_DedicatedServerRequest = 0;
	}
	void SetJoinFailure(const char *pReason)
	{
		str_copy(m_aJoinFailure, pReason ? pReason : "Unable to join Steam Lobby", sizeof(m_aJoinFailure));
		m_aPendingJoin[0] = 0;
	}
	bool InitSteamInput()
	{
		if(!SteamInput() || !SteamInput()->Init(false))
			return false;
		char aExecutable[1024];
		char aManifest[1200];
		bool ManifestFound = false;
		if(fs_executable_path(aExecutable, sizeof(aExecutable)) == 0)
		{
			char *pSlash = strrchr(aExecutable, '/');
#if defined(CONF_FAMILY_WINDOWS)
			char *pBackslash = strrchr(aExecutable, '\\');
			if(!pSlash || (pBackslash && pBackslash > pSlash)) pSlash = pBackslash;
#endif
			if(pSlash) *pSlash = 0;
			str_format(aManifest, sizeof(aManifest), "%s/data/steam_input_manifest.vdf", aExecutable);
			IOHANDLE File = io_open(aManifest, IOFLAG_READ);
			ManifestFound = File != 0;
			if(File) io_close(File);
		}
		if(!ManifestFound)
		{
			char aCurrent[1024];
			if(fs_getcwd(aCurrent, sizeof(aCurrent)))
			{
				str_format(aManifest, sizeof(aManifest), "%s/data/steam_input_manifest.vdf", aCurrent);
				IOHANDLE File = io_open(aManifest, IOFLAG_READ);
				ManifestFound = File != 0;
				if(File) io_close(File);
			}
		}
		if(!ManifestFound || !SteamInput()->SetInputActionManifestFilePath(aManifest))
		{
			SteamInput()->Shutdown();
			dbg_msg("steam", "Steam Input manifest unavailable; using SDL gamepad fallback");
			return false;
		}
		static const char *s_apSets[] = {"game", "menu", "spectator", "chat"};
		static const char *s_apActions[] = {"confirm", "cancel", "fire", "alt_fire", "scoreboard", "build", "drop", "emote", "picker", "last_weapon", "prev_weapon", "next_weapon", "up", "down", "left", "right"};
		for(int i = 0; i < 4; i++) m_aInputActionSets[i] = SteamInput()->GetActionSetHandle(s_apSets[i]);
		for(int i = 0; i < NUM_PLATFORM_INPUT_ACTIONS; i++) m_aDigitalActions[i] = SteamInput()->GetDigitalActionHandle(s_apActions[i]);
		m_MoveAction = SteamInput()->GetAnalogActionHandle("move");
		m_AimAction = SteamInput()->GetAnalogActionHandle("aim");
		return true;
	}
	CCallback<CSteamPlatformServices, GameRichPresenceJoinRequested_t> m_JoinRequestedCallback;
	CCallback<CSteamPlatformServices, GetAuthSessionTicketResponse_t> m_AuthTicketCallback;
	CCallback<CSteamPlatformServices, GameLobbyJoinRequested_t> m_LobbyJoinRequestedCallback;
	CCallback<CSteamPlatformServices, LobbyChatUpdate_t> m_LobbyMembersChangedCallback;
	CCallback<CSteamPlatformServices, DownloadItemResult_t> m_WorkshopDownloadedCallback;
	CCallResult<CSteamPlatformServices, LobbyCreated_t> m_LobbyCreatedCall;
	CCallResult<CSteamPlatformServices, LobbyEnter_t> m_LobbyEnteredCall;
	CCallResult<CSteamPlatformServices, LobbyMatchList_t> m_LobbyListCall;
	CCallResult<CSteamPlatformServices, CreateItemResult_t> m_WorkshopCreatedCall;
	CCallResult<CSteamPlatformServices, SubmitItemUpdateResult_t> m_WorkshopSubmittedCall;
	CCallResult<CSteamPlatformServices, LeaderboardFindResult_t> m_LeaderboardFoundCall;
	CCallResult<CSteamPlatformServices, LeaderboardScoreUploaded_t> m_LeaderboardUploadedCall;
	void SaveEventQueue()
	{
		if(!m_pStorage) return;
		char aData[4096];
		const int Size = m_EventQueue.WriteText(aData, sizeof(aData));
		if(Size < 0) return;
		IOHANDLE File = m_pStorage->OpenFile("steam_pending_events.dat", IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(File) { io_write(File, aData, Size); io_close(File); }
	}
	void LoadEventQueue()
	{
		if(!m_pStorage) return;
		IOHANDLE File = m_pStorage->OpenFile("steam_pending_events.dat", IOFLAG_READ, IStorage::TYPE_SAVE);
		if(!File) return;
		const long Size = io_length(File);
		if(Size >= 0 && Size < 4096)
		{
			char aData[4096];
			const unsigned Read = io_read(File, aData, (unsigned)Size);
			aData[Read] = 0;
			if(Read != (unsigned)Size || !m_EventQueue.ReadText(aData)) m_EventQueue.Clear();
		}
		io_close(File);
	}
	void PumpEventQueue();
	void OnLeaderboardFound(LeaderboardFindResult_t *pResult, bool IOError);
	void OnLeaderboardUploaded(LeaderboardScoreUploaded_t *pResult, bool IOError);

public:
	CSteamPlatformServices() :
		m_Initialized(false), m_ExitRequested(false), m_SteamInputInitialized(false), m_CurrentLobbyID(0), m_PendingLobbyJoinID(0), m_HostedLobbyID(0), m_ListenServerStopRequested(false), m_LobbyCreatePending(false), m_LobbyRefreshPending(false), m_LobbyJoinPending(false), m_pStorage(0), m_ActiveLeaderboardEvent(-1), m_ActiveLeaderboardValue(0), m_NextEventRetry(0), m_WorkshopItemCount(0), m_LobbyCount(0), m_WorkshopUpdateHandle(k_UGCUpdateHandleInvalid), m_DedicatedServerRequest(0), m_InputActionSet(PLATFORM_INPUT_MENU), m_MoveAction(0), m_AimAction(0), m_AuthTicketSize(0), m_AuthTicketHandle(k_HAuthTicketInvalid), m_AuthTicketState(0),
		m_JoinRequestedCallback(this, &CSteamPlatformServices::OnJoinRequested),
		m_AuthTicketCallback(this, &CSteamPlatformServices::OnAuthTicketResponse),
		m_LobbyJoinRequestedCallback(this, &CSteamPlatformServices::OnLobbyJoinRequested),
		m_LobbyMembersChangedCallback(this, &CSteamPlatformServices::OnLobbyMembersChanged),
		m_WorkshopDownloadedCallback(this, &CSteamPlatformServices::OnWorkshopDownloaded)
	{
		m_aPendingJoin[0] = 0;
		m_aJoinFailure[0] = 0;
		mem_zero(m_aInputActionSets, sizeof(m_aInputActionSets));
		mem_zero(m_aDigitalActions, sizeof(m_aDigitalActions));
		mem_zero(&m_WorkshopPublish, sizeof(m_WorkshopPublish));
	}

	virtual bool Init()
	{
		m_pStorage = Kernel()->RequestInterface<IStorage>();
		LoadEventQueue();
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
		m_SteamInputInitialized = InitSteamInput();
		RefreshWorkshopItems();
		// Recent Steamworks SDKs populate local-user stats automatically; older
		// RequestCurrentStats was removed from the public interface.
		return true;
	}

	virtual void Shutdown()
	{
		if(!m_Initialized)
			return;
		CancelAuthSessionTicket();
		m_LobbyCreatedCall.Cancel();
		m_LobbyEnteredCall.Cancel();
		m_LobbyListCall.Cancel();
		m_WorkshopCreatedCall.Cancel();
		m_WorkshopSubmittedCall.Cancel();
		if(m_DedicatedServerRequest && SteamMatchmakingServers())
		{
			SteamMatchmakingServers()->CancelQuery(m_DedicatedServerRequest);
			SteamMatchmakingServers()->ReleaseRequest(m_DedicatedServerRequest);
			m_DedicatedServerRequest = 0;
		}
		m_WorkshopPublish.m_Active = false;
		m_WorkshopUpdateHandle = k_UGCUpdateHandleInvalid;
		m_LeaderboardFoundCall.Cancel();
		m_LeaderboardUploadedCall.Cancel();
		m_ActiveLeaderboardEvent = -1;
		m_LobbyCreatePending = false;
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
			PumpEventQueue();
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
		if(m_AuthTicketState == 0)
		{
			uint32 TicketSize = 0;
			// A null remote identity creates a ticket for a dedicated GameServer.
			m_AuthTicketHandle = SteamUser()->GetAuthSessionTicket(m_aAuthTicket, sizeof(m_aAuthTicket), &TicketSize, 0);
			if(m_AuthTicketHandle == k_HAuthTicketInvalid || !TicketSize || TicketSize > sizeof(m_aAuthTicket))
			{
				m_AuthTicketState = 3;
				return 0;
			}
			m_AuthTicketSize = (int)TicketSize;
			m_AuthTicketState = 1;
			return -1;
		}
		if(m_AuthTicketState == 1)
			return -1;
		if(m_AuthTicketState != 2 || BufferSize < m_AuthTicketSize)
			return 0;
		mem_copy(pBuffer, m_aAuthTicket, m_AuthTicketSize);
		return m_AuthTicketSize;
	}

	virtual void CancelAuthSessionTicket()
	{
		if(m_Initialized && SteamUser() && m_AuthTicketHandle != k_HAuthTicketInvalid)
			SteamUser()->CancelAuthTicket(m_AuthTicketHandle);
		mem_zero(m_aAuthTicket, sizeof(m_aAuthTicket));
		m_AuthTicketSize = 0;
		m_AuthTicketHandle = k_HAuthTicketInvalid;
		m_AuthTicketState = 0;
	}

	virtual void SetRichPresence(const char *pStatus, const char *pConnect)
	{
		if(!m_Initialized || !SteamFriends())
			return;
		SteamFriends()->SetRichPresence("status", pStatus ? pStatus : "");
		SteamFriends()->SetRichPresence("connect", pConnect ? pConnect : "");
		SteamFriends()->SetRichPresence("steam_display", "#Status");
		if(m_CurrentLobbyID && SteamMatchmaking())
		{
			char aLobbyID[32];
			char aMembers[16];
			str_format(aLobbyID, sizeof(aLobbyID), "%llu", m_CurrentLobbyID);
			str_format(aMembers, sizeof(aMembers), "%d", SteamMatchmaking()->GetNumLobbyMembers(CSteamID(m_CurrentLobbyID)));
			SteamFriends()->SetRichPresence("steam_player_group", aLobbyID);
			SteamFriends()->SetRichPresence("steam_player_group_size", aMembers);
		}
		else
		{
			SteamFriends()->SetRichPresence("steam_player_group", "");
			SteamFriends()->SetRichPresence("steam_player_group_size", "");
		}
	}

	virtual bool ConsumeJoinRequest(char *pBuffer, int BufferSize)
	{
		if(!m_aPendingJoin[0] || BufferSize <= 0)
			return false;
		str_copy(pBuffer, m_aPendingJoin, BufferSize);
		m_aPendingJoin[0] = 0;
		return true;
	}
	virtual bool ConsumeJoinFailure(char *pBuffer, int BufferSize)
	{
		if(!m_aJoinFailure[0] || !pBuffer || BufferSize <= 0)
			return false;
		str_copy(pBuffer, m_aJoinFailure, BufferSize);
		m_aJoinFailure[0] = 0;
		return true;
	}

	virtual bool CreateLobby(EPlatformLobbyVisibility Visibility, int MaxMembers)
	{
		if(!m_Initialized || !SteamMatchmaking() || m_CurrentLobbyID || m_LobbyCreatePending || MaxMembers < 1 || MaxMembers > 64)
			return false;
		ELobbyType Type = k_ELobbyTypeFriendsOnly;
		if(Visibility == PLATFORM_LOBBY_INVITE_ONLY)
			Type = k_ELobbyTypePrivate;
		else if(Visibility == PLATFORM_LOBBY_PUBLIC)
			Type = k_ELobbyTypePublic;
		m_ListenServerStopRequested = false;
		m_LobbyCreatePending = true;
		m_LobbyCreatedCall.Set(SteamMatchmaking()->CreateLobby(Type, MaxMembers), this, &CSteamPlatformServices::OnLobbyCreated);
		return true;
	}
	virtual bool JoinLobby(unsigned long long LobbyID)
	{
		if(!m_Initialized || !SteamMatchmaking() || !LobbyID)
			return false;
		m_LobbyJoinPending = true;
		m_LobbyEnteredCall.Set(SteamMatchmaking()->JoinLobby(CSteamID(LobbyID)), this, &CSteamPlatformServices::OnLobbyEntered);
		return true;
	}
	virtual void LeaveLobby()
	{
		m_LobbyCreatedCall.Cancel();
		m_LobbyEnteredCall.Cancel();
		m_LobbyCreatePending = false;
		m_LobbyJoinPending = false;
		if(m_Initialized && m_CurrentLobbyID && SteamMatchmaking())
			SteamMatchmaking()->LeaveLobby(CSteamID(m_CurrentLobbyID));
		m_CurrentLobbyID = 0;
		m_PendingLobbyJoinID = 0;
		m_HostedLobbyID = 0;
		m_aPendingJoin[0] = 0;
		m_aJoinFailure[0] = 0;
		if(SteamFriends())
		{
			SteamFriends()->SetRichPresence("connect", "");
			SteamFriends()->SetRichPresence("steam_player_group", "");
			SteamFriends()->SetRichPresence("steam_player_group_size", "");
		}
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
	virtual bool ConsumeListenServerStopRequest()
	{
		const bool Result = m_ListenServerStopRequested;
		m_ListenServerStopRequested = false;
		return Result;
	}
	virtual bool OpenLobbyInviteDialog()
	{
		if(!m_Initialized || !m_CurrentLobbyID || !SteamFriends())
			return false;
		SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(m_CurrentLobbyID));
		return true;
	}
	virtual bool RefreshLobbyList()
	{
		if(!m_Initialized || !SteamMatchmaking()) return false;
		m_LobbyCount = 0;
		m_LobbyRefreshPending = true;
		SteamMatchmaking()->AddRequestLobbyListStringFilter("protocol", GAME_NETVERSION, k_ELobbyComparisonEqual);
		SteamMatchmaking()->AddRequestLobbyListStringFilter("room_type", "steam_listen", k_ELobbyComparisonEqual);
		SteamMatchmaking()->AddRequestLobbyListResultCountFilter(128);
		m_LobbyListCall.Set(SteamMatchmaking()->RequestLobbyList(), this, &CSteamPlatformServices::OnLobbyList);
		return true;
	}
	virtual bool RefreshDedicatedServerList()
	{
		if(!m_Initialized || !SteamMatchmakingServers()) return false;
		if(m_DedicatedServerRequest)
		{
			SteamMatchmakingServers()->CancelQuery(m_DedicatedServerRequest);
			SteamMatchmakingServers()->ReleaseRequest(m_DedicatedServerRequest);
			m_DedicatedServerRequest = 0;
		}
		m_DedicatedServerRequest = SteamMatchmakingServers()->RequestInternetServerList((AppId_t)STEAM_APP_ID, 0, 0, this);
		return m_DedicatedServerRequest != 0;
	}
	virtual int LobbyCount() const { return m_LobbyCount; }
	virtual bool LobbyInfo(int Index, CPlatformLobbyInfo *pInfo) const
	{
		if(!pInfo || Index < 0 || Index >= m_LobbyCount) return false;
		*pInfo = m_aLobbies[Index]; return true;
	}
	virtual void LobbyOperationStatus(CPlatformOperationStatus *pStatus) const
	{
		if(!pStatus) return;
		mem_zero(pStatus, sizeof(*pStatus));
		if(m_LobbyCreatePending || m_LobbyRefreshPending || m_LobbyJoinPending)
		{
			pStatus->m_State = CLIENT_ASYNC_WORKING;
			pStatus->m_Stage = m_LobbyCreatePending ? CLIENT_STAGE_CREATING_ROOM : m_LobbyJoinPending ? CLIENT_STAGE_JOINING_ROOM : CLIENT_STAGE_REFRESHING_ROOMS;
		}
		else if(m_CurrentLobbyID || m_LobbyCount)
		{
			pStatus->m_State = CLIENT_ASYNC_SUCCEEDED;
			pStatus->m_Progress = 1.0f;
		}
	}
	virtual bool SubscribeWorkshopItem(unsigned long long PublishedFileID)
	{
		return m_Initialized && PublishedFileID && SteamUGC() && SteamUGC()->SubscribeItem((PublishedFileId_t)PublishedFileID) != k_uAPICallInvalid;
	}
	virtual bool UnsubscribeWorkshopItem(unsigned long long PublishedFileID)
	{
		return m_Initialized && PublishedFileID && SteamUGC() && SteamUGC()->UnsubscribeItem((PublishedFileId_t)PublishedFileID) != k_uAPICallInvalid;
	}
	virtual bool OpenWorkshopItemPage(unsigned long long PublishedFileID)
	{
		if(!m_Initialized || !PublishedFileID || !SteamFriends()) return false;
		char aUrl[160]; str_format(aUrl, sizeof(aUrl), "https://steamcommunity.com/sharedfiles/filedetails/?id=%llu", PublishedFileID);
		SteamFriends()->ActivateGameOverlayToWebPage(aUrl, k_EActivateGameOverlayToWebPageMode_Default); return true;
	}
	virtual bool OpenWorkshopBrowsePage()
	{
		if(!m_Initialized || !SteamFriends()) return false;
		char aUrl[160]; str_format(aUrl, sizeof(aUrl), "https://steamcommunity.com/app/%d/workshop/", STEAM_APP_ID);
		SteamFriends()->ActivateGameOverlayToWebPage(aUrl, k_EActivateGameOverlayToWebPageMode_Default); return true;
	}
	virtual bool WorkshopDownloadProgress(unsigned long long PublishedFileID, unsigned long long *pDownloaded, unsigned long long *pTotal) const
	{
		uint64 Downloaded = 0, Total = 0;
		const bool Result = m_Initialized && PublishedFileID && SteamUGC() && SteamUGC()->GetItemDownloadInfo((PublishedFileId_t)PublishedFileID, &Downloaded, &Total);
		if(pDownloaded) *pDownloaded = Downloaded;
		if(pTotal) *pTotal = Total;
		return Result;
	}
	virtual int RefreshWorkshopItems()
	{
		m_WorkshopItemCount = 0; m_ModCollection.Clear();
		if(!m_Initialized || !SteamUGC()) { g_Config.m_ClModHash[0]=0; return 0; }
		char aWorkshopRoot[1024]; aWorkshopRoot[0] = 0;
		if(m_pStorage)
		{
			m_pStorage->CreateFolder("workshop", IStorage::TYPE_SAVE);
			m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, "workshop", aWorkshopRoot, sizeof(aWorkshopRoot));
		}
		PublishedFileId_t aIDs[256];
		const uint32 Count = SteamUGC()->GetSubscribedItems(aIDs, 256);
		for(uint32 i=0;i<Count&&m_WorkshopItemCount<256;i++)
		{
			CPlatformWorkshopItem &Item=m_aWorkshopItems[m_WorkshopItemCount++];mem_zero(&Item,sizeof(Item));Item.m_PublishedFileID=aIDs[i];Item.m_State=SteamUGC()->GetItemState(aIDs[i]);
			uint64 Downloaded=0,Total=0;SteamUGC()->GetItemDownloadInfo(aIDs[i],&Downloaded,&Total);Item.m_Downloaded=Downloaded;Item.m_Total=Total;
			if((Item.m_State & k_EItemStateNeedsUpdate) || !(Item.m_State & k_EItemStateInstalled)) { SteamUGC()->DownloadItem(aIDs[i],false); str_copy(Item.m_aError,"download or update required",sizeof(Item.m_aError)); continue; }
			uint64 Size=0;uint32 Timestamp=0;char aSteamInstallPath[1024];if(!SteamUGC()->GetItemInstallInfo(aIDs[i],&Size,aSteamInstallPath,sizeof(aSteamInstallPath),&Timestamp)){str_copy(Item.m_aError,"install directory unavailable",sizeof(Item.m_aError));continue;}
			char aID[32];str_format(aID,sizeof(aID),"%llu",(unsigned long long)aIDs[i]);CModManifest Manifest;
			if(!aWorkshopRoot[0] || !ModPackageStage(aSteamInstallPath,aWorkshopRoot,aID,GAME_NETVERSION,&Manifest,Item.m_aInstallPath,sizeof(Item.m_aInstallPath),Item.m_aError,sizeof(Item.m_aError)))continue;
			Item.m_Valid=true;str_copy(Item.m_aName,Manifest.m_aName,sizeof(Item.m_aName));str_copy(Item.m_aVersion,Manifest.m_aVersion,sizeof(Item.m_aVersion));
			if(!m_ModCollection.AddManifest(Manifest,Item.m_aInstallPath,Item.m_aError,sizeof(Item.m_aError)))Item.m_Valid=false;
		}
		const char *apRoots[64];char aaRoots[64][32];int RootCount=0;const char *p=g_Config.m_ClModIds;
		while(*p&&RootCount<64){int N=0;while(p[N]&&p[N]!=','&&N<31){aaRoots[RootCount][N]=p[N];N++;}aaRoots[RootCount][N]=0;if(N){apRoots[RootCount]=aaRoots[RootCount];RootCount++;}p+=N;if(*p==',')p++;else if(*p)break;}
		if(!RootCount) g_Config.m_ClModHash[0]=0;
		else {int aOrder[64],OrderCount=0;char aError[256];if(!m_ModCollection.Resolve(apRoots,RootCount,aOrder,&OrderCount,g_Config.m_ClModHash,aError,sizeof(aError)))g_Config.m_ClModHash[0]=0;}
		return m_WorkshopItemCount;
	}
	virtual int WorkshopItemCount() const { return m_WorkshopItemCount; }
	virtual bool WorkshopItem(int Index,CPlatformWorkshopItem *pItem) const { if(!pItem||Index<0||Index>=m_WorkshopItemCount)return false;*pItem=m_aWorkshopItems[Index];return true; }
	virtual bool SetWorkshopItemDisabled(unsigned long long ID,bool Disabled)
	{
		if(!m_Initialized||!SteamUGC()||!ID)return false;
		PublishedFileId_t FileID=(PublishedFileId_t)ID;
		const bool Result=SteamUGC()->SetItemsDisabledLocally(&FileID,1,Disabled);
		if(Result)RefreshWorkshopItems();
		return Result;
	}
	virtual void WorkshopOperationStatus(CPlatformOperationStatus *pStatus) const
	{
		if(!pStatus) return;
		mem_zero(pStatus, sizeof(*pStatus));
		unsigned long long Downloaded = 0, Total = 0;
		bool Working = false, Failed = false;
		for(int i = 0; i < m_WorkshopItemCount; i++)
		{
			const CPlatformWorkshopItem &Item = m_aWorkshopItems[i];
			Downloaded += Item.m_Downloaded;
			Total += Item.m_Total;
			Working = Working || (Item.m_Total > 0 && Item.m_Downloaded < Item.m_Total);
			Failed = Failed || (!Item.m_Valid && Item.m_aError[0] && !Working);
		}
		pStatus->m_State = Working ? CLIENT_ASYNC_WORKING : Failed ? CLIENT_ASYNC_FAILED : m_WorkshopItemCount ? CLIENT_ASYNC_SUCCEEDED : CLIENT_ASYNC_IDLE;
		pStatus->m_Stage = Working ? CLIENT_STAGE_SYNCING_MODS : CLIENT_STAGE_NONE;
		pStatus->m_Progress = Total ? clamp(Downloaded / (float)Total, 0.0f, 1.0f) : (pStatus->m_State == CLIENT_ASYNC_SUCCEEDED ? 1.0f : 0.0f);
		if(Failed) str_copy(pStatus->m_aErrorKey, "One or more Mods failed validation. Open Mods for details.", sizeof(pStatus->m_aErrorKey));
	}
	virtual bool CreateWorkshopItem()
	{
		if(!m_Initialized || !SteamUGC() || m_WorkshopPublish.m_Active) return false;
		mem_zero(&m_WorkshopPublish,sizeof(m_WorkshopPublish)); m_WorkshopPublish.m_Active=true;
		str_copy(m_WorkshopPublish.m_aStatus,"creating Workshop item",sizeof(m_WorkshopPublish.m_aStatus));
		const SteamAPICall_t Call=SteamUGC()->CreateItem((AppId_t)STEAM_APP_ID,k_EWorkshopFileTypeCommunity);
		if(Call==k_uAPICallInvalid){m_WorkshopPublish.m_Active=false;str_copy(m_WorkshopPublish.m_aStatus,"Steam rejected CreateItem",sizeof(m_WorkshopPublish.m_aStatus));return false;}
		m_WorkshopCreatedCall.Set(Call,this,&CSteamPlatformServices::OnWorkshopCreated);return true;
	}
	virtual bool UpdateWorkshopItem(unsigned long long ID,const char *pContentRoot,const char *pPreviewFile)
	{
		if(!m_Initialized||!SteamUGC()||m_WorkshopPublish.m_Active||!ID||!pContentRoot||!pContentRoot[0])return false;
		char aID[32],aError[256];str_format(aID,sizeof(aID),"%llu",ID);CModManifest Manifest;
		if(!ModPackageValidate(pContentRoot,aID,GAME_NETVERSION,&Manifest,aError,sizeof(aError))){mem_zero(&m_WorkshopPublish,sizeof(m_WorkshopPublish));str_copy(m_WorkshopPublish.m_aStatus,aError,sizeof(m_WorkshopPublish.m_aStatus));return false;}
		m_WorkshopUpdateHandle=SteamUGC()->StartItemUpdate((AppId_t)STEAM_APP_ID,(PublishedFileId_t)ID);
		if(m_WorkshopUpdateHandle==k_UGCUpdateHandleInvalid)return false;
		char aDescription[512],aMetadata[320];str_format(aDescription,sizeof(aDescription),"Ninslash Mod version %s by %s",Manifest.m_aVersion,Manifest.m_aAuthor);str_format(aMetadata,sizeof(aMetadata),"version=%s;protocol=%s;hash=%s;rating=%s",Manifest.m_aVersion,Manifest.m_aTargetProtocol,Manifest.m_aContentHash,Manifest.m_aContentRating);
		bool Ok=SteamUGC()->SetItemTitle(m_WorkshopUpdateHandle,Manifest.m_aName)&&SteamUGC()->SetItemDescription(m_WorkshopUpdateHandle,aDescription)&&SteamUGC()->SetItemMetadata(m_WorkshopUpdateHandle,aMetadata)&&SteamUGC()->SetItemContent(m_WorkshopUpdateHandle,pContentRoot);
		if(Ok&&pPreviewFile&&pPreviewFile[0])Ok=SteamUGC()->SetItemPreview(m_WorkshopUpdateHandle,pPreviewFile);
		const char *apTags[6];uint32 TagCount=0;if(Manifest.m_Api.m_Capabilities&MOD_CAPABILITY_GAMEPLAY_RULES)apTags[TagCount++]="Gameplay";if(Manifest.m_Api.m_Capabilities&MOD_CAPABILITY_WEAPONS)apTags[TagCount++]="Weapons";if(Manifest.m_Api.m_Capabilities&MOD_CAPABILITY_ITEMS)apTags[TagCount++]="Items";if(Manifest.m_Api.m_Capabilities&MOD_CAPABILITY_RESOURCES)apTags[TagCount++]="Resources";if(Manifest.m_FileCount)for(int i=0;i<Manifest.m_FileCount;i++)if(Manifest.m_aFiles[i].m_Type==MOD_FILE_MAP){apTags[TagCount++]="Maps";break;}apTags[TagCount++]=Manifest.m_aContentRating;
		SteamParamStringArray_t Tags={apTags,(int32)TagCount};if(Ok&&TagCount)Ok=SteamUGC()->SetItemTags(m_WorkshopUpdateHandle,&Tags,false);
		if(!Ok){m_WorkshopUpdateHandle=k_UGCUpdateHandleInvalid;return false;}
		mem_zero(&m_WorkshopPublish,sizeof(m_WorkshopPublish));m_WorkshopPublish.m_Active=true;m_WorkshopPublish.m_PublishedFileID=ID;str_copy(m_WorkshopPublish.m_aStatus,"submitting Workshop update",sizeof(m_WorkshopPublish.m_aStatus));
		const SteamAPICall_t Call=SteamUGC()->SubmitItemUpdate(m_WorkshopUpdateHandle,"Ninslash Mod update");if(Call==k_uAPICallInvalid){m_WorkshopPublish.m_Active=false;return false;}m_WorkshopSubmittedCall.Set(Call,this,&CSteamPlatformServices::OnWorkshopSubmitted);return true;
	}
	virtual void WorkshopPublishStatus(CPlatformWorkshopPublishStatus *pStatus) const
	{
		if(!pStatus) return;
		*pStatus=m_WorkshopPublish;
		if(m_WorkshopPublish.m_Active&&m_WorkshopUpdateHandle!=k_UGCUpdateHandleInvalid&&SteamUGC())
		{
			uint64 Processed=0,Total=0;SteamUGC()->GetItemUpdateProgress(m_WorkshopUpdateHandle,&Processed,&Total);
			pStatus->m_Processed=Processed;pStatus->m_Total=Total;
		}
	}
	virtual bool UnlockAchievement(const char *pAchievement)
	{
		return m_Initialized && pAchievement && pAchievement[0] && SteamUserStats() && SteamUserStats()->SetAchievement(pAchievement) && SteamUserStats()->StoreStats();
	}
	virtual void ProcessServerEvent(int Event, int Value, bool LeaderboardEligible)
	{
		if(PlatformEventIsLeaderboard(Event) && !LeaderboardEligible) return;
		if(m_EventQueue.Add(Event, Value, LeaderboardEligible))
		{
			SaveEventQueue();
			PumpEventQueue();
		}
	}
	virtual bool SteamInputActive() const { return m_SteamInputInitialized; }
	virtual void SetInputActionSet(EPlatformInputActionSet ActionSet)
	{
		if(ActionSet >= PLATFORM_INPUT_GAME && ActionSet <= PLATFORM_INPUT_CHAT)
			m_InputActionSet = ActionSet;
	}
	virtual bool ReadInputState(CPlatformInputState *pState)
	{
		if(!pState) return false;
		mem_zero(pState, sizeof(*pState));
		if(!m_SteamInputInitialized || !SteamInput()) return false;
		InputHandle_t aControllers[STEAM_INPUT_MAX_COUNT];
		const int Count = SteamInput()->GetConnectedControllers(aControllers);
		if(Count <= 0) return false;
		const InputHandle_t Controller = aControllers[0];
		SteamInput()->ActivateActionSet(Controller, m_aInputActionSets[m_InputActionSet]);
		pState->m_Connected = true;
		for(int i = 0; i < NUM_PLATFORM_INPUT_ACTIONS; i++)
		{
			const InputDigitalActionData_t Data = SteamInput()->GetDigitalActionData(Controller, m_aDigitalActions[i]);
			pState->m_aActions[i] = Data.bActive && Data.bState;
		}
		const InputAnalogActionData_t Move = SteamInput()->GetAnalogActionData(Controller, m_MoveAction);
		const InputAnalogActionData_t Aim = SteamInput()->GetAnalogActionData(Controller, m_AimAction);
		if(Move.bActive) { pState->m_MoveX = Move.x; pState->m_MoveY = Move.y; }
		if(Aim.bActive) { pState->m_AimX = Aim.x; pState->m_AimY = Aim.y; }
		return true;
	}
	virtual INetPacketTransport *RelayTransport() { return m_Initialized ? &m_RelayTransport : 0; }
	virtual INetPacketTransport *RelayListenTransport() { return m_Initialized ? &m_RelayListenTransport : 0; }
};

void CSteamPlatformServices::PumpEventQueue()
{
	if(!m_Initialized || !SteamUserStats() || m_ActiveLeaderboardEvent >= 0 || time_get() < m_NextEventRetry)
		return;
	const CPlatformEventQueue::CEntry *pEntry = m_EventQueue.First();
	if(!pEntry)
		return;
	static const char *s_apAchievements[12] = {
		"ACH_FIRST_INVASION", "ACH_FIRST_HORDE", "ACH_FIRST_EXTRACTION", "ACH_INVASION_10",
		"ACH_INVASION_30", "ACH_INVASION_60", "ACH_FIRST_FORGE", "ACH_FIRST_BUILD",
		"ACH_COOP_RESCUE", "ACH_FIRST_PVP_WIN", "ACH_FIRST_COOP_COMPLETE", "ACH_FIRST_BOSS"};
	if(pEntry->m_Event >= 0 && pEntry->m_Event < 12)
	{
		if(UnlockAchievement(s_apAchievements[pEntry->m_Event]))
		{
			m_EventQueue.RemoveFirst();
			SaveEventQueue();
		}
		else m_NextEventRetry = time_get() + time_freq() * 30;
		return;
	}
	if(pEntry->m_Event == PLATFORM_EVENT_STAT_COOP_COMPLETIONS)
	{
		int32 Current = 0;
		if(SteamUserStats()->GetStat("STAT_COOP_COMPLETIONS", &Current) &&
			SteamUserStats()->SetStat("STAT_COOP_COMPLETIONS", Current + max(1, pEntry->m_Value)) && SteamUserStats()->StoreStats())
		{
			m_EventQueue.RemoveFirst();
			SaveEventQueue();
		}
		else m_NextEventRetry = time_get() + time_freq() * 30;
		return;
	}
	if(!pEntry->m_Eligible || !PlatformEventIsLeaderboard(pEntry->m_Event))
	{
		m_EventQueue.RemoveFirst();
		SaveEventQueue();
		return;
	}
	const char *pName = pEntry->m_Event == PLATFORM_EVENT_LB_INVASION_FLOOR ? "Invasion Highest Floor" : "Fixed Seed Clear Time";
	const ELeaderboardSortMethod Sort = pEntry->m_Event == PLATFORM_EVENT_LB_INVASION_FLOOR ? k_ELeaderboardSortMethodDescending : k_ELeaderboardSortMethodAscending;
	const ELeaderboardDisplayType Display = pEntry->m_Event == PLATFORM_EVENT_LB_INVASION_FLOOR ? k_ELeaderboardDisplayTypeNumeric : k_ELeaderboardDisplayTypeTimeMilliSeconds;
	m_ActiveLeaderboardEvent = pEntry->m_Event;
	m_ActiveLeaderboardValue = pEntry->m_Value;
	m_LeaderboardFoundCall.Set(SteamUserStats()->FindOrCreateLeaderboard(pName, Sort, Display), this, &CSteamPlatformServices::OnLeaderboardFound);
}

void CSteamPlatformServices::OnLeaderboardFound(LeaderboardFindResult_t *pResult, bool IOError)
{
	if(IOError || !pResult || !pResult->m_bLeaderboardFound || !pResult->m_hSteamLeaderboard || !SteamUserStats())
	{
		m_ActiveLeaderboardEvent = -1;
		m_NextEventRetry = time_get() + time_freq() * 30;
		return;
	}
	m_LeaderboardUploadedCall.Set(SteamUserStats()->UploadLeaderboardScore(pResult->m_hSteamLeaderboard, k_ELeaderboardUploadScoreMethodKeepBest, m_ActiveLeaderboardValue, 0, 0), this, &CSteamPlatformServices::OnLeaderboardUploaded);
}

void CSteamPlatformServices::OnLeaderboardUploaded(LeaderboardScoreUploaded_t *pResult, bool IOError)
{
	if(!IOError && pResult && pResult->m_bSuccess)
	{
		m_EventQueue.RemoveFirst();
		SaveEventQueue();
	}
	else m_NextEventRetry = time_get() + time_freq() * 30;
	m_ActiveLeaderboardEvent = -1;
	PumpEventQueue();
}

void CSteamPlatformServices::OnJoinRequested(GameRichPresenceJoinRequested_t *pRequest)
{
	str_copy(m_aPendingJoin, pRequest->m_rgchConnect, sizeof(m_aPendingJoin));
}

void CSteamPlatformServices::OnLobbyJoinRequested(GameLobbyJoinRequested_t *pRequest)
{
	if(pRequest)
		JoinLobby(pRequest->m_steamIDLobby.ConvertToUint64());
}

void CSteamPlatformServices::OnLobbyMembersChanged(LobbyChatUpdate_t *pUpdate)
{
	if(!pUpdate || pUpdate->m_ulSteamIDLobby != m_CurrentLobbyID || !SteamFriends() || !SteamMatchmaking())
		return;
	if(m_HostedLobbyID == m_CurrentLobbyID && SteamMatchmaking()->GetLobbyOwner(CSteamID(m_CurrentLobbyID)) != SteamUser()->GetSteamID())
	{
		m_ListenServerStopRequested = true;
		m_HostedLobbyID = 0;
	}
	char aMembers[16];
	str_format(aMembers, sizeof(aMembers), "%d", SteamMatchmaking()->GetNumLobbyMembers(CSteamID(m_CurrentLobbyID)));
	SteamFriends()->SetRichPresence("steam_player_group_size", aMembers);
}

void CSteamPlatformServices::OnLobbyList(LobbyMatchList_t *pResult, bool IOError)
{
	m_LobbyRefreshPending = false;
	m_LobbyCount = 0;
	if(IOError || !pResult || !SteamMatchmaking()) return;
	const uint32 Count = min((uint32)128, pResult->m_nLobbiesMatching);
	for(uint32 i = 0; i < Count; i++)
	{
		const CSteamID Lobby = SteamMatchmaking()->GetLobbyByIndex((int)i);
		const CSteamID Owner = SteamMatchmaking()->GetLobbyOwner(Lobby);
		const char *pProtocol = SteamMatchmaking()->GetLobbyData(Lobby,"protocol");
		const char *pRoomType = SteamMatchmaking()->GetLobbyData(Lobby,"room_type");
		const char *pHost = SteamMatchmaking()->GetLobbyData(Lobby,"host_steamid");
		const char *pConnect = SteamMatchmaking()->GetLobbyData(Lobby,"connect");
		unsigned long long Host = 0; char Trailing = 0, aExpected[48];
		if(!Lobby.IsValid() || !Owner.IsValid() || !pProtocol || str_comp(pProtocol,GAME_NETVERSION) || !pRoomType || str_comp(pRoomType,"steam_listen") ||
			!pHost || sscanf(pHost,"%llu%c",&Host,&Trailing)!=1 || Host!=Owner.ConvertToUint64()) continue;
		str_format(aExpected,sizeof(aExpected),"steam:%llu",Host);
		if(!pConnect || str_comp(pConnect,aExpected)) continue;
		CPlatformLobbyInfo &Info=m_aLobbies[m_LobbyCount++]; mem_zero(&Info,sizeof(Info));
		Info.m_LobbyID=Lobby.ConvertToUint64(); Info.m_HostSteamID=Host;
		Info.m_Members=SteamMatchmaking()->GetNumLobbyMembers(Lobby); Info.m_MaxMembers=SteamMatchmaking()->GetLobbyMemberLimit(Lobby);
		const char *pPassword=SteamMatchmaking()->GetLobbyData(Lobby,"password"); Info.m_Password=pPassword&&str_comp(pPassword,"1")==0;
		const char *pModHash=SteamMatchmaking()->GetLobbyData(Lobby,"mod_hash"); str_copy(Info.m_aModHash,pModHash?pModHash:"none",sizeof(Info.m_aModHash)); Info.m_Modded=str_comp(Info.m_aModHash,"none")!=0;
		str_copy(Info.m_aMap,SteamMatchmaking()->GetLobbyData(Lobby,"map"),sizeof(Info.m_aMap));
		str_copy(Info.m_aGameType,SteamMatchmaking()->GetLobbyData(Lobby,"gametype"),sizeof(Info.m_aGameType));
		str_copy(Info.m_aRegion,SteamMatchmaking()->GetLobbyData(Lobby,"region"),sizeof(Info.m_aRegion));
		if(!Info.m_aRegion[0]) str_copy(Info.m_aRegion,"auto",sizeof(Info.m_aRegion));
		if(SteamFriends())
		{
			str_copy(Info.m_aHostName,SteamFriends()->GetFriendPersonaName(Owner),sizeof(Info.m_aHostName));
			Info.m_FriendHosted=SteamFriends()->HasFriend(Owner,k_EFriendFlagImmediate);
		}
	}
}

void CSteamPlatformServices::OnWorkshopCreated(CreateItemResult_t *pResult, bool IOError)
{
	m_WorkshopPublish.m_Active=false;
	if(IOError||!pResult||pResult->m_eResult!=k_EResultOK){str_copy(m_WorkshopPublish.m_aStatus,"Workshop item creation failed",sizeof(m_WorkshopPublish.m_aStatus));return;}
	m_WorkshopPublish.m_PublishedFileID=pResult->m_nPublishedFileId;m_WorkshopPublish.m_NeedsLegalAgreement=pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement;
	str_copy(m_WorkshopPublish.m_aStatus,pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement?"item created; accept the Workshop legal agreement, then add this ID to ninslash_mod.json":"item created; add this ID to ninslash_mod.json before publishing content",sizeof(m_WorkshopPublish.m_aStatus));
}

void CSteamPlatformServices::OnWorkshopSubmitted(SubmitItemUpdateResult_t *pResult, bool IOError)
{
	m_WorkshopPublish.m_Active=false;m_WorkshopUpdateHandle=k_UGCUpdateHandleInvalid;
	if(IOError||!pResult||pResult->m_eResult!=k_EResultOK){str_copy(m_WorkshopPublish.m_aStatus,"Workshop update failed",sizeof(m_WorkshopPublish.m_aStatus));return;}
	m_WorkshopPublish.m_PublishedFileID=pResult->m_nPublishedFileId;m_WorkshopPublish.m_NeedsLegalAgreement=pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement;
	str_copy(m_WorkshopPublish.m_aStatus,pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement?"update submitted; Workshop legal agreement acceptance required":"Workshop update published",sizeof(m_WorkshopPublish.m_aStatus));RefreshWorkshopItems();
}

void CSteamPlatformServices::OnLobbyCreated(LobbyCreated_t *pResult, bool IOError)
{
	m_LobbyCreatePending = false;
	if(IOError || !pResult || pResult->m_eResult != k_EResultOK)
	{
		m_ListenServerStopRequested = true;
		return;
	}
	m_CurrentLobbyID = pResult->m_ulSteamIDLobby;
	m_HostedLobbyID = m_CurrentLobbyID;
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "protocol", GAME_NETVERSION);
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "room_type", "steam_listen");
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "map", g_Config.m_SvMap);
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "gametype", g_Config.m_SvGametype);
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "mod_hash", g_Config.m_SvModHash[0] ? g_Config.m_SvModHash : "none");
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "mod_ids", g_Config.m_ClModIds);
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "password", g_Config.m_Password[0] ? "1" : "0");
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "official", "0");
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "region", "auto");
	char aHostSteamID[32];
	str_format(aHostSteamID, sizeof(aHostSteamID), "%llu", SteamUser()->GetSteamID().ConvertToUint64());
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "host_steamid", aHostSteamID);
	char aConnect[48];
	str_format(aConnect, sizeof(aConnect), "steam:%s", aHostSteamID);
	SteamMatchmaking()->SetLobbyData(CSteamID(m_CurrentLobbyID), "connect", aConnect);
	str_format(m_aPendingJoin, sizeof(m_aPendingJoin), "127.0.0.1:%d", clamp(g_Config.m_ClLocalServerPort, 1024, 65535));
	SteamFriends()->SetRichPresence("connect", aConnect);
}

void CSteamPlatformServices::OnLobbyEntered(LobbyEnter_t *pResult, bool IOError)
{
	m_LobbyJoinPending = false;
	if(IOError || !pResult || pResult->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess)
	{
		SetJoinFailure("Steam rejected the room join request. Refresh rooms and try again.");
		return;
	}
	m_CurrentLobbyID = pResult->m_ulSteamIDLobby;
	m_PendingLobbyJoinID = m_CurrentLobbyID;
	if(m_HostedLobbyID == m_CurrentLobbyID)
	{
		str_format(m_aPendingJoin, sizeof(m_aPendingJoin), "127.0.0.1:%d", clamp(g_Config.m_ClLocalServerPort, 1024, 65535));
		return;
	}

	const CSteamID Lobby(m_CurrentLobbyID);
	const char *pRoomType = SteamMatchmaking()->GetLobbyData(Lobby, "room_type");
	const char *pProtocol = SteamMatchmaking()->GetLobbyData(Lobby, "protocol");
	const char *pHostSteamID = SteamMatchmaking()->GetLobbyData(Lobby, "host_steamid");
	const char *pConnect = SteamMatchmaking()->GetLobbyData(Lobby, "connect");
	const char *pModHash = SteamMatchmaking()->GetLobbyData(Lobby, "mod_hash");
	const char *pModIDs = SteamMatchmaking()->GetLobbyData(Lobby, "mod_ids");
	unsigned long long HostSteamID = 0;
	char Trailing = 0;
	char aExpectedConnect[48];
	if(!pRoomType || str_comp(pRoomType, "steam_listen") != 0 || !pProtocol || str_comp(pProtocol, GAME_NETVERSION) != 0)
		SetJoinFailure("This room uses an incompatible game version. Update Ninslash and retry.");
	else if(!pHostSteamID || sscanf(pHostSteamID, "%llu%c", &HostSteamID, &Trailing) != 1 || !HostSteamID || SteamMatchmaking()->GetLobbyOwner(Lobby).ConvertToUint64() != HostSteamID)
		SetJoinFailure("The room host identity could not be verified. Leave the room and choose another.");
	else
	{
		str_format(aExpectedConnect, sizeof(aExpectedConnect), "steam:%llu", HostSteamID);
		const char *pLocalModHash = g_Config.m_ClModHash[0] ? g_Config.m_ClModHash : "none";
		if(!pConnect || str_comp(pConnect, aExpectedConnect) != 0)
			SetJoinFailure("The room connection data is invalid. Ask the host to recreate the room.");
		else if(!pModHash || str_comp(pModHash, pLocalModHash) != 0)
		{
			bool Requested = false, ValidIDs = pModIDs && pModIDs[0];
			const char *pID = pModIDs;
			while(ValidIDs && *pID)
			{
				unsigned long long ID = 0; char aID[32]; int Length = 0;
				while(pID[Length] && pID[Length] != ',' && Length < 31) { aID[Length] = pID[Length]; Length++; }
				aID[Length] = 0;
				char TrailingID = 0;
				if(!Length || sscanf(aID, "%llu%c", &ID, &TrailingID) != 1 || !ID) { ValidIDs = false; break; }
				Requested = SubscribeWorkshopItem(ID) || Requested;
				if(SteamUGC()) SteamUGC()->DownloadItem((PublishedFileId_t)ID, true);
				pID += Length; if(*pID == ',') pID++; else if(*pID) ValidIDs = false;
			}
			SetJoinFailure(ValidIDs && Requested ? "Required Mods are downloading. Open Mods, wait for validation, then retry joining." : "This room requires a different Mod collection. Open Mods and enable the required collection.");
		}
		else
			str_copy(m_aPendingJoin, pConnect, sizeof(m_aPendingJoin));
	}

	if(m_aJoinFailure[0])
	{
		SteamMatchmaking()->LeaveLobby(Lobby);
		m_CurrentLobbyID = 0;
		m_PendingLobbyJoinID = 0;
	}
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
