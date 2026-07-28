#include <base/system.h>
#include <base/math.h>
#include <engine/platform_services.h>
#include <engine/client.h>
#include <engine/platform_events.h>
#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/shared/platform_event_queue.h>
#include <engine/shared/platform_server_metadata.h>
#include <engine/shared/content_collection.h>
#include <engine/shared/content_package.h>
#include <engine/shared/community_challenge.h>
#include <engine/storage.h>
#include <engine/serverbrowser.h>
#include <game/version.h>

#if defined(CONF_STEAMWORKS)
#include <steam_api.h>
#endif

namespace
{
#if defined(CONF_STEAMWORKS)
struct CWorkshopPreviewCacheEntry { char m_aName[256]; long m_Size; int64 m_Age; };
struct CWorkshopPreviewCacheScan { char m_aRoot[1024]; CWorkshopPreviewCacheEntry m_aEntries[512]; int m_Count; long long m_Total; };
int WorkshopPreviewCacheScanCallback(const char *pName, int IsDir, int DirType, void *pUser)
{
	(void)DirType;
	CWorkshopPreviewCacheScan *pScan = (CWorkshopPreviewCacheScan *)pUser;
	if(IsDir || !pName || pName[0] == '.' || str_find(pName, ".tmp") || pScan->m_Count >= 512) return 0;
	char aPath[1280]; str_format(aPath, sizeof(aPath), "%s/%s", pScan->m_aRoot, pName); IOHANDLE File = io_open(aPath, IOFLAG_READ); if(!File) return 0;
	const long Size = io_length(File); io_close(File); if(Size < 0) return 0;
	CWorkshopPreviewCacheEntry &Entry = pScan->m_aEntries[pScan->m_Count++]; str_copy(Entry.m_aName, pName, sizeof(Entry.m_aName)); Entry.m_Size = Size; const char *pAge = strrchr(pName, '_'); Entry.m_Age = pAge ? str_toint(pAge + 1) : 0; pScan->m_Total += Size; return 0;
}

void TrimWorkshopPreviewCache(IStorage *pStorage)
{
	if(!pStorage) return;
	CWorkshopPreviewCacheScan Scan; mem_zero(&Scan, sizeof(Scan)); pStorage->GetCompletePath(IStorage::TYPE_SAVE, "workshop_cache/previews", Scan.m_aRoot, sizeof(Scan.m_aRoot)); fs_listdir(Scan.m_aRoot, WorkshopPreviewCacheScanCallback, 0, &Scan);
	while(Scan.m_Count > 256 || Scan.m_Total > 128LL * 1024 * 1024)
	{
		int Oldest = 0; for(int i = 1; i < Scan.m_Count; i++) if(Scan.m_aEntries[i].m_Age < Scan.m_aEntries[Oldest].m_Age) Oldest = i;
		char aPath[1280]; str_format(aPath, sizeof(aPath), "%s/%s", Scan.m_aRoot, Scan.m_aEntries[Oldest].m_aName); if(fs_remove(aPath) == 0) Scan.m_Total -= Scan.m_aEntries[Oldest].m_Size; Scan.m_aEntries[Oldest] = Scan.m_aEntries[--Scan.m_Count];
	}
}

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
	virtual void SetRichPresence(const CPlatformPresence &Presence) { (void)Presence; }
	virtual void SetTimelineMode(EPlatformTimelineMode Mode, const char *pDescription) { (void)Mode; (void)pDescription; }
	virtual bool AddTimelineEvent(const CPlatformTimelineEvent &Event) { (void)Event; return false; }
	virtual bool RegisterScreenshot(const char *pAbsolutePath, int Width, int Height, const CPlatformScreenshotContext &Context) { (void)pAbsolutePath; (void)Width; (void)Height; (void)Context; return false; }
	virtual void SetScreenshotContext(const CPlatformScreenshotContext &Context) { (void)Context; }
	virtual bool ConsumeJoinRequest(char *pBuffer, int BufferSize)
	{
		if(BufferSize > 0)
			pBuffer[0] = 0;
		return false;
	}
	virtual bool ConsumeJoinFailure(char *pBuffer, int BufferSize) { if(pBuffer && BufferSize > 0) pBuffer[0] = 0; return false; }
	virtual void CloudStatus(CPlatformCloudStatus *pStatus) const { if(pStatus) mem_zero(pStatus, sizeof(*pStatus)); }
	virtual int CloudFileSize(const char *pFilename) const { (void)pFilename; return -1; }
	virtual long long CloudFileTimestamp(const char *pFilename) const { (void)pFilename; return 0; }
	virtual int CloudReadFile(const char *pFilename, void *pBuffer, int BufferSize) { (void)pFilename; (void)pBuffer; (void)BufferSize; return -1; }
	virtual bool CloudWriteFile(const char *pFilename, const void *pBuffer, int BufferSize) { (void)pFilename; (void)pBuffer; (void)BufferSize; return false; }
	virtual bool CreateParty() { return false; }
	virtual bool JoinParty(unsigned long long LobbyID) { (void)LobbyID; return false; }
	virtual void LeaveParty() {}
	virtual unsigned long long PartyLobbyID() const { return 0; }
	virtual bool PartyState(CPlatformPartyState *pState) const { if(pState) mem_zero(pState, sizeof(*pState)); return false; }
	virtual int PartyMemberCount() const { return 0; }
	virtual bool PartyMemberInfo(int Index, CPlatformUserInfo *pInfo) const { (void)Index; if(pInfo) mem_zero(pInfo, sizeof(*pInfo)); return false; }
	virtual bool InvitePartyUser(unsigned long long UserID) { (void)UserID; return false; }
	virtual bool OpenPartyInviteDialog() { return false; }
	virtual bool SetPartyReady(bool Ready) { (void)Ready; return false; }
	virtual bool SetPartyTarget(int TargetType, unsigned long long TargetLobbyID, const char *pAddress, const char *pModHash) { (void)TargetType; (void)TargetLobbyID; (void)pAddress; (void)pModHash; return false; }
	virtual bool ClearPartyTarget() { return false; }
	virtual bool LaunchParty(bool Force) { (void)Force; return false; }
	virtual bool ConsumePartyLaunch(CPlatformPartyLaunch *pLaunch) { if(pLaunch) mem_zero(pLaunch, sizeof(*pLaunch)); return false; }
	virtual void PartyOperationStatus(CPlatformOperationStatus *pStatus) const { if(pStatus) mem_zero(pStatus, sizeof(*pStatus)); }
	virtual bool CreateLobby(EPlatformLobbyVisibility Visibility, int MaxMembers, int HostLocalPort) { (void)Visibility; (void)MaxMembers; (void)HostLocalPort; return false; }
	virtual bool JoinLobby(unsigned long long LobbyID) { (void)LobbyID; return false; }
	virtual void LeaveLobby() {}
	virtual unsigned long long CurrentLobbyID() const { return 0; }
	virtual unsigned long long GameLobbyID() const { return 0; }
	virtual void LeaveGameLobby() {}
	virtual bool SetLobbyData(const char *pKey, const char *pValue) { (void)pKey; (void)pValue; return false; }
	virtual bool ConsumeLobbyJoin(unsigned long long *pLobbyID) { if(pLobbyID) *pLobbyID = 0; return false; }
	virtual bool ConsumeListenServerStopRequest() { return false; }
	virtual bool OpenLobbyInviteDialog() { return false; }
	virtual int FriendCount() const { return 0; }
	virtual bool FriendInfo(int Index, CPlatformUserInfo *pInfo) const { (void)Index; if(pInfo) mem_zero(pInfo, sizeof(*pInfo)); return false; }
	virtual bool UserInfo(unsigned long long UserID, CPlatformUserInfo *pInfo) const { (void)UserID; if(pInfo) mem_zero(pInfo, sizeof(*pInfo)); return false; }
	virtual int LobbyMemberCount() const { return 0; }
	virtual bool LobbyMemberInfo(int Index, CPlatformUserInfo *pInfo) const { (void)Index; if(pInfo) mem_zero(pInfo, sizeof(*pInfo)); return false; }
	virtual bool InviteUser(unsigned long long UserID, const char *pConnect) { (void)UserID; (void)pConnect; return false; }
	virtual bool JoinUser(unsigned long long UserID) { (void)UserID; return false; }
	virtual bool OpenUserProfile(unsigned long long UserID) { (void)UserID; return false; }
	virtual void SetPlayedWith(unsigned long long UserID) { (void)UserID; }
	virtual int UserAvatarRGBA(unsigned long long UserID, int PreferredSize, void *pBuffer, int BufferSize, int *pWidth, int *pHeight) { (void)UserID; (void)PreferredSize; (void)pBuffer; (void)BufferSize; if(pWidth) *pWidth = 0; if(pHeight) *pHeight = 0; return -1; }
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
	virtual unsigned QueryWorkshop(const CPlatformWorkshopQuery &Query) { (void)Query; return 0; }
	virtual bool ConsumeWorkshopQueryResult(CPlatformWorkshopQueryResult *pResult) { if(pResult) mem_zero(pResult,sizeof(*pResult)); return false; }
	virtual int WorkshopQueryItemCount() const { return 0; }
	virtual bool WorkshopQueryItem(int Index,CPlatformWorkshopItem *pItem) const { (void)Index; if(pItem) mem_zero(pItem,sizeof(*pItem)); return false; }
	virtual unsigned RequestWorkshopPreview(unsigned long long PublishedFileID) { (void)PublishedFileID; return 0; }
	virtual bool ConsumeWorkshopPreviewResult(CPlatformWorkshopPreviewResult *pResult) { if(pResult) mem_zero(pResult, sizeof(*pResult)); return false; }
	virtual bool RequestWorkshopDownload(unsigned long long PublishedFileID) { (void)PublishedFileID; return false; }
	virtual bool UserDisplayName(unsigned long long UserID, char *pBuffer, int BufferSize) { (void)UserID; if(pBuffer && BufferSize > 0) pBuffer[0] = 0; return false; }
	virtual bool CreateWorkshopItem() { return false; }
	virtual bool UpdateWorkshopItem(unsigned long long PublishedFileID, const char *pContentRoot, const char *pPreviewFile) { (void)PublishedFileID;(void)pContentRoot;(void)pPreviewFile;return false; }
	virtual void WorkshopPublishStatus(CPlatformWorkshopPublishStatus *pStatus) const { if(pStatus)mem_zero(pStatus,sizeof(*pStatus)); }
	virtual bool UnlockAchievement(const char *pAchievement) { (void)pAchievement; return false; }
	virtual void ProcessServerEvent(int Event, int Value, bool LeaderboardEligible) { (void)Event; (void)Value; (void)LeaderboardEligible; }
	virtual unsigned SubmitCommunityChallenge(unsigned long long PublishedFileID,int Revision,int Metric,int Score) { (void)PublishedFileID;(void)Revision;(void)Metric;(void)Score;return 0; }
	virtual unsigned QueryCommunityChallenge(unsigned long long PublishedFileID,int Revision,int Metric,EPlatformLeaderboardScope Scope) { (void)PublishedFileID;(void)Revision;(void)Metric;(void)Scope;return 0; }
	virtual bool ConsumeCommunityLeaderboardResult(CPlatformLeaderboardResult *pResult) { if(pResult)mem_zero(pResult,sizeof(*pResult));return false; }
	virtual int CommunityLeaderboardEntryCount() const { return 0; }
	virtual bool CommunityLeaderboardEntry(int Index,CPlatformLeaderboardEntry *pEntry) const { (void)Index;if(pEntry)mem_zero(pEntry,sizeof(*pEntry));return false; }
	virtual bool SteamInputActive() const { return false; }
	virtual void SetInputActionSet(EPlatformInputActionSet ActionSet) { (void)ActionSet; }
	virtual bool ReadInputState(CPlatformInputState *pState) { if(pState) mem_zero(pState, sizeof(*pState)); return false; }
	virtual bool InputGlyph(EPlatformInputAction Action, char *pBuffer, int BufferSize) { (void)Action; if(pBuffer && BufferSize > 0) pBuffer[0] = 0; return false; }
	virtual void TriggerInputVibration(unsigned short LeftSpeed, unsigned short RightSpeed) { (void)LeftSpeed; (void)RightSpeed; }
	virtual bool OpenInputConfiguration() { return false; }
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
	unsigned long long m_PartyLobbyID;
	unsigned long long m_PartyOwnerID;
	unsigned long long m_PendingPartyInviteUserID;
	bool m_CreatingParty;
	bool m_JoiningParty;
	bool m_OpenPartyInviteAfterCreate;
	unsigned int m_ConsumedPartyLaunchGeneration;
	CPlatformPartyLaunch m_PendingPartyLaunch;
	unsigned long long m_PendingLobbyJoinID;
	unsigned long long m_HostedLobbyID;
	int m_HostLocalPort;
	bool m_ListenServerStopRequested;
	bool m_LobbyCreatePending;
	ELobbyType m_LobbyCreateType;
	int m_LobbyCreateMaxMembers;
	int m_LobbyCreateRetries;
	int64 m_LobbyCreateRetryAt;
	char m_aLobbyCreateFailure[128];
	bool m_LobbyRefreshPending;
	bool m_LobbyJoinPending;
	IStorage *m_pStorage;
	CPlatformEventQueue m_EventQueue;
	int m_ActiveLeaderboardEvent;
	int m_ActiveLeaderboardValue;
	int64 m_NextEventRetry;
	CPlatformLeaderboardEntry m_aCommunityEntries[100];
	int m_CommunityEntryCount;
	unsigned m_CommunityOperationID;
	int m_CommunityOperation; // 1 upload, 2 query.
	int m_CommunityScore;
	EPlatformLeaderboardScope m_CommunityScope;
	bool m_CommunityResultReady;
	CPlatformLeaderboardResult m_CommunityResult;
	CPlatformWorkshopItem m_aWorkshopItems[256];
	int m_WorkshopItemCount;
	CPlatformWorkshopItem m_aWorkshopQueryItems[50];
	int m_WorkshopQueryItemCount;
	UGCQueryHandle_t m_WorkshopQueryHandle;
	unsigned m_WorkshopQueryOperationID;
	bool m_WorkshopQueryPending;
	bool m_WorkshopQueryResultReady;
	CPlatformWorkshopQueryResult m_WorkshopQueryResult;
	struct CWorkshopPreviewRequest
	{
		HTTPRequestHandle m_Handle;
		unsigned m_OperationID;
		unsigned long long m_PublishedFileID;
		unsigned int m_UpdatedAt;
	};
	CWorkshopPreviewRequest m_aWorkshopPreviewRequests[4];
	int m_WorkshopPreviewRequestCount;
	CPlatformWorkshopPreviewResult m_aWorkshopPreviewResults[16];
	int m_WorkshopPreviewResultCount;
	unsigned m_WorkshopPreviewOperationID;
	CPlatformLobbyInfo m_aLobbies[128];
	int m_LobbyCount;
	CPlatformWorkshopPublishStatus m_WorkshopPublish;
	UGCUpdateHandle_t m_WorkshopUpdateHandle;
	HServerListRequest m_DedicatedServerRequest;
	CContentCollection m_ModCollection;
	EPlatformInputActionSet m_InputActionSet;
	InputActionSetHandle_t m_aInputActionSets[NUM_PLATFORM_INPUT_ACTION_SETS];
	InputDigitalActionHandle_t m_aDigitalActions[NUM_PLATFORM_INPUT_ACTIONS];
	InputAnalogActionHandle_t m_MoveAction;
	InputAnalogActionHandle_t m_AimAction;
	char m_aaInputGlyphs[NUM_PLATFORM_INPUT_ACTIONS][512];
	CPlatformScreenshotContext m_ScreenshotContext;
	struct CPendingScreenshot { ScreenshotHandle m_Handle; CPlatformScreenshotContext m_Context; };
	CPendingScreenshot m_aPendingScreenshots[16];
	int m_PendingScreenshotCount;
	unsigned long long m_aTimelineDedupe[256];
	int m_TimelineDedupeCount;
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
	void OnLobbyDataUpdated(LobbyDataUpdate_t *pUpdate);
	void OnLobbyCreated(LobbyCreated_t *pResult, bool IOError);
	void OnLobbyEntered(LobbyEnter_t *pResult, bool IOError);
	void OnLobbyList(LobbyMatchList_t *pResult, bool IOError);
	void OnWorkshopDownloaded(DownloadItemResult_t *pResult) { if(pResult && pResult->m_unAppID == STEAM_APP_ID) RefreshWorkshopItems(); }
	void OnWorkshopCreated(CreateItemResult_t *pResult, bool IOError);
	void OnWorkshopSubmitted(SubmitItemUpdateResult_t *pResult, bool IOError);
	void OnWorkshopQuery(SteamUGCQueryCompleted_t *pResult, bool IOError);
	void OnWorkshopPreviewDownloaded(HTTPRequestCompleted_t *pResult);
	void OnScreenshotReady(ScreenshotReady_t *pResult);
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
	static bool LobbyCreateFailureIsTransient(EResult Result, bool IOError)
	{
		return IOError || Result == k_EResultNoConnection || Result == k_EResultBusy || Result == k_EResultTimeout || Result == k_EResultServiceUnavailable || Result == k_EResultConnectFailed;
	}
	static const char *LobbyCreateFailureKey(EResult Result, bool IOError)
	{
		if(IOError || Result == k_EResultTimeout)
			return "Steam room creation timed out. Check Steam connection and retry.";
		if(Result == k_EResultNoConnection || Result == k_EResultConnectFailed)
			return "Steam is offline. Reconnect Steam and retry room creation.";
		if(Result == k_EResultAccessDenied)
			return "Steam denied room creation for this account.";
		if(Result == k_EResultLimitExceeded)
			return "Steam room limit reached. Close another room and retry.";
		if(Result == k_EResultBusy || Result == k_EResultServiceUnavailable)
			return "Steam room service is busy. Wait a moment and retry.";
		return "Steam rejected room creation. Check your connection and retry.";
	}
	bool BeginLobbyCreateCall()
	{
		if(!m_Initialized || !SteamMatchmaking())
			return false;
		const SteamAPICall_t Call = SteamMatchmaking()->CreateLobby(m_LobbyCreateType, m_LobbyCreateMaxMembers);
		if(Call == k_uAPICallInvalid)
		{
			str_copy(m_aLobbyCreateFailure, "Steam could not start room creation. Restart Steam and retry.", sizeof(m_aLobbyCreateFailure));
			dbg_msg("steam", "CreateLobby returned an invalid API call");
			return false;
		}
		m_LobbyCreatePending = true;
		m_LobbyCreatedCall.Set(Call, this, &CSteamPlatformServices::OnLobbyCreated);
		return true;
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
		static const char *s_apSets[] = {"gameplay", "menu", "spectator", "chat", "inventory", "build", "radial_menu", "replay", "editor"};
		static const char *s_apActions[] = {"confirm", "cancel", "fire", "turbo", "scoreboard", "build", "drop", "emote", "weapon_picker", "last_weapon", "prev_weapon", "next_weapon", "up", "down", "left", "right", "jump", "crouch", "charge", "inventory", "forge", "drone_radial", "weapon_1", "weapon_2", "weapon_3", "weapon_4", "ready", "vote_yes", "vote_no", "chat", "pause", "replay_play_pause", "replay_seek_back", "replay_seek_forward", "editor_primary", "editor_secondary"};
		for(int i = 0; i < NUM_PLATFORM_INPUT_ACTION_SETS; i++) m_aInputActionSets[i] = SteamInput()->GetActionSetHandle(s_apSets[i]);
		for(int i = 0; i < NUM_PLATFORM_INPUT_ACTIONS; i++) m_aDigitalActions[i] = SteamInput()->GetDigitalActionHandle(s_apActions[i]);
		m_MoveAction = SteamInput()->GetAnalogActionHandle("move");
		m_AimAction = SteamInput()->GetAnalogActionHandle("aim");
		return true;
	}
	CCallback<CSteamPlatformServices, GameRichPresenceJoinRequested_t> m_JoinRequestedCallback;
	CCallback<CSteamPlatformServices, GetAuthSessionTicketResponse_t> m_AuthTicketCallback;
	CCallback<CSteamPlatformServices, GameLobbyJoinRequested_t> m_LobbyJoinRequestedCallback;
	CCallback<CSteamPlatformServices, LobbyChatUpdate_t> m_LobbyMembersChangedCallback;
	CCallback<CSteamPlatformServices, LobbyDataUpdate_t> m_LobbyDataUpdatedCallback;
	CCallback<CSteamPlatformServices, DownloadItemResult_t> m_WorkshopDownloadedCallback;
	CCallback<CSteamPlatformServices, HTTPRequestCompleted_t> m_WorkshopPreviewDownloadedCallback;
	CCallback<CSteamPlatformServices, ScreenshotReady_t> m_ScreenshotReadyCallback;
	CCallResult<CSteamPlatformServices, LobbyCreated_t> m_LobbyCreatedCall;
	CCallResult<CSteamPlatformServices, LobbyEnter_t> m_LobbyEnteredCall;
	CCallResult<CSteamPlatformServices, LobbyMatchList_t> m_LobbyListCall;
	CCallResult<CSteamPlatformServices, CreateItemResult_t> m_WorkshopCreatedCall;
	CCallResult<CSteamPlatformServices, SubmitItemUpdateResult_t> m_WorkshopSubmittedCall;
	CCallResult<CSteamPlatformServices, SteamUGCQueryCompleted_t> m_WorkshopQueryCall;
	CCallResult<CSteamPlatformServices, LeaderboardFindResult_t> m_LeaderboardFoundCall;
	CCallResult<CSteamPlatformServices, LeaderboardScoreUploaded_t> m_LeaderboardUploadedCall;
	CCallResult<CSteamPlatformServices, LeaderboardFindResult_t> m_CommunityFoundCall;
	CCallResult<CSteamPlatformServices, LeaderboardScoreUploaded_t> m_CommunityUploadedCall;
	CCallResult<CSteamPlatformServices, LeaderboardScoresDownloaded_t> m_CommunityDownloadedCall;
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
	void OnCommunityFound(LeaderboardFindResult_t *pResult, bool IOError);
	void OnCommunityUploaded(LeaderboardScoreUploaded_t *pResult, bool IOError);
	void OnCommunityDownloaded(LeaderboardScoresDownloaded_t *pResult, bool IOError);

public:
	CSteamPlatformServices() :
		m_Initialized(false), m_ExitRequested(false), m_SteamInputInitialized(false), m_CurrentLobbyID(0), m_PartyLobbyID(0), m_PartyOwnerID(0), m_PendingPartyInviteUserID(0), m_CreatingParty(false), m_JoiningParty(false), m_OpenPartyInviteAfterCreate(false), m_ConsumedPartyLaunchGeneration(0), m_PendingLobbyJoinID(0), m_HostedLobbyID(0), m_HostLocalPort(0), m_ListenServerStopRequested(false), m_LobbyCreatePending(false), m_LobbyCreateType(k_ELobbyTypeFriendsOnly), m_LobbyCreateMaxMembers(0), m_LobbyCreateRetries(0), m_LobbyCreateRetryAt(0), m_LobbyRefreshPending(false), m_LobbyJoinPending(false), m_pStorage(0), m_ActiveLeaderboardEvent(-1), m_ActiveLeaderboardValue(0), m_NextEventRetry(0), m_CommunityEntryCount(0), m_CommunityOperationID(0), m_CommunityOperation(0), m_CommunityScore(0), m_CommunityScope(PLATFORM_LEADERBOARD_GLOBAL), m_CommunityResultReady(false), m_WorkshopItemCount(0), m_WorkshopQueryItemCount(0), m_WorkshopQueryHandle(k_UGCQueryHandleInvalid), m_WorkshopQueryOperationID(0), m_WorkshopQueryPending(false), m_WorkshopQueryResultReady(false), m_WorkshopPreviewRequestCount(0), m_WorkshopPreviewResultCount(0), m_WorkshopPreviewOperationID(0), m_LobbyCount(0), m_WorkshopUpdateHandle(k_UGCUpdateHandleInvalid), m_DedicatedServerRequest(0), m_InputActionSet(PLATFORM_INPUT_MENU), m_MoveAction(0), m_AimAction(0), m_PendingScreenshotCount(0), m_TimelineDedupeCount(0), m_AuthTicketSize(0), m_AuthTicketHandle(k_HAuthTicketInvalid), m_AuthTicketState(0),
		m_JoinRequestedCallback(this, &CSteamPlatformServices::OnJoinRequested),
		m_AuthTicketCallback(this, &CSteamPlatformServices::OnAuthTicketResponse),
		m_LobbyJoinRequestedCallback(this, &CSteamPlatformServices::OnLobbyJoinRequested),
		m_LobbyMembersChangedCallback(this, &CSteamPlatformServices::OnLobbyMembersChanged),
		m_LobbyDataUpdatedCallback(this, &CSteamPlatformServices::OnLobbyDataUpdated),
		m_WorkshopDownloadedCallback(this, &CSteamPlatformServices::OnWorkshopDownloaded),
		m_WorkshopPreviewDownloadedCallback(this, &CSteamPlatformServices::OnWorkshopPreviewDownloaded),
		m_ScreenshotReadyCallback(this, &CSteamPlatformServices::OnScreenshotReady)
	{
		m_aPendingJoin[0] = 0;
		m_aJoinFailure[0] = 0;
		m_aLobbyCreateFailure[0] = 0;
		mem_zero(&m_PendingPartyLaunch, sizeof(m_PendingPartyLaunch));
		mem_zero(m_aInputActionSets, sizeof(m_aInputActionSets));
		mem_zero(m_aDigitalActions, sizeof(m_aDigitalActions));
		mem_zero(m_aaInputGlyphs, sizeof(m_aaInputGlyphs));
		mem_zero(&m_ScreenshotContext, sizeof(m_ScreenshotContext));
		mem_zero(m_aPendingScreenshots, sizeof(m_aPendingScreenshots));
		mem_zero(m_aTimelineDedupe, sizeof(m_aTimelineDedupe));
		mem_zero(&m_WorkshopPublish, sizeof(m_WorkshopPublish));
		mem_zero(&m_WorkshopQueryResult, sizeof(m_WorkshopQueryResult));
		mem_zero(m_aWorkshopPreviewRequests, sizeof(m_aWorkshopPreviewRequests));
		mem_zero(m_aWorkshopPreviewResults, sizeof(m_aWorkshopPreviewResults));
		mem_zero(&m_CommunityResult, sizeof(m_CommunityResult));
	}

