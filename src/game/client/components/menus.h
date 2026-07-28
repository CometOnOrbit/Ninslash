

#ifndef GAME_CLIENT_COMPONENTS_MENUS_H
#define GAME_CLIENT_COMPONENTS_MENUS_H

#include <base/system.h>
#include <base/vmath.h>
#include <base/tl/sorted_array.h>

#include <engine/demo.h>
#include <engine/friends.h>
#include <engine/platform_services.h>
#include <engine/serverbrowser.h>

#include <game/voting.h>
#include <game/client/component.h>
#include <game/client/cloud_profile.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>


// compnent to fetch keypresses, override all other input
class CMenusKeyBinder : public CComponent
{
public:
	bool m_TakeKey;
	bool m_GotKey;
	IInput::CEvent m_Key;
	CMenusKeyBinder();
	virtual bool OnInput(IInput::CEvent Event);
};

// UI-only adapter. Dedicated servers remain keyed/deduplicated by endpoint;
// Steam rooms remain keyed by LobbyID.
struct CPlayRoomEntry
{
	enum ESource { SOURCE_DEDICATED, SOURCE_STEAM_LOBBY };
	int m_Source;
	char m_aStableID[128];
	const struct CPlayServerSnapshot *m_pServer;
	const struct CPlayLobbySnapshot *m_pLobby;
};

// The server browser exposes one UDP collection at a time. Keep a compact,
// menu-only snapshot for each collection so Play can present a unified view.
struct CPlayServerSnapshot
{
	NETADDR m_NetAddr;
	int m_Collection;
	int m_MaxClients;
	int m_NumClients;
	int m_Flags;
	int m_Latency;
	int m_DiscoverySources;
	int m_AuthPolicy;
	bool m_Official;
	bool m_Modded;
	bool m_Favorite;
	char m_aAddress[NETADDR_MAXSTRSIZE];
	char m_aName[64];
	char m_aGameType[16];
	char m_aMap[32];
	char m_aVersion[32];
};

struct CPlayLobbySnapshot
{
	CPlatformLobbyInfo m_Info;
};

class CMenus : public CComponent
{
	static vec4 ms_GuiColor;
	static vec4 ms_ColorTabbarInactiveOutgame;
	static vec4 ms_ColorTabbarActiveOutgame;
	static vec4 ms_ColorTabbarInactiveIngame;
	static vec4 ms_ColorTabbarActiveIngame;
	static vec4 ms_ColorTabbarInactive;
	static vec4 ms_ColorTabbarActive;

	// dark-punk palette
	static vec4 ms_ColorBgDeep;
	static vec4 ms_ColorBgPanel;
	static vec4 ms_ColorBgInset;
	static vec4 ms_ColorAccent;
	static vec4 ms_ColorAccentDim;
	static vec4 ms_ColorDanger;
	static vec4 ms_ColorText;
	static float ms_PanelRounding;
	static float ms_ControlRounding;

	enum
	{
		BUTTONSTYLE_NORMAL = 0,
		BUTTONSTYLE_ACCENT = 1,
		BUTTONSTYLE_DANGER = 2,
	};

	vec4 ButtonColorMul(const void *pID);
	float MenuAlpha() const;
	void DrawMenuPanel(const CUIRect *pRect, int Corners = CUI::CORNER_ALL);
	void DrawMenuInset(const CUIRect *pRect, int Corners = CUI::CORNER_ALL);
	void DrawSectionHeader(const CUIRect *pRect, int Corners = CUI::CORNER_T);
	void DrawAccentUnderline(const CUIRect *pRect);
	void DrawMenuBorder(const CUIRect *pRect, const vec4 &Fill, const vec4 &Border, int Corners, float Rounding);
	void ConfigureScrollRegion(CScrollRegionParams *pParams) const;
	void LayoutCenterPanel(CUIRect *pScreen, CUIRect *pOut);
	void DrawNavigationIcon(const CUIRect &Rect, int Icon, bool Active);
	void DrawPlayArtwork(const CUIRect &Rect, int Mode, const vec4 &Color);
	void DrawModeVoteImage(const CUIRect &Rect, const char *pImage, bool Active);
	void DrawStatusBadge(CUIRect Rect, const char *pText, const vec4 &Color);

	float AnimSelected(const void *pID, bool Selected, float Speed = 12.0f);
	static vec4 MixColor(const vec4 &A, const vec4 &B, float t);

	int64 m_LastUpdate;

