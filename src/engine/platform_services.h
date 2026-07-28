#ifndef ENGINE_PLATFORM_SERVICES_H
#define ENGINE_PLATFORM_SERVICES_H

#include "kernel.h"

class INetPacketTransport;

enum EPlatformInputActionSet
{
	PLATFORM_INPUT_GAME,
	PLATFORM_INPUT_MENU,
	PLATFORM_INPUT_SPECTATOR,
	PLATFORM_INPUT_CHAT,
};

enum EPlatformInputAction
{
	PLATFORM_ACTION_CONFIRM,
	PLATFORM_ACTION_CANCEL,
	PLATFORM_ACTION_FIRE,
	PLATFORM_ACTION_ALT_FIRE,
	PLATFORM_ACTION_SCOREBOARD,
	PLATFORM_ACTION_BUILD,
	PLATFORM_ACTION_DROP,
	PLATFORM_ACTION_EMOTE,
	PLATFORM_ACTION_PICKER,
	PLATFORM_ACTION_LAST_WEAPON,
	PLATFORM_ACTION_PREV_WEAPON,
	PLATFORM_ACTION_NEXT_WEAPON,
	PLATFORM_ACTION_UP,
	PLATFORM_ACTION_DOWN,
	PLATFORM_ACTION_LEFT,
	PLATFORM_ACTION_RIGHT,
	NUM_PLATFORM_INPUT_ACTIONS,
};

struct CPlatformInputState
{
	bool m_Connected;
	bool m_aActions[NUM_PLATFORM_INPUT_ACTIONS];
	float m_MoveX;
	float m_MoveY;
	float m_AimX;
	float m_AimY;
};

struct CPlatformWorkshopItem
{
	unsigned long long m_PublishedFileID;
	unsigned int m_State;
	unsigned long long m_Downloaded;
	unsigned long long m_Total;
	bool m_Valid;
	char m_aName[128];
	char m_aVersion[32];
	char m_aInstallPath[1024];
	char m_aError[256];
};

struct CPlatformLobbyInfo
{
	unsigned long long m_LobbyID;
	unsigned long long m_HostSteamID;
	int m_Members;
	int m_MaxMembers;
	bool m_Password;
	bool m_Modded;
	bool m_FriendHosted;
	char m_aHostName[128];
	char m_aMap[128];
	char m_aGameType[32];
	char m_aRegion[32];
	char m_aModHash[65];
};

struct CPlatformUserInfo
{
	unsigned long long m_UserID;
	unsigned long long m_LobbyID;
	int m_PersonaState;
	bool m_Friend;
	bool m_PlayingThisGame;
	bool m_Joinable;
	bool m_Local;
	bool m_LobbyOwner;
	bool m_PartyMember;
	bool m_PartyReady;
	int m_PartyReadyRevision;
	char m_aName[128];
	char m_aConnect[256];
};

enum EPlatformPartyTargetType
{
	PLATFORM_PARTY_TARGET_NONE,
	PLATFORM_PARTY_TARGET_GAME_LOBBY,
	PLATFORM_PARTY_TARGET_ADDRESS,
};

struct CPlatformPartyState
{
	unsigned long long m_LobbyID;
	unsigned long long m_OwnerUserID;
	unsigned long long m_TargetLobbyID;
	int m_TargetType;
	int m_TargetRevision;
	unsigned int m_LaunchGeneration;
	bool m_LocalOwner;
	char m_aPhase[24];
	char m_aTargetAddress[256];
	char m_aTargetModHash[65];
};

struct CPlatformPartyLaunch
{
	unsigned long long m_TargetLobbyID;
	int m_TargetType;
	unsigned int m_Generation;
	char m_aTargetAddress[256];
	char m_aTargetModHash[65];
};

struct CPlatformWorkshopPublishStatus
{
	bool m_Active;
	bool m_NeedsLegalAgreement;
	unsigned long long m_PublishedFileID;
	unsigned long long m_Processed;
	unsigned long long m_Total;
	char m_aStatus[256];
};