	virtual bool Init()
	{
		m_pStorage = Kernel()->RequestInterface<IStorage>();
		LoadEventQueue();
		if(m_Initialized)
			return true;
		if(SteamAPI_RestartAppIfNecessary((AppId_t)STEAM_APP_ID))
		{
			dbg_msg("steam", "relaunching AppID %d through Steam", STEAM_APP_ID);
			m_ExitRequested = true;
			return false;
		}
		dbg_msg("steam", "initializing Steam API for AppID %d", STEAM_APP_ID);
		SteamErrMsg aInitError;
		const ESteamAPIInitResult InitResult = SteamAPI_InitEx(&aInitError);
		m_Initialized = InitResult == k_ESteamAPIInitResult_OK;
		if(!m_Initialized)
		{
			dbg_msg("steam", "SteamAPI_Init failed (%d): %s; Steam features are unavailable", (int)InitResult, aInitError[0] ? aInitError : "no detail");
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
		m_WorkshopQueryCall.Cancel();
		if(m_WorkshopQueryHandle != k_UGCQueryHandleInvalid && SteamUGC()) SteamUGC()->ReleaseQueryUGCRequest(m_WorkshopQueryHandle);
		m_WorkshopQueryHandle = k_UGCQueryHandleInvalid; m_WorkshopQueryPending = false; m_WorkshopQueryResultReady = false;
		if(SteamHTTP())
			for(int i = 0; i < m_WorkshopPreviewRequestCount; i++)
				SteamHTTP()->ReleaseHTTPRequest(m_aWorkshopPreviewRequests[i].m_Handle);
		m_WorkshopPreviewRequestCount = 0; m_WorkshopPreviewResultCount = 0;
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
		m_CommunityFoundCall.Cancel(); m_CommunityUploadedCall.Cancel(); m_CommunityDownloadedCall.Cancel(); m_CommunityOperation = 0; m_CommunityResultReady = false;
		m_ActiveLeaderboardEvent = -1;
		m_LobbyCreatePending = false;
		SteamFriends()->ClearRichPresence();
		if(m_CurrentLobbyID && SteamMatchmaking())
			SteamMatchmaking()->LeaveLobby(CSteamID(m_CurrentLobbyID));
		if(m_PartyLobbyID && m_PartyLobbyID != m_CurrentLobbyID && SteamMatchmaking())
			SteamMatchmaking()->LeaveLobby(CSteamID(m_PartyLobbyID));
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
			if(m_LobbyCreatePending && m_LobbyCreateRetryAt && time_get() >= m_LobbyCreateRetryAt)
			{
				m_LobbyCreateRetryAt = 0;
				if(!BeginLobbyCreateCall())
				{
					m_LobbyCreatePending = false;
					if(!m_CreatingParty)
						m_ListenServerStopRequested = true;
				}
			}
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

	virtual void CloudStatus(CPlatformCloudStatus *pStatus) const
	{
		if(!pStatus)
			return;
		mem_zero(pStatus, sizeof(*pStatus));
		pStatus->m_Available = m_Initialized && SteamRemoteStorage();
		if(!pStatus->m_Available)
		{
			str_copy(pStatus->m_aError, "Steam Cloud is unavailable", sizeof(pStatus->m_aError));
			return;
		}
		pStatus->m_AccountEnabled = SteamRemoteStorage()->IsCloudEnabledForAccount();
		pStatus->m_AppEnabled = SteamRemoteStorage()->IsCloudEnabledForApp();
		uint64 Total = 0, Available = 0;
		if(SteamRemoteStorage()->GetQuota(&Total, &Available))
		{
			pStatus->m_BytesTotal = Total;
			pStatus->m_BytesAvailable = Available;
		}
		if(!pStatus->m_AccountEnabled || !pStatus->m_AppEnabled)
			str_copy(pStatus->m_aError, "Steam Cloud is disabled", sizeof(pStatus->m_aError));
	}

	virtual int CloudFileSize(const char *pFilename) const
	{
		if(!m_Initialized || !SteamRemoteStorage() || !pFilename || !pFilename[0])
			return -1;
		return SteamRemoteStorage()->GetFileSize(pFilename);
	}

	virtual long long CloudFileTimestamp(const char *pFilename) const
	{
		if(!m_Initialized || !SteamRemoteStorage() || !pFilename || !pFilename[0])
			return 0;
		return (long long)SteamRemoteStorage()->GetFileTimestamp(pFilename);
	}

	virtual int CloudReadFile(const char *pFilename, void *pBuffer, int BufferSize)
	{
		if(!m_Initialized || !SteamRemoteStorage() || !pFilename || !pBuffer || BufferSize <= 0)
			return -1;
		const int Size = SteamRemoteStorage()->GetFileSize(pFilename);
		if(Size < 0 || Size > BufferSize)
			return -1;
		return SteamRemoteStorage()->FileRead(pFilename, pBuffer, Size);
	}

	virtual bool CloudWriteFile(const char *pFilename, const void *pBuffer, int BufferSize)
	{
		if(!m_Initialized || !SteamRemoteStorage() || !pFilename || !pFilename[0] || !pBuffer || BufferSize < 0)
			return false;
		if(!SteamRemoteStorage()->IsCloudEnabledForAccount() || !SteamRemoteStorage()->IsCloudEnabledForApp())
			return false;
		return SteamRemoteStorage()->FileWrite(pFilename, pBuffer, BufferSize);
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

	virtual void SetRichPresence(const CPlatformPresence &Presence)
	{
		if(!m_Initialized || !SteamFriends())
			return;
		auto SetSafe = [&](const char *pKey, const char *pValue, int MaxLength) { char aSafe[256]; int Out = 0; for(int i = 0; pValue && pValue[i] && Out < MaxLength && Out < (int)sizeof(aSafe) - 1; i++) if((unsigned char)pValue[i] >= 32 && (unsigned char)pValue[i] != 127) aSafe[Out++] = pValue[i]; aSafe[Out] = 0; SteamFriends()->SetRichPresence(pKey, aSafe); };
		static const char *s_apDisplays[] = {"#Status_Menu", "#Status_Party", "#Status_Ready", "#Status_Loading", "#Status_Playing", "#Status_Challenge", "#Status_Spectating", "#Status_Replay"};
		const int State = clamp(Presence.m_State, (int)PLATFORM_PRESENCE_MENU, (int)PLATFORM_PRESENCE_REPLAY);
		SteamFriends()->SetRichPresence("steam_display", s_apDisplays[State]);
		SetSafe("mode", Presence.m_aMode, 63); SetSafe("map", Presence.m_aMap, 127); SetSafe("challenge", Presence.m_aChallenge, 127);
		char aNumber[16]; str_format(aNumber, sizeof(aNumber), "%d", max(0, Presence.m_Floor)); SteamFriends()->SetRichPresence("floor", aNumber);
		str_format(aNumber, sizeof(aNumber), "%d", max(0, Presence.m_RoomPlayers)); SteamFriends()->SetRichPresence("players", aNumber);
		str_format(aNumber, sizeof(aNumber), "%d", max(0, Presence.m_RoomCapacity)); SteamFriends()->SetRichPresence("capacity", aNumber);
		SetSafe("connect", Presence.m_ConnectVerified ? Presence.m_aConnect : "", 255);
		const unsigned long long GroupLobbyID = Presence.m_PlayerGroup ? Presence.m_PlayerGroup : (m_PartyLobbyID ? m_PartyLobbyID : m_CurrentLobbyID);
		if(GroupLobbyID && SteamMatchmaking())
		{
			char aLobbyID[32];
			char aMembers[16];
			str_format(aLobbyID, sizeof(aLobbyID), "%llu", GroupLobbyID);
			str_format(aMembers, sizeof(aMembers), "%d", SteamMatchmaking()->GetNumLobbyMembers(CSteamID(GroupLobbyID)));
			SteamFriends()->SetRichPresence("steam_player_group", aLobbyID);
			SteamFriends()->SetRichPresence("steam_player_group_size", aMembers);
		}
		else
		{
			SteamFriends()->SetRichPresence("steam_player_group", "");
			SteamFriends()->SetRichPresence("steam_player_group_size", "");
		}
	}

	virtual void SetTimelineMode(EPlatformTimelineMode Mode, const char *pDescription)
	{
		if(!m_Initialized || !SteamTimeline()) return;
		ETimelineGameMode SteamMode = k_ETimelineGameMode_Menus;
		if(Mode == PLATFORM_TIMELINE_LOADING) SteamMode = k_ETimelineGameMode_LoadingScreen;
		else if(Mode == PLATFORM_TIMELINE_PARTY) SteamMode = k_ETimelineGameMode_Staging;
		else if(Mode == PLATFORM_TIMELINE_PLAYING || Mode == PLATFORM_TIMELINE_PAUSED || Mode == PLATFORM_TIMELINE_REPLAY) SteamMode = k_ETimelineGameMode_Playing;
		SteamTimeline()->SetTimelineGameMode(SteamMode);
		if(pDescription && pDescription[0]) SteamTimeline()->SetTimelineTooltip(pDescription, 0.0f); else SteamTimeline()->ClearTimelineTooltip(0.0f);
	}

	virtual bool AddTimelineEvent(const CPlatformTimelineEvent &Event)
	{
		if(!m_Initialized || !SteamTimeline()) return false;
		unsigned long long Key = Event.m_SessionID ^ ((unsigned long long)(unsigned)Event.m_ServerTick << 32) ^ ((unsigned long long)(unsigned)Event.m_EventType * 0x9e3779b97f4a7c15ULL);
		for(int i = 0; i < m_TimelineDedupeCount; i++) if(m_aTimelineDedupe[i] == Key) return false;
		m_aTimelineDedupe[m_TimelineDedupeCount < 256 ? m_TimelineDedupeCount++ : Event.m_ServerTick & 255] = Key;
		ETimelineEventClipPriority Priority = Event.m_ClipPriority == PLATFORM_TIMELINE_CLIP_FEATURED ? k_ETimelineEventClipPriority_Featured : Event.m_ClipPriority == PLATFORM_TIMELINE_CLIP_STANDARD ? k_ETimelineEventClipPriority_Standard : k_ETimelineEventClipPriority_None;
		SteamTimeline()->AddInstantaneousTimelineEvent(Event.m_aTitle, Event.m_aDescription, Event.m_aIcon[0] ? Event.m_aIcon : "steam_starburst", Event.m_ClipPriority == PLATFORM_TIMELINE_CLIP_FEATURED ? 900 : 500, 0.0f, Priority);
		return true;
	}

	virtual void SetScreenshotContext(const CPlatformScreenshotContext &Context) { m_ScreenshotContext = Context; }
	virtual bool RegisterScreenshot(const char *pAbsolutePath, int Width, int Height, const CPlatformScreenshotContext &Context)
	{
		if(!m_Initialized || !SteamScreenshots() || !pAbsolutePath || !pAbsolutePath[0] || Width <= 0 || Height <= 0 || !Context.m_SyncToSteam) return false;
		const ScreenshotHandle Handle = SteamScreenshots()->AddScreenshotToLibrary(pAbsolutePath, 0, Width, Height);
		if(Handle == INVALID_SCREENSHOT_HANDLE || m_PendingScreenshotCount >= 16) return false;
		m_aPendingScreenshots[m_PendingScreenshotCount].m_Handle = Handle; m_aPendingScreenshots[m_PendingScreenshotCount].m_Context = Context; m_PendingScreenshotCount++;
		return true;
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

	virtual bool CreateParty()
	{
		if(!m_Initialized || !SteamMatchmaking() || m_PartyLobbyID || m_LobbyCreatePending)
			return false;
		m_CreatingParty = true;
		m_LobbyCreateType = k_ELobbyTypePrivate;
		m_LobbyCreateMaxMembers = 16;
		m_LobbyCreateRetries = 0;
		m_LobbyCreateRetryAt = 0;
		m_aLobbyCreateFailure[0] = 0;
		if(BeginLobbyCreateCall())
			return true;
		m_CreatingParty = false;
		return false;
	}
	virtual bool JoinParty(unsigned long long LobbyID)
	{
		if(!m_Initialized || !SteamMatchmaking() || !LobbyID || m_PartyLobbyID || m_LobbyJoinPending)
			return false;
		m_JoiningParty = true;
		m_LobbyJoinPending = true;
		m_LobbyEnteredCall.Set(SteamMatchmaking()->JoinLobby(CSteamID(LobbyID)), this, &CSteamPlatformServices::OnLobbyEntered);
		return true;
	}
	virtual void LeaveParty()
	{
		CPlatformPartyState State;
		const bool StopHostedGame = PartyState(&State) && State.m_LocalOwner && m_HostedLobbyID;
		if(StopHostedGame)
		{
			m_ListenServerStopRequested = true;
			LeaveLobby();
		}
		if(m_Initialized && m_PartyLobbyID && SteamMatchmaking())
			SteamMatchmaking()->LeaveLobby(CSteamID(m_PartyLobbyID));
		m_PartyLobbyID = 0;
		m_PartyOwnerID = 0;
		m_PendingPartyInviteUserID = 0;
		m_CreatingParty = false;
		m_JoiningParty = false;
		m_OpenPartyInviteAfterCreate = false;
		m_ConsumedPartyLaunchGeneration = 0;
		mem_zero(&m_PendingPartyLaunch, sizeof(m_PendingPartyLaunch));
	}
	virtual unsigned long long PartyLobbyID() const { return m_PartyLobbyID; }
	virtual bool PartyState(CPlatformPartyState *pState) const
	{
		if(!pState)
			return false;
		mem_zero(pState, sizeof(*pState));
		if(!m_Initialized || !m_PartyLobbyID || !SteamMatchmaking())
			return false;
		const CSteamID Lobby(m_PartyLobbyID);
		const CSteamID Owner = SteamMatchmaking()->GetLobbyOwner(Lobby);
		pState->m_LobbyID = m_PartyLobbyID;
		pState->m_OwnerUserID = Owner.ConvertToUint64();
		pState->m_LocalOwner = SteamUser() && Owner == SteamUser()->GetSteamID();
		str_copy(pState->m_aPhase, SteamMatchmaking()->GetLobbyData(Lobby, "party_phase"), sizeof(pState->m_aPhase));
		const char *pType = SteamMatchmaking()->GetLobbyData(Lobby, "target_type");
		pState->m_TargetType = pType && !str_comp(pType, "game_lobby") ? PLATFORM_PARTY_TARGET_GAME_LOBBY : pType && !str_comp(pType, "address") ? PLATFORM_PARTY_TARGET_ADDRESS : PLATFORM_PARTY_TARGET_NONE;
		sscanf(SteamMatchmaking()->GetLobbyData(Lobby, "target_lobby"), "%llu", &pState->m_TargetLobbyID);
		pState->m_TargetRevision = str_toint(SteamMatchmaking()->GetLobbyData(Lobby, "target_revision"));
		pState->m_LaunchGeneration = (unsigned int)str_toint(SteamMatchmaking()->GetLobbyData(Lobby, "launch_generation"));
		str_copy(pState->m_aTargetAddress, SteamMatchmaking()->GetLobbyData(Lobby, "target_address"), sizeof(pState->m_aTargetAddress));
		str_copy(pState->m_aTargetModHash, SteamMatchmaking()->GetLobbyData(Lobby, "target_mod_hash"), sizeof(pState->m_aTargetModHash));
		return true;
	}
	virtual int PartyMemberCount() const
	{
		return m_Initialized && m_PartyLobbyID && SteamMatchmaking() ? SteamMatchmaking()->GetNumLobbyMembers(CSteamID(m_PartyLobbyID)) : 0;
	}
	virtual bool PartyMemberInfo(int Index, CPlatformUserInfo *pInfo) const
	{
		if(Index < 0 || Index >= PartyMemberCount() || !FillUserInfo(SteamMatchmaking()->GetLobbyMemberByIndex(CSteamID(m_PartyLobbyID), Index), false, pInfo))
			return false;
		CPlatformPartyState State;
		PartyState(&State);
		const CSteamID User(pInfo->m_UserID);
		const char *pReady = SteamMatchmaking()->GetLobbyMemberData(CSteamID(m_PartyLobbyID), User, "ready_revision");
		pInfo->m_PartyMember = true;
		pInfo->m_LobbyOwner = State.m_OwnerUserID == pInfo->m_UserID;
		pInfo->m_PartyReadyRevision = str_toint(pReady);
		pInfo->m_PartyReady = State.m_TargetRevision > 0 && pInfo->m_PartyReadyRevision == State.m_TargetRevision;
		return true;
	}
	virtual bool InvitePartyUser(unsigned long long UserID)
	{
		if(!m_Initialized || !UserID || !SteamMatchmaking())
			return false;
		if(m_PartyLobbyID)
		{
			for(int i = 0; i < PartyMemberCount(); i++)
				if(SteamMatchmaking()->GetLobbyMemberByIndex(CSteamID(m_PartyLobbyID), i).ConvertToUint64() == UserID)
					return true;
			return SteamMatchmaking()->InviteUserToLobby(CSteamID(m_PartyLobbyID), CSteamID(UserID));
		}
		m_PendingPartyInviteUserID = UserID;
		if(CreateParty())
			return true;
		m_PendingPartyInviteUserID = 0;
		return false;
	}
	virtual bool OpenPartyInviteDialog()
	{
		if(!m_Initialized || !SteamFriends())
			return false;
		if(!m_PartyLobbyID)
		{
			m_OpenPartyInviteAfterCreate = true;
			if(CreateParty()) return true;
			m_OpenPartyInviteAfterCreate = false;
			return false;
		}
		SteamFriends()->ActivateGameOverlayInviteDialog(CSteamID(m_PartyLobbyID));
		return true;
	}
	virtual bool SetPartyReady(bool Ready)
	{
		CPlatformPartyState State;
		if(!PartyState(&State) || !SteamMatchmaking())
			return false;
		char aRevision[16];
		str_format(aRevision, sizeof(aRevision), "%d", Ready ? State.m_TargetRevision : 0);
		SteamMatchmaking()->SetLobbyMemberData(CSteamID(m_PartyLobbyID), "ready_revision", aRevision);
		return true;
	}
	virtual bool SetPartyTarget(int TargetType, unsigned long long TargetLobbyID, const char *pAddress, const char *pModHash)
	{
		CPlatformPartyState State;
		if(!PartyState(&State) || !State.m_LocalOwner || !SteamMatchmaking() ||
			(TargetType != PLATFORM_PARTY_TARGET_GAME_LOBBY && TargetType != PLATFORM_PARTY_TARGET_ADDRESS) ||
			(TargetType == PLATFORM_PARTY_TARGET_GAME_LOBBY && !TargetLobbyID) ||
			(TargetType == PLATFORM_PARTY_TARGET_ADDRESS && (!pAddress || !pAddress[0])))
			return false;
		if(m_HostedLobbyID && (TargetType != PLATFORM_PARTY_TARGET_GAME_LOBBY || TargetLobbyID != m_HostedLobbyID))
			m_ListenServerStopRequested = true;
		const CSteamID Lobby(m_PartyLobbyID);
		char aNumber[32];
		str_format(aNumber, sizeof(aNumber), "%d", max(1, State.m_TargetRevision + 1));
		SteamMatchmaking()->SetLobbyData(Lobby, "party_phase", "ready_check");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_type", TargetType == PLATFORM_PARTY_TARGET_GAME_LOBBY ? "game_lobby" : "address");
		char aLobby[32]; str_format(aLobby, sizeof(aLobby), "%llu", TargetLobbyID);
		SteamMatchmaking()->SetLobbyData(Lobby, "target_lobby", TargetType == PLATFORM_PARTY_TARGET_GAME_LOBBY ? aLobby : "0");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_address", TargetType == PLATFORM_PARTY_TARGET_ADDRESS ? pAddress : "");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_mod_hash", pModHash && pModHash[0] ? pModHash : "none");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_revision", aNumber);
		SteamMatchmaking()->SetLobbyMemberData(Lobby, "ready_revision", "0");
		return true;
	}
	virtual bool ClearPartyTarget()
	{
		CPlatformPartyState State;
		if(!PartyState(&State) || !State.m_LocalOwner || !SteamMatchmaking())
			return false;
		const CSteamID Lobby(m_PartyLobbyID);
		char aRevision[16]; str_format(aRevision, sizeof(aRevision), "%d", max(1, State.m_TargetRevision + 1));
		SteamMatchmaking()->SetLobbyData(Lobby, "party_phase", "forming");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_type", "none");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_lobby", "0");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_address", "");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_mod_hash", "none");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_revision", aRevision);
		return true;
	}
	virtual bool LaunchParty(bool Force)
	{
		CPlatformPartyState State;
		if(!PartyState(&State) || !State.m_LocalOwner || State.m_TargetType == PLATFORM_PARTY_TARGET_NONE || !SteamMatchmaking())
			return false;
		if(!Force)
			for(int i = 0; i < PartyMemberCount(); i++) { CPlatformUserInfo Info; if(!PartyMemberInfo(i, &Info) || !Info.m_PartyReady) return false; }
		const CSteamID Lobby(m_PartyLobbyID);
		char aOwner[32], aGeneration[16];
		str_format(aOwner, sizeof(aOwner), "%llu", State.m_OwnerUserID);
		str_format(aGeneration, sizeof(aGeneration), "%u", State.m_LaunchGeneration + 1);
		SteamMatchmaking()->SetLobbyData(Lobby, "launch_owner", aOwner);
		SteamMatchmaking()->SetLobbyData(Lobby, "party_phase", "launching");
		return SteamMatchmaking()->SetLobbyData(Lobby, "launch_generation", aGeneration);
	}
	virtual bool ConsumePartyLaunch(CPlatformPartyLaunch *pLaunch)
	{
		if(!pLaunch || !m_PendingPartyLaunch.m_Generation)
			return false;
		*pLaunch = m_PendingPartyLaunch;
		m_ConsumedPartyLaunchGeneration = m_PendingPartyLaunch.m_Generation;
		mem_zero(&m_PendingPartyLaunch, sizeof(m_PendingPartyLaunch));
		return true;
	}
	virtual void PartyOperationStatus(CPlatformOperationStatus *pStatus) const
	{
		if(!pStatus) return;
		mem_zero(pStatus, sizeof(*pStatus));
		pStatus->m_State = m_LobbyCreatePending && m_CreatingParty ? CLIENT_ASYNC_WORKING : m_PartyLobbyID ? CLIENT_ASYNC_SUCCEEDED : m_aLobbyCreateFailure[0] ? CLIENT_ASYNC_FAILED : CLIENT_ASYNC_IDLE;
		pStatus->m_Stage = CLIENT_STAGE_CREATING_ROOM;
		pStatus->m_Progress = m_PartyLobbyID ? 1.0f : 0.25f;
		str_copy(pStatus->m_aErrorKey, m_aLobbyCreateFailure, sizeof(pStatus->m_aErrorKey));
	}

	virtual bool CreateLobby(EPlatformLobbyVisibility Visibility, int MaxMembers, int HostLocalPort)
	{
		if(!m_Initialized || !SteamMatchmaking() || m_CurrentLobbyID || m_LobbyCreatePending || MaxMembers < 1 || MaxMembers > 64)
			return false;
		m_CreatingParty = false;
		m_HostLocalPort = clamp(HostLocalPort, 1024, 65535);
		m_LobbyCreateType = k_ELobbyTypeFriendsOnly;
		if(Visibility == PLATFORM_LOBBY_INVITE_ONLY)
			m_LobbyCreateType = k_ELobbyTypePrivate;
		else if(Visibility == PLATFORM_LOBBY_PUBLIC)
			m_LobbyCreateType = k_ELobbyTypePublic;
		m_LobbyCreateMaxMembers = MaxMembers;
		m_LobbyCreateRetries = 0;
		m_LobbyCreateRetryAt = 0;
		m_aLobbyCreateFailure[0] = 0;
		m_ListenServerStopRequested = false;
		return BeginLobbyCreateCall();
	}
	virtual bool JoinLobby(unsigned long long LobbyID)
	{
		if(!m_Initialized || !SteamMatchmaking() || !LobbyID)
			return false;
		if(LobbyID == m_CurrentLobbyID)
		{
			if(m_HostedLobbyID == m_CurrentLobbyID)
				str_format(m_aPendingJoin, sizeof(m_aPendingJoin), "127.0.0.1:%d", m_HostLocalPort);
			else
			{
				const char *pConnect = SteamMatchmaking()->GetLobbyData(CSteamID(m_CurrentLobbyID), "connect");
				if(pConnect) str_copy(m_aPendingJoin, pConnect, sizeof(m_aPendingJoin));
			}
			return m_aPendingJoin[0] != 0;
		}
		if(m_CurrentLobbyID)
		{
			SteamMatchmaking()->LeaveLobby(CSteamID(m_CurrentLobbyID));
			m_CurrentLobbyID = 0;
			m_PendingLobbyJoinID = 0;
			m_HostedLobbyID = 0;
		}
		m_JoiningParty = false;
		m_LobbyJoinPending = true;
		m_LobbyEnteredCall.Set(SteamMatchmaking()->JoinLobby(CSteamID(LobbyID)), this, &CSteamPlatformServices::OnLobbyEntered);
		return true;
	}
	virtual void LeaveLobby()
	{
		m_LobbyCreatedCall.Cancel();
		m_LobbyEnteredCall.Cancel();
		m_LobbyCreatePending = false;
		m_LobbyCreateRetryAt = 0;
		m_LobbyCreateRetries = 0;
		m_LobbyJoinPending = false;
		if(m_Initialized && m_CurrentLobbyID && SteamMatchmaking())
			SteamMatchmaking()->LeaveLobby(CSteamID(m_CurrentLobbyID));
		m_CurrentLobbyID = 0;
		m_PendingLobbyJoinID = 0;
		m_HostedLobbyID = 0;
		m_HostLocalPort = 0;
		m_aPendingJoin[0] = 0;
		m_aJoinFailure[0] = 0;
		m_aLobbyCreateFailure[0] = 0;
		if(SteamFriends())
		{
			SteamFriends()->SetRichPresence("connect", "");
			if(!m_PartyLobbyID)
			{
				SteamFriends()->SetRichPresence("steam_player_group", "");
				SteamFriends()->SetRichPresence("steam_player_group_size", "");
			}
		}
	}
	virtual unsigned long long CurrentLobbyID() const { return m_CurrentLobbyID; }
	virtual unsigned long long GameLobbyID() const { return m_CurrentLobbyID; }
	virtual void LeaveGameLobby() { LeaveLobby(); }
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
	bool FillUserInfo(CSteamID User, bool LobbyMember, CPlatformUserInfo *pInfo) const
	{
		if(!pInfo || !m_Initialized || !User.IsValid() || !SteamFriends())
			return false;
		mem_zero(pInfo, sizeof(*pInfo));
		pInfo->m_UserID = User.ConvertToUint64();
		pInfo->m_Local = SteamUser() && User == SteamUser()->GetSteamID();
		pInfo->m_Friend = SteamFriends()->HasFriend(User, k_EFriendFlagImmediate);
		pInfo->m_PersonaState = pInfo->m_Local ? (int)SteamFriends()->GetPersonaState() : (int)SteamFriends()->GetFriendPersonaState(User);
		str_copy(pInfo->m_aName, pInfo->m_Local ? SteamFriends()->GetPersonaName() : SteamFriends()->GetFriendPersonaName(User), sizeof(pInfo->m_aName));
		FriendGameInfo_t GameInfo;
		mem_zero(&GameInfo, sizeof(GameInfo));
		if(SteamFriends()->GetFriendGamePlayed(User, &GameInfo) && GameInfo.m_gameID.AppID() == (AppId_t)STEAM_APP_ID)
		{
			pInfo->m_PlayingThisGame = true;
			if(GameInfo.m_steamIDLobby.IsValid())
				pInfo->m_LobbyID = GameInfo.m_steamIDLobby.ConvertToUint64();
		}
		const char *pConnect = SteamFriends()->GetFriendRichPresence(User, "connect");
		if(pConnect)
			str_copy(pInfo->m_aConnect, pConnect, sizeof(pInfo->m_aConnect));
		pInfo->m_Joinable = pInfo->m_PlayingThisGame && (pInfo->m_LobbyID || pInfo->m_aConnect[0]);
		if(LobbyMember && m_CurrentLobbyID && SteamMatchmaking())
		{
			pInfo->m_LobbyID = m_CurrentLobbyID;
			pInfo->m_PlayingThisGame = true;
			pInfo->m_LobbyOwner = SteamMatchmaking()->GetLobbyOwner(CSteamID(m_CurrentLobbyID)) == User;
		}
		if(m_PartyLobbyID && SteamMatchmaking())
		{
			for(int i = 0; i < SteamMatchmaking()->GetNumLobbyMembers(CSteamID(m_PartyLobbyID)); i++)
				if(SteamMatchmaking()->GetLobbyMemberByIndex(CSteamID(m_PartyLobbyID), i) == User)
				{
					pInfo->m_PartyMember = true;
					break;
				}
		}
		return true;
	}
	virtual int FriendCount() const
	{
		return m_Initialized && SteamFriends() ? SteamFriends()->GetFriendCount(k_EFriendFlagImmediate) : 0;
	}
	virtual bool FriendInfo(int Index, CPlatformUserInfo *pInfo) const
	{
		if(Index < 0 || Index >= FriendCount())
			return false;
		return FillUserInfo(SteamFriends()->GetFriendByIndex(Index, k_EFriendFlagImmediate), false, pInfo);
	}
	virtual bool UserInfo(unsigned long long UserID, CPlatformUserInfo *pInfo) const
	{
		return UserID != 0 && FillUserInfo(CSteamID(UserID), false, pInfo);
	}
	virtual int LobbyMemberCount() const
	{
		return m_Initialized && m_CurrentLobbyID && SteamMatchmaking() ? SteamMatchmaking()->GetNumLobbyMembers(CSteamID(m_CurrentLobbyID)) : 0;
	}
	virtual bool LobbyMemberInfo(int Index, CPlatformUserInfo *pInfo) const
	{
		if(Index < 0 || Index >= LobbyMemberCount())
			return false;
		return FillUserInfo(SteamMatchmaking()->GetLobbyMemberByIndex(CSteamID(m_CurrentLobbyID), Index), true, pInfo);
	}
	virtual bool InviteUser(unsigned long long UserID, const char *pConnect)
	{
		if(!m_Initialized || !UserID)
			return false;
		const CSteamID User(UserID);
		if(m_CurrentLobbyID && SteamMatchmaking())
			return SteamMatchmaking()->InviteUserToLobby(CSteamID(m_CurrentLobbyID), User);
		return pConnect && pConnect[0] && SteamFriends() && SteamFriends()->InviteUserToGame(User, pConnect);
	}
	virtual bool JoinUser(unsigned long long UserID)
	{
		if(!m_Initialized || !UserID || !SteamFriends())
			return false;
		CPlatformUserInfo Info;
		if(!FillUserInfo(CSteamID(UserID), false, &Info) || !Info.m_Joinable)
			return false;
		if(Info.m_LobbyID)
			return JoinLobby(Info.m_LobbyID);
		str_copy(m_aPendingJoin, Info.m_aConnect, sizeof(m_aPendingJoin));
		return m_aPendingJoin[0] != 0;
	}
	virtual bool OpenUserProfile(unsigned long long UserID)
	{
		if(!m_Initialized || !UserID || !SteamFriends())
			return false;
		SteamFriends()->ActivateGameOverlayToUser("steamid", CSteamID(UserID));
		return true;
	}
	virtual void SetPlayedWith(unsigned long long UserID)
	{
		if(m_Initialized && UserID && SteamFriends() && UserID != LocalUserID())
			SteamFriends()->SetPlayedWith(CSteamID(UserID));
	}
	virtual int UserAvatarRGBA(unsigned long long UserID, int PreferredSize, void *pBuffer, int BufferSize, int *pWidth, int *pHeight)
	{
		if(pWidth) *pWidth = 0;
		if(pHeight) *pHeight = 0;
		if(!m_Initialized || !UserID || !pBuffer || !SteamFriends() || !SteamUtils())
			return -1;
		const CSteamID User(UserID);
		SteamFriends()->RequestUserInformation(User, true);
		const int Image = PreferredSize >= 64 ? SteamFriends()->GetMediumFriendAvatar(User) : SteamFriends()->GetSmallFriendAvatar(User);
		if(Image == -1)
			return 0;
		uint32 Width = 0, Height = 0;
		if(Image <= 0 || !SteamUtils()->GetImageSize(Image, &Width, &Height) || Width == 0 || Height == 0 || BufferSize < (int)(Width * Height * 4))
			return -1;
		if(!SteamUtils()->GetImageRGBA(Image, (uint8 *)pBuffer, Width * Height * 4))
			return -1;
		if(pWidth) *pWidth = (int)Width;
		if(pHeight) *pHeight = (int)Height;
		return 1;
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
		else if(m_aLobbyCreateFailure[0])
		{
			pStatus->m_State = CLIENT_ASYNC_FAILED;
			pStatus->m_Stage = CLIENT_STAGE_CREATING_ROOM;
			str_copy(pStatus->m_aErrorKey, m_aLobbyCreateFailure, sizeof(pStatus->m_aErrorKey));
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
			char aID[32];str_format(aID,sizeof(aID),"%llu",(unsigned long long)aIDs[i]);CContentManifest Manifest;
			if(!aWorkshopRoot[0] || !ContentPackageStage(aSteamInstallPath,aWorkshopRoot,aID,GAME_NETVERSION,&Manifest,Item.m_aInstallPath,sizeof(Item.m_aInstallPath),Item.m_aError,sizeof(Item.m_aError)))continue;
			Item.m_Valid=true; Item.m_ContentType=Manifest.m_ContentType; str_copy(Item.m_aName,Manifest.m_aName,sizeof(Item.m_aName)); str_copy(Item.m_aDescription,Manifest.m_aDescription,sizeof(Item.m_aDescription)); str_copy(Item.m_aAuthor,Manifest.m_aAuthor,sizeof(Item.m_aAuthor)); str_copy(Item.m_aVersion,Manifest.m_aVersion,sizeof(Item.m_aVersion)); str_copy(Item.m_aTargetProtocol,Manifest.m_aTargetProtocol,sizeof(Item.m_aTargetProtocol)); str_copy(Item.m_aContentHash,Manifest.m_aContentHash,sizeof(Item.m_aContentHash)); str_copy(Item.m_aContentRating,Manifest.m_aContentRating,sizeof(Item.m_aContentRating));
			if(Manifest.m_ContentType==CONTENT_TYPE_MOD && !m_ModCollection.AddManifest(Manifest,Item.m_aInstallPath,Item.m_aError,sizeof(Item.m_aError)))Item.m_Valid=false;
		}
		const char *apRoots[64];char aaRoots[64][32];int RootCount=0;const char *p=g_Config.m_ClModIds;
		while(*p&&RootCount<64){int N=0;while(p[N]&&p[N]!=','&&N<31){aaRoots[RootCount][N]=p[N];N++;}aaRoots[RootCount][N]=0;if(N){apRoots[RootCount]=aaRoots[RootCount];RootCount++;}p+=N;if(*p==',')p++;else if(*p)break;}
		if(!RootCount) g_Config.m_ClModHash[0]=0;
		else {int aOrder[64],OrderCount=0;char aError[256];if(!m_ModCollection.Resolve(apRoots,RootCount,aOrder,&OrderCount,g_Config.m_ClModHash,aError,sizeof(aError)))g_Config.m_ClModHash[0]=0;}
		if(Count > 0 && !m_WorkshopQueryPending)
		{
			m_WorkshopQueryHandle = SteamUGC()->CreateQueryUGCDetailsRequest(aIDs, min(Count, (uint32)50));
			bool QueryStarted = false;
			if(m_WorkshopQueryHandle != k_UGCQueryHandleInvalid && SteamUGC()->SetReturnMetadata(m_WorkshopQueryHandle, true) && SteamUGC()->SetReturnLongDescription(m_WorkshopQueryHandle, true))
			{
				const SteamAPICall_t Call = SteamUGC()->SendQueryUGCRequest(m_WorkshopQueryHandle);
				if(Call != k_uAPICallInvalid) { mem_zero(&m_WorkshopQueryResult, sizeof(m_WorkshopQueryResult)); m_WorkshopQueryResult.m_OperationID = ++m_WorkshopQueryOperationID; m_WorkshopQueryPending = true; m_WorkshopQueryResultReady = false; m_WorkshopQueryCall.Set(Call, this, &CSteamPlatformServices::OnWorkshopQuery); QueryStarted = true; }
			}
			if(!QueryStarted && m_WorkshopQueryHandle != k_UGCQueryHandleInvalid) { SteamUGC()->ReleaseQueryUGCRequest(m_WorkshopQueryHandle); m_WorkshopQueryHandle = k_UGCQueryHandleInvalid; }
		}
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
	virtual unsigned QueryWorkshop(const CPlatformWorkshopQuery &Query)
	{
		if(!m_Initialized || !SteamUGC() || Query.m_Page < 1 || Query.m_ContentType < -1 || Query.m_ContentType > 3) return 0;
		if(m_WorkshopQueryPending) m_WorkshopQueryCall.Cancel();
		if(m_WorkshopQueryHandle != k_UGCQueryHandleInvalid) SteamUGC()->ReleaseQueryUGCRequest(m_WorkshopQueryHandle);
		m_WorkshopQueryPending = false;
		EUGCQuery Sort = k_EUGCQuery_RankedByLastUpdatedDate;
		if(Query.m_Sort == PLATFORM_WORKSHOP_POPULAR) Sort = k_EUGCQuery_RankedByTrend;
		else if(Query.m_Sort == PLATFORM_WORKSHOP_RATING) Sort = k_EUGCQuery_RankedByVote;
		else if(Query.m_Sort == PLATFORM_WORKSHOP_SUBSCRIPTIONS) Sort = k_EUGCQuery_RankedByTotalUniqueSubscriptions;
		else if(Query.m_aSearch[0]) Sort = k_EUGCQuery_RankedByTextSearch;
		m_WorkshopQueryHandle = SteamUGC()->CreateQueryAllUGCRequest(Sort, k_EUGCMatchingUGCType_Items, (AppId_t)STEAM_APP_ID, (AppId_t)STEAM_APP_ID, (uint32)Query.m_Page);
		if(m_WorkshopQueryHandle == k_UGCQueryHandleInvalid) return 0;
		static const char *s_apTags[] = {"Mods", "Maps", "Room Presets", "Challenges"};
		bool Ok = SteamUGC()->SetReturnMetadata(m_WorkshopQueryHandle, true) && SteamUGC()->SetReturnLongDescription(m_WorkshopQueryHandle, true);
		if(Ok && Query.m_ContentType >= 0) Ok = SteamUGC()->AddRequiredTag(m_WorkshopQueryHandle, s_apTags[Query.m_ContentType]);
		if(Ok && Query.m_aSearch[0]) Ok = SteamUGC()->SetSearchText(m_WorkshopQueryHandle, Query.m_aSearch);
		if(Ok && Query.m_Sort == PLATFORM_WORKSHOP_POPULAR) Ok = SteamUGC()->SetRankedByTrendDays(m_WorkshopQueryHandle, 30);
		if(!Ok) { SteamUGC()->ReleaseQueryUGCRequest(m_WorkshopQueryHandle); m_WorkshopQueryHandle = k_UGCQueryHandleInvalid; return 0; }
		const SteamAPICall_t Call = SteamUGC()->SendQueryUGCRequest(m_WorkshopQueryHandle); if(Call == k_uAPICallInvalid) { SteamUGC()->ReleaseQueryUGCRequest(m_WorkshopQueryHandle); m_WorkshopQueryHandle = k_UGCQueryHandleInvalid; return 0; }
		mem_zero(&m_WorkshopQueryResult, sizeof(m_WorkshopQueryResult)); m_WorkshopQueryResult.m_OperationID = ++m_WorkshopQueryOperationID; m_WorkshopQueryResult.m_Page = Query.m_Page; m_WorkshopQueryPending = true; m_WorkshopQueryResultReady = false; m_WorkshopQueryCall.Set(Call, this, &CSteamPlatformServices::OnWorkshopQuery); return m_WorkshopQueryOperationID;
	}
	virtual bool ConsumeWorkshopQueryResult(CPlatformWorkshopQueryResult *pResult) { if(!pResult || !m_WorkshopQueryResultReady) return false; *pResult = m_WorkshopQueryResult; m_WorkshopQueryResultReady = false; return true; }
	virtual int WorkshopQueryItemCount() const { return m_WorkshopQueryItemCount; }
	virtual bool WorkshopQueryItem(int Index,CPlatformWorkshopItem *pItem) const { if(!pItem || Index < 0 || Index >= m_WorkshopQueryItemCount) return false; *pItem = m_aWorkshopQueryItems[Index]; return true; }
	virtual unsigned RequestWorkshopPreview(unsigned long long PublishedFileID)
	{
		if(!m_Initialized || !SteamHTTP() || !m_pStorage || !PublishedFileID || m_WorkshopPreviewRequestCount >= 4)
			return 0;
		const CPlatformWorkshopItem *pItem = 0;
		for(int i = 0; i < m_WorkshopQueryItemCount && !pItem; i++) if(m_aWorkshopQueryItems[i].m_PublishedFileID == PublishedFileID) pItem = &m_aWorkshopQueryItems[i];
		for(int i = 0; i < m_WorkshopItemCount && !pItem; i++) if(m_aWorkshopItems[i].m_PublishedFileID == PublishedFileID) pItem = &m_aWorkshopItems[i];
		if(!pItem || !pItem->m_aPreviewURL[0] || str_comp_num(pItem->m_aPreviewURL, "https://", 8)) return 0;
		for(int i = 0; i < m_WorkshopPreviewRequestCount; i++) if(m_aWorkshopPreviewRequests[i].m_PublishedFileID == PublishedFileID) return m_aWorkshopPreviewRequests[i].m_OperationID;
		const unsigned OperationID = ++m_WorkshopPreviewOperationID;
		m_pStorage->CreateFolder("workshop_cache", IStorage::TYPE_SAVE);
		m_pStorage->CreateFolder("workshop_cache/previews", IStorage::TYPE_SAVE);
		for(int Format = 0; Format < 2; Format++)
		{
			char aPath[256]; str_format(aPath, sizeof(aPath), "workshop_cache/previews/%llu_%u.%s", PublishedFileID, pItem->m_UpdatedAt, Format ? "jpg" : "png");
			IOHANDLE File = m_pStorage->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE);
			if(File)
			{
				io_close(File);
				if(m_WorkshopPreviewResultCount < 16)
				{
					CPlatformWorkshopPreviewResult &Result = m_aWorkshopPreviewResults[m_WorkshopPreviewResultCount++]; mem_zero(&Result, sizeof(Result));
					Result.m_OperationID = OperationID; Result.m_PublishedFileID = PublishedFileID; Result.m_UpdatedAt = pItem->m_UpdatedAt; Result.m_Succeeded = true; str_copy(Result.m_aCachePath, aPath, sizeof(Result.m_aCachePath));
				}
				return OperationID;
			}
		}
		const HTTPRequestHandle Handle = SteamHTTP()->CreateHTTPRequest(k_EHTTPMethodGET, pItem->m_aPreviewURL);
		if(Handle == INVALID_HTTPREQUEST_HANDLE) return 0;
		SteamAPICall_t Call = k_uAPICallInvalid;
		if(!SteamHTTP()->SetHTTPRequestContextValue(Handle, OperationID) || !SteamHTTP()->SetHTTPRequestNetworkActivityTimeout(Handle, 15) || !SteamHTTP()->SendHTTPRequest(Handle, &Call))
		{
			SteamHTTP()->ReleaseHTTPRequest(Handle); return 0;
		}
		CWorkshopPreviewRequest &Request = m_aWorkshopPreviewRequests[m_WorkshopPreviewRequestCount++]; Request.m_Handle = Handle; Request.m_OperationID = OperationID; Request.m_PublishedFileID = PublishedFileID; Request.m_UpdatedAt = pItem->m_UpdatedAt;
		return OperationID;
	}
	virtual bool ConsumeWorkshopPreviewResult(CPlatformWorkshopPreviewResult *pResult)
	{
		if(!pResult || m_WorkshopPreviewResultCount <= 0) return false;
		*pResult = m_aWorkshopPreviewResults[0];
		for(int i = 1; i < m_WorkshopPreviewResultCount; i++) m_aWorkshopPreviewResults[i - 1] = m_aWorkshopPreviewResults[i];
		m_WorkshopPreviewResultCount--; return true;
	}
	virtual bool RequestWorkshopDownload(unsigned long long PublishedFileID) { return m_Initialized && SteamUGC() && PublishedFileID && SteamUGC()->DownloadItem((PublishedFileId_t)PublishedFileID, true); }
	virtual bool UserDisplayName(unsigned long long UserID, char *pBuffer, int BufferSize)
	{
		if(!pBuffer || BufferSize <= 0) return false;
		pBuffer[0] = 0;
		if(!m_Initialized || !SteamFriends() || !UserID) return false;
		const CSteamID User(UserID); const char *pName = SteamUser() && User == SteamUser()->GetSteamID() ? SteamFriends()->GetPersonaName() : SteamFriends()->GetFriendPersonaName(User);
		if(pName && pName[0] && str_comp(pName, "[unknown]") != 0) { str_copy(pBuffer, pName, BufferSize); return true; }
		SteamFriends()->RequestUserInformation(User, true); return false;
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
		char aID[32],aError[256];str_format(aID,sizeof(aID),"%llu",ID);CContentManifest Manifest;
		if(!ContentPackageValidate(pContentRoot,aID,GAME_NETVERSION,&Manifest,aError,sizeof(aError))){mem_zero(&m_WorkshopPublish,sizeof(m_WorkshopPublish));str_copy(m_WorkshopPublish.m_aStatus,aError,sizeof(m_WorkshopPublish.m_aStatus));return false;}
		m_WorkshopUpdateHandle=SteamUGC()->StartItemUpdate((AppId_t)STEAM_APP_ID,(PublishedFileId_t)ID);
		if(m_WorkshopUpdateHandle==k_UGCUpdateHandleInvalid)return false;
		char aMetadata[384];str_format(aMetadata,sizeof(aMetadata),"schema=1;type=%s;version=%s;protocol=%s;hash=%s;rating=%s",ContentTypeName(Manifest.m_ContentType),Manifest.m_aVersion,Manifest.m_aTargetProtocol,Manifest.m_aContentHash,Manifest.m_aContentRating);
		bool Ok=SteamUGC()->SetItemTitle(m_WorkshopUpdateHandle,Manifest.m_aName)&&SteamUGC()->SetItemDescription(m_WorkshopUpdateHandle,Manifest.m_aDescription)&&SteamUGC()->SetItemMetadata(m_WorkshopUpdateHandle,aMetadata)&&SteamUGC()->SetItemContent(m_WorkshopUpdateHandle,pContentRoot);
		if(Ok&&pPreviewFile&&pPreviewFile[0])Ok=SteamUGC()->SetItemPreview(m_WorkshopUpdateHandle,pPreviewFile);
		const char *apTags[8];uint32 TagCount=0; static const char *s_apTypeTags[] = {"Mods", "Maps", "Room Presets", "Challenges"}; apTags[TagCount++]=s_apTypeTags[Manifest.m_ContentType]; if(Manifest.m_ContentType==CONTENT_TYPE_MOD&&Manifest.m_Api.m_Capabilities&MOD_CAPABILITY_GAMEPLAY_RULES)apTags[TagCount++]="Gameplay";if(Manifest.m_ContentType==CONTENT_TYPE_MOD&&Manifest.m_Api.m_Capabilities&MOD_CAPABILITY_WEAPONS)apTags[TagCount++]="Weapons";if(Manifest.m_ContentType==CONTENT_TYPE_MOD&&Manifest.m_Api.m_Capabilities&MOD_CAPABILITY_ITEMS)apTags[TagCount++]="Items";if(Manifest.m_Api.m_Capabilities&MOD_CAPABILITY_RESOURCES)apTags[TagCount++]="Resources";apTags[TagCount++]=Manifest.m_aContentRating;
		SteamParamStringArray_t Tags={apTags,(int32)TagCount};if(Ok&&TagCount)Ok=SteamUGC()->SetItemTags(m_WorkshopUpdateHandle,&Tags,false);
		if(!Ok){m_WorkshopUpdateHandle=k_UGCUpdateHandleInvalid;return false;}
		mem_zero(&m_WorkshopPublish,sizeof(m_WorkshopPublish));m_WorkshopPublish.m_Active=true;m_WorkshopPublish.m_PublishedFileID=ID;str_copy(m_WorkshopPublish.m_aStatus,"submitting Workshop update",sizeof(m_WorkshopPublish.m_aStatus));
		const SteamAPICall_t Call=SteamUGC()->SubmitItemUpdate(m_WorkshopUpdateHandle,"Ninslash content update");if(Call==k_uAPICallInvalid){m_WorkshopPublish.m_Active=false;return false;}m_WorkshopSubmittedCall.Set(Call,this,&CSteamPlatformServices::OnWorkshopSubmitted);return true;
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
	virtual unsigned SubmitCommunityChallenge(unsigned long long PublishedFileID,int Revision,int Metric,int Score)
	{
		if(!m_Initialized || !SteamUserStats() || m_CommunityOperation || !PublishedFileID || Revision <= 0 || Metric < 0 || Metric > 1 || Score < 0) return 0;
		CCommunityChallengeDescriptor Descriptor; mem_zero(&Descriptor, sizeof(Descriptor)); Descriptor.m_PublishedFileID = PublishedFileID; Descriptor.m_Revision = Revision; Descriptor.m_Metric = Metric; mem_zero(&m_CommunityResult, sizeof(m_CommunityResult)); if(!CommunityChallengeLeaderboardName(Descriptor, m_CommunityResult.m_aName, sizeof(m_CommunityResult.m_aName))) return 0;
		m_CommunityResult.m_OperationID = ++m_CommunityOperationID; m_CommunityResult.m_Upload = true; m_CommunityScore = Score; m_CommunityOperation = 1; m_CommunityResultReady = false; const ELeaderboardSortMethod Sort = Metric == COMMUNITY_CHALLENGE_CLEAR_TIME_MS ? k_ELeaderboardSortMethodAscending : k_ELeaderboardSortMethodDescending; const ELeaderboardDisplayType Display = Metric == COMMUNITY_CHALLENGE_CLEAR_TIME_MS ? k_ELeaderboardDisplayTypeTimeMilliSeconds : k_ELeaderboardDisplayTypeNumeric; m_CommunityFoundCall.Set(SteamUserStats()->FindOrCreateLeaderboard(m_CommunityResult.m_aName, Sort, Display), this, &CSteamPlatformServices::OnCommunityFound); return m_CommunityOperationID;
	}
	virtual unsigned QueryCommunityChallenge(unsigned long long PublishedFileID,int Revision,int Metric,EPlatformLeaderboardScope Scope)
	{
		if(!m_Initialized || !SteamUserStats() || m_CommunityOperation || !PublishedFileID || Revision <= 0 || Metric < 0 || Metric > 1 || Scope < PLATFORM_LEADERBOARD_GLOBAL || Scope > PLATFORM_LEADERBOARD_AROUND_ME) return 0;
		CCommunityChallengeDescriptor Descriptor; mem_zero(&Descriptor, sizeof(Descriptor)); Descriptor.m_PublishedFileID = PublishedFileID; Descriptor.m_Revision = Revision; Descriptor.m_Metric = Metric; mem_zero(&m_CommunityResult, sizeof(m_CommunityResult)); if(!CommunityChallengeLeaderboardName(Descriptor, m_CommunityResult.m_aName, sizeof(m_CommunityResult.m_aName))) return 0;
		m_CommunityResult.m_OperationID = ++m_CommunityOperationID; m_CommunityScope = Scope; m_CommunityOperation = 2; m_CommunityResultReady = false; m_CommunityEntryCount = 0; const ELeaderboardSortMethod Sort = Metric == COMMUNITY_CHALLENGE_CLEAR_TIME_MS ? k_ELeaderboardSortMethodAscending : k_ELeaderboardSortMethodDescending; const ELeaderboardDisplayType Display = Metric == COMMUNITY_CHALLENGE_CLEAR_TIME_MS ? k_ELeaderboardDisplayTypeTimeMilliSeconds : k_ELeaderboardDisplayTypeNumeric; m_CommunityFoundCall.Set(SteamUserStats()->FindOrCreateLeaderboard(m_CommunityResult.m_aName, Sort, Display), this, &CSteamPlatformServices::OnCommunityFound); return m_CommunityOperationID;
	}
	virtual bool ConsumeCommunityLeaderboardResult(CPlatformLeaderboardResult *pResult) { if(!pResult || !m_CommunityResultReady) return false; *pResult = m_CommunityResult; m_CommunityResultReady = false; return true; }
	virtual int CommunityLeaderboardEntryCount() const { return m_CommunityEntryCount; }
	virtual bool CommunityLeaderboardEntry(int Index,CPlatformLeaderboardEntry *pEntry) const { if(!pEntry || Index < 0 || Index >= m_CommunityEntryCount) return false; *pEntry = m_aCommunityEntries[Index]; return true; }
	virtual bool SteamInputActive() const { return m_SteamInputInitialized; }
	virtual void SetInputActionSet(EPlatformInputActionSet ActionSet)
	{
		if(ActionSet >= PLATFORM_INPUT_GAME && ActionSet < NUM_PLATFORM_INPUT_ACTION_SETS)
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
		const InputMotionData_t Motion = SteamInput()->GetMotionData(Controller);
		pState->m_GyroX = Motion.rotVelY; pState->m_GyroY = Motion.rotVelX;
		return true;
	}
	virtual bool InputGlyph(EPlatformInputAction Action, char *pBuffer, int BufferSize)
	{
		if(!pBuffer || BufferSize <= 0 || Action < 0 || Action >= NUM_PLATFORM_INPUT_ACTIONS || !m_SteamInputInitialized || !SteamInput()) return false;
		if(!m_aaInputGlyphs[Action][0])
		{
			InputHandle_t aControllers[STEAM_INPUT_MAX_COUNT]; if(SteamInput()->GetConnectedControllers(aControllers) <= 0) return false;
			EInputActionOrigin aOrigins[STEAM_INPUT_MAX_ORIGINS];
			if(SteamInput()->GetDigitalActionOrigins(aControllers[0], m_aInputActionSets[m_InputActionSet], m_aDigitalActions[Action], aOrigins) <= 0) return false;
			const char *pGlyph = SteamInput()->GetGlyphPNGForActionOrigin(aOrigins[0], k_ESteamInputGlyphSize_Medium, 0);
			if(!pGlyph || !pGlyph[0]) return false;
			str_copy(m_aaInputGlyphs[Action], pGlyph, sizeof(m_aaInputGlyphs[Action]));
		}
		str_copy(pBuffer, m_aaInputGlyphs[Action], BufferSize); return true;
	}
	virtual void TriggerInputVibration(unsigned short LeftSpeed, unsigned short RightSpeed)
	{
		if(!m_SteamInputInitialized || !SteamInput()) return;
		InputHandle_t aControllers[STEAM_INPUT_MAX_COUNT]; const int Count = SteamInput()->GetConnectedControllers(aControllers); for(int i = 0; i < Count; i++) SteamInput()->TriggerVibration(aControllers[i], LeftSpeed, RightSpeed);
	}
	virtual bool OpenInputConfiguration()
	{
		if(!m_SteamInputInitialized || !SteamInput()) return false;
		InputHandle_t aControllers[STEAM_INPUT_MAX_COUNT]; if(SteamInput()->GetConnectedControllers(aControllers) <= 0) return false; return SteamInput()->ShowBindingPanel(aControllers[0]);
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

void CSteamPlatformServices::OnCommunityFound(LeaderboardFindResult_t *pResult, bool IOError)
{
	if(IOError || !pResult || !pResult->m_bLeaderboardFound || !pResult->m_hSteamLeaderboard || !SteamUserStats()) { str_copy(m_CommunityResult.m_aError, "Community leaderboard is unavailable", sizeof(m_CommunityResult.m_aError)); m_CommunityOperation = 0; m_CommunityResultReady = true; return; }
	if(m_CommunityOperation == 1) m_CommunityUploadedCall.Set(SteamUserStats()->UploadLeaderboardScore(pResult->m_hSteamLeaderboard, k_ELeaderboardUploadScoreMethodKeepBest, m_CommunityScore, 0, 0), this, &CSteamPlatformServices::OnCommunityUploaded);
	else if(m_CommunityOperation == 2)
	{
		ELeaderboardDataRequest Request = m_CommunityScope == PLATFORM_LEADERBOARD_FRIENDS ? k_ELeaderboardDataRequestFriends : m_CommunityScope == PLATFORM_LEADERBOARD_AROUND_ME ? k_ELeaderboardDataRequestGlobalAroundUser : k_ELeaderboardDataRequestGlobal; const int Start = m_CommunityScope == PLATFORM_LEADERBOARD_AROUND_ME ? -10 : 1; const int End = m_CommunityScope == PLATFORM_LEADERBOARD_AROUND_ME ? 10 : 100; m_CommunityDownloadedCall.Set(SteamUserStats()->DownloadLeaderboardEntries(pResult->m_hSteamLeaderboard, Request, Start, End), this, &CSteamPlatformServices::OnCommunityDownloaded);
	}
}

void CSteamPlatformServices::OnCommunityUploaded(LeaderboardScoreUploaded_t *pResult, bool IOError)
{
	m_CommunityResult.m_Succeeded = !IOError && pResult && pResult->m_bSuccess; if(!m_CommunityResult.m_Succeeded) str_copy(m_CommunityResult.m_aError, "Community score upload failed", sizeof(m_CommunityResult.m_aError)); m_CommunityOperation = 0; m_CommunityResultReady = true;
}

void CSteamPlatformServices::OnCommunityDownloaded(LeaderboardScoresDownloaded_t *pResult, bool IOError)
{
	m_CommunityEntryCount = 0;
	if(!IOError && pResult && SteamUserStats()) for(int i = 0; i < pResult->m_cEntryCount && m_CommunityEntryCount < 100; i++) { LeaderboardEntry_t SteamEntry; if(!SteamUserStats()->GetDownloadedLeaderboardEntry(pResult->m_hSteamLeaderboardEntries, i, &SteamEntry, 0, 0)) continue; CPlatformLeaderboardEntry &Entry = m_aCommunityEntries[m_CommunityEntryCount++]; mem_zero(&Entry, sizeof(Entry)); Entry.m_UserID = SteamEntry.m_steamIDUser.ConvertToUint64(); Entry.m_Rank = SteamEntry.m_nGlobalRank; Entry.m_Score = SteamEntry.m_nScore; if(SteamFriends()) str_copy(Entry.m_aName, SteamFriends()->GetFriendPersonaName(SteamEntry.m_steamIDUser), sizeof(Entry.m_aName)); }
	m_CommunityResult.m_Succeeded = !IOError && pResult; m_CommunityResult.m_EntryCount = m_CommunityEntryCount; if(!m_CommunityResult.m_Succeeded) str_copy(m_CommunityResult.m_aError, "Community leaderboard download failed", sizeof(m_CommunityResult.m_aError)); m_CommunityOperation = 0; m_CommunityResultReady = true;
}

void CSteamPlatformServices::OnJoinRequested(GameRichPresenceJoinRequested_t *pRequest)
{
	str_copy(m_aPendingJoin, pRequest->m_rgchConnect, sizeof(m_aPendingJoin));
}

void CSteamPlatformServices::OnLobbyJoinRequested(GameLobbyJoinRequested_t *pRequest)
{
	if(!pRequest || !SteamMatchmaking())
		return;
	const CSteamID Lobby = pRequest->m_steamIDLobby;
	const char *pRoomType = SteamMatchmaking()->GetLobbyData(Lobby, "room_type");
	if(pRoomType && str_comp(pRoomType, "party") == 0)
	{
		if(m_PartyLobbyID && m_PartyLobbyID != Lobby.ConvertToUint64())
			LeaveParty();
		JoinParty(Lobby.ConvertToUint64());
	}
	else
		JoinLobby(Lobby.ConvertToUint64());
}

void CSteamPlatformServices::OnLobbyMembersChanged(LobbyChatUpdate_t *pUpdate)
{
	if(!pUpdate || !SteamFriends() || !SteamMatchmaking())
		return;
	if(pUpdate->m_ulSteamIDLobby == m_PartyLobbyID)
	{
		const unsigned long long NewOwner = SteamMatchmaking()->GetLobbyOwner(CSteamID(m_PartyLobbyID)).ConvertToUint64();
		const bool WasOwner = m_PartyOwnerID == LocalUserID();
		m_PartyOwnerID = NewOwner;
		if(WasOwner && NewOwner != LocalUserID() && m_HostedLobbyID)
		{
			m_ListenServerStopRequested = true;
			LeaveLobby();
		}
		else if(!WasOwner && NewOwner == LocalUserID())
			ClearPartyTarget();
		char aMembers[16];
		str_format(aMembers, sizeof(aMembers), "%d", PartyMemberCount());
		SteamFriends()->SetRichPresence("steam_player_group_size", aMembers);
		return;
	}
	if(pUpdate->m_ulSteamIDLobby != m_CurrentLobbyID)
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

void CSteamPlatformServices::OnLobbyDataUpdated(LobbyDataUpdate_t *pUpdate)
{
	if(!pUpdate || !pUpdate->m_bSuccess || pUpdate->m_ulSteamIDLobby != m_PartyLobbyID ||
		pUpdate->m_ulSteamIDMember != m_PartyLobbyID || !SteamMatchmaking())
		return;
	CPlatformPartyState State;
	if(!PartyState(&State) || State.m_LaunchGeneration <= m_ConsumedPartyLaunchGeneration ||
		State.m_LaunchGeneration <= m_PendingPartyLaunch.m_Generation || State.m_TargetType == PLATFORM_PARTY_TARGET_NONE)
		return;
	unsigned long long LaunchOwner = 0;
	char Trailing = 0;
	const char *pLaunchOwner = SteamMatchmaking()->GetLobbyData(CSteamID(m_PartyLobbyID), "launch_owner");
	if(!pLaunchOwner || sscanf(pLaunchOwner, "%llu%c", &LaunchOwner, &Trailing) != 1 || LaunchOwner != State.m_OwnerUserID)
		return;
	const char *pLocalModHash = g_Config.m_ClModHash[0] ? g_Config.m_ClModHash : "none";
	if(State.m_aTargetModHash[0] && str_comp(State.m_aTargetModHash, "none") && str_comp(State.m_aTargetModHash, pLocalModHash))
	{
		m_ConsumedPartyLaunchGeneration = State.m_LaunchGeneration;
		SetJoinFailure("The party target requires a different Mod collection.");
		return;
	}
	mem_zero(&m_PendingPartyLaunch, sizeof(m_PendingPartyLaunch));
	m_PendingPartyLaunch.m_TargetType = State.m_TargetType;
	m_PendingPartyLaunch.m_TargetLobbyID = State.m_TargetLobbyID;
	m_PendingPartyLaunch.m_Generation = State.m_LaunchGeneration;
	str_copy(m_PendingPartyLaunch.m_aTargetAddress, State.m_aTargetAddress, sizeof(m_PendingPartyLaunch.m_aTargetAddress));
	str_copy(m_PendingPartyLaunch.m_aTargetModHash, State.m_aTargetModHash, sizeof(m_PendingPartyLaunch.m_aTargetModHash));
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
	str_copy(m_WorkshopPublish.m_aStatus,pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement?"item created; accept the Workshop legal agreement, then add this ID to ninslash_content.json":"item created; add this ID to ninslash_content.json before publishing content",sizeof(m_WorkshopPublish.m_aStatus));
}

void CSteamPlatformServices::OnWorkshopSubmitted(SubmitItemUpdateResult_t *pResult, bool IOError)
{
	m_WorkshopPublish.m_Active=false;m_WorkshopUpdateHandle=k_UGCUpdateHandleInvalid;
	if(IOError||!pResult||pResult->m_eResult!=k_EResultOK){str_copy(m_WorkshopPublish.m_aStatus,"Workshop update failed",sizeof(m_WorkshopPublish.m_aStatus));return;}
	m_WorkshopPublish.m_PublishedFileID=pResult->m_nPublishedFileId;m_WorkshopPublish.m_NeedsLegalAgreement=pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement;
	str_copy(m_WorkshopPublish.m_aStatus,pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement?"update submitted; Workshop legal agreement acceptance required":"Workshop update published",sizeof(m_WorkshopPublish.m_aStatus));RefreshWorkshopItems();
}

void CSteamPlatformServices::OnWorkshopQuery(SteamUGCQueryCompleted_t *pResult, bool IOError)
{
	m_WorkshopQueryPending = false;
	if(IOError || !pResult || pResult->m_eResult != k_EResultOK || pResult->m_handle != m_WorkshopQueryHandle || !SteamUGC())
	{
		str_copy(m_WorkshopQueryResult.m_aError, "Steam Workshop query failed", sizeof(m_WorkshopQueryResult.m_aError)); m_WorkshopQueryResultReady = true;
		if(m_WorkshopQueryHandle != k_UGCQueryHandleInvalid && SteamUGC()) SteamUGC()->ReleaseQueryUGCRequest(m_WorkshopQueryHandle);
		m_WorkshopQueryHandle = k_UGCQueryHandleInvalid;
		return;
	}
	m_WorkshopQueryItemCount = 0;
	const uint32 Count = min((uint32)50, pResult->m_unNumResultsReturned);
	for(uint32 i = 0; i < Count; i++)
	{
		SteamUGCDetails_t Details; if(!SteamUGC()->GetQueryUGCResult(m_WorkshopQueryHandle, i, &Details) || Details.m_eResult != k_EResultOK || Details.m_bBanned) continue;
		CPlatformWorkshopItem &Item = m_aWorkshopQueryItems[m_WorkshopQueryItemCount++]; mem_zero(&Item, sizeof(Item)); Item.m_PublishedFileID = Details.m_nPublishedFileId; Item.m_OwnerUserID = Details.m_ulSteamIDOwner; Item.m_State = SteamUGC()->GetItemState(Details.m_nPublishedFileId); Item.m_Total = Details.m_ulTotalFilesSize; Item.m_CreatedAt = Details.m_rtimeCreated; Item.m_UpdatedAt = Details.m_rtimeUpdated; Item.m_VotesUp = Details.m_unVotesUp; Item.m_VotesDown = Details.m_unVotesDown; Item.m_Score = Details.m_flScore; str_copy(Item.m_aName, Details.m_rgchTitle, sizeof(Item.m_aName)); str_copy(Item.m_aDescription, Details.m_rgchDescription, sizeof(Item.m_aDescription)); str_copy(Item.m_aTags, Details.m_rgchTags, sizeof(Item.m_aTags)); str_format(Item.m_aAuthor, sizeof(Item.m_aAuthor), "%llu", Details.m_ulSteamIDOwner); SteamUGC()->GetQueryUGCPreviewURL(m_WorkshopQueryHandle, i, Item.m_aPreviewURL, sizeof(Item.m_aPreviewURL));
		Item.m_ContentType = str_find(Details.m_rgchTags, "Maps") ? CONTENT_TYPE_MAP : str_find(Details.m_rgchTags, "Room Presets") ? CONTENT_TYPE_ROOM_PRESET : str_find(Details.m_rgchTags, "Challenges") ? CONTENT_TYPE_CHALLENGE : CONTENT_TYPE_MOD;
		char aMetadata[1024]; if(SteamUGC()->GetQueryUGCMetadata(m_WorkshopQueryHandle, i, aMetadata, sizeof(aMetadata)))
		{
			auto ReadMetadata = [&](const char *pKey, char *pOut, int OutSize) { char aNeedle[64]; str_format(aNeedle, sizeof(aNeedle), "%s=", pKey); const char *pValue = str_find(aMetadata, aNeedle); if(!pValue) return; pValue += str_length(aNeedle); int Length = 0; while(pValue[Length] && pValue[Length] != ';' && Length < OutSize - 1) { pOut[Length] = pValue[Length]; Length++; } pOut[Length] = 0; };
			char aType[32]; aType[0] = 0; ReadMetadata("type", aType, sizeof(aType)); int Type = 0; if(aType[0] && ContentTypeFromName(aType, &Type)) Item.m_ContentType = Type; ReadMetadata("version", Item.m_aVersion, sizeof(Item.m_aVersion)); ReadMetadata("protocol", Item.m_aTargetProtocol, sizeof(Item.m_aTargetProtocol)); ReadMetadata("hash", Item.m_aContentHash, sizeof(Item.m_aContentHash)); ReadMetadata("rating", Item.m_aContentRating, sizeof(Item.m_aContentRating));
		}
		for(int Local = 0; Local < m_WorkshopItemCount; Local++) if(m_aWorkshopItems[Local].m_PublishedFileID == Item.m_PublishedFileID) { const CPlatformWorkshopItem Remote = Item; CPlatformWorkshopItem &Installed = m_aWorkshopItems[Local]; Installed.m_OwnerUserID = Remote.m_OwnerUserID; Installed.m_State = Remote.m_State; Installed.m_Total = Remote.m_Total ? Remote.m_Total : Installed.m_Total; Installed.m_CreatedAt = Remote.m_CreatedAt; Installed.m_UpdatedAt = Remote.m_UpdatedAt; Installed.m_VotesUp = Remote.m_VotesUp; Installed.m_VotesDown = Remote.m_VotesDown; Installed.m_Score = Remote.m_Score; str_copy(Installed.m_aPreviewURL, Remote.m_aPreviewURL, sizeof(Installed.m_aPreviewURL)); str_copy(Installed.m_aTags, Remote.m_aTags, sizeof(Installed.m_aTags)); Item = Installed; break; }
	}
	m_WorkshopQueryResult.m_Succeeded = true; m_WorkshopQueryResult.m_Returned = m_WorkshopQueryItemCount; m_WorkshopQueryResult.m_TotalMatching = pResult->m_unTotalMatchingResults; m_WorkshopQueryResultReady = true;
	SteamUGC()->ReleaseQueryUGCRequest(m_WorkshopQueryHandle); m_WorkshopQueryHandle = k_UGCQueryHandleInvalid;
}

void CSteamPlatformServices::OnWorkshopPreviewDownloaded(HTTPRequestCompleted_t *pResult)
{
	if(!pResult || !SteamHTTP()) return;
	int RequestIndex = -1;
	for(int i = 0; i < m_WorkshopPreviewRequestCount; i++) if(m_aWorkshopPreviewRequests[i].m_Handle == pResult->m_hRequest) { RequestIndex = i; break; }
	if(RequestIndex < 0) { SteamHTTP()->ReleaseHTTPRequest(pResult->m_hRequest); return; }
	const CWorkshopPreviewRequest Request = m_aWorkshopPreviewRequests[RequestIndex];
	m_aWorkshopPreviewRequests[RequestIndex] = m_aWorkshopPreviewRequests[--m_WorkshopPreviewRequestCount];
	CPlatformWorkshopPreviewResult Result; mem_zero(&Result, sizeof(Result)); Result.m_OperationID = Request.m_OperationID; Result.m_PublishedFileID = Request.m_PublishedFileID; Result.m_UpdatedAt = Request.m_UpdatedAt;
	uint32 BodySize = 0;
	bool MimeValid = true; uint32 MimeSize = 0;
	if(SteamHTTP()->GetHTTPResponseHeaderSize(pResult->m_hRequest, "content-type", &MimeSize) && MimeSize > 0)
	{
		if(MimeSize > 63) MimeValid = false;
		else { char aMime[64]; mem_zero(aMime, sizeof(aMime)); MimeValid = SteamHTTP()->GetHTTPResponseHeaderValue(pResult->m_hRequest, "content-type", (uint8 *)aMime, MimeSize) && (str_find_nocase(aMime, "image/png") || str_find_nocase(aMime, "image/jpeg") || str_find_nocase(aMime, "image/jpg")); }
	}
	if(!pResult->m_bRequestSuccessful || pResult->m_eStatusCode != k_EHTTPStatusCode200OK || !MimeValid || !SteamHTTP()->GetHTTPResponseBodySize(pResult->m_hRequest, &BodySize) || BodySize < 16 || BodySize > 8 * 1024 * 1024)
		str_copy(Result.m_aError, "Workshop preview download failed", sizeof(Result.m_aError));
	else
	{
		unsigned char *pData = (unsigned char *)mem_alloc(BodySize, 1);
		if(!pData || !SteamHTTP()->GetHTTPResponseBodyData(pResult->m_hRequest, pData, BodySize))
			str_copy(Result.m_aError, "Workshop preview response was invalid", sizeof(Result.m_aError));
		else
		{
			const bool PNG = BodySize >= 8 && pData[0] == 0x89 && pData[1] == 'P' && pData[2] == 'N' && pData[3] == 'G';
			const bool JPEG = BodySize >= 3 && pData[0] == 0xff && pData[1] == 0xd8 && pData[2] == 0xff;
			if(!PNG && !JPEG)
				str_copy(Result.m_aError, "Workshop preview format is unsupported", sizeof(Result.m_aError));
			else if(m_pStorage)
			{
				char aFinal[256], aTemporary[272]; str_format(aFinal, sizeof(aFinal), "workshop_cache/previews/%llu_%u.%s", Request.m_PublishedFileID, Request.m_UpdatedAt, JPEG ? "jpg" : "png"); str_format(aTemporary, sizeof(aTemporary), "%s.tmp", aFinal);
				IOHANDLE File = m_pStorage->OpenFile(aTemporary, IOFLAG_WRITE, IStorage::TYPE_SAVE);
				const bool Written = File && io_write(File, pData, BodySize) == BodySize;
				if(File) io_close(File);
				if(Written) { m_pStorage->RemoveFile(aFinal, IStorage::TYPE_SAVE); Result.m_Succeeded = m_pStorage->RenameFile(aTemporary, aFinal, IStorage::TYPE_SAVE); }
				if(Result.m_Succeeded) { str_copy(Result.m_aCachePath, aFinal, sizeof(Result.m_aCachePath)); TrimWorkshopPreviewCache(m_pStorage); }
				else { m_pStorage->RemoveFile(aTemporary, IStorage::TYPE_SAVE); str_copy(Result.m_aError, "Workshop preview could not be cached", sizeof(Result.m_aError)); }
			}
		}
		if(pData) mem_free(pData);
	}
	SteamHTTP()->ReleaseHTTPRequest(pResult->m_hRequest);
	if(m_WorkshopPreviewResultCount >= 16) { for(int i = 1; i < 16; i++) m_aWorkshopPreviewResults[i - 1] = m_aWorkshopPreviewResults[i]; m_WorkshopPreviewResultCount = 15; }
	m_aWorkshopPreviewResults[m_WorkshopPreviewResultCount++] = Result;
}

void CSteamPlatformServices::OnScreenshotReady(ScreenshotReady_t *pResult)
{
	if(!pResult || pResult->m_eResult != k_EResultOK || !SteamScreenshots()) return;
	CPlatformScreenshotContext Context = m_ScreenshotContext;
	for(int i = 0; i < m_PendingScreenshotCount; i++)
	{
		if(m_aPendingScreenshots[i].m_Handle != pResult->m_hLocal) continue;
		Context = m_aPendingScreenshots[i].m_Context;
		m_aPendingScreenshots[i] = m_aPendingScreenshots[--m_PendingScreenshotCount];
		break;
	}
	if(Context.m_aLocation[0]) SteamScreenshots()->SetLocation(pResult->m_hLocal, Context.m_aLocation);
	for(int i = 0; i < clamp(Context.m_UserCount, 0, 32); i++) if(Context.m_aUsers[i]) SteamScreenshots()->TagUser(pResult->m_hLocal, CSteamID(Context.m_aUsers[i]));
	for(int i = 0; i < clamp(Context.m_PublishedFileCount, 0, 32); i++) if(Context.m_aPublishedFiles[i]) SteamScreenshots()->TagPublishedFile(pResult->m_hLocal, (PublishedFileId_t)Context.m_aPublishedFiles[i]);
}

void CSteamPlatformServices::OnLobbyCreated(LobbyCreated_t *pResult, bool IOError)
{
	m_LobbyCreatePending = false;
	if(IOError || !pResult || pResult->m_eResult != k_EResultOK)
	{
		const EResult Result = pResult ? pResult->m_eResult : k_EResultFail;
		dbg_msg("steam", "CreateLobby failed: io=%d result=%d attempt=%d", IOError ? 1 : 0, (int)Result, m_LobbyCreateRetries + 1);
		if(LobbyCreateFailureIsTransient(Result, IOError) && m_LobbyCreateRetries < 1)
		{
			m_LobbyCreateRetries++;
			m_LobbyCreatePending = true;
			m_LobbyCreateRetryAt = time_get() + time_freq();
			dbg_msg("steam", "retrying CreateLobby in 1 second");
			return;
		}
		str_copy(m_aLobbyCreateFailure, LobbyCreateFailureKey(Result, IOError), sizeof(m_aLobbyCreateFailure));
		if(!m_CreatingParty)
			m_ListenServerStopRequested = true;
		else
			m_PendingPartyInviteUserID = 0;
		m_OpenPartyInviteAfterCreate = false;
		m_CreatingParty = false;
		return;
	}
	m_LobbyCreateRetryAt = 0;
	m_aLobbyCreateFailure[0] = 0;
	if(m_CreatingParty)
	{
		m_CreatingParty = false;
		m_PartyLobbyID = pResult->m_ulSteamIDLobby;
		m_PartyOwnerID = LocalUserID();
		m_ConsumedPartyLaunchGeneration = 0;
		const CSteamID Lobby(m_PartyLobbyID);
		SteamMatchmaking()->SetLobbyData(Lobby, "protocol", GAME_NETVERSION);
		SteamMatchmaking()->SetLobbyData(Lobby, "room_type", "party");
		SteamMatchmaking()->SetLobbyData(Lobby, "party_phase", "forming");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_type", "none");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_lobby", "0");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_address", "");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_mod_hash", "none");
		SteamMatchmaking()->SetLobbyData(Lobby, "target_revision", "0");
		SteamMatchmaking()->SetLobbyData(Lobby, "launch_generation", "0");
		SteamMatchmaking()->SetLobbyData(Lobby, "launch_owner", "0");
		SteamMatchmaking()->SetLobbyMemberData(Lobby, "ready_revision", "0");
		if(m_PendingPartyInviteUserID)
		{
			SteamMatchmaking()->InviteUserToLobby(Lobby, CSteamID(m_PendingPartyInviteUserID));
			m_PendingPartyInviteUserID = 0;
		}
		if(m_OpenPartyInviteAfterCreate && SteamFriends())
			SteamFriends()->ActivateGameOverlayInviteDialog(Lobby);
		m_OpenPartyInviteAfterCreate = false;
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
	if(m_PartyLobbyID)
	{
		SetPartyTarget(PLATFORM_PARTY_TARGET_GAME_LOBBY, m_CurrentLobbyID, "", g_Config.m_SvModHash[0] ? g_Config.m_SvModHash : "none");
		SetPartyReady(true);
	}
	else
		str_format(m_aPendingJoin, sizeof(m_aPendingJoin), "127.0.0.1:%d", m_HostLocalPort);
	SteamFriends()->SetRichPresence("connect", aConnect);
}

void CSteamPlatformServices::OnLobbyEntered(LobbyEnter_t *pResult, bool IOError)
{
	m_LobbyJoinPending = false;
	if(IOError || !pResult || pResult->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess)
	{
		SetJoinFailure("Steam rejected the room join request. Refresh rooms and try again.");
		m_JoiningParty = false;
		return;
	}
	if(m_JoiningParty)
	{
		m_JoiningParty = false;
		const CSteamID Lobby(pResult->m_ulSteamIDLobby);
		const char *pRoomType = SteamMatchmaking()->GetLobbyData(Lobby, "room_type");
		const char *pProtocol = SteamMatchmaking()->GetLobbyData(Lobby, "protocol");
		if(!pRoomType || str_comp(pRoomType, "party") || !pProtocol || str_comp(pProtocol, GAME_NETVERSION))
		{
			SteamMatchmaking()->LeaveLobby(Lobby);
			SetJoinFailure("This party uses an incompatible game version.");
			return;
		}
		m_PartyLobbyID = Lobby.ConvertToUint64();
		m_PartyOwnerID = SteamMatchmaking()->GetLobbyOwner(Lobby).ConvertToUint64();
		m_ConsumedPartyLaunchGeneration = (unsigned int)str_toint(SteamMatchmaking()->GetLobbyData(Lobby, "launch_generation"));
		SteamMatchmaking()->SetLobbyMemberData(Lobby, "ready_revision", "0");
		return;
	}
	m_CurrentLobbyID = pResult->m_ulSteamIDLobby;
	m_PendingLobbyJoinID = m_CurrentLobbyID;
	if(m_HostedLobbyID == m_CurrentLobbyID)
	{
		str_format(m_aPendingJoin, sizeof(m_aPendingJoin), "127.0.0.1:%d", m_HostLocalPort);
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