	int DoButton_DemoPlayer(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_Sprite(const void *pID, int ImageID, int SpriteID, int Checked, const CUIRect *pRect, int Corners);
	int DoButton_Toggle(const void *pID, int Checked, const CUIRect *pRect, bool Active);
	int DoButton_Menu(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Style = BUTTONSTYLE_NORMAL);
	int DoButton_MenuTab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners);

	int DoButton_CheckBox_Common(const void *pID, const char *pText, const char *pBoxText, const CUIRect *pRect);
	int DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect);

	/*static void ui_draw_menu_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_keyselect_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_menu_tab_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_settings_tab_button(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	*/

	int DoButton_Icon(int ImageId, int SpriteId, const CUIRect *pRect);
	int DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect, bool Interactive = true);

	//static void ui_draw_browse_icon(int what, const CUIRect *r);
	//static void ui_draw_grid_header(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);

	/*static void ui_draw_checkbox_common(const void *id, const char *text, const char *boxtext, const CUIRect *r, const void *extra);
	static void ui_draw_checkbox(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	static void ui_draw_checkbox_number(const void *id, const char *text, int checked, const CUIRect *r, const void *extra);
	*/
	int DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden=false, int Corners=CUI::CORNER_ALL);
	//static int ui_do_edit_box(void *id, const CUIRect *rect, char *str, unsigned str_size, float font_size, bool hidden=false);

	float DoScrollbarV(const void *pID, const CUIRect *pRect, float Current);
	float DoScrollbarH(const void *pID, const CUIRect *pRect, float Current);

	typedef float (CMenus::*FDropdownCallback)(CUIRect View);
	float DoIndependentDropdownMenu(void *pID, CUIRect *pRect, const char *pStr, float HeaderHeight, FDropdownCallback pfnCallback, bool *pActive);
	float RenderSettingsControlsMovement(CUIRect View);
	float RenderSettingsControlsWeapons(CUIRect View);
	float RenderSettingsControlsVoting(CUIRect View);
	float RenderSettingsControlsChat(CUIRect View);
	float RenderSettingsControlsMisc(CUIRect View);
	void DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect);
	int DoKeyReader(void *pID, const CUIRect *pRect, int Key);

	// When set, interactive widgets skip hit-testing if scrolled out of the clip rect
	CScrollRegion *m_pUiClipScrollRegion;

	//static int ui_do_key_reader(void *id, const CUIRect *rect, int key);
	void UiDoGetButtons(int Start, int Stop, CUIRect View);

	struct CListboxItem
	{
		int m_Visible;
		int m_Selected;
		CUIRect m_Rect;
		CUIRect m_HitRect;
	};

	void UiDoListboxStart(const void *pID, const CUIRect *pRect, float RowHeight, const char *pTitle, const char *pBottomText, int NumItems,
						int ItemsPerRow, int SelectedIndex, float ScrollValue);
	CListboxItem UiDoListboxNextItem(const void *pID, bool Selected = false, bool Interactive = true);
	CListboxItem UiDoListboxNextRow();
	int UiDoListboxEnd(float *pScrollValue, bool *pItemActivated);

	//static void demolist_listdir_callback(const char *name, int is_dir, void *user);
	//static void demolist_list_callback(const CUIRect *rect, int index, void *user);

	enum
	{
		POPUP_NONE=0,
		POPUP_FIRST_LAUNCH,
		POPUP_CONNECTING,
		POPUP_MESSAGE,
		POPUP_DISCONNECTED,
		POPUP_PURE,
		POPUP_LANGUAGE,
		POPUP_COUNTRY,
		POPUP_DELETE_DEMO,
		POPUP_RENAME_DEMO,
		POPUP_REMOVE_FRIEND,
		POPUP_SOUNDERROR,
		POPUP_PASSWORD,
		POPUP_QUIT,
		POPUP_SLICE_DEMO,
		POPUP_RENDER_DEMO,
		POPUP_TUTORIAL_EXIT,
		POPUP_CLOUD_CONFLICT,
	};

	enum
	{
		PAGE_FRONT=1,
		PAGE_NEWS,
		PAGE_GAME,
		PAGE_PLAYERS,
		PAGE_SERVER_INFO,
		PAGE_CALLVOTE,
		PAGE_INTERNET,
		PAGE_LAN,
		PAGE_FAVORITES,
		PAGE_DEMOS,
		PAGE_SETTINGS,
		PAGE_CUSTOMIZE,
		PAGE_SYSTEM,
		PAGE_RESEARCH,
		PAGE_LOCAL_SERVER,
		PAGE_STEAM,
		PAGE_MODS,
		PAGE_TUTORIAL_SELECT,
	};

	enum
	{
		LOCAL_SERVER_STOPPED = 0,
		LOCAL_SERVER_STARTING,
		LOCAL_SERVER_RUNNING,
		LOCAL_SERVER_STOPPING,
		LOCAL_SERVER_FAILED,
	};

	int m_GamePage;
	int m_Popup;
	bool m_CloudInitialized;
	bool m_CloudConflict;
	bool m_CloudPaused;
	bool m_CloudDirty;
	int64 m_CloudNextCheck;
	int m_CloudRevision;
	unsigned long long m_CloudSyncedHash;
	CCloudProfileSummary m_CloudLocalSummary;
	CCloudProfileSummary m_CloudRemoteSummary;
	char m_aCloudLocalProfile[64 * 1024];
	char m_aCloudRemoteProfile[64 * 1024];
	char m_aCloudStatus[192];
	void InitCloudProfile();
	void PumpCloudProfile(bool Force);
	bool UploadCloudProfile();
	void ResolveCloudConflict(bool UseRemote);
	void SaveCloudSyncState(unsigned long long Hash, int Revision);
	void BackupCloudProfile(const char *pData, const char *pSuffix);
	int m_ActivePage;
	int m_NavigationFocus;
	int m_LastInputDevice;
	int m_PlayTab;
	int m_CreateRoomStep;
	int m_CreateRoomPreviousSlots;
	bool m_NavigationHasFocus;
	bool m_MenuActive;
	bool m_UseMouseButtons;
	vec2 m_MousePos;
	vec2 m_PrevMousePos;

	PROCESS m_LocalServerProcess;
	int m_LocalServerState;
	int m_LocalServerExitCode;
	int64 m_LocalServerStateTime;
	int64 m_LocalServerJoinRetryTime;
	int64 m_LocalServerInfoRequestTime;
	int m_LocalServerJoinAttempts;
	int m_LocalServerActualPort;
	bool m_LocalServerAutoJoin;
	bool m_LocalServerRestartPending;
	bool m_TutorialChapterReplay;
	bool m_LocalServerSummaryLocalized;
	int m_LocalServerFocus;
	NETADDR m_LocalServerAddress;
	char m_aLocalServerJoinAddress[NETADDR_MAXSTRSIZE];
	char m_aLocalServerPassword[32];
	char m_aLocalServerSummary[512];
	char m_aLocalServerLogPath[512];
	char m_aLocalServerErrorDetail[256];

	int64 m_LastInput;

	// loading
	int m_LoadCurrent;
	int m_LoadTotal;

	//
	char m_aMessageTopic[512];
	char m_aMessageBody[512];
	char m_aMessageButton[512];

	void PopupMessage(const char *pTopic, const char *pBody, const char *pButton);

	// TODO: this is a bit ugly but.. well.. yeah
	enum { MAX_INPUTEVENTS = 32 };
	static IInput::CEvent m_aInputEvents[MAX_INPUTEVENTS];
	static int m_NumInputEvents;

	// some settings
	static float ms_ButtonHeight;
	static float ms_ListheaderHeight;
	static float ms_FontmodHeight;

	// for settings
	bool m_NeedRestartGraphics;
	bool m_NeedRestartSound;
	bool m_NeedSendinfo;
	int m_SettingPlayerPage;

	// video modes (aligned with Teeworlds Recommended / Other)
	enum
	{
		MAX_RESOLUTIONS = 256,
	};
	CVideoMode m_aModes[MAX_RESOLUTIONS];
	int m_NumModes;
	sorted_array<CVideoMode> m_lRecommendedVideoModes;
	sorted_array<CVideoMode> m_lOtherVideoModes;
	void UpdatedFilteredVideoModes();
	void UpdateVideoModeSettings();
	bool DoResolutionList(CUIRect *pRect, const void *pID, float *pScrollValue, const sorted_array<CVideoMode> &lModes);

	//
	bool m_EscapePressed;
	bool m_EnterPressed;
	bool m_DeletePressed;

	// for map download popup
	int64 m_DownloadLastCheckTime;
	int m_DownloadLastCheckSize;
	float m_DownloadSpeed;

	// for call vote
	int m_CallvoteSelectedOption;
	int m_CallvoteSelectedPlayer;
	char m_aCallvoteReason[VOTE_REASON_LENGTH];

	// demo
	struct CDemoItem
	{
		char m_aFilename[128];
		char m_aName[128];
		bool m_IsDir;
		int m_StorageType;

		bool m_InfosLoaded;
		bool m_Valid;
		CDemoHeader m_Info;

		bool operator<(const CDemoItem &Other) { return !str_comp(m_aFilename, "..") ? true : !str_comp(Other.m_aFilename, "..") ? false :
														m_IsDir && !Other.m_IsDir ? true : !m_IsDir && Other.m_IsDir ? false :
														str_comp_filenames(m_aFilename, Other.m_aFilename) < 0; }
	};

	sorted_array<CDemoItem> m_lDemos;
	char m_aCurrentDemoFolder[256];
	char m_aCurrentDemoFile[64];
	char m_aDemoRenderSource[512];
	char m_aVideoOutputName[128];
	int m_DemoRenderStorageType;
	int m_DemolistSelectedIndex;
	bool m_DemolistSelectedIsDir;
	int m_DemolistStorageType;

	// demo slicing
	int m_DemoSliceState;
	int m_DemoSliceStartTick;
	int m_DemoSliceEndTick;

	void DemolistOnUpdate(bool Reset);
	void DemolistPopulate();
	static int DemolistFetchCallback(const char *pName, int IsDir, int StorageType, void *pUser);

	// friends
	struct CFriendItem
	{
		const CFriendInfo *m_pFriendInfo;
		int m_NumFound;

		bool operator<(const CFriendItem &Other)
		{
			if(m_NumFound && !Other.m_NumFound)
				return true;
			else if(!m_NumFound && Other.m_NumFound)
				return false;
			else
			{
				int Result = str_comp(m_pFriendInfo->m_aName, Other.m_pFriendInfo->m_aName);
				if(Result)
					return Result < 0;
				else
					return str_comp(m_pFriendInfo->m_aClan, Other.m_pFriendInfo->m_aClan) < 0;
			}
		}
	};

	sorted_array<CFriendItem> m_lFriends;
	int m_FriendlistSelectedIndex;

	void FriendlistOnUpdate();

	// found in menus.cpp
	int Render();
	//void render_background();
	//void render_loading(float percent);
	int RenderMenubar(CUIRect r);
	void RenderNews(CUIRect MainView);

	// found in menus_demo.cpp
	void RenderDemoPlayer(CUIRect MainView);
	void RenderDemoList(CUIRect MainView);

	// found in menus_ingame.cpp
	void RenderGame(CUIRect MainView);
	void RenderPlayers(CUIRect MainView);
	void RenderServerInfo(CUIRect MainView);
	void RenderServerControl(CUIRect MainView);
	void RenderServerControlKick(CUIRect MainView, bool FilterSpectators);
	void RenderServerControlServer(CUIRect MainView);

	// found in menus_browser.cpp
	int m_SelectedIndex;
	int m_ScrollOffset;

	struct CUiFilterPreset
	{
		char m_aName[32];
		bool m_Used;

		char m_aFilterString[25];
		int m_FilterFull;
		int m_FilterEmpty;
		int m_FilterSpectators;
		int m_FilterFriends;
		int m_FilterCountry;
		int m_FilterCountryIndex;
		int m_FilterPw;
		int m_FilterPing;
		char m_aFilterGametype[128];
		int m_FilterGametypeStrict;
		char m_aFilterServerAddress[128];
		int m_FilterPure;
		int m_FilterPureMap;
		int m_FilterCompatversion;
	};

	enum
	{
		UI_FILTER_PRESET_ALL = 0,
		UI_FILTER_PRESET_FAVORITES,
		UI_FILTER_PRESET_CUSTOM_START,
		MAX_UI_FILTER_CUSTOM_PRESETS = 8,
		NUM_UI_FILTER_PRESETS = 2 + MAX_UI_FILTER_CUSTOM_PRESETS,
	};

	CUiFilterPreset m_aFilterPresets[NUM_UI_FILTER_PRESETS];
	int m_ActiveFilterPreset;
	int m_FilterPresetRenameSlot;
	char m_aFilterPresetRenameBuf[32];

	enum
	{
		PLAY_COLLECTION_INTERNET = 0,
		PLAY_COLLECTION_LAN,
		PLAY_COLLECTION_FAVORITES,
		NUM_PLAY_COLLECTIONS,
		MAX_PLAY_SERVER_SNAPSHOTS = 256,
		MAX_PLAY_LOBBY_SNAPSHOTS = 256,
	};
	CPlayServerSnapshot m_aaPlayServerSnapshots[NUM_PLAY_COLLECTIONS][MAX_PLAY_SERVER_SNAPSHOTS];
	int m_aPlayServerSnapshotCount[NUM_PLAY_COLLECTIONS];
	CPlayLobbySnapshot m_aPlayLobbySnapshots[MAX_PLAY_LOBBY_SNAPSHOTS];
	int m_PlayLobbySnapshotCount;
	int m_PlayBrowserCollection;
	char m_aPlaySelectedID[128];
	bool m_PlayFiltersOpen;
	bool m_PlayFiltersAdvanced;
	bool m_FilterPresetMenuOpen;
	bool m_PlayDetailOpen;
	bool m_PlayListHasFocus;

	void UpdatePlaySnapshots();

	void LoadFilterPresets();
	void SaveFilterPresets();
	void SnapshotConfigToFilterPreset(int Slot);
	void ApplyFilterPresetToConfig(int Slot);
	void SwitchFilterPreset(int NewSlot);
	void RenderFilterPresetBar(CUIRect View);

	void RenderServerbrowserServerList(CUIRect View);
	void RenderServerbrowserServerDetail(CUIRect View);
	void RenderServerbrowserFilters(CUIRect View);
	void RenderServerbrowserFriends(CUIRect View);
	void RenderServerbrowser(CUIRect MainView);
	void RenderSteam(CUIRect MainView);
	void RenderPlay(CUIRect MainView);
	void RenderMods(CUIRect MainView);
	static void ConchainFriendlistUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainServerbrowserUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	// found in menus_settings.cpp
	void RenderLanguageSelection(CUIRect MainView);
	void RenderSettingsGeneral(CUIRect MainView);
	void RenderSettingsPlayer(CUIRect MainView);
	void RenderCustomization(CUIRect MainView);
	void RenderSettingsControls(CUIRect MainView);
	void RenderSettingsGraphics(CUIRect MainView);
	void RenderSettingsSound(CUIRect MainView);
	void RenderSettingsCloud(CUIRect MainView);
	void RenderSettingsGamepad(CUIRect MainView);
	void RenderSettingsCustom(CUIRect MainView);
	void RenderSettings(CUIRect MainView);
	void RenderCustomize(CUIRect MainView);
	void RenderFront(CUIRect MainView);
	void RenderTutorialRoomPractice(CUIRect MainView);
	void RenderLocalServer(CUIRect MainView);
	void RenderCreateRoom(CUIRect MainView);
	void CreateConfiguredRoom();
	void UpdateLocalServer();
	void StartLocalServer(bool AutoJoin);
	void StopLocalServer(bool Restart);
	void StartTutorial(int Chapter = 1, bool Resume = true);
	void StartPvpPractice();
	void JoinLocalServer();
	bool IsConnectedToLocalServer() const;
	void RefreshLocalServerErrorDetail();
	static void ConLocalGameStart(IConsole::IResult *pResult, void *pUserData);
	static void ConLocalGameStop(IConsole::IResult *pResult, void *pUserData);
	static void ConLocalGameRestart(IConsole::IResult *pResult, void *pUserData);

	void SetActive(bool Active);
	
	void SetClientRandomSkin();
	
	void SaveSkin();
	
