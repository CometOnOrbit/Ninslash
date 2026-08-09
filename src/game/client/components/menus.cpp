

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <engine/config.h>
#include <engine/editor.h>
#include <engine/engine.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/shared/content_package.h>
#include <engine/shared/community_challenge.h>
#include <engine/shared/room_preset.h>
#include <engine/platform_services.h>

#include <game/version.h>
#include <game/challenge_variant.h>
#include <generated/protocol.h>

#include <generated/game_data.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/local_game_modes.h>
#include <game/client/menu_home.h>
#include <game/client/room_creation.h>
#include <game/client/skelebank.h>
#include <game/localization.h>
#include <game/tutorial.h>
#include <mastersrv/mastersrv.h>
#include <game/client/customstuff.h>

#include "countryflags.h"
#include "menus.h"
#include "pve_roguelite.h"
#include "skins.h"

vec4 CMenus::ms_GuiColor;
vec4 CMenus::ms_ColorTabbarInactiveOutgame;
vec4 CMenus::ms_ColorTabbarActiveOutgame;
vec4 CMenus::ms_ColorTabbarInactive;
vec4 CMenus::ms_ColorTabbarActive = vec4(0.04f, 0.05f, 0.06f, 0.88f);
vec4 CMenus::ms_ColorTabbarInactiveIngame;
vec4 CMenus::ms_ColorTabbarActiveIngame;

vec4 CMenus::ms_ColorBgDeep = vec4(0.008f, 0.020f, 0.038f, 0.92f);
vec4 CMenus::ms_ColorBgPanel = vec4(0.030f, 0.067f, 0.098f, 0.78f);
vec4 CMenus::ms_ColorBgInset = vec4(0.018f, 0.045f, 0.071f, 0.72f);
vec4 CMenus::ms_ColorAccent = vec4(0.22f, 0.88f, 1.00f, 1.0f);
vec4 CMenus::ms_ColorAccentDim = vec4(0.42f, 0.96f, 0.72f, 1.0f);
vec4 CMenus::ms_ColorAccentWarm = vec4(1.00f, 0.69f, 0.24f, 1.0f);
vec4 CMenus::ms_ColorDanger = vec4(0.92f, 0.24f, 0.30f, 1.0f);
vec4 CMenus::ms_ColorText = vec4(0.92f, 0.97f, 1.00f, 1.0f);
vec4 CMenus::ms_ColorGlassLine = vec4(0.72f, 0.94f, 1.00f, 0.18f);
float CMenus::ms_PanelRounding = 12.0f;
float CMenus::ms_ControlRounding = 6.0f;

float CMenus::ms_ButtonHeight = 25.0f;
float CMenus::ms_ListheaderHeight = 14.0f;
float CMenus::ms_FontmodHeight = 0.8f;

IInput::CEvent CMenus::m_aInputEvents[MAX_INPUTEVENTS];
int CMenus::m_NumInputEvents;

static bool s_ResetMenu = true;

static float FitLabelFontSize(ITextRender *pTextRender, const char *pText, float FontSize, float MaxWidth)
{
	if(!pText || !pText[0] || FontSize <= 0.0f || MaxWidth <= 0.0f)
		return FontSize;

	const float TextWidth = pTextRender->TextWidth(0, FontSize, pText, -1);
	if(TextWidth > MaxWidth && TextWidth > 0.0f)
		FontSize *= MaxWidth / TextWidth;
	return FontSize;
}

static float
FitScaledLabelFontSize(ITextRender *pTextRender, const char *pText, float FontSize, float MaxWidth, float Scale)
{
	return FitLabelFontSize(pTextRender, pText, FontSize, MaxWidth / max(Scale, 0.01f));
}

static bool ModCollectionContains(const char *pIds, unsigned long long ID)
{
	if(!pIds || !ID)
		return false;
	const char *pCursor = pIds;
	while(*pCursor)
	{
		unsigned long long Current = 0;
		int Length = 0;
		if(sscanf(pCursor, "%llu%n", &Current, &Length) != 1 || Length <= 0)
			return false;
		if(Current == ID && (pCursor[Length] == 0 || pCursor[Length] == ','))
			return true;
		pCursor += Length;
		if(*pCursor == ',')
			++pCursor;
		else if(*pCursor)
			return false;
	}
	return false;
}

static bool SetModCollectionEnabled(char *pIds, int IdsSize, unsigned long long ID, bool Enabled)
{
	if(!pIds || IdsSize <= 0 || !ID)
		return false;
	const bool Present = ModCollectionContains(pIds, ID);
	if(Present == Enabled)
		return true;
	char aResult[1024];
	aResult[0] = 0;
	const char *pCursor = pIds;
	while(*pCursor)
	{
		unsigned long long Current = 0;
		int Length = 0;
		if(sscanf(pCursor, "%llu%n", &Current, &Length) != 1 || Length <= 0 ||
		   (pCursor[Length] != 0 && pCursor[Length] != ','))
			return false;
		if(Current != ID)
		{
			char aEntry[32];
			str_format(aEntry, sizeof(aEntry), "%s%llu", aResult[0] ? "," : "", Current);
			if(str_length(aResult) + str_length(aEntry) >= (int)sizeof(aResult))
				return false;
			str_append(aResult, aEntry, sizeof(aResult));
		}
		pCursor += Length;
		if(*pCursor == ',')
			++pCursor;
	}
	if(Enabled)
	{
		char aEntry[32];
		str_format(aEntry, sizeof(aEntry), "%s%llu", aResult[0] ? "," : "", ID);
		if(str_length(aResult) + str_length(aEntry) >= (int)sizeof(aResult))
			return false;
		str_append(aResult, aEntry, sizeof(aResult));
	}
	if(str_length(aResult) >= IdsSize)
		return false;
	str_copy(pIds, aResult, IdsSize);
	return true;
}

const char *CMenus::DisplayGameType(const char *pGameType) const
{
	if(!pGameType || !pGameType[0])
		return Localize("Unknown");
	struct CGameTypeName
	{
		const char *m_pCode;
		const char *m_pName;
	};
	static const CGameTypeName s_aNames[] = {
		{"dm", "Deathmatch"},
		{"tdm", "Team deathmatch"},
		{"ctf", "Capture the flag"},
		{"ball", "Ball"},
		{"def", "Reactor Assault"},
		{"base", "Reactor Defense"},
		{"inf", "Infection"},
		{"inv", "Invasion"},
		{"coop", "Invasion"},
		{"extract", "Extraction"},
		{"horde", "Horde"},
		{"tut", "Tutorial"},
		{"tutorial", "Tutorial"},
		{"roam", "Roam"},
		{"gun", "Gun game"},
	};
	for(unsigned i = 0; i < sizeof(s_aNames) / sizeof(s_aNames[0]); i++)
		if(str_comp_nocase(pGameType, s_aNames[i].m_pCode) == 0)
			return Localize(s_aNames[i].m_pName);
	return pGameType;
}

CMenus::CMenus()
{
	m_Popup = POPUP_NONE;
	m_ActivePage = PAGE_FRONT;
	m_GamePage = PAGE_GAME;
	m_ResearchReturnPage = PAGE_FRONT;
	m_ResearchReturnGamePage = PAGE_GAME;
	m_LastAnimatedPage = PAGE_FRONT;
	m_LastAnimatedPopup = POPUP_NONE;
	m_LastCreateRoomStep = 0;
	m_LastPlayTab = 0;
	m_LastWorkshopDiscover = false;
	m_LastPlayFiltersOpen = false;
	m_LastWorkshopAnimatedID = 0;
	m_PageTransition = 0.0f;
	m_PopupTransition = 0.0f;
	m_CreateRoomTransition = 1.0f;
	m_PlayTabTransition = 1.0f;
	m_WorkshopTransition = 1.0f;
	m_WorkshopDetailTransition = 1.0f;
	m_PlayFilterTransition = 1.0f;
	m_MenuOpenTransition = 0.0f;
	m_NavigationFocus = 0;
	m_HomeActionFocus = 0;
	m_LastInputDevice = 0;
	m_PlayTab = 0;
	m_CreateRoomStep = 0;
	m_CreateRoomPreviousSlots = 8;
	m_NavigationHasFocus = false;

	g_Config.m_UiPage = PAGE_FRONT;

	m_NeedRestartGraphics = false;
	m_NeedSendinfo = false;
	m_MenuActive = true;
	m_UseMouseButtons = true;
	m_LocalServerProcess = 0;
	m_LocalServerState = LOCAL_SERVER_STOPPED;
	m_LocalServerExitCode = 0;
	m_LocalServerStateTime = 0;
	m_LocalServerJoinRetryTime = 0;
	m_LocalServerInfoRequestTime = 0;
	m_LocalServerJoinAttempts = 0;
	m_LocalServerActualPort = 0;
	m_LocalServerAutoJoin = false;
	m_LocalServerRestartPending = false;
	m_TutorialChapterReplay = false;
	m_LocalServerSummaryLocalized = false;
	m_LocalServerFocus = 0;
	mem_zero(&m_LocalServerAddress, sizeof(m_LocalServerAddress));
	m_aLocalServerJoinAddress[0] = 0;
	m_aLocalServerPassword[0] = 0;
	m_aLocalServerSummary[0] = 0;
	m_aLocalServerLogPath[0] = 0;
	m_aLocalServerErrorDetail[0] = 0;

	m_EscapePressed = false;
	m_EnterPressed = false;
	m_DeletePressed = false;
	m_NumInputEvents = 0;

	m_LastInput = time_get();

	str_copy(m_aCurrentDemoFolder, "demos", sizeof(m_aCurrentDemoFolder));
	m_aDemoRenderSource[0] = 0;
	m_aVideoOutputName[0] = 0;
	m_DemoRenderStorageType = IStorage::TYPE_ALL;
	m_aCallvoteReason[0] = 0;

	m_FriendlistSelectedIndex = -1;
	m_LastUpdate = 0;

	m_DemoSliceState = 0;
	m_DemoSliceStartTick = 0;
	m_DemoSliceEndTick = 0;

	m_pUiClipScrollRegion = 0;

	m_ActiveFilterPreset = UI_FILTER_PRESET_ALL;
	m_FilterPresetRenameSlot = -1;
	m_aFilterPresetRenameBuf[0] = 0;
	mem_zero(m_aFilterPresets, sizeof(m_aFilterPresets));
	mem_zero(m_aaPlayServerSnapshots, sizeof(m_aaPlayServerSnapshots));
	mem_zero(m_aPlayServerSnapshotCount, sizeof(m_aPlayServerSnapshotCount));
	mem_zero(m_aPlayLobbySnapshots, sizeof(m_aPlayLobbySnapshots));
	m_PlayLobbySnapshotCount = 0;
	m_PlayBrowserCollection = PLAY_COLLECTION_INTERNET;
	m_aPlaySelectedID[0] = 0;
	m_PlayFiltersOpen = false;
	m_PlayFiltersAdvanced = false;
	m_FilterPresetMenuOpen = false;
	m_PlayDetailOpen = false;
	m_PlayListHasFocus = false;
	for(int i = 0; i < 128; i++)
	{
		m_aSteamAvatars[i].m_UserID = 0;
		m_aSteamAvatars[i].m_Texture = -1;
		m_aSteamAvatars[i].m_LastUsed = 0;
		m_aSteamAvatars[i].m_NextRetry = 0;
	}
	for(int i = 0; i < 32; i++)
	{
		m_aWorkshopPreviews[i].m_PublishedFileID = 0;
		m_aWorkshopPreviews[i].m_UpdatedAt = 0;
		m_aWorkshopPreviews[i].m_OperationID = 0;
		m_aWorkshopPreviews[i].m_Texture = -1;
		m_aWorkshopPreviews[i].m_LastUsed = 0;
		m_aWorkshopPreviews[i].m_NextRetry = 0;
	}
	m_WorkshopSelectedID = 0;
	m_WorkshopDiscover = false;
	m_WorkshopDetailOpen = false;
	m_SteamFriendCacheCount = 0;
	m_SteamFriendCacheNextRefresh = 0;
	m_CloudInitialized = false;
	m_CloudConflict = false;
	m_CloudPaused = false;
	m_CloudDirty = false;
	m_CloudNextCheck = 0;
	m_CloudRevision = 0;
	m_CloudSyncedHash = 0;
	mem_zero(&m_CloudLocalSummary, sizeof(m_CloudLocalSummary));
	mem_zero(&m_CloudRemoteSummary, sizeof(m_CloudRemoteSummary));
	m_aCloudLocalProfile[0] = 0;
	m_aCloudRemoteProfile[0] = 0;
	m_aCloudStatus[0] = 0;
	str_copy(
		m_aFilterPresets[UI_FILTER_PRESET_ALL].m_aName, "All", sizeof(m_aFilterPresets[UI_FILTER_PRESET_ALL].m_aName));
	str_copy(m_aFilterPresets[UI_FILTER_PRESET_FAVORITES].m_aName,
			 "Favorites",
			 sizeof(m_aFilterPresets[UI_FILTER_PRESET_FAVORITES].m_aName));
}

void CMenus::SaveCloudSyncState(unsigned long long Hash, int Revision)
{
	IOHANDLE File = Storage()->OpenFile("cloud_sync_state.tmp", IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char aState[96];
	str_format(aState, sizeof(aState), "%016llx %d\n", Hash, Revision);
	const int Length = str_length(aState);
	const bool Written = io_write(File, aState, Length) == (unsigned)Length && io_flush(File) == 0;
	io_close(File);
	if(!Written)
	{
		Storage()->RemoveFile("cloud_sync_state.tmp", IStorage::TYPE_SAVE);
		return;
	}
	Storage()->RemoveFile("cloud_sync_state.json", IStorage::TYPE_SAVE);
	Storage()->RenameFile("cloud_sync_state.tmp", "cloud_sync_state.json", IStorage::TYPE_SAVE);
	m_CloudSyncedHash = Hash;
	m_CloudRevision = Revision;
}

void CMenus::BackupCloudProfile(const char *pData, const char *pSuffix)
{
	if(!pData || !pData[0])
		return;
	char aFilename[128];
	str_format(aFilename, sizeof(aFilename), "cloud_profile_conflict_%s.json", pSuffix ? pSuffix : "backup");
	IOHANDLE File = Storage()->OpenFile(aFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	io_write(File, pData, str_length(pData));
	io_flush(File);
	io_close(File);
}

bool CMenus::UploadCloudProfile()
{
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	CPlatformCloudStatus Status;
	if(!pPlatform)
		return false;
	pPlatform->CloudStatus(&Status);
	if(!Status.m_Available || !Status.m_AccountEnabled || !Status.m_AppEnabled)
	{
		str_copy(m_aCloudStatus,
				 Status.m_aError[0] ? Status.m_aError : "Steam Cloud is unavailable",
				 sizeof(m_aCloudStatus));
		return false;
	}
	const int Revision = max(m_CloudRevision, m_CloudRemoteSummary.m_Revision) + 1;
	if(!CloudProfileBuild(m_pClient->m_pBinds,
						  Revision,
						  time_timestamp(),
						  m_aCloudLocalProfile,
						  sizeof(m_aCloudLocalProfile),
						  &m_CloudLocalSummary))
	{
		str_copy(m_aCloudStatus, "Cloud profile is too large", sizeof(m_aCloudStatus));
		return false;
	}
	if(!pPlatform->CloudWriteFile("steam_cloud_profile.json", m_aCloudLocalProfile, str_length(m_aCloudLocalProfile)))
	{
		str_copy(m_aCloudStatus, "Steam Cloud upload failed; local progress is safe", sizeof(m_aCloudStatus));
		m_CloudDirty = true;
		return false;
	}
	m_CloudRemoteSummary = m_CloudLocalSummary;
	SaveCloudSyncState(m_CloudLocalSummary.m_ContentHash, Revision);
	m_CloudDirty = false;
	str_copy(m_aCloudStatus, "Steam Cloud is up to date", sizeof(m_aCloudStatus));
	return true;
}

void CMenus::ResolveCloudConflict(bool UseRemote)
{
	if(!m_CloudConflict)
		return;
	if(UseRemote)
	{
		BackupCloudProfile(m_aCloudLocalProfile, "local");
		if(CloudProfileApply(
			   m_aCloudRemoteProfile, str_length(m_aCloudRemoteProfile), m_pClient->m_pBinds, &m_CloudRemoteSummary) !=
		   CLOUD_PROFILE_OK)
		{
			str_copy(m_aCloudStatus, "Steam Cloud profile is invalid; local progress was kept", sizeof(m_aCloudStatus));
			m_CloudPaused = true;
		}
		else
		{
			m_pClient->m_pPveRoguelite->FlushPersistentProgress();
			SaveCloudSyncState(m_CloudRemoteSummary.m_ContentHash, m_CloudRemoteSummary.m_Revision);
			str_copy(m_aCloudStatus, "Steam Cloud profile applied", sizeof(m_aCloudStatus));
		}
	}
	else
	{
		BackupCloudProfile(m_aCloudRemoteProfile, "steam");
		UploadCloudProfile();
	}
	m_CloudConflict = false;
	m_Popup = POPUP_NONE;
}

void CMenus::InitCloudProfile()
{
	m_CloudInitialized = true;
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	CPlatformCloudStatus Status;
	if(!pPlatform)
		return;
	pPlatform->CloudStatus(&Status);
	if(!Status.m_Available || !Status.m_AccountEnabled || !Status.m_AppEnabled)
	{
		str_copy(m_aCloudStatus, Status.m_aError, sizeof(m_aCloudStatus));
		return;
	}
	IOHANDLE StateFile = Storage()->OpenFile("cloud_sync_state.json", IOFLAG_READ, IStorage::TYPE_SAVE);
	if(StateFile)
	{
		char aState[96];
		const int Read = io_read(StateFile, aState, sizeof(aState) - 1);
		io_close(StateFile);
		aState[clamp(Read, 0, (int)sizeof(aState) - 1)] = 0;
		sscanf(aState, "%llx %d", &m_CloudSyncedHash, &m_CloudRevision);
	}
	CloudProfileBuild(m_pClient->m_pBinds,
					  m_CloudRevision,
					  time_timestamp(),
					  m_aCloudLocalProfile,
					  sizeof(m_aCloudLocalProfile),
					  &m_CloudLocalSummary);
	if(!pPlatform->CloudFileExists("steam_cloud_profile.json"))
	{
		UploadCloudProfile();
		return;
	}
	const int RemoteSize = pPlatform->CloudFileSize("steam_cloud_profile.json");
	if(RemoteSize <= 0)
	{
		m_CloudPaused = true;
		str_copy(m_aCloudStatus, "Steam Cloud profile is empty; local progress was kept", sizeof(m_aCloudStatus));
		return;
	}
	if(RemoteSize >= (int)sizeof(m_aCloudRemoteProfile) ||
	   pPlatform->CloudReadFile("steam_cloud_profile.json", m_aCloudRemoteProfile, sizeof(m_aCloudRemoteProfile) - 1) !=
		   RemoteSize)
	{
		// Steam can report the remote size before the file has finished
		// synchronizing. Treat that read failure as transient: in particular, do
		// not upload the local profile over a remote file that is temporarily
		// unavailable and do not disable Cloud for the rest of the session.
		m_CloudInitialized = false;
		m_CloudNextCheck = time_get() + time_freq() * 5;
		str_copy(m_aCloudStatus, "Steam Cloud is synchronizing; retrying...", sizeof(m_aCloudStatus));
		return;
	}
	m_aCloudRemoteProfile[RemoteSize] = 0;
	const ECloudProfileReadResult RemoteResult =
		CloudProfileInspect(m_aCloudRemoteProfile, RemoteSize, &m_CloudRemoteSummary);
	if(RemoteResult != CLOUD_PROFILE_OK)
	{
		str_copy(m_aCloudStatus,
				 RemoteResult == CLOUD_PROFILE_FUTURE_VERSION
					 ? "Steam Cloud data was created by a newer game version"
					 : "Steam Cloud profile is invalid; local progress was kept",
				 sizeof(m_aCloudStatus));
		m_CloudPaused = true;
		return;
	}
	const bool LocalDefault = m_CloudLocalSummary.m_ResearchPoints == 0 && m_CloudLocalSummary.m_HighestInvasion == 0 &&
							  m_CloudLocalSummary.m_TutorialCompletedMask == 0 &&
							  str_comp(g_Config.m_PlayerName, "bloodless") == 0;
	const ECloudProfileSyncDecision Decision = CloudProfileDecide(
		m_CloudLocalSummary.m_ContentHash, m_CloudRemoteSummary.m_ContentHash, m_CloudSyncedHash, LocalDefault);
	if(Decision == CLOUD_SYNC_CURRENT)
	{
		SaveCloudSyncState(m_CloudRemoteSummary.m_ContentHash, m_CloudRemoteSummary.m_Revision);
		str_copy(m_aCloudStatus, "Steam Cloud is up to date", sizeof(m_aCloudStatus));
		return;
	}
	if(Decision == CLOUD_SYNC_APPLY_REMOTE)
	{
		BackupCloudProfile(m_aCloudLocalProfile, "local");
		CloudProfileApply(m_aCloudRemoteProfile, RemoteSize, m_pClient->m_pBinds, &m_CloudRemoteSummary);
		m_pClient->m_pPveRoguelite->FlushPersistentProgress();
		SaveCloudSyncState(m_CloudRemoteSummary.m_ContentHash, m_CloudRemoteSummary.m_Revision);
		str_copy(m_aCloudStatus, "Steam Cloud profile applied", sizeof(m_aCloudStatus));
		return;
	}
	if(Decision == CLOUD_SYNC_UPLOAD_LOCAL)
	{
		UploadCloudProfile();
		return;
	}
	m_CloudConflict = true;
	m_Popup = POPUP_CLOUD_CONFLICT;
	str_copy(m_aCloudStatus, "Steam Cloud conflict needs your choice", sizeof(m_aCloudStatus));
}

void CMenus::PumpCloudProfile(bool Force)
{
	const int64 Now = time_get();
	if(!m_CloudInitialized)
	{
		if(Force || Now >= m_CloudNextCheck)
			InitCloudProfile();
		return;
	}
	if(m_CloudConflict || m_CloudPaused)
		return;
	if(!Force && Now < m_CloudNextCheck)
		return;
	m_CloudNextCheck = Now + time_freq() * 5;
	char aCurrent[64 * 1024];
	CCloudProfileSummary Current;
	if(!CloudProfileBuild(m_pClient->m_pBinds, m_CloudRevision, time_timestamp(), aCurrent, sizeof(aCurrent), &Current))
		return;
	if(Current.m_ContentHash != m_CloudSyncedHash)
		m_CloudDirty = true;
	if(m_CloudDirty && (Force || Client()->State() == IClient::STATE_OFFLINE))
		UploadCloudProfile();
}

void CMenus::UpdatePlaySnapshots()
{
	// Replacing a snapshot only after a refresh completes avoids transient empty
	// lists while master or LAN discovery is still in flight.
	if(!ServerBrowser()->IsRefreshing())
	{
		const int Collection =
			clamp(m_PlayBrowserCollection, (int)PLAY_COLLECTION_INTERNET, (int)PLAY_COLLECTION_FAVORITES);
		int Count = 0;
		for(int i = 0; i < ServerBrowser()->NumSortedServers() && Count < MAX_PLAY_SERVER_SNAPSHOTS; i++)
		{
			const CServerInfo *pInfo = ServerBrowser()->SortedGet(i);
			if(!pInfo)
				continue;
			CPlayServerSnapshot &Snapshot = m_aaPlayServerSnapshots[Collection][Count++];
			Snapshot.m_NetAddr = pInfo->m_NetAddr;
			Snapshot.m_Collection = Collection;
			Snapshot.m_MaxClients = pInfo->m_MaxClients;
			Snapshot.m_NumClients = pInfo->m_NumClients;
			Snapshot.m_Flags = pInfo->m_Flags;
			Snapshot.m_Latency = pInfo->m_Latency;
			Snapshot.m_DiscoverySources = pInfo->m_DiscoverySources;
			Snapshot.m_AuthPolicy = pInfo->m_AuthPolicy;
			Snapshot.m_Official = pInfo->m_Official;
			Snapshot.m_Modded = pInfo->m_Modded;
			Snapshot.m_Favorite = pInfo->m_Favorite;
			str_copy(Snapshot.m_aAddress, pInfo->m_aAddress, sizeof(Snapshot.m_aAddress));
			str_copy(Snapshot.m_aName, pInfo->m_aName, sizeof(Snapshot.m_aName));
			str_copy(Snapshot.m_aGameType, pInfo->m_aGameType, sizeof(Snapshot.m_aGameType));
			str_copy(Snapshot.m_aMap, pInfo->m_aMap, sizeof(Snapshot.m_aMap));
			str_copy(Snapshot.m_aVersion, pInfo->m_aVersion, sizeof(Snapshot.m_aVersion));
		}
		m_aPlayServerSnapshotCount[Collection] = Count;
	}

	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	if(!pPlatform || !pPlatform->Available())
		return;
	int Count = 0;
	for(int i = 0; i < pPlatform->LobbyCount() && Count < MAX_PLAY_LOBBY_SNAPSHOTS; i++)
		if(pPlatform->LobbyInfo(i, &m_aPlayLobbySnapshots[Count].m_Info))
			Count++;
	m_PlayLobbySnapshotCount = Count;
}

float CMenus::MenuAlpha() const
{
	return g_Config.m_ClMenuAlpha / 100.0f;
}

vec4 CMenus::ThemeBgDeep()
{
	return ms_ColorBgDeep;
}
vec4 CMenus::ThemeBgPanel()
{
	return ms_ColorBgPanel;
}
vec4 CMenus::ThemeBgInset()
{
	return ms_ColorBgInset;
}
vec4 CMenus::ThemeAccent()
{
	return ms_ColorAccent;
}
vec4 CMenus::ThemeAccentDim()
{
	return ms_ColorAccentDim;
}
vec4 CMenus::ThemeDanger()
{
	return ms_ColorDanger;
}
vec4 CMenus::ThemeText()
{
	return ms_ColorText;
}
vec4 CMenus::ThemeTextMuted()
{
	return vec4(0.72f, 0.74f, 0.76f, 1.0f);
}
vec4 CMenus::ThemeResearchAvailable()
{
	return vec4(1.0f, 0.70f, 0.22f, 1.0f);
}
vec4 CMenus::ThemeResearchLocked()
{
	return vec4(0.42f, 0.48f, 0.60f, 1.0f);
}

void CMenus::OpenResearchPage()
{
	s_ResetMenu = false;
	m_Popup = POPUP_NONE;
	if(Client()->State() == IClient::STATE_OFFLINE)
	{
		if(g_Config.m_UiPage != PAGE_RESEARCH)
			m_ResearchReturnPage = g_Config.m_UiPage > 0 ? g_Config.m_UiPage : PAGE_FRONT;
		g_Config.m_UiPage = PAGE_RESEARCH;
	}
	else
	{
		if(m_GamePage != PAGE_RESEARCH)
			m_ResearchReturnGamePage = m_GamePage > 0 ? m_GamePage : PAGE_GAME;
		m_GamePage = PAGE_RESEARCH;
	}
	SetActive(true);
}

void CMenus::CloseResearchPage()
{
	if(Client()->State() == IClient::STATE_OFFLINE)
		g_Config.m_UiPage = m_ResearchReturnPage == PAGE_RESEARCH ? PAGE_FRONT : m_ResearchReturnPage;
	else
		m_GamePage = m_ResearchReturnGamePage == PAGE_RESEARCH ? PAGE_GAME : m_ResearchReturnGamePage;
}

bool CMenus::IsResearchPageActive() const
{
	if(!m_MenuActive)
		return false;
	const bool Offline = Client()->State() == IClient::STATE_OFFLINE;
	return Offline ? g_Config.m_UiPage == PAGE_RESEARCH : m_GamePage == PAGE_RESEARCH;
}

void CMenus::DrawTechShape(const CUIRect *pRect, const vec4 &Color, float Cut)
{
	if(!pRect || pRect->w <= 0.0f || pRect->h <= 0.0f || Color.a <= 0.0f)
		return;

	Cut = clamp(Cut, 0.0f, min(pRect->w, pRect->h) * 0.45f);
	if(Cut < 0.25f)
	{
		RenderTools()->DrawUIRect(pRect, Color, 0, 0.0f);
		return;
	}

	const float x = pRect->x;
	const float y = pRect->y;
	const float w = pRect->w;
	const float h = pRect->h;
	IGraphics::CFreeformItem aShape[3] = {
		IGraphics::CFreeformItem(x, y + Cut, x + Cut, y, x, y + h - Cut, x + Cut, y + h),
		IGraphics::CFreeformItem(x + Cut, y, x + w - Cut, y, x + Cut, y + h, x + w - Cut, y + h),
		IGraphics::CFreeformItem(x + w - Cut, y, x + w, y + Cut, x + w - Cut, y + h, x + w, y + h - Cut)};
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
	Graphics()->QuadsDrawFreeform(aShape, 3);
	Graphics()->QuadsEnd();
}

void CMenus::DrawTechOutline(const CUIRect *pRect, const vec4 &Top, const vec4 &Bottom, float Cut)
{
	if(!pRect || pRect->w <= 0.0f || pRect->h <= 0.0f)
		return;
	Cut = clamp(Cut, 0.0f, min(pRect->w, pRect->h) * 0.45f);
	const float x = pRect->x;
	const float y = pRect->y;
	const float w = pRect->w;
	const float h = pRect->h;
	IGraphics::CLineItem aTop[3] = {IGraphics::CLineItem(x, y + Cut, x + Cut, y),
									IGraphics::CLineItem(x + Cut, y, x + w - Cut, y),
									IGraphics::CLineItem(x + w - Cut, y, x + w, y + Cut)};
	IGraphics::CLineItem aBottom[3] = {IGraphics::CLineItem(x, y + h - Cut, x + Cut, y + h),
									   IGraphics::CLineItem(x + Cut, y + h, x + w - Cut, y + h),
									   IGraphics::CLineItem(x + w - Cut, y + h, x + w, y + h - Cut)};
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Top.r, Top.g, Top.b, Top.a);
	Graphics()->LinesDraw(aTop, 3);
	Graphics()->SetColor(Bottom.r, Bottom.g, Bottom.b, Bottom.a);
	Graphics()->LinesDraw(aBottom, 3);
	Graphics()->LinesEnd();
}

void CMenus::DrawGlassSurface(const CUIRect *pRect, const vec4 &Fill, const vec4 &Border, float Cut, float Depth)
{
	if(!pRect || pRect->w <= 0.0f || pRect->h <= 0.0f)
		return;
	Cut = clamp(Cut, 0.0f, min(pRect->w, pRect->h) * 0.45f);
	if(Depth > 0.0f)
	{
		CUIRect Shadow = *pRect;
		Shadow.x += Depth * 0.45f;
		Shadow.y += Depth;
		DrawTechShape(&Shadow, vec4(0.0f, 0.006f, 0.015f, min(0.52f, 0.24f + Depth * 0.07f)), Cut);
	}

	DrawTechShape(pRect, Border, Cut);
	CUIRect Inner = *pRect;
	Inner.Margin(1.0f, &Inner);
	if(Inner.w <= 0.0f || Inner.h <= 0.0f)
		return;
	DrawTechShape(&Inner, Fill, max(0.0f, Cut - 1.0f));

	CUIRect Frost = Inner;
	Frost.Margin(1.0f, &Frost);
	Frost.h = min(Frost.h * 0.38f, 18.0f);
	if(Frost.w > 2.0f && Frost.h > 2.0f)
		DrawTechShape(&Frost, vec4(0.70f, 0.92f, 1.0f, 0.035f), max(1.0f, Cut * 0.55f));

	vec4 Highlight = ms_ColorGlassLine;
	Highlight.a = max(0.04f, min(Highlight.a, Border.a * 0.28f));
	const float BottomAlpha = min(0.60f, max(0.08f, Border.a * 0.70f));
	DrawTechOutline(pRect, Highlight, vec4(0.0f, 0.015f, 0.030f, BottomAlpha), Cut);
}

void CMenus::DrawTechBrackets(const CUIRect *pRect, const vec4 &Color, float Length, float Inset)
{
	if(!pRect || pRect->w <= 0.0f || pRect->h <= 0.0f || Color.a <= 0.0f)
		return;
	Length = min(Length, min(pRect->w, pRect->h) * 0.28f);
	const float x0 = pRect->x + Inset;
	const float y0 = pRect->y + Inset;
	const float x1 = pRect->x + pRect->w - Inset;
	const float y1 = pRect->y + pRect->h - Inset;
	IGraphics::CLineItem aLines[4] = {IGraphics::CLineItem(x0, y0 + Length, x0, y0),
									  IGraphics::CLineItem(x0, y0, x0 + Length, y0),
									  IGraphics::CLineItem(x1 - Length, y1, x1, y1),
									  IGraphics::CLineItem(x1, y1, x1, y1 - Length)};
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
	Graphics()->LinesDraw(aLines, 4);
	Graphics()->LinesEnd();
}

void CMenus::DrawMenuBorder(const CUIRect *pRect, const vec4 &Fill, const vec4 &Border, int Corners, float Rounding)
{
	if(Corners == CUI::CORNER_ALL && pRect->w >= 10.0f && pRect->h >= 8.0f)
	{
		DrawGlassSurface(pRect, Fill, Border, max(1.5f, Rounding));
		return;
	}
	RenderTools()->DrawUIRect(pRect, Border, Corners, Rounding);
	CUIRect Inner = *pRect;
	Inner.Margin(1.0f, &Inner);
	RenderTools()->DrawUIRect(&Inner, Fill, Corners, max(0.0f, Rounding - 1.0f));
}

void CMenus::DrawMenuPanel(const CUIRect *pRect, int Corners)
{
	const CUIRect *pScreen = UI()->Screen();
	const bool CanvasSurface =
		Corners == CUI::CORNER_ALL && pScreen && pRect->w >= pScreen->w * 0.52f && pRect->h >= pScreen->h * 0.48f;
	if(CanvasSurface)
	{
		DrawOpenPageFrame(pRect);
		return;
	}

	vec4 Fill = ms_ColorBgPanel;
	Fill.a = max(Fill.a * MenuAlpha(), 0.62f);
	vec4 Border = vec4(0.22f, 0.53f, 0.65f, max(0.62f, 0.82f * MenuAlpha()));
	if(Corners == CUI::CORNER_ALL)
	{
		DrawGlassSurface(pRect, Fill, Border, ms_PanelRounding, 3.2f);
		vec4 Bracket = ms_ColorAccent;
		Bracket.a = 0.19f;
		DrawTechBrackets(pRect, Bracket, 18.0f, 6.0f);
	}
	else
		DrawMenuBorder(pRect, Fill, Border, Corners, ms_PanelRounding);
}

void CMenus::DrawMenuInset(const CUIRect *pRect, int Corners)
{
	const CUIRect *pScreen = UI()->Screen();
	const bool ContentCanvas = Corners == CUI::CORNER_ALL && pScreen && pRect->w >= pScreen->w * .50f &&
		pRect->h >= pScreen->h * .42f;
	if(ContentCanvas)
	{
		vec4 Fill = ms_ColorBgInset;
		Fill.a = .20f * MenuAlpha();
		DrawTechShape(pRect, Fill, min(ms_ControlRounding + 1.0f, pRect->h * .025f));
		vec4 Bracket = ms_ColorAccent;
		Bracket.a = .10f;
		DrawTechBrackets(pRect, Bracket, 16.0f, 3.0f);
		return;
	}
	vec4 Fill = ms_ColorBgInset;
	Fill.a = max(Fill.a * MenuAlpha(), 0.42f);
	vec4 Border = vec4(0.16f, 0.37f, 0.47f, max(0.38f, 0.62f * MenuAlpha()));
	if(Corners == CUI::CORNER_ALL)
		DrawGlassSurface(pRect, Fill, Border, ms_ControlRounding + 1.0f, 1.4f);
	else
		DrawMenuBorder(pRect, Fill, Border, Corners, ms_ControlRounding);
}

void CMenus::DrawSectionHeader(const CUIRect *pRect, int Corners)
{
	vec4 Fill = ms_ColorBgDeep;
	Fill.a = max(0.78f * MenuAlpha(), 0.64f);
	vec4 Border = vec4(0.20f, 0.48f, 0.58f, max(0.56f, 0.76f * MenuAlpha()));
	if(Corners == CUI::CORNER_ALL)
		DrawGlassSurface(pRect, Fill, Border, ms_ControlRounding, 1.0f);
	else
		DrawMenuBorder(pRect, Fill, Border, Corners, ms_ControlRounding);
	DrawAccentUnderline(pRect);
}

void CMenus::DrawOpenPageFrame(const CUIRect *pRect)
{
	if(!pRect || pRect->w <= 0.0f || pRect->h <= 0.0f)
		return;
	vec4 Ambient = ms_ColorBgDeep;
	Ambient.a = 0.055f * MenuAlpha();
	DrawTechShape(pRect, Ambient, min(ms_PanelRounding, pRect->h * .035f));
	vec4 Bracket = ms_ColorAccent;
	Bracket.a = .18f * MenuAlpha();
	DrawTechBrackets(pRect, Bracket, min(24.0f, pRect->h * .06f), 4.0f);
	const float y = pRect->y + 1.0f;
	IGraphics::CLineItem aLines[2] = {
		IGraphics::CLineItem(pRect->x + 18.0f, y, pRect->x + min(210.0f, pRect->w * .24f), y),
		IGraphics::CLineItem(pRect->x + pRect->w - min(84.0f, pRect->w * .10f), y, pRect->x + pRect->w - 18.0f, y)};
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .20f * MenuAlpha());
	Graphics()->LinesDraw(aLines, 2);
	Graphics()->LinesEnd();
}

void CMenus::ConfigureScrollRegion(CScrollRegionParams *pParams) const
{
	pParams->m_ScrollbarBgColor = vec4(0.11f, 0.12f, 0.15f, 0.98f);
	pParams->m_RailBgColor = vec4(0.015f, 0.018f, 0.024f, 1.0f);
	const vec4 Silver = vec4(0.66f, 0.70f, 0.76f, 1.0f);
	pParams->m_SliderColor = MixColor(Silver, ms_ColorAccent, 0.18f);
	pParams->m_SliderColorHover = MixColor(Silver, ms_ColorAccent, 0.55f);
	pParams->m_SliderColorGrabbed = ms_ColorAccent;
}

void CMenus::DrawAccentUnderline(const CUIRect *pRect)
{
	const float y = pRect->y + pRect->h - 1.0f;
	const float Break = pRect->x + pRect->w * 0.68f;
	IGraphics::CLineItem aLines[2] = {
		IGraphics::CLineItem(pRect->x + 5.0f, y, Break, y),
		IGraphics::CLineItem(Break + 5.0f, y, min(pRect->x + pRect->w - 5.0f, Break + 22.0f), y)};
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, ms_ColorAccent.a);
	Graphics()->LinesDraw(aLines, 2);
	Graphics()->LinesEnd();
}

void CMenus::LayoutCenterPanel(CUIRect *pScreen, CUIRect *pOut)
{
	const float MaxW = 1180.0f;
	*pOut = *pScreen;
	if(pOut->w > MaxW)
	{
		const float Side = (pOut->w - MaxW) * 0.5f;
		pOut->VMargin(Side, pOut);
	}
}

vec4 CMenus::MixColor(const vec4 &A, const vec4 &B, float t)
{
	t = clamp(t, 0.0f, 1.0f);
	return vec4(A.r + (B.r - A.r) * t, A.g + (B.g - A.g) * t, A.b + (B.b - A.b) * t, A.a + (B.a - A.a) * t);
}

namespace
{

enum
{
	MENU_ANIM_SLOTS = 1024
};

struct CMenuAnimSlot
{
	const void *m_pID;
	float m_Hover;
	float m_Selected;
	float m_Pressed;
	float m_LastTime;
	float m_FrameDt;
	float m_FrameStamp;
};

CMenuAnimSlot s_aMenuAnims[MENU_ANIM_SLOTS];
bool s_MenuAnimsInit = false;

CMenuAnimSlot *MenuAnimSlot(const void *pID)
{
	if(!s_MenuAnimsInit)
	{
		mem_zero(s_aMenuAnims, sizeof(s_aMenuAnims));
		s_MenuAnimsInit = true;
	}

	int Free = -1;
	float Oldest = 1e9f;
	int OldestIdx = 0;
	for(int i = 0; i < MENU_ANIM_SLOTS; i++)
	{
		if(s_aMenuAnims[i].m_pID == pID)
			return &s_aMenuAnims[i];
		if(!s_aMenuAnims[i].m_pID && Free < 0)
			Free = i;
		if(s_aMenuAnims[i].m_LastTime < Oldest)
		{
			Oldest = s_aMenuAnims[i].m_LastTime;
			OldestIdx = i;
		}
	}

	CMenuAnimSlot *pSlot = &s_aMenuAnims[Free >= 0 ? Free : OldestIdx];
	pSlot->m_pID = pID;
	pSlot->m_Hover = 0.0f;
	pSlot->m_Selected = 0.0f;
	pSlot->m_Pressed = 0.0f;
	pSlot->m_LastTime = 0.0f;
	pSlot->m_FrameDt = 0.0f;
	pSlot->m_FrameStamp = -1.0f;
	return pSlot;
}

float SmoothToward(float Current, float Target, float dt, float Speed)
{
	const float t = 1.0f - expf(-Speed * dt);
	return Current + (Target - Current) * t;
}

float MenuEaseOutCubic(float Amount)
{
	const float Remaining = 1.0f - clamp(Amount, 0.0f, 1.0f);
	return 1.0f - Remaining * Remaining * Remaining;
}

float MenuAnimDt(CMenuAnimSlot *pSlot, float Now)
{
	if(pSlot->m_FrameStamp == Now)
		return pSlot->m_FrameDt;

	float dt = pSlot->m_LastTime > 0.0f ? Now - pSlot->m_LastTime : 0.0f;
	dt = clamp(dt, 0.0f, 0.05f);
	pSlot->m_LastTime = Now;
	pSlot->m_FrameStamp = Now;
	pSlot->m_FrameDt = dt;
	return dt;
}

} // namespace

float CMenus::AnimHover(const void *pID, float Speed)
{
	CMenuAnimSlot *pSlot = MenuAnimSlot(pID);
	const float Now = Client()->LocalTime();
	const float dt = MenuAnimDt(pSlot, Now);

	const float Target = (UI()->HotItem() == pID || UI()->ActiveItem() == pID) ? 1.0f : 0.0f;
	pSlot->m_Hover = SmoothToward(pSlot->m_Hover, Target, dt, Speed);
	if(fabs(pSlot->m_Hover - Target) < 0.001f)
		pSlot->m_Hover = Target;
	return pSlot->m_Hover;
}

float CMenus::AnimSelected(const void *pID, bool Selected, float Speed)
{
	CMenuAnimSlot *pSlot = MenuAnimSlot(pID);
	if(Speed <= 0.0f)
		return pSlot->m_Selected;
	const float Now = Client()->LocalTime();
	const float dt = MenuAnimDt(pSlot, Now);

	pSlot->m_Selected = SmoothToward(pSlot->m_Selected, Selected ? 1.0f : 0.0f, dt, Speed);
	if(fabs(pSlot->m_Selected - (Selected ? 1.0f : 0.0f)) < 0.001f)
		pSlot->m_Selected = Selected ? 1.0f : 0.0f;
	return pSlot->m_Selected;
}

float CMenus::AnimPressed(const void *pID, float Speed)
{
	CMenuAnimSlot *pSlot = MenuAnimSlot(pID);
	const float Now = Client()->LocalTime();
	const float dt = MenuAnimDt(pSlot, Now);
	const float Target = UI()->ActiveItem() == pID ? 1.0f : 0.0f;
	pSlot->m_Pressed = SmoothToward(pSlot->m_Pressed, Target, dt, Speed);
	if(fabs(pSlot->m_Pressed - Target) < 0.001f)
		pSlot->m_Pressed = Target;
	return pSlot->m_Pressed;
}

vec4 CMenus::ButtonColorMul(const void *pID)
{
	const float H = AnimHover(pID);
	const float Press = UI()->ActiveItem() == pID ? 1.0f : 0.0f;
	const float Bright = 1.0f + 0.08f * H - 0.20f * Press;
	return vec4(Bright, Bright, Bright, 1.0f);
}

int CMenus::DoButton_Icon(int ImageId, int SpriteId, const CUIRect *pRect)
{
	Graphics()->TextureSet(g_pData->m_aImages[ImageId].m_Id);

	Graphics()->QuadsBegin();
	RenderTools()->SelectSprite(SpriteId);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	return 0;
}

int CMenus::DoButton_Toggle(const void *pID, int Checked, const CUIRect *pRect, bool Active)
{
	const float Hover = Active ? AnimHover(pID) : 0.0f;
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GUIBUTTONS].m_Id);
	Graphics()->QuadsBegin();
	if(!Active)
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.5f);
	else
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	RenderTools()->SelectSprite(Checked ? SPRITE_GUIBUTTON_ON : SPRITE_GUIBUTTON_OFF);
	IGraphics::CQuadItem QuadItem(pRect->x, pRect->y, pRect->w, pRect->h);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	if(Active && Hover > 0.01f)
	{
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Hover);
		RenderTools()->SelectSprite(SPRITE_GUIBUTTON_HOVER);
		IGraphics::CQuadItem HoverQuad(pRect->x, pRect->y, pRect->w, pRect->h);
		Graphics()->QuadsDrawTL(&HoverQuad, 1);
	}
	Graphics()->QuadsEnd();

	return Active ? UI()->DoButtonLogic(pID, "", Checked, pRect) : 0;
}

int CMenus::DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Style)
{
	const float Hover = AnimHover(pID);
	const float Selected = AnimSelected(pID, Checked != 0);
	const float Press = AnimPressed(pID);
	const float Activity = max(Hover, Selected);
	const bool Ghost = Style == BUTTONSTYLE_GHOST;
	CUIRect VisualRect = *pRect;
	VisualRect.x += Hover * 0.8f;
	VisualRect.y -= Hover * 0.55f;
	VisualRect.Margin(Press * 0.75f, &VisualRect);
	VisualRect.y += Press * 0.8f;

	vec4 FillBase = vec4(0.022f, 0.070f, 0.103f, 0.82f);
	vec4 FillHot = vec4(0.040f, 0.160f, 0.215f, 0.91f);
	vec4 BorderBase = vec4(0.16f, 0.36f, 0.44f, 0.88f);
	vec4 BorderHot = ms_ColorAccent;

	if(Style == BUTTONSTYLE_DANGER)
	{
		FillBase = vec4(0.20f, 0.035f, 0.070f, 0.88f);
		FillHot = vec4(0.37f, 0.070f, 0.115f, 0.95f);
		BorderBase = ms_ColorDanger;
		BorderHot = ms_ColorDanger;
	}
	else if(Style == BUTTONSTYLE_ACCENT)
	{
		FillBase = vec4(0.015f, 0.155f, 0.205f, 0.90f);
		FillHot = vec4(0.025f, 0.285f, 0.355f, 0.97f);
		BorderBase = ms_ColorAccent;
		BorderHot = ms_ColorAccent;
	}
	else if(Ghost)
	{
		FillBase = vec4(0.012f, 0.050f, 0.076f, 0.08f);
		FillHot = vec4(0.025f, 0.145f, 0.190f, 0.54f);
		BorderBase = vec4(0.18f, 0.50f, 0.60f, 0.10f);
		BorderHot = ms_ColorAccent;
	}

	vec4 Fill = MixColor(FillBase, FillHot, max(Hover, Selected * 0.62f));
	vec4 Border = MixColor(BorderBase, BorderHot, max(Hover, Selected * 0.88f));
	if(Press > 0.0f)
		Fill = MixColor(Fill, vec4(Fill.r * 0.72f, Fill.g * 0.76f, Fill.b * 0.80f, Fill.a), Press);

	const float Cut = min(max(3.0f, VisualRect.h * 0.22f), min(VisualRect.w, VisualRect.h) * 0.34f);
	if(Activity > 0.015f)
	{
		const float Expand = 1.0f + Activity * 1.8f;
		CUIRect Glow = VisualRect;
		Glow.x -= Expand;
		Glow.y -= Expand;
		Glow.w += Expand * 2.0f;
		Glow.h += Expand * 2.0f;
		vec4 GlowColor = Style == BUTTONSTYLE_DANGER ? ms_ColorDanger : ms_ColorAccent;
		GlowColor.a = (0.045f + 0.095f * Activity) * (1.0f - Press * 0.55f);
		DrawTechShape(&Glow, GlowColor, Cut + Expand);
	}
	const float Depth =
		Ghost ? max(0.0f, Activity * 1.4f - Press * 0.9f) : max(0.35f, 2.2f + Hover * 0.7f - Press * 1.7f);
	DrawGlassSurface(&VisualRect, Fill, Border, Cut, Depth);

	const float SignalAlpha = (Style == BUTTONSTYLE_ACCENT || Style == BUTTONSTYLE_DANGER) ? 0.92f : Activity * 0.86f;
	if(SignalAlpha > 0.02f)
	{
		vec4 Signal = Style == BUTTONSTYLE_DANGER ? ms_ColorDanger : ms_ColorAccent;
		Signal.a *= SignalAlpha;
		const float NodeSize = min(6.0f, VisualRect.h * 0.18f);
		CUIRect Node = {
			VisualRect.x + Cut * 0.62f, VisualRect.y + (VisualRect.h - NodeSize) * 0.5f, NodeSize, NodeSize};
		DrawTechShape(&Node, Signal, NodeSize * 0.46f);
		IGraphics::CLineItem SignalLine(Node.x + Node.w + 3.0f,
										Node.y + Node.h * 0.5f,
										min(VisualRect.x + VisualRect.w * 0.34f, Node.x + Node.w + 20.0f),
										Node.y + Node.h * 0.5f);
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(Signal.r, Signal.g, Signal.b, Signal.a * 0.55f);
		Graphics()->LinesDraw(&SignalLine, 1);
		Graphics()->LinesEnd();
	}

	CUIRect Temp;
	VisualRect.HMargin(VisualRect.h >= 20.0f ? 2.5f : 1.0f, &Temp);
	Temp.VMargin(min(10.0f, Cut + 1.0f), &Temp);
	float FontSize = min(Temp.h * ms_FontmodHeight, 14.0f);
	FontSize = FitLabelFontSize(TextRender(), pText, FontSize, Temp.w);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	UI()->DoLabel(&Temp, pText, FontSize, 0);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

void CMenus::DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	const float Hover = AnimHover(pID);
	vec4 Fill = MixColor(vec4(0.02f, 0.07f, 0.10f, 0.80f), vec4(0.04f, 0.16f, 0.21f, 0.91f), Hover);
	vec4 Border = MixColor(vec4(0.16f, 0.35f, 0.43f, 0.88f), ms_ColorAccent, Hover);
	const float Cut = min(max(3.0f, pRect->h * 0.20f), min(pRect->w, pRect->h) * 0.34f);
	DrawGlassSurface(pRect, Fill, Border, Cut, 1.0f + Hover);
	CUIRect Temp;
	pRect->HMargin(1.0f, &Temp);
	Temp.VMargin(min(8.0f, Cut), &Temp);
	float FontSize = min(Temp.h * ms_FontmodHeight, 13.0f);
	FontSize = FitLabelFontSize(TextRender(), pText, FontSize, Temp.w);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	UI()->DoLabel(&Temp, pText, FontSize, 0);
}

int CMenus::DoButton_MenuTab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	(void)Corners;
	const bool IsQuit = str_comp(pText, Localize("Quit")) == 0;
	const float Hover = AnimHover(pID);
	const float Sel = AnimSelected(pID, Checked);
	const float Press = AnimPressed(pID);
	CUIRect VisualRect = *pRect;
	VisualRect.x += Hover * 0.6f;
	VisualRect.y -= Hover * 0.35f;
	VisualRect.Margin(Press * 0.55f, &VisualRect);
	VisualRect.y += Press * 0.65f;

	vec4 FillIdle = IsQuit ? vec4(0.20f, 0.035f, 0.070f, 0.84f) : vec4(0.018f, 0.060f, 0.090f, 0.76f);
	vec4 FillOn = IsQuit ? vec4(0.35f, 0.065f, 0.105f, 0.94f) : vec4(0.025f, 0.170f, 0.220f, 0.92f);
	vec4 FillHot = IsQuit ? vec4(0.29f, 0.050f, 0.090f, 0.91f) : vec4(0.040f, 0.135f, 0.185f, 0.88f);

	vec4 BorderIdle = IsQuit ? ms_ColorDanger : vec4(0.14f, 0.32f, 0.40f, 0.84f);
	vec4 BorderOn = IsQuit ? ms_ColorDanger : ms_ColorAccent;

	vec4 Fill = MixColor(FillIdle, FillOn, Sel);
	Fill = MixColor(Fill, FillHot, Hover * (1.0f - Sel * 0.5f));
	vec4 Border = MixColor(BorderIdle, BorderOn, max(Sel, Hover * 0.85f));

	const float Cut = min(max(3.0f, VisualRect.h * 0.22f), min(VisualRect.w, VisualRect.h) * 0.34f);
	const float Activity = max(Sel, Hover);
	if(Activity > 0.02f)
	{
		CUIRect Glow = VisualRect;
		Glow.x -= 1.0f;
		Glow.y -= 1.0f;
		Glow.w += 2.0f;
		Glow.h += 2.0f;
		vec4 GlowColor = IsQuit ? ms_ColorDanger : ms_ColorAccent;
		GlowColor.a = Activity * 0.09f;
		DrawTechShape(&Glow, GlowColor, Cut + 1.0f);
	}
	DrawGlassSurface(&VisualRect, Fill, Border, Cut, max(0.25f, 1.7f + Hover * 0.5f - Press * 1.2f));

	if(!IsQuit && Activity > 0.02f)
	{
		vec4 Accent = ms_ColorAccent;
		Accent.a *= Activity;
		IGraphics::CLineItem Line(VisualRect.x + Cut,
								  VisualRect.y + VisualRect.h - 1.0f,
								  VisualRect.x + VisualRect.w * (0.38f + 0.35f * Sel),
								  VisualRect.y + VisualRect.h - 1.0f);
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(Accent.r, Accent.g, Accent.b, Accent.a);
		Graphics()->LinesDraw(&Line, 1);
		Graphics()->LinesEnd();
	}
	else if(IsQuit)
	{
		vec4 Danger = ms_ColorDanger;
		Danger.a = 0.85f;
		DrawTechBrackets(&VisualRect, Danger, min(8.0f, VisualRect.h * 0.28f), 3.0f);
	}

	CUIRect Temp;
	VisualRect.HMargin(2.0f, &Temp);
	Temp.VMargin(min(9.0f, Cut + 1.0f), &Temp);
	float FontSize = min(Temp.h * ms_FontmodHeight, 13.0f);
	FontSize = FitLabelFontSize(TextRender(), pText, FontSize, Temp.w);
	TextRender()->TextColor(0.96f + 0.02f * Sel, 0.96f + 0.01f * Sel, 0.94f - 0.01f * Sel, 1.0f);
	UI()->DoLabel(&Temp, pText, FontSize, 0);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

int CMenus::DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect, bool Interactive)
{
	const float Sel = AnimSelected(pID, Checked);
	const float Hover = AnimHover(pID);
	if(Sel > 0.02f || Hover > 0.02f)
	{
		vec4 Fill = MixColor(vec4(0.06f, 0.07f, 0.08f, 0.0f), vec4(0.12f, 0.13f, 0.16f, 0.9f), max(Sel, Hover * 0.5f));
		vec4 Border = MixColor(vec4(0.18f, 0.20f, 0.24f, 0.0f), ms_ColorAccent, max(Sel, Hover));
		if(Fill.a > 0.02f)
			DrawMenuBorder(pRect, Fill, Border, CUI::CORNER_T, ms_ControlRounding);
		if(Sel > 0.02f)
		{
			CUIRect Line = *pRect;
			Line.HSplitBottom(2.0f, 0, &Line);
			vec4 Accent = ms_ColorAccent;
			Accent.a *= Sel;
			RenderTools()->DrawUIRect(&Line, Accent, 0, 0.0f);
		}
	}
	CUIRect t;
	pRect->VSplitLeft(5.0f, 0, &t);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	UI()->DoLabel(&t, pText, min(pRect->h * ms_FontmodHeight, 12.0f), -1);
	return Interactive ? UI()->DoButtonLogic(pID, pText, Checked, pRect) : 0;
}

int CMenus::DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect)
{
	CUIRect c = *pRect;
	CUIRect t = *pRect;
	c.w = c.h;
	t.x += c.w;
	t.w -= c.w;
	t.VSplitLeft(5.0f, 0, &t);

	const float Hover = AnimHover(pID);
	const bool Checked = pBoxText[0] == 'X';
	const float Sel = AnimSelected(pID, Checked);

	c.Margin(2.0f, &c);
	vec4 BoxFill = vec4(0.05f, 0.06f, 0.08f, 0.95f);
	vec4 BoxBorder = MixColor(vec4(0.22f, 0.24f, 0.28f, 1.0f), ms_ColorAccent, max(Hover, Sel));
	DrawMenuBorder(&c, BoxFill, BoxBorder, CUI::CORNER_ALL, ms_ControlRounding);
	if(Sel > 0.02f)
	{
		CUIRect Inner = c;
		Inner.Margin(c.h * 0.22f, &Inner);
		vec4 Mark = ms_ColorAccent;
		Mark.a *= Sel;
		RenderTools()->DrawUIRect(&Inner, Mark, CUI::CORNER_ALL, 2.0f);
	}
	else if(pBoxText[0] && pBoxText[0] != 'X')
	{
		TextRender()->TextColor(0.98f, 0.98f, 0.96f, 1.0f);
		UI()->DoLabel(&c, pBoxText, min(pRect->h * ms_FontmodHeight * 0.6f, 12.0f), 0);
	}
	TextRender()->TextColor(0.96f, 0.96f, 0.94f, 1.0f);
	UI()->DoLabel(&t, pText, min(pRect->h * ms_FontmodHeight * 0.8f, 13.0f), -1);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	return UI()->DoButtonLogic(pID, pText, 0, pRect);
}

int CMenus::DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	return DoButton_CheckBox_Common(pID, pText, Checked ? "X" : "", pRect);
}

int CMenus::DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Checked);
	return DoButton_CheckBox_Common(pID, pText, aBuf, pRect);
}

int CMenus::DoEditBox(void *pID,
					  const CUIRect *pRect,
					  char *pStr,
					  unsigned StrSize,
					  float FontSize,
					  float *Offset,
					  bool Hidden,
					  int Corners)
{
	enum
	{
		MAX_EDIT_BINDINGS = 64
	};
	struct CEditBinding
	{
		const void *m_pID;
		char *m_pBoundStr;
		CLineInput m_Input;
	};
	static CEditBinding s_aEditBindings[MAX_EDIT_BINDINGS];
	static int s_NumEditBindings = 0;

	CLineInput *pLineInput = 0;
	for(int i = 0; i < s_NumEditBindings; i++)
	{
		if(s_aEditBindings[i].m_pID == pID)
		{
			pLineInput = &s_aEditBindings[i].m_Input;
			if(s_aEditBindings[i].m_pBoundStr != pStr)
			{
				pLineInput->SetBuffer(pStr, StrSize, StrSize);
				s_aEditBindings[i].m_pBoundStr = pStr;
			}
			break;
		}
	}
	if(!pLineInput && s_NumEditBindings < MAX_EDIT_BINDINGS)
	{
		CEditBinding *pBinding = &s_aEditBindings[s_NumEditBindings++];
		pBinding->m_pID = pID;
		pBinding->m_pBoundStr = pStr;
		pBinding->m_Input.SetBuffer(pStr, StrSize, StrSize);
		pLineInput = &pBinding->m_Input;
	}
	if(!pLineInput)
		return 0;

	pLineInput->SetHidden(Hidden);
	if(Offset)
		pLineInput->SetScrollOffset(*Offset);

	const float Focus = max(AnimHover(pID), UI()->LastActiveItem() == pLineInput ? 1.0f : 0.0f);
	vec4 EditFill = vec4(0.04f, 0.05f, 0.06f, 0.95f);
	vec4 EditBorder = MixColor(vec4(0.18f, 0.20f, 0.24f, 0.95f), ms_ColorAccent, Focus);
	DrawMenuBorder(pRect, EditFill, EditBorder, Corners, ms_ControlRounding);

	bool Changed = false;
	TextRender()->TextColor(0.96f, 0.96f, 0.94f, 1.0f);
	UI()->DoEditBox(pLineInput, pRect, FontSize, Corners, &Changed);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	if(Offset)
		*Offset = pLineInput->GetScrollOffset();
	return Changed ? 1 : 0;
}

float CMenus::DoScrollbarV(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float OffsetY;
	pRect->HSplitTop(33, &Handle, 0);

	Handle.y += (pRect->h - Handle.h) * Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		float Min = pRect->y;
		float Max = pRect->h - Handle.h;
		float Cur = UI()->MouseY() - OffsetY;
		ReturnValue = (Cur - Min) / Max;
		if(ReturnValue < 0.0f)
			ReturnValue = 0.0f;
		if(ReturnValue > 1.0f)
			ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetY = UI()->MouseY() - Handle.y;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	DrawMenuBorder(
		pRect, vec4(0.11f, 0.12f, 0.15f, 0.98f), vec4(0.31f, 0.34f, 0.40f, 0.96f), CUI::CORNER_ALL, ms_ControlRounding);

	CUIRect Rail;
	pRect->VMargin(5.0f, &Rail);
	DrawMenuBorder(&Rail,
				   vec4(0.012f, 0.015f, 0.020f, 1.0f),
				   vec4(0.24f, 0.27f, 0.32f, 0.96f),
				   CUI::CORNER_ALL,
				   ms_ControlRounding);

	CUIRect Slider = Handle;
	Slider.Margin(3.0f, &Slider);
	const float Interaction = max(AnimHover(pID), UI()->ActiveItem() == pID ? 1.0f : 0.0f);
	const vec4 SliderIdle = vec4(0.66f, 0.70f, 0.76f, 1.0f);
	const vec4 SliderCol = MixColor(SliderIdle, ms_ColorAccent, Interaction * 0.72f);
	const vec4 SliderBorder = MixColor(vec4(0.88f, 0.90f, 0.94f, 1.0f), ms_ColorAccent, Interaction);
	DrawMenuBorder(&Slider, SliderCol, SliderBorder, CUI::CORNER_ALL, ms_ControlRounding);

	return ReturnValue;
}

float CMenus::DoScrollbarH(const void *pID, const CUIRect *pRect, float Current)
{
	CUIRect Handle;
	static float OffsetX;
	pRect->VSplitLeft(33, &Handle, 0);

	Handle.x += (pRect->w - Handle.w) * Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		float Min = pRect->x;
		float Max = pRect->w - Handle.w;
		float Cur = UI()->MouseX() - OffsetX;
		ReturnValue = (Cur - Min) / Max;
		if(ReturnValue < 0.0f)
			ReturnValue = 0.0f;
		if(ReturnValue > 1.0f)
			ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetX = UI()->MouseX() - Handle.x;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	DrawMenuBorder(
		pRect, vec4(0.11f, 0.12f, 0.15f, 0.98f), vec4(0.31f, 0.34f, 0.40f, 0.96f), CUI::CORNER_ALL, ms_ControlRounding);

	CUIRect Rail;
	pRect->HMargin(5.0f, &Rail);
	DrawMenuBorder(&Rail,
				   vec4(0.012f, 0.015f, 0.020f, 1.0f),
				   vec4(0.24f, 0.27f, 0.32f, 0.96f),
				   CUI::CORNER_ALL,
				   ms_ControlRounding);

	CUIRect Slider = Handle;
	Slider.Margin(3.0f, &Slider);
	const float Interaction = max(AnimHover(pID), UI()->ActiveItem() == pID ? 1.0f : 0.0f);
	const vec4 SliderIdle = vec4(0.66f, 0.70f, 0.76f, 1.0f);
	const vec4 SliderCol = MixColor(SliderIdle, ms_ColorAccent, Interaction * 0.72f);
	const vec4 SliderBorder = MixColor(vec4(0.88f, 0.90f, 0.94f, 1.0f), ms_ColorAccent, Interaction);
	DrawMenuBorder(&Slider, SliderCol, SliderBorder, CUI::CORNER_ALL, ms_ControlRounding);

	return ReturnValue;
}

float CMenus::DoIndependentDropdownMenu(
	void *pID, CUIRect *pRect, const char *pStr, float HeaderHeight, FDropdownCallback pfnCallback, bool *pActive)
{
	CUIRect View = *pRect;
	CUIRect Header;
	View.HSplitTop(HeaderHeight, &Header, &View);

	RenderTools()->DrawUIRect(
		&Header, vec4(0.06f, 0.07f, 0.09f, 0.95f), *pActive ? CUI::CORNER_T : CUI::CORNER_ALL, ms_ControlRounding);
	{
		CUIRect Border = Header;
		// light top edge for separation
		Border.HSplitTop(1.0f, &Border, 0);
		RenderTools()->DrawUIRect(&Border, vec4(0.18f, 0.20f, 0.24f, 0.8f), 0, 0.0f);
	}
	if(*pActive)
		DrawAccentUnderline(&Header);

	CUIRect Icon;
	Header.VSplitLeft(HeaderHeight, &Icon, &Header);
	Icon.Margin(2.0f, &Icon);
	char aIcon[2] = {*pActive ? '-' : '+', 0};
	UI()->DoLabel(&Icon, aIcon, min(HeaderHeight * 0.65f, 12.0f), 0);

	UI()->DoLabel(&Header, pStr, min(HeaderHeight * 0.65f, 12.0f), -1);

	const bool HeaderClipped = m_pUiClipScrollRegion && m_pUiClipScrollRegion->IsRectClipped(Header);
	if(!HeaderClipped && UI()->DoButtonLogic(pID, &Header))
		*pActive ^= 1;

	if(*pActive)
		return HeaderHeight + (this->*pfnCallback)(View);
	return HeaderHeight;
}

int CMenus::DoKeyReader(void *pID, const CUIRect *pRect, int Key)
{
	// process
	static void *pGrabbedID = 0;
	static bool MouseReleased = true;
	static int ButtonUsed = 0;

	const bool Clipped = m_pUiClipScrollRegion && m_pUiClipScrollRegion->IsRectClipped(*pRect);
	int Inside = Clipped ? 0 : UI()->MouseInside(pRect);
	int NewKey = Key;

	if(!UI()->MouseButton(0) && !UI()->MouseButton(1) && pGrabbedID == pID)
		MouseReleased = true;

	if(UI()->ActiveItem() == pID)
	{
		if(m_Binder.m_GotKey)
		{
			// abort with escape key
			if(m_Binder.m_Key.m_Key != KEY_ESCAPE)
				NewKey = m_Binder.m_Key.m_Key;
			m_Binder.m_GotKey = false;
			UI()->SetActiveItem(0);
			MouseReleased = false;
			pGrabbedID = pID;
		}

		if(ButtonUsed == 1 && !UI()->MouseButton(1))
		{
			if(Inside)
				NewKey = 0;
			UI()->SetActiveItem(0);
		}
	}
	else if(UI()->HotItem() == pID)
	{
		if(MouseReleased)
		{
			if(UI()->MouseButton(0))
			{
				m_Binder.m_TakeKey = true;
				m_Binder.m_GotKey = false;
				UI()->SetActiveItem(pID);
				ButtonUsed = 0;
			}

			if(UI()->MouseButton(1))
			{
				UI()->SetActiveItem(pID);
				ButtonUsed = 1;
			}
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// draw (still show when clipped — graphics clip handles visibility)
	if(UI()->ActiveItem() == pID && ButtonUsed == 0)
		DoButton_KeySelect(pID, "???", 0, pRect);
	else
	{
		if(Key == 0)
			DoButton_KeySelect(pID, "", 0, pRect);
		else
			DoButton_KeySelect(pID, Input()->KeyName(Key), 0, pRect);
	}
	return NewKey;
}

void CMenus::DrawNavigationIcon(const CUIRect &Rect, int Icon, bool Active)
{
	const vec4 Color = Active ? ms_ColorAccent : vec4(0.68f, 0.72f, 0.78f, 1.0f);
	const float x = Rect.x + Rect.w * 0.5f;
	const float y = Rect.y + Rect.h * 0.5f;
	const float s = min(Rect.w, Rect.h) * 0.22f;
	IGraphics::CLineItem aLines[8];
	int Num = 0;
	if(Icon == 0) // command/home
	{
		aLines[Num++] = IGraphics::CLineItem(x - s, y, x, y - s);
		aLines[Num++] = IGraphics::CLineItem(x, y - s, x + s, y);
		aLines[Num++] = IGraphics::CLineItem(x - s * .72f, y - s * .05f, x - s * .72f, y + s);
		aLines[Num++] = IGraphics::CLineItem(x + s * .72f, y - s * .05f, x + s * .72f, y + s);
		aLines[Num++] = IGraphics::CLineItem(x - s * .72f, y + s, x + s * .72f, y + s);
	}
	else if(Icon == 1) // operative
	{
		aLines[Num++] = IGraphics::CLineItem(x - s * .45f, y - s, x + s * .45f, y - s);
		aLines[Num++] = IGraphics::CLineItem(x + s * .45f, y - s, x + s * .65f, y - s * .25f);
		aLines[Num++] = IGraphics::CLineItem(x + s * .65f, y - s * .25f, x, y + s);
		aLines[Num++] = IGraphics::CLineItem(x, y + s, x - s * .65f, y - s * .25f);
		aLines[Num++] = IGraphics::CLineItem(x - s * .65f, y - s * .25f, x - s * .45f, y - s);
	}
	else if(Icon == 2) // progress/research
	{
		aLines[Num++] = IGraphics::CLineItem(x - s, y + s, x - s * .25f, y + s * .2f);
		aLines[Num++] = IGraphics::CLineItem(x - s * .25f, y + s * .2f, x + s * .25f, y + s * .55f);
		aLines[Num++] = IGraphics::CLineItem(x + s * .25f, y + s * .55f, x + s, y - s);
	}
	else if(Icon == 3) // mods/blocks
	{
		aLines[Num++] = IGraphics::CLineItem(x - s, y - s, x, y - s);
		aLines[Num++] = IGraphics::CLineItem(x, y - s, x, y);
		aLines[Num++] = IGraphics::CLineItem(x, y, x + s, y);
		aLines[Num++] = IGraphics::CLineItem(x + s, y, x + s, y + s);
		aLines[Num++] = IGraphics::CLineItem(x + s, y + s, x - s, y + s);
		aLines[Num++] = IGraphics::CLineItem(x - s, y + s, x - s, y - s);
	}
	else if(Icon == 4) // replay
	{
		aLines[Num++] = IGraphics::CLineItem(x - s, y - s * .25f, x - s, y - s);
		aLines[Num++] = IGraphics::CLineItem(x - s, y - s, x - s * .25f, y - s);
		aLines[Num++] = IGraphics::CLineItem(x - s, y - s, x - s * .35f, y - s * .35f);
		aLines[Num++] = IGraphics::CLineItem(x - s * .35f, y - s * .35f, x + s, y + s * .65f);
	}
	else if(Icon == 5) // settings
	{
		aLines[Num++] = IGraphics::CLineItem(x - s, y, x + s, y);
		aLines[Num++] = IGraphics::CLineItem(x, y - s, x, y + s);
		aLines[Num++] = IGraphics::CLineItem(x - s * .7f, y - s * .7f, x + s * .7f, y + s * .7f);
		aLines[Num++] = IGraphics::CLineItem(x + s * .7f, y - s * .7f, x - s * .7f, y + s * .7f);
	}
	else if(Icon == 7) // mode sliders
	{
		aLines[Num++] = IGraphics::CLineItem(x - s, y - s * .65f, x + s, y - s * .65f);
		aLines[Num++] = IGraphics::CLineItem(x - s, y, x + s, y);
		aLines[Num++] = IGraphics::CLineItem(x - s, y + s * .65f, x + s, y + s * .65f);
		aLines[Num++] = IGraphics::CLineItem(x - s * .35f, y - s, x - s * .35f, y - s * .3f);
		aLines[Num++] = IGraphics::CLineItem(x + s * .4f, y - s * .35f, x + s * .4f, y + s * .35f);
		aLines[Num++] = IGraphics::CLineItem(x - s * .15f, y + s * .3f, x - s * .15f, y + s);
	}
	else // exit
	{
		aLines[Num++] = IGraphics::CLineItem(x - s, y - s, x + s, y + s);
		aLines[Num++] = IGraphics::CLineItem(x + s, y - s, x - s, y + s);
		aLines[Num++] = IGraphics::CLineItem(x - s, y, x + s, y);
	}
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
	Graphics()->LinesDraw(aLines, Num);
	Graphics()->LinesEnd();
}

void CMenus::DrawPlayArtwork(const CUIRect &Rect, int Mode, const vec4 &Color)
{
	CUIRect Art = Rect;
	const vec4 ArtFill = vec4(0.010f + Color.r * .055f, 0.024f + Color.g * .055f, 0.040f + Color.b * .055f, .86f);
	const vec4 ArtBorder = vec4(Color.r, Color.g, Color.b, .52f);
	DrawGlassSurface(&Art, ArtFill, ArtBorder, min(9.0f, Art.h * .18f), .8f);
	CUIRect Grid = Art;
	Grid.Margin(4.0f, &Grid);
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, .14f);
	IGraphics::CLineItem aGrid[18];
	int Num = 0;
	for(int i = 1; i < 9; i++)
	{
		const float gx = Grid.x + Grid.w * i / 9.0f;
		aGrid[Num++] = IGraphics::CLineItem(gx, Grid.y, gx, Grid.y + Grid.h);
	}
	for(int i = 1; i < 5; i++)
	{
		const float gy = Grid.y + Grid.h * i / 5.0f;
		aGrid[Num++] = IGraphics::CLineItem(Grid.x, gy, Grid.x + Grid.w, gy);
	}
	Graphics()->LinesDraw(aGrid, Num);
	Graphics()->SetColor(Color.r, Color.g, Color.b, .82f);
	const float cx = Art.x + Art.w * .72f, cy = Art.y + Art.h * .54f, s = min(Art.w, Art.h) * .25f;
	IGraphics::CLineItem aMark[8];
	if(Mode == 0)
	{
		aMark[0] = IGraphics::CLineItem(cx - s, cy + s, cx, cy - s);
		aMark[1] = IGraphics::CLineItem(cx, cy - s, cx + s, cy + s);
		aMark[2] = IGraphics::CLineItem(cx - s * .55f, cy + s * .2f, cx + s * .55f, cy + s * .2f);
		Num = 3;
	}
	else if(Mode == 1)
	{
		aMark[0] = IGraphics::CLineItem(cx - s, cy, cx + s, cy);
		aMark[1] = IGraphics::CLineItem(cx, cy - s, cx, cy + s);
		aMark[2] = IGraphics::CLineItem(cx - s * .7f, cy - s * .7f, cx + s * .7f, cy + s * .7f);
		aMark[3] = IGraphics::CLineItem(cx + s * .7f, cy - s * .7f, cx - s * .7f, cy + s * .7f);
		Num = 4;
	}
	else
	{
		aMark[0] = IGraphics::CLineItem(cx - s, cy - s, cx - s, cy + s);
		aMark[1] = IGraphics::CLineItem(cx + s, cy - s, cx + s, cy + s);
		aMark[2] = IGraphics::CLineItem(cx - s, cy, cx + s, cy);
		aMark[3] = IGraphics::CLineItem(cx - s * .2f, cy - s * .35f, cx + s * .2f, cy);
		aMark[4] = IGraphics::CLineItem(cx + s * .2f, cy, cx - s * .2f, cy + s * .35f);
		Num = 5;
	}
	Graphics()->LinesDraw(aMark, Num);
	Graphics()->LinesEnd();
	// Existing weapon atlas elements anchor the procedural panel in the game's
	// visual language. They stay deliberately faint so replacement key art can
	// be dropped behind the same card content later.
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_WEAPONS].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, .38f);
	RenderTools()->SelectSprite(Mode == 0 ? SPRITE_PICKUP_AMMO : Mode == 1 ? SPRITE_PICKUP_ARMOR : SPRITE_PICKUP_KIT);
	const float SpriteSize = min(Art.h * .62f, Art.w * .18f);
	IGraphics::CQuadItem AtlasSprite(Art.x + Art.w * .12f, Art.y + (Art.h - SpriteSize) * .5f, SpriteSize, SpriteSize);
	Graphics()->QuadsDrawTL(&AtlasSprite, 1);
	Graphics()->QuadsEnd();
	CUIRect Scan = Art;
	Scan.x += 5.0f;
	Scan.w -= 10.0f;
	Scan.y += fmodf(Client()->LocalTime() * 18.0f, max(1.0f, Art.h));
	Scan.h = 1.0f;
	RenderTools()->DrawUIRect(&Scan, vec4(Color.r, Color.g, Color.b, .30f), 0, 0.0f);
	vec4 Bracket = Color;
	Bracket.a = .34f;
	DrawTechBrackets(&Art, Bracket, min(10.0f, Art.h * .22f), 4.0f);
}

void CMenus::DrawModeVoteImage(const CUIRect &Rect, const char *pImage, bool Active)
{
	DrawMenuInset(&Rect, CUI::CORNER_ALL);
	int Texture = -1;
	if(m_pClient->m_pSkins && m_pClient->m_pSkins->NumGameVotes() > 0)
	{
		const int Index = m_pClient->m_pSkins->FindGameVote(pImage);
		if(Index >= 0)
			Texture = m_pClient->m_pSkins->GetGameVote(Index)->m_Texture;
	}
	if(Texture >= 0)
	{
		Graphics()->TextureSet(Texture);
		Graphics()->QuadsBegin();
		const float Brightness = Active ? 1.0f : 0.72f;
		Graphics()->SetColor(Brightness, Brightness, Brightness, 1.0f);
		Graphics()->QuadsSetSubsetFree(0, 0, 1, 0, 0, 1, 1, 1);
		IGraphics::CFreeformItem Image(
			Rect.x, Rect.y, Rect.x + Rect.w, Rect.y, Rect.x, Rect.y + Rect.h, Rect.x + Rect.w, Rect.y + Rect.h);
		Graphics()->QuadsDrawFreeform(&Image, 1);
		Graphics()->QuadsEnd();
	}
	else
	{
		TextRender()->TextColor(ms_ColorAccentDim.r, ms_ColorAccentDim.g, ms_ColorAccentDim.b, 1.0f);
		UI()->DoLabelScaled(
			&Rect, pImage, FitScaledLabelFontSize(TextRender(), pImage, 8.0f, Rect.w - 8.0f, UI()->Scale()), 0);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}

void CMenus::DrawStatusBadge(CUIRect Rect, const char *pText, const vec4 &Color)
{
	const vec4 Fill = vec4(0.014f + Color.r * .05f, 0.035f + Color.g * .05f, 0.052f + Color.b * .05f, .22f);
	const float Cut = min(6.0f, Rect.h * .22f);
	DrawTechShape(&Rect, Fill, Cut);
	DrawTechOutline(&Rect, vec4(Color.r, Color.g, Color.b, .18f), vec4(0.0f, 0.015f, 0.030f, .12f), Cut);
	CUIRect Content = Rect;
	Content.Margin(3.0f, &Content);
	CUIRect Node;
	Content.VSplitLeft(min(12.0f, Content.h), &Node, &Content);
	Node.Margin(max(2.0f, Node.h * .28f), &Node);
	DrawTechShape(&Node, Color, min(Node.w, Node.h) * .45f);
	TextRender()->TextColor(Color.r, Color.g, Color.b, 1.0f);
	UI()->DoLabelScaled(
		&Content, pText, FitScaledLabelFontSize(TextRender(), pText, 9.0f, Content.w - 5.0f, UI()->Scale()), 0);
	TextRender()->TextColor(1, 1, 1, 1);
}

int CMenus::RenderMenubar(CUIRect r)
{
	if(s_ResetMenu)
	{
		g_Config.m_UiPage = PAGE_FRONT;
		s_ResetMenu = false;
	}
	const bool Offline = Client()->State() == IClient::STATE_OFFLINE;
	m_ActivePage = Offline ? g_Config.m_UiPage : m_GamePage;
	const bool Compact = r.w < 142.0f;
	CClientAsyncStatus SteamHostStatus;
	Client()->SteamHostedGameStatus(&SteamHostStatus);
	const bool ManagedLocalGameActive =
		m_LocalServerState == LOCAL_SERVER_STARTING || m_LocalServerState == LOCAL_SERVER_RUNNING;
	const bool SteamHostedGameActive =
		SteamHostStatus.m_State == CLIENT_ASYNC_WORKING || SteamHostStatus.m_State == CLIENT_ASYNC_SUCCEEDED;
	const char *apOfflineLabels[] = {"Play", "Training", "Customize", "Research", "Workshop", "Demos", "Settings"};
	const int aOfflinePages[] = {
		PAGE_LOCAL_SERVER, PAGE_TUTORIAL_SELECT, PAGE_CUSTOMIZE, PAGE_RESEARCH, PAGE_MODS, PAGE_DEMOS, PAGE_SETTINGS};
	const char *apGameLabels[] = {"Continue",
								  "Game",
								  InGameRoomActionLabel(ManagedLocalGameActive, SteamHostedGameActive),
								  "Players",
								  "Server",
								  "Vote",
							  "Research",
								  "Settings",
								  "Leave"};
	const int aGamePages[] = {-2,
							  PAGE_GAME,
							  PAGE_LOCAL_SERVER,
							  PAGE_PLAYERS,
							  PAGE_SERVER_INFO,
							  PAGE_CALLVOTE,
							  PAGE_RESEARCH,
							  PAGE_SETTINGS,
							  -3};
	const char **apLabels = Offline ? apOfflineLabels : apGameLabels;
	const int *pPages = Offline ? aOfflinePages : aGamePages;
	const int Count = Offline ? 7 : 9;
	static int s_aNavigationButtons[9];
	static int s_BackToHome;
	static int s_QuitButton;

	for(int i = 0; i < m_NumInputEvents; i++)
	{
		const IInput::CEvent &Event = m_aInputEvents[i];
		if(!(Event.m_Flags & IInput::FLAG_PRESS))
			continue;
		const bool Up = Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP ||
			Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT;
		const bool Down = Event.m_Key == KEY_DOWN || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN ||
			Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT;
		if(Up || Down)
			m_NavigationHasFocus = true;
		if(m_NavigationHasFocus && Up)
			m_NavigationFocus = (m_NavigationFocus + Count - 1) % Count;
		if(m_NavigationHasFocus && Down)
			m_NavigationFocus = (m_NavigationFocus + 1) % Count;
	}
	m_NavigationFocus = clamp(m_NavigationFocus, 0, Count - 1);

	CUIRect Rail = r;
	Rail.HMargin(2.0f, &Rail);
	vec4 RailFill = ms_ColorBgDeep;
	RailFill.a = (m_ActivePage == PAGE_RESEARCH ? .82f : .28f) * MenuAlpha();
	DrawTechShape(&Rail, RailFill, min(10.0f, Rail.h * .03f));
	vec4 RailLine = ms_ColorAccent;
	RailLine.a = .22f * MenuAlpha();
	DrawTechOutline(&Rail, RailLine, vec4(0.0f, 0.02f, 0.04f, .18f), min(10.0f, Rail.h * .03f));

	CUIRect Header, NavigationItems, StatusArea;
	Rail.HSplitTop(58.0f, &Header, &NavigationItems);
	NavigationItems.HSplitBottom(72.0f, &NavigationItems, &StatusArea);
	Header.Margin(8.0f, &Header);
	if(Offline)
	{
		if(DoButton_Menu(&s_BackToHome, Localize("Back to main menu"), 0, &Header, BUTTONSTYLE_GHOST))
			g_Config.m_UiPage = PAGE_FRONT;
	}
	else
	{
		TextRender()->TextColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, 1.0f);
		UI()->DoLabelScaled(&Header, Compact ? "NS" : "NINSLASH", Compact ? 11.0f : 10.0f, -1);
		TextRender()->TextColor(1, 1, 1, 1);
	}

	NavigationItems.Margin(6.0f, &NavigationItems);
	int NewPage = -1;
	for(int i = 0; i < Count; i++)
	{
		CUIRect Button;
		const float Height = min(44.0f, NavigationItems.h / max(1, Count - i));
		NavigationItems.HSplitTop(Height, &Button, &NavigationItems);
		Button.VMargin(1.5f, &Button);
		const char *pText = Localize(apLabels[i]);
		const bool Focused = m_NavigationHasFocus && m_NavigationFocus == i;
		const bool PageSelected = pPages[i] == m_ActivePage;
		const bool Activated = DoButton_Menu(&s_aNavigationButtons[i],
											 pText,
											 PageSelected || Focused,
											 &Button,
											 pPages[i] == -3 ? BUTTONSTYLE_DANGER : BUTTONSTYLE_GHOST) ||
							   (Focused && m_LastInputDevice != 0 && m_EnterPressed);
		const float SelectedAmount = AnimSelected(&s_aNavigationButtons[i], PageSelected || Focused, 0.0f);
		if(SelectedAmount > 0.01f)
		{
			CUIRect Indicator = Button;
			Indicator.VSplitLeft(2.0f, &Indicator, 0);
			Indicator.Margin(2.0f, &Indicator);
			Indicator.h *= MenuEaseOutCubic(SelectedAmount);
			vec4 IndicatorColor = ms_ColorAccent;
			IndicatorColor.a *= SelectedAmount;
			DrawTechShape(&Indicator, IndicatorColor, min(1.5f, Indicator.w * .45f));
		}
		CUIRect Index = Button;
		Index.VSplitLeft(24.0f, &Index, &Button);
		char aIndex[8];
		str_format(aIndex, sizeof(aIndex), "%02d", i + 1);
		TextRender()->TextColor(ms_ColorAccentDim.r, ms_ColorAccentDim.g, ms_ColorAccentDim.b, PageSelected || Focused ? 1.0f : .56f);
		UI()->DoLabelScaled(&Index, aIndex, Compact ? 7.0f : 7.5f, -1);
		TextRender()->TextColor(1, 1, 1, 1);
		if(Activated)
		{
			m_EnterPressed = false;
			m_NavigationFocus = i;
			if(pPages[i] == -2)
				SetActive(false);
			else if(pPages[i] == -3)
			{
				if(InGameLeaveAction(g_Config.m_ClTutorialActive != 0) == INGAME_LEAVE_OPEN_TUTORIAL_EXIT)
					m_Popup = POPUP_TUTORIAL_EXIT;
				else
				{
					if(SteamHostedGameActive)
						Client()->StopSteamHostedGame();
					Client()->Disconnect();
					g_Config.m_UiPage = PAGE_FRONT;
					m_GamePage = PAGE_GAME;
				}
			}
			else
				NewPage = pPages[i];
		}
	}

	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	char aIdentity[128];
	if(pPlatform && pPlatform->Available())
	{
		str_copy(aIdentity, Localize("Steam · online"), sizeof(aIdentity));
	}
	else
		str_copy(aIdentity, Localize("Standalone · UDP ready"), sizeof(aIdentity));
	StatusArea.Margin(6.0f, &StatusArea);
	CUIRect Status, Quit;
	StatusArea.HSplitTop(28.0f, &Status, &Quit);
	DrawStatusBadge(Status, aIdentity, ms_ColorAccentDim);
	if(Offline)
	{
		if(DoButton_Menu(&s_QuitButton, Localize("Quit"), 0, &Quit, BUTTONSTYLE_GHOST))
			m_Popup = POPUP_QUIT;
	}

		if(NewPage != -1)
		{
			if(Client()->State() == IClient::STATE_OFFLINE)
			{
				if(NewPage == PAGE_RESEARCH)
					OpenResearchPage();
				else
					g_Config.m_UiPage = NewPage;
			if(NewPage == PAGE_LOCAL_SERVER)
			{
				m_PlayTab = 1;
				m_CreateRoomStep = 0;
				m_LocalServerFocus = g_Config.m_ClLocalServerMode;
			}
		}
			else
			{
				if(NewPage == PAGE_RESEARCH)
					OpenResearchPage();
				else
					m_GamePage = NewPage;
			if(NewPage == PAGE_LOCAL_SERVER)
			{
				m_PlayTab = 1;
				m_CreateRoomStep = 0; // CREATE_ROOM_CHOOSE_MODE (declared with the room model below)
				m_LocalServerFocus = g_Config.m_ClLocalServerMode;
			}
		}
	}

	return 0;
}

void CMenus::RenderLoading()
{
	// TODO: not supported right now due to separate render thread

	static int64 LastLoadRender = 0;
	float Percent = m_LoadCurrent++ / (float)m_LoadTotal;

	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	if(time_get() - LastLoadRender < time_freq() / 60)
		return;

	LastLoadRender = time_get();

	// need up date this here to get correct
	// vec3 Rgb = HslToRgb(vec3(g_Config.m_UiColorHue/255.0f, g_Config.m_UiColorSat/255.0f,
	// g_Config.m_UiColorLht/255.0f)); ms_GuiColor = vec4(Rgb.r, Rgb.g, Rgb.b, g_Config.m_UiColorAlpha/255.0f);
	ms_GuiColor = vec4(0.2f, 0.25f, 0.3f, 0.75f);

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	RenderBackground();

	float w = 700;
	float h = 200;
	float x = Screen.w / 2 - w / 2;
	float y = Screen.h / 2 - h / 2;

	Graphics()->BlendNormal();

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	{
		vec4 Panel = ms_ColorBgPanel;
		Graphics()->SetColor(Panel.r, Panel.g, Panel.b, 0.90f);
	}
	RenderTools()->DrawRoundRect(x, y, w, h, 40.0f);
	Graphics()->QuadsEnd();

	const char *pCaption = Localize("Loading");

	CUIRect r;
	r.x = x;
	r.y = y + 20;
	r.w = w;
	r.h = h;
	UI()->DoLabel(&r, pCaption, 48.0f, 0, -1);

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 0.75f);
	RenderTools()->DrawRoundRect(x + 40, y + h - 75, (w - 80) * Percent, 25, 5.0f);
	Graphics()->QuadsEnd();

	Graphics()->Swap();
}

void CMenus::RenderNews(CUIRect MainView)
{
	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_ALL, 10.0f);
}

void CMenus::UpdatedFilteredVideoModes()
{
	// same aspect as desktop -> recommended list (Teeworlds behaviour)
	m_lRecommendedVideoModes.clear();
	m_lOtherVideoModes.clear();

	const int DesktopG = gcd(Graphics()->DesktopWidth(), Graphics()->DesktopHeight());
	const int DesktopWidthG = Graphics()->DesktopWidth() / DesktopG;
	const int DesktopHeightG = Graphics()->DesktopHeight() / DesktopG;

	for(int i = 0; i < m_NumModes; i++)
	{
		const int G = gcd(m_aModes[i].m_Width, m_aModes[i].m_Height);
		if(m_aModes[i].m_Width / G == DesktopWidthG && m_aModes[i].m_Height / G == DesktopHeightG &&
		   m_aModes[i].m_Width <= Graphics()->DesktopWidth() && m_aModes[i].m_Height <= Graphics()->DesktopHeight())
		{
			m_lRecommendedVideoModes.add(m_aModes[i]);
		}
		else
		{
			m_lOtherVideoModes.add(m_aModes[i]);
		}
	}
}

void CMenus::UpdateVideoModeSettings()
{
	m_NumModes = Graphics()->GetVideoModes(m_aModes, MAX_RESOLUTIONS, g_Config.m_GfxScreen);
	UpdatedFilteredVideoModes();
}

void CMenus::OnInit()
{
	UpdateVideoModeSettings();

	/*
	array<string> my_strings;
	array<string>::range r2;
	my_strings.add("4");
	my_strings.add("6");
	my_strings.add("1");
	my_strings.add("3");
	my_strings.add("7");
	my_strings.add("5");
	my_strings.add("2");

	for(array<string>::range r = my_strings.all(); !r.empty(); r.pop_front())
		dbg_msg("", "%s", r.front().cstr());

	sort(my_strings.all());

	dbg_msg("", "after:");
	for(array<string>::range r = my_strings.all(); !r.empty(); r.pop_front())
		dbg_msg("", "%s", r.front().cstr());


	array<int> myarray;
	myarray.add(4);
	myarray.add(6);
	myarray.add(1);
	myarray.add(3);
	myarray.add(7);
	myarray.add(5);
	myarray.add(2);

	for(array<int>::range r = myarray.all(); !r.empty(); r.pop_front())
		dbg_msg("", "%d", r.front());

	sort(myarray.all());
	sort_verify(myarray.all());

	dbg_msg("", "after:");
	for(array<int>::range r = myarray.all(); !r.empty(); r.pop_front())
		dbg_msg("", "%d", r.front());

	exit(-1);
	// */

	// Migrate the former six client-observed checkpoints once. Completed or
	// skipped legacy tutorials suppress the modal, but remain replayable.
	if(g_Config.m_ClTutorialVersion == 0)
	{
		if(g_Config.m_ClTutorialState == 1)
		{
			g_Config.m_ClTutorialChapter =
				TutorialChapterFromLegacy(g_Config.m_ClTutorialState, g_Config.m_ClTutorialCheckpoint);
			g_Config.m_ClTutorialStep = 0;
		}
		if(g_Config.m_ClTutorialState == 2 || g_Config.m_ClTutorialState == 3)
			g_Config.m_ClTutorialPromptHandled = 1;
		g_Config.m_ClTutorialCompletedMask = 0;
		g_Config.m_ClTutorialVersion = TUTORIAL_CONTENT_VERSION;
	}
	if(g_Config.m_ClShowWelcome)
		m_Popup = POPUP_LANGUAGE;
	g_Config.m_ClShowWelcome = 0;

	Console()->Chain("add_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("remove_favorite", ConchainServerbrowserUpdate, this);
	Console()->Chain("add_friend", ConchainFriendlistUpdate, this);
	Console()->Chain("remove_friend", ConchainFriendlistUpdate, this);
	LoadFilterPresets();
	InitCloudProfile();

	// setup load amount
	m_LoadCurrent = 0;
	m_LoadTotal = g_pData->m_NumImages;
	if(!g_Config.m_ClThreadsoundloading)
		m_LoadTotal += g_pData->m_NumSounds;
}

void CMenus::OnConsoleInit()
{
	Console()->Register("local_game_start",
						"?i",
						CFGFLAG_CLIENT,
						ConLocalGameStart,
						this,
						"Start a local game; pass 0 to stay in the menu");
	Console()->Register(
		"local_game_stop", "", CFGFLAG_CLIENT, ConLocalGameStop, this, "Stop the managed local game server");
	Console()->Register("local_game_restart",
						"",
						CFGFLAG_CLIENT,
						ConLocalGameRestart,
						this,
						"Restart and rejoin the managed local game server");
}

void CMenus::PopupMessage(const char *pTopic, const char *pBody, const char *pButton)
{
	// reset active item
	UI()->SetActiveItem(0);

	str_copy(m_aMessageTopic, pTopic, sizeof(m_aMessageTopic));
	str_copy(m_aMessageBody, pBody, sizeof(m_aMessageBody));
	str_copy(m_aMessageButton, pButton, sizeof(m_aMessageButton));
	m_Popup = POPUP_MESSAGE;
}

namespace
{
enum
{
	LOCAL_INVASION_TEAM_CHECKPOINT = 0,
	LOCAL_INVASION_FLOOR_ONE,
	LOCAL_INVASION_CUSTOM_FLOOR,
};

enum
{
	LOCAL_SERVER_ERROR_EXECUTABLE = -1,
	LOCAL_SERVER_ERROR_PORT = -2,
	LOCAL_SERVER_ERROR_TIMEOUT = -3,
};

enum ECreateRoomStep
{
	CREATE_ROOM_CHOOSE_MODE = 0,
	CREATE_ROOM_CONFIGURE,
};

// Challenge code format: MODE-DIFF-SEED-VARIANTS, e.g. "INV-15-1234567-3"
// (mode short name, difficulty, mapgen seed, variant bitmask).
static const char *ChallengeModeCode(int Mode)
{
	switch(Mode)
	{
		case LOCAL_MODE_TUTORIAL: return "TUT";
		case LOCAL_MODE_INVASION: return "INV";
		case LOCAL_MODE_HORDE: return "HOR";
		case LOCAL_MODE_EXTRACTION: return "EXT";
		case LOCAL_MODE_DM: return "DM";
		case LOCAL_MODE_TDM: return "TDM";
		case LOCAL_MODE_CTF: return "CTF";
		case LOCAL_MODE_REACTOR_DEFENSE: return "RDEF";
		case LOCAL_MODE_REACTOR_ASSAULT: return "RASS";
		case LOCAL_MODE_BALL: return "BAL";
		case LOCAL_MODE_BATTLE_ROYALE: return "BR";
		case LOCAL_MODE_GRENADE_DM: return "GDM";
		case LOCAL_MODE_INSTAGIB_CTF: return "ICTF";
		case LOCAL_MODE_ROAM: return "ROAM";
		default: return "INV";
	}
}

static bool ChallengeModeFromCode(const char *pCode, int *pMode)
{
	static const struct
	{
		const char *m_pCode;
		int m_Mode;
	} s_aModes[] = {{"TUT", LOCAL_MODE_TUTORIAL},
		{"INV", LOCAL_MODE_INVASION},
		{"HOR", LOCAL_MODE_HORDE},
		{"EXT", LOCAL_MODE_EXTRACTION},
		{"DM", LOCAL_MODE_DM},
		{"TDM", LOCAL_MODE_TDM},
		{"CTF", LOCAL_MODE_CTF},
		{"RDEF", LOCAL_MODE_REACTOR_DEFENSE},
		{"RASS", LOCAL_MODE_REACTOR_ASSAULT},
		{"BAL", LOCAL_MODE_BALL},
		{"BR", LOCAL_MODE_BATTLE_ROYALE},
		{"GDM", LOCAL_MODE_GRENADE_DM},
		{"ICTF", LOCAL_MODE_INSTAGIB_CTF},
		{"ROAM", LOCAL_MODE_ROAM}};
	for(const auto &Mode : s_aModes)
		if(str_comp_nocase(pCode, Mode.m_pCode) == 0)
		{
			*pMode = Mode.m_Mode;
			return true;
		}
	return false;
}

static bool ChallengeModeAllowed(int Mode)
{
	return Mode != LOCAL_MODE_BALL && Mode != LOCAL_MODE_ROAM;
}

static bool ParseChallengeCode(const char *pCode, int *pMode, int *pDifficulty, int *pSeed, int *pVariants)
{
	char aCode[128];
	str_copy(aCode, pCode, sizeof(aCode));
	char *apParts[4] = {0};
	int Count = 0;
	char *pCursor = aCode;
	while(Count < 4)
	{
		char *pDash = (char *)str_find(pCursor, "-");
		apParts[Count++] = pCursor;
		if(!pDash)
			break;
		*pDash = 0;
		pCursor = pDash + 1;
	}
	if(Count < 4)
		return false;
	if(str_find(apParts[3], "-"))
		return false;

	if(!ChallengeModeFromCode(apParts[0], pMode))
		return false;

	*pDifficulty = clamp(str_toint(apParts[1]), 1, 50);
	*pSeed = clamp(str_toint(apParts[2]), 0, 0x7FFFFFFF);
	*pVariants = clamp(str_toint(apParts[3]), 0, 255);
	return true;
}

// Renders a challenge code from the current settings, e.g. "INV-15-84721-3".
static void FormatChallengeCode(char *pBuf, int Size, int Mode, int Difficulty, int Seed, int Variants)
{
	str_format(pBuf, Size, "%s-%d-%d-%d", ChallengeModeCode(Mode), Difficulty, Seed, Variants);
}

static void ApplyLocalGameModeDefaults(int Mode)
{
	Mode = clamp(Mode, (int)LOCAL_MODE_INVASION, (int)LOCAL_MODE_COUNT - 1);
	const CRoomModeDefaults Defaults = RoomModeDefaults(Mode);
	g_Config.m_ClLocalServerMode = Mode;
	g_Config.m_ClLocalServerMap = 0;
	g_Config.m_ClLocalServerWorkshopMap[0] = 0;
	g_Config.m_ClLocalServerMaxClients = Defaults.m_Players;
	g_Config.m_ClLocalServerDifficulty = Defaults.m_Difficulty;
	g_Config.m_ClLocalServerBots = min(Defaults.m_Bots, Defaults.m_Players - 1);
	if(Mode == LOCAL_MODE_INVASION)
		g_Config.m_ClLocalServerInvasionStart = LOCAL_INVASION_TEAM_CHECKPOINT;
	else if(Mode == LOCAL_MODE_HORDE)
		g_Config.m_ClLocalServerHordeWaves = Defaults.m_Rule;
	else if(Mode == LOCAL_MODE_EXTRACTION)
		g_Config.m_ClLocalServerExtractionTime = Defaults.m_Rule;
	else if(LocalGameMode(Mode).m_Rule == LOCAL_RULE_DM_SCORE)
		g_Config.m_ClLocalServerDmScore = Defaults.m_Rule;
	else if(LocalGameMode(Mode).m_Rule == LOCAL_RULE_TDM_SCORE)
		g_Config.m_ClLocalServerTdmScore = Defaults.m_Rule;
	else if(LocalGameMode(Mode).m_Rule == LOCAL_RULE_CTF_SCORE)
		g_Config.m_ClLocalServerCtfScore = Defaults.m_Rule;
	else if(LocalGameMode(Mode).m_Rule == LOCAL_RULE_REACTOR_SCORE)
		g_Config.m_ClLocalServerReactorScore = Defaults.m_Rule;
	else if(LocalGameMode(Mode).m_Rule == LOCAL_RULE_BALL_SCORE)
		g_Config.m_ClLocalServerBallScore = Defaults.m_Rule;
	else if(LocalGameMode(Mode).m_Rule == LOCAL_RULE_ROAM_CHECKPOINTS)
		g_Config.m_ClLocalServerRoamCheckpoints = Defaults.m_Rule;
}

struct CLocalServerLaunchSettings
{
	int m_Mode;
	int m_Map;
	int m_Port;
	int m_MaxClients;
	int m_Bots;
	int m_Difficulty;
	int m_MapLevel;
	int m_BotLevel;
	int m_InvasionStart;
	int m_InvasionFloor;
	int m_Seed;
	int m_ModeRule;
	bool m_Lan;
	bool m_RandomSeed;
	bool m_MapGen;
	bool m_Roguelite;
	bool m_Contracts;
	bool m_UseCheckpoint;
	const CLocalGameMode *m_pMode;
	const char *m_pConfig;
	const char *m_pMapName;
	const char *m_pMapCommand;
	char m_aName[64];
	char m_aPassword[32];
};

static const char *LocalInvasionConfigForFloor(int Floor)
{
	static const char *s_aConfigs[] = {
		"cfg/invasion1.cfg",
		"cfg/invasion2.cfg",
		"cfg/invasion3.cfg",
		"cfg/invasion4.cfg",
		"cfg/invasion5.cfg",
		"cfg/invasion6.cfg",
		"cfg/invasion7.cfg",
	};
	const int ConfigIndex = (max(1, Floor) - 1) / 10;
	if(ConfigIndex < (int)(sizeof(s_aConfigs) / sizeof(s_aConfigs[0])))
		return s_aConfigs[ConfigIndex];
	return "cfg/invasion-endless.cfg";
}

static int *LocalModeRuleConfig(int Rule)
{
	if(Rule == LOCAL_RULE_HORDE)
		return &g_Config.m_ClLocalServerHordeWaves;
	if(Rule == LOCAL_RULE_EXTRACTION)
		return &g_Config.m_ClLocalServerExtractionTime;
	if(Rule == LOCAL_RULE_DM_SCORE)
		return &g_Config.m_ClLocalServerDmScore;
	if(Rule == LOCAL_RULE_TDM_SCORE)
		return &g_Config.m_ClLocalServerTdmScore;
	if(Rule == LOCAL_RULE_CTF_SCORE)
		return &g_Config.m_ClLocalServerCtfScore;
	if(Rule == LOCAL_RULE_REACTOR_SCORE)
		return &g_Config.m_ClLocalServerReactorScore;
	if(Rule == LOCAL_RULE_BALL_SCORE)
		return &g_Config.m_ClLocalServerBallScore;
	if(Rule == LOCAL_RULE_ROAM_CHECKPOINTS)
		return &g_Config.m_ClLocalServerRoamCheckpoints;
	return 0;
}

static bool LoadWorkshopRoomPreset(const CPlatformWorkshopItem &Item, CRoomPreset *pPreset, char *pError, int ErrorSize)
{
	if(!pPreset || !Item.m_Valid ||
	   (Item.m_ContentType != CONTENT_TYPE_ROOM_PRESET && Item.m_ContentType != CONTENT_TYPE_CHALLENGE))
	{
		str_copy(pError, "Content is not an installed validated room preset", ErrorSize);
		return false;
	}
	char aID[32];
	str_format(aID, sizeof(aID), "%llu", Item.m_PublishedFileID);
	CContentManifest Manifest;
	if(!ContentPackageValidate(Item.m_aInstallPath, aID, GAME_NETVERSION, &Manifest, pError, ErrorSize))
		return false;
	const char *pDefinition = 0;
	for(int i = 0; i < Manifest.m_FileCount; i++)
		if(Manifest.m_aFiles[i].m_Type == CONTENT_FILE_DEFINITION)
		{
			pDefinition = Manifest.m_aFiles[i].m_aPath;
			break;
		}
	if(!pDefinition)
	{
		str_copy(pError, "Preset definition is missing", ErrorSize);
		return false;
	}
	char aPath[1400];
	str_format(aPath, sizeof(aPath), "%s/%s", Item.m_aInstallPath, pDefinition);
	IOHANDLE File = io_open(aPath, IOFLAG_READ);
	if(!File)
	{
		str_copy(pError, "Unable to read preset definition", ErrorSize);
		return false;
	}
	const long Size = io_length(File);
	if(Size <= 0 || Size > 64 * 1024)
	{
		io_close(File);
		str_copy(pError, "Invalid preset definition size", ErrorSize);
		return false;
	}
	char *pJson = (char *)mem_alloc((unsigned)Size + 1, 1);
	const unsigned Read = io_read(File, pJson, (unsigned)Size);
	io_close(File);
	pJson[Read] = 0;
	const bool Result =
		Read == (unsigned)Size &&
		RoomPresetParse(pJson, (int)Size, Item.m_ContentType == CONTENT_TYPE_CHALLENGE, pPreset, pError, ErrorSize);
	mem_free(pJson);
	return Result;
}

static bool LoadWorkshopChallengeDescriptor(const CPlatformWorkshopItem &Item,
											CCommunityChallengeDescriptor *pDescriptor,
											char *pError,
											int ErrorSize)
{
	if(Item.m_ContentType != CONTENT_TYPE_CHALLENGE || !Item.m_Valid || !pDescriptor)
		return false;
	char aID[32];
	str_format(aID, sizeof(aID), "%llu", Item.m_PublishedFileID);
	CContentManifest Manifest;
	if(!ContentPackageValidate(Item.m_aInstallPath, aID, GAME_NETVERSION, &Manifest, pError, ErrorSize))
		return false;
	const char *pDefinition = 0;
	for(int i = 0; i < Manifest.m_FileCount; i++)
		if(Manifest.m_aFiles[i].m_Type == CONTENT_FILE_DEFINITION)
			pDefinition = Manifest.m_aFiles[i].m_aPath;
	if(!pDefinition)
		return false;
	char aPath[1400];
	str_format(aPath, sizeof(aPath), "%s/%s", Item.m_aInstallPath, pDefinition);
	IOHANDLE File = io_open(aPath, IOFLAG_READ);
	if(!File)
		return false;
	const long Size = io_length(File);
	if(Size <= 0 || Size > 64 * 1024)
	{
		io_close(File);
		return false;
	}
	char *pJson = (char *)mem_alloc((unsigned)Size + 1, 1);
	const unsigned Read = io_read(File, pJson, (unsigned)Size);
	io_close(File);
	pJson[Read] = 0;
	const bool Result =
		Read == (unsigned)Size && CommunityChallengeParse(pJson, (int)Size, Manifest, pDescriptor, pError, ErrorSize);
	mem_free(pJson);
	return Result;
}

static bool ApplyWorkshopRoomPreset(const CRoomPreset &Preset, int PartySize, char *pSummary, int SummarySize)
{
	int Mode = -1;
	for(int i = 0; i < LocalGameModeCount(); i++)
		if(str_comp(LocalGameMode(i).m_pGameType, Preset.m_aGameType) == 0)
		{
			Mode = i;
			break;
		}
	if(Mode < 0)
	{
		str_copy(pSummary, "Preset uses an unsupported game type", SummarySize);
		return false;
	}
	g_Config.m_ClLocalServerMode = Mode;
	g_Config.m_ClLocalServerMaxClients = clamp(max(Preset.m_MaxPlayers, PartySize), 1, 16);
	g_Config.m_ClLocalServerDifficulty = clamp(Preset.m_Difficulty, 1, 50);
	g_Config.m_ClLocalServerBots = clamp(Preset.m_Bots, 0, max(0, g_Config.m_ClLocalServerMaxClients - 1));
	g_Config.m_ClLocalServerRandomSeed = Preset.m_RandomSeed;
	g_Config.m_ClLocalServerSeed = clamp(Preset.m_Seed, 0, 0x7FFFFFFF);
	g_Config.m_ClLocalServerRoguelite = Preset.m_Roguelite;
	g_Config.m_ClLocalServerContracts = Preset.m_Contracts;
	g_Config.m_ClLocalServerInvasionStart = Preset.m_InvasionStart;
	g_Config.m_ClLocalServerInvasionFloor = Preset.m_InvasionFloor;
	g_Config.m_ClLocalServerWorkshopMap[0] = 0;
	const CLocalGameMode &ModeDef = LocalGameMode(Mode);
	bool FoundMap = false;
	for(int i = 0; i < ModeDef.m_MapCount; i++)
		if(str_comp(ModeDef.m_ppMapCommands[i], Preset.m_aMapLocator) == 0)
		{
			g_Config.m_ClLocalServerMap = i;
			FoundMap = true;
			break;
		}
	if(!FoundMap && str_comp_num(Preset.m_aMapLocator, "workshop:", 9) == 0)
		str_copy(
			g_Config.m_ClLocalServerWorkshopMap, Preset.m_aMapLocator, sizeof(g_Config.m_ClLocalServerWorkshopMap));
	else if(!FoundMap)
	{
		str_copy(pSummary, "Preset map is not valid for this game mode", SummarySize);
		return false;
	}
	int *pRule = LocalModeRuleConfig(ModeDef.m_Rule);
	if(pRule)
		*pRule = Preset.m_ModeRule;
	str_format(pSummary,
			   SummarySize,
			   "Applied %s, %s, %d players, difficulty %d, %d bots, %s seed. Visibility, password and server name were "
			   "left unchanged.",
			   ModeDef.m_pName,
			   Preset.m_aMapLocator,
			   g_Config.m_ClLocalServerMaxClients,
			   Preset.m_Difficulty,
			   g_Config.m_ClLocalServerBots,
			   Preset.m_RandomSeed ? "random" : "fixed");
	return true;
}

static bool LocalRuleUsesScoreLimit(int Rule)
{
	return Rule == LOCAL_RULE_HORDE || Rule == LOCAL_RULE_DM_SCORE || Rule == LOCAL_RULE_TDM_SCORE ||
		   Rule == LOCAL_RULE_CTF_SCORE || Rule == LOCAL_RULE_REACTOR_SCORE || Rule == LOCAL_RULE_BALL_SCORE;
}

static void BuildLocalServerLaunchSettings(CLocalServerLaunchSettings *pSettings)
{
	mem_zero(pSettings, sizeof(*pSettings));
	pSettings->m_Mode = clamp(g_Config.m_ClLocalServerMode, 0, LocalGameModeCount() - 1);
	pSettings->m_pMode = &LocalGameMode(pSettings->m_Mode);
	pSettings->m_pConfig = pSettings->m_pMode->m_pConfig;
	pSettings->m_Map = clamp(g_Config.m_ClLocalServerMap, 0, pSettings->m_pMode->m_MapCount - 1);
	pSettings->m_pMapName = pSettings->m_pMode->m_SelectableMap ? pSettings->m_pMode->m_ppMapNames[pSettings->m_Map]
																: "Automatic by Invasion floor";
	pSettings->m_pMapCommand =
		pSettings->m_pMode->m_SelectableMap ? pSettings->m_pMode->m_ppMapCommands[pSettings->m_Map] : 0;
	if(g_Config.m_ClLocalServerWorkshopMap[0])
	{
		pSettings->m_pMapName = g_Config.m_ClLocalServerWorkshopMap;
		pSettings->m_pMapCommand = g_Config.m_ClLocalServerWorkshopMap;
	}
	pSettings->m_Port = clamp(g_Config.m_ClLocalServerPort, 1024, 65535);
	pSettings->m_MaxClients = clamp(g_Config.m_ClLocalServerMaxClients, 1, 16);
	pSettings->m_Bots = pSettings->m_pMode->m_HasBots ? clamp(g_Config.m_ClLocalServerBots, 0, 16) : 0;
	pSettings->m_Difficulty = clamp(g_Config.m_ClLocalServerDifficulty, 1, 50);
	pSettings->m_BotLevel = clamp(pSettings->m_Difficulty, 1, 30);
	pSettings->m_InvasionStart = clamp(
		g_Config.m_ClLocalServerInvasionStart, (int)LOCAL_INVASION_TEAM_CHECKPOINT, (int)LOCAL_INVASION_CUSTOM_FLOOR);
	pSettings->m_InvasionFloor =
		clamp(g_Config.m_ClLocalServerInvasionFloor, 1, max(1, g_Config.m_ClPveHighestInvasion));
	pSettings->m_Lan = g_Config.m_ClLocalServerLan != 0;
	pSettings->m_RandomSeed = g_Config.m_ClLocalServerRandomSeed != 0;
	pSettings->m_MapGen = pSettings->m_pMode->m_MapGen && !g_Config.m_ClLocalServerWorkshopMap[0];
	pSettings->m_Seed = clamp(g_Config.m_ClLocalServerSeed, 0, 0x7FFFFFFF);
	pSettings->m_Roguelite = pSettings->m_pMode->m_HasRoguelite && g_Config.m_ClLocalServerRoguelite != 0;
	pSettings->m_Contracts = pSettings->m_Roguelite && g_Config.m_ClLocalServerContracts != 0;
	pSettings->m_MapLevel = pSettings->m_Difficulty;
	pSettings->m_ModeRule = RoomModeDefaults(pSettings->m_Mode).m_Rule;
	pSettings->m_UseCheckpoint = false;
	if(pSettings->m_Mode == LOCAL_MODE_TUTORIAL)
	{
		// The multiplayer chapter renders a simulated room form, but the tutorial
		// server itself is strictly single-player.
		pSettings->m_MaxClients = 1;
		pSettings->m_Bots = 0;
		pSettings->m_Difficulty = 1;
		pSettings->m_BotLevel = 1;
		pSettings->m_MapLevel = clamp(g_Config.m_ClTutorialChapter, 1, 6);
		pSettings->m_RandomSeed = false;
		pSettings->m_Seed = TutorialFixedSeed(pSettings->m_MapLevel);
		pSettings->m_Roguelite = true;
		pSettings->m_Contracts = false;
	}
	if(pSettings->m_Mode == LOCAL_MODE_INVASION)
	{
		pSettings->m_UseCheckpoint =
			pSettings->m_Roguelite && pSettings->m_InvasionStart == LOCAL_INVASION_TEAM_CHECKPOINT;
		pSettings->m_MapLevel =
			pSettings->m_InvasionStart == LOCAL_INVASION_CUSTOM_FLOOR ? pSettings->m_InvasionFloor : 1;
		int TemplateFloor = pSettings->m_MapLevel;
		if(pSettings->m_UseCheckpoint)
		{
			const int MaxCheckpoint =
				g_Config.m_ClPveHighestInvasion >= 10 ? (g_Config.m_ClPveHighestInvasion / 10) * 10 + 1 : 1;
			TemplateFloor = clamp(g_Config.m_ClPvePreferredCheckpoint, 1, MaxCheckpoint);
		}
		pSettings->m_pConfig = LocalInvasionConfigForFloor(TemplateFloor);
	}
	int *pRule = LocalModeRuleConfig(pSettings->m_pMode->m_Rule);
	if(pRule)
		pSettings->m_ModeRule = *pRule;
	str_copy(pSettings->m_aName, g_Config.m_ClLocalServerName, sizeof(pSettings->m_aName));
	str_copy(pSettings->m_aPassword, g_Config.m_ClLocalServerPassword, sizeof(pSettings->m_aPassword));

	// Keep saved UI state valid so mouse, keyboard and console launches all use
	// exactly the same settings.
	g_Config.m_ClLocalServerMode = pSettings->m_Mode;
	g_Config.m_ClLocalServerMap = pSettings->m_Map;
	g_Config.m_ClLocalServerPort = pSettings->m_Port;
	g_Config.m_ClLocalServerMaxClients = pSettings->m_MaxClients;
	g_Config.m_ClLocalServerBots = pSettings->m_Bots;
	g_Config.m_ClLocalServerDifficulty = pSettings->m_Difficulty;
	g_Config.m_ClLocalServerInvasionStart = pSettings->m_InvasionStart;
	g_Config.m_ClLocalServerInvasionFloor = pSettings->m_InvasionFloor;
}

static const char *LocalInvasionStartName(int Start)
{
	if(Start == LOCAL_INVASION_FLOOR_ONE)
		return "Floor 1";
	if(Start == LOCAL_INVASION_CUSTOM_FLOOR)
		return "Custom floor";
	return "Team checkpoint";
}

static void
FormatLocalServerSummary(const CLocalServerLaunchSettings &Settings, int Port, char *pBuffer, int BufferSize)
{
	char aStart[64];
	char aRule[64];
	char aSeed[48];
	char aSlots[48];
	char aPopulation[64];
	if(Settings.m_Mode == LOCAL_MODE_TUTORIAL)
		str_copy(aStart, Localize("Guided solo mission"), sizeof(aStart));
	else if(Settings.m_Mode == LOCAL_MODE_INVASION)
	{
		if(Settings.m_InvasionStart == LOCAL_INVASION_TEAM_CHECKPOINT && !Settings.m_UseCheckpoint)
			str_copy(aStart, Localize("Floor 1"), sizeof(aStart));
		else if(Settings.m_InvasionStart == LOCAL_INVASION_CUSTOM_FLOOR)
			str_format(aStart, sizeof(aStart), Localize("Floor %d"), Settings.m_InvasionFloor);
		else
			str_copy(aStart, Localize(LocalInvasionStartName(Settings.m_InvasionStart)), sizeof(aStart));
	}
	else
		str_format(aStart, sizeof(aStart), Localize("Difficulty %d"), Settings.m_Difficulty);

	if(Settings.m_Mode == LOCAL_MODE_HORDE)
	{
		if(Settings.m_ModeRule == 0)
			str_copy(aRule, Localize("Endless"), sizeof(aRule));
		else
			str_format(aRule, sizeof(aRule), Localize("%d waves"), Settings.m_ModeRule);
	}
	else if(Settings.m_Mode == LOCAL_MODE_EXTRACTION)
		str_format(aRule, sizeof(aRule), Localize("%d min"), Settings.m_ModeRule);
	else if(LocalModeRuleConfig(Settings.m_pMode->m_Rule))
		str_format(aRule, sizeof(aRule), Localize("Score %d"), Settings.m_ModeRule);
	else if(Settings.m_Mode == LOCAL_MODE_BATTLE_ROYALE)
		str_copy(aRule, Localize("Last survivor"), sizeof(aRule));
	else if(Settings.m_Mode == LOCAL_MODE_REACTOR_DEFENSE)
		str_copy(aRule, Localize("Defend the reactor"), sizeof(aRule));
	else if(Settings.m_Mode == LOCAL_MODE_ROAM)
		str_format(aRule, sizeof(aRule), Localize("%d checkpoints"), Settings.m_ModeRule);
	else
		str_copy(aRule, Localize(Settings.m_Roguelite ? "Roguelite" : "Classic"), sizeof(aRule));
	if(Settings.m_RandomSeed)
		str_copy(aSeed, Localize("Random seed"), sizeof(aSeed));
	else
		str_format(aSeed, sizeof(aSeed), Localize("Seed %d"), Settings.m_Seed);
	str_format(aSlots, sizeof(aSlots), Localize("%d human slots"), Settings.m_MaxClients);
	if(Settings.m_pMode->m_HasBots)
	{
		if(Settings.m_Bots <= 0)
			str_copy(aPopulation, Localize("No bots"), sizeof(aPopulation));
		else if(LocalGameModeUsesTeamPopulation(Settings.m_Mode))
			str_format(aPopulation, sizeof(aPopulation), Localize("%d players per team"), Settings.m_Bots);
		else
			str_format(aPopulation, sizeof(aPopulation), Localize("Target %d active players"), Settings.m_Bots);
	}
	else
		aPopulation[0] = 0;

	if(Settings.m_Lan && aPopulation[0])
		str_format(pBuffer,
				   BufferSize,
				   "%s · %s · %s · %s · %s · %s · %s · 127.0.0.1:%d / LAN:%d",
				   Localize(Settings.m_pMode->m_pName),
				   Localize(Settings.m_pMapName),
				   aStart,
				   aRule,
				   aPopulation,
				   aSeed,
				   aSlots,
				   Port,
				   Port);
	else if(Settings.m_Lan)
		str_format(pBuffer,
				   BufferSize,
				   "%s · %s · %s · %s · %s · %s · 127.0.0.1:%d / LAN:%d",
				   Localize(Settings.m_pMode->m_pName),
				   Localize(Settings.m_pMapName),
				   aStart,
				   aRule,
				   aSeed,
				   aSlots,
				   Port,
				   Port);
	else if(aPopulation[0])
		str_format(pBuffer,
				   BufferSize,
				   "%s · %s · %s · %s · %s · %s · %s · 127.0.0.1:%d",
				   Localize(Settings.m_pMode->m_pName),
				   Localize(Settings.m_pMapName),
				   aStart,
				   aRule,
				   aPopulation,
				   aSeed,
				   aSlots,
				   Port);
	else
		str_format(pBuffer,
				   BufferSize,
				   "%s · %s · %s · %s · %s · %s · 127.0.0.1:%d",
				   Localize(Settings.m_pMode->m_pName),
				   Localize(Settings.m_pMapName),
				   aStart,
				   aRule,
				   aSeed,
				   aSlots,
				   Port);
}

static bool LocalFileExists(const char *pPath)
{
	IOHANDLE File = io_open(pPath, IOFLAG_READ);
	if(!File)
		return false;
	io_close(File);
	return true;
}

static bool LocalServerPortAvailable(int Port, bool Lan)
{
	NETADDR BindAddress;
	mem_zero(&BindAddress, sizeof(BindAddress));
	BindAddress.type = NETTYPE_IPV4;
	BindAddress.port = Port;
	if(!Lan)
	{
		BindAddress.ip[0] = 127;
		BindAddress.ip[3] = 1;
	}
	NETSOCKET Socket = net_udp_create(BindAddress);
	if(Socket.type == NETTYPE_INVALID)
		return false;
	net_udp_close(Socket);
	return true;
}

static void FindLocalServerExecutable(char *pPath, int PathSize)
{
	char aSibling[512];
	if(fs_executable_path(aSibling, sizeof(aSibling)) == 0 && fs_parent_dir(aSibling) == 0)
	{
#if defined(CONF_FAMILY_WINDOWS)
		str_append(aSibling, "/ninslash_srv.exe", sizeof(aSibling));
#else
		str_append(aSibling, "/ninslash_srv", sizeof(aSibling));
#endif
		if(LocalFileExists(aSibling))
		{
			str_copy(pPath, aSibling, PathSize);
			return;
		}
	}
#if defined(CONF_FAMILY_WINDOWS)
	if(LocalFileExists("ninslash_srv.exe"))
		str_copy(pPath, "ninslash_srv.exe", PathSize);
	else if(LocalFileExists("build/ninslash_srv.exe"))
		str_copy(pPath, "build/ninslash_srv.exe", PathSize);
	else
		str_copy(pPath, "ninslash_srv.exe", PathSize);
#else
	if(LocalFileExists("ninslash_srv"))
		str_copy(pPath, "./ninslash_srv", PathSize);
	else if(LocalFileExists("build/ninslash_srv"))
		str_copy(pPath, "./build/ninslash_srv", PathSize);
	else
		str_copy(pPath, "ninslash_srv", PathSize);
#endif
}

static void EscapeLocalServerValue(const char *pValue, char *pEscaped, int EscapedSize)
{
	int Out = 0;
	if(EscapedSize <= 0)
		return;
	pEscaped[Out++] = '"';
	for(int i = 0; pValue[i] && Out + 2 < EscapedSize; i++)
	{
		unsigned char c = (unsigned char)pValue[i];
		if(c < 32)
			continue;
		if(c == '\\' || c == '"')
			pEscaped[Out++] = '\\';
		pEscaped[Out++] = (char)c;
	}
	if(Out + 1 < EscapedSize)
		pEscaped[Out++] = '"';
	pEscaped[Out] = 0;
}

static void ReadLocalServerLogTail(const char *pPath, char *pBuffer, int BufferSize)
{
	pBuffer[0] = 0;
	IOHANDLE File = io_open(pPath, IOFLAG_READ);
	if(!File)
		return;
	const long Length = io_length(File);
	if(Length <= 0)
	{
		io_close(File);
		return;
	}
	const int ReadSize = min((int)Length, 1023);
	if(Length > ReadSize)
		io_seek(File, Length - ReadSize, IOSEEK_START);
	char aTail[1024];
	const int Bytes = io_read(File, aTail, ReadSize);
	io_close(File);
	aTail[max(0, Bytes)] = 0;
	for(int i = 0; aTail[i]; i++)
		if(aTail[i] == '\r' || aTail[i] == '\n' || aTail[i] == '\t')
			aTail[i] = ' ';
	int Start = max(0, str_length(aTail) - (BufferSize - 1));
	while(aTail[Start] == ' ')
		Start++;
	str_copy(pBuffer, aTail + Start, BufferSize);
}
} // namespace

void CMenus::JoinLocalServer()
{
	if(!m_LocalServerProcess || m_LocalServerState != LOCAL_SERVER_RUNNING || !m_aLocalServerJoinAddress[0])
		return;
	if(IsConnectedToLocalServer())
		return;
	str_copy(g_Config.m_UiServerAddress, m_aLocalServerJoinAddress, sizeof(g_Config.m_UiServerAddress));
	str_copy(g_Config.m_Password, m_aLocalServerPassword, sizeof(g_Config.m_Password));
	Client()->Connect(m_aLocalServerJoinAddress);
}

bool CMenus::IsConnectedToLocalServer() const
{
	if(m_LocalServerActualPort <= 0 || Client()->State() == IClient::STATE_OFFLINE ||
	   Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return false;
	NETADDR Address;
	Client()->GetServerAddress(&Address);
	return net_addr_comp(&Address, &m_LocalServerAddress) == 0;
}

void CMenus::RefreshLocalServerErrorDetail()
{
	ReadLocalServerLogTail(m_aLocalServerLogPath, m_aLocalServerErrorDetail, sizeof(m_aLocalServerErrorDetail));
	if(!m_aLocalServerErrorDetail[0] && m_LocalServerExitCode > 0)
		str_format(m_aLocalServerErrorDetail,
				   sizeof(m_aLocalServerErrorDetail),
				   "Server exited with code %d",
				   m_LocalServerExitCode);
}

void CMenus::StartTutorial(int Chapter, bool Resume)
{
	Chapter = clamp(Chapter, 1, (int)NUM_TUTORIAL_CHAPTERS);
	m_TutorialChapterReplay = TutorialChapterIsReplay(Chapter, g_Config.m_ClTutorialCompletedMask);
	if(!Resume || g_Config.m_ClTutorialChapter != Chapter)
		g_Config.m_ClTutorialStep = 0;
	g_Config.m_ClTutorialState = 1;
	g_Config.m_ClTutorialChapter = Chapter;
	g_Config.m_ClTutorialStep = clamp(g_Config.m_ClTutorialStep, 0, max(0, TutorialStepCount(Chapter) - 1));
	g_Config.m_ClTutorialCheckpoint = Chapter - 1;
	g_Config.m_ClTutorialPromptHandled = 1;
	g_Config.m_ClTutorialActive = 1;
	g_Config.m_ClLocalServerMode = LOCAL_MODE_TUTORIAL;
	g_Config.m_ClLocalServerLan = 0;
	g_Config.m_ClLocalServerRoguelite = Chapter != TUTORIAL_CHAPTER_MULTIPLAYER;
	g_Config.m_ClLocalServerContracts = 0;
	g_Config.m_ClLocalServerSeed = TutorialFixedSeed(Chapter);
	g_Config.m_ClLocalServerMaxClients = 1;
	g_Config.m_ClLocalServerBots = 0;
	g_Config.m_ClLocalServerDifficulty = Chapter == TUTORIAL_CHAPTER_MULTIPLAYER ? 2 : 1;
	str_copy(g_Config.m_ClLocalServerName, "Ninslash Tutorial", sizeof(g_Config.m_ClLocalServerName));
	StartLocalServer(true);
}

void CMenus::StartQuickMatch()
{
	g_Config.m_ClTutorialActive = 0;
	g_Config.m_ClLocalServerMode = LOCAL_MODE_DM;
	g_Config.m_ClLocalServerMap = 0;
	g_Config.m_ClLocalServerMaxClients = 5;
	g_Config.m_ClLocalServerBots = 4;
	g_Config.m_ClLocalServerDifficulty = 3;
	g_Config.m_ClLocalServerDmScore = 15;
	str_copy(g_Config.m_ClLocalServerName, "Quick match", sizeof(g_Config.m_ClLocalServerName));
	StartLocalServer(true);
}

void CMenus::FinishTutorial()
{
	g_Config.m_ClTutorialState = 2;
	g_Config.m_ClTutorialActive = 0;
	ShutdownLocalServer();
	OpenTutorialChapterSelect();
}

void CMenus::OpenTutorialChapterSelect()
{
	s_ResetMenu = false;
	g_Config.m_UiPage = PAGE_TUTORIAL_SELECT;
	SetActive(true);
}

void CMenus::HandleTutorialChapterCompleted(int Chapter, int CompletedMask)
{
	Chapter = clamp(Chapter, 1, (int)NUM_TUTORIAL_CHAPTERS);
	CompletedMask &= TutorialCompletedMaskLimit();
	g_Config.m_ClTutorialCompletedMask = CompletedMask;
	g_Config.m_ClTutorialState = CompletedMask == TutorialCompletedMaskLimit() ? 2 : 1;
	g_Config.m_ClTutorialActive = 0;
	g_Config.m_ClTutorialStep = 0;
	ShutdownLocalServer();

	const int NextChapter = TutorialNextChapter(Chapter, CompletedMask, m_TutorialChapterReplay);
	if(NextChapter != 0)
		StartTutorial(NextChapter, false);
	else
		OpenTutorialChapterSelect();
}

void CMenus::OpenTutorialRoomPractice()
{
	g_Config.m_UiPage = PAGE_LOCAL_SERVER;
	m_GamePage = PAGE_LOCAL_SERVER;
	SetActive(true);
}

void CMenus::OpenPlayHub()
{
	g_Config.m_UiPage = PAGE_FRONT;
	SetActive(true);
}

void CMenus::StartLocalServer(bool AutoJoin)
{
	int ExitCode = 0;
	if(m_LocalServerProcess)
	{
		if(process_running(m_LocalServerProcess, &ExitCode))
		{
			if(AutoJoin)
			{
				m_LocalServerAutoJoin = true;
				m_LocalServerJoinAttempts = 0;
				m_LocalServerJoinRetryTime = time_get();
				if(m_LocalServerState == LOCAL_SERVER_RUNNING)
					JoinLocalServer();
			}
			return;
		}
		process_destroy(m_LocalServerProcess);
		m_LocalServerProcess = 0;
	}

	CLocalServerLaunchSettings Settings;
	BuildLocalServerLaunchSettings(&Settings);
	m_LocalServerActualPort = 0;
	mem_zero(&m_LocalServerAddress, sizeof(m_LocalServerAddress));
	m_aLocalServerJoinAddress[0] = 0;
	m_aLocalServerLogPath[0] = 0;
	m_aLocalServerErrorDetail[0] = 0;
	FormatLocalServerSummary(Settings, Settings.m_Port, m_aLocalServerSummary, sizeof(m_aLocalServerSummary));
	m_LocalServerSummaryLocalized = false;
	int AvailablePort = -1;
	for(int Offset = 0; Offset < 10; Offset++)
	{
		const int Candidate =
			Settings.m_Port + Offset <= 65535 ? Settings.m_Port + Offset : 1024 + Settings.m_Port + Offset - 65536;
		if(LocalServerPortAvailable(Candidate, Settings.m_Lan))
		{
			AvailablePort = Candidate;
			break;
		}
	}
	if(AvailablePort < 0)
	{
		m_LocalServerState = LOCAL_SERVER_FAILED;
		m_LocalServerExitCode = LOCAL_SERVER_ERROR_PORT;
		m_LocalServerStateTime = time_get();
		m_LocalServerAutoJoin = false;
		str_copy(m_aLocalServerErrorDetail,
				 Localize("The preferred port and the next nine ports are already in use."),
				 sizeof(m_aLocalServerErrorDetail));
		return;
	}
	Settings.m_Port = AvailablePort;
	m_LocalServerActualPort = AvailablePort;
	str_format(m_aLocalServerJoinAddress, sizeof(m_aLocalServerJoinAddress), "127.0.0.1:%d", AvailablePort);
	mem_zero(&m_LocalServerAddress, sizeof(m_LocalServerAddress));
	net_addr_from_str(&m_LocalServerAddress, m_aLocalServerJoinAddress);
	str_copy(m_aLocalServerPassword, Settings.m_aPassword, sizeof(m_aLocalServerPassword));
	FormatLocalServerSummary(Settings, AvailablePort, m_aLocalServerSummary, sizeof(m_aLocalServerSummary));

	Storage()->CreateFolder("logs", IStorage::TYPE_SAVE);
	char aTimestamp[64];
	char aRelativeLogPath[128];
	str_timestamp(aTimestamp, sizeof(aTimestamp));
	str_format(aRelativeLogPath,
			   sizeof(aRelativeLogPath),
			   "logs/local_server_%s_%06d.log",
			   aTimestamp,
			   (int)(time_get() % 1000000));
	Storage()->GetCompletePath(
		IStorage::TYPE_SAVE, aRelativeLogPath, m_aLocalServerLogPath, sizeof(m_aLocalServerLogPath));

	char aExecutable[512];
	char aPort[64];
	char aMaxClients[64];
	char aMap[192];
	char aMapGen[64];
	char aDifficulty[64];
	char aBots[64];
	char aBotLevel[64];
	char aRandomSeed[64];
	char aSeed[64];
	char aChallenge[64];
	char aChallengeScript[320];
	char aChallengeHash[100];
	char aChallengeScriptValue[320];
	char aChallengeHashValue[100];
	char aRoguelite[64];
	char aContracts[64];
	char aCheckpoint[64];
	char aTutorialChapter[64];
	char aTutorialStep[64];
	char aTutorialMode[64];
	char aTutorialCompleted[64];
	char aModeRule[64];
	char aNameValue[160];
	char aPasswordValue[96];
	char aName[256];
	char aPassword[160];
	char aLogValue[sizeof(m_aLocalServerLogPath) * 2];
	char aLog[sizeof(m_aLocalServerLogPath) * 2 + 16];
	FindLocalServerExecutable(aExecutable, sizeof(aExecutable));
	str_format(aPort, sizeof(aPort), "sv_port %d", Settings.m_Port);
	str_format(aMaxClients, sizeof(aMaxClients), "sv_max_clients %d", Settings.m_MaxClients);
	if(Settings.m_pMapCommand)
		str_format(aMap, sizeof(aMap), "sv_map %s", Settings.m_pMapCommand);
	str_format(aMapGen, sizeof(aMapGen), "sv_mapgen %d", Settings.m_MapGen ? 1 : 0);
	str_format(aDifficulty, sizeof(aDifficulty), "sv_mapgen_level %d", Settings.m_MapLevel);
	str_format(aBots, sizeof(aBots), "sv_bots %d", Settings.m_Bots);
	str_format(aBotLevel, sizeof(aBotLevel), "sv_botlevel %d", Settings.m_BotLevel);
	str_format(aRandomSeed, sizeof(aRandomSeed), "sv_mapgen_random_seed %d", Settings.m_RandomSeed);
	str_format(aSeed, sizeof(aSeed), "sv_mapgen_seed %d", Settings.m_Seed);
	str_format(aChallenge,
		 sizeof(aChallenge),
		 "sv_challenge_variants %d",
		 ChallengeModeAllowed(Settings.m_Mode) ? g_Config.m_ClChallengeVariants : 0);
	aChallengeScript[0] = 0;
	aChallengeHash[0] = 0;
	if(g_Config.m_ClChallengeScript[0])
	{
		EscapeLocalServerValue(g_Config.m_ClChallengeScript, aChallengeScriptValue, sizeof(aChallengeScriptValue));
		str_format(aChallengeScript, sizeof(aChallengeScript), "sv_challenge_script %s", aChallengeScriptValue);
	}
	if(g_Config.m_ClChallengeContentHash[0])
	{
		EscapeLocalServerValue(g_Config.m_ClChallengeContentHash, aChallengeHashValue, sizeof(aChallengeHashValue));
		str_format(aChallengeHash, sizeof(aChallengeHash), "sv_challenge_content_hash %s", aChallengeHashValue);
	}
	str_format(aRoguelite, sizeof(aRoguelite), "sv_pve_roguelite %d", Settings.m_Roguelite);
	str_format(aContracts, sizeof(aContracts), "sv_pve_contracts %d", Settings.m_Contracts);
	str_format(aCheckpoint, sizeof(aCheckpoint), "sv_invasion_use_checkpoint %d", Settings.m_UseCheckpoint);
	str_format(aTutorialChapter, sizeof(aTutorialChapter), "sv_tutorial_chapter %d", g_Config.m_ClTutorialChapter);
	str_format(aTutorialStep, sizeof(aTutorialStep), "sv_tutorial_step %d", g_Config.m_ClTutorialStep);
	str_format(aTutorialMode, sizeof(aTutorialMode), "sv_tutorial_mode %d", g_Config.m_ClTutorialActive ? 1 : 0);
	str_format(aTutorialCompleted,
			   sizeof(aTutorialCompleted),
			   "sv_tutorial_completed_mask %d",
			   g_Config.m_ClTutorialCompletedMask);
	if(LocalRuleUsesScoreLimit(Settings.m_pMode->m_Rule))
		str_format(aModeRule, sizeof(aModeRule), "sv_scorelimit %d", Settings.m_ModeRule);
	else if(Settings.m_pMode->m_Rule == LOCAL_RULE_EXTRACTION)
		str_format(aModeRule, sizeof(aModeRule), "sv_timelimit %d", Settings.m_ModeRule);
	else if(Settings.m_pMode->m_Rule == LOCAL_RULE_ROAM_CHECKPOINTS)
		str_format(aModeRule, sizeof(aModeRule), "sv_roam_checkpoints %d", Settings.m_ModeRule);
	EscapeLocalServerValue(Settings.m_aName, aNameValue, sizeof(aNameValue));
	EscapeLocalServerValue(Settings.m_aPassword, aPasswordValue, sizeof(aPasswordValue));
	EscapeLocalServerValue(m_aLocalServerLogPath, aLogValue, sizeof(aLogValue));
	str_format(aName, sizeof(aName), "sv_name %s", aNameValue);
	str_format(aPassword, sizeof(aPassword), "password %s", aPasswordValue);
	str_format(aLog, sizeof(aLog), "logfile %s", aLogValue);

	const char *apArguments[40];
	int NumArguments = 0;
	apArguments[NumArguments++] = aExecutable;
	apArguments[NumArguments++] = "-s";
	apArguments[NumArguments++] = "-f";
	apArguments[NumArguments++] = Settings.m_pConfig;
	apArguments[NumArguments++] = "sv_register 0";
	apArguments[NumArguments++] = "sv_register_steam 0";
	// This managed process is a local/LAN game. Steam Relay rooms use the
	// in-process listen server and enforce Steam identity on remote peers there.
	apArguments[NumArguments++] = "sv_steam_auth 0";
	if(!Settings.m_Lan)
		apArguments[NumArguments++] = "bindaddr 127.0.0.1";
	apArguments[NumArguments++] = aPort;
	apArguments[NumArguments++] = aMaxClients;
	if(Settings.m_pMapCommand)
		apArguments[NumArguments++] = aMap;
	apArguments[NumArguments++] = aMapGen;
	apArguments[NumArguments++] = aDifficulty;
	apArguments[NumArguments++] = aBots;
	apArguments[NumArguments++] = aBotLevel;
	apArguments[NumArguments++] = aRandomSeed;
	apArguments[NumArguments++] = aSeed;
	apArguments[NumArguments++] = aChallenge;
	if(aChallengeScript[0])
		apArguments[NumArguments++] = aChallengeScript;
	if(aChallengeHash[0])
		apArguments[NumArguments++] = aChallengeHash;
	apArguments[NumArguments++] = aRoguelite;
	apArguments[NumArguments++] = aContracts;
	apArguments[NumArguments++] = aCheckpoint;
	if(g_Config.m_ClTutorialActive)
	{
		apArguments[NumArguments++] = aTutorialMode;
		apArguments[NumArguments++] = aTutorialChapter;
		apArguments[NumArguments++] = aTutorialStep;
		apArguments[NumArguments++] = aTutorialCompleted;
	}
	if(LocalModeRuleConfig(Settings.m_pMode->m_Rule))
		apArguments[NumArguments++] = aModeRule;
	apArguments[NumArguments++] = aName;
	apArguments[NumArguments++] = aPassword;
	apArguments[NumArguments++] = aLog;
	apArguments[NumArguments] = 0;

	m_LocalServerProcess = process_spawn(aExecutable, apArguments);
	m_LocalServerStateTime = time_get();
	m_LocalServerJoinRetryTime = 0;
	m_LocalServerInfoRequestTime = m_LocalServerStateTime;
	m_LocalServerJoinAttempts = 0;
	m_LocalServerExitCode = 0;
	m_LocalServerRestartPending = false;
	m_LocalServerAutoJoin = AutoJoin;
	if(m_LocalServerProcess)
	{
		m_LocalServerState = LOCAL_SERVER_STARTING;
		ServerBrowser()->Request(m_LocalServerAddress);
		m_LocalServerInfoRequestTime = m_LocalServerStateTime + time_freq() / 2;
		dbg_msg("local-server",
				"started: %s; preferred port %d, actual port %d; log: %s",
				m_aLocalServerSummary,
				g_Config.m_ClLocalServerPort,
				Settings.m_Port,
				m_aLocalServerLogPath);
	}
	else
	{
		m_LocalServerState = LOCAL_SERVER_FAILED;
		m_LocalServerExitCode = LOCAL_SERVER_ERROR_EXECUTABLE;
		RefreshLocalServerErrorDetail();
		dbg_msg("local-server", "could not start '%s'", aExecutable);
	}
}

void CMenus::ConLocalGameStart(IConsole::IResult *pResult, void *pUserData)
{
	CMenus *pSelf = (CMenus *)pUserData;
	pSelf->StartLocalServer(!pResult->NumArguments() || pResult->GetInteger(0) != 0);
}

void CMenus::ConLocalGameStop(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	((CMenus *)pUserData)->StopLocalServer(false);
}

void CMenus::ConLocalGameRestart(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	((CMenus *)pUserData)->StopLocalServer(true);
}

void CMenus::StopLocalServer(bool Restart)
{
	m_LocalServerAutoJoin = false;
	m_LocalServerJoinRetryTime = 0;
	m_LocalServerInfoRequestTime = 0;
	m_LocalServerJoinAttempts = 0;
	m_LocalServerRestartPending = Restart;
	if(IsConnectedToLocalServer())
		Client()->Disconnect();
	if(m_LocalServerProcess && process_running(m_LocalServerProcess, 0))
	{
		process_terminate(m_LocalServerProcess);
		m_LocalServerState = LOCAL_SERVER_STOPPING;
		m_LocalServerStateTime = time_get();
	}
	else
	{
		if(m_LocalServerProcess)
			process_destroy(m_LocalServerProcess);
		m_LocalServerProcess = 0;
		m_LocalServerState = LOCAL_SERVER_STOPPED;
		if(Restart)
			StartLocalServer(true);
	}
}

void CMenus::UpdateLocalServer()
{
	if(!m_LocalServerProcess)
		return;

	int ExitCode = 0;
	if(!process_running(m_LocalServerProcess, &ExitCode))
	{
		const bool Restart = m_LocalServerRestartPending;
		const bool WasStopping = m_LocalServerState == LOCAL_SERVER_STOPPING;
		process_destroy(m_LocalServerProcess);
		m_LocalServerProcess = 0;
		m_LocalServerExitCode = ExitCode;
		m_LocalServerAutoJoin = false;
		m_LocalServerJoinRetryTime = 0;
		m_LocalServerInfoRequestTime = 0;
		m_LocalServerJoinAttempts = 0;
		m_LocalServerRestartPending = false;
		if(Restart)
		{
			m_LocalServerState = LOCAL_SERVER_STOPPED;
			StartLocalServer(true);
		}
		else
		{
			m_LocalServerState = WasStopping ? LOCAL_SERVER_STOPPED : LOCAL_SERVER_FAILED;
			if(!WasStopping)
			{
				RefreshLocalServerErrorDetail();
				dbg_msg("local-server", "server exited with code %d", ExitCode);
			}
		}
		return;
	}

	const int64 Now = time_get();
	const int64 Elapsed = Now - m_LocalServerStateTime;
	if(m_LocalServerState == LOCAL_SERVER_STARTING)
	{
		CServerInfo Info;
		const bool HasInfo = ServerBrowser()->GetServerInfo(m_LocalServerAddress, &Info);
		const bool ConnectedAndLoading = IsConnectedToLocalServer() && Client()->State() >= IClient::STATE_LOADING;
		if(HasInfo || ConnectedAndLoading)
		{
			m_LocalServerState = LOCAL_SERVER_RUNNING;
			m_LocalServerJoinRetryTime = Now;
		}
		else if(Elapsed > time_freq() * 20)
		{
			process_kill(m_LocalServerProcess);
			process_destroy(m_LocalServerProcess);
			m_LocalServerProcess = 0;
			m_LocalServerState = LOCAL_SERVER_FAILED;
			m_LocalServerExitCode = LOCAL_SERVER_ERROR_TIMEOUT;
			m_LocalServerAutoJoin = false;
			RefreshLocalServerErrorDetail();
			return;
		}
		else if(Now >= m_LocalServerInfoRequestTime)
		{
			ServerBrowser()->Request(m_LocalServerAddress);
			m_LocalServerInfoRequestTime = Now + time_freq() / 2;
		}
	}

	if(m_LocalServerState == LOCAL_SERVER_RUNNING && m_LocalServerAutoJoin)
	{
		if(IsConnectedToLocalServer() &&
		   (Client()->State() == IClient::STATE_LOADING || Client()->State() == IClient::STATE_ONLINE))
		{
			m_LocalServerAutoJoin = false;
			m_LocalServerJoinRetryTime = 0;
		}
		else if(!IsConnectedToLocalServer() && Now >= m_LocalServerJoinRetryTime)
		{
			if(m_LocalServerJoinAttempts >= 8)
			{
				m_LocalServerAutoJoin = false;
				m_LocalServerJoinRetryTime = 0;
			}
			else
			{
				m_LocalServerJoinAttempts++;
				m_LocalServerJoinRetryTime = Now + time_freq();
				JoinLocalServer();
			}
		}
	}
	else if(m_LocalServerState == LOCAL_SERVER_STOPPING && Elapsed > time_freq() * 2)
	{
		process_kill(m_LocalServerProcess);
	}
}

void CMenus::ShutdownLocalServer()
{
	m_LocalServerAutoJoin = false;
	m_LocalServerJoinRetryTime = 0;
	m_LocalServerInfoRequestTime = 0;
	m_LocalServerJoinAttempts = 0;
	m_LocalServerRestartPending = false;
	if(m_LocalServerProcess)
	{
		if(IsConnectedToLocalServer())
			Client()->Disconnect();
		if(process_running(m_LocalServerProcess, 0))
		{
			process_terminate(m_LocalServerProcess);
			for(int Attempt = 0; Attempt < 20 && process_running(m_LocalServerProcess, 0); Attempt++)
				thread_sleep(25);
		}
		process_destroy(m_LocalServerProcess);
		m_LocalServerProcess = 0;
	}
	m_LocalServerState = LOCAL_SERVER_STOPPED;
}

void CMenus::CreateConfiguredRoom()
{
	if(g_Config.m_ClTutorialActive && g_Config.m_ClTutorialChapter == TUTORIAL_CHAPTER_MULTIPLAYER)
	{
		// The real form is retained, but chapter six is a local simulation. Keep
		// the tutorial server alive so it can validate nonce and advance state.
		const int Action =
			g_Config.m_ClTutorialStep < 2 ? TUTORIAL_ACTION_UI_ROOM_CREATE : TUTORIAL_ACTION_UI_ROOM_JOIN;
		m_pClient->m_pPveRoguelite->SendTutorialAction(Action, g_Config.m_ClRoomVisibility);
		return;
	}
	const int Visibility = clamp(g_Config.m_ClRoomVisibility, (int)ROOM_VISIBILITY_SOLO, (int)ROOM_VISIBILITY_PUBLIC);
	if(RoomHostKind(Visibility) == ROOM_HOST_LOCAL)
	{
		CClientAsyncStatus SteamHostStatus;
		Client()->SteamHostedGameStatus(&SteamHostStatus);
		if(SteamHostStatus.m_State == CLIENT_ASYNC_WORKING || SteamHostStatus.m_State == CLIENT_ASYNC_SUCCEEDED)
			Client()->StopSteamHostedGame();
		g_Config.m_ClLocalServerLan = Visibility == ROOM_VISIBILITY_LAN;
		if(Visibility == ROOM_VISIBILITY_SOLO)
			g_Config.m_ClLocalServerMaxClients = RoomSlotsForVisibility(Visibility, m_CreateRoomPreviousSlots);
		if(m_LocalServerProcess &&
		   (m_LocalServerState == LOCAL_SERVER_RUNNING || m_LocalServerState == LOCAL_SERVER_STARTING))
			StopLocalServer(true);
		else
			StartLocalServer(true);
		return;
	}

	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	if(!pPlatform || !pPlatform->Available())
		return;
	CPlatformPartyState Party;
	if(pPlatform->PartyState(&Party) && !Party.m_LocalOwner)
		return;
	// A managed local process and the in-process Steam listen server otherwise
	// compete for cl_local_server_port when the player changes visibility.
	// Stop the old host synchronously before handing the room to Steam Relay.
	if(m_LocalServerProcess)
		ShutdownLocalServer();

	CLocalServerLaunchSettings Preview;
	BuildLocalServerLaunchSettings(&Preview);
	CHostGameSettings Settings;
	mem_zero(&Settings, sizeof(Settings));
	Settings.m_Visibility = Visibility == ROOM_VISIBILITY_FRIENDS ? PLATFORM_LOBBY_FRIENDS : PLATFORM_LOBBY_PUBLIC;
	Settings.m_MaxClients = max(Preview.m_MaxClients, pPlatform->PartyMemberCount());
	Settings.m_MaxClients = clamp(Settings.m_MaxClients, 1, (int)MAX_CLIENTS);
	Settings.m_Difficulty = Preview.m_MapLevel;
	Settings.m_Seed = Preview.m_Seed;
	Settings.m_Bots = Preview.m_Bots;
	Settings.m_BotLevel = Preview.m_BotLevel;
	Settings.m_ModeRule = Preview.m_ModeRule;
	Settings.m_RandomSeed = Preview.m_RandomSeed;
	Settings.m_MapGen = Preview.m_MapGen;
	Settings.m_Roguelite = Preview.m_Roguelite;
	Settings.m_Contracts = Preview.m_Contracts;
	Settings.m_UseCheckpoint = Preview.m_UseCheckpoint;
	str_copy(Settings.m_aName, Preview.m_aName, sizeof(Settings.m_aName));
	str_copy(Settings.m_aPassword, Preview.m_aPassword, sizeof(Settings.m_aPassword));
	str_copy(Settings.m_aMap, Preview.m_pMode->m_ppMapCommands[Preview.m_Map], sizeof(Settings.m_aMap));
	str_copy(Settings.m_aGameType, Preview.m_pMode->m_pGameType, sizeof(Settings.m_aGameType));
	str_copy(Settings.m_aConfig, Preview.m_pConfig, sizeof(Settings.m_aConfig));
	str_copy(Settings.m_aModHash, g_Config.m_ClModHash, sizeof(Settings.m_aModHash));
	str_copy(Settings.m_aModIDs, g_Config.m_ClModIds, sizeof(Settings.m_aModIDs));
	str_copy(Settings.m_aChallengeScript, g_Config.m_ClChallengeScript, sizeof(Settings.m_aChallengeScript));
	str_copy(Settings.m_aChallengeContentHash,
		g_Config.m_ClChallengeContentHash,
		sizeof(Settings.m_aChallengeContentHash));
	Client()->StartSteamHostedGame(Settings);
}

void CMenus::RenderCreateRoom(CUIRect MainView)
{
	static int s_aModeButtons[LOCAL_MODE_COUNT];
	static int s_aVisibilityButtons[4];
	static int s_ChangeMode, s_MapPrevious, s_MapNext, s_SlotsPrevious, s_SlotsNext;
	static int s_DifficultyPrevious, s_DifficultyNext, s_BotsPrevious, s_BotsNext;
	static int s_RulePrevious, s_RuleNext, s_InvasionPrevious, s_InvasionNext, s_FloorPrevious, s_FloorNext;
	static int s_PortPrevious, s_PortNext;
	static int s_Advanced, s_RandomSeed, s_Roguelite, s_Contracts;
	static int s_Create, s_Log, s_Stop;
	static float s_NameOffset, s_PasswordOffset, s_SeedOffset;
	static char s_aSeedText[8] = "0";
	static int s_SeedTextValue = -1;
	static float s_ChallengeOffset;
	static char s_aChallengeText[64];
	static int s_ChallengeTextMode = -1;
	static int s_ChallengeTextDifficulty = -1;
	static int s_ChallengeTextSeed = -1;
	static int s_ChallengeTextVariants = -1;
	static int s_ChVLowGrav, s_ChVNoBuild, s_ChVMelee, s_ChVDark;
	const float LayoutDivisor = max(1.0f, UI()->Scale());
	auto L = [LayoutDivisor](float Value)
	{
		return Value / LayoutDivisor;
	};
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	const bool SteamAvailable = pPlatform && pPlatform->Available();
	if(!CLineInput::GetActiveInput())
	{
		for(int EventIndex = 0; EventIndex < m_NumInputEvents; EventIndex++)
		{
			const IInput::CEvent &Event = m_aInputEvents[EventIndex];
			if(!(Event.m_Flags & IInput::FLAG_PRESS))
				continue;
			const bool Left = Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT;
			const bool Right = Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT;
			const bool Up = Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP;
			const bool Down = Event.m_Key == KEY_DOWN || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN;
			const bool Confirm =
				Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A;
			if(Event.m_Key == KEY_GAMEPAD_BUTTON_B)
			{
				if(m_CreateRoomStep == CREATE_ROOM_CONFIGURE)
					m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
				continue;
			}
			if(m_CreateRoomStep == CREATE_ROOM_CHOOSE_MODE)
			{
				const int AllCount = (int)(sizeof(s_aAllLocalModes) / sizeof(s_aAllLocalModes[0]));
				m_LocalServerFocus = clamp(m_LocalServerFocus, (int)s_aAllLocalModes[0], (int)s_aAllLocalModes[AllCount - 1]);
				int FocusIndex = 0;
				while(FocusIndex < AllCount && s_aAllLocalModes[FocusIndex] != m_LocalServerFocus)
					FocusIndex++;
				if(Left || Right || Up || Down)
				{
					const int cols = (MainView.w >= 650.0f) ? 2 : 1;
					if(Up)
						FocusIndex = max(0, FocusIndex - cols);
					else if(Down)
						FocusIndex = min(AllCount - 1, FocusIndex + cols);
					else if(Left)
						FocusIndex = max(0, FocusIndex - 1);
					else if(Right)
						FocusIndex = min(AllCount - 1, FocusIndex + 1);
					m_LocalServerFocus = s_aAllLocalModes[FocusIndex];
				}
				else if(Confirm)
				{
					ApplyLocalGameModeDefaults(m_LocalServerFocus);
					m_CreateRoomPreviousSlots = g_Config.m_ClLocalServerMaxClients;
					m_CreateRoomStep = CREATE_ROOM_CONFIGURE;
				}
			}
			else if(Left || Right)
			{
				int Visibility = g_Config.m_ClRoomVisibility;
				do
					Visibility = (Visibility + (Right ? 1 : 3)) % 4;
				while(!SteamAvailable &&
					  (Visibility == ROOM_VISIBILITY_FRIENDS || Visibility == ROOM_VISIBILITY_PUBLIC));
				if(g_Config.m_ClRoomVisibility != ROOM_VISIBILITY_SOLO)
					m_CreateRoomPreviousSlots = max(2, g_Config.m_ClLocalServerMaxClients);
				const bool LeavingSolo = g_Config.m_ClRoomVisibility == ROOM_VISIBILITY_SOLO;
				g_Config.m_ClRoomVisibility = Visibility;
				g_Config.m_ClLocalServerMaxClients = Visibility == ROOM_VISIBILITY_SOLO ? 1
													 : LeavingSolo ? clamp(m_CreateRoomPreviousSlots, 2, 16)
																   : g_Config.m_ClLocalServerMaxClients;
			}
		}
	}

	if(m_EscapePressed)
	{
		if(m_CreateRoomStep == CREATE_ROOM_CONFIGURE)
			m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
		m_EscapePressed = false;
	}
	if(m_CreateRoomStep != m_LastCreateRoomStep)
	{
		m_LastCreateRoomStep = m_CreateRoomStep;
		m_CreateRoomTransition = 0.0f;
	}
	const float StepDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_CreateRoomTransition = SmoothToward(m_CreateRoomTransition, 1.0f, StepDt, 15.0f);
	if(fabs(m_CreateRoomTransition - 1.0f) < 0.001f)
		m_CreateRoomTransition = 1.0f;

	static float s_CardAnimTimer = 1.0f;
	static int s_CardAnimLastStep = CREATE_ROOM_CONFIGURE;
	if(m_CreateRoomStep != s_CardAnimLastStep)
	{
		s_CardAnimLastStep = m_CreateRoomStep;
		s_CardAnimTimer = 0.0f;
	}
	s_CardAnimTimer = SmoothToward(s_CardAnimTimer, 1.0f, StepDt, 6.0f);

	CUIRect StepRail, Workspace;
	const float StepRailWidth = L(150.0f);
	MainView.VSplitLeft(StepRailWidth, &StepRail, &Workspace);
	Workspace.VSplitLeft(L(10.0f), 0, &Workspace);
	DrawTechShape(&StepRail, vec4(ms_ColorBgInset.r, ms_ColorBgInset.g, ms_ColorBgInset.b, .20f), 10.0f);
	DrawTechOutline(&StepRail,
					vec4(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .24f),
					vec4(0.0f, 0.02f, 0.04f, .18f),
					10.0f);
	StepRail.Margin(L(9.0f), &StepRail);
	CUIRect StepTitle, StepBody;
	StepRail.HSplitTop(L(34.0f), &StepTitle, &StepBody);
	UI()->DoLabelScaled(&StepTitle, Localize("Create room"), 14.0f, -1);
	DrawAccentUnderline(&StepTitle);
	static int s_StepModeButton, s_StepConfigureButton;
	const char *apStepNames[] = {Localize("Choose a game mode"), Localize("Configure room")};
	int *pStepIDs[] = {&s_StepModeButton, &s_StepConfigureButton};
	for(int Step = 0; Step < 2; Step++)
	{
		CUIRect StepRect;
		StepBody.HSplitTop(L(44.0f), &StepRect, &StepBody);
		StepRect.HSplitBottom(L(5.0f), &StepRect, 0);
		char aStepLabel[128];
		str_format(aStepLabel, sizeof(aStepLabel), "%02d  %s", Step + 1, apStepNames[Step]);
		const bool Selected = m_CreateRoomStep == Step;
		const bool Enabled = Step == 0 || m_CreateRoomStep == CREATE_ROOM_CONFIGURE;
		if(DoButton_Menu(pStepIDs[Step], aStepLabel, Selected, &StepRect,
						 Enabled ? (Selected ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_GHOST) : BUTTONSTYLE_GHOST) &&
			Enabled)
		{
			m_CreateRoomStep = Step;
		}
	}
	StepBody.HSplitTop(L(10.0f), 0, &StepBody);
	if(m_CreateRoomStep == CREATE_ROOM_CONFIGURE)
	{
		const int RailMode = clamp(g_Config.m_ClLocalServerMode, (int)LOCAL_MODE_INVASION, (int)LOCAL_MODE_COUNT - 1);
		DrawStatusBadge(StepBody, Localize(LocalGameMode(RailMode).m_pName), ms_ColorAccentDim);
	}

	MainView = Workspace;
	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(L(12.0f), &MainView);
	CUIRect Header, Body, Footer;
	const float LargeScale = max(0.0f, UI()->Scale() - 1.0f);
	const float HeaderHeight = L(48.0f + LargeScale * 84.0f);
	const float StepOffset = L(27.0f + LargeScale * 66.0f);
	MainView.HSplitTop(HeaderHeight, &Header, &Body);
	Body.HSplitBottom(L(72.0f), &Body, &Footer);
	const float StepEase = MenuEaseOutCubic(m_CreateRoomTransition);
	const float StepOffsetX = (1.0f - StepEase) * L(14.0f);
	Header.x += StepOffsetX;
	Header.w -= StepOffsetX;
	Body.x += StepOffsetX;
	Body.w -= StepOffsetX;
	Footer.x += StepOffsetX;
	Footer.w -= StepOffsetX;
	const char *pTitle = m_CreateRoomStep == CREATE_ROOM_CHOOSE_MODE ? "Choose a game mode" : "Configure room";
	UI()->DoLabelScaled(&Header, Localize(pTitle), 22.0f, -1);
	CUIRect StepLabel = Header;
	StepLabel.y += StepOffset;
	UI()->DoLabelScaled(
		&StepLabel, Localize(m_CreateRoomStep == CREATE_ROOM_CHOOSE_MODE ? "Step 1 of 2" : "Step 2 of 2"), 10.0f, -1);
	DrawAccentUnderline(&Header);

	if(m_CreateRoomStep == CREATE_ROOM_CHOOSE_MODE)
	{
		Body.HMargin(L(6.0f), &Body);
		CUIRect ModeGrid, ModePreview;
		const bool HasPreview = Body.w >= 660.0f;
		if(HasPreview)
		{
			Body.VSplitRight(L(244.0f), &ModeGrid, &ModePreview);
			ModeGrid.VSplitRight(L(8.0f), &ModeGrid, 0);
			ModePreview.VSplitLeft(L(8.0f), 0, &ModePreview);
			DrawTechShape(&ModePreview, vec4(ms_ColorBgInset.r, ms_ColorBgInset.g, ms_ColorBgInset.b, .16f), 9.0f);
			DrawTechOutline(&ModePreview,
							vec4(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .18f),
							vec4(0.0f, 0.02f, 0.04f, .12f),
							9.0f);
		}
		else
			ModeGrid = Body;
		const bool SingleColumn = ModeGrid.w < 650.0f;
		static CScrollRegion s_ModeScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CUIRect ModeContent = ModeGrid;
		CScrollRegionParams ScrollParams;
		ConfigureScrollRegion(&ScrollParams);
		ScrollParams.m_ClipBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		ScrollParams.m_ScrollUnit = L(94.0f);
		s_ModeScrollRegion.Begin(&ModeGrid, &ScrollOffset, &ScrollParams);
		ModeContent.y += ScrollOffset.y;
		ModeContent.VSplitRight(L(20.0f), &ModeContent, 0);
		const int AllCount = (int)(sizeof(s_aAllLocalModes) / sizeof(s_aAllLocalModes[0]));
		const int Cols = SingleColumn ? 1 : 2;
		const int Rows = (AllCount + Cols - 1) / Cols;
		const float CardHeight = L(88.0f);
		const float Gap = L(6.0f);
		const float ColGap = L(4.0f);
		CUIRect Group = ModeContent;
		for(int Row = 0; Row < Rows; Row++)
		{
			CUIRect RowRect, LeftCard, RightCard;
			Group.HSplitTop(CardHeight, &RowRect, &Group);
			Group.HSplitTop(Gap, 0, &Group);
			if(Cols == 2)
			{
				CUIRect Junk;
				RowRect.VSplitMid(&LeftCard, &RightCard);
				LeftCard.VSplitRight(ColGap * .5f, &LeftCard, &Junk);
				RightCard.VSplitLeft(ColGap * .5f, &Junk, &RightCard);
			}
			for(int Col = 0; Col < Cols; Col++)
			{
				int Index = Row * Cols + Col;
				if(Index >= AllCount)
					break;
				const int Mode = s_aAllLocalModes[Index];
				CUIRect Card = (Cols == 1) ? RowRect : (Col == 0 ? LeftCard : RightCard);
				const bool Selected = m_LocalServerFocus == Mode;

				// staggered fade-in + slide-up
				float Fade = clamp((s_CardAnimTimer - Index * 0.03f) * 2.5f, 0.0f, 1.0f);
				float Hover = AnimHover(&s_aModeButtons[Mode]);
				Card.y += (1.0f - Fade) * L(10.0f);

				// button underneath, renders selection border when focused
				if(DoButton_Menu(&s_aModeButtons[Mode], "", Selected, &Card, BUTTONSTYLE_ACCENT))
				{
					ApplyLocalGameModeDefaults(Mode);
					m_CreateRoomPreviousSlots = g_Config.m_ClLocalServerMaxClients;
					m_CreateRoomStep = CREATE_ROOM_CONFIGURE;
				}

				// card visual on top
				CUIRect Inner = Card;
				DrawMenuInset(&Inner, CUI::CORNER_ALL);
				float HoverMargin = L(7.0f - Hover * 1.5f);
				Inner.Margin(HoverMargin, &Inner);
				CUIRect Preview, Content;
				Inner.VSplitLeft(L(88.0f), &Preview, &Content);
				const float PreviewHeight = min(Preview.h, L(44.0f));
				Preview.y += (Preview.h - PreviewHeight) * 0.5f;
				Preview.h = PreviewHeight;
				DrawModeVoteImage(Preview, s_aLocalGameModes[Mode].m_pGameVoteImage, Selected);
				Content.VSplitLeft(L(8.0f), 0, &Inner);
				CUIRect Top;
				Inner.HSplitTop(L(20.0f), &Top, &Inner);
				UI()->DoLabelScaled(&Top, Localize(s_aLocalGameModes[Mode].m_pName), 14.0f, -1);
				Inner.HSplitTop(L(25.0f), &Top, &Inner);
				UI()->DoLabelScaled(&Top, Localize(s_aLocalGameModes[Mode].m_pDescription), 9.0f, -1, (int)Top.w);
				Inner.HSplitTop(L(16.0f), &Top, &Inner);
				char aMeta[160];
				str_format(aMeta,
						   sizeof(aMeta),
						   Localize("Recommended: %s players  ·  %s  ·  %s difficulty"),
						   s_aLocalGameModes[Mode].m_pRecommendedPlayers,
						   Localize(s_aLocalGameModes[Mode].m_pDuration),
						   Localize(s_aLocalGameModes[Mode].m_pRecommendedDifficulty));
				UI()->DoLabelScaled(&Top, aMeta, 8.5f, -1);
				Inner.HSplitTop(L(15.0f), &Top, &Inner);
				UI()->DoLabelScaled(&Top, Localize(s_aLocalGameModes[Mode].m_pMechanics), 8.5f, -1);
			}
		}
		CUIRect ScrollContent = ModeContent;
		ScrollContent.h = Rows * (CardHeight + Gap) - Gap;
		s_ModeScrollRegion.AddRect(ScrollContent);
		s_ModeScrollRegion.End();
		DrawMenuInset(&Footer, CUI::CORNER_ALL);
		Footer.Margin(L(9.0f), &Footer);
		UI()->DoLabelScaled(&Footer, Localize("Training is available from the Play hub."), 10.0f, -1);
		if(HasPreview)
		{
			const int AllCount = (int)(sizeof(s_aAllLocalModes) / sizeof(s_aAllLocalModes[0]));
			int PreviewIndex = 0;
			for(int Index = 0; Index < AllCount; Index++)
				if(s_aAllLocalModes[Index] == m_LocalServerFocus)
				{
					PreviewIndex = Index;
					break;
				}
			const int PreviewMode = s_aAllLocalModes[PreviewIndex];
			const CLocalGameMode &Preview = LocalGameMode(PreviewMode);
			CUIRect PreviewContent = ModePreview;
			PreviewContent.Margin(L(12.0f), &PreviewContent);
			CUIRect PreviewTitle;
			PreviewContent.HSplitTop(L(26.0f), &PreviewTitle, &PreviewContent);
			UI()->DoLabelScaled(&PreviewTitle, Localize("Choose a game mode"), 12.0f, -1);
			CUIRect PreviewImage;
			PreviewContent.HSplitTop(L(112.0f), &PreviewImage, &PreviewContent);
			DrawModeVoteImage(PreviewImage, Preview.m_pGameVoteImage, true);
			PreviewContent.HSplitTop(L(12.0f), 0, &PreviewContent);
			CUIRect PreviewName;
			PreviewContent.HSplitTop(L(25.0f), &PreviewName, &PreviewContent);
			UI()->DoLabelScaled(&PreviewName, Localize(Preview.m_pName), 15.0f, -1);
			CUIRect PreviewDescription;
			PreviewContent.HSplitTop(L(42.0f), &PreviewDescription, &PreviewContent);
			UI()->DoLabelScaled(&PreviewDescription,
								Localize(Preview.m_pDescription),
								9.0f,
								-1,
								(int)PreviewDescription.w);
			char aPreviewMeta[160];
			str_format(aPreviewMeta,
						   sizeof(aPreviewMeta),
						   Localize("Recommended: %s players  ·  %s  ·  %s difficulty"),
						   Preview.m_pRecommendedPlayers,
						   Localize(Preview.m_pDuration),
						   Localize(Preview.m_pRecommendedDifficulty));
			UI()->DoLabelScaled(&PreviewContent, aPreviewMeta, 8.5f, -1, (int)PreviewContent.w);
		}
		return;
	}

	const int Mode = clamp(g_Config.m_ClLocalServerMode, (int)LOCAL_MODE_INVASION, (int)LOCAL_MODE_COUNT - 1);
	const CLocalGameMode &ModeDef = LocalGameMode(Mode);
	g_Config.m_ClLocalServerMap = clamp(g_Config.m_ClLocalServerMap, 0, ModeDef.m_MapCount - 1);
	static CScrollRegion s_ConfigScrollRegion;
	static vec2 s_ConfigScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ConfigScrollParams;
	ConfigureScrollRegion(&ConfigScrollParams);
	ConfigScrollParams.m_ClipBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	ConfigScrollParams.m_ScrollUnit = L(35.0f);
	CUIRect ConfigViewport = Body;
	s_ConfigScrollRegion.Begin(&ConfigViewport, &s_ConfigScrollOffset, &ConfigScrollParams);
	CUIRect ConfigBody = ConfigViewport;
	ConfigBody.y += s_ConfigScrollOffset.y;
	ConfigBody.VSplitRight(L(20.0f), &ConfigBody, 0);
	const float ConfigStartY = ConfigBody.y;
	CUIRect Summary, Change;
	ConfigBody.HSplitTop(L(42.0f), &Summary, &ConfigBody);
	Summary.VSplitRight(L(124.0f), &Summary, &Change);
	char aModeSummary[256];
	str_format(aModeSummary,
			   sizeof(aModeSummary),
			   "%s  ·  %s  ·  %s",
			   Localize(ModeDef.m_pName),
			   Localize(ModeDef.m_pDescription),
			   Localize(ModeDef.m_pMechanics));
	UI()->DoLabelScaled(&Summary,
						aModeSummary,
						FitScaledLabelFontSize(TextRender(), aModeSummary, 11.0f, Summary.w, UI()->Scale()),
						-1);
	if(DoButton_Menu(&s_ChangeMode, Localize("Change mode"), 0, &Change))
		m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
	ConfigBody.HSplitTop(L(8.0f), 0, &ConfigBody);

	CUIRect MainSettings, Identity;
	// Reserve the optional custom-floor row for Invasion as well, so changing
	// the starting point cannot make the panel overlap for a single frame.
	const int MainRows = 3 + (ModeDef.m_HasBots ? 1 : 0) +
						 (Mode == LOCAL_MODE_INVASION			? 2
						  : LocalModeRuleConfig(ModeDef.m_Rule) ? 1
																: 0);
	const bool AdvancedExpanded = g_Config.m_ClLocalServerAdvanced != 0;
	// Challenge section (code input + 4 variant rows + live code) adds six
	// rows to the advanced area; without this the fixed Identity height clips
	// them out of the panel.
	const int AdvancedRows = AdvancedExpanded ? 2 + (ModeDef.m_HasRoguelite ? 1 : 0) + 6 : 0;
	const CRoomConfigureLayout ConfigureLayout =
		RoomConfigureLayout(ConfigBody.w, UI()->Scale(), SteamAvailable, MainRows, AdvancedRows, AdvancedExpanded);
	if(ConfigureLayout.m_SingleColumn)
	{
		ConfigBody.HSplitTop(ConfigureLayout.m_MainSettingsHeight, &MainSettings, &ConfigBody);
		ConfigBody.HSplitTop(L(8.0f), 0, &ConfigBody);
		ConfigBody.HSplitTop(ConfigureLayout.m_IdentityHeight, &Identity, &ConfigBody);
	}
	else
	{
		ConfigBody.VSplitMid(&MainSettings, &Identity);
		MainSettings.h = ConfigureLayout.m_MainSettingsHeight;
		Identity.h = ConfigureLayout.m_IdentityHeight;
		MainSettings.VSplitRight(L(5.0f), &MainSettings, 0);
		Identity.VSplitLeft(L(5.0f), 0, &Identity);
		ConfigBody.y += max(ConfigureLayout.m_MainSettingsHeight, ConfigureLayout.m_IdentityHeight);
	}
	DrawMenuInset(&MainSettings, CUI::CORNER_ALL);
	DrawMenuInset(&Identity, CUI::CORNER_ALL);
	MainSettings.Margin(L(9.0f), &MainSettings);
	Identity.Margin(L(9.0f), &Identity);
	CUIRect Row, Label, Control, Previous, Next, Value;
	char aLabel[160];
	auto SplitRow = [&](CUIRect &Area, CUIRect *pLabel, CUIRect *pControl)
	{
		Area.HSplitTop(L(31.0f), &Row, &Area);
		Area.HSplitTop(L(4.0f), 0, &Area);
		Row.VSplitLeft(Row.w * 0.39f, pLabel, pControl);
		pControl->VSplitLeft(L(6.0f), 0, pControl);
	};
	auto Stepper = [&](CUIRect Rect, int *pPrevious, int *pNext, const char *pValue)
	{
		Rect.VSplitLeft(L(29.0f), &Previous, &Value);
		Value.VSplitRight(L(29.0f), &Value, &Next);
		const int Delta =
			(DoButton_Menu(pPrevious, "-", 0, &Previous) ? -1 : 0) + (DoButton_Menu(pNext, "+", 0, &Next) ? 1 : 0);
		UI()->DoLabelScaled(&Value, pValue, 11.0f, 0);
		return Delta;
	};

	MainSettings.HSplitTop(L(20.0f), &Row, &MainSettings);
	UI()->DoLabelScaled(&Row, Localize("Visibility"), 12.0f, -1);
	MainSettings.HSplitTop(L(32.0f), &Row, &MainSettings);
	const char *apVisibility[] = {"Solo", "Friends", "LAN", "Public"};
	for(int i = 0; i < 4; i++)
	{
		CUIRect Button;
		Row.VSplitLeft(Row.w / (4 - i), &Button, &Row);
		const bool Disabled = RoomVisibilityRequiresSteam(i) && !SteamAvailable;
		if(DoButton_Menu(&s_aVisibilityButtons[i],
						 Localize(apVisibility[i]),
						 g_Config.m_ClRoomVisibility == i,
						 &Button,
						 g_Config.m_ClRoomVisibility == i ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL) &&
		   !Disabled)
		{
			const int Old = g_Config.m_ClRoomVisibility;
			if(Old != ROOM_VISIBILITY_SOLO)
				m_CreateRoomPreviousSlots = max(2, g_Config.m_ClLocalServerMaxClients);
			g_Config.m_ClRoomVisibility = i;
			if(i == ROOM_VISIBILITY_SOLO)
				g_Config.m_ClLocalServerMaxClients = 1;
			else if(Old == ROOM_VISIBILITY_SOLO)
				g_Config.m_ClLocalServerMaxClients = clamp(m_CreateRoomPreviousSlots, 2, 16);
		}
	}
	MainSettings.HSplitTop(L(6.0f), 0, &MainSettings);
	if(!SteamAvailable)
	{
		MainSettings.HSplitTop(L(22.0f), &Row, &MainSettings);
		UI()->DoLabelScaled(&Row, Localize("Friends and Public require Steam."), 9.0f, -1);
	}

	SplitRow(MainSettings, &Label, &Control);
	UI()->DoLabelScaled(&Label, Localize("Map preset"), 11.0f, -1);
	int Delta = Stepper(Control,
						&s_MapPrevious,
						&s_MapNext,
						Localize(ModeDef.m_SelectableMap ? ModeDef.m_ppMapNames[g_Config.m_ClLocalServerMap]
														 : "Automatic by Invasion floor"));
	if(ModeDef.m_SelectableMap && Delta)
		g_Config.m_ClLocalServerMap = (g_Config.m_ClLocalServerMap + Delta + ModeDef.m_MapCount) % ModeDef.m_MapCount;

	SplitRow(MainSettings, &Label, &Control);
	UI()->DoLabelScaled(&Label, Localize("Human slots"), 11.0f, -1);
	str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerMaxClients);
	Delta = Stepper(Control, &s_SlotsPrevious, &s_SlotsNext, aLabel);
	if(g_Config.m_ClRoomVisibility != ROOM_VISIBILITY_SOLO)
	{
		g_Config.m_ClLocalServerMaxClients = clamp(g_Config.m_ClLocalServerMaxClients + Delta, 2, 16);
		m_CreateRoomPreviousSlots = g_Config.m_ClLocalServerMaxClients;
	}
	else
		g_Config.m_ClLocalServerMaxClients = 1;

	SplitRow(MainSettings, &Label, &Control);
	UI()->DoLabelScaled(&Label, Localize("Difficulty"), 11.0f, -1);
	str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerDifficulty);
	Delta = Stepper(Control, &s_DifficultyPrevious, &s_DifficultyNext, aLabel);
	g_Config.m_ClLocalServerDifficulty = clamp(g_Config.m_ClLocalServerDifficulty + Delta, 1, 50);

	if(ModeDef.m_HasBots)
	{
		SplitRow(MainSettings, &Label, &Control);
		UI()->DoLabelScaled(&Label, Localize(LocalGamePopulationLabel(Mode)), 11.0f, -1);
		g_Config.m_ClLocalServerBots = clamp(g_Config.m_ClLocalServerBots, 0, 16);
		if(g_Config.m_ClLocalServerBots == 0)
			str_copy(aLabel, Localize("No bots"), sizeof(aLabel));
		else
			str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerBots);
		Delta = Stepper(Control, &s_BotsPrevious, &s_BotsNext, aLabel);
		g_Config.m_ClLocalServerBots = clamp(g_Config.m_ClLocalServerBots + Delta, 0, 16);
	}

	if(Mode == LOCAL_MODE_INVASION)
	{
		SplitRow(MainSettings, &Label, &Control);
		UI()->DoLabelScaled(&Label, Localize("Starting point"), 11.0f, -1);
		Delta = Stepper(Control,
						&s_InvasionPrevious,
						&s_InvasionNext,
						Localize(LocalInvasionStartName(g_Config.m_ClLocalServerInvasionStart)));
		g_Config.m_ClLocalServerInvasionStart = (g_Config.m_ClLocalServerInvasionStart + Delta + 3) % 3;
		if(g_Config.m_ClLocalServerInvasionStart == LOCAL_INVASION_CUSTOM_FLOOR)
		{
			SplitRow(MainSettings, &Label, &Control);
			UI()->DoLabelScaled(&Label, Localize("Starting floor"), 11.0f, -1);
			const int MaxFloor = max(1, g_Config.m_ClPveHighestInvasion);
			g_Config.m_ClLocalServerInvasionFloor = clamp(g_Config.m_ClLocalServerInvasionFloor, 1, MaxFloor);
			str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerInvasionFloor);
			Delta = Stepper(Control, &s_FloorPrevious, &s_FloorNext, aLabel);
			g_Config.m_ClLocalServerInvasionFloor = clamp(g_Config.m_ClLocalServerInvasionFloor + Delta, 1, MaxFloor);
		}
	}
	else if(int *pRule = LocalModeRuleConfig(ModeDef.m_Rule))
	{
		SplitRow(MainSettings, &Label, &Control);
		UI()->DoLabelScaled(&Label, Localize(LocalGameRuleLabel(ModeDef.m_Rule)), 11.0f, -1);
		str_format(aLabel, sizeof(aLabel), ModeDef.m_Rule == LOCAL_RULE_EXTRACTION ? Localize("%d min") : "%d", *pRule);
		Delta = Stepper(Control, &s_RulePrevious, &s_RuleNext, aLabel);
		int Step = ModeDef.m_Rule == LOCAL_RULE_CTF_SCORE ? 25 : ModeDef.m_Rule >= LOCAL_RULE_DM_SCORE ? 5 : 1;
		int Minimum = ModeDef.m_Rule == LOCAL_RULE_HORDE ? 0 : ModeDef.m_Rule == LOCAL_RULE_EXTRACTION ? 2 : 1;
		int Maximum = ModeDef.m_Rule == LOCAL_RULE_EXTRACTION ? 15 :
			ModeDef.m_Rule == LOCAL_RULE_HORDE || ModeDef.m_Rule == LOCAL_RULE_BALL_SCORE ? 100 : 1000;
		if(ModeDef.m_Rule == LOCAL_RULE_ROAM_CHECKPOINTS)
		{
			Step = 1;
			Minimum = 3;
			Maximum = 63;
		}
		*pRule = clamp(*pRule + Delta * Step, Minimum, Maximum);
	}

	Identity.HSplitTop(L(20.0f), &Row, &Identity);
	UI()->DoLabelScaled(&Row, Localize("Room details"), 12.0f, -1);
	SplitRow(Identity, &Label, &Control);
	UI()->DoLabelScaled(&Label, Localize("Room name"), 11.0f, -1);
	DoEditBox(g_Config.m_ClLocalServerName,
			  &Control,
			  g_Config.m_ClLocalServerName,
			  sizeof(g_Config.m_ClLocalServerName),
			  11.0f,
			  &s_NameOffset);
	SplitRow(Identity, &Label, &Control);
	UI()->DoLabelScaled(&Label, Localize("Password (optional)"), 11.0f, -1);
	DoEditBox(g_Config.m_ClLocalServerPassword,
			  &Control,
			  g_Config.m_ClLocalServerPassword,
			  sizeof(g_Config.m_ClLocalServerPassword),
			  11.0f,
			  &s_PasswordOffset);
	Identity.HSplitTop(L(7.0f), 0, &Identity);
	Identity.HSplitTop(L(31.0f), &Row, &Identity);
	if(DoButton_Menu(&s_Advanced,
					 Localize(AdvancedExpanded ? "Hide advanced settings" : "Advanced settings"),
					 AdvancedExpanded,
					 &Row))
		g_Config.m_ClLocalServerAdvanced ^= 1;
	if(AdvancedExpanded)
	{
		SplitRow(Identity, &Label, &Control);
		if(DoButton_CheckBox(&s_RandomSeed, Localize("Random map seed"), g_Config.m_ClLocalServerRandomSeed, &Label))
			g_Config.m_ClLocalServerRandomSeed ^= 1;
		if(!g_Config.m_ClLocalServerRandomSeed)
		{
			UI()->DoLabelScaled(&Control, Localize("Map seed"), 9.0f, -1);
			if(s_SeedTextValue != g_Config.m_ClLocalServerSeed)
			{
				str_format(s_aSeedText, sizeof(s_aSeedText), "%d", g_Config.m_ClLocalServerSeed);
				s_SeedTextValue = g_Config.m_ClLocalServerSeed;
			}
			if(DoEditBox(s_aSeedText, &Control, s_aSeedText, sizeof(s_aSeedText), 10.0f, &s_SeedOffset))
				g_Config.m_ClLocalServerSeed = clamp(str_toint(s_aSeedText), 0, 0x7FFFFFFF);
		}
		SplitRow(Identity, &Label, &Control);
		UI()->DoLabelScaled(&Label, Localize("Challenge code"), 9.0f, -1);
		const bool ChallengeCodeChanged = s_ChallengeTextMode != g_Config.m_ClLocalServerMode ||
			s_ChallengeTextDifficulty != g_Config.m_ClLocalServerDifficulty ||
			s_ChallengeTextSeed != g_Config.m_ClLocalServerSeed ||
			s_ChallengeTextVariants != g_Config.m_ClChallengeVariants;
		if(ChallengeCodeChanged && !CLineInput::GetActiveInput())
		{
			FormatChallengeCode(s_aChallengeText,
				sizeof(s_aChallengeText),
				g_Config.m_ClLocalServerMode,
				g_Config.m_ClLocalServerDifficulty,
				g_Config.m_ClLocalServerSeed,
				ChallengeModeAllowed(g_Config.m_ClLocalServerMode) ? g_Config.m_ClChallengeVariants : 0);
			s_ChallengeTextMode = g_Config.m_ClLocalServerMode;
			s_ChallengeTextDifficulty = g_Config.m_ClLocalServerDifficulty;
			s_ChallengeTextSeed = g_Config.m_ClLocalServerSeed;
			s_ChallengeTextVariants = g_Config.m_ClChallengeVariants;
		}
		if(DoEditBox(s_aChallengeText, &Control, s_aChallengeText, sizeof(s_aChallengeText), 10.0f, &s_ChallengeOffset))
		{
			int Mode, Difficulty, Seed, Variants;
			if(ParseChallengeCode(s_aChallengeText, &Mode, &Difficulty, &Seed, &Variants))
			{
				m_LocalServerFocus = Mode;
				ApplyLocalGameModeDefaults(Mode);
				g_Config.m_ClLocalServerDifficulty = Difficulty;
				g_Config.m_ClLocalServerSeed = Seed;
				g_Config.m_ClLocalServerRandomSeed = 0;
				g_Config.m_ClChallengeVariants = ChallengeModeAllowed(Mode) ? Variants : 0;
				s_SeedTextValue = -1;
				s_ChallengeTextMode = -1;
				s_ChallengeTextDifficulty = -1;
				s_ChallengeTextSeed = -1;
				s_ChallengeTextVariants = -1;
			}
		}
		// Challenge variant checkboxes (settings-page row style). Ball and Roam
		// intentionally remain outside the challenge system.
		if(ChallengeModeAllowed(Mode))
		{
			auto ChallengeVariantRow = [&](int Variant, const char *pName, const char *pDesc, int *pButton)
			{
				SplitRow(Identity, &Label, &Control);
				if(DoButton_CheckBox(
					   pButton, Localize(pName), ChallengeVariantEnabled(g_Config.m_ClChallengeVariants, Variant), &Label))
					g_Config.m_ClChallengeVariants ^= 1 << Variant;
				UI()->DoLabelScaled(&Control, Localize(pDesc), 9.0f, -1, (int)Control.w);
			};
			ChallengeVariantRow(CHALLENGE_LOW_GRAVITY, "Low gravity", "Weaker gravity", &s_ChVLowGrav);
			ChallengeVariantRow(CHALLENGE_NO_BUILD, "No building", "Construction disabled", &s_ChVNoBuild);
			ChallengeVariantRow(
				CHALLENGE_ONLY_MELEE, "Melee only", "Firearms disabled; other items allowed", &s_ChVMelee);
			ChallengeVariantRow(CHALLENGE_DARK, "Dark vision", "Darker screen", &s_ChVDark);
		}
		// Live challenge code for sharing (clamped to the column width).
		SplitRow(Identity, &Label, &Control);
		UI()->DoLabelScaled(&Label, Localize("Code"), 9.0f, -1);
		char aChallengeCode[64];
		FormatChallengeCode(aChallengeCode,
			sizeof(aChallengeCode),
			g_Config.m_ClLocalServerMode,
			g_Config.m_ClLocalServerDifficulty,
			g_Config.m_ClLocalServerSeed,
			ChallengeModeAllowed(g_Config.m_ClLocalServerMode) ? g_Config.m_ClChallengeVariants : 0);
		UI()->DoLabelScaled(&Control, aChallengeCode, 9.0f, -1, (int)Control.w);
		if(ModeDef.m_HasRoguelite)
		{
			SplitRow(Identity, &Label, &Control);
			if(DoButton_CheckBox(
				   &s_Roguelite, Localize("Roguelite Director"), g_Config.m_ClLocalServerRoguelite, &Label))
				g_Config.m_ClLocalServerRoguelite ^= 1;
			if(DoButton_CheckBox(&s_Contracts,
								 Localize("Team contracts"),
								 g_Config.m_ClLocalServerContracts && g_Config.m_ClLocalServerRoguelite,
								 &Control) &&
			   g_Config.m_ClLocalServerRoguelite)
				g_Config.m_ClLocalServerContracts ^= 1;
		}
		SplitRow(Identity, &Label, &Control);
		UI()->DoLabelScaled(&Label, Localize("Port"), 10.0f, -1);
		str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerPort);
		Delta = Stepper(Control, &s_PortPrevious, &s_PortNext, aLabel);
		g_Config.m_ClLocalServerPort = clamp(g_Config.m_ClLocalServerPort + Delta, 1024, 65535);
		Identity.HSplitTop(L(18.0f), &Row, &Identity);
		UI()->DoLabelScaled(
			&Row,
			Localize(g_Config.m_ClRoomVisibility == ROOM_VISIBILITY_LAN ? "LAN binding" : "Managed automatically"),
			9.0f,
			-1);
	}
	CUIRect ConfigExtent = ConfigViewport;
	ConfigExtent.y = ConfigStartY;
	ConfigExtent.h = ConfigBody.y - ConfigStartY;
	s_ConfigScrollRegion.AddRect(ConfigExtent);
	s_ConfigScrollRegion.End();

	CLocalServerLaunchSettings Preview;
	BuildLocalServerLaunchSettings(&Preview);
	DrawMenuInset(&Footer, CUI::CORNER_ALL);
	Footer.Margin(L(8.0f), &Footer);
	CUIRect Status, Action;
	Footer.VSplitRight(L(164.0f), &Status, &Action);
	const char *pVisibility = apVisibility[g_Config.m_ClRoomVisibility];
	char aFinalSummary[512];
	{
		char aPopulation[96];
		if(Preview.m_pMode->m_HasBots && Preview.m_Bots > 0)
			str_format(aPopulation,
					   sizeof(aPopulation),
					   Localize("%s: %d"),
					   Localize(LocalGamePopulationLabel(Mode)),
					   Preview.m_Bots);
		else if(Preview.m_pMode->m_HasBots)
			str_copy(aPopulation, Localize("No bots"), sizeof(aPopulation));
		else
			aPopulation[0] = '\0';

		if(aPopulation[0])
			str_format(aFinalSummary,
					   sizeof(aFinalSummary),
					   Localize("%s  ·  %s  ·  %s  ·  %d human slots  ·  %s  ·  Community room · Unverified"),
					   Localize(Preview.m_pMode->m_pName),
					   Localize(pVisibility),
					   Localize(Preview.m_pMapName),
					   Preview.m_MaxClients,
					   aPopulation);
		else
			str_format(aFinalSummary,
					   sizeof(aFinalSummary),
					   Localize("%s  ·  %s  ·  %s  ·  %d human slots  ·  Community room · Unverified"),
					   Localize(Preview.m_pMode->m_pName),
					   Localize(pVisibility),
					   Localize(Preview.m_pMapName),
					   Preview.m_MaxClients);
	}
	UI()->DoLabelScaled(&Status,
						aFinalSummary,
						FitScaledLabelFontSize(TextRender(), aFinalSummary, 10.0f, Status.w, UI()->Scale()),
						-1);
	CClientAsyncStatus SteamStatus;
	Client()->SteamHostedGameStatus(&SteamStatus);
	CUIRect StatusDetail = Status;
	StatusDetail.y += L(23.0f);
	if(SteamStatus.m_State == CLIENT_ASYNC_FAILED)
		UI()->DoLabelScaled(&StatusDetail, Localize(SteamStatus.m_aErrorKey), 8.5f, -1, (int)StatusDetail.w);
	else if(m_LocalServerState == LOCAL_SERVER_FAILED)
		UI()->DoLabelScaled(&StatusDetail,
							Localize("Room creation failed. Your settings were kept; retry or open the log."),
							8.5f,
							-1,
							(int)StatusDetail.w);
	const bool RelayUnavailable = !SteamAvailable && RoomVisibilityRequiresSteam(g_Config.m_ClRoomVisibility);
	CUIRect PrimaryAction = Action, SecondaryAction;
	const bool SteamHostActive =
		SteamStatus.m_State == CLIENT_ASYNC_WORKING || SteamStatus.m_State == CLIENT_ASYNC_SUCCEEDED;
	const bool ShowSecondary = m_LocalServerState == LOCAL_SERVER_RUNNING ||
							   m_LocalServerState == LOCAL_SERVER_STARTING || SteamHostActive ||
							   (m_LocalServerState == LOCAL_SERVER_FAILED && m_aLocalServerLogPath[0]);
	if(ShowSecondary)
	{
		Action.HSplitTop(L(25.0f), &SecondaryAction, &PrimaryAction);
		PrimaryAction.HSplitTop(L(5.0f), 0, &PrimaryAction);
	}
	const int PrimaryState = SteamStatus.m_State == CLIENT_ASYNC_WORKING	 ? ROOM_PRIMARY_CREATING_STEAM
							 : m_LocalServerState == LOCAL_SERVER_STARTING	 ? ROOM_PRIMARY_STARTING_LOCAL
							 : m_LocalServerState == LOCAL_SERVER_STOPPING	 ? ROOM_PRIMARY_STOPPING_LOCAL
							 : m_LocalServerState == LOCAL_SERVER_RUNNING	 ? ROOM_PRIMARY_RESTART_LOCAL
							 : SteamStatus.m_State == CLIENT_ASYNC_SUCCEEDED ? ROOM_PRIMARY_RESTART_STEAM
																			 : ROOM_PRIMARY_CREATE;
	if(DoButton_Menu(
		   &s_Create, Localize(RoomPrimaryActionLabel(PrimaryState)), 0, &PrimaryAction, BUTTONSTYLE_ACCENT) &&
	   !RelayUnavailable && RoomPrimaryActionEnabled(PrimaryState))
		CreateConfiguredRoom();
	if(m_LocalServerState == LOCAL_SERVER_RUNNING || m_LocalServerState == LOCAL_SERVER_STARTING || SteamHostActive)
	{
		const char *pStopLabel = m_LocalServerState == LOCAL_SERVER_STARTING && !SteamHostActive ? "Cancel" : "Stop";
		if(DoButton_Menu(&s_Stop, Localize(pStopLabel), 0, &SecondaryAction, BUTTONSTYLE_DANGER))
		{
			if(SteamHostActive)
				Client()->StopSteamHostedGame();
			else
				StopLocalServer(false);
		}
	}
	if(m_LocalServerState == LOCAL_SERVER_FAILED && m_aLocalServerLogPath[0])
	{
		if(DoButton_Menu(&s_Log, Localize("Log"), 0, &SecondaryAction))
		{
			char aBody[512];
			str_format(aBody, sizeof(aBody), "%s\n\n%s", m_aLocalServerLogPath, m_aLocalServerErrorDetail);
			PopupMessage(Localize("Local server log"), aBody, Localize("OK"));
		}
	}
}

void CMenus::RenderLocalServer(CUIRect MainView)
{
	static int s_aModeButtons[sizeof(s_aLocalGameModes) / sizeof(s_aLocalGameModes[0])] = {0};
	static int s_aSectionButtons[3] = {0};
	static int s_LocalSection = -1;
	static int s_MapPrevious = 0;
	static int s_MapNext = 0;
	static int s_PortPrevious = 0;
	static int s_PortNext = 0;
	static int s_LanButton = 0;
	static int s_RogueliteButton = 0;
	static int s_ContractsButton = 0;
	static int s_RandomSeedButton = 0;
	static int s_InvasionStartPrevious = 0;
	static int s_InvasionStartNext = 0;
	static int s_InvasionFloorPrevious = 0;
	static int s_InvasionFloorNext = 0;
	static int s_RulePrevious = 0;
	static int s_RuleNext = 0;
	static int s_StartButton = 0;
	static int s_LogButton = 0;
	static int s_JoinButton = 0;
	static int s_RestartButton = 0;
	static int s_StopButton = 0;
	static float s_NameOffset = 0.0f;
	static float s_PasswordOffset = 0.0f;
	static float s_SeedOffset = 0.0f;
	static char s_aSeedText[8] = "0";
	static int s_SeedTextValue = -1;
	const float LayoutDivisor = max(1.0f, UI()->Scale());
	auto L = [LayoutDivisor](float Value)
	{
		return Value / LayoutDivisor;
	};

	enum
	{
		FOCUS_MODE = 0,
		FOCUS_SECTION_SERVER,
		FOCUS_SLOTS,
		FOCUS_PORT,
		FOCUS_LAN,
		FOCUS_SECTION_MAP,
		FOCUS_MAP,
		FOCUS_INVASION_START,
		FOCUS_INVASION_FLOOR,
		FOCUS_DIFFICULTY,
		FOCUS_BOTS,
		FOCUS_RANDOM_SEED,
		FOCUS_SEED,
		FOCUS_CHALLENGE,
		FOCUS_SECTION_RULES,
		FOCUS_ROGUELITE,
		FOCUS_CONTRACTS,
		FOCUS_MODE_RULE,
		FOCUS_PRIMARY_ACTION,
		FOCUS_RESTART,
		FOCUS_STOP,
	};
	if(s_LocalSection < 0)
		s_LocalSection = g_Config.m_ClLocalServerAdvanced ? 2 : 0;
	s_LocalSection = clamp(s_LocalSection, 0, 2);
	g_Config.m_ClLocalServerMode = clamp(g_Config.m_ClLocalServerMode, 0, LocalGameModeCount() - 1);
	const int MaxFocus = m_LocalServerState == LOCAL_SERVER_RUNNING ? FOCUS_STOP : FOCUS_PRIMARY_ACTION;
	auto FocusAvailable = [&](int Focus)
	{
		const int Mode = clamp(g_Config.m_ClLocalServerMode, 0, LocalGameModeCount() - 1);
		if(Focus == FOCUS_SECTION_SERVER || Focus == FOCUS_SECTION_MAP || Focus == FOCUS_SECTION_RULES)
			return true;
		if(Focus >= FOCUS_SLOTS && Focus <= FOCUS_LAN)
			return s_LocalSection == 0;
		if(Focus >= FOCUS_MAP && Focus <= FOCUS_CHALLENGE)
		{
			if(s_LocalSection != 1)
				return false;
			if(Focus == FOCUS_MAP)
				return LocalGameMode(Mode).m_SelectableMap;
			if(Focus == FOCUS_INVASION_START)
				return Mode == LOCAL_MODE_INVASION;
			if(Focus == FOCUS_INVASION_FLOOR)
				return Mode == LOCAL_MODE_INVASION &&
					   g_Config.m_ClLocalServerInvasionStart == LOCAL_INVASION_CUSTOM_FLOOR;
			if(Focus == FOCUS_DIFFICULTY)
				return Mode != LOCAL_MODE_INVASION && Mode != LOCAL_MODE_TUTORIAL;
			if(Focus == FOCUS_BOTS)
				return LocalGameMode(Mode).m_HasBots;
			if(Focus == FOCUS_SEED)
				return !g_Config.m_ClLocalServerRandomSeed;
		}
		if(Focus >= FOCUS_ROGUELITE && Focus <= FOCUS_MODE_RULE)
		{
			if(s_LocalSection != 2)
				return false;
			if(Focus == FOCUS_ROGUELITE || Focus == FOCUS_CONTRACTS)
				return LocalGameMode(Mode).m_HasRoguelite;
			if(Focus == FOCUS_MODE_RULE)
				return Mode != LOCAL_MODE_INVASION && Mode != LOCAL_MODE_TUTORIAL;
		}
		if((Focus == FOCUS_RESTART || Focus == FOCUS_STOP) && m_LocalServerState != LOCAL_SERVER_RUNNING)
			return false;
		return true;
	};
	auto SectionFocus = [](int Section)
	{
		return Section == 0 ? FOCUS_SECTION_SERVER : (Section == 1 ? FOCUS_SECTION_MAP : FOCUS_SECTION_RULES);
	};
	auto AdjustModeRule = [&](int Direction)
	{
		const int Mode = clamp(g_Config.m_ClLocalServerMode, 0, LocalGameModeCount() - 1);
		if(Mode == LOCAL_MODE_HORDE)
		{
			if(Direction > 0)
				g_Config.m_ClLocalServerHordeWaves =
					g_Config.m_ClLocalServerHordeWaves <= 0 ? 4 : min(100, g_Config.m_ClLocalServerHordeWaves + 4);
			else
				g_Config.m_ClLocalServerHordeWaves =
					g_Config.m_ClLocalServerHordeWaves <= 4 ? 0 : g_Config.m_ClLocalServerHordeWaves - 4;
		}
		else if(Mode == LOCAL_MODE_EXTRACTION)
			g_Config.m_ClLocalServerExtractionTime = clamp(g_Config.m_ClLocalServerExtractionTime + Direction, 2, 15);
		else if(Mode == LOCAL_MODE_DM)
			g_Config.m_ClLocalServerDmScore = clamp(g_Config.m_ClLocalServerDmScore + Direction * 5, 1, 1000);
		else if(Mode == LOCAL_MODE_TDM)
			g_Config.m_ClLocalServerTdmScore = clamp(g_Config.m_ClLocalServerTdmScore + Direction * 5, 1, 1000);
		else if(Mode == LOCAL_MODE_CTF)
			g_Config.m_ClLocalServerCtfScore = clamp(g_Config.m_ClLocalServerCtfScore + Direction * 25, 1, 1000);
		else if(Mode == LOCAL_MODE_ROAM)
			g_Config.m_ClLocalServerRoamCheckpoints = clamp(g_Config.m_ClLocalServerRoamCheckpoints + Direction, 3, 63);
	};

	m_LocalServerFocus = clamp(m_LocalServerFocus, 0, MaxFocus);
	while(!FocusAvailable(m_LocalServerFocus))
		m_LocalServerFocus = (m_LocalServerFocus + 1) % (MaxFocus + 1);
	if(!CLineInput::GetActiveInput())
	{
		for(int EventIndex = 0; EventIndex < m_NumInputEvents; EventIndex++)
		{
			const IInput::CEvent &Event = m_aInputEvents[EventIndex];
			if(!(Event.m_Flags & IInput::FLAG_PRESS))
				continue;
			const bool Up = Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP;
			const bool Down =
				Event.m_Key == KEY_DOWN || Event.m_Key == KEY_TAB || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN;
			const bool Left = Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT;
			const bool Right = Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT;
			const bool Confirm = Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER ||
								 Event.m_Key == KEY_GAMEPAD_BUTTON_A || Event.m_Key == KEY_GAMEPAD_BUTTON_START;
			if(Event.m_Key == KEY_GAMEPAD_BUTTON_B)
			{
				if(s_LocalSection != 0)
				{
					s_LocalSection = 0;
					g_Config.m_ClLocalServerAdvanced = 0;
					m_LocalServerFocus = FOCUS_SECTION_SERVER;
				}
				else
					g_Config.m_UiPage = PAGE_INTERNET;
				continue;
			}
			if(Up || Down)
			{
				const int Direction = Down ? 1 : -1;
				do
				{
					m_LocalServerFocus = (m_LocalServerFocus + Direction + MaxFocus + 1) % (MaxFocus + 1);
				} while(!FocusAvailable(m_LocalServerFocus));
				continue;
			}
			if(Left || Right)
			{
				const int Direction = Right ? 1 : -1;
				if(m_LocalServerFocus == FOCUS_MODE)
				{
					const int ModeCount = LocalGameModeCount();
					g_Config.m_ClLocalServerMode = (g_Config.m_ClLocalServerMode + Direction + ModeCount) % ModeCount;
					g_Config.m_ClLocalServerMap = clamp(
						g_Config.m_ClLocalServerMap, 0, LocalGameMode(g_Config.m_ClLocalServerMode).m_MapCount - 1);
				}
				else if(m_LocalServerFocus == FOCUS_MAP)
				{
					const int Count = LocalGameMode(g_Config.m_ClLocalServerMode).m_MapCount;
					g_Config.m_ClLocalServerMap = (g_Config.m_ClLocalServerMap + Direction + Count) % Count;
				}
				else if(m_LocalServerFocus == FOCUS_INVASION_START)
					g_Config.m_ClLocalServerInvasionStart = (g_Config.m_ClLocalServerInvasionStart + Direction + 3) % 3;
				else if(m_LocalServerFocus == FOCUS_INVASION_FLOOR)
					g_Config.m_ClLocalServerInvasionFloor = clamp(
						g_Config.m_ClLocalServerInvasionFloor + Direction, 1, max(1, g_Config.m_ClPveHighestInvasion));
				else if(m_LocalServerFocus == FOCUS_DIFFICULTY)
					g_Config.m_ClLocalServerDifficulty = clamp(g_Config.m_ClLocalServerDifficulty + Direction, 1, 50);
				else if(m_LocalServerFocus == FOCUS_BOTS)
					g_Config.m_ClLocalServerBots = clamp(g_Config.m_ClLocalServerBots + Direction, 0, 16);
				else if(m_LocalServerFocus == FOCUS_SLOTS)
					g_Config.m_ClLocalServerMaxClients = clamp(g_Config.m_ClLocalServerMaxClients + Direction, 1, 16);
				else if(m_LocalServerFocus == FOCUS_PORT)
					g_Config.m_ClLocalServerPort = clamp(g_Config.m_ClLocalServerPort + Direction, 1024, 65535);
				else if(m_LocalServerFocus == FOCUS_LAN)
					g_Config.m_ClLocalServerLan ^= 1;
				else if(m_LocalServerFocus == FOCUS_SECTION_SERVER || m_LocalServerFocus == FOCUS_SECTION_MAP ||
						m_LocalServerFocus == FOCUS_SECTION_RULES)
				{
					s_LocalSection = (s_LocalSection + Direction + 3) % 3;
					g_Config.m_ClLocalServerAdvanced = s_LocalSection == 2;
					m_LocalServerFocus = SectionFocus(s_LocalSection);
				}
				else if(m_LocalServerFocus == FOCUS_ROGUELITE)
					g_Config.m_ClLocalServerRoguelite = Right;
				else if(m_LocalServerFocus == FOCUS_CONTRACTS && g_Config.m_ClLocalServerRoguelite)
					g_Config.m_ClLocalServerContracts = Right;
				else if(m_LocalServerFocus == FOCUS_MODE_RULE)
					AdjustModeRule(Direction);
				else if(m_LocalServerFocus == FOCUS_RANDOM_SEED)
					g_Config.m_ClLocalServerRandomSeed = Right;
				else if(m_LocalServerFocus == FOCUS_SEED)
					g_Config.m_ClLocalServerSeed = clamp(g_Config.m_ClLocalServerSeed + Direction, 0, 0x7FFFFFFF);
				continue;
			}
			if(!Confirm)
				continue;
			if(m_LocalServerFocus == FOCUS_LAN)
				g_Config.m_ClLocalServerLan ^= 1;
			else if(m_LocalServerFocus == FOCUS_SECTION_SERVER)
			{
				s_LocalSection = 0;
				g_Config.m_ClLocalServerAdvanced = 0;
			}
			else if(m_LocalServerFocus == FOCUS_SECTION_MAP)
			{
				s_LocalSection = 1;
				g_Config.m_ClLocalServerAdvanced = 0;
			}
			else if(m_LocalServerFocus == FOCUS_SECTION_RULES)
			{
				s_LocalSection = 2;
				g_Config.m_ClLocalServerAdvanced = 1;
			}
			else if(m_LocalServerFocus == FOCUS_ROGUELITE)
				g_Config.m_ClLocalServerRoguelite ^= 1;
			else if(m_LocalServerFocus == FOCUS_CONTRACTS && g_Config.m_ClLocalServerRoguelite)
				g_Config.m_ClLocalServerContracts ^= 1;
			else if(m_LocalServerFocus == FOCUS_RANDOM_SEED)
				g_Config.m_ClLocalServerRandomSeed ^= 1;
			else if(m_LocalServerFocus == FOCUS_PRIMARY_ACTION)
			{
				if(m_LocalServerState == LOCAL_SERVER_STOPPED || m_LocalServerState == LOCAL_SERVER_FAILED)
					StartLocalServer(true);
				else if(m_LocalServerState == LOCAL_SERVER_RUNNING)
					JoinLocalServer();
				else
					StopLocalServer(false);
			}
			else if(m_LocalServerFocus == FOCUS_RESTART && m_LocalServerState == LOCAL_SERVER_RUNNING)
				StopLocalServer(true);
			else if(m_LocalServerFocus == FOCUS_STOP && m_LocalServerState == LOCAL_SERVER_RUNNING)
				StopLocalServer(false);
		}
	}
	CLocalServerLaunchSettings PreviewSettings;
	BuildLocalServerLaunchSettings(&PreviewSettings);
	char aPreviewSummary[512];
	FormatLocalServerSummary(PreviewSettings, PreviewSettings.m_Port, aPreviewSummary, sizeof(aPreviewSummary));
	if((m_LocalServerProcess || m_LocalServerState == LOCAL_SERVER_FAILED) && !m_LocalServerSummaryLocalized)
	{
		FormatLocalServerSummary(PreviewSettings,
								 m_LocalServerActualPort > 0 ? m_LocalServerActualPort : PreviewSettings.m_Port,
								 m_aLocalServerSummary,
								 sizeof(m_aLocalServerSummary));
		m_LocalServerSummaryLocalized = true;
	}

	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(L(10.0f), &MainView);

	CUIRect Header, Body, StatusBar;
	const float LargeScale = max(0.0f, UI()->Scale() - 1.0f);
	const float HeaderHeight = 48.0f + LargeScale * 36.0f;
	const float TitleLineHeight = 27.0f + LargeScale * 22.0f;
	MainView.HSplitTop(L(HeaderHeight), &Header, &Body);
	Body.HSplitBottom(L(72.0f), &Body, &StatusBar);
	UI()->DoLabelScaled(&Header, Localize("Local game"), 22.0f, -1);
	Header.HSplitTop(L(TitleLineHeight), 0, &Header);
	UI()->DoLabelScaled(
		&Header, Localize("Choose a mode, expand a category, then start and join in one click."), 11.0f, -1);
	DrawAccentUnderline(&Header);

	Body.HMargin(L(8.0f), &Body);
	CUIRect Modes, Settings;
	Body.VSplitLeft(L(205.0f), &Modes, &Settings);
	Settings.VSplitLeft(L(10.0f), 0, &Settings);
	DrawMenuInset(&Modes, CUI::CORNER_ALL);
	DrawMenuInset(&Settings, CUI::CORNER_ALL);
	Modes.Margin(L(8.0f), &Modes);
	Settings.Margin(L(10.0f), &Settings);

	CUIRect Row;
	for(int i = 1; i < LocalGameModeCount(); i++)
	{
		Modes.HSplitTop(L(30.0f), &Row, &Modes);
		if(DoButton_Menu(&s_aModeButtons[i],
						 Localize(s_aLocalGameModes[i].m_pName),
						 g_Config.m_ClLocalServerMode == i,
						 &Row,
						 g_Config.m_ClLocalServerMode == i ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
		{
			g_Config.m_ClLocalServerMode = i;
			g_Config.m_ClLocalServerMap = clamp(g_Config.m_ClLocalServerMap, 0, LocalGameMode(i).m_MapCount - 1);
		}
		Modes.HSplitTop(L(4.0f), 0, &Modes);
	}
	Modes.HSplitTop(L(8.0f), 0, &Modes);
	UI()->DoLabelScaled(
		&Modes, Localize(s_aLocalGameModes[g_Config.m_ClLocalServerMode].m_pDescription), 10.0f, -1, (int)Modes.w);

	const CLocalGameMode &ModeDef = LocalGameMode(g_Config.m_ClLocalServerMode);
	const int MapCount = ModeDef.m_MapCount;
	g_Config.m_ClLocalServerMap = clamp(g_Config.m_ClLocalServerMap, 0, MapCount - 1);
	const char *pMapName = ModeDef.m_ppMapNames[g_Config.m_ClLocalServerMap];

	auto SplitSettingRow = [&Settings, &L](CUIRect *pLabel, CUIRect *pControl)
	{
		CUIRect Full;
		Settings.HSplitTop(L(31.0f), &Full, &Settings);
		Settings.HSplitTop(L(4.0f), 0, &Settings);
		const float LabelWidth = clamp(Full.w * 0.34f, 162.0f, 210.0f);
		Full.VSplitLeft(L(LabelWidth), pLabel, pControl);
		pControl->VSplitLeft(L(8.0f), 0, pControl);
	};
	auto DrawFocusMarker = [this](const CUIRect &Rect, int Focus)
	{
		if(m_LocalServerFocus != Focus)
			return;
		CUIRect Marker = Rect;
		Marker.w = 3.0f * UI()->Scale();
		RenderTools()->DrawUIRect(&Marker, ms_ColorAccent, CUI::CORNER_ALL, 1.0f);
	};

	CUIRect Label, Control, Previous, Next, Value;
	char aLabel[128];
	auto DrawSectionHeader = [&](int Section, const char *pTitle, int Focus)
	{
		CUIRect SectionHeader;
		Settings.HSplitTop(L(31.0f), &SectionHeader, &Settings);
		Settings.HSplitTop(L(4.0f), 0, &Settings);
		const bool Expanded = s_LocalSection == Section;
		char aSectionTitle[96];
		str_format(aSectionTitle, sizeof(aSectionTitle), "%s  %s", Expanded ? "−" : "+", Localize(pTitle));
		if(DoButton_Menu(&s_aSectionButtons[Section],
						 aSectionTitle,
						 Expanded,
						 &SectionHeader,
						 Expanded ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
		{
			s_LocalSection = Section;
			g_Config.m_ClLocalServerAdvanced = Section == 2;
			m_LocalServerFocus = Focus;
		}
		DrawFocusMarker(SectionHeader, Focus);
		return Expanded;
	};

	if(DrawSectionHeader(0, "Server & network", FOCUS_SECTION_SERVER))
	{
		SplitSettingRow(&Label, &Control);
		UI()->DoLabelScaled(&Label, Localize("Server name"), 12.0f, -1);
		DoEditBox(g_Config.m_ClLocalServerName,
				  &Control,
				  g_Config.m_ClLocalServerName,
				  sizeof(g_Config.m_ClLocalServerName),
				  12.0f,
				  &s_NameOffset);

		SplitSettingRow(&Label, &Control);
		UI()->DoLabelScaled(&Label, Localize("Password (optional)"), 12.0f, -1);
		DoEditBox(g_Config.m_ClLocalServerPassword,
				  &Control,
				  g_Config.m_ClLocalServerPassword,
				  sizeof(g_Config.m_ClLocalServerPassword),
				  12.0f,
				  &s_PasswordOffset,
				  true);

		SplitSettingRow(&Label, &Control);
		DrawFocusMarker(Label, FOCUS_SLOTS);
		str_format(aLabel, sizeof(aLabel), "%s: %d", Localize("Human slots"), g_Config.m_ClLocalServerMaxClients);
		UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
		Control.HMargin(L(5.0f), &Control);
		g_Config.m_ClLocalServerMaxClients = 1 + (int)(DoScrollbarH(&g_Config.m_ClLocalServerMaxClients,
																	&Control,
																	(g_Config.m_ClLocalServerMaxClients - 1) / 15.0f) *
														   15.0f +
													   0.5f);

		SplitSettingRow(&Label, &Control);
		DrawFocusMarker(Label, FOCUS_PORT);
		UI()->DoLabelScaled(&Label, Localize("Port"), 12.0f, -1);
		Control.VSplitLeft(L(30.0f), &Previous, &Value);
		Value.VSplitRight(L(30.0f), &Value, &Next);
		if(DoButton_Menu(&s_PortPrevious, "-", 0, &Previous))
			g_Config.m_ClLocalServerPort = max(1024, g_Config.m_ClLocalServerPort - 1);
		if(DoButton_Menu(&s_PortNext, "+", 0, &Next))
			g_Config.m_ClLocalServerPort = min(65535, g_Config.m_ClLocalServerPort + 1);
		str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerPort);
		UI()->DoLabelScaled(&Value, aLabel, 12.0f, 0);

		SplitSettingRow(&Label, &Control);
		DrawFocusMarker(Label, FOCUS_LAN);
		if(DoButton_CheckBox(&s_LanButton, Localize("Allow LAN players"), g_Config.m_ClLocalServerLan, &Label))
			g_Config.m_ClLocalServerLan ^= 1;
		UI()->DoLabelScaled(&Control, Localize("Never listed publicly"), 10.0f, -1);
	}

	if(DrawSectionHeader(1, "Map & difficulty", FOCUS_SECTION_MAP))
	{
		SplitSettingRow(&Label, &Control);
		if(ModeDef.m_SelectableMap)
		{
			DrawFocusMarker(Label, FOCUS_MAP);
			UI()->DoLabelScaled(&Label, Localize("Map preset"), 12.0f, -1);
			Control.VSplitLeft(L(30.0f), &Previous, &Value);
			Value.VSplitRight(L(30.0f), &Value, &Next);
			if(DoButton_Menu(&s_MapPrevious, "<", 0, &Previous))
				g_Config.m_ClLocalServerMap = (g_Config.m_ClLocalServerMap + MapCount - 1) % MapCount;
			if(DoButton_Menu(&s_MapNext, ">", 0, &Next))
				g_Config.m_ClLocalServerMap = (g_Config.m_ClLocalServerMap + 1) % MapCount;
			UI()->DoLabelScaled(&Value, Localize(pMapName), 12.0f, 0);
		}
		else if(g_Config.m_ClLocalServerMode != LOCAL_MODE_TUTORIAL)
		{
			UI()->DoLabelScaled(&Label, Localize("Map selection"), 12.0f, -1);
			UI()->DoLabelScaled(&Control, Localize("Automatic by Invasion floor"), 11.0f, -1);
		}

		if(g_Config.m_ClLocalServerMode == LOCAL_MODE_INVASION)
		{
			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_INVASION_START);
			UI()->DoLabelScaled(&Label, Localize("Starting point"), 12.0f, -1);
			Control.VSplitLeft(L(30.0f), &Previous, &Value);
			Value.VSplitRight(L(30.0f), &Value, &Next);
			if(DoButton_Menu(&s_InvasionStartPrevious, "<", 0, &Previous))
				g_Config.m_ClLocalServerInvasionStart = (g_Config.m_ClLocalServerInvasionStart + 2) % 3;
			if(DoButton_Menu(&s_InvasionStartNext, ">", 0, &Next))
				g_Config.m_ClLocalServerInvasionStart = (g_Config.m_ClLocalServerInvasionStart + 1) % 3;
			UI()->DoLabelScaled(
				&Value, Localize(LocalInvasionStartName(g_Config.m_ClLocalServerInvasionStart)), 11.0f, 0);

			if(g_Config.m_ClLocalServerInvasionStart == LOCAL_INVASION_CUSTOM_FLOOR)
			{
				const int MaxFloor = max(1, g_Config.m_ClPveHighestInvasion);
				g_Config.m_ClLocalServerInvasionFloor = clamp(g_Config.m_ClLocalServerInvasionFloor, 1, MaxFloor);
				SplitSettingRow(&Label, &Control);
				DrawFocusMarker(Label, FOCUS_INVASION_FLOOR);
				str_format(aLabel,
						   sizeof(aLabel),
						   "%s: %d",
						   Localize("Starting floor"),
						   g_Config.m_ClLocalServerInvasionFloor);
				UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
				Control.VSplitLeft(L(30.0f), &Previous, &Value);
				Value.VSplitRight(L(30.0f), &Value, &Next);
				if(DoButton_Menu(&s_InvasionFloorPrevious, "-", 0, &Previous))
					g_Config.m_ClLocalServerInvasionFloor = max(1, g_Config.m_ClLocalServerInvasionFloor - 1);
				if(DoButton_Menu(&s_InvasionFloorNext, "+", 0, &Next))
					g_Config.m_ClLocalServerInvasionFloor = min(MaxFloor, g_Config.m_ClLocalServerInvasionFloor + 1);
				if(MaxFloor > 1)
					g_Config.m_ClLocalServerInvasionFloor =
						1 + (int)(DoScrollbarH(&g_Config.m_ClLocalServerInvasionFloor,
											   &Value,
											   (g_Config.m_ClLocalServerInvasionFloor - 1) / (float)(MaxFloor - 1)) *
									  (MaxFloor - 1) +
								  0.5f);
				else
					UI()->DoLabelScaled(&Value, Localize("Complete more floors to unlock"), 9.0f, 0);
			}
		}
		else
		{
			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_DIFFICULTY);
			const char *pDifficultyLabel = "Difficulty";
			str_format(
				aLabel, sizeof(aLabel), "%s: %d", Localize(pDifficultyLabel), g_Config.m_ClLocalServerDifficulty);
			UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
			Control.HMargin(L(5.0f), &Control);
			g_Config.m_ClLocalServerDifficulty =
				1 + (int)(DoScrollbarH(&g_Config.m_ClLocalServerDifficulty,
									   &Control,
									   (g_Config.m_ClLocalServerDifficulty - 1) / 49.0f) *
							  49.0f +
						  0.5f);
		}

		SplitSettingRow(&Label, &Control);
		if(ModeDef.m_HasBots)
		{
			DrawFocusMarker(Label, FOCUS_BOTS);
			g_Config.m_ClLocalServerBots = clamp(g_Config.m_ClLocalServerBots, 0, 16);
			if(g_Config.m_ClLocalServerBots == 0)
				str_format(aLabel,
						   sizeof(aLabel),
						   "%s: %s",
						   Localize(LocalGamePopulationLabel(g_Config.m_ClLocalServerMode)),
						   Localize("No bots"));
			else
				str_format(aLabel,
						   sizeof(aLabel),
						   "%s: %d",
						   Localize(LocalGamePopulationLabel(g_Config.m_ClLocalServerMode)),
						   g_Config.m_ClLocalServerBots);
			UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
			Control.HMargin(L(5.0f), &Control);
			const int MaxBots = 16;
			if(MaxBots > 0)
				g_Config.m_ClLocalServerBots =
					(int)(DoScrollbarH(
							  &g_Config.m_ClLocalServerBots, &Control, g_Config.m_ClLocalServerBots / (float)MaxBots) *
							  MaxBots +
						  0.5f);
			else
				g_Config.m_ClLocalServerBots = 0;
		}
		else
		{
			UI()->DoLabelScaled(&Label, Localize("Enemy scaling"), 12.0f, -1);
			UI()->DoLabelScaled(&Control,
								Localize(g_Config.m_ClLocalServerMode == LOCAL_MODE_INVASION ||
												 g_Config.m_ClLocalServerMode == LOCAL_MODE_TUTORIAL
											 ? "Automatic by floor and party size"
											 : "Health, elites and party size"),
								11.0f,
								-1);
		}

		SplitSettingRow(&Label, &Control);
		DrawFocusMarker(Label, FOCUS_RANDOM_SEED);
		if(DoButton_CheckBox(
			   &s_RandomSeedButton, Localize("Random map seed"), g_Config.m_ClLocalServerRandomSeed, &Label))
			g_Config.m_ClLocalServerRandomSeed ^= 1;
		UI()->DoLabelScaled(&Control, Localize("New layout every launch"), 10.0f, -1);

		if(!g_Config.m_ClLocalServerRandomSeed)
		{
			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_SEED);
			UI()->DoLabelScaled(&Label, Localize("Map seed"), 12.0f, -1);
			if(s_SeedTextValue != g_Config.m_ClLocalServerSeed)
			{
				str_format(s_aSeedText, sizeof(s_aSeedText), "%d", g_Config.m_ClLocalServerSeed);
				s_SeedTextValue = g_Config.m_ClLocalServerSeed;
			}
			if(DoEditBox(s_aSeedText, &Control, s_aSeedText, sizeof(s_aSeedText), 12.0f, &s_SeedOffset))
			{
				g_Config.m_ClLocalServerSeed = clamp(str_toint(s_aSeedText), 0, 0x7FFFFFFF);
				s_SeedTextValue = g_Config.m_ClLocalServerSeed;
			}
			if(!CLineInput::GetActiveInput() && !s_aSeedText[0])
				s_SeedTextValue = -1;
		}
	}

	if(DrawSectionHeader(2, "Rules", FOCUS_SECTION_RULES))
	{
		if(ModeDef.m_HasRoguelite)
		{
			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_ROGUELITE);
			if(DoButton_CheckBox(
				   &s_RogueliteButton, Localize("Roguelite Director"), g_Config.m_ClLocalServerRoguelite, &Label))
				g_Config.m_ClLocalServerRoguelite ^= 1;
			UI()->DoLabelScaled(&Control, Localize("Perks, resources and research"), 10.0f, -1);

			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_CONTRACTS);
			if(DoButton_CheckBox(&s_ContractsButton,
								 Localize("Team contracts"),
								 g_Config.m_ClLocalServerContracts && g_Config.m_ClLocalServerRoguelite,
								 &Label) &&
			   g_Config.m_ClLocalServerRoguelite)
				g_Config.m_ClLocalServerContracts ^= 1;
			UI()->DoLabelScaled(&Control,
								Localize(g_Config.m_ClLocalServerRoguelite ? "Offer optional team challenges"
																		   : "Requires Roguelite Director"),
								10.0f,
								-1);
		}

		if(g_Config.m_ClLocalServerMode != LOCAL_MODE_INVASION && g_Config.m_ClLocalServerMode != LOCAL_MODE_TUTORIAL)
		{
			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_MODE_RULE);
			UI()->DoLabelScaled(&Label, Localize(LocalGameRuleLabel(ModeDef.m_Rule)), 12.0f, -1);
			Control.VSplitLeft(L(30.0f), &Previous, &Value);
			Value.VSplitRight(L(30.0f), &Value, &Next);
			if(DoButton_Menu(&s_RulePrevious, "-", 0, &Previous))
				AdjustModeRule(-1);
			if(DoButton_Menu(&s_RuleNext, "+", 0, &Next))
				AdjustModeRule(1);
			if(g_Config.m_ClLocalServerMode == LOCAL_MODE_HORDE && g_Config.m_ClLocalServerHordeWaves == 0)
				str_copy(aLabel, Localize("Endless"), sizeof(aLabel));
			else if(g_Config.m_ClLocalServerMode == LOCAL_MODE_HORDE)
				str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerHordeWaves);
			else if(g_Config.m_ClLocalServerMode == LOCAL_MODE_EXTRACTION)
				str_format(aLabel, sizeof(aLabel), Localize("%d min"), g_Config.m_ClLocalServerExtractionTime);
			else if(int *pRule = LocalModeRuleConfig(ModeDef.m_Rule))
				str_format(aLabel, sizeof(aLabel), "%d", *pRule);
			UI()->DoLabelScaled(&Value, aLabel, 12.0f, 0);
		}

		CUIRect RuleNote;
		Settings.HSplitTop(L(36.0f), &RuleNote, &Settings);
		UI()->DoLabelScaled(
			&RuleNote, Localize("Rules apply the next time the server starts."), 10.0f, -1, (int)RuleNote.w);
	}

	DrawMenuInset(&StatusBar, CUI::CORNER_ALL);
	StatusBar.Margin(L(8.0f), &StatusBar);
	CUIRect Status, Actions;
	const float StatusWidth = clamp(StatusBar.w * 0.62f, 390.0f, 520.0f);
	StatusBar.VSplitLeft(L(StatusWidth), &Status, &Actions);
	const char *pStatus = Localize("Ready to start");
	if(m_LocalServerState == LOCAL_SERVER_STARTING)
		pStatus = Localize("Starting local server and waiting for readiness...");
	else if(m_LocalServerState == LOCAL_SERVER_RUNNING)
		pStatus = Localize(m_LocalServerAutoJoin ? "Local server is ready; joining..." : "Local server is running");
	else if(m_LocalServerState == LOCAL_SERVER_STOPPING)
		pStatus = Localize("Stopping local server...");
	else if(m_LocalServerState == LOCAL_SERVER_FAILED)
	{
		if(m_LocalServerExitCode == LOCAL_SERVER_ERROR_EXECUTABLE)
			pStatus = Localize("Server executable was not found or could not be started");
		else if(m_LocalServerExitCode == LOCAL_SERVER_ERROR_PORT)
			pStatus = Localize("No free local server port was found");
		else if(m_LocalServerExitCode == LOCAL_SERVER_ERROR_TIMEOUT)
			pStatus = Localize("Local server did not become ready in time");
		else
			pStatus = Localize("Local server stopped unexpectedly");
	}
	CUIRect StatusLine, SummaryLine, DetailLine;
	Status.HSplitTop(L(18.0f), &StatusLine, &Status);
	Status.HSplitTop(L(17.0f), &SummaryLine, &DetailLine);
	UI()->DoLabelScaled(&StatusLine, pStatus, 12.0f, -1);
	const char *pSummary = (m_LocalServerState == LOCAL_SERVER_STOPPED || !m_aLocalServerSummary[0])
							   ? aPreviewSummary
							   : m_aLocalServerSummary;
	UI()->DoLabelScaled(&SummaryLine, pSummary, 8.5f, -1, (int)SummaryLine.w);
	const char *pDetail = Localize("Arrows / D-pad: select and adjust · Enter / A: confirm");
	if(m_LocalServerState == LOCAL_SERVER_FAILED)
	{
		if(m_LocalServerExitCode == LOCAL_SERVER_ERROR_PORT)
			pDetail = Localize("The preferred port and the next nine ports are already in use.");
		else if(m_aLocalServerErrorDetail[0])
			pDetail = m_aLocalServerErrorDetail;
	}
	UI()->DoLabelScaled(&DetailLine, pDetail, 8.0f, -1, (int)DetailLine.w);

	CUIRect Button;
	if(m_LocalServerState == LOCAL_SERVER_STOPPED || m_LocalServerState == LOCAL_SERVER_FAILED)
	{
		Actions.VSplitRight(L(150.0f), &Actions, &Button);
		if(DoButton_Menu(&s_StartButton,
						 Localize("Start and join"),
						 m_LocalServerFocus == FOCUS_PRIMARY_ACTION,
						 &Button,
						 BUTTONSTYLE_ACCENT))
			StartLocalServer(true);
		if(m_LocalServerState == LOCAL_SERVER_FAILED && m_aLocalServerLogPath[0])
		{
			Actions.VSplitRight(L(6.0f), &Actions, 0);
			Actions.VSplitRight(L(78.0f), &Actions, &Button);
			if(DoButton_Menu(&s_LogButton, Localize("Log"), 0, &Button))
			{
				char aBody[512];
				str_format(aBody, sizeof(aBody), "%s\n\n%s", m_aLocalServerLogPath, m_aLocalServerErrorDetail);
				PopupMessage(Localize("Local server log"), aBody, Localize("OK"));
			}
		}
	}
	else if(m_LocalServerState == LOCAL_SERVER_RUNNING)
	{
		Actions.VSplitRight(L(92.0f), &Actions, &Button);
		if(DoButton_Menu(
			   &s_StopButton, Localize("Stop"), m_LocalServerFocus == FOCUS_STOP, &Button, BUTTONSTYLE_DANGER))
			StopLocalServer(false);
		Actions.VSplitRight(L(6.0f), &Actions, 0);
		Actions.VSplitRight(L(92.0f), &Actions, &Button);
		if(DoButton_Menu(&s_RestartButton, Localize("Restart"), m_LocalServerFocus == FOCUS_RESTART, &Button))
			StopLocalServer(true);
		Actions.VSplitRight(L(6.0f), &Actions, 0);
		if(!IsConnectedToLocalServer())
		{
			Actions.VSplitRight(L(92.0f), &Actions, &Button);
			if(DoButton_Menu(&s_JoinButton,
							 Localize("Join"),
							 m_LocalServerFocus == FOCUS_PRIMARY_ACTION,
							 &Button,
							 BUTTONSTYLE_ACCENT))
				JoinLocalServer();
		}
	}
	else
	{
		Actions.VSplitRight(L(110.0f), 0, &Button);
		if(DoButton_Menu(&s_StopButton,
						 Localize("Cancel"),
						 m_LocalServerFocus == FOCUS_PRIMARY_ACTION,
						 &Button,
						 BUTTONSTYLE_DANGER))
			StopLocalServer(false);
	}
}

void CMenus::RenderFront(CUIRect MainView)
{
	s_ResetMenu = false;
	const float ScaleDivisor = max(1.0f, UI()->Scale());
	auto L = [ScaleDivisor](float Value)
	{
		return Value / ScaleDivisor;
	};
	const float dt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	const float ResponsiveWidth = UI()->Screen()->w / max(1.0f, UI()->Scale());
	const bool Compact = ResponsiveWidth < 760.0f || MainView.h < L(480.0f);
	CUIRect Canvas = MainView;
	Canvas.Margin(L(12.0f), &Canvas);

	CMenuHomeState HomeState = {m_LocalServerState == LOCAL_SERVER_STARTING,
								m_LocalServerState == LOCAL_SERVER_RUNNING,
								IsConnectedToLocalServer(),
								g_Config.m_ClTutorialState == 1,
								g_Config.m_ClTutorialChapter};
	const CMenuHomePrimary Primary = ResolveMenuHomePrimary(HomeState);

	// A slim edge header replaces the previous full-width hero panel so the
	// shader remains the visual centre of gravity.
	CUIRect TopRail, Body, BottomRail;
	Canvas.HSplitTop(L(54.0f), &TopRail, &Body);
	Body.HSplitBottom(L(42.0f), &Body, &BottomRail);
	CUIRect Brand, NetworkRail;
	TopRail.VSplitLeft(Compact ? min(L(190.0f), TopRail.w * .42f) : min(L(245.0f), TopRail.w * .34f), &Brand, &NetworkRail);
	NetworkRail.VSplitLeft(L(16.0f), 0, &NetworkRail);
	CUIRect BrandCode = Brand;
	BrandCode.y += L(18.0f);
	BrandCode.h = L(13.0f);
	TextRender()->TextColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, 1.0f);
	UI()->DoLabelScaled(&BrandCode, "NINSLASH (NEED A NEW LOGO VERSION)", L(7.0f), -1);
	TextRender()->TextColor(1, 1, 1, 1);

	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	const bool PlatformOnline = pPlatform && pPlatform->Available();
	const char *pServerStatus = m_LocalServerState == LOCAL_SERVER_RUNNING	  ? "Local server · running"
								: m_LocalServerState == LOCAL_SERVER_STARTING ? "Local server · starting"
								: m_LocalServerState == LOCAL_SERVER_FAILED	  ? "Local server · attention"
																	  : "Local server · idle";
	const vec4 ServerColor = m_LocalServerState == LOCAL_SERVER_FAILED
								 ? ms_ColorDanger
								 : (m_LocalServerState == LOCAL_SERVER_RUNNING ? ms_ColorAccentDim : vec4(.62f, .72f, .78f, 1.0f));
	CUIRect NetworkBadge, ServerBadge;
	if(Compact)
	{
		NetworkRail.VSplitRight(min(L(116.0f), NetworkRail.w), 0, &NetworkBadge);
	}
	else
	{
		NetworkRail.VSplitRight(min(L(154.0f), NetworkRail.w * .48f), &NetworkRail, &NetworkBadge);
		NetworkRail.VSplitRight(L(7.0f), &NetworkRail, 0);
		NetworkRail.VSplitRight(min(L(166.0f), NetworkRail.w), 0, &ServerBadge);
	}
	DrawStatusBadge(NetworkBadge,
					Localize(PlatformOnline ? "Steam · online" : "Standalone · UDP ready"),
					ms_ColorAccentDim);
	if(!Compact)
		DrawStatusBadge(ServerBadge, Localize(pServerStatus), ServerColor);

	// Keyboard and gamepad focus belongs to the four deployment actions. Mouse
	// movement switches back to hover-only disclosure through OnMouseMove().
	for(int EventIndex = 0; EventIndex < m_NumInputEvents; EventIndex++)
	{
		const IInput::CEvent &Event = m_aInputEvents[EventIndex];
		if(!(Event.m_Flags & IInput::FLAG_PRESS))
			continue;
		if(Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP)
			m_HomeActionFocus = (m_HomeActionFocus + 3) % 4;
		else if(Event.m_Key == KEY_DOWN || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN)
			m_HomeActionFocus = (m_HomeActionFocus + 1) % 4;
	}
	m_HomeActionFocus = clamp(m_HomeActionFocus, 0, 3);

	CUIRect ActionRegion, DetailRegion;
	Body.VSplitLeft(Compact ? min(Body.w * .54f, L(330.0f)) : min(Body.w * .39f, L(410.0f)), &ActionRegion, &DetailRegion);
	ActionRegion.VSplitLeft(L(18.0f), 0, &ActionRegion);
	ActionRegion.VSplitRight(L(12.0f), &ActionRegion, 0);
	CUIRect ActionHeading, ActionList;
	ActionRegion.HSplitTop(L(57.0f), &ActionHeading, &ActionList);
	CUIRect HeadingKicker, HeadingTitle;
	ActionHeading.HSplitTop(L(15.0f), &HeadingKicker, &HeadingTitle);
	TextRender()->TextColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .90f);
	UI()->DoLabelScaled(&HeadingKicker, Localize("TACTICAL COMMAND"), L(7.5f), -1);
	TextRender()->TextColor(1, 1, 1, 1);
	UI()->DoLabelScaled(&HeadingTitle, Localize("Choose how to deploy"), L(16.0f), -1);

	static int s_aActionButtons[4];
	const char *apTitles[4] = {Primary.m_pTitle, "Browse rooms", "Quick match", "Training"};
	const char *apCategories[4] = {"RECOMMENDED ACTION", "MULTIPLAYER · COMMUNITY", "SOLO · 1 CLICK", "SOLO · 30–45 MIN"};
	const char *apDescriptions[4] = {Primary.m_pDescription,
								  "Dedicated servers, LAN, Favorites and Steam rooms",
								  "Jump straight into a bot deathmatch — no setup.",
								  "Six guided chapters, always replayable."};
	const vec4 aActionColors[4] = {ms_ColorAccent, vec4(.42f, .64f, 1.0f, 1.0f), ms_ColorAccentWarm, ms_ColorAccentDim};

	// TODO: Reintroduce a right-side information panel when a real live-data source exists.

	const float RowHeight = min(L(62.0f), max(L(42.0f), (ActionList.h - L(16.0f)) / 4.0f));
	const float RowGap = min(L(8.0f), max(L(3.0f), (ActionList.h - RowHeight * 4.0f) / 3.0f));
	CUIRect aActionRects[4];
	bool aActionClicks[4] = {false, false, false, false};
	int HoveredAction = -1;
	for(int i = 0; i < 4; i++)
	{
		aActionRects[i] = {ActionList.x, ActionList.y + i * (RowHeight + RowGap), ActionList.w, RowHeight};
		const bool KeyboardSelected = m_LastInputDevice != 0 && m_HomeActionFocus == i;
		const bool MouseInside = m_LastInputDevice == 0 && UI()->MouseInside(&aActionRects[i]);
		if(MouseInside)
			HoveredAction = i;
		const bool Selected = KeyboardSelected || MouseInside;
		const float Select = AnimSelected(&s_aActionButtons[i], Selected, 15.0f);
		const float Hover = AnimHover(&s_aActionButtons[i], 16.0f);
		const float Press = AnimPressed(&s_aActionButtons[i], 22.0f);
		const float Reveal = max(Select, Hover);
		CUIRect Visual = aActionRects[i];
		Visual.x += L(7.0f) * MenuEaseOutCubic(Reveal);
		Visual.y += Press * L(1.0f);

		const vec4 &Color = aActionColors[i];
		CUIRect Node = {Visual.x, Visual.y + Visual.h * .5f - L(3.0f), L(6.0f), L(6.0f)};
		vec4 NodeColor = Color;
		NodeColor.a = .48f + Reveal * .52f;
		DrawTechShape(&Node, NodeColor, L(2.8f));
		IGraphics::CLineItem Signal(Node.x + Node.w + L(4.0f),
								 Node.y + Node.h * .5f,
								 Node.x + Node.w + L(18.0f + 19.0f * Reveal),
								 Node.y + Node.h * .5f);
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(Color.r, Color.g, Color.b, .32f + Reveal * .55f);
		Graphics()->LinesDraw(&Signal, 1);
		Graphics()->LinesEnd();
		if(Reveal > .01f)
		{
			CUIRect SelectionWash = Visual;
			SelectionWash.x -= L(5.0f);
			SelectionWash.w = min(SelectionWash.w, L(250.0f));
			vec4 Fill = vec4(Color.r * .05f, Color.g * .09f, Color.b * .10f, Reveal * .16f);
			DrawTechShape(&SelectionWash, Fill, min(L(8.0f), SelectionWash.h * .16f));
		}

		CUIRect Index, Title;
		Visual.VSplitLeft(L(39.0f), &Index, &Title);
		Title.VSplitLeft(L(8.0f), 0, &Title);
		char aIndex[8];
		str_format(aIndex, sizeof(aIndex), "%02d", i + 1);
		TextRender()->TextColor(Color.r, Color.g, Color.b, .50f + Reveal * .50f);
		UI()->DoLabelScaled(&Index, aIndex, L(8.5f), 1);
		TextRender()->TextColor(.76f + Reveal * .24f, .82f + Reveal * .18f, .86f + Reveal * .14f, 1.0f);
		const char *pTitle = Localize(apTitles[i]);
		UI()->DoLabelScaled(&Title,
						pTitle,
						FitScaledLabelFontSize(TextRender(), pTitle, L(17.0f), Title.w, UI()->Scale()),
						-1);
		TextRender()->TextColor(1, 1, 1, 1);
		aActionClicks[i] = UI()->DoButtonLogic(&s_aActionButtons[i], pTitle, 0, &aActionRects[i]) != 0;
	}

	const int DetailAction = m_LastInputDevice == 0 ? HoveredAction : m_HomeActionFocus;
	static int s_LastDetailAction = 0;
	static float s_DetailReveal = 0.0f;
	if(DetailAction >= 0)
		s_LastDetailAction = DetailAction;
	s_DetailReveal = SmoothToward(s_DetailReveal, DetailAction >= 0 ? 1.0f : 0.0f, dt, DetailAction >= 0 ? 14.0f : 10.0f);
	if(fabs(s_DetailReveal - (DetailAction >= 0 ? 1.0f : 0.0f)) < .001f)
		s_DetailReveal = DetailAction >= 0 ? 1.0f : 0.0f;
	s_LastDetailAction = clamp(s_LastDetailAction, 0, 3);

	// Details occupy only the lower-right corner and disappear completely when
	// the mouse leaves the action list.
	if(s_DetailReveal > .01f)
	{
		const float DetailWidth = Compact ? min(DetailRegion.w - L(8.0f), L(292.0f)) : min(DetailRegion.w * .70f, L(410.0f));
		const float DetailHeight = Compact ? min(DetailRegion.h * .52f, L(205.0f)) : min(DetailRegion.h * .48f, L(220.0f));
		CUIRect Detail = {DetailRegion.x + DetailRegion.w - DetailWidth - L(10.0f),
						  DetailRegion.y + DetailRegion.h - DetailHeight - L(10.0f),
						  DetailWidth,
						  DetailHeight};
		Detail.x += (1.0f - MenuEaseOutCubic(s_DetailReveal)) * L(13.0f);
		const vec4 &Color = aActionColors[s_LastDetailAction];
		DrawGlassSurface(&Detail,
						 vec4(.010f, .044f, .066f, .23f * s_DetailReveal),
						 vec4(Color.r, Color.g, Color.b, .25f * s_DetailReveal),
						 min(L(12.0f), Detail.h * .08f),
						 L(.7f) * s_DetailReveal);
		vec4 Brackets = Color;
		Brackets.a = .36f * s_DetailReveal;
		DrawTechBrackets(&Detail, Brackets, L(15.0f), L(6.0f));
		Detail.Margin(L(16.0f), &Detail);
		CUIRect Category, DetailTitle, Description;
		Detail.HSplitTop(L(16.0f), &Category, &Detail);
		Detail.HSplitTop(L(34.0f), &DetailTitle, &Detail);
		Detail.HSplitBottom(L(20.0f), &Description, 0);
		TextRender()->TextColor(Color.r, Color.g, Color.b, s_DetailReveal);
		UI()->DoLabelScaled(&Category, Localize(apCategories[s_LastDetailAction]), L(7.5f), -1);
		TextRender()->TextColor(1, 1, 1, s_DetailReveal);
		const char *pDetailTitle = Localize(apTitles[s_LastDetailAction]);
		UI()->DoLabelScaled(&DetailTitle,
						pDetailTitle,
						FitScaledLabelFontSize(TextRender(), pDetailTitle, L(19.0f), DetailTitle.w, UI()->Scale()),
						-1);
		char aDescription[192];
		if(s_LastDetailAction == 0 && Primary.m_Chapter)
			str_format(aDescription, sizeof(aDescription), Localize(apDescriptions[0]), Primary.m_Chapter);
		else
			str_copy(aDescription, Localize(apDescriptions[s_LastDetailAction]), sizeof(aDescription));
		TextRender()->TextColor(.76f, .84f, .88f, s_DetailReveal);
		UI()->DoLabelScaled(&Description, aDescription, L(8.5f), -1, (int)Description.w);
		TextRender()->TextColor(1, 1, 1, 1);
	}

	if(m_LastInputDevice != 0 && m_EnterPressed)
	{
		aActionClicks[m_HomeActionFocus] = true;
		m_EnterPressed = false;
	}

	if(aActionClicks[0])
	{
		if(Primary.m_Action == MENU_HOME_JOIN_LOCAL)
			JoinLocalServer();
		else if(Primary.m_Action == MENU_HOME_SHOW_LOCAL)
		{
			if(IsConnectedToLocalServer())
				SetActive(false);
			else
			{
				m_PlayTab = 1;
				g_Config.m_UiPage = PAGE_LOCAL_SERVER;
			}
		}
		else if(Primary.m_Action == MENU_HOME_CONTINUE_TUTORIAL)
			StartTutorial(Primary.m_Chapter, true);
		else
		{
			m_PlayTab = 1;
			m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
			m_LocalServerFocus = LOCAL_MODE_INVASION;
			g_Config.m_UiPage = PAGE_LOCAL_SERVER;
		}
	}
	else if(aActionClicks[1])
	{
		m_PlayTab = 0;
		ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
		g_Config.m_UiPage = PAGE_INTERNET;
	}
	else if(aActionClicks[2])
		StartQuickMatch();
	else if(aActionClicks[3])
		OpenTutorialChapterSelect();

	// Secondary destinations become a quiet bottom-edge command strip. They
	// have generous hit targets but no permanent button containers.
	static int s_aUtilityButtons[6];
	const char *apUtilityLabels[6] = {"Customize", "Research", "Workshop", "Demos", "Settings", "Quit"};
	const int aUtilityPages[6] = {PAGE_CUSTOMIZE, PAGE_RESEARCH, PAGE_MODS, PAGE_DEMOS, PAGE_SETTINGS, -1};
	BottomRail.VSplitLeft(L(18.0f), 0, &BottomRail);
	BottomRail.VSplitRight(L(8.0f), &BottomRail, 0);
	const float UtilityGap = L(4.0f);
	for(int i = 0; i < 6; i++)
	{
		const float Width = (BottomRail.w - UtilityGap * (5 - i)) / (6 - i);
		CUIRect Utility;
		BottomRail.VSplitLeft(Width, &Utility, &BottomRail);
		if(i < 5)
			BottomRail.VSplitLeft(UtilityGap, 0, &BottomRail);
		const float Hover = AnimHover(&s_aUtilityButtons[i], 18.0f);
		const vec4 Color = i == 5 ? ms_ColorDanger : ms_ColorAccent;
		CUIRect UtilityNode = {Utility.x + L(5.0f), Utility.y + Utility.h * .5f - L(2.0f), L(4.0f), L(4.0f)};
		vec4 UtilityNodeColor = Color;
		UtilityNodeColor.a = .42f + Hover * .58f;
		DrawTechShape(&UtilityNode, UtilityNodeColor, L(1.8f));
		const float ChevronX = Utility.x + Utility.w - L(8.0f);
		const float ChevronY = Utility.y + Utility.h * .5f;
		IGraphics::CLineItem aChevron[2] = {
			IGraphics::CLineItem(ChevronX - L(3.0f), ChevronY - L(3.0f), ChevronX, ChevronY),
			IGraphics::CLineItem(ChevronX, ChevronY, ChevronX - L(3.0f), ChevronY + L(3.0f))};
		Graphics()->TextureClear();
		Graphics()->LinesBegin();
		Graphics()->SetColor(Color.r, Color.g, Color.b, .35f + Hover * .55f);
		Graphics()->LinesDraw(aChevron, 2);
		Graphics()->LinesEnd();
		if(Hover > .01f)
		{
			CUIRect Signal = Utility;
			Signal.HSplitBottom(L(2.0f), 0, &Signal);
			Signal.VMargin(L(7.0f), &Signal);
			vec4 SignalColor = Color;
			SignalColor.a *= Hover;
			DrawTechShape(&Signal, SignalColor, min(L(1.0f), Signal.h * .45f));
		}
		TextRender()->TextColor(.62f + Hover * .38f, .69f + Hover * .28f, .73f + Hover * .27f, 1.0f);
		const char *pLabel = Localize(apUtilityLabels[i]);
		UI()->DoLabelScaled(&Utility,
						pLabel,
						FitScaledLabelFontSize(TextRender(), pLabel, L(8.5f), Utility.w - L(3.0f), UI()->Scale()),
						0);
		TextRender()->TextColor(1, 1, 1, 1);
		if(UI()->DoButtonLogic(&s_aUtilityButtons[i], pLabel, 0, &Utility))
		{
			if(aUtilityPages[i] >= 0)
			{
				if(aUtilityPages[i] == PAGE_RESEARCH)
					OpenResearchPage();
				else
					g_Config.m_UiPage = aUtilityPages[i];
			}
			else
				m_Popup = POPUP_QUIT;
		}
	}

	// Minimal edge marks visually lock the interface to the viewport without
	// placing a frame over the animated background.
	vec4 Edge = ms_ColorAccent;
	Edge.a = .22f;
	IGraphics::CLineItem aEdgeLines[4] = {
		IGraphics::CLineItem(Canvas.x, Canvas.y + L(49.0f), Canvas.x + L(46.0f), Canvas.y + L(49.0f)),
		IGraphics::CLineItem(Canvas.x, Canvas.y + L(49.0f), Canvas.x, Canvas.y + L(70.0f)),
		IGraphics::CLineItem(Canvas.x + Canvas.w - L(46.0f), Canvas.y + Canvas.h - L(3.0f), Canvas.x + Canvas.w, Canvas.y + Canvas.h - L(3.0f)),
		IGraphics::CLineItem(Canvas.x + Canvas.w, Canvas.y + Canvas.h - L(24.0f), Canvas.x + Canvas.w, Canvas.y + Canvas.h - L(3.0f))};
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Edge.r, Edge.g, Edge.b, Edge.a);
	Graphics()->LinesDraw(aEdgeLines, 4);
	Graphics()->LinesEnd();
}

void CMenus::RenderTutorialChapterSelect(CUIRect MainView)
{
	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(16.0f, &MainView);
	CUIRect Header, Body, Back;
	MainView.HSplitTop(48.0f, &Header, &Body);
	Header.VSplitRight(110.0f, &Header, &Back);
	UI()->DoLabelScaled(&Header, Localize("Tutorial chapters"), 20.0f, -1);
	static int s_Back;
	if(DoButton_Menu(&s_Back, Localize("Back to Play"), 0, &Back))
		g_Config.m_UiPage = PAGE_FRONT;

	const char *apNames[NUM_TUTORIAL_CHAPTERS] = {"First Deployment",
												  "Combat and Recovery",
												  "PvE Mission",
												  "Forge and Build",
												  "Build and Growth",
												  "Multiplayer Ready"};
	const char *apDescriptions[NUM_TUTORIAL_CHAPTERS] = {"Movement, weapons and the training target.",
														 "Combat, recovery and respawning.",
														 "Objectives, defense and extraction.",
														 "Materials, forging and construction.",
														 "Perks, drones and research.",
														 "Bot PvP and multiplayer rooms."};
	static int s_aChapterButtons[NUM_TUTORIAL_CHAPTERS];
	const float Gap = 10.0f;
	const float RowHeight = (Body.h - Gap) / 2.0f;
	const float ColumnWidth = (Body.w - Gap * 2.0f) / 3.0f;
	const bool HasCurrentProgress = g_Config.m_ClTutorialState == 1;

	for(int Index = 0; Index < NUM_TUTORIAL_CHAPTERS; Index++)
	{
		const int Chapter = Index + 1;
		CUIRect Card = {Body.x + (Index % 3) * (ColumnWidth + Gap),
						Body.y + (Index / 3) * (RowHeight + Gap),
						ColumnWidth,
						RowHeight};
		DrawMenuInset(&Card, CUI::CORNER_ALL);
		Card.Margin(10.0f, &Card);
		CUIRect Line, Button;
		Card.HSplitTop(22.0f, &Line, &Card);
		char aChapter[32];
		str_format(aChapter, sizeof(aChapter), Localize("Chapter %d"), Chapter);
		UI()->DoLabelScaled(&Line, aChapter, 9.0f, -1);
		Card.HSplitTop(25.0f, &Line, &Card);
		UI()->DoLabelScaled(&Line, Localize(apNames[Index]), 13.0f, -1);
		Card.HSplitTop(38.0f, &Line, &Card);
		UI()->DoLabelScaled(&Line, Localize(apDescriptions[Index]), 9.0f, -1, (int)Line.w);

		const bool Completed = TutorialChapterCompleted(Chapter, g_Config.m_ClTutorialCompletedMask);
		const bool InProgress = HasCurrentProgress && g_Config.m_ClTutorialChapter == Chapter && !Completed;
		const bool Unlocked = TutorialChapterUnlocked(
			Chapter, g_Config.m_ClTutorialCompletedMask, g_Config.m_ClTutorialChapter, HasCurrentProgress);
		char aStatus[96];
		const char *pAction;
		if(InProgress)
		{
			str_format(aStatus,
					   sizeof(aStatus),
					   Localize("Step %d of %d"),
					   clamp(g_Config.m_ClTutorialStep + 1, 1, TutorialStepCount(Chapter)),
					   TutorialStepCount(Chapter));
			pAction = "Continue";
		}
		else if(Completed)
		{
			str_copy(aStatus, Localize("Completed"), sizeof(aStatus));
			pAction = "Replay chapter";
		}
		else if(Unlocked)
		{
			str_copy(aStatus, Localize("Available"), sizeof(aStatus));
			pAction = "Start";
		}
		else
		{
			str_format(aStatus, sizeof(aStatus), Localize("Complete chapter %d first"), Chapter - 1);
			pAction = "Locked";
		}
		Card.HSplitTop(18.0f, &Line, &Card);
		UI()->DoLabelScaled(&Line, aStatus, 9.0f, -1);
		Card.HSplitBottom(30.0f, &Card, &Button);
		if(DoButton_Menu(&s_aChapterButtons[Index],
						 Localize(pAction),
						 InProgress,
						 &Button,
						 InProgress || (!Completed && Unlocked) ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL) &&
		   Unlocked)
			StartTutorial(Chapter, InProgress);
	}
}

void CMenus::RenderTutorialRoomPractice(CUIRect MainView)
{
	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(16.0f, &MainView);
	CUIRect Header, Body, Footer;
	MainView.HSplitTop(54.0f, &Header, &Body);
	Body.HSplitBottom(54.0f, &Body, &Footer);
	UI()->DoLabelScaled(&Header, Localize("Multiplayer room practice"), 20.0f, -1);
	CUIRect Subtitle = Header;
	Subtitle.y += 27.0f;
	UI()->DoLabelScaled(&Subtitle,
						Localize(g_Config.m_ClTutorialStep == 1
									 ? "Configure a simulated room, then create it."
									 : "Filter the simulated room list, then join the training room."),
						9.5f,
						-1);

	static int s_aVisibility[4];
	static int s_aFilters[3];
	static int s_Create, s_Join;
	if(g_Config.m_ClTutorialStep == 1)
	{
		CUIRect Panel = Body;
		Panel.Margin(22.0f, &Panel);
		DrawMenuInset(&Panel, CUI::CORNER_ALL);
		Panel.Margin(14.0f, &Panel);
		CUIRect Row, Label;
		Panel.HSplitTop(28.0f, &Label, &Panel);
		UI()->DoLabelScaled(&Label, Localize("Room visibility"), 12.0f, -1);
		Panel.HSplitTop(34.0f, &Row, &Panel);
		const char *apVisibility[] = {"Solo", "Friends", "LAN", "Public"};
		for(int i = 0; i < 4; i++)
		{
			CUIRect Button;
			Row.VSplitLeft(Row.w / (4 - i), &Button, &Row);
			if(DoButton_Menu(&s_aVisibility[i],
							 Localize(apVisibility[i]),
							 g_Config.m_ClRoomVisibility == i,
							 &Button,
							 g_Config.m_ClRoomVisibility == i ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
				g_Config.m_ClRoomVisibility = i;
		}
		Panel.HSplitTop(18.0f, 0, &Panel);
		Panel.HSplitTop(26.0f, &Label, &Panel);
		UI()->DoLabelScaled(&Label, Localize("Training Room · Tutorial rules · 4 slots"), 11.0f, -1);
		Panel.HSplitBottom(34.0f, &Panel, &Row);
		if(DoButton_Menu(&s_Create, Localize("Create simulated room"), 0, &Row, BUTTONSTYLE_ACCENT))
			m_pClient->m_pPveRoguelite->SendTutorialAction(TUTORIAL_ACTION_UI_ROOM_CREATE, g_Config.m_ClRoomVisibility);
	}
	else
	{
		CUIRect Filters, List, Room, Button;
		Body.HSplitTop(38.0f, &Filters, &List);
		const char *apFilters[] = {"All rooms", "Friends", "Low ping"};
		for(int i = 0; i < 3; i++)
		{
			Filters.VSplitLeft(120.0f, &Button, &Filters);
			DoButton_Menu(&s_aFilters[i],
						  Localize(apFilters[i]),
						  i == 0,
						  &Button,
						  i == 0 ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL);
			Filters.VSplitLeft(6.0f, 0, &Filters);
		}
		List.HSplitTop(12.0f, 0, &List);
		List.HSplitTop(74.0f, &Room, &List);
		DrawMenuInset(&Room, CUI::CORNER_ALL);
		Room.Margin(10.0f, &Room);
		Room.VSplitRight(150.0f, &Room, &Button);
		CUIRect Name, Meta;
		Room.HSplitTop(25.0f, &Name, &Meta);
		UI()->DoLabelScaled(&Name, Localize("Ninslash Training Room"), 13.0f, -1);
		UI()->DoLabelScaled(&Meta, Localize("Tutorial · 1/4 players · local simulation"), 9.0f, -1);
		if(DoButton_Menu(&s_Join, Localize("Join simulated room"), 0, &Button, BUTTONSTYLE_ACCENT))
			m_pClient->m_pPveRoguelite->SendTutorialAction(TUTORIAL_ACTION_UI_ROOM_JOIN, 0);
	}
	UI()->DoLabelScaled(
		&Footer, Localize("This practice does not publish a real room or contact external services."), 9.0f, -1);
}

void CMenus::RenderSteam(CUIRect MainView)
{
	// Compatibility entry point for old saved PAGE_STEAM values. The player UI
	// intentionally exposes only Mod consumption/management.
	RenderMods(MainView);
	return;
#if 0
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(8.0f, &MainView);
	if(!pPlatform || !pPlatform->Available())
	{
		UI()->DoLabelScaled(&MainView, Localize("Steam unavailable"), 16.0f, 0);
		return;
	}
	static int s_View = 0, s_RoomTab = 0, s_WorkshopTab = 0;
	CUIRect Tabs, Body, Left, Right;
	MainView.HSplitTop(28.0f, &Tabs, &Body);
	Tabs.VSplitLeft(120.0f, &Left, &Tabs);
	Tabs.VSplitLeft(120.0f, &Right, &Tabs);
	if(DoButton_MenuTab(&s_RoomTab, Localize("Rooms"), s_View == 0, &Left, CUI::CORNER_TL))
		s_View = 0;
	if(DoButton_MenuTab(&s_WorkshopTab, "Workshop", s_View == 1, &Right, CUI::CORNER_TR))
		s_View = 1;
	Body.HSplitTop(6.0f, 0, &Body);
	if(s_View == 0)
	{
		static int s_Refresh = 0, s_Private = 0, s_Friends = 0, s_Public = 0, s_Invite = 0, s_Leave = 0,
				   s_Selected = -1;
		static float s_Scroll = 0.0f;
		CUIRect Toolbar, List, Actions, Button;
		Body.HSplitTop(32.0f, &Toolbar, &List);
		List.HSplitBottom(42.0f, &List, &Actions);
		Toolbar.VSplitLeft(86.0f, &Button, &Toolbar);
		if(DoButton_Menu(&s_Refresh, Localize("Refresh"), 0, &Button))
			pPlatform->RefreshLobbyList();
		Toolbar.VSplitLeft(4.0f, 0, &Toolbar);
		Toolbar.VSplitLeft(94.0f, &Button, &Toolbar);
		if(DoButton_Menu(&s_Private, Localize("Invite only"), 0, &Button))
			Console()->ExecuteLine("steam_lobby_create invite");
		Toolbar.VSplitLeft(4.0f, 0, &Toolbar);
		Toolbar.VSplitLeft(94.0f, &Button, &Toolbar);
		if(DoButton_Menu(&s_Friends, Localize("Friends"), 0, &Button, BUTTONSTYLE_ACCENT))
			Console()->ExecuteLine("steam_lobby_create friends");
		Toolbar.VSplitLeft(4.0f, 0, &Toolbar);
		Toolbar.VSplitLeft(94.0f, &Button, &Toolbar);
		if(DoButton_Menu(&s_Public, Localize("Public"), 0, &Button))
			Console()->ExecuteLine("steam_lobby_create public");
		Toolbar.VSplitLeft(4.0f, 0, &Toolbar);
		Toolbar.VSplitLeft(86.0f, &Button, &Toolbar);
		if(DoButton_Menu(&s_Invite, Localize("Invite"), 0, &Button))
			pPlatform->OpenLobbyInviteDialog();
		Toolbar.VSplitLeft(4.0f, 0, &Toolbar);
		Toolbar.VSplitLeft(86.0f, &Button, &Toolbar);
		if(DoButton_Menu(&s_Leave, Localize("Leave"), 0, &Button))
			pPlatform->LeaveLobby();
		static int s_aRoomIDs[128];
		for(int i = 0; i < 128; i++)
			s_aRoomIDs[i] = i;
		UiDoListboxStart(
			&s_aRoomIDs, &List, 34.0f, Localize("Steam rooms"), "", pPlatform->LobbyCount(), 1, s_Selected, s_Scroll);
		for(int i = 0; i < pPlatform->LobbyCount(); i++)
		{
			CPlatformLobbyInfo Info;
			pPlatform->LobbyInfo(i, &Info);
			CListboxItem Item = UiDoListboxNextItem(&s_aRoomIDs[i], s_Selected == i);
			if(Item.m_Visible)
			{
				char aLine[512];
				str_format(aLine,
						   sizeof(aLine),
						   "%s  |  %s  |  %d/%d%s%s%s",
						   Info.m_aHostName[0] ? Info.m_aHostName : "Steam host",
						   DisplayGameType(Info.m_aGameType),
						   Info.m_Members,
						   Info.m_MaxMembers,
						   Info.m_FriendHosted ? "  FRIEND" : "",
						   Info.m_Password ? "  PASSWORD" : "",
						   Info.m_Modded ? "  MODDED" : "");
				Item.m_Rect.Margin(6.0f, &Item.m_Rect);
				UI()->DoLabelScaled(&Item.m_Rect, aLine, 11.0f, -1);
			}
		}
		s_Selected = UiDoListboxEnd(&s_Scroll, 0);
		s_Selected = clamp(s_Selected, -1, max(-1, pPlatform->LobbyCount() - 1));
		Actions.VSplitRight(110.0f, &Actions, &Button);
		static int s_Join = 0;
		if(DoButton_Menu(&s_Join, Localize("Join"), 0, &Button, BUTTONSTYLE_ACCENT) && s_Selected >= 0)
		{
			CPlatformLobbyInfo Info;
			if(pPlatform->LobbyInfo(s_Selected, &Info))
				pPlatform->JoinLobby(Info.m_LobbyID);
		}
		if(s_Selected >= 0)
		{
			CPlatformLobbyInfo Info;
			if(pPlatform->LobbyInfo(s_Selected, &Info))
			{
				char aDetail[512];
				str_format(aDetail,
						   sizeof(aDetail),
						   "SteamID %llu  |  %s  |  %s",
						   Info.m_HostSteamID,
						   Info.m_aRegion,
						   Info.m_aModHash);
				UI()->DoLabelScaled(&Actions, aDetail, 10.0f, -1);
			}
		}
	}
	else
	{
		static int s_Refresh = 0, s_Create = 0, s_Select = 0, s_Enable = 0, s_Disable = 0, s_Remove = 0,
				   s_Community = 0, s_Publish = 0, s_Selected = -1;
		static float s_Scroll = 0.0f, s_ContentOffset = 0.0f, s_PreviewOffset = 0.0f;
		static char s_aContent[512] = "", s_aPreview[512] = "";
		CUIRect Toolbar, List, Detail, Button, Row, Label, Edit;
		Body.HSplitTop(32.0f, &Toolbar, &Body);
		Toolbar.VSplitLeft(100.0f, &Button, &Toolbar);
		if(DoButton_Menu(&s_Refresh, Localize("Refresh"), 0, &Button))
			pPlatform->RefreshWorkshopItems();
		Toolbar.VSplitLeft(6.0f, 0, &Toolbar);
		Toolbar.VSplitLeft(130.0f, &Button, &Toolbar);
		if(DoButton_Menu(&s_Create, Localize("Create item"), 0, &Button))
			pPlatform->CreateWorkshopItem();
		Body.VSplitLeft(Body.w * 0.62f, &List, &Detail);
		List.VSplitRight(8.0f, &List, 0);
		static int s_aItemIDs[256];
		for(int i = 0; i < 256; i++)
			s_aItemIDs[i] = i;
		UiDoListboxStart(
			&s_aItemIDs, &List, 40.0f, "Workshop", "", pPlatform->WorkshopItemCount(), 1, s_Selected, s_Scroll);
		for(int i = 0; i < pPlatform->WorkshopItemCount(); i++)
		{
			CPlatformWorkshopItem Info;
			pPlatform->WorkshopItem(i, &Info);
			CListboxItem Item = UiDoListboxNextItem(&s_aItemIDs[i], s_Selected == i);
			if(Item.m_Visible)
			{
				char aLine[512];
				const int Percent = Info.m_Total ? (int)(Info.m_Downloaded * 100 / Info.m_Total) : 0;
				str_format(aLine,
						   sizeof(aLine),
						   "%llu  %s  v%s  %s  %d%%",
						   Info.m_PublishedFileID,
						   Info.m_aName,
						   Info.m_aVersion,
						   Info.m_Valid ? "VALID" : Info.m_aError,
						   Percent);
				Item.m_Rect.Margin(6.0f, &Item.m_Rect);
				UI()->DoLabelScaled(&Item.m_Rect, aLine, 10.0f, -1);
			}
		}
		s_Selected = UiDoListboxEnd(&s_Scroll, 0);
		s_Selected = clamp(s_Selected, -1, max(-1, pPlatform->WorkshopItemCount() - 1));
		Detail.HSplitTop(34.0f, &Row, &Detail);
		Row.VSplitLeft(Row.w / 2 - 2, &Button, &Row);
		if(DoButton_Menu(&s_Select, Localize("Use collection"), 0, &Button, BUTTONSTYLE_ACCENT) && s_Selected >= 0)
		{
			CPlatformWorkshopItem Info;
			if(pPlatform->WorkshopItem(s_Selected, &Info))
			{
				str_format(g_Config.m_ClModIds, sizeof(g_Config.m_ClModIds), "%llu", Info.m_PublishedFileID);
				pPlatform->RefreshWorkshopItems();
			}
		}
		Row.VSplitLeft(4.0f, 0, &Row);
		if(DoButton_Menu(&s_Disable, Localize("Disable"), 0, &Row) && s_Selected >= 0)
		{
			CPlatformWorkshopItem Info;
			if(pPlatform->WorkshopItem(s_Selected, &Info))
				pPlatform->SetWorkshopItemDisabled(Info.m_PublishedFileID, true);
		}
		Detail.HSplitTop(4.0f, 0, &Detail);
		Detail.HSplitTop(34.0f, &Row, &Detail);
		Row.VSplitLeft(Row.w / 2 - 2, &Button, &Row);
		if(DoButton_Menu(&s_Enable, Localize("Enable"), 0, &Button) && s_Selected >= 0)
		{
			CPlatformWorkshopItem Info;
			if(pPlatform->WorkshopItem(s_Selected, &Info))
				pPlatform->SetWorkshopItemDisabled(Info.m_PublishedFileID, false);
		}
		Row.VSplitLeft(4.0f, 0, &Row);
		if(DoButton_Menu(&s_Remove, Localize("Unsubscribe"), 0, &Row) && s_Selected >= 0)
		{
			CPlatformWorkshopItem Info;
			if(pPlatform->WorkshopItem(s_Selected, &Info))
				pPlatform->UnsubscribeWorkshopItem(Info.m_PublishedFileID);
		}
		Detail.HSplitTop(4.0f, 0, &Detail);
		Detail.HSplitTop(34.0f, &Button, &Detail);
		if(DoButton_Menu(&s_Community, Localize("Community page / Report"), 0, &Button) && s_Selected >= 0)
		{
			CPlatformWorkshopItem Info;
			if(pPlatform->WorkshopItem(s_Selected, &Info))
				pPlatform->OpenWorkshopItemPage(Info.m_PublishedFileID);
		}
		Detail.HSplitTop(16.0f, 0, &Detail);
		Detail.HSplitTop(24.0f, &Label, &Detail);
		UI()->DoLabelScaled(&Label, Localize("Content directory"), 10.0f, -1);
		Detail.HSplitTop(28.0f, &Edit, &Detail);
		DoEditBox(s_aContent, &Edit, s_aContent, sizeof(s_aContent), 10.0f, &s_ContentOffset);
		Detail.HSplitTop(8.0f, 0, &Detail);
		Detail.HSplitTop(24.0f, &Label, &Detail);
		UI()->DoLabelScaled(&Label, Localize("Preview file"), 10.0f, -1);
		Detail.HSplitTop(28.0f, &Edit, &Detail);
		DoEditBox(s_aPreview, &Edit, s_aPreview, sizeof(s_aPreview), 10.0f, &s_PreviewOffset);
		Detail.HSplitTop(10.0f, 0, &Detail);
		Detail.HSplitTop(34.0f, &Button, &Detail);
		if(DoButton_Menu(&s_Publish, Localize("Publish update"), 0, &Button, BUTTONSTYLE_ACCENT) && s_Selected >= 0)
		{
			CPlatformWorkshopItem Info;
			if(pPlatform->WorkshopItem(s_Selected, &Info))
				pPlatform->UpdateWorkshopItem(Info.m_PublishedFileID, s_aContent, s_aPreview);
		}
		CPlatformWorkshopPublishStatus Status;
		pPlatform->WorkshopPublishStatus(&Status);
		char aStatus[512];
		const int Percent = Status.m_Total ? (int)(Status.m_Processed * 100 / Status.m_Total) : 0;
		if(Status.m_Active)
			str_format(
				aStatus, sizeof(aStatus), "%s  %d%%  ID %llu", Status.m_aStatus, Percent, Status.m_PublishedFileID);
		else
			str_format(aStatus, sizeof(aStatus), "%s  ID %llu", Status.m_aStatus, Status.m_PublishedFileID);
		Detail.HSplitTop(12.0f, 0, &Detail);
		UI()->DoLabelScaled(&Detail, aStatus, 10.0f, -1);
	}
#endif
}

int CMenus::SteamAvatarTexture(unsigned long long UserID)
{
	if(!UserID)
		return -1;
	const int64 Now = time_get();
	int Slot = -1;
	for(int i = 0; i < 128; i++)
	{
		if(m_aSteamAvatars[i].m_UserID == UserID)
		{
			Slot = i;
			m_aSteamAvatars[i].m_LastUsed = Now;
			if(m_aSteamAvatars[i].m_Texture >= 0)
				return m_aSteamAvatars[i].m_Texture;
			if(Now < m_aSteamAvatars[i].m_NextRetry)
				return -1;
			break;
		}
	}
	if(Slot < 0)
	{
		for(int i = 0; i < 128; i++)
			if(m_aSteamAvatars[i].m_UserID == 0)
			{
				Slot = i;
				break;
			}
		if(Slot < 0)
		{
			Slot = 0;
			for(int i = 1; i < 128; i++)
				if(m_aSteamAvatars[i].m_LastUsed < m_aSteamAvatars[Slot].m_LastUsed)
					Slot = i;
			if(m_aSteamAvatars[Slot].m_Texture >= 0)
				Graphics()->UnloadTexture(m_aSteamAvatars[Slot].m_Texture);
		}
		m_aSteamAvatars[Slot].m_UserID = UserID;
		m_aSteamAvatars[Slot].m_Texture = -1;
		m_aSteamAvatars[Slot].m_LastUsed = Now;
		m_aSteamAvatars[Slot].m_NextRetry = 0;
	}
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	unsigned char aPixels[128 * 128 * 4];
	int Width = 0, Height = 0;
	const int Result =
		pPlatform ? pPlatform->UserAvatarRGBA(UserID, 64, aPixels, sizeof(aPixels), &Width, &Height) : -1;
	if(Result != 1)
	{
		m_aSteamAvatars[Slot].m_NextRetry = Now + time_freq() * (Result == 0 ? 2 : 30);
		return -1;
	}
	m_aSteamAvatars[Slot].m_Texture = Graphics()->LoadTextureRaw(
		Width, Height, CImageInfo::FORMAT_RGBA, aPixels, CImageInfo::FORMAT_RGBA, IGraphics::TEXLOAD_NOMIPMAPS);
	m_aSteamAvatars[Slot].m_NextRetry = 0;
	return m_aSteamAvatars[Slot].m_Texture;
}

void CMenus::DrawSteamAvatar(const CUIRect &Rect, unsigned long long UserID)
{
	const int Texture = SteamAvatarTexture(UserID);
	if(Texture < 0)
	{
		DrawMenuInset(&Rect, CUI::CORNER_ALL);
		CUIRect Initial = Rect;
		UI()->DoLabelScaled(&Initial, "?", Rect.h * 0.48f, 0);
		return;
	}
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem Quad(Rect.x, Rect.y, Rect.w, Rect.h);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
}

int CMenus::WorkshopPreviewTexture(const CPlatformWorkshopItem &Item)
{
	if(!Item.m_PublishedFileID || !Item.m_aPreviewURL[0])
		return -1;
	const int64 Now = time_get();
	int Slot = -1;
	for(int i = 0; i < 32; i++)
		if(m_aWorkshopPreviews[i].m_PublishedFileID == Item.m_PublishedFileID &&
		   m_aWorkshopPreviews[i].m_UpdatedAt == Item.m_UpdatedAt)
		{
			Slot = i;
			break;
		}
	if(Slot < 0)
	{
		for(int i = 0; i < 32; i++)
			if(!m_aWorkshopPreviews[i].m_PublishedFileID)
			{
				Slot = i;
				break;
			}
		if(Slot < 0)
		{
			Slot = 0;
			for(int i = 1; i < 32; i++)
				if(m_aWorkshopPreviews[i].m_LastUsed < m_aWorkshopPreviews[Slot].m_LastUsed)
					Slot = i;
		}
		if(m_aWorkshopPreviews[Slot].m_Texture >= 0)
			Graphics()->UnloadTexture(m_aWorkshopPreviews[Slot].m_Texture);
		mem_zero(&m_aWorkshopPreviews[Slot], sizeof(m_aWorkshopPreviews[Slot]));
		m_aWorkshopPreviews[Slot].m_PublishedFileID = Item.m_PublishedFileID;
		m_aWorkshopPreviews[Slot].m_UpdatedAt = Item.m_UpdatedAt;
		m_aWorkshopPreviews[Slot].m_Texture = -1;
	}
	m_aWorkshopPreviews[Slot].m_LastUsed = Now;
	if(m_aWorkshopPreviews[Slot].m_Texture >= 0)
		return m_aWorkshopPreviews[Slot].m_Texture;
	if(!m_aWorkshopPreviews[Slot].m_OperationID && Now >= m_aWorkshopPreviews[Slot].m_NextRetry)
	{
		IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
		m_aWorkshopPreviews[Slot].m_OperationID =
			pPlatform ? pPlatform->RequestWorkshopPreview(Item.m_PublishedFileID) : 0;
		if(!m_aWorkshopPreviews[Slot].m_OperationID)
			m_aWorkshopPreviews[Slot].m_NextRetry = Now + time_freq() * 5;
	}
	return -1;
}

void CMenus::DrawWorkshopPreview(const CUIRect &Rect, const CPlatformWorkshopItem &Item)
{
	const int Texture = WorkshopPreviewTexture(Item);
	if(Texture < 0)
	{
		DrawMenuInset(&Rect, CUI::CORNER_ALL);
		const char *apTypes[] = {"MOD", "MAP", "PRESET", "CHALLENGE"};
		CUIRect Label = Rect;
		UI()->DoLabelScaled(&Label, apTypes[clamp(Item.m_ContentType, 0, 3)], min(12.0f, Rect.h * 0.22f), 0);
		return;
	}
	Graphics()->TextureSet(Texture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem Quad(Rect.x, Rect.y, Rect.w, Rect.h);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
}

void CMenus::RenderSteamFriends(CUIRect MainView)
{
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	DrawMenuInset(&MainView, CUI::CORNER_ALL);
	MainView.Margin(8.0f, &MainView);
	if(!pPlatform || !pPlatform->Available())
	{
		UI()->DoLabelScaled(&MainView, Localize("Steam friends are unavailable in standalone mode."), 12.0f, 0);
		return;
	}

	static char s_aSearch[64] = "";
	static float s_SearchOffset = 0.0f;
	static unsigned long long s_SelectedUser = 0;
	static float s_Scroll = 0.0f;
	CUIRect Toolbar, Content, Actions, SearchLabel, SearchBox, OverlayButton;
	MainView.HSplitTop(30.0f, &Toolbar, &MainView);
	Toolbar.VSplitRight(150.0f, &Toolbar, &OverlayButton);
	Toolbar.VSplitLeft(54.0f, &SearchLabel, &Toolbar);
	Toolbar.VSplitLeft(min(260.0f, Toolbar.w), &SearchBox, &Toolbar);
	UI()->DoLabelScaled(&SearchLabel, Localize("Search"), 10.0f, -1);
	DoEditBox(&s_aSearch, &SearchBox, s_aSearch, sizeof(s_aSearch), 10.0f, &s_SearchOffset);
	static int s_OverlayInvite;
	if(DoButton_Menu(&s_OverlayInvite, Localize("Invite to party"), 0, &OverlayButton))
		pPlatform->OpenPartyInviteDialog();
	MainView.HSplitTop(6.0f, 0, &MainView);
	MainView.HSplitBottom(36.0f, &Content, &Actions);

	const int64 Now = time_get();
	if(Now >= m_SteamFriendCacheNextRefresh)
	{
		m_SteamFriendCacheCount = 0;
		for(int i = 0; i < pPlatform->FriendCount() && m_SteamFriendCacheCount < 512; i++)
			if(pPlatform->FriendInfo(i, &m_aSteamFriendCache[m_SteamFriendCacheCount]))
				m_SteamFriendCacheCount++;
		auto Rank = [](const CPlatformUserInfo &Info)
		{
			return Info.m_Joinable ? 0 : Info.m_PlayingThisGame ? 1 : Info.m_PersonaState != 0 ? 2 : 3;
		};
		for(int i = 1; i < m_SteamFriendCacheCount; i++)
		{
			CPlatformUserInfo Key = m_aSteamFriendCache[i];
			int j = i - 1;
			while(j >= 0 && (Rank(m_aSteamFriendCache[j]) > Rank(Key) ||
							 (Rank(m_aSteamFriendCache[j]) == Rank(Key) &&
							  str_comp_nocase(m_aSteamFriendCache[j].m_aName, Key.m_aName) > 0)))
			{
				m_aSteamFriendCache[j + 1] = m_aSteamFriendCache[j];
				j--;
			}
			m_aSteamFriendCache[j + 1] = Key;
		}
		m_SteamFriendCacheNextRefresh = Now + time_freq();
	}
	int aVisibleFriends[512], Count = 0;
	for(int i = 0; i < m_SteamFriendCacheCount; i++)
		if(!s_aSearch[0] || str_find_nocase(m_aSteamFriendCache[i].m_aName, s_aSearch))
			aVisibleFriends[Count++] = i;
	auto FriendAt = [&](int Index) -> CPlatformUserInfo &
	{
		return m_aSteamFriendCache[aVisibleFriends[Index]];
	};
	int Selected = -1;
	for(int i = 0; i < Count; i++)
		if(FriendAt(i).m_UserID == s_SelectedUser)
			Selected = i;
	if(Selected < 0 && Count)
	{
		Selected = 0;
		s_SelectedUser = FriendAt(0).m_UserID;
	}

	CUIRect List = Content, Detail;
	const bool Wide = Content.w > 650.0f;
	if(Wide)
	{
		Content.VSplitRight(250.0f, &List, &Detail);
		List.VSplitRight(8.0f, &List, 0);
	}
	static int s_ListID, s_aFriendIDs[512];
	for(int i = 0; i < 512; i++)
		s_aFriendIDs[i] = i;
	UiDoListboxStart(&s_ListID, &List, 44.0f, Localize("Steam friends"), "", Count, 1, Selected, s_Scroll);
	for(int i = 0; i < Count; i++)
	{
		CListboxItem Item = UiDoListboxNextItem(&s_aFriendIDs[i], i == Selected);
		if(!Item.m_Visible)
			continue;
		CUIRect Row = Item.m_Rect, Avatar, Text;
		Row.Margin(4.0f, &Row);
		Row.VSplitLeft(34.0f, &Avatar, &Text);
		Text.VSplitLeft(8.0f, 0, &Text);
		CPlatformUserInfo &Friend = FriendAt(i);
		DrawSteamAvatar(Avatar, Friend.m_UserID);
		const char *pState = Friend.m_PartyMember		  ? Localize("In your party")
							 : Friend.m_Joinable		  ? Localize("Joinable")
							 : Friend.m_PlayingThisGame	  ? Localize("Playing Ninslash")
							 : Friend.m_PersonaState != 0 ? Localize("Online")
														  : Localize("Offline");
		char aLine[256];
		str_format(aLine, sizeof(aLine), "%s\n%s", Friend.m_aName, pState);
		UI()->DoLabelScaled(&Text, aLine, 10.0f, -1);
	}
	const int NewSelected = UiDoListboxEnd(&s_Scroll, 0);
	if(NewSelected >= 0 && NewSelected < Count)
	{
		Selected = NewSelected;
		s_SelectedUser = FriendAt(Selected).m_UserID;
	}
	if(Wide)
	{
		DrawMenuInset(&Detail, CUI::CORNER_ALL);
		Detail.Margin(10.0f, &Detail);
		if(Selected >= 0)
		{
			CUIRect Avatar, Name;
			Detail.HSplitTop(64.0f, &Avatar, &Detail);
			Avatar.VSplitLeft(64.0f, &Avatar, &Name);
			Name.VSplitLeft(10.0f, 0, &Name);
			CPlatformUserInfo &Friend = FriendAt(Selected);
			DrawSteamAvatar(Avatar, Friend.m_UserID);
			char aText[320];
			str_format(aText,
					   sizeof(aText),
					   "%s\n%s\nSteamID %llu",
					   Friend.m_aName,
					   Localize(Friend.m_PartyMember		 ? "In your party"
								: Friend.m_Joinable			 ? "Joinable"
								: Friend.m_PlayingThisGame	 ? "Playing Ninslash"
								: Friend.m_PersonaState != 0 ? "Online"
															 : "Offline"),
					   Friend.m_UserID);
			UI()->DoLabelScaled(&Name, aText, 10.0f, -1);
		}
	}

	static int s_JoinFriend, s_InviteFriend, s_ProfileFriend;
	CUIRect Join, Invite, Profile;
	Actions.VSplitRight(110.0f, &Actions, &Join);
	Actions.VSplitRight(6.0f, &Actions, 0);
	Actions.VSplitRight(110.0f, &Actions, &Invite);
	Actions.VSplitRight(6.0f, &Actions, 0);
	Actions.VSplitRight(110.0f, &Actions, &Profile);
	if(Selected >= 0)
	{
		CPlatformUserInfo &Friend = FriendAt(Selected);
		if(DoButton_Menu(&s_JoinFriend, Localize("Join friend"), 0, &Join, BUTTONSTYLE_ACCENT) && Friend.m_Joinable)
			pPlatform->JoinUser(Friend.m_UserID);
		if(DoButton_Menu(&s_InviteFriend,
						 Localize(Friend.m_PartyMember ? "In party" : "Invite to party"),
						 Friend.m_PartyMember,
						 &Invite) &&
		   !Friend.m_PartyMember)
			pPlatform->InvitePartyUser(Friend.m_UserID);
		if(DoButton_Menu(&s_ProfileFriend, Localize("Profile"), 0, &Profile))
			pPlatform->OpenUserProfile(Friend.m_UserID);
	}
	char aCount[64];
	str_format(aCount, sizeof(aCount), "%d %s", Count, Localize("friends"));
	UI()->DoLabelScaled(&Actions, aCount, 10.0f, -1);
}

void CMenus::RenderPartyPanel(CUIRect *pMainView)
{
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	if(!pMainView || !pPlatform || !pPlatform->Available())
		return;
	CUIRect Panel;
	pMainView->HSplitTop(92.0f, &Panel, pMainView);
	pMainView->HSplitTop(6.0f, 0, pMainView);
	DrawMenuInset(&Panel, CUI::CORNER_ALL);
	Panel.Margin(7.0f, &Panel);
	CPlatformPartyState Party;
	const bool InParty = pPlatform->PartyState(&Party);
	CPlatformOperationStatus Status;
	pPlatform->PartyOperationStatus(&Status);
	CUIRect Header, Members, Actions;
	Panel.HSplitTop(20.0f, &Header, &Panel);
	Panel.HSplitBottom(25.0f, &Members, &Actions);
	static int s_CreateParty, s_LeaveParty, s_Ready, s_Launch;
	static bool s_ConfirmForce = false;
	if(!InParty)
	{
		CUIRect Button;
		Header.VSplitRight(126.0f, &Header, &Button);
		UI()->DoLabelScaled(
			&Header,
			Localize(Status.m_State == CLIENT_ASYNC_WORKING ? "Creating party..." : "Steam party · not in a party"),
			10.5f,
			-1);
		if(Status.m_State != CLIENT_ASYNC_WORKING &&
		   DoButton_Menu(&s_CreateParty, Localize("Create party"), 0, &Button, BUTTONSTYLE_ACCENT))
			pPlatform->CreateParty();
		if(Status.m_State == CLIENT_ASYNC_FAILED)
		{
			TextRender()->TextColor(ms_ColorDanger.r, ms_ColorDanger.g, ms_ColorDanger.b, 1.0f);
			UI()->DoLabelScaled(&Members, Localize(Status.m_aErrorKey), 9.0f, -1);
			TextRender()->TextColor(1, 1, 1, 1);
		}
		else
			UI()->DoLabelScaled(&Members, Localize("Create a private party or invite a friend to begin."), 9.5f, -1);
		return;
	}

	char aHeader[384];
	const char *pTarget = Party.m_TargetType == PLATFORM_PARTY_TARGET_GAME_LOBBY ? Localize("Steam room selected")
						  : Party.m_TargetType == PLATFORM_PARTY_TARGET_ADDRESS	 ? Party.m_aTargetAddress
																				 : Localize("Choose a game or server");
	str_format(aHeader,
			   sizeof(aHeader),
			   "%s · %d/16 · %s: %s",
			   Localize("Steam party"),
			   pPlatform->PartyMemberCount(),
			   Localize(Party.m_LocalOwner ? "Leader" : "Member"),
			   pTarget);
	UI()->DoLabelScaled(
		&Header, aHeader, FitScaledLabelFontSize(TextRender(), aHeader, 10.5f, Header.w, UI()->Scale()), -1);

	CPlatformUserInfo LocalMember;
	mem_zero(&LocalMember, sizeof(LocalMember));
	bool AllReady = Party.m_TargetType != PLATFORM_PARTY_TARGET_NONE;
	for(int i = 0; i < pPlatform->PartyMemberCount(); i++)
	{
		CPlatformUserInfo Member;
		if(!pPlatform->PartyMemberInfo(i, &Member))
			continue;
		if(Member.m_Local)
			LocalMember = Member;
		AllReady = AllReady && Member.m_PartyReady;
		CUIRect Cell, Avatar, Label;
		Members.VSplitLeft(min(110.0f, Members.w), &Cell, &Members);
		Cell.VSplitLeft(26.0f, &Avatar, &Label);
		Avatar.Margin(1.0f, &Avatar);
		DrawSteamAvatar(Avatar, Member.m_UserID);
		char aMember[160];
		str_format(aMember,
				   sizeof(aMember),
				   "%s%s\n%s",
				   Member.m_LobbyOwner ? "★ " : "",
				   Member.m_aName,
				   Member.m_PartyReady ? Localize("Ready") : Localize("Not ready"));
		UI()->DoLabelScaled(
			&Label, aMember, FitScaledLabelFontSize(TextRender(), aMember, 8.5f, Label.w, UI()->Scale()), -1);
		if(Members.w <= 1.0f)
			break;
	}

	CUIRect Leave, Ready, Launch;
	Actions.VSplitRight(100.0f, &Actions, &Leave);
	Leave.VSplitLeft(4.0f, 0, &Leave);
	if(DoButton_Menu(&s_LeaveParty, Localize("Leave party"), 0, &Leave, BUTTONSTYLE_DANGER))
	{
		pPlatform->LeaveParty();
		s_ConfirmForce = false;
		return;
	}
	if(Party.m_TargetType != PLATFORM_PARTY_TARGET_NONE)
	{
		Actions.VSplitRight(108.0f, &Actions, &Ready);
		Ready.VSplitLeft(4.0f, 0, &Ready);
		if(DoButton_Menu(&s_Ready,
						 Localize(LocalMember.m_PartyReady ? "Not ready" : "Ready"),
						 LocalMember.m_PartyReady,
						 &Ready,
						 LocalMember.m_PartyReady ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
			pPlatform->SetPartyReady(!LocalMember.m_PartyReady);
	}
	if(Party.m_LocalOwner && Party.m_TargetType != PLATFORM_PARTY_TARGET_NONE)
	{
		Actions.VSplitRight(142.0f, &Actions, &Launch);
		Launch.VSplitLeft(4.0f, 0, &Launch);
		const char *pLabel = AllReady ? "Start party" : s_ConfirmForce ? "Confirm force start" : "Force start";
		if(DoButton_Menu(&s_Launch, Localize(pLabel), 0, &Launch, AllReady ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_DANGER))
		{
			if(AllReady || s_ConfirmForce)
			{
				pPlatform->LaunchParty(!AllReady);
				s_ConfirmForce = false;
			}
			else
				s_ConfirmForce = true;
		}
	}
	else
		s_ConfirmForce = false;
	UI()->DoLabelScaled(
		&Actions,
		Localize(Party.m_LocalOwner ? "Select a room below, then wait for Ready." : "The leader selects the target."),
		9.0f,
		-1);
}

void CMenus::RenderPlay(CUIRect MainView)
{
	static int s_Filter = 0;
	static int s_Selected = -1;
	static float s_Scroll = 0.0f;
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();

	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(10.0f, &MainView);
	const CUIRect PageBounds = MainView;
	CUIRect Title, Tabs, Body;
	MainView.HSplitTop(36.0f, &Title, &MainView);
	UI()->DoLabelScaled(&Title, Localize("Play"), 22.0f, -1);
	MainView.HSplitTop(32.0f, &Tabs, &Body);
	const char *apTabs[] = {"Browse rooms", "Create room", "Steam friends"};
	static int s_aTabs[3];
	for(int i = 0; i < 3; i++)
	{
		CUIRect Button;
		Tabs.VSplitLeft(min(150.0f, Tabs.w / (3 - i)), &Button, &Tabs);
		if(DoButton_MenuTab(&s_aTabs[i],
							Localize(apTabs[i]),
							m_PlayTab == i,
							&Button,
							i == 0	 ? CUI::CORNER_TL
							: i == 2 ? CUI::CORNER_TR
									 : 0))
		{
			m_PlayTab = i;
			if(i == 0)
			{
				ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
				if(pPlatform && pPlatform->Available())
					pPlatform->RefreshLobbyList();
			}
		}
	}
	if(m_PlayTab != m_LastPlayTab)
	{
		m_LastPlayTab = m_PlayTab;
		m_PlayTabTransition = 0.0f;
	}
	const float TabDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_PlayTabTransition = SmoothToward(m_PlayTabTransition, 1.0f, TabDt, 15.0f);
	if(fabs(m_PlayTabTransition - 1.0f) < 0.001f)
		m_PlayTabTransition = 1.0f;
	const float TabEase = MenuEaseOutCubic(m_PlayTabTransition);
	const float TabOffset = (1.0f - TabEase) * 12.0f / max(1.0f, UI()->Scale());
	Body.x += TabOffset;
	Body.w -= TabOffset;
	Body.HSplitTop(8.0f, 0, &Body);
	RenderPartyPanel(&Body);
	if(m_PlayTab == 2)
	{
		RenderSteamFriends(Body);
		return;
	}

	if(m_PlayTab == 1)
	{
		RenderCreateRoom(Body);
		return;
	}

	UpdatePlaySnapshots();
	CUIRect StatusBar, Filters, List, Detail, Actions, Button;
	Body.HSplitTop(30.0f, &StatusBar, &Body);
	CClientAsyncStatus Connection;
	Client()->ConnectionStatus(&Connection);
	CPlatformOperationStatus LobbyStatus;
	mem_zero(&LobbyStatus, sizeof(LobbyStatus));
	if(pPlatform)
		pPlatform->LobbyOperationStatus(&LobbyStatus);
	if(LobbyStatus.m_State == CLIENT_ASYNC_WORKING && LobbyStatus.m_Stage == CLIENT_STAGE_JOINING_ROOM)
	{
		CUIRect Cancel;
		StatusBar.VSplitRight(90.0f, &StatusBar, &Cancel);
		UI()->DoLabelScaled(&StatusBar, Localize("Joining room"), 10.0f, -1);
		static int s_CancelJoin;
		if(DoButton_Menu(&s_CancelJoin, Localize("Cancel"), 0, &Cancel, BUTTONSTYLE_DANGER) || m_EscapePressed)
		{
			pPlatform->LeaveLobby();
			m_EscapePressed = false;
		}
	}
	else if(Connection.m_State == CLIENT_ASYNC_FAILED)
	{
		TextRender()->TextColor(ms_ColorDanger.r, ms_ColorDanger.g, ms_ColorDanger.b, 1.0f);
		UI()->DoLabelScaled(&StatusBar, Localize(Connection.m_aErrorKey), 10.0f, -1);
		TextRender()->TextColor(1, 1, 1, 1);
	}
	else if(ServerBrowser()->IsRefreshing())
	{
		char aStatus[96];
		str_format(aStatus, sizeof(aStatus), "%s  %d%%", Localize("Refreshing"), ServerBrowser()->LoadingProgression());
		UI()->DoLabelScaled(&StatusBar, aStatus, 10.0f, -1);
	}
	else if(!pPlatform || !pPlatform->Available())
		UI()->DoLabelScaled(
			&StatusBar,
			Localize("Steam unavailable — dedicated servers, Favorites and direct connection remain available."),
			10.0f,
			-1);
	else
		UI()->DoLabelScaled(&StatusBar, Localize("Dedicated servers, LAN, Favorites and Steam rooms"), 10.0f, -1);

	Body.HSplitTop(28.0f, &Filters, &Body);
	const char *apFilters[] = {"All", "Official", "Community", "Friends", "Modded", "Favorites", "LAN"};
	static int s_aFilters[7], s_FilterButton;
	CUIRect FilterTabs, FilterAnchor;
	Filters.VSplitRight(96.0f, &FilterTabs, &FilterAnchor);
	FilterTabs.VSplitRight(4.0f, &FilterTabs, 0);
	for(int i = 0; i < 7; i++)
	{
		const float AvailableTabWidth = FilterTabs.w / UI()->Scale() / (7 - i);
		FilterTabs.VSplitLeft(min(82.0f, AvailableTabWidth), &Button, &FilterTabs);
		if(DoButton_MenuTab(&s_aFilters[i], Localize(apFilters[i]), s_Filter == i, &Button, 0))
		{
			s_Filter = i;
			m_PlayBrowserCollection = i == 6   ? PLAY_COLLECTION_LAN
									  : i == 5 ? PLAY_COLLECTION_FAVORITES
											   : PLAY_COLLECTION_INTERNET;
			ServerBrowser()->Refresh(m_PlayBrowserCollection == PLAY_COLLECTION_LAN ? IServerBrowser::TYPE_LAN
									 : m_PlayBrowserCollection == PLAY_COLLECTION_FAVORITES
										 ? IServerBrowser::TYPE_FAVORITES
										 : IServerBrowser::TYPE_INTERNET);
			if(pPlatform && pPlatform->Available())
				pPlatform->RefreshLobbyList();
		}
		FilterTabs.VSplitLeft(3.0f, 0, &FilterTabs);
	}
	const int ActiveFilterCount =
		(g_Config.m_BrFilterEmpty != 0) + (g_Config.m_BrFilterSpectators != 0) + (g_Config.m_BrFilterFull != 0) +
		(g_Config.m_BrFilterFriends != 0) + (g_Config.m_BrFilterPw != 0) + (g_Config.m_BrFilterCompatversion != 0) +
		(g_Config.m_BrFilterPure != 0) + (g_Config.m_BrFilterPureMap != 0) + (g_Config.m_BrFilterGametypeStrict != 0) +
		(g_Config.m_BrFilterPing < 999) + (g_Config.m_BrFilterGametype[0] != 0) +
		(g_Config.m_BrFilterServerAddress[0] != 0) + (g_Config.m_BrFilterCountry != 0);
	char aFilterLabel[64];
	if(ActiveFilterCount > 0)
		str_format(aFilterLabel, sizeof(aFilterLabel), "%s  %d", Localize("Filter"), ActiveFilterCount);
	else
		str_copy(aFilterLabel, Localize("Filter"), sizeof(aFilterLabel));
	if(DoButton_Menu(&s_FilterButton,
					 aFilterLabel,
					 m_PlayFiltersOpen || ActiveFilterCount > 0,
					 &FilterAnchor,
					 ActiveFilterCount > 0 ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
	{
		m_PlayFiltersOpen = !m_PlayFiltersOpen;
		if(!m_PlayFiltersOpen)
		{
			m_FilterPresetMenuOpen = false;
			m_FilterPresetRenameSlot = -1;
		}
	}
	Body.HSplitTop(30.0f, &Filters, &Body);
	CUIRect SearchLabel, Search;
	Filters.VSplitLeft(52.0f, &SearchLabel, &Search);
	UI()->DoLabelScaled(&SearchLabel, Localize("Search"), 10.0f, -1);
	static float s_SearchOffset;
	if(DoEditBox(&g_Config.m_BrFilterString,
				 &Search,
				 g_Config.m_BrFilterString,
				 sizeof(g_Config.m_BrFilterString),
				 10.0f,
				 &s_SearchOffset))
		Client()->ServerBrowserUpdate();

	Body.HSplitTop(6.0f, 0, &Body);
	Body.HSplitBottom(72.0f, &Body, &Actions);
	if(m_PlayFiltersOpen != m_LastPlayFiltersOpen)
	{
		m_LastPlayFiltersOpen = m_PlayFiltersOpen;
		m_PlayFilterTransition = m_PlayFiltersOpen ? 0.0f : 1.0f;
	}
	if(m_PlayFiltersOpen)
		m_PlayFilterTransition = SmoothToward(m_PlayFilterTransition, 1.0f, TabDt, 18.0f);
	const float FilterEase = MenuEaseOutCubic(m_PlayFilterTransition);
	CUIRect FilterPopup;
	if(m_PlayFiltersOpen)
	{
		int VisiblePresetCount = 0;
		for(int Slot = 0; Slot < NUM_UI_FILTER_PRESETS; Slot++)
			if(Slot < UI_FILTER_PRESET_CUSTOM_START || m_aFilterPresets[Slot].m_Used)
				VisiblePresetCount++;
		const float PopupWidth = min(PageBounds.w, 420.0f * UI()->Scale());
		const float PresetMenuHeight = min(350.0f, 130.0f + VisiblePresetCount * 22.0f);
		const float DesiredHeight =
			(m_FilterPresetMenuOpen ? PresetMenuHeight : (m_PlayFiltersAdvanced ? 376.0f : 224.0f)) * UI()->Scale();
		FilterPopup.w = PopupWidth;
		FilterPopup.x =
			clamp(FilterAnchor.x + FilterAnchor.w - PopupWidth, PageBounds.x, PageBounds.x + PageBounds.w - PopupWidth);
		FilterPopup.y = Filters.y + Filters.h + 4.0f * UI()->Scale();
		FilterPopup.y -= (1.0f - FilterEase) * 10.0f / max(1.0f, UI()->Scale());
		FilterPopup.h =
			min(DesiredHeight,
				max(120.0f * UI()->Scale(), PageBounds.y + PageBounds.h - FilterPopup.y - 6.0f * UI()->Scale()));
		if(UI()->MouseButtonClicked(0) && !UI()->MouseInside(&FilterPopup) && !UI()->MouseInside(&FilterAnchor))
		{
			m_PlayFiltersOpen = false;
			m_FilterPresetMenuOpen = false;
			m_FilterPresetRenameSlot = -1;
			UI()->SetActiveItem(0);
		}
		else if(m_EscapePressed)
		{
			if(m_FilterPresetMenuOpen)
				m_FilterPresetMenuOpen = false;
			else if(m_FilterPresetRenameSlot >= 0)
				m_FilterPresetRenameSlot = -1;
			else
				m_PlayFiltersOpen = false;
			m_EscapePressed = false;
			UI()->SetActiveItem(0);
		}
	}
	const bool BlockPlayListInput = m_PlayFiltersOpen && UI()->MouseInside(&FilterPopup);
	const bool Compact = Body.w < 760.0f;
	if(!Compact)
	{
		Body.VSplitRight(max(220.0f, Body.w * 0.30f), &List, &Detail);
		List.VSplitRight(6.0f, &List, 0);
	}
	else
	{
		List = Body;
		Detail = CUIRect();
	}

	CPlayRoomEntry aEntries[512];
	int EntryCount = 0;
	auto AddServer = [&](const CPlayServerSnapshot *pServer)
	{
		if(!pServer || EntryCount >= 512)
			return;
		const bool Show = s_Filter == 0 || (s_Filter == 1 && pServer->m_Official) ||
						  (s_Filter == 2 && !pServer->m_Official) || (s_Filter == 4 && pServer->m_Modded) ||
						  (s_Filter == 5 && pServer->m_Favorite) ||
						  (s_Filter == 6 && pServer->m_Collection == PLAY_COLLECTION_LAN);
		if(!Show || (g_Config.m_BrFilterString[0] && !str_find_nocase(pServer->m_aName, g_Config.m_BrFilterString) &&
					 !str_find_nocase(pServer->m_aMap, g_Config.m_BrFilterString) &&
					 !str_find_nocase(pServer->m_aAddress, g_Config.m_BrFilterString)))
			return;
		for(int i = 0; i < EntryCount; i++)
			if(!str_comp(aEntries[i].m_aStableID, pServer->m_aAddress))
			{
				if(pServer->m_Favorite)
					aEntries[i].m_pServer = pServer;
				return;
			}
		CPlayRoomEntry &Entry = aEntries[EntryCount++];
		Entry.m_Source = CPlayRoomEntry::SOURCE_DEDICATED;
		Entry.m_pServer = pServer;
		Entry.m_pLobby = 0;
		str_copy(Entry.m_aStableID, pServer->m_aAddress, sizeof(Entry.m_aStableID));
	};
	for(int Collection = 0; Collection < NUM_PLAY_COLLECTIONS; Collection++)
		for(int i = 0; i < m_aPlayServerSnapshotCount[Collection]; i++)
			AddServer(&m_aaPlayServerSnapshots[Collection][i]);
	for(int i = 0; i < m_PlayLobbySnapshotCount && EntryCount < 512; i++)
	{
		const CPlatformLobbyInfo &Info = m_aPlayLobbySnapshots[i].m_Info;
		const bool Show = s_Filter == 0 || s_Filter == 2 || (s_Filter == 3 && Info.m_FriendHosted) ||
						  (s_Filter == 4 && Info.m_Modded);
		if(Show && (!g_Config.m_BrFilterString[0] || str_find_nocase(Info.m_aHostName, g_Config.m_BrFilterString) ||
					str_find_nocase(Info.m_aMap, g_Config.m_BrFilterString)))
		{
			CPlayRoomEntry &Entry = aEntries[EntryCount++];
			Entry.m_Source = CPlayRoomEntry::SOURCE_STEAM_LOBBY;
			Entry.m_pServer = 0;
			Entry.m_pLobby = &m_aPlayLobbySnapshots[i];
			str_format(Entry.m_aStableID, sizeof(Entry.m_aStableID), "lobby:%llu", Info.m_LobbyID);
		}
	}
	for(int i = 1; i < EntryCount; i++)
	{
		CPlayRoomEntry Key = aEntries[i];
		int j = i - 1;
		auto Compare = [&](const CPlayRoomEntry &A, const CPlayRoomEntry &B)
		{
			const char *pA =
				A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_aName : A.m_pLobby->m_Info.m_aHostName;
			const char *pB =
				B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_aName : B.m_pLobby->m_Info.m_aHostName;
			int Result = str_comp_nocase(pA, pB);
			if(g_Config.m_BrSort == IServerBrowser::SORT_MAP)
				Result = str_comp_nocase(
					A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_aMap : A.m_pLobby->m_Info.m_aMap,
					B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_aMap : B.m_pLobby->m_Info.m_aMap);
			else if(g_Config.m_BrSort == IServerBrowser::SORT_GAMETYPE)
				Result =
					str_comp_nocase(A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_aGameType
																				   : A.m_pLobby->m_Info.m_aGameType,
									B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_aGameType
																				   : B.m_pLobby->m_Info.m_aGameType);
			else if(g_Config.m_BrSort == IServerBrowser::SORT_NUMPLAYERS)
				Result = (A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_NumClients
																		 : A.m_pLobby->m_Info.m_Members) -
						 (B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_NumClients
																		 : B.m_pLobby->m_Info.m_Members);
			else if(g_Config.m_BrSort == IServerBrowser::SORT_PING)
			{
				const int PingA = A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_Latency : 10000;
				const int PingB = B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_Latency : 10000;
				Result = PingA - PingB;
			}
			return g_Config.m_BrSortOrder ? Result < 0 : Result > 0;
		};
		while(j >= 0 && Compare(aEntries[j], Key))
		{
			aEntries[j + 1] = aEntries[j];
			j--;
		}
		aEntries[j + 1] = Key;
	}
	auto SelectPlayEntry = [&](int Index)
	{
		if(Index < 0 || Index >= EntryCount)
			return;
		str_copy(m_aPlaySelectedID, aEntries[Index].m_aStableID, sizeof(m_aPlaySelectedID));
		if(aEntries[Index].m_pServer)
			str_copy(
				g_Config.m_UiServerAddress, aEntries[Index].m_pServer->m_aAddress, sizeof(g_Config.m_UiServerAddress));
		else
			g_Config.m_UiServerAddress[0] = 0;
	};
	if(!m_aPlaySelectedID[0] && EntryCount)
		SelectPlayEntry(0);
	s_Selected = -1;
	for(int i = 0; i < EntryCount; i++)
		if(!str_comp(m_aPlaySelectedID, aEntries[i].m_aStableID))
		{
			s_Selected = i;
			break;
		}
	if(s_Selected < 0 && EntryCount)
	{
		s_Selected = 0;
		SelectPlayEntry(s_Selected);
	}
	const int SelectionBeforeKeyboard = s_Selected;
	if(m_PlayListHasFocus && !m_PlayFiltersOpen)
		for(int i = 0; i < m_NumInputEvents; i++)
			if(m_aInputEvents[i].m_Flags & IInput::FLAG_PRESS)
			{
				const int Key = m_aInputEvents[i].m_Key;
				if((Key == KEY_UP || Key == KEY_GAMEPAD_BUTTON_DPAD_UP) && s_Selected > 0)
					s_Selected--;
				if((Key == KEY_DOWN || Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN) && s_Selected + 1 < EntryCount)
					s_Selected++;
			}
	if(s_Selected >= 0 && s_Selected != SelectionBeforeKeyboard)
		SelectPlayEntry(s_Selected);

	CUIRect Headers;
	List.HSplitTop(20.0f, &Headers, &List);
	DrawSectionHeader(&Headers, CUI::CORNER_T);
	struct CColumn
	{
		const char *m_pName;
		int m_Sort;
		float m_Width;
		CUIRect m_Rect;
	};
	CColumn aColumns[] = {{"Source", -1, Compact ? 66.0f : 76.0f, {}},
							  {"Name", IServerBrowser::SORT_NAME, 0.0f, {}},
							  {"Type", IServerBrowser::SORT_GAMETYPE, Compact ? 92.0f : 120.0f, {}},
							  {"Map", IServerBrowser::SORT_MAP, 0.0f, {}},
							  {"Players", IServerBrowser::SORT_NUMPLAYERS, 62.0f, {}},
							  {"Ping", IServerBrowser::SORT_PING, 56.0f, {}}};
	CUIRect ColumnArea = Headers;
	ColumnArea.VSplitRight(15.0f, &ColumnArea, 0);
	ColumnArea.VMargin(5.0f, &ColumnArea);
	CUIRect Remaining = ColumnArea;
	Remaining.VSplitLeft(aColumns[0].m_Width, &aColumns[0].m_Rect, &Remaining);
	Remaining.VSplitRight(aColumns[5].m_Width, &Remaining, &aColumns[5].m_Rect);
	Remaining.VSplitRight(aColumns[4].m_Width, &Remaining, &aColumns[4].m_Rect);
	Remaining.VSplitRight(aColumns[2].m_Width, &Remaining, &aColumns[2].m_Rect);
	aColumns[1].m_Rect = Remaining;
	for(int i = 0; i < 6; i++)
		if(aColumns[i].m_Rect.w > 0.0f &&
		   DoButton_GridHeader(&aColumns[i],
							   Localize(aColumns[i].m_pName),
							   g_Config.m_BrSort == aColumns[i].m_Sort,
							   &aColumns[i].m_Rect,
							   !BlockPlayListInput) &&
		   aColumns[i].m_Sort >= 0)
		{
			if(g_Config.m_BrSort == aColumns[i].m_Sort)
				g_Config.m_BrSortOrder ^= 1;
			else
			{
				g_Config.m_BrSort = aColumns[i].m_Sort;
				g_Config.m_BrSortOrder = 0;
			}
		}
	static int s_EntryListID;
	static int s_aEntryIDs[512];
	for(int i = 0; i < 512; i++)
		s_aEntryIDs[i] = i;
	if(!UI()->MouseButton(0) && !UI()->MouseButton(1))
	{
		for(int i = EntryCount; i < 512; i++)
		{
			if(UI()->ActiveItem() == &s_aEntryIDs[i])
			{
				UI()->SetActiveItem(0);
				break;
			}
		}
	}
	UiDoListboxStart(&s_EntryListID, &List, 30.0f, Localize("Rooms"), "", EntryCount, 1, s_Selected, s_Scroll);
	for(int i = 0; i < EntryCount; i++)
	{
		CListboxItem Item = UiDoListboxNextItem(&s_aEntryIDs[i], s_Selected == i, !BlockPlayListInput);
		if(!Item.m_Visible)
			continue;
		CUIRect Row = Item.m_Rect;
		Row.HMargin(4.0f, &Row);
		for(int Column = 0; Column < 6; Column++)
		{
			if(aColumns[Column].m_Rect.w <= 0.0f)
				continue;
			CUIRect Cell = aColumns[Column].m_Rect;
			Cell.x = Row.x + (Cell.x - ColumnArea.x);
			Cell.y = Row.y;
			Cell.h = Row.h;
			Cell.VMargin(4.0f, &Cell);
			const CPlayRoomEntry &Entry = aEntries[i];
			const CPlayServerSnapshot *pServer = Entry.m_pServer;
			const CPlatformLobbyInfo *pLobby = Entry.m_pLobby ? &Entry.m_pLobby->m_Info : 0;
			char aValue[128];
			if(Column == 0)
				str_copy(aValue,
						 Entry.m_Source == CPlayRoomEntry::SOURCE_STEAM_LOBBY
							 ? (pLobby->m_FriendHosted ? Localize("FRIEND") : "STEAM")
						 : pServer->m_Collection == PLAY_COLLECTION_LAN ? "LAN"
						 : pServer->m_Official							? Localize("OFFICIAL")
																		: Localize("COMMUNITY"),
						 sizeof(aValue));
			else if(Column == 1)
				str_copy(aValue, pServer ? pServer->m_aName : pLobby->m_aHostName, sizeof(aValue));
			else if(Column == 2)
				str_copy(aValue, DisplayGameType(pServer ? pServer->m_aGameType : pLobby->m_aGameType), sizeof(aValue));
			else if(Column == 4)
				str_format(aValue,
						   sizeof(aValue),
						   "%d/%d",
						   pServer ? pServer->m_NumClients : pLobby->m_Members,
						   pServer ? pServer->m_MaxClients : pLobby->m_MaxMembers);
			else
				str_copy(aValue, pServer ? (pServer->m_Latency >= 0 ? "" : "-") : "RELAY", sizeof(aValue));
			if(Column == 5 && pServer && pServer->m_Latency >= 0)
				str_format(aValue, sizeof(aValue), "%dms", pServer->m_Latency);
			UI()->DoLabelScaled(
				&Cell, aValue, FitScaledLabelFontSize(TextRender(), aValue, 10.0f, Cell.w, UI()->Scale()), -1);
		}
	}
	const int NewSelection = UiDoListboxEnd(&s_Scroll, 0);
	if(NewSelection >= 0 && NewSelection < EntryCount)
	{
		if(NewSelection != s_Selected)
		{
			s_Selected = NewSelection;
			SelectPlayEntry(s_Selected);
		}
		m_PlayListHasFocus = true;
	}

	auto RenderDetail = [&](CUIRect View)
	{
		DrawMenuInset(&View, CUI::CORNER_ALL);
		View.Margin(10.0f, &View);
		if(s_Selected < 0)
			UI()->DoLabelScaled(&View, Localize("No servers found"), 11.0f, -1);
		else
		{
			const CPlayRoomEntry &Entry = aEntries[s_Selected];
			char aDetail[768];
			if(Entry.m_pServer)
			{
				const CPlayServerSnapshot *pInfo = Entry.m_pServer;
				str_format(aDetail,
						   sizeof(aDetail),
						   "%s\n\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s",
						   pInfo->m_aName,
						   Localize("Game type"),
						   DisplayGameType(pInfo->m_aGameType),
						   Localize("Address"),
						   pInfo->m_aAddress,
						   Localize("Version"),
						   pInfo->m_aVersion,
						   Localize("Source"),
						   pInfo->m_Collection == PLAY_COLLECTION_LAN					 ? "LAN"
						   : pInfo->m_DiscoverySources & IServerBrowser::DISCOVERY_STEAM ? "Steam GameServer + UDP"
																						 : "UDP",
						   Localize("Mods"),
						   pInfo->m_Modded ? Localize("Required") : Localize("None"),
						   Localize("Authentication"),
						   pInfo->m_AuthPolicy ? Localize("Required") : Localize("Open"));
			}
			else
			{
				const CPlatformLobbyInfo &Info = Entry.m_pLobby->m_Info;
				str_format(
					aDetail,
					sizeof(aDetail),
					"%s\n\n%s: %s\nLobbyID: %llu\n%s: %llu\n%s: %s\n%s: %s\nMod hash: %s\nRelay / Steam authentication",
					Info.m_aHostName,
					Localize("Game type"),
					DisplayGameType(Info.m_aGameType),
					Info.m_LobbyID,
					Localize("Host"),
					Info.m_HostSteamID,
					Localize("Region"),
					Info.m_aRegion,
					Localize("Source"),
					Info.m_FriendHosted ? Localize("Friend room") : Localize("Steam room"),
					Info.m_aModHash);
			}
			UI()->DoLabelScaled(&View, aDetail, 10.5f, -1);
		}
	};
	if(!Compact)
		RenderDetail(Detail);
	Actions.HSplitTop(6.0f, 0, &Actions);
	CUIRect Direct, ActionButtons;
	Actions.HSplitTop(30.0f, &Direct, &ActionButtons);
	CUIRect DirectLabel, DirectBox;
	Direct.VSplitLeft(76.0f, &DirectLabel, &DirectBox);
	UI()->DoLabelScaled(&DirectLabel, Localize("Host address"), 10.0f, -1);
	static float s_DirectOffset;
	if(!(BlockPlayListInput && UI()->MouseInside(&DirectBox)))
		DoEditBox(&g_Config.m_UiServerAddress,
				  &DirectBox,
				  g_Config.m_UiServerAddress,
				  sizeof(g_Config.m_UiServerAddress),
				  10.0f,
				  &s_DirectOffset);
	static int s_Join, s_Refresh, s_Copy, s_Favorite, s_Details;
	CUIRect JoinButton, RefreshButton, CopyButton, FavoriteButton, DetailButton;
	ActionButtons.VSplitRight(100.0f, &ActionButtons, &JoinButton);
	ActionButtons.VSplitRight(4.0f, &ActionButtons, 0);
	ActionButtons.VSplitRight(80.0f, &ActionButtons, &RefreshButton);
	ActionButtons.VSplitRight(4.0f, &ActionButtons, 0);
	ActionButtons.VSplitRight(70.0f, &ActionButtons, &CopyButton);
	ActionButtons.VSplitRight(4.0f, &ActionButtons, 0);
	ActionButtons.VSplitRight(92.0f, &ActionButtons, &FavoriteButton);
	if(Compact)
	{
		ActionButtons.VSplitRight(4.0f, &ActionButtons, 0);
		ActionButtons.VSplitRight(74.0f, &ActionButtons, &DetailButton);
	}
	const bool HasDedicated = s_Selected >= 0 && aEntries[s_Selected].m_pServer;
	const bool JoinBlocked = BlockPlayListInput && UI()->MouseInside(&JoinButton);
	const bool RefreshBlocked = BlockPlayListInput && UI()->MouseInside(&RefreshButton);
	const bool CopyBlocked = BlockPlayListInput && UI()->MouseInside(&CopyButton);
	const bool FavoriteBlocked = BlockPlayListInput && UI()->MouseInside(&FavoriteButton);
	const bool DetailBlocked = BlockPlayListInput && UI()->MouseInside(&DetailButton);
	if((!JoinBlocked && DoButton_Menu(&s_Join, Localize("Join"), 0, &JoinButton, BUTTONSTYLE_ACCENT)) ||
	   (!m_PlayFiltersOpen && m_PlayListHasFocus && m_EnterPressed && s_Selected >= 0))
	{
		CPlatformPartyState Party;
		const bool InParty = pPlatform && pPlatform->PartyState(&Party);
		if(InParty)
		{
			if(Party.m_LocalOwner)
			{
				bool TargetUpdated = false;
				if(s_Selected >= 0)
				{
					const CPlayRoomEntry &Entry = aEntries[s_Selected];
					if(Entry.m_pServer)
						TargetUpdated = pPlatform->SetPartyTarget(
							PLATFORM_PARTY_TARGET_ADDRESS,
							0,
							Entry.m_pServer->m_aAddress,
							Entry.m_pServer->m_Modded ? (g_Config.m_ClModHash[0] ? g_Config.m_ClModHash : "none")
													  : "none");
					else
						TargetUpdated = pPlatform->SetPartyTarget(PLATFORM_PARTY_TARGET_GAME_LOBBY,
																  Entry.m_pLobby->m_Info.m_LobbyID,
																  "",
																  Entry.m_pLobby->m_Info.m_aModHash);
				}
				else if(g_Config.m_UiServerAddress[0])
					TargetUpdated = pPlatform->SetPartyTarget(PLATFORM_PARTY_TARGET_ADDRESS,
															  0,
															  g_Config.m_UiServerAddress,
															  g_Config.m_ClModHash[0] ? g_Config.m_ClModHash : "none");
				if(TargetUpdated)
					pPlatform->SetPartyReady(true);
				else
					PopupMessage(Localize("Steam party"),
								 Localize("Unable to update Steam party target. Retry or recreate the party."),
								 Localize("OK"));
			}
		}
		else if(s_Selected >= 0)
		{
			const CPlayRoomEntry &Entry = aEntries[s_Selected];
			if(Entry.m_pServer)
				Client()->Connect(Entry.m_pServer->m_aAddress);
			else if(pPlatform)
				pPlatform->JoinLobby(Entry.m_pLobby->m_Info.m_LobbyID);
		}
		else if(g_Config.m_UiServerAddress[0])
			Client()->Connect(g_Config.m_UiServerAddress);
		m_EnterPressed = false;
	}
	if(!RefreshBlocked && DoButton_Menu(&s_Refresh, Localize("Refresh"), 0, &RefreshButton))
	{
		ServerBrowser()->Refresh(m_PlayBrowserCollection == PLAY_COLLECTION_LAN ? IServerBrowser::TYPE_LAN
								 : m_PlayBrowserCollection == PLAY_COLLECTION_FAVORITES
									 ? IServerBrowser::TYPE_FAVORITES
									 : IServerBrowser::TYPE_INTERNET);
		if(pPlatform && pPlatform->Available())
			pPlatform->RefreshLobbyList();
	}
	if(!CopyBlocked && DoButton_Menu(&s_Copy, Localize("Copy"), 0, &CopyButton) && s_Selected >= 0)
		Input()->SetClipboardText(HasDedicated ? aEntries[s_Selected].m_pServer->m_aAddress
											   : aEntries[s_Selected].m_aStableID);
	if(HasDedicated && !FavoriteBlocked &&
	   DoButton_Menu(&s_Favorite,
					 Localize(aEntries[s_Selected].m_pServer->m_Favorite ? "Unfavorite" : "Favorite"),
					 0,
					 &FavoriteButton))
	{
		const CPlayServerSnapshot *pInfo = aEntries[s_Selected].m_pServer;
		if(pInfo->m_Favorite)
			ServerBrowser()->RemoveFavorite(pInfo->m_NetAddr);
		else
			ServerBrowser()->AddFavorite(pInfo->m_NetAddr);
	}
	if(Compact && !DetailBlocked && DoButton_Menu(&s_Details, Localize("Details"), m_PlayDetailOpen, &DetailButton))
		m_PlayDetailOpen = !m_PlayDetailOpen;
	if(Compact && m_PlayDetailOpen)
	{
		CUIRect Overlay = Body;
		Overlay.Margin(8.0f, &Overlay);
		RenderDetail(Overlay);
		if(m_EscapePressed)
		{
			m_PlayDetailOpen = false;
			m_EscapePressed = false;
		}
	}

	if(m_PlayFiltersOpen)
	{
		DrawMenuBorder(&FilterPopup,
					   vec4(0.035f, 0.043f, 0.052f, 0.99f),
					   vec4(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, 0.72f),
					   CUI::CORNER_ALL,
					   ms_PanelRounding);
		CUIRect PopupContent = FilterPopup;
		PopupContent.Margin(8.0f, &PopupContent);

		CUIRect PopupHeader, PresetRow, FilterContent, Footer;
		PopupContent.HSplitTop(24.0f, &PopupHeader, &PopupContent);
		CUIRect CloseButton;
		PopupHeader.VSplitRight(24.0f, &PopupHeader, &CloseButton);
		UI()->DoLabelScaled(&PopupHeader, Localize("Server filter"), 13.0f, -1);
		static int s_CloseFilters;
		if(DoButton_Menu(&s_CloseFilters, "x", 0, &CloseButton))
		{
			m_PlayFiltersOpen = false;
			m_FilterPresetMenuOpen = false;
			m_FilterPresetRenameSlot = -1;
			UI()->SetActiveItem(0);
		}

		PopupContent.HSplitTop(4.0f, 0, &PopupContent);
		PopupContent.HSplitTop(26.0f, &PresetRow, &PopupContent);
		const bool CustomPreset =
			m_ActiveFilterPreset >= UI_FILTER_PRESET_CUSTOM_START && m_aFilterPresets[m_ActiveFilterPreset].m_Used;
		if(m_FilterPresetRenameSlot >= UI_FILTER_PRESET_CUSTOM_START)
		{
			CUIRect RenameBox, SaveButton, CancelButton;
			PresetRow.VSplitRight(108.0f, &RenameBox, &SaveButton);
			SaveButton.VSplitLeft(52.0f, &SaveButton, &CancelButton);
			CancelButton.VSplitLeft(4.0f, 0, &CancelButton);
			static float s_RenameOffset;
			DoEditBox(&m_aFilterPresetRenameBuf,
					  &RenameBox,
					  m_aFilterPresetRenameBuf,
					  sizeof(m_aFilterPresetRenameBuf),
					  10.0f,
					  &s_RenameOffset);
			static int s_SavePresetName, s_CancelPresetName;
			if((DoButton_Menu(&s_SavePresetName, Localize("Save"), 0, &SaveButton, BUTTONSTYLE_ACCENT) ||
				m_EnterPressed) &&
			   m_aFilterPresetRenameBuf[0])
			{
				str_copy(m_aFilterPresets[m_FilterPresetRenameSlot].m_aName,
						 m_aFilterPresetRenameBuf,
						 sizeof(m_aFilterPresets[m_FilterPresetRenameSlot].m_aName));
				m_FilterPresetRenameSlot = -1;
				m_EnterPressed = false;
				SaveFilterPresets();
			}
			if(DoButton_Menu(&s_CancelPresetName, Localize("Cancel"), 0, &CancelButton))
				m_FilterPresetRenameSlot = -1;
		}
		else
		{
			const char *pPresetName = m_aFilterPresets[m_ActiveFilterPreset].m_aName;
			if(m_ActiveFilterPreset == UI_FILTER_PRESET_ALL)
				pPresetName = Localize("All");
			else if(m_ActiveFilterPreset == UI_FILTER_PRESET_FAVORITES)
				pPresetName = Localize("Favorites");
			char aPresetLabel[96];
			str_format(aPresetLabel, sizeof(aPresetLabel), "%s:  %s", Localize("Preset"), pPresetName);
			static int s_PresetSelector;
			if(DoButton_Menu(&s_PresetSelector,
							 aPresetLabel,
							 m_FilterPresetMenuOpen,
							 &PresetRow,
							 m_FilterPresetMenuOpen ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
				m_FilterPresetMenuOpen = !m_FilterPresetMenuOpen;
		}

		PopupContent.HSplitTop(6.0f, 0, &PopupContent);
		PopupContent.HSplitBottom(24.0f, &FilterContent, &Footer);
		if(m_FilterPresetMenuOpen)
		{
			CUIRect PresetList, PresetActions;
			FilterContent.HSplitBottom(30.0f, &PresetList, &PresetActions);
			PresetActions.HSplitTop(6.0f, 0, &PresetActions);
			static CScrollRegion s_PresetScrollRegion;
			CScrollRegionParams ScrollParams;
			ConfigureScrollRegion(&ScrollParams);
			ScrollParams.m_ClipBgColor = vec4(0, 0, 0, 0);
			vec2 ScrollOffset;
			s_PresetScrollRegion.Begin(&PresetList, &ScrollOffset, &ScrollParams);
			CUIRect PresetRows = PresetList;
			PresetRows.y += ScrollOffset.y;
			static int s_aPresetIds[NUM_UI_FILTER_PRESETS];
			for(int Slot = 0; Slot < NUM_UI_FILTER_PRESETS; Slot++)
			{
				if(Slot >= UI_FILTER_PRESET_CUSTOM_START && !m_aFilterPresets[Slot].m_Used)
					continue;
				CUIRect Row;
				PresetRows.HSplitTop(22.0f, &Row, &PresetRows);
				const char *pName = m_aFilterPresets[Slot].m_aName;
				if(Slot == UI_FILTER_PRESET_ALL)
					pName = Localize("All");
				else if(Slot == UI_FILTER_PRESET_FAVORITES)
					pName = Localize("Favorites");
				if(!s_PresetScrollRegion.IsRectClipped(Row) &&
				   DoButton_Menu(&s_aPresetIds[Slot],
								 pName,
								 Slot == m_ActiveFilterPreset,
								 &Row,
								 Slot == m_ActiveFilterPreset ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
				{
					SwitchFilterPreset(Slot);
					m_FilterPresetMenuOpen = false;
				}
				s_PresetScrollRegion.AddRect(Row);
			}
			s_PresetScrollRegion.End();

			CUIRect NewButton, RenameButton, DeleteButton;
			PresetActions.VSplitLeft((PresetActions.w - 8.0f * UI()->Scale()) / 3.0f, &NewButton, &PresetActions);
			PresetActions.VSplitLeft(4.0f, 0, &PresetActions);
			PresetActions.VSplitLeft((PresetActions.w - 4.0f * UI()->Scale()) / 2.0f, &RenameButton, &DeleteButton);
			DeleteButton.VSplitLeft(4.0f, 0, &DeleteButton);
			static int s_NewPreset, s_RenamePreset, s_DeletePreset;
			if(DoButton_Menu(&s_NewPreset, Localize("New"), 0, &NewButton, BUTTONSTYLE_ACCENT))
			{
				int Slot = -1;
				for(int i = UI_FILTER_PRESET_CUSTOM_START; i < NUM_UI_FILTER_PRESETS; i++)
					if(!m_aFilterPresets[i].m_Used)
					{
						Slot = i;
						break;
					}
				if(Slot >= 0)
				{
					m_aFilterPresets[Slot].m_Used = true;
					str_format(m_aFilterPresets[Slot].m_aName,
							   sizeof(m_aFilterPresets[Slot].m_aName),
							   "%s %d",
							   Localize("Preset"),
							   Slot - UI_FILTER_PRESET_CUSTOM_START + 1);
					SnapshotConfigToFilterPreset(Slot);
					SwitchFilterPreset(Slot);
					m_FilterPresetRenameSlot = Slot;
					str_copy(
						m_aFilterPresetRenameBuf, m_aFilterPresets[Slot].m_aName, sizeof(m_aFilterPresetRenameBuf));
					m_FilterPresetMenuOpen = false;
				}
			}
			if(DoButton_Menu(&s_RenamePreset, Localize("Rename"), 0, &RenameButton) && CustomPreset)
			{
				m_FilterPresetRenameSlot = m_ActiveFilterPreset;
				str_copy(m_aFilterPresetRenameBuf,
						 m_aFilterPresets[m_ActiveFilterPreset].m_aName,
						 sizeof(m_aFilterPresetRenameBuf));
				m_FilterPresetMenuOpen = false;
			}
			if(DoButton_Menu(&s_DeletePreset,
							 Localize("Delete"),
							 0,
							 &DeleteButton,
							 CustomPreset ? BUTTONSTYLE_DANGER : BUTTONSTYLE_NORMAL) &&
			   CustomPreset)
			{
				m_aFilterPresets[m_ActiveFilterPreset].m_Used = false;
				SwitchFilterPreset(UI_FILTER_PRESET_ALL);
				m_FilterPresetMenuOpen = false;
				SaveFilterPresets();
			}
		}
		else
		{
			static CScrollRegion s_PlayFilterScrollRegion;
			CScrollRegionParams ScrollParams;
			ConfigureScrollRegion(&ScrollParams);
			ScrollParams.m_ClipBgColor = vec4(0, 0, 0, 0);
			vec2 ScrollOffset;
			s_PlayFilterScrollRegion.Begin(&FilterContent, &ScrollOffset, &ScrollParams);
			CUIRect Content = FilterContent;
			Content.y += ScrollOffset.y;
			Content.VSplitRight(8.0f, &Content, 0);

			CUIRect SectionLabel;
			Content.HSplitTop(16.0f, &SectionLabel, &Content);
			TextRender()->TextColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, 1.0f);
			if(!s_PlayFilterScrollRegion.IsRectClipped(SectionLabel))
				UI()->DoLabelScaled(&SectionLabel, Localize("Common filters"), 9.0f, -1);
			TextRender()->TextColor(1, 1, 1, 1);
			s_PlayFilterScrollRegion.AddRect(SectionLabel);

			auto RenderToggleRow = [&](int &LeftValue, const char *pLeftText, int &RightValue, const char *pRightText)
			{
				CUIRect Row, Left, Right;
				Content.HSplitTop(22.0f, &Row, &Content);
				Row.VSplitMid(&Left, &Right);
				Right.VSplitLeft(4.0f, 0, &Right);
				if(!s_PlayFilterScrollRegion.IsRectClipped(Left) &&
				   DoButton_CheckBox(&LeftValue, Localize(pLeftText), LeftValue, &Left))
					LeftValue ^= 1;
				if(!s_PlayFilterScrollRegion.IsRectClipped(Right) &&
				   DoButton_CheckBox(&RightValue, Localize(pRightText), RightValue, &Right))
					RightValue ^= 1;
				s_PlayFilterScrollRegion.AddRect(Row);
			};
			RenderToggleRow(g_Config.m_BrFilterEmpty, "Has people playing", g_Config.m_BrFilterFull, "Server not full");
			RenderToggleRow(g_Config.m_BrFilterPw, "No password", g_Config.m_BrFilterFriends, "Show friends only");

			Content.HSplitTop(4.0f, 0, &Content);
			CUIRect PingRow, PingLabel, PingBox;
			Content.HSplitTop(24.0f, &PingRow, &Content);
			PingRow.VSplitRight(76.0f, &PingLabel, &PingBox);
			if(!s_PlayFilterScrollRegion.IsRectClipped(PingLabel))
				UI()->DoLabelScaled(&PingLabel, Localize("Maximum ping:"), 10.0f, -1);
			char aPing[5];
			str_format(aPing, sizeof(aPing), "%d", g_Config.m_BrFilterPing);
			static float s_PingOffset;
			if(!s_PlayFilterScrollRegion.IsRectClipped(PingBox))
			{
				DoEditBox(&g_Config.m_BrFilterPing, &PingBox, aPing, sizeof(aPing), 10.0f, &s_PingOffset);
				UI()->ClipEnable(&FilterContent);
			}
			g_Config.m_BrFilterPing = clamp(str_toint(aPing), 0, 999);
			s_PlayFilterScrollRegion.AddRect(PingRow);

			Content.HSplitTop(6.0f, 0, &Content);
			CUIRect AdvancedButton;
			Content.HSplitTop(24.0f, &AdvancedButton, &Content);
			static int s_AdvancedFilters;
			if(!s_PlayFilterScrollRegion.IsRectClipped(AdvancedButton) &&
			   DoButton_Menu(&s_AdvancedFilters,
							 Localize("Advanced filters"),
							 m_PlayFiltersAdvanced,
							 &AdvancedButton,
							 m_PlayFiltersAdvanced ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
				m_PlayFiltersAdvanced = !m_PlayFiltersAdvanced;
			s_PlayFilterScrollRegion.AddRect(AdvancedButton);

			if(m_PlayFiltersAdvanced)
			{
				Content.HSplitTop(4.0f, 0, &Content);
				RenderToggleRow(g_Config.m_BrFilterSpectators,
								"Count players only",
								g_Config.m_BrFilterCompatversion,
								"Compatible version");
				RenderToggleRow(
					g_Config.m_BrFilterPure, "Standard gametype", g_Config.m_BrFilterPureMap, "Standard map");
				CUIRect StrictRow;
				Content.HSplitTop(22.0f, &StrictRow, &Content);
				if(!s_PlayFilterScrollRegion.IsRectClipped(StrictRow) &&
				   DoButton_CheckBox(&g_Config.m_BrFilterGametypeStrict,
									 Localize("Strict gametype filter"),
									 g_Config.m_BrFilterGametypeStrict,
									 &StrictRow))
					g_Config.m_BrFilterGametypeStrict ^= 1;
				s_PlayFilterScrollRegion.AddRect(StrictRow);

				auto RenderTextFilter =
					[&](void *pID, const char *pLabel, char *pBuffer, unsigned BufferSize, float *pOffset)
				{
					CUIRect Row, Label, Edit;
					Content.HSplitTop(3.0f, 0, &Content);
					Content.HSplitTop(24.0f, &Row, &Content);
					Row.VSplitLeft(112.0f, &Label, &Edit);
					if(!s_PlayFilterScrollRegion.IsRectClipped(Row))
					{
						UI()->DoLabelScaled(&Label, Localize(pLabel), 10.0f, -1);
						if(DoEditBox(pID, &Edit, pBuffer, BufferSize, 10.0f, pOffset))
							Client()->ServerBrowserUpdate();
						UI()->ClipEnable(&FilterContent);
					}
					s_PlayFilterScrollRegion.AddRect(Row);
				};
				static float s_GameTypeOffset, s_AddressOffset;
				RenderTextFilter(&g_Config.m_BrFilterGametype,
								 "Game types:",
								 g_Config.m_BrFilterGametype,
								 sizeof(g_Config.m_BrFilterGametype),
								 &s_GameTypeOffset);
				RenderTextFilter(&g_Config.m_BrFilterServerAddress,
								 "Server address:",
								 g_Config.m_BrFilterServerAddress,
								 sizeof(g_Config.m_BrFilterServerAddress),
								 &s_AddressOffset);

				CUIRect CountryRow, CountryLabel, Flag;
				Content.HSplitTop(3.0f, 0, &Content);
				Content.HSplitTop(24.0f, &CountryRow, &Content);
				CountryRow.VSplitRight(54.0f, &CountryLabel, &Flag);
				const bool CountryVisible = !s_PlayFilterScrollRegion.IsRectClipped(CountryRow);
				if(CountryVisible && DoButton_CheckBox(&g_Config.m_BrFilterCountry,
													   Localize("Player country:"),
													   g_Config.m_BrFilterCountry,
													   &CountryLabel))
					g_Config.m_BrFilterCountry ^= 1;
				CUIRect FlagImage = Flag;
				FlagImage.Margin(3.0f, &FlagImage);
				const float OldWidth = FlagImage.w;
				FlagImage.w = FlagImage.h * 2.0f;
				FlagImage.x += (OldWidth - FlagImage.w) * 0.5f;
				vec4 FlagColor(1, 1, 1, g_Config.m_BrFilterCountry ? 1.0f : 0.45f);
				if(CountryVisible)
					m_pClient->m_pCountryFlags->Render(g_Config.m_BrFilterCountryIndex,
													   &FlagColor,
													   FlagImage.x,
													   FlagImage.y,
													   FlagImage.w,
													   FlagImage.h);
				if(CountryVisible && g_Config.m_BrFilterCountry &&
				   UI()->DoButtonLogic(&g_Config.m_BrFilterCountryIndex, "", 0, &Flag))
					m_Popup = POPUP_COUNTRY;
				s_PlayFilterScrollRegion.AddRect(CountryRow);
			}
			s_PlayFilterScrollRegion.End();
		}

		CUIRect ResetButton, DoneButton;
		Footer.VSplitLeft((Footer.w - 4.0f * UI()->Scale()) * 0.5f, &ResetButton, &DoneButton);
		DoneButton.VSplitLeft(4.0f, 0, &DoneButton);
		static int s_ResetFilters, s_DoneFilters;
		if(DoButton_Menu(&s_ResetFilters, Localize("Reset filter"), 0, &ResetButton))
		{
			g_Config.m_BrFilterString[0] = 0;
			g_Config.m_BrFilterFull = 0;
			g_Config.m_BrFilterEmpty = 0;
			g_Config.m_BrFilterSpectators = 0;
			g_Config.m_BrFilterFriends = 0;
			g_Config.m_BrFilterCountry = 0;
			g_Config.m_BrFilterCountryIndex = -1;
			g_Config.m_BrFilterPw = 0;
			g_Config.m_BrFilterPing = 999;
			g_Config.m_BrFilterGametype[0] = 0;
			g_Config.m_BrFilterGametypeStrict = 0;
			g_Config.m_BrFilterServerAddress[0] = 0;
			g_Config.m_BrFilterPure = 0;
			g_Config.m_BrFilterPureMap = 0;
			g_Config.m_BrFilterCompatversion = 0;
			m_ActiveFilterPreset = UI_FILTER_PRESET_ALL;
			Client()->ServerBrowserUpdate();
			SaveFilterPresets();
		}
		if(DoButton_Menu(&s_DoneFilters, Localize("Done"), 0, &DoneButton, BUTTONSTYLE_ACCENT))
		{
			m_PlayFiltersOpen = false;
			m_FilterPresetMenuOpen = false;
			m_FilterPresetRenameSlot = -1;
			UI()->SetActiveItem(0);
		}
	}
}

void CMenus::RenderMods(CUIRect MainView)
{
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(10.0f, &MainView);
	CUIRect Title, ViewTabs, SearchRow, CategoryRow, StatusRow, Body, Footer, Button;
	MainView.HSplitTop(36.0f, &Title, &MainView);
	const bool OnlineWorkshop = pPlatform && pPlatform->Available();
	const bool LocalLibrary = pPlatform && str_comp(pPlatform->PlatformName(), "standalone") == 0;
	const bool LocalImportAvailable = LocalLibrary || OnlineWorkshop;
	UI()->DoLabelScaled(&Title, Localize(LocalLibrary ? "Local Mods" : "Workshop"), 22.0f, -1);
	if(!pPlatform)
	{
		UI()->DoLabelScaled(&MainView, Localize("Mod manager is unavailable."), 12.0f, -1);
		return;
	}
	if(!OnlineWorkshop && !LocalLibrary)
	{
		UI()->DoLabelScaled(
			&MainView, Localize("Steam Workshop is unavailable. Installed game data remains unchanged."), 12.0f, -1);
		return;
	}
	CPlatformLocalImportResult ImportResult;
	while(LocalImportAvailable && pPlatform->ConsumeLocalContentImportResult(&ImportResult))
	{
		unsigned long long ImportID = 0;
		sscanf(ImportResult.m_aPublishedFileID, "%llu", &ImportID);
		if(ImportResult.m_State == PLATFORM_LOCAL_IMPORT_FAILED)
			PopupMessage(Localize("Unable to import Mod"), ImportResult.m_aError, Localize("OK"));
		else if(ImportResult.m_State == PLATFORM_LOCAL_IMPORT_ALREADY_INSTALLED)
			PopupMessage(Localize("Mod manager"), Localize("This Mod version is already installed."), Localize("OK"));
		else if(ImportResult.m_State == PLATFORM_LOCAL_IMPORT_REPLACE_REQUIRED)
		{
			if(Client()->State() != IClient::STATE_OFFLINE && ModCollectionContains(g_Config.m_ClModIds, ImportID))
				PopupMessage(
					Localize("Mod manager"), Localize("Disconnect before replacing an enabled Mod."), Localize("OK"));
			else
			{
				str_copy(m_aModImportArchive, ImportResult.m_aArchivePath, sizeof(m_aModImportArchive));
				str_copy(m_aModImportName, ImportResult.m_aName, sizeof(m_aModImportName));
				str_copy(m_aModImportVersion, ImportResult.m_aVersion, sizeof(m_aModImportVersion));
				str_copy(
					m_aModImportPreviousVersion, ImportResult.m_aPreviousVersion, sizeof(m_aModImportPreviousVersion));
				m_Popup = POPUP_MOD_REPLACE;
			}
		}
		else if(ImportResult.m_State == PLATFORM_LOCAL_IMPORT_INSTALLED)
		{
			pPlatform->RefreshWorkshopItems();
			const bool Enabled = ModCollectionContains(g_Config.m_ClModIds, ImportID);
			bool KeepInstalled = true;
			char aReloadError[256] = "";
			if(Enabled && Client()->State() == IClient::STATE_OFFLINE)
				KeepInstalled = m_pClient->ReloadWeaponPackages(aReloadError, sizeof(aReloadError));
			else if(Enabled)
				KeepInstalled = false;
			pPlatform->CompleteLocalContentImport(KeepInstalled);
			if(!KeepInstalled)
			{
				pPlatform->RefreshWorkshopItems();
				if(Client()->State() == IClient::STATE_OFFLINE)
				{
					char aRollbackError[256];
					m_pClient->ReloadWeaponPackages(aRollbackError, sizeof(aRollbackError));
				}
				PopupMessage(Localize("Unable to import Mod"),
							 aReloadError[0] ? aReloadError : Localize("Disconnect before replacing an enabled Mod."),
							 Localize("OK"));
			}
			else
				PopupMessage(Localize("Mod installed"), ImportResult.m_aName, Localize("OK"));
		}
	}
	static char s_aSearch[128] = "";
	static float s_SearchOffset = 0.0f;
	static int s_Category = 0, s_LibraryStatus = 0, s_Sort = PLATFORM_WORKSHOP_POPULAR, s_Page = 1;
	static unsigned s_QueryOperation = 0, s_TotalMatching = 0;
	static bool s_QueryWorking = false;
	static char s_aQueryError[128] = "";
	static unsigned s_LeaderboardOperation = 0;
	static bool s_LeaderboardWorking = false;
	static char s_aLeaderboardError[128] = "";
	static int s_aViewTabs[2], s_aCategories[5], s_aStatuses[5], s_aSorts[4];
	static float s_ListScroll = 0.0f;
	static int s_ListSelection = -1;
	if(!OnlineWorkshop)
	{
		m_WorkshopDiscover = false;
		s_Category = 4;
		if(s_LibraryStatus > 3)
			s_LibraryStatus = 0;
	}

	auto ContentTypeFilter = [&]()
	{
		return s_Category == 0 ? -1 : s_Category == 4 ? CONTENT_TYPE_MOD : s_Category;
	};
	auto StartQuery = [&](int Page)
	{
		CPlatformWorkshopQuery Query;
		mem_zero(&Query, sizeof(Query));
		Query.m_ContentType = ContentTypeFilter();
		Query.m_Sort = s_Sort;
		Query.m_Page = Page;
		str_copy(Query.m_aSearch, s_aSearch, sizeof(Query.m_aSearch));
		const unsigned Operation = pPlatform->QueryWorkshop(Query);
		if(Operation)
		{
			s_QueryOperation = Operation;
			s_Page = Page;
			s_QueryWorking = true;
			s_aQueryError[0] = 0;
		}
		else
		{
			s_QueryWorking = false;
			str_copy(s_aQueryError, Localize("Unable to start Steam Workshop query."), sizeof(s_aQueryError));
		}
	};

	CPlatformWorkshopQueryResult QueryResult;
	while(pPlatform->ConsumeWorkshopQueryResult(&QueryResult))
	{
		if(QueryResult.m_OperationID != s_QueryOperation)
			continue;
		s_QueryWorking = false;
		if(QueryResult.m_Succeeded)
		{
			s_TotalMatching = QueryResult.m_TotalMatching;
			s_aQueryError[0] = 0;
		}
		else
			str_copy(s_aQueryError, QueryResult.m_aError, sizeof(s_aQueryError));
	}
	CPlatformLeaderboardResult LeaderboardResult;
	while(pPlatform->ConsumeCommunityLeaderboardResult(&LeaderboardResult))
	{
		if(LeaderboardResult.m_OperationID != s_LeaderboardOperation)
			continue;
		s_LeaderboardWorking = false;
		if(LeaderboardResult.m_Succeeded)
			s_aLeaderboardError[0] = 0;
		else
			str_copy(s_aLeaderboardError, LeaderboardResult.m_aError, sizeof(s_aLeaderboardError));
	}
	CPlatformWorkshopPreviewResult PreviewResult;
	while(pPlatform->ConsumeWorkshopPreviewResult(&PreviewResult))
	{
		for(int i = 0; i < 32; i++)
		{
			if(m_aWorkshopPreviews[i].m_PublishedFileID == PreviewResult.m_PublishedFileID &&
			   m_aWorkshopPreviews[i].m_OperationID == PreviewResult.m_OperationID)
			{
				m_aWorkshopPreviews[i].m_OperationID = 0;
				if(PreviewResult.m_Succeeded)
					m_aWorkshopPreviews[i].m_Texture = Graphics()->LoadTexture(PreviewResult.m_aCachePath,
																			   IStorage::TYPE_SAVE,
																			   CImageInfo::FORMAT_AUTO,
																			   IGraphics::TEXLOAD_NOMIPMAPS);
				else
					m_aWorkshopPreviews[i].m_NextRetry = time_get() + time_freq() * 30;
				break;
			}
		}
	}

	if(OnlineWorkshop)
	{
		MainView.HSplitTop(30.0f, &ViewTabs, &MainView);
		for(int i = 0; i < 2; i++)
		{
			ViewTabs.VSplitLeft(140.0f, &Button, &ViewTabs);
			const bool Active = i == (m_WorkshopDiscover ? 1 : 0);
			if(DoButton_MenuTab(&s_aViewTabs[i],
								Localize(i ? "Discover" : "My Library"),
								Active,
								&Button,
								i == 0 ? CUI::CORNER_L : CUI::CORNER_R))
			{
				m_WorkshopDiscover = i == 1;
				m_WorkshopSelectedID = 0;
				s_ListSelection = -1;
				s_ListScroll = 0.0f;
				if(m_WorkshopDiscover)
					StartQuery(1);
				else
					pPlatform->RefreshWorkshopItems();
			}
		}
	}
	if(m_WorkshopDiscover != m_LastWorkshopDiscover)
	{
		m_LastWorkshopDiscover = m_WorkshopDiscover;
		m_WorkshopTransition = 0.0f;
	}
	const float WorkshopDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_WorkshopTransition = SmoothToward(m_WorkshopTransition, 1.0f, WorkshopDt, 15.0f);
	if(fabs(m_WorkshopTransition - 1.0f) < 0.001f)
		m_WorkshopTransition = 1.0f;
	const float WorkshopEase = MenuEaseOutCubic(m_WorkshopTransition);
	const float WorkshopOffset = (1.0f - WorkshopEase) * 12.0f / max(1.0f, UI()->Scale());
	MainView.x += WorkshopOffset;
	MainView.w -= WorkshopOffset;
	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(30.0f, &SearchRow, &MainView);
	CUIRect Search, SearchButton, RefreshButton, ImportButton;
	SearchRow.VSplitRight(82.0f, &SearchRow, &RefreshButton);
	SearchRow.VSplitRight(5.0f, &SearchRow, 0);
	if(LocalImportAvailable && !m_WorkshopDiscover)
	{
		SearchRow.VSplitRight(92.0f, &SearchRow, &ImportButton);
		SearchRow.VSplitRight(5.0f, &SearchRow, 0);
	}
	if(m_WorkshopDiscover)
	{
		SearchRow.VSplitRight(76.0f, &SearchRow, &SearchButton);
		SearchRow.VSplitRight(5.0f, &SearchRow, 0);
	}
	Search = SearchRow;
	DoEditBox(&s_aSearch, &Search, s_aSearch, sizeof(s_aSearch), 10.0f, &s_SearchOffset);
	static int s_SearchButton;
	const bool SubmitSearch =
		m_WorkshopDiscover &&
		(DoButton_Menu(&s_SearchButton, Localize("Search"), 0, &SearchButton, BUTTONSTYLE_ACCENT) ||
		 (m_EnterPressed && UI()->ActiveItem() == &s_aSearch));
	if(SubmitSearch)
	{
		m_EnterPressed = false;
		StartQuery(1);
	}
	static int s_Refresh;
	if(DoButton_Menu(&s_Refresh, Localize("Refresh"), 0, &RefreshButton))
	{
		if(m_WorkshopDiscover)
			StartQuery(s_Page);
		else
			pPlatform->RefreshWorkshopItems();
	}
	static int s_ImportPackage;
	if(LocalImportAvailable && !m_WorkshopDiscover &&
	   DoButton_Menu(&s_ImportPackage, Localize("Import ZIP"), 0, &ImportButton, BUTTONSTYLE_ACCENT) &&
	   !pPlatform->BeginLocalContentImport())
	{
		m_aModImportArchive[0] = 0;
		m_Popup = POPUP_MOD_IMPORT_PATH;
		UI()->SetActiveItem(m_aModImportArchive);
	}

	if(OnlineWorkshop)
	{
		MainView.HSplitTop(5.0f, 0, &MainView);
		MainView.HSplitTop(28.0f, &CategoryRow, &MainView);
		const char *apCategories[] = {"All", "Maps", "Room Presets", "Challenges", "Mods"};
		for(int i = 0; i < 5; i++)
		{
			CategoryRow.VSplitLeft(min(112.0f, CategoryRow.w / (5 - i)), &Button, &CategoryRow);
			if(DoButton_MenuTab(&s_aCategories[i], Localize(apCategories[i]), s_Category == i, &Button, 0))
			{
				s_Category = i;
				m_WorkshopSelectedID = 0;
				if(m_WorkshopDiscover)
					StartQuery(1);
			}
			CategoryRow.VSplitLeft(3.0f, 0, &CategoryRow);
		}
	}
	MainView.HSplitTop(4.0f, 0, &MainView);
	MainView.HSplitTop(26.0f, &StatusRow, &MainView);
	if(m_WorkshopDiscover)
	{
		const char *apSorts[] = {"Latest", "Popular", "Rating", "Most subscribed"};
		for(int i = 0; i < 4; i++)
		{
			StatusRow.VSplitLeft(min(116.0f, StatusRow.w / (4 - i)), &Button, &StatusRow);
			if(DoButton_MenuTab(&s_aSorts[i], Localize(apSorts[i]), s_Sort == i, &Button, 0))
			{
				s_Sort = i;
				StartQuery(1);
			}
			StatusRow.VSplitLeft(3.0f, 0, &StatusRow);
		}
	}
	else
	{
		const char *apOnlineStatuses[] = {"All", "Installed", "Downloading", "Disabled", "Invalid"};
		const char *apLocalStatuses[] = {"All", "Enabled", "Disabled", "Invalid"};
		const char **ppStatuses = OnlineWorkshop ? apOnlineStatuses : apLocalStatuses;
		const int StatusCount = OnlineWorkshop ? 5 : 4;
		for(int i = 0; i < StatusCount; i++)
		{
			StatusRow.VSplitLeft(min(108.0f, StatusRow.w / (StatusCount - i)), &Button, &StatusRow);
			if(DoButton_MenuTab(&s_aStatuses[i], Localize(ppStatuses[i]), s_LibraryStatus == i, &Button, 0))
			{
				s_LibraryStatus = i;
				m_WorkshopSelectedID = 0;
			}
			StatusRow.VSplitLeft(3.0f, 0, &StatusRow);
		}
	}
	MainView.HSplitTop(7.0f, 0, &MainView);
	MainView.HSplitBottom(32.0f, &Body, &Footer);

	int aItems[256];
	int Count = 0;
	const int SourceCount = m_WorkshopDiscover ? pPlatform->WorkshopQueryItemCount() : pPlatform->WorkshopItemCount();
	for(int i = 0; i < SourceCount && Count < 256; i++)
	{
		CPlatformWorkshopItem Info;
		if(!(m_WorkshopDiscover ? pPlatform->WorkshopQueryItem(i, &Info) : pPlatform->WorkshopItem(i, &Info)))
			continue;
		if(ContentTypeFilter() >= 0 && Info.m_ContentType != ContentTypeFilter())
			continue;
		if(!m_WorkshopDiscover && s_aSearch[0] && !str_find_nocase(Info.m_aName, s_aSearch) &&
		   !str_find_nocase(Info.m_aAuthor, s_aSearch) && !str_find_nocase(Info.m_aDescription, s_aSearch))
			continue;
		const bool Downloading = (Info.m_State & (16 | 32)) != 0 || (Info.m_Total && Info.m_Downloaded < Info.m_Total);
		const bool Disabled = (Info.m_State & 64) != 0;
		const bool Enabled = Info.m_ContentType == CONTENT_TYPE_MOD &&
							 ModCollectionContains(g_Config.m_ClModIds, Info.m_PublishedFileID);
		if(!m_WorkshopDiscover && OnlineWorkshop &&
		   ((s_LibraryStatus == 1 && !Info.m_Valid) || (s_LibraryStatus == 2 && !Downloading) ||
			(s_LibraryStatus == 3 && !Disabled) || (s_LibraryStatus == 4 && (Info.m_Valid || Downloading))))
			continue;
		if(!m_WorkshopDiscover && !OnlineWorkshop &&
		   ((s_LibraryStatus == 1 && !Enabled) || (s_LibraryStatus == 2 && (Enabled || !Info.m_Valid)) ||
			(s_LibraryStatus == 3 && Info.m_Valid)))
			continue;
		aItems[Count++] = i;
	}
	auto GetItem = [&](int VisibleIndex, CPlatformWorkshopItem *pInfo)
	{
		return VisibleIndex >= 0 && VisibleIndex < Count &&
			   (m_WorkshopDiscover ? pPlatform->WorkshopQueryItem(aItems[VisibleIndex], pInfo)
								   : pPlatform->WorkshopItem(aItems[VisibleIndex], pInfo));
	};
	bool SelectionFound = false;
	if(m_WorkshopSelectedID)
	{
		for(int i = 0; i < Count; i++)
		{
			CPlatformWorkshopItem Info;
			if(GetItem(i, &Info) && Info.m_PublishedFileID == m_WorkshopSelectedID)
			{
				s_ListSelection = i;
				SelectionFound = true;
				break;
			}
		}
	}
	if(m_WorkshopSelectedID && !SelectionFound)
	{
		m_WorkshopSelectedID = 0;
		s_ListSelection = -1;
	}
	if(!m_WorkshopSelectedID && Count)
	{
		CPlatformWorkshopItem Info;
		if(GetItem(0, &Info))
		{
			m_WorkshopSelectedID = Info.m_PublishedFileID;
			s_ListSelection = 0;
		}
	}
	if(!Count)
	{
		s_ListSelection = -1;
		m_WorkshopSelectedID = 0;
	}
	if(m_WorkshopSelectedID != m_LastWorkshopAnimatedID)
	{
		m_LastWorkshopAnimatedID = m_WorkshopSelectedID;
		m_WorkshopDetailTransition = 0.0f;
	}
	m_WorkshopDetailTransition = SmoothToward(m_WorkshopDetailTransition, 1.0f, WorkshopDt, 17.0f);
	if(fabs(m_WorkshopDetailTransition - 1.0f) < 0.001f)
		m_WorkshopDetailTransition = 1.0f;
	const float DetailEase = MenuEaseOutCubic(m_WorkshopDetailTransition);

	const bool Compact = Body.w / max(1.0f, UI()->Scale()) < 760.0f;
	CUIRect List = Body;
	CUIRect Detail;
	if(!Compact)
	{
		Body.VSplitLeft(Body.w * 0.60f, &List, &Detail);
		List.VSplitRight(8.0f, &List, 0);
		const float DetailOffset = (1.0f - DetailEase) * 10.0f;
		Detail.x += DetailOffset;
		Detail.w -= DetailOffset;
	}
	static int s_aListIDs[256];
	for(int i = 0; i < 256; i++)
		s_aListIDs[i] = i;
	UiDoListboxStart(
		&s_aListIDs, &List, 72.0f, Localize("Workshop content"), "", Count, 1, s_ListSelection, s_ListScroll);
	for(int i = 0; i < Count; i++)
	{
		CPlatformWorkshopItem Info;
		if(!GetItem(i, &Info))
			continue;
		CListboxItem Entry = UiDoListboxNextItem(&s_aListIDs[i], s_ListSelection == i);
		if(!Entry.m_Visible)
			continue;
		CUIRect Row = Entry.m_Rect;
		CUIRect Preview;
		CUIRect Text;
		CUIRect Line;
		CUIRect Badge;
		Row.Margin(5.0f, &Row);
		Row.VSplitLeft(92.0f, &Preview, &Text);
		Text.VSplitLeft(8.0f, 0, &Text);
		DrawWorkshopPreview(Preview, Info);
		Text.HSplitTop(18.0f, &Line, &Text);
		UI()->DoLabelScaled(&Line,
							Info.m_aName[0] ? Info.m_aName : Localize("Downloading content"),
							FitScaledLabelFontSize(TextRender(), Info.m_aName, 11.5f, Line.w, UI()->Scale()),
							-1);
		Text.HSplitTop(17.0f, &Line, &Text);
		const char *apTypes[] = {"Mod", "Map", "Room Preset", "Challenge"};
		char aMeta[256];
		char aAuthor[128];
		if(!pPlatform->UserDisplayName(Info.m_OwnerUserID, aAuthor, sizeof(aAuthor)))
			str_copy(aAuthor, Info.m_aAuthor, sizeof(aAuthor));
		str_format(aMeta,
				   sizeof(aMeta),
				   "%s  ·  %s",
				   Localize(apTypes[clamp(Info.m_ContentType, 0, 3)]),
				   aAuthor[0] ? aAuthor : Localize("Unknown author"));
		UI()->DoLabelScaled(&Line, aMeta, 9.5f, -1);
		Text.HSplitTop(16.0f, &Line, &Text);
		const bool Downloading = (Info.m_State & (16 | 32)) != 0 || (Info.m_Total && Info.m_Downloaded < Info.m_Total);
		const bool Disabled = (Info.m_State & 64) != 0;
		const bool Enabled = Info.m_ContentType == CONTENT_TYPE_MOD &&
							 ModCollectionContains(g_Config.m_ClModIds, Info.m_PublishedFileID);
		const char *pState = Downloading		  ? "Downloading"
							 : !Info.m_Valid	  ? "Invalid"
							 : Disabled			  ? "Disabled"
							 : Enabled			  ? "Enabled"
							 : m_WorkshopDiscover ? "Not subscribed"
												  : "Installed";
		char aState[128];
		str_format(
			aState, sizeof(aState), "%s%s%s", Localize(pState), Info.m_aVersion[0] ? "  ·  v" : "", Info.m_aVersion);
		UI()->DoLabelScaled(&Line, aState, 9.0f, -1);
		if(Downloading)
		{
			Text.HSplitBottom(5.0f, &Text, &Badge);
			RenderTools()->DrawUIRect(&Badge, vec4(.10f, .11f, .14f, 1), CUI::CORNER_ALL, 2.0f);
			if(Info.m_Total)
			{
				CUIRect Fill = Badge;
				Fill.w *= clamp(Info.m_Downloaded / (float)Info.m_Total, 0.0f, 1.0f);
				RenderTools()->DrawUIRect(&Fill, ms_ColorAccent, CUI::CORNER_ALL, 2.0f);
			}
		}
	}
	const int NewSelection = UiDoListboxEnd(&s_ListScroll, 0);
	if(NewSelection >= 0 && NewSelection < Count && NewSelection != s_ListSelection)
	{
		s_ListSelection = NewSelection;
		CPlatformWorkshopItem Info;
		if(GetItem(NewSelection, &Info))
		{
			m_WorkshopSelectedID = Info.m_PublishedFileID;
			m_WorkshopDetailOpen = Compact;
		}
	}

	auto UseItem = [&](const CPlatformWorkshopItem &Info)
	{
		if(Info.m_ContentType == CONTENT_TYPE_MOD)
		{
			const bool Enabled = ModCollectionContains(g_Config.m_ClModIds, Info.m_PublishedFileID);
			char aPreviousIds[sizeof(g_Config.m_ClModIds)];
			char aPreviousHash[sizeof(g_Config.m_ClModHash)];
			str_copy(aPreviousIds, g_Config.m_ClModIds, sizeof(aPreviousIds));
			str_copy(aPreviousHash, g_Config.m_ClModHash, sizeof(aPreviousHash));
			if(!SetModCollectionEnabled(
				   g_Config.m_ClModIds, sizeof(g_Config.m_ClModIds), Info.m_PublishedFileID, !Enabled))
			{
				PopupMessage(
					Localize("Mod manager"), Localize("Unable to update the enabled Mod collection."), Localize("OK"));
				return;
			}
			pPlatform->RefreshWorkshopItems();
			if(g_Config.m_ClModIds[0] && !g_Config.m_ClModHash[0])
			{
				str_copy(g_Config.m_ClModIds, aPreviousIds, sizeof(g_Config.m_ClModIds));
				str_copy(g_Config.m_ClModHash, aPreviousHash, sizeof(g_Config.m_ClModHash));
				pPlatform->RefreshWorkshopItems();
				PopupMessage(
					Localize("Mod manager"), Localize("Unable to update the enabled Mod collection."), Localize("OK"));
				return;
			}
			if(Client()->State() == IClient::STATE_OFFLINE)
			{
				char aError[256];
				if(!m_pClient->ReloadWeaponPackages(aError, sizeof(aError)))
				{
					str_copy(g_Config.m_ClModIds, aPreviousIds, sizeof(g_Config.m_ClModIds));
					str_copy(g_Config.m_ClModHash, aPreviousHash, sizeof(g_Config.m_ClModHash));
					pPlatform->RefreshWorkshopItems();
					char aRollbackError[256];
					m_pClient->ReloadWeaponPackages(aRollbackError, sizeof(aRollbackError));
					PopupMessage(Localize("Mod manager"), aError, Localize("OK"));
					return;
				}
			}
			else
				PopupMessage(
					Localize("Mod manager"), Localize("The Mod change will apply after reconnecting."), Localize("OK"));
			return;
		}
		if(Info.m_ContentType == CONTENT_TYPE_ROOM_PRESET || Info.m_ContentType == CONTENT_TYPE_CHALLENGE)
		{
			CRoomPreset Preset;
			char aMessage[512];
			if(LoadWorkshopRoomPreset(Info, &Preset, aMessage, sizeof(aMessage)) &&
			   ApplyWorkshopRoomPreset(Preset, max(1, pPlatform->PartyMemberCount()), aMessage, sizeof(aMessage)))
			{
				// A challenge's optional Lua file is loaded by both the managed
				// server and the client prediction runtime. Use the installed
				// absolute path and the canonical package hash so a mismatched
				// script is never executed silently.
				g_Config.m_ClChallengeScript[0] = 0;
				g_Config.m_ClChallengeContentHash[0] = 0;
				if(Info.m_ContentType == CONTENT_TYPE_CHALLENGE)
				{
					char aID[32], aError[256];
					str_format(aID, sizeof(aID), "%llu", Info.m_PublishedFileID);
					CContentManifest Manifest;
					if(ContentPackageValidate(Info.m_aInstallPath, aID, GAME_NETVERSION, &Manifest, aError, sizeof(aError)))
					{
						for(int FileIndex = 0; FileIndex < Manifest.m_FileCount; ++FileIndex)
						{
							const CContentDeclaredFile &File = Manifest.m_aFiles[FileIndex];
							if(File.m_Type != CONTENT_FILE_SCRIPT)
								continue;
							str_format(g_Config.m_ClChallengeScript,
								 sizeof(g_Config.m_ClChallengeScript),
								 "%s/%s",
								 Info.m_aInstallPath,
								 File.m_aPath);
							str_copy(g_Config.m_ClChallengeContentHash,
								 Manifest.m_aContentHash,
								 sizeof(g_Config.m_ClChallengeContentHash));
							break;
						}
					}
				}
				CPlatformPartyState Party;
				if(pPlatform->PartyState(&Party) && Party.m_LocalOwner &&
				   Party.m_TargetType != PLATFORM_PARTY_TARGET_NONE)
					pPlatform->ClearPartyTarget();
				g_Config.m_UiPage = PAGE_LOCAL_SERVER;
				PopupMessage(Localize("Room preset applied"), aMessage, Localize("Continue"));
			}
			else
				PopupMessage(Localize("Unable to apply preset"), aMessage, Localize("OK"));
			return;
		}
		if(Info.m_ContentType == CONTENT_TYPE_MAP)
		{
			char aID[32];
			char aError[256];
			str_format(aID, sizeof(aID), "%llu", Info.m_PublishedFileID);
			CContentManifest Manifest;
			if(ContentPackageValidate(Info.m_aInstallPath, aID, GAME_NETVERSION, &Manifest, aError, sizeof(aError)))
			{
				for(int i = 0; i < Manifest.m_FileCount; i++)
				{
					if(Manifest.m_aFiles[i].m_Type == CONTENT_FILE_MAP)
					{
						str_format(g_Config.m_ClLocalServerWorkshopMap,
								   sizeof(g_Config.m_ClLocalServerWorkshopMap),
								   "workshop:%llu:%s",
								   Info.m_PublishedFileID,
								   Manifest.m_aFiles[i].m_aPath);
						g_Config.m_UiPage = PAGE_LOCAL_SERVER;
						PopupMessage(Localize("Workshop map selected"),
									 Localize("Review room rules, visibility and password before creating the room."),
									 Localize("Continue"));
						break;
					}
				}
			}
		}
	};
	auto RenderDetail = [&](CUIRect View, const CPlatformWorkshopItem &Info, bool Overlay)
	{
		DrawMenuInset(&View, CUI::CORNER_ALL);
		View.Margin(9.0f, &View);
		if(Overlay)
		{
			CUIRect Header;
			CUIRect Close;
			View.HSplitTop(28.0f, &Header, &View);
			Header.VSplitRight(32.0f, &Header, &Close);
			UI()->DoLabelScaled(&Header, Localize("Workshop details"), 14.0f, -1);
			static int s_Close;
			if(DoButton_Menu(&s_Close, "x", 0, &Close))
				m_WorkshopDetailOpen = false;
			View.HSplitTop(5.0f, 0, &View);
		}
		CUIRect Actions;
		CUIRect Preview;
		View.HSplitBottom(OnlineWorkshop ? 96.0f : 38.0f, &View, &Actions);
		View.HSplitTop(min(150.0f, View.w * 9.0f / 16.0f), &Preview, &View);
		DrawWorkshopPreview(Preview, Info);
		View.HSplitTop(7.0f, 0, &View);
		static CScrollRegion s_DetailScroll;
		static vec2 s_DetailOffset(0.0f, 0.0f);
		CScrollRegionParams Params;
		ConfigureScrollRegion(&Params);
		Params.m_ScrollUnit = 32.0f;
		s_DetailScroll.Begin(&View, &s_DetailOffset, &Params);
		CUIRect Content = View;
		Content.y += s_DetailOffset.y;
		Content.VSplitRight(16.0f, &Content, 0);
		CUIRect Line;
		Content.HSplitTop(25.0f, &Line, &Content);
		UI()->DoLabelScaled(
			&Line, Info.m_aName, FitScaledLabelFontSize(TextRender(), Info.m_aName, 14.0f, Line.w, UI()->Scale()), -1);
		s_DetailScroll.AddRect(Line);
		char aAuthor[128];
		if(!pPlatform->UserDisplayName(Info.m_OwnerUserID, aAuthor, sizeof(aAuthor)))
			str_copy(aAuthor, Info.m_aAuthor, sizeof(aAuthor));
		char aDetails[1024];
		const unsigned Votes = Info.m_VotesUp + Info.m_VotesDown;
		str_format(aDetails,
				   sizeof(aDetails),
				   "%s: %s\n%s: %s    %s: %s\n%s: %s    %s: %.0f%% (%u)\n%s: %.2f MB\nID: %llu\nHash: %.16s%s",
				   Localize("Author"),
				   aAuthor[0] ? aAuthor : Localize("Unknown author"),
				   Localize("Version"),
				   Info.m_aVersion[0] ? Info.m_aVersion : "-",
				   Localize("Protocol"),
				   Info.m_aTargetProtocol[0] ? Info.m_aTargetProtocol : "-",
				   Localize("Content rating"),
				   Info.m_aContentRating[0] ? Info.m_aContentRating : "-",
				   Localize("Rating"),
				   Info.m_Score * 100.0f,
				   Votes,
				   Localize("Size"),
				   Info.m_Total / (1024.0f * 1024.0f),
				   Info.m_PublishedFileID,
				   Info.m_aContentHash,
				   Info.m_aContentHash[0] ? "…" : "-");
		if(LocalImportAvailable && !m_WorkshopDiscover)
		{
			const char *pFolder = strrchr(Info.m_aInstallPath, '/');
			char aFolder[320];
			str_format(
				aFolder, sizeof(aFolder), "\n%s: %s", Localize("Folder"), pFolder ? pFolder + 1 : Info.m_aInstallPath);
			str_append(aDetails, aFolder, sizeof(aDetails));
		}
		Content.HSplitTop(LocalImportAvailable && !m_WorkshopDiscover ? 121.0f : 105.0f, &Line, &Content);
		UI()->DoLabelScaled(&Line, aDetails, 9.5f, -1);
		s_DetailScroll.AddRect(Line);
		Content.HSplitTop(22.0f, &Line, &Content);
		UI()->DoLabelScaled(&Line, Localize("Description"), 11.0f, -1);
		s_DetailScroll.AddRect(Line);
		Content.HSplitTop(max(90.0f, min(220.0f, 70.0f + str_length(Info.m_aDescription) * 0.16f)), &Line, &Content);
		UI()->DoLabelScaled(
			&Line, Info.m_aDescription[0] ? Info.m_aDescription : Localize("No description provided."), 9.5f, -1);
		s_DetailScroll.AddRect(Line);
		if(Info.m_aError[0])
		{
			Content.HSplitTop(22.0f, &Line, &Content);
			TextRender()->TextColor(ms_ColorDanger.r, ms_ColorDanger.g, ms_ColorDanger.b, 1);
			UI()->DoLabelScaled(&Line, Info.m_aError, 9.5f, -1);
			TextRender()->TextColor(1, 1, 1, 1);
			s_DetailScroll.AddRect(Line);
		}
		if(Info.m_ContentType == CONTENT_TYPE_CHALLENGE)
		{
			Content.HSplitTop(24.0f, &Line, &Content);
			UI()->DoLabelScaled(&Line, Localize("Community leaderboard · Unverified"), 11.0f, -1);
			s_DetailScroll.AddRect(Line);
			if(s_LeaderboardWorking || s_aLeaderboardError[0])
			{
				Content.HSplitTop(18.0f, &Line, &Content);
				UI()->DoLabelScaled(
					&Line,
					Localize(s_LeaderboardWorking ? "Loading community leaderboard…" : s_aLeaderboardError),
					9.0f,
					-1);
				s_DetailScroll.AddRect(Line);
			}
			for(int i = 0; i < pPlatform->CommunityLeaderboardEntryCount() && i < 8; i++)
			{
				CPlatformLeaderboardEntry Entry;
				if(!pPlatform->CommunityLeaderboardEntry(i, &Entry))
					continue;
				char aRank[192];
				str_format(aRank, sizeof(aRank), "#%d  %s  %d", Entry.m_Rank, Entry.m_aName, Entry.m_Score);
				Content.HSplitTop(18.0f, &Line, &Content);
				UI()->DoLabelScaled(&Line, aRank, 9.5f, -1);
				s_DetailScroll.AddRect(Line);
			}
		}
		s_DetailScroll.End();
		const bool SteamManaged = OnlineWorkshop && !Info.m_LocalInstall;
		const bool Subscribed = SteamManaged && (Info.m_State & 1) != 0;
		const bool Disabled = SteamManaged && (Info.m_State & 64) != 0;
		const bool Downloading =
			SteamManaged && ((Info.m_State & (16 | 32)) != 0 || (Info.m_Total && Info.m_Downloaded < Info.m_Total));
		const bool Enabled = Info.m_ContentType == CONTENT_TYPE_MOD &&
							 ModCollectionContains(g_Config.m_ClModIds, Info.m_PublishedFileID);
		CUIRect MainAction;
		CUIRect Secondary;
		CUIRect Community;
		CUIRect Remove;
		Actions.HSplitTop(30.0f, &MainAction, &Actions);
		Actions.HSplitTop(4.0f, 0, &Actions);
		Actions.HSplitTop(28.0f, &Secondary, &Actions);
		Secondary.VSplitLeft(Secondary.w / 2.0f - 2.0f, &Secondary, &Community);
		Community.VSplitLeft(4.0f, 0, &Community);
		Actions.HSplitTop(4.0f, 0, &Actions);
		Remove = Actions;
		static int s_Main;
		static int s_Toggle;
		static int s_Community;
		static int s_Remove;
		static int s_LeaderboardScope;
		const char *pMain = !SteamManaged							 ? Enabled		  ? "Disable Mod"
																	   : Info.m_Valid ? "Enable Mod"
																					  : "Invalid Mod"
							: !Subscribed							 ? "Subscribe"
							: Downloading							 ? "Downloading"
							: Disabled								 ? "Enable to use"
							: !Info.m_Valid							 ? "Retry download"
							: Info.m_ContentType == CONTENT_TYPE_MOD ? (Enabled ? "Disable Mod" : "Enable Mod")
																	 : "Use to create room";
		if(DoButton_Menu(&s_Main, Localize(pMain), 0, &MainAction, BUTTONSTYLE_ACCENT) && !Downloading)
		{
			if(!SteamManaged)
			{
				if(Enabled || Info.m_Valid)
					UseItem(Info);
			}
			else if(!Subscribed)
				pPlatform->SubscribeWorkshopItem(Info.m_PublishedFileID);
			else if(Disabled)
				pPlatform->SetWorkshopItemDisabled(Info.m_PublishedFileID, false);
			else if(!Info.m_Valid)
				pPlatform->RequestWorkshopDownload(Info.m_PublishedFileID);
			else
				UseItem(Info);
		}
		if(!SteamManaged)
			return;
		const char *apScopes[] = {"Leaderboard: Global", "Leaderboard: Friends", "Leaderboard: Around me"};
		const char *pToggle = Disabled										 ? "Enable"
							  : Info.m_ContentType == CONTENT_TYPE_CHALLENGE ? apScopes[s_LeaderboardScope]
																			 : "Disable";
		if(DoButton_Menu(&s_Toggle, Localize(pToggle), 0, &Secondary) && Subscribed)
		{
			if(Disabled)
				pPlatform->SetWorkshopItemDisabled(Info.m_PublishedFileID, false);
			else if(Info.m_ContentType == CONTENT_TYPE_CHALLENGE && Info.m_Valid)
			{
				CCommunityChallengeDescriptor Descriptor;
				char aError[128];
				if(LoadWorkshopChallengeDescriptor(Info, &Descriptor, aError, sizeof(aError)))
				{
					const unsigned Operation =
						pPlatform->QueryCommunityChallenge(Descriptor.m_PublishedFileID,
														   Descriptor.m_Revision,
														   Descriptor.m_Metric,
														   (EPlatformLeaderboardScope)s_LeaderboardScope);
					if(Operation)
					{
						s_LeaderboardOperation = Operation;
						s_LeaderboardWorking = true;
						s_aLeaderboardError[0] = 0;
						s_LeaderboardScope = (s_LeaderboardScope + 1) % 3;
					}
					else
					{
						s_LeaderboardWorking = false;
						str_copy(s_aLeaderboardError,
								 Localize("Unable to start community leaderboard request."),
								 sizeof(s_aLeaderboardError));
					}
				}
			}
			else
				pPlatform->SetWorkshopItemDisabled(Info.m_PublishedFileID, true);
		}
		if(OnlineWorkshop && DoButton_Menu(&s_Community, Localize("Community / Report"), 0, &Community))
			pPlatform->OpenWorkshopItemPage(Info.m_PublishedFileID);
		if(OnlineWorkshop && DoButton_Menu(&s_Remove, Localize("Unsubscribe"), 0, &Remove, BUTTONSTYLE_DANGER) &&
		   Subscribed)
		{
			pPlatform->UnsubscribeWorkshopItem(Info.m_PublishedFileID);
			m_WorkshopSelectedID = 0;
		}
	};

	CPlatformWorkshopItem Selected;
	const bool HasSelection = GetItem(s_ListSelection, &Selected);
	if(!Compact)
	{
		if(HasSelection)
			RenderDetail(Detail, Selected, false);
		else
		{
			DrawMenuInset(&Detail, CUI::CORNER_ALL);
			Detail.Margin(10.0f, &Detail);
			UI()->DoLabelScaled(&Detail,
								Localize(Count				? "Select Workshop content to view details."
										 : s_aQueryError[0] ? s_aQueryError
										 : s_QueryWorking	? "Loading Workshop content…"
															: "No Workshop content matches these filters."),
								11.0f,
								-1);
		}
	}
	if(Compact && HasSelection)
	{
		CUIRect DetailButton;
		Footer.VSplitRight(100.0f, &Footer, &DetailButton);
		static int s_Details;
		if(DoButton_Menu(&s_Details, Localize("Details"), m_WorkshopDetailOpen, &DetailButton, BUTTONSTYLE_ACCENT))
			m_WorkshopDetailOpen = true;
	}
	if(Compact && m_WorkshopDetailOpen && HasSelection)
	{
		CUIRect Overlay = Body;
		const float DetailOffset = (1.0f - DetailEase) * 12.0f;
		Overlay.x += DetailOffset;
		Overlay.w -= DetailOffset;
		RenderDetail(Overlay, Selected, true);
		if(m_EscapePressed)
		{
			m_WorkshopDetailOpen = false;
			m_EscapePressed = false;
		}
	}

	static int s_Browse;
	static int s_Previous;
	static int s_Next;
	if(OnlineWorkshop)
	{
		Footer.VSplitLeft(145.0f, &Button, &Footer);
		if(DoButton_Menu(&s_Browse, Localize("Browse Workshop"), 0, &Button))
			pPlatform->OpenWorkshopBrowsePage();
	}
	if(m_WorkshopDiscover)
	{
		Footer.VSplitLeft(8.0f, 0, &Footer);
		Footer.VSplitLeft(38.0f, &Button, &Footer);
		if(DoButton_Menu(&s_Previous, "<", 0, &Button) && s_Page > 1)
			StartQuery(s_Page - 1);
		Footer.VSplitLeft(6.0f, 0, &Footer);
		CUIRect PageLabel;
		Footer.VSplitLeft(86.0f, &PageLabel, &Footer);
		char aPage[64];
		str_format(aPage, sizeof(aPage), "%d / %u", s_Page, max(1u, (s_TotalMatching + 49) / 50));
		UI()->DoLabelScaled(&PageLabel, aPage, 10.0f, 0);
		Footer.VSplitLeft(38.0f, &Button, &Footer);
		if(DoButton_Menu(&s_Next, ">", 0, &Button) && (unsigned)(s_Page * 50) < s_TotalMatching)
			StartQuery(s_Page + 1);
		if(s_QueryWorking)
			UI()->DoLabelScaled(&Footer, Localize("Loading Workshop content…"), 9.5f, 1);
		else if(s_aQueryError[0])
		{
			TextRender()->TextColor(ms_ColorDanger.r, ms_ColorDanger.g, ms_ColorDanger.b, 1);
			UI()->DoLabelScaled(&Footer, s_aQueryError, 9.5f, 1);
			TextRender()->TextColor(1, 1, 1, 1);
		}
	}
}

int CMenus::Render()
{
	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
	const float OpenDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_MenuOpenTransition = SmoothToward(m_MenuOpenTransition, 1.0f, OpenDt, 14.0f);
	if(fabs(m_MenuOpenTransition - 1.0f) < 0.001f)
		m_MenuOpenTransition = 1.0f;
	const float OpenOffset = (1.0f - MenuEaseOutCubic(m_MenuOpenTransition)) * 8.0f / max(1.0f, UI()->Scale());
	Screen.y += OpenOffset;
	Screen.h -= OpenOffset;

	static bool s_First = true;
	if(s_First)
	{
		if(g_Config.m_UiPage == PAGE_STEAM || g_Config.m_UiPage == PAGE_INTERNET)
		{
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
			IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
			if(pPlatform && pPlatform->Available())
				pPlatform->RefreshLobbyList();
		}
		else if(g_Config.m_UiPage == PAGE_LAN)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
		else if(g_Config.m_UiPage == PAGE_FAVORITES)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
		else if(g_Config.m_UiPage == PAGE_LOCAL_SERVER)
		{
			m_PlayTab = 1;
			m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
		}
		m_pClient->m_pSounds->Enqueue(CSounds::CHN_MUSIC, SOUND_MENU);
		s_First = false;
	}

	if(Client()->State() == IClient::STATE_ONLINE)
	{
		ms_ColorTabbarInactive = ms_ColorTabbarInactiveIngame;
		ms_ColorTabbarActive = ms_ColorTabbarActiveIngame;
	}
	else
	{
		RenderBackground();
		ms_ColorTabbarInactive = ms_ColorTabbarInactiveOutgame;
		ms_ColorTabbarActive = ms_ColorTabbarActiveOutgame;
	}

	CUIRect Navigation;
	CUIRect MainView;
	const int RequestedPage = Client()->State() == IClient::STATE_OFFLINE ? g_Config.m_UiPage : m_GamePage;
	const bool FullscreenResearch = RequestedPage == PAGE_RESEARCH;

	if(!FullscreenResearch)
	{
		Screen.Margin(g_Config.m_UiWideview ? 6.0f : 10.0f, &Screen);
		LayoutCenterPanel(&Screen, &Screen);
	}

	static bool s_SoundCheck = false;
	if(!s_SoundCheck && m_Popup == POPUP_NONE)
	{
		if(Client()->SoundInitFailed())
			m_Popup = POPUP_SOUNDERROR;
		s_SoundCheck = true;
	}

	const float MenuDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	if(RequestedPage != m_LastAnimatedPage)
	{
		m_LastAnimatedPage = RequestedPage;
		m_PageTransition = 0.0f;
	}
	m_PageTransition = SmoothToward(m_PageTransition, 1.0f, MenuDt, 13.0f);
	if(fabs(m_PageTransition - 1.0f) < 0.001f)
		m_PageTransition = 1.0f;

	if(m_Popup != m_LastAnimatedPopup)
	{
		m_LastAnimatedPopup = m_Popup;
		m_PopupTransition = m_Popup == POPUP_NONE ? 1.0f : 0.0f;
	}
	if(m_Popup != POPUP_NONE)
		m_PopupTransition = SmoothToward(m_PopupTransition, 1.0f, MenuDt, 16.0f);

	if(m_Popup == POPUP_NONE)
	{
		const bool FrontCanvas = Client()->State() == IClient::STATE_OFFLINE && g_Config.m_UiPage == PAGE_FRONT;
		if(FrontCanvas || FullscreenResearch)
			MainView = Screen;
		else
		{
			const float NavigationWidth = Screen.w < 900.0f ? 126.0f : 154.0f;
			Screen.VSplitLeft(NavigationWidth, &Navigation, &MainView);
			MainView.VSplitLeft(10.0f, 0, &MainView);
			MainView.HMargin(2.0f, &MainView);
		}
		if(!FrontCanvas && !FullscreenResearch)
			DrawOpenPageFrame(&MainView);
		const CUIRect PageBounds = MainView;
		const float PageEase = MenuEaseOutCubic(m_PageTransition);
		const float PageOffset = (1.0f - PageEase) * 14.0f / max(1.0f, UI()->Scale());
		MainView.x += PageOffset;
		MainView.w -= PageOffset;

		// render current page
		if(Client()->State() != IClient::STATE_OFFLINE)
		{
			if(m_GamePage == PAGE_LOCAL_SERVER && g_Config.m_ClTutorialActive &&
			   g_Config.m_ClTutorialChapter == TUTORIAL_CHAPTER_MULTIPLAYER && g_Config.m_ClTutorialStep >= 1)
				RenderTutorialRoomPractice(MainView);
			else if(m_GamePage == PAGE_LOCAL_SERVER)
				RenderCreateRoom(MainView);
			else if(m_GamePage == PAGE_GAME)
				RenderGame(MainView);
			else if(m_GamePage == PAGE_PLAYERS)
				RenderPlayers(MainView);
			else if(m_GamePage == PAGE_SERVER_INFO)
				RenderServerInfo(MainView);
			else if(m_GamePage == PAGE_CALLVOTE)
				RenderServerControl(MainView);
			else if(m_GamePage == PAGE_SETTINGS)
				RenderSettings(MainView);
			else if(m_GamePage == PAGE_RESEARCH)
				m_pClient->m_pPveRoguelite->RenderResearch(MainView);
			else if(m_GamePage == PAGE_CUSTOMIZE)
				RenderCustomize(MainView);
		}
		else if(g_Config.m_UiPage == PAGE_FRONT)
			RenderFront(MainView);
		else if(g_Config.m_UiPage == PAGE_TUTORIAL_SELECT)
			RenderTutorialChapterSelect(MainView);
		else if(g_Config.m_UiPage == PAGE_NEWS)
			RenderNews(MainView);
		else if(g_Config.m_UiPage == PAGE_INTERNET || g_Config.m_UiPage == PAGE_LAN ||
				g_Config.m_UiPage == PAGE_FAVORITES || g_Config.m_UiPage == PAGE_LOCAL_SERVER)
			RenderPlay(MainView);
		else if(g_Config.m_UiPage == PAGE_LAN)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_DEMOS)
			RenderDemoList(MainView);
		else if(g_Config.m_UiPage == PAGE_SETTINGS)
			RenderSettings(MainView);
		else if(g_Config.m_UiPage == PAGE_RESEARCH)
			m_pClient->m_pPveRoguelite->RenderResearch(MainView);
		else if(g_Config.m_UiPage == PAGE_MODS || g_Config.m_UiPage == PAGE_STEAM)
			RenderMods(MainView);
		else if(g_Config.m_UiPage == PAGE_CUSTOMIZE)
			RenderCustomize(MainView);

		if(m_PageTransition < 0.999f)
		{
			vec4 Veil = ms_ColorBgDeep;
			Veil.a = (1.0f - PageEase) * 0.22f;
			RenderTools()->DrawUIRect(&PageBounds, Veil, CUI::CORNER_ALL, ms_PanelRounding);
		}

		// Draw navigation last so compact hover labels can float over the page.
		if(!FrontCanvas && !FullscreenResearch)
			RenderMenubar(Navigation);
	}
	else
	{
		// make sure that other windows doesn't do anything funnay!
		// UI()->SetHotItem(0);
		// UI()->SetActiveItem(0);
		char aBuf[128];
		const char *pTitle = "";
		const char *pExtraText = "";
		const char *pButtonText = "";
		int ExtraAlign = 0;

		if(m_Popup == POPUP_MESSAGE)
		{
			pTitle = m_aMessageTopic;
			pExtraText = m_aMessageBody;
			pButtonText = m_aMessageButton;
		}
		else if(m_Popup == POPUP_MOD_REPLACE)
		{
			pTitle = Localize("Replace local Mod?");
			str_format(aBuf,
					   sizeof(aBuf),
					   "%s\n%s: %s  ->  %s",
					   m_aModImportName,
					   Localize("Version"),
					   m_aModImportPreviousVersion[0] ? m_aModImportPreviousVersion : "-",
					   m_aModImportVersion[0] ? m_aModImportVersion : "-");
			pExtraText = aBuf;
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_MOD_IMPORT_PATH)
		{
			pTitle = Localize("Import ZIP");
			pExtraText = Localize("Enter the full path to a ZIP package.");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			CClientAsyncStatus Status;
			Client()->ConnectionStatus(&Status);
			if(Status.m_Stage == CLIENT_STAGE_LOADING_MAP)
				pTitle = Localize("Loading map");
			else if(Status.m_Stage == CLIENT_STAGE_AUTHENTICATING)
				pTitle = Localize("Authenticating with Steam");
			else if(Status.m_Stage == CLIENT_STAGE_SYNCING_MODS)
				pTitle = Localize("Synchronizing Workshop content");
			else
				pTitle = Localize("Connecting to server");
			pExtraText = Client()->GetConnectAddress();
			pButtonText = Localize("Abort");
			if(Client()->MapDownloadTotalsize() > 0)
			{
				pTitle = Localize("Downloading map");
				pExtraText = "";
			}
		}
		else if(m_Popup == POPUP_DISCONNECTED)
		{
			pTitle = Localize("Disconnected");
			pExtraText = Client()->ErrorString();
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_PURE)
		{
			pTitle = Localize("Disconnected");
			pExtraText = Localize("The server is running a non-standard tuning on a pure game type.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			pTitle = Localize("Delete demo");
			pExtraText = Localize("Are you sure that you want to delete the demo?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			pTitle = Localize("Rename demo");
			pExtraText = "";
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			pTitle = Localize("Remove friend");
			pExtraText = Localize("Are you sure that you want to remove the player from your friends list?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_SLICE_DEMO)
		{
			pTitle = Localize("Slice demo");
			pExtraText = Localize("Please enter a filename for the sliced demo:");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			pTitle = Localize("Render video");
			pExtraText = Localize("Requires FFmpeg in PATH. Output is saved under videos/.");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_SOUNDERROR)
		{
			pTitle = Localize("Sound error");
			pExtraText = Localize("The audio device couldn't be initialised.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			pTitle = Localize("Password incorrect");
			pExtraText = "";
			pButtonText = Localize("Try again");
		}
		else if(m_Popup == POPUP_QUIT)
		{
			pTitle = Localize("Quit");
			pExtraText = Localize("Are you sure that you want to quit?");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_TUTORIAL_EXIT)
		{
			pTitle = Localize("Leave training?");
			pExtraText = Localize("Your last completed checkpoint is saved locally. You can continue now, return to "
								  "the hub, or skip training.");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_CLOUD_CONFLICT)
		{
			pTitle = Localize("Steam Cloud conflict");
			str_format(aBuf,
					   sizeof(aBuf),
					   "%s: %d RP, %s %d\n%s: %d RP, %s %d",
					   Localize("This device"),
					   m_CloudLocalSummary.m_ResearchPoints,
					   Localize("highest floor"),
					   m_CloudLocalSummary.m_HighestInvasion,
					   Localize("Steam Cloud"),
					   m_CloudRemoteSummary.m_ResearchPoints,
					   Localize("highest floor"),
					   m_CloudRemoteSummary.m_HighestInvasion);
			pExtraText = aBuf;
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			pTitle = Localize("Welcome to Ninslash");
			pExtraText = Localize(
				"As this is the first time you launch the game, please enter your nick name below. It's recommended "
				"that you check the settings to adjust them to your liking before joining a server.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_TUTORIAL_PROMPT)
		{
			pTitle = Localize("Learn the basics?");
			pExtraText = Localize("Ninslash has a short guided tutorial covering movement, combat, objectives, "
								  "forging and building. Training is optional and always replayable.");
			ExtraAlign = -1;
		}

		CUIRect Box, Part;
		Box = Screen;
		Box.VMargin(150.0f / UI()->Scale(), &Box);
		Box.HMargin(150.0f / UI()->Scale(), &Box);
		const float PopupEase = MenuEaseOutCubic(m_PopupTransition);
		Box.Margin((1.0f - PopupEase) * 14.0f / max(1.0f, UI()->Scale()), &Box);

		// render the box
		DrawMenuBorder(&Box, ms_ColorBgPanel, ms_ColorAccentDim, CUI::CORNER_ALL, 15.0f);

		Box.HSplitTop(20.f / UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f / UI()->Scale(), &Part, &Box);
		{
			vec4 Accent = ms_ColorAccent;
			TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
		}
		UI()->DoLabelScaled(&Part, pTitle, 18.f, 0);
		{
			vec4 TextCol = ms_ColorText;
			TextRender()->TextColor(TextCol.r, TextCol.g, TextCol.b, 1.0f);
		}
		Box.HSplitTop(20.f / UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f / UI()->Scale(), &Part, &Box);
		Part.VMargin(20.f / UI()->Scale(), &Part);

		if(ExtraAlign == -1)
			UI()->DoLabelScaled(&Part, pExtraText, 14.f, -1, (int)Part.w);
		else
			UI()->DoLabelScaled(&Part, pExtraText, 14.f, 0, -1);

		if(m_Popup == POPUP_MOD_REPLACE)
		{
			CUIRect Replace, Cancel;
			Box.HSplitBottom(28.0f, &Box, &Part);
			Part.VMargin(24.0f, &Part);
			Part.VSplitMid(&Cancel, &Replace);
			Cancel.VMargin(6.0f, &Cancel);
			Replace.VMargin(6.0f, &Replace);
			static int s_CancelModReplace, s_ConfirmModReplace;
			if(DoButton_Menu(&s_CancelModReplace, Localize("Cancel"), 0, &Cancel) || m_EscapePressed)
				m_Popup = POPUP_NONE;
			if(DoButton_Menu(&s_ConfirmModReplace, Localize("Replace"), 0, &Replace, BUTTONSTYLE_DANGER) ||
			   m_EnterPressed)
			{
				IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
				if(!pPlatform || !pPlatform->ImportLocalContentArchive(m_aModImportArchive, true))
					PopupMessage(Localize("Mod manager"), Localize("Unable to start the Mod import."), Localize("OK"));
				else
					m_Popup = POPUP_NONE;
			}
		}
		else if(m_Popup == POPUP_MOD_IMPORT_PATH)
		{
			CUIRect Import, Cancel, TextBox;
			Box.HSplitBottom(28.0f, &Box, &Part);
			Part.VMargin(24.0f, &Part);
			Part.VSplitMid(&Cancel, &Import);
			Cancel.VMargin(6.0f, &Cancel);
			Import.VMargin(6.0f, &Import);
			Box.HSplitBottom(62.0f, &Box, &TextBox);
			TextBox.VMargin(24.0f, &TextBox);
			TextBox.HSplitTop(26.0f, &TextBox, 0);
			static float s_PathOffset = 0.0f;
			DoEditBox(
				m_aModImportArchive, &TextBox, m_aModImportArchive, sizeof(m_aModImportArchive), 11.0f, &s_PathOffset);
			static int s_CancelModPath, s_ImportModPath;
			if(DoButton_Menu(&s_CancelModPath, Localize("Cancel"), 0, &Cancel) || m_EscapePressed)
				m_Popup = POPUP_NONE;
			if((DoButton_Menu(&s_ImportModPath, Localize("Import"), 0, &Import, BUTTONSTYLE_ACCENT) ||
				m_EnterPressed) &&
			   m_aModImportArchive[0])
			{
				IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
				if(!pPlatform || !pPlatform->ImportLocalContentArchive(m_aModImportArchive, false))
					PopupMessage(Localize("Mod manager"), Localize("Unable to start the Mod import."), Localize("OK"));
				else
					m_Popup = POPUP_NONE;
			}
		}
		else if(m_Popup == POPUP_CLOUD_CONFLICT)
		{
			CUIRect LocalButton, CloudButton;
			Box.HSplitBottom(28.0f, &Box, &Part);
			Part.VMargin(24.0f, &Part);
			Part.VSplitMid(&LocalButton, &CloudButton);
			LocalButton.VMargin(6.0f, &LocalButton);
			CloudButton.VMargin(6.0f, &CloudButton);
			static int s_UseLocalCloudProfile, s_UseRemoteCloudProfile;
			if(DoButton_Menu(&s_UseLocalCloudProfile, Localize("Use this device"), 0, &LocalButton))
				ResolveCloudConflict(false);
			if(DoButton_Menu(
				   &s_UseRemoteCloudProfile, Localize("Use Steam Cloud"), 0, &CloudButton, BUTTONSTYLE_ACCENT) ||
			   m_EnterPressed)
				ResolveCloudConflict(true);
			if(m_EscapePressed)
			{
				m_CloudPaused = true;
				m_Popup = POPUP_NONE;
				str_copy(
					m_aCloudStatus, "Steam Cloud conflict postponed; cloud writes are paused", sizeof(m_aCloudStatus));
			}
		}
		else if(m_Popup == POPUP_TUTORIAL_EXIT)
		{
			CUIRect Continue, Save, Skip;
			Box.HSplitBottom(28.0f, &Box, &Part);
			Part.VMargin(24.0f, &Part);
			Part.VSplitLeft((Part.w - 12.0f) / 3.0f, &Continue, &Part);
			Part.VSplitLeft(6.0f, 0, &Part);
			Part.VSplitLeft((Part.w - 6.0f) / 2.0f, &Save, &Part);
			Part.VSplitLeft(6.0f, 0, &Part);
			Skip = Part;
			static int s_ContinueTraining, s_SaveTraining, s_SkipTraining;
			if(DoButton_Menu(&s_ContinueTraining, Localize("Continue"), 0, &Continue, BUTTONSTYLE_ACCENT) ||
			   m_EscapePressed)
			{
				m_Popup = POPUP_NONE;
				SetActive(false);
			}
			if(DoButton_Menu(&s_SaveTraining, Localize("Exit and save"), 0, &Save))
			{
				if(g_Config.m_ClTutorialState == 2)
					FinishTutorial();
				else
				{
					m_Popup = POPUP_NONE;
					g_Config.m_ClTutorialActive = 0;
					StopLocalServer(false);
					OpenTutorialChapterSelect();
				}
			}
			if(DoButton_Menu(&s_SkipTraining,
							 Localize(g_Config.m_ClTutorialState == 2 ? "Return to hub" : "Skip training"),
							 0,
							 &Skip,
							 g_Config.m_ClTutorialState == 2 ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_DANGER))
			{
				if(g_Config.m_ClTutorialState == 2)
					FinishTutorial();
				else
				{
					g_Config.m_ClTutorialState = 3;
					g_Config.m_ClTutorialActive = 0;
					m_Popup = POPUP_NONE;
					g_Config.m_UiPage = PAGE_FRONT;
					StopLocalServer(false);
				}
			}
		}
		else if(m_Popup == POPUP_QUIT)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			// additional info
			Box.HSplitTop(10.0f, 0, &Box);
			Box.VMargin(20.f / UI()->Scale(), &Box);
			if(m_pClient->Editor()->HasUnsavedData())
			{
				char aBuf[256];
				str_format(
					aBuf,
					sizeof(aBuf),
					"%s\n%s",
					Localize(
						"There's an unsaved map in the editor, you might want to save it before you quit the game."),
					Localize("Quit anyway?"));
				UI()->DoLabelScaled(&Box, aBuf, 20.f, -1, Part.w - 20.0f);
			}

			// buttons
			Part.VMargin(80.0f, &Part);
			Part.VSplitMid(&No, &Yes);
			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
				Client()->Quit();
		}
		else if(m_Popup == POPUP_PASSWORD)
		{
			CUIRect Label, TextBox, TryAgain, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &TryAgain);

			TryAgain.VMargin(20.0f, &TryAgain);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Try again"), 0, &TryAgain) || m_EnterPressed)
			{
				Client()->Connect(Client()->GetConnectAddress());
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Password"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(
				&g_Config.m_Password, &TextBox, g_Config.m_Password, sizeof(g_Config.m_Password), 12.0f, &Offset, true);
		}
		else if(m_Popup == POPUP_CONNECTING)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || m_EnterPressed)
			{
				Client()->Disconnect();
				m_Popup = POPUP_NONE;
			}

			if(Client()->MapDownloadTotalsize() > 0)
			{
				int64 Now = time_get();
				if(Now - m_DownloadLastCheckTime >= time_freq())
				{
					if(m_DownloadLastCheckSize > Client()->MapDownloadAmount())
					{
						// map downloaded restarted
						m_DownloadLastCheckSize = 0;
					}

					// update download speed
					float Diff = (Client()->MapDownloadAmount() - m_DownloadLastCheckSize) /
								 ((int)((Now - m_DownloadLastCheckTime) / time_freq()));
					float StartDiff = m_DownloadLastCheckSize - 0.0f;
					if(StartDiff + Diff > 0.0f)
						m_DownloadSpeed = (Diff / (StartDiff + Diff)) * (Diff / 1.0f) +
										  (StartDiff / (Diff + StartDiff)) * m_DownloadSpeed;
					else
						m_DownloadSpeed = 0.0f;
					m_DownloadLastCheckTime = Now;
					m_DownloadLastCheckSize = Client()->MapDownloadAmount();
				}

				Box.HSplitTop(64.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				str_format(aBuf,
						   sizeof(aBuf),
						   "%d/%d KiB (%.1f KiB/s)",
						   Client()->MapDownloadAmount() / 1024,
						   Client()->MapDownloadTotalsize() / 1024,
						   m_DownloadSpeed / 1024.0f);
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

				// time left
				const char *pTimeLeftString;
				int TimeLeft =
					max(1,
						m_DownloadSpeed > 0.0f
							? static_cast<int>((Client()->MapDownloadTotalsize() - Client()->MapDownloadAmount()) /
											   m_DownloadSpeed)
							: 1);
				if(TimeLeft >= 60)
				{
					TimeLeft /= 60;
					pTimeLeftString = TimeLeft == 1 ? Localize("%i minute left") : Localize("%i minutes left");
				}
				else
					pTimeLeftString = TimeLeft == 1 ? Localize("%i second left") : Localize("%i seconds left");
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				str_format(aBuf, sizeof(aBuf), pTimeLeftString, TimeLeft);
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

				// progress bar
				Box.HSplitTop(20.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				Part.VMargin(40.0f, &Part);
				RenderTools()->DrawUIRect(&Part, ms_ColorBgInset, CUI::CORNER_ALL, 5.0f);
				Part.w = max(10.0f, (Part.w * Client()->MapDownloadAmount()) / Client()->MapDownloadTotalsize());
				RenderTools()->DrawUIRect(&Part, ms_ColorAccent, CUI::CORNER_ALL, 5.0f);
			}
		}
		else if(m_Popup == POPUP_LANGUAGE)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);
			RenderLanguageSelection(Box);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || m_EscapePressed || m_EnterPressed)
				m_Popup = g_Config.m_ClTutorialPromptHandled ? POPUP_FIRST_LAUNCH : POPUP_TUTORIAL_PROMPT;
		}
		else if(m_Popup == POPUP_COUNTRY)
		{
			Box = Screen;
			Box.VMargin(150.0f, &Box);
			Box.HMargin(150.0f, &Box);
			Box.HSplitTop(20.f, &Part, &Box);
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Box.HSplitBottom(20.f, &Box, 0);
			Box.VMargin(20.0f, &Box);

			static int ActSelection = -2;
			if(ActSelection == -2)
				ActSelection = g_Config.m_BrFilterCountryIndex;
			static float s_ScrollValue = 0.0f;
			int OldSelected = -1;
			UiDoListboxStart(&s_ScrollValue,
							 &Box,
							 50.0f,
							 Localize("Country"),
							 "",
							 m_pClient->m_pCountryFlags->Num(),
							 6,
							 OldSelected,
							 s_ScrollValue);

			for(int i = 0; i < m_pClient->m_pCountryFlags->Num(); ++i)
			{
				const CCountryFlags::CCountryFlag *pEntry = m_pClient->m_pCountryFlags->GetByIndex(i);
				if(pEntry->m_CountryCode == ActSelection)
					OldSelected = i;

				CListboxItem Item = UiDoListboxNextItem(&pEntry->m_CountryCode, OldSelected == i);
				if(Item.m_Visible)
				{
					CUIRect Label;
					Item.m_Rect.Margin(5.0f, &Item.m_Rect);
					Item.m_Rect.HSplitBottom(10.0f, &Item.m_Rect, &Label);
					float OldWidth = Item.m_Rect.w;
					Item.m_Rect.w = Item.m_Rect.h * 2;
					Item.m_Rect.x += (OldWidth - Item.m_Rect.w) / 2.0f;
					vec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
					m_pClient->m_pCountryFlags->Render(
						pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
					UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, 0);
				}
			}

			const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
			if(OldSelected != NewSelected)
				ActSelection = m_pClient->m_pCountryFlags->GetByIndex(NewSelected)->m_CountryCode;

			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, Localize("Ok"), 0, &Part) || m_EnterPressed)
			{
				g_Config.m_BrFilterCountryIndex = ActSelection;
				Client()->ServerBrowserUpdate();
				m_Popup = POPUP_NONE;
			}

			if(m_EscapePressed)
			{
				ActSelection = g_Config.m_BrFilterCountryIndex;
				m_Popup = POPUP_NONE;
			}
		}
		else if(m_Popup == POPUP_DELETE_DEMO)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// delete demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBuf[512];
					str_format(aBuf,
							   sizeof(aBuf),
							   "%s/%s",
							   m_aCurrentDemoFolder,
							   m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					if(Storage()->RemoveFile(aBuf, m_lDemos[m_DemolistSelectedIndex].m_StorageType))
					{
						DemolistPopulate();
						DemolistOnUpdate(false);
					}
					else
						PopupMessage(Localize("Error"), Localize("Unable to delete the demo"), Localize("Ok"));
				}
			}
		}
		else if(m_Popup == POPUP_RENAME_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &Ok);

			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonOk = 0;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// rename demo
				if(m_DemolistSelectedIndex >= 0 && !m_DemolistSelectedIsDir)
				{
					char aBufOld[512];
					str_format(aBufOld,
							   sizeof(aBufOld),
							   "%s/%s",
							   m_aCurrentDemoFolder,
							   m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 4 || m_aCurrentDemoFile[Length - 5] != '.' ||
					   str_comp_nocase(m_aCurrentDemoFile + Length - 4, "demo"))
						str_format(aBufNew, sizeof(aBufNew), "%s/%s.demo", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					else
						str_format(aBufNew, sizeof(aBufNew), "%s/%s", m_aCurrentDemoFolder, m_aCurrentDemoFile);
					if(Storage()->RenameFile(aBufOld, aBufNew, m_lDemos[m_DemolistSelectedIndex].m_StorageType))
					{
						DemolistPopulate();
						DemolistOnUpdate(false);
					}
					else
						PopupMessage(Localize("Error"), Localize("Unable to rename the demo"), Localize("Ok"));
				}
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("New name:"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &Offset);
		}
		else if(m_Popup == POPUP_REMOVE_FRIEND)
		{
			CUIRect Yes, No;
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&No, &Yes);

			Yes.VMargin(20.0f, &Yes);
			No.VMargin(20.0f, &No);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("No"), 0, &No) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonTryAgain = 0;
			if(DoButton_Menu(&s_ButtonTryAgain, Localize("Yes"), 0, &Yes) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				// remove friend
				if(m_FriendlistSelectedIndex >= 0)
				{
					m_pClient->Friends()->RemoveFriend(m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aName,
													   m_lFriends[m_FriendlistSelectedIndex].m_pFriendInfo->m_aClan);
					FriendlistOnUpdate();
					Client()->ServerBrowserUpdate();
				}
			}
		}
		else if(m_Popup == POPUP_SLICE_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &Ok);

			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
			{
				m_Popup = POPUP_NONE;
				m_DemoSliceState = 0;
			}

			static int s_ButtonOk = 0;
			if(DoButton_Menu(&s_ButtonOk, Localize("Ok"), 0, &Ok) || m_EnterPressed)
			{
				m_Popup = POPUP_NONE;
				Client()->DemoSlice(m_aCurrentDemoFile);
				m_DemoSliceState = 0;
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("New name:"), 18.0f, -1);
			static float s_Offset = 0.0f;
			DoEditBox(&s_Offset, &TextBox, m_aCurrentDemoFile, sizeof(m_aCurrentDemoFile), 12.0f, &s_Offset);
		}
		else if(m_Popup == POPUP_RENDER_DEMO)
		{
			CUIRect Label, TextBox, Ok, Abort, FpsRow, Fps30, Fps60;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			Part.VSplitMid(&Abort, &Ok);
			Ok.VMargin(20.0f, &Ok);
			Abort.VMargin(20.0f, &Abort);

			static int s_ButtonAbort = 0;
			if(DoButton_Menu(&s_ButtonAbort, Localize("Abort"), 0, &Abort) || m_EscapePressed)
				m_Popup = POPUP_NONE;

			static int s_ButtonOk = 0;
			if(DoButton_Menu(&s_ButtonOk, Localize("Start"), 0, &Ok) || m_EnterPressed)
			{
				const char *pError = Client()->DemoPlayer_Play(m_aDemoRenderSource, m_DemoRenderStorageType);
				if(pError)
				{
					m_Popup = POPUP_NONE;
					PopupMessage(Localize("Error"),
								 str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"),
								 Localize("Ok"));
				}
				else if(!Client()->VideoStart(m_aVideoOutputName, g_Config.m_ClVideoFps))
				{
					Client()->Disconnect();
					m_Popup = POPUP_NONE;
					PopupMessage(Localize("Error"),
								 Localize("Failed to start video render. Is FFmpeg installed?"),
								 Localize("Ok"));
				}
				else
				{
					m_Popup = POPUP_NONE;
					SetActive(false);
					UI()->SetActiveItem(0);
				}
			}

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &FpsRow);
			FpsRow.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &FpsRow);
			FpsRow.VSplitLeft(20.0f, 0, &FpsRow);
			UI()->DoLabel(&Label, Localize("FPS:"), 18.0f, -1);
			FpsRow.VSplitLeft(80.0f, &Fps30, &FpsRow);
			FpsRow.VSplitLeft(10.0f, 0, &FpsRow);
			FpsRow.VSplitLeft(80.0f, &Fps60, 0);
			static int s_Fps30 = 0;
			static int s_Fps60 = 0;
			if(DoButton_Menu(&s_Fps30, "30", g_Config.m_ClVideoFps == 30, &Fps30))
				g_Config.m_ClVideoFps = 30;
			if(DoButton_Menu(&s_Fps60, "60", g_Config.m_ClVideoFps == 60, &Fps60))
				g_Config.m_ClVideoFps = 60;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(120.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("File name:"), 18.0f, -1);
			static float s_Offset = 0.0f;
			DoEditBox(&s_Offset, &TextBox, m_aVideoOutputName, sizeof(m_aVideoOutputName), 12.0f, &s_Offset);
		}
		else if(m_Popup == POPUP_TUTORIAL_PROMPT)
		{
			CUIRect Start, Skip;
			Box.HSplitBottom(28.0f, &Box, &Part);
			Part.VMargin(24.0f, &Part);
			Part.VSplitMid(&Start, &Skip);
			Start.VMargin(6.0f, &Start);
			Skip.VMargin(6.0f, &Skip);

			static int s_StartTraining, s_SkipTraining;
			if(DoButton_Menu(&s_StartTraining, Localize("Start training"), 0, &Start) || m_EnterPressed)
			{
				g_Config.m_ClTutorialPromptHandled = 1;
				m_Popup = POPUP_NONE;
				StartTutorial(1, false);
			}
			if(DoButton_Menu(&s_SkipTraining, Localize("Skip"), 0, &Skip) || m_EscapePressed)
			{
				g_Config.m_ClTutorialPromptHandled = 1;
				m_Popup = POPUP_FIRST_LAUNCH;
			}
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			rand();

			CUIRect Label, TextBox;

			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(80.0f, &Part);

			static int s_EnterButton = 0;
			if(DoButton_Menu(&s_EnterButton, Localize("Enter"), 0, &Part) || m_EnterPressed)
			{
				SetClientRandomSkin();
				m_Popup = POPUP_NONE;
			}

			Box.HSplitBottom(40.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Nickname"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(
				&g_Config.m_PlayerName, &TextBox, g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), 12.0f, &Offset);
		}
		else
		{
			Box.HSplitBottom(20.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);
			Part.VMargin(120.0f, &Part);

			static int s_Button = 0;
			if(DoButton_Menu(&s_Button, pButtonText, 0, &Part) || m_EscapePressed || m_EnterPressed)
				m_Popup = POPUP_NONE;
		}

		if(m_Popup == POPUP_NONE)
			UI()->SetActiveItem(0);
	}

	return 0;
}

void CMenus::SetClientRandomSkin()
{
	g_Config.m_PlayerColorSkin = 982985;
	g_Config.m_PlayerColorBody = rand() % (0xFFFFFF / 10) * 1000;
	;
	g_Config.m_PlayerColorFeet = rand() % (0xFFFFFF / 10) * 1000;
	;
	g_Config.m_PlayerColorTopper = rand() % (0xFFFFFF / 10) * 1000;

	str_copy(g_Config.m_PlayerBody, "default", 24);
	str_copy(g_Config.m_PlayerHead, "default", 24);
	str_copy(g_Config.m_PlayerHand, "default", 24);
	str_copy(g_Config.m_PlayerFoot, "default", 24);

	switch(rand() % 7)
	{
		case 0:
			str_copy(g_Config.m_PlayerTopper, "basic", 24);
			break;
		case 1:
			str_copy(g_Config.m_PlayerTopper, "casual", 24);
			break;
		case 2:
			str_copy(g_Config.m_PlayerTopper, "dr", 24);
			break;
		case 3:
			str_copy(g_Config.m_PlayerTopper, "emo", 24);
			break;
		case 4:
			str_copy(g_Config.m_PlayerTopper, "nerd2", 24);
			break;
		case 5:
			str_copy(g_Config.m_PlayerTopper, "nerd", 24);
			break;
		default:
			str_copy(g_Config.m_PlayerTopper, "default", 24);
	};

	switch(rand() % 5)
	{
		case 0:
			str_copy(g_Config.m_PlayerEye, "cyan", 24);
			break;
		case 1:
			str_copy(g_Config.m_PlayerEye, "lsd", 24);
			break;
		case 2:
			str_copy(g_Config.m_PlayerEye, "sleepy", 24);
			break;
		case 3:
			str_copy(g_Config.m_PlayerEye, "diag", 24);
			break;
		default:
			str_copy(g_Config.m_PlayerEye, "default", 24);
	};
}

void CMenus::SetActive(bool Active)
{
	if(Active && !m_MenuActive)
		m_MenuOpenTransition = 0.0f;
	m_MenuActive = Active;
	if(!m_MenuActive)
	{
		CLineInput *pActiveInput = CLineInput::GetActiveInput();
		if(pActiveInput && UI()->ActiveItem() == pActiveInput)
			pActiveInput->Deactivate();
		UI()->SetActiveItem(0);
		UI()->SetHotItem(0);
		UI()->ClearLastActiveItem();
		m_EscapePressed = false;
		m_EnterPressed = false;
		m_DeletePressed = false;
		m_NumInputEvents = 0;

		if(m_NeedSendinfo)
		{
			m_pClient->SendInfo(false);
			m_NeedSendinfo = false;
		}

		if(Client()->State() == IClient::STATE_ONLINE)
		{
			m_pClient->OnRelease();
		}
	}
	else if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		m_pClient->OnRelease();
	}
}

void CMenus::OnReset()
{
	for(int i = 0; i < 32; i++)
	{
		if(m_aWorkshopPreviews[i].m_Texture >= 0)
			Graphics()->UnloadTexture(m_aWorkshopPreviews[i].m_Texture);
		m_aWorkshopPreviews[i].m_PublishedFileID = 0;
		m_aWorkshopPreviews[i].m_Texture = -1;
		m_aWorkshopPreviews[i].m_OperationID = 0;
	}
	m_WorkshopDetailOpen = false;
}

bool CMenus::OnMouseMove(float x, float y)
{
	m_LastInput = time_get();
	m_LastInputDevice = 0;
	m_NavigationHasFocus = false;

	if(!m_MenuActive)
		return false;

	Input()->SetMouseModes(0);
	Input()->ShowCursor(g_Config.m_InpHWCursor);

	// prev mouse position
	m_PrevMousePos = m_MousePos;

	UI()->ConvertMouseMove(&x, &y);
	m_MousePos.x = x;
	m_MousePos.y = y;

	return true;
}

bool CMenus::OnInput(IInput::CEvent e)
{
	m_LastInput = time_get();
	if(e.m_Key >= KEY_GAMEPAD_BUTTON_A && e.m_Key < KEY_LAST)
		m_LastInputDevice = 2;
	else if(e.m_Key < KEY_MOUSE_1 || e.m_Key > KEY_MOUSE_WHEEL_DOWN)
		m_LastInputDevice = 1;

	// special handle esc and enter for popup purposes
	if(e.m_Flags & IInput::FLAG_PRESS)
	{
		if(e.m_Key == KEY_GAMEPAD_BUTTON_B && IsActive())
		{
			m_EscapePressed = true;
			const bool LocalGameSubpage =
				m_GamePage == PAGE_LOCAL_SERVER && m_CreateRoomStep == CREATE_ROOM_CONFIGURE &&
				!(g_Config.m_ClTutorialActive && g_Config.m_ClTutorialChapter == TUTORIAL_CHAPTER_MULTIPLAYER);
			// Popups consume B/Escape first. Their render handlers clear or dismiss
			// themselves below, so a confirmation cannot accidentally close the menu.
			if(m_Popup == POPUP_NONE && Client()->State() != IClient::STATE_OFFLINE && !LocalGameSubpage)
				SetActive(false);
			return true;
		}
		if(e.m_Key == KEY_ESCAPE && !CustomStuff()->m_Inventory)
		{
			if(Client()->IsRecordingVideo())
			{
				Client()->VideoStop();
				Client()->Disconnect();
				SetActive(true);
				PopupMessage(Localize("Render video"), Localize("Video render cancelled."), Localize("Ok"));
				return true;
			}
			if(IsActive())
			{
				m_EscapePressed = true;
				const bool LocalGameSubpage =
					m_GamePage == PAGE_LOCAL_SERVER && m_CreateRoomStep == CREATE_ROOM_CONFIGURE &&
					!(g_Config.m_ClTutorialActive && g_Config.m_ClTutorialChapter == TUTORIAL_CHAPTER_MULTIPLAYER);
				if(m_Popup == POPUP_NONE && Client()->State() != IClient::STATE_OFFLINE && !LocalGameSubpage)
					SetActive(false);
			}
			else
				SetActive(true);
			return true;
		}
	}

	if(IsActive())
	{
		if(UI()->OnInput(e))
			return true;

		if(e.m_Flags & IInput::FLAG_PRESS)
		{
			// special for popups
			if(e.m_Key == KEY_RETURN || e.m_Key == KEY_KP_ENTER || e.m_Key == KEY_GAMEPAD_BUTTON_A)
				m_EnterPressed = true;
			else if(e.m_Key == KEY_DELETE)
				m_DeletePressed = true;
		}

		if(m_NumInputEvents < MAX_INPUTEVENTS)
			m_aInputEvents[m_NumInputEvents++] = e;
		return true;
	}
	return false;
}

void CMenus::OnStateChange(int NewState, int OldState)
{
	// reset active item
	UI()->SetActiveItem(0);

	if(NewState == IClient::STATE_OFFLINE)
	{
		if(OldState >= IClient::STATE_ONLINE && NewState < IClient::STATE_QUITING)
			m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
		m_Popup = POPUP_NONE;
		if(Client()->ErrorString() && Client()->ErrorString()[0] != 0)
		{
			if(str_find(Client()->ErrorString(), "password"))
			{
				m_Popup = POPUP_PASSWORD;
				UI()->SetHotItem(&g_Config.m_Password);
				UI()->SetActiveItem(&g_Config.m_Password);
			}
			else
				m_Popup = POPUP_DISCONNECTED;
		}
	}
	else if(NewState == IClient::STATE_LOADING)
	{
		m_Popup = POPUP_CONNECTING;
		m_DownloadLastCheckTime = time_get();
		m_DownloadLastCheckSize = 0;
		m_DownloadSpeed = 0.0f;
		// client_serverinfo_request();
	}
	else if(NewState == IClient::STATE_CONNECTING)
		m_Popup = POPUP_CONNECTING;
	else if(NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
	{
		m_Popup = POPUP_NONE;
		SetActive(false);
	}
}

extern "C" void font_debug_render();

void CMenus::OnRender()
{
	UpdateLocalServer();
	PumpCloudProfile(false);

	/*
	// text rendering test stuff
	render_background();

	CTextCursor cursor;
	TextRender()->SetCursor(&cursor, 10, 10, 20, TEXTFLAG_RENDER);
	TextRender()->TextEx(&cursor, "ようこそ - ガイド", -1);

	TextRender()->SetCursor(&cursor, 10, 30, 15, TEXTFLAG_RENDER);
	TextRender()->TextEx(&cursor, "ようこそ - ガイド", -1);

	//Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->QuadsDrawTL(60, 60, 5000, 5000);
	Graphics()->QuadsEnd();
	return;*/

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		SetActive(true);

	if(Client()->ConsumeVideoFinished())
	{
		SetActive(true);
		PopupMessage(Localize("Render video"), Localize("Video render complete. Saved under videos/."), Localize("Ok"));
	}

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && !Client()->IsRecordingVideo())
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
		RenderDemoPlayer(Screen);
	}

	/*
	if(Client()->State() == IClient::STATE_ONLINE && m_pClient->m_ServerMode == m_pClient->SERVERMODE_PUREMOD)
	{
		Client()->Disconnect();
		SetActive(true);
		m_Popup = POPUP_PURE;
	}
	*/

	if(!IsActive())
	{
		m_EscapePressed = false;
		m_EnterPressed = false;
		m_DeletePressed = false;
		m_NumInputEvents = 0;
		return;
	}

	// Update the shared tactical-glass design tokens.
	ms_GuiColor = vec4(0.008f, 0.020f, 0.038f, 0.95f);
	const float A = MenuAlpha();
	ms_ColorBgDeep = vec4(0.008f, 0.020f, 0.038f, 0.92f * A);
	ms_ColorBgPanel = vec4(0.030f, 0.067f, 0.098f, 0.78f * A);
	ms_ColorBgInset = vec4(0.018f, 0.045f, 0.071f, 0.72f * A);
	ms_ColorAccent = vec4(0.22f, 0.88f, 1.00f, 1.0f);	  // primary interaction / focus
	ms_ColorAccentDim = vec4(0.42f, 0.96f, 0.72f, 1.0f);  // online / trusted state
	ms_ColorAccentWarm = vec4(1.00f, 0.69f, 0.24f, 1.0f); // tactical warning / high energy
	ms_ColorDanger = vec4(1.00f, 0.25f, 0.38f, 1.0f);
	ms_ColorText = vec4(0.92f, 0.97f, 1.00f, 1.0f);
	ms_ColorGlassLine = vec4(0.72f, 0.94f, 1.00f, 0.18f);

	ms_ColorTabbarInactiveOutgame = vec4(0.018f, 0.060f, 0.090f, 0.76f * A);
	ms_ColorTabbarActiveOutgame = vec4(0.025f, 0.170f, 0.220f, 0.92f * A);
	ms_ColorTabbarInactiveIngame = vec4(0.018f, 0.060f, 0.090f, 0.80f * A);
	ms_ColorTabbarActiveIngame = vec4(0.025f, 0.170f, 0.220f, 0.94f * A);

	// update the ui
	CUIRect *pScreen = UI()->Screen();
	float mx = (m_MousePos.x / (float)Graphics()->ScreenWidth()) * pScreen->w;
	float my = (m_MousePos.y / (float)Graphics()->ScreenHeight()) * pScreen->h;

	int Buttons = 0;
	if(m_UseMouseButtons)
	{
		if(Input()->KeyPressed(KEY_MOUSE_1))
			Buttons |= 1;
		if(Input()->KeyPressed(KEY_MOUSE_2))
			Buttons |= 2;
		if(Input()->KeyPressed(KEY_MOUSE_3))
			Buttons |= 4;
	}

	UI()->Update(mx, my, mx * 3.0f, my * 3.0f, Buttons);

	// render
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		Render();
	m_pClient->m_pPveRoguelite->RenderMenuDebugOverlay();
	m_pClient->m_pPveRoguelite->RenderBuildDebug();

	// render cursor
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem QuadItem(mx, my, 24, 24);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	// render debug information
	if(g_Config.m_Debug)
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "%p %p %p", UI()->HotItem(), UI()->ActiveItem(), UI()->LastActiveItem());
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, 10, 10, 10, TEXTFLAG_RENDER);
		TextRender()->TextEx(&Cursor, aBuf, -1);
	}

	m_EscapePressed = false;
	m_EnterPressed = false;
	m_DeletePressed = false;
	m_NumInputEvents = 0;
}

static float s_ShaderIntensity = 0.1f;

void CMenus::RenderBackground()
{
	// menu timestep

	int64 currentTime = time_get();
	if((currentTime - m_LastUpdate > time_freq()) || (m_LastUpdate == 0))
		m_LastUpdate = currentTime;

	int step = time_freq() / 60;

	if(step <= 0)
		step = 1;

	int i = 0;

	for(; m_LastUpdate < currentTime; m_LastUpdate += step)
	{
		if(Client()->Loaded())
			s_ShaderIntensity += 0.05f;

		if(i++ > 1)
		{
			m_LastUpdate = currentTime;
			break;
		}
	}

	// menu background effect
	vec2 s = vec2(Graphics()->ScreenWidth(), Graphics()->ScreenHeight()) / 8;
	Graphics()->MapScreen(0, 0, s.x, s.y);
	const bool UseMenuShader = g_Config.m_GfxMultiBuffering && Client()->Loaded() &&
		Graphics()->IsShaderAvailable(SHADER_MENU);

	if(UseMenuShader)
	{
		// render background shader
		Graphics()->RenderToTexture(RENDERBUFFER_MENU);
		Graphics()->ShaderBegin(SHADER_MENU, s_ShaderIntensity);
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		IGraphics::CQuadItem QuadItem = IGraphics::CQuadItem(0, 0, s.x, s.y);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
		Graphics()->ShaderEnd();
		Graphics()->RenderToScreen();
	}
	Graphics()->RenderToScreen();

	// render background color
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	vec4 Top(0.006f, 0.018f, 0.038f, 1.0f);
	vec4 Bottom(0.002f, 0.008f, 0.020f, 1.0f);
	IGraphics::CColorVertex Array[4] = {IGraphics::CColorVertex(0, Top.r, Top.g, Top.b, Top.a),
										IGraphics::CColorVertex(1, Top.r, Top.g, Top.b, Top.a),
										IGraphics::CColorVertex(2, Bottom.r, Bottom.g, Bottom.b, Bottom.a),
										IGraphics::CColorVertex(3, Bottom.r, Bottom.g, Bottom.b, Bottom.a)};
	Graphics()->SetColorVertex(Array, 4);
	IGraphics::CQuadItem QuadItem(0, 0, s.x, s.y);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	if(UseMenuShader)
	{
		Graphics()->TextureSet(-2, RENDERBUFFER_MENU);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		QuadItem = IGraphics::CQuadItem(0, 0, s.x, s.y);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}

	// Modular translucent planes echo construction blocks, while the angled
	// cuts maintain the interface's tactical rhythm without external assets.
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .035f);
	IGraphics::CFreeformItem LeftPlane(0.0f, s.y * .62f, s.x * .18f, s.y * .47f, 0.0f, s.y, s.x * .46f, s.y);
	Graphics()->QuadsDrawFreeform(&LeftPlane, 1);
	Graphics()->SetColor(ms_ColorAccentWarm.r, ms_ColorAccentWarm.g, ms_ColorAccentWarm.b, .022f);
	IGraphics::CFreeformItem RightPlane(s.x * .72f, 0.0f, s.x, 0.0f, s.x * .56f, s.y * .34f, s.x, s.y * .19f);
	Graphics()->QuadsDrawFreeform(&RightPlane, 1);
	Graphics()->QuadsEnd();

	const float Drift = fmodf(Client()->LocalTime() * 2.4f, 18.0f);
	IGraphics::CLineItem aGrid[96];
	int GridNum = 0;
	for(float x = -18.0f + Drift; x < s.x && GridNum < 96; x += 18.0f)
		aGrid[GridNum++] = IGraphics::CLineItem(x, 0.0f, x, s.y);
	for(float y = -18.0f + Drift * .42f; y < s.y && GridNum < 96; y += 18.0f)
		aGrid[GridNum++] = IGraphics::CLineItem(0.0f, y, s.x, y);
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .045f);
	Graphics()->LinesDraw(aGrid, GridNum);
	Graphics()->LinesEnd();

	GridNum = 0;
	for(float x = -72.0f + Drift; x < s.x && GridNum < 96; x += 72.0f)
		aGrid[GridNum++] = IGraphics::CLineItem(x, 0.0f, x, s.y);
	for(float y = -72.0f + Drift * .42f; y < s.y && GridNum < 96; y += 72.0f)
		aGrid[GridNum++] = IGraphics::CLineItem(0.0f, y, s.x, y);
	Graphics()->LinesBegin();
	Graphics()->SetColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .085f);
	Graphics()->LinesDraw(aGrid, GridNum);
	Graphics()->LinesEnd();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .055f);
	const float ScanY = fmodf(Client()->LocalTime() * 5.0f, max(1.0f, s.y));
	IGraphics::CQuadItem Scan(0.0f, ScanY, s.x, 0.45f);
	Graphics()->QuadsDrawTL(&Scan, 1);
	Graphics()->QuadsEnd();

	// restore screen
	{
		CUIRect Screen = *UI()->Screen();
		Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
	}
}