struct CPlatformOperationStatus
{
	int m_State; // EClientAsyncState-compatible: idle/working/succeeded/failed.
	int m_Stage; // EClientConnectionStage-compatible for unified menu progress.
	float m_Progress;
	char m_aErrorKey[128];
};

struct CPlatformCloudStatus
{
	bool m_Available;
	bool m_AccountEnabled;
	bool m_AppEnabled;
	unsigned long long m_BytesTotal;
	unsigned long long m_BytesAvailable;
	char m_aError[128];
};

enum EPlatformLobbyVisibility
{
	PLATFORM_LOBBY_INVITE_ONLY,
	PLATFORM_LOBBY_FRIENDS,
	PLATFORM_LOBBY_PUBLIC,
};

class IPlatformServices : public IInterface
{
	MACRO_INTERFACE("platformservices", 0)

public:
	virtual bool Init() = 0;
	// True only when Steam requested that this process exits because it has
	// forwarded startup to the Steam client. Other Init failures fall back to
	// standalone networking.
	virtual bool ExitRequested() const = 0;
	virtual void Shutdown() = 0;
	virtual void RunCallbacks() = 0;
	virtual bool Available() const = 0;
	virtual const char *PlatformName() const = 0;
	virtual unsigned long long LocalUserID() const = 0;
	// Returns -1 while Steam registers a newly requested ticket, 0 when no
	// ticket is available, and the ticket size after registration succeeds.
	virtual int GetAuthSessionTicket(void *pBuffer, int BufferSize) = 0;
	virtual void CancelAuthSessionTicket() = 0;
	virtual void SetRichPresence(const char *pStatus, const char *pConnect) = 0;
	virtual bool ConsumeJoinRequest(char *pBuffer, int BufferSize) = 0;
	virtual bool ConsumeJoinFailure(char *pBuffer, int BufferSize) = 0;
	virtual void CloudStatus(CPlatformCloudStatus *pStatus) const = 0;
	virtual int CloudFileSize(const char *pFilename) const = 0;
	virtual long long CloudFileTimestamp(const char *pFilename) const = 0;
	virtual int CloudReadFile(const char *pFilename, void *pBuffer, int BufferSize) = 0;
	virtual bool CloudWriteFile(const char *pFilename, const void *pBuffer, int BufferSize) = 0;

	// A party is an invite-only, persistent coordination Lobby. It is kept
	// separate from the transient game Lobby used by Steam listen servers.
	virtual bool CreateParty() = 0;
	virtual bool JoinParty(unsigned long long LobbyID) = 0;
	virtual void LeaveParty() = 0;
	virtual unsigned long long PartyLobbyID() const = 0;
	virtual bool PartyState(CPlatformPartyState *pState) const = 0;
	virtual int PartyMemberCount() const = 0;
	virtual bool PartyMemberInfo(int Index, CPlatformUserInfo *pInfo) const = 0;
	virtual bool InvitePartyUser(unsigned long long UserID) = 0;
	virtual bool OpenPartyInviteDialog() = 0;
	virtual bool SetPartyReady(bool Ready) = 0;
	virtual bool SetPartyTarget(int TargetType, unsigned long long TargetLobbyID, const char *pAddress, const char *pModHash) = 0;
	virtual bool ClearPartyTarget() = 0;
	virtual bool LaunchParty(bool Force) = 0;
	virtual bool ConsumePartyLaunch(CPlatformPartyLaunch *pLaunch) = 0;
	virtual void PartyOperationStatus(CPlatformOperationStatus *pStatus) const = 0;

