#ifndef ENGINE_PLATFORM_SERVICES_H
#define ENGINE_PLATFORM_SERVICES_H

#include "kernel.h"

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
	// The caller owns no ticket memory. A zero-length ticket is a valid
	// standalone response and must never be used to bypass a required server.
	virtual int GetAuthSessionTicket(void *pBuffer, int BufferSize) = 0;
	virtual void SetRichPresence(const char *pStatus, const char *pConnect) = 0;
	virtual bool ConsumeJoinRequest(char *pBuffer, int BufferSize) = 0;

	// All methods below are asynchronous on Steam. They report whether the
	// request was accepted locally; completion arrives through RunCallbacks.
	virtual bool CreateLobby(EPlatformLobbyVisibility Visibility, int MaxMembers) = 0;
	virtual bool JoinLobby(unsigned long long LobbyID) = 0;
	virtual void LeaveLobby() = 0;
	virtual unsigned long long CurrentLobbyID() const = 0;
	virtual bool SetLobbyData(const char *pKey, const char *pValue) = 0;
	virtual bool ConsumeLobbyJoin(unsigned long long *pLobbyID) = 0;
	virtual bool OpenLobbyInviteDialog() = 0;
	virtual bool SubscribeWorkshopItem(unsigned long long PublishedFileID) = 0;
	virtual bool WorkshopDownloadProgress(unsigned long long PublishedFileID, unsigned long long *pDownloaded, unsigned long long *pTotal) const = 0;
	virtual bool UnlockAchievement(const char *pAchievement) = 0;
	virtual bool SteamInputActive() const = 0;
};

IPlatformServices *CreatePlatformServices();

#endif