public:
	void RenderBackground();
	void RenderTutorialChapterSelect(CUIRect MainView);

	void UseMouseButtons(bool Use) { m_UseMouseButtons = Use; }

	static CMenusKeyBinder m_Binder;

	CMenus();

	static vec4 ThemeBgDeep();
	static vec4 ThemeBgPanel();
	static vec4 ThemeBgInset();
	static vec4 ThemeAccent();
	static vec4 ThemeAccentDim();
	static vec4 ThemeDanger();
	static vec4 ThemeText();
	float AnimHover(const void *pID, float Speed = 14.0f);
	void OpenResearchPage();
	void OpenTutorialChapterSelect();
	void HandleTutorialChapterCompleted(int Chapter, int CompletedMask);
	void FinishTutorial();
	void ShutdownLocalServer();
	void OpenTutorialRoomPractice();
	void OpenPlayHub();

	void RenderLoading();

	bool IsActive() const { return m_MenuActive; }

	virtual void OnInit();
	virtual void OnConsoleInit();
	virtual void OnRelease();

	virtual void OnStateChange(int NewState, int OldState);
	virtual void OnReset();
	virtual void OnRender();
	virtual bool OnInput(IInput::CEvent Event);
	virtual bool OnMouseMove(float x, float y);
};
#endif