	// All methods below are asynchronous on Steam. They report whether the
	// request was accepted locally; completion arrives through RunCallbacks.
	virtual bool CreateLobby(EPlatformLobbyVisibility Visibility, int MaxMembers, int HostLocalPort) = 0;
	virtual bool JoinLobby(unsigned long long LobbyID) = 0;
	virtual void LeaveLobby() = 0;
	virtual unsigned long long CurrentLobbyID() const = 0;
	virtual unsigned long long GameLobbyID() const = 0;
	virtual void LeaveGameLobby() = 0;
	virtual bool SetLobbyData(const char *pKey, const char *pValue) = 0;
	virtual bool ConsumeLobbyJoin(unsigned long long *pLobbyID) = 0;
	// Listen servers close instead of migrating when Steam transfers ownership.
	virtual bool ConsumeListenServerStopRequest() = 0;
	virtual bool OpenLobbyInviteDialog() = 0;
	virtual int FriendCount() const = 0;
	virtual bool FriendInfo(int Index, CPlatformUserInfo *pInfo) const = 0;
	virtual bool UserInfo(unsigned long long UserID, CPlatformUserInfo *pInfo) const = 0;
	virtual int LobbyMemberCount() const = 0;
	virtual bool LobbyMemberInfo(int Index, CPlatformUserInfo *pInfo) const = 0;
	virtual bool InviteUser(unsigned long long UserID, const char *pConnect) = 0;
	virtual bool JoinUser(unsigned long long UserID) = 0;
	virtual bool OpenUserProfile(unsigned long long UserID) = 0;
	virtual void SetPlayedWith(unsigned long long UserID) = 0;
	// Returns 1 when RGBA data was copied, 0 while loading, and -1 when unavailable.
	virtual int UserAvatarRGBA(unsigned long long UserID, int PreferredSize, void *pBuffer, int BufferSize, int *pWidth, int *pHeight) = 0;
	virtual bool RefreshLobbyList() = 0;
	virtual bool RefreshDedicatedServerList() = 0;
	virtual int LobbyCount() const = 0;
	virtual bool LobbyInfo(int Index, CPlatformLobbyInfo *pInfo) const = 0;
	virtual void LobbyOperationStatus(CPlatformOperationStatus *pStatus) const = 0;
	virtual bool SubscribeWorkshopItem(unsigned long long PublishedFileID) = 0;
	virtual bool UnsubscribeWorkshopItem(unsigned long long PublishedFileID) = 0;
	virtual bool OpenWorkshopItemPage(unsigned long long PublishedFileID) = 0;
	virtual bool OpenWorkshopBrowsePage() = 0;
	virtual bool WorkshopDownloadProgress(unsigned long long PublishedFileID, unsigned long long *pDownloaded, unsigned long long *pTotal) const = 0;
	virtual int RefreshWorkshopItems() = 0;
	virtual int WorkshopItemCount() const = 0;
	virtual bool WorkshopItem(int Index, CPlatformWorkshopItem *pItem) const = 0;
	virtual bool SetWorkshopItemDisabled(unsigned long long PublishedFileID, bool Disabled) = 0;
	virtual void WorkshopOperationStatus(CPlatformOperationStatus *pStatus) const = 0;
	virtual bool CreateWorkshopItem() = 0;
	virtual bool UpdateWorkshopItem(unsigned long long PublishedFileID, const char *pContentRoot, const char *pPreviewFile) = 0;
	virtual void WorkshopPublishStatus(CPlatformWorkshopPublishStatus *pStatus) const = 0;
	virtual bool UnlockAchievement(const char *pAchievement) = 0;
	virtual void ProcessServerEvent(int Event, int Value, bool LeaderboardEligible) = 0;
	virtual bool SteamInputActive() const = 0;
	virtual void SetInputActionSet(EPlatformInputActionSet ActionSet) = 0;
	virtual bool ReadInputState(CPlatformInputState *pState) = 0;
	virtual INetPacketTransport *RelayTransport() = 0;
	virtual INetPacketTransport *RelayListenTransport() = 0;
};

IPlatformServices *CreatePlatformServices();

#endif
