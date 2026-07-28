

#include <math.h>

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
#include <engine/platform_services.h>

#include <game/version.h>
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

vec4 CMenus::ms_ColorBgDeep = vec4(0.012f, 0.014f, 0.017f, 0.96f);
vec4 CMenus::ms_ColorBgPanel = vec4(0.044f, 0.048f, 0.058f, 0.96f);
vec4 CMenus::ms_ColorBgInset = vec4(0.026f, 0.028f, 0.034f, 0.92f);
vec4 CMenus::ms_ColorAccent = vec4(0.95f, 0.58f, 0.18f, 1.0f);
vec4 CMenus::ms_ColorAccentDim = vec4(0.18f, 0.66f, 0.46f, 1.0f);
vec4 CMenus::ms_ColorDanger = vec4(0.92f, 0.24f, 0.30f, 1.0f);
vec4 CMenus::ms_ColorText = vec4(0.97f, 0.97f, 0.95f, 1.0f);
float CMenus::ms_PanelRounding = 4.0f;
float CMenus::ms_ControlRounding = 3.0f;

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

CMenus::CMenus()
{
	m_Popup = POPUP_NONE;
	m_ActivePage = PAGE_FRONT;
	m_GamePage = PAGE_GAME;
	m_NavigationFocus = 0;
	m_LastInputDevice = 0;
	m_PlayTab = 0;
	m_CreateRoomStep = 0;
	m_CreateRoomPreviousSlots = 8;
	m_NavigationHasFocus = false;

	g_Config.m_UiPage = PAGE_FRONT;

	m_NeedRestartGraphics = false;
	m_NeedRestartSound = false;
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
	str_copy(m_aFilterPresets[UI_FILTER_PRESET_ALL].m_aName, "All", sizeof(m_aFilterPresets[UI_FILTER_PRESET_ALL].m_aName));
	str_copy(m_aFilterPresets[UI_FILTER_PRESET_FAVORITES].m_aName, "Favorites", sizeof(m_aFilterPresets[UI_FILTER_PRESET_FAVORITES].m_aName));
}

void CMenus::UpdatePlaySnapshots()
{
	// Replacing a snapshot only after a refresh completes avoids transient empty
	// lists while master or LAN discovery is still in flight.
	if(!ServerBrowser()->IsRefreshing())
	{
		const int Collection = clamp(m_PlayBrowserCollection, (int)PLAY_COLLECTION_INTERNET, (int)PLAY_COLLECTION_FAVORITES);
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

vec4 CMenus::ThemeBgDeep() { return ms_ColorBgDeep; }
vec4 CMenus::ThemeBgPanel() { return ms_ColorBgPanel; }
vec4 CMenus::ThemeBgInset() { return ms_ColorBgInset; }
vec4 CMenus::ThemeAccent() { return ms_ColorAccent; }
vec4 CMenus::ThemeAccentDim() { return ms_ColorAccentDim; }
vec4 CMenus::ThemeDanger() { return ms_ColorDanger; }
vec4 CMenus::ThemeText() { return ms_ColorText; }

void CMenus::OpenResearchPage()
{
	s_ResetMenu = false;
	m_Popup = POPUP_NONE;
	if(Client()->State() == IClient::STATE_OFFLINE)
		g_Config.m_UiPage = PAGE_RESEARCH;
	else
		m_GamePage = PAGE_RESEARCH;
	SetActive(true);
}

void CMenus::DrawMenuBorder(const CUIRect *pRect, const vec4 &Fill, const vec4 &Border, int Corners, float Rounding)
{
	RenderTools()->DrawUIRect(pRect, Border, Corners, Rounding);
	CUIRect Inner = *pRect;
	Inner.Margin(1.0f, &Inner);
	RenderTools()->DrawUIRect(&Inner, Fill, Corners, max(0.0f, Rounding - 1.0f));
}

void CMenus::DrawMenuPanel(const CUIRect *pRect, int Corners)
{
	vec4 Fill = ms_ColorBgPanel;
	Fill.a = max(Fill.a * MenuAlpha(), 0.82f);
	vec4 Border = vec4(0.18f, 0.20f, 0.24f, max(0.80f, 0.92f * MenuAlpha()));
	DrawMenuBorder(pRect, Fill, Border, Corners, ms_PanelRounding);
}

void CMenus::DrawMenuInset(const CUIRect *pRect, int Corners)
{
	vec4 Fill = ms_ColorBgInset;
	Fill.a = max(Fill.a * MenuAlpha(), 0.78f);
	vec4 Border = vec4(0.16f, 0.18f, 0.22f, max(0.70f, 0.85f * MenuAlpha()));
	DrawMenuBorder(pRect, Fill, Border, Corners, ms_ControlRounding);
}

void CMenus::DrawSectionHeader(const CUIRect *pRect, int Corners)
{
	vec4 Fill = ms_ColorBgDeep;
	Fill.a = max(0.9f * MenuAlpha(), 0.82f);
	vec4 Border = vec4(0.18f, 0.20f, 0.24f, max(0.75f, 0.88f * MenuAlpha()));
	DrawMenuBorder(pRect, Fill, Border, Corners, ms_ControlRounding);
	DrawAccentUnderline(pRect);
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
	CUIRect Line = *pRect;
	Line.HSplitBottom(2.0f, 0, &Line);
	RenderTools()->DrawUIRect(&Line, ms_ColorAccent, 0, 0.0f);
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
	return vec4(
		A.r + (B.r - A.r) * t,
		A.g + (B.g - A.g) * t,
		A.b + (B.b - A.b) * t,
		A.a + (B.a - A.a) * t);
}

namespace {

enum { MENU_ANIM_SLOTS = 128 };

struct CMenuAnimSlot
{
	const void *m_pID;
	float m_Hover;
	float m_Selected;
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
	const float Now = Client()->LocalTime();
	const float dt = MenuAnimDt(pSlot, Now);

	pSlot->m_Selected = SmoothToward(pSlot->m_Selected, Selected ? 1.0f : 0.0f, dt, Speed);
	if(fabs(pSlot->m_Selected - (Selected ? 1.0f : 0.0f)) < 0.001f)
		pSlot->m_Selected = Selected ? 1.0f : 0.0f;
	return pSlot->m_Selected;
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
	RenderTools()->SelectSprite(Checked?SPRITE_GUIBUTTON_ON:SPRITE_GUIBUTTON_OFF);
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
	const float Press = UI()->ActiveItem() == pID ? 1.0f : 0.0f;

	vec4 FillBase = vec4(0.09f, 0.10f, 0.12f, 0.96f);
	vec4 FillHot = vec4(0.15f, 0.16f, 0.19f, 0.98f);
	vec4 BorderBase = vec4(0.22f, 0.24f, 0.28f, 0.95f);
	vec4 BorderHot = ms_ColorAccent;

	if(Style == BUTTONSTYLE_DANGER)
	{
		FillBase = vec4(0.20f, 0.06f, 0.08f, 0.96f);
		FillHot = vec4(0.32f, 0.10f, 0.12f, 0.98f);
		BorderBase = ms_ColorDanger;
		BorderHot = ms_ColorDanger;
	}
	else if(Style == BUTTONSTYLE_ACCENT)
	{
		FillBase = vec4(0.16f, 0.12f, 0.04f, 0.96f);
		FillHot = vec4(0.28f, 0.20f, 0.06f, 0.98f);
		BorderBase = ms_ColorAccent;
		BorderHot = ms_ColorAccent;
	}

	vec4 Fill = MixColor(FillBase, FillHot, Hover);
	vec4 Border = MixColor(BorderBase, BorderHot, Hover);
	if(Press > 0.0f)
		Fill = MixColor(Fill, vec4(Fill.r * 0.82f, Fill.g * 0.82f, Fill.b * 0.82f, Fill.a), Press);

	DrawMenuBorder(pRect, Fill, Border, CUI::CORNER_ALL, ms_ControlRounding);

	const float EdgeAlpha = (Style == BUTTONSTYLE_ACCENT || Style == BUTTONSTYLE_DANGER) ? 1.0f : Hover;
	if(EdgeAlpha > 0.02f)
	{
		CUIRect Edge = *pRect;
		Edge.HSplitBottom(2.0f, 0, &Edge);
		vec4 EdgeCol = Style == BUTTONSTYLE_DANGER ? ms_ColorDanger : ms_ColorAccent;
		EdgeCol.a *= EdgeAlpha;
		RenderTools()->DrawUIRect(&Edge, EdgeCol, CUI::CORNER_B, 0.0f);
	}

	CUIRect Temp;
	pRect->HMargin(pRect->h>=20.0f?2.0f:1.0f, &Temp);
	float FontSize = min(Temp.h*ms_FontmodHeight, 14.0f);
	FontSize = FitLabelFontSize(TextRender(), pText, FontSize, Temp.w - 8.0f);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	UI()->DoLabel(&Temp, pText, FontSize, 0);
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
}

void CMenus::DoButton_KeySelect(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	const float Hover = AnimHover(pID);
	vec4 Fill = MixColor(vec4(0.08f, 0.09f, 0.11f, 0.96f), vec4(0.14f, 0.16f, 0.19f, 0.98f), Hover);
	vec4 Border = MixColor(vec4(0.20f, 0.22f, 0.26f, 0.95f), ms_ColorAccent, Hover);
	DrawMenuBorder(pRect, Fill, Border, CUI::CORNER_ALL, ms_ControlRounding);
	CUIRect Temp;
	pRect->HMargin(1.0f, &Temp);
	float FontSize = min(Temp.h*ms_FontmodHeight, 13.0f);
	FontSize = FitLabelFontSize(TextRender(), pText, FontSize, Temp.w - 8.0f);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	UI()->DoLabel(&Temp, pText, FontSize, 0);
}

int CMenus::DoButton_MenuTab(const void *pID, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	const bool IsQuit = str_comp(pText, Localize("Quit")) == 0;
	const float Hover = AnimHover(pID);
	const float Sel = AnimSelected(pID, Checked);

	vec4 FillIdle = IsQuit ? vec4(0.22f, 0.07f, 0.09f, 0.92f) : vec4(0.08f, 0.09f, 0.11f, 0.92f);
	vec4 FillOn = IsQuit ? vec4(0.34f, 0.11f, 0.13f, 0.96f) : vec4(0.12f, 0.13f, 0.16f, 0.98f);
	vec4 FillHot = IsQuit ? vec4(0.28f, 0.09f, 0.11f, 0.95f) : vec4(0.16f, 0.18f, 0.22f, 0.96f);

	vec4 BorderIdle = IsQuit ? ms_ColorDanger : vec4(0.20f, 0.22f, 0.26f, 0.9f);
	vec4 BorderOn = IsQuit ? ms_ColorDanger : ms_ColorAccent;

	vec4 Fill = MixColor(FillIdle, FillOn, Sel);
	Fill = MixColor(Fill, FillHot, Hover * (1.0f - Sel * 0.5f));
	vec4 Border = MixColor(BorderIdle, BorderOn, max(Sel, Hover * 0.85f));

	DrawMenuBorder(pRect, Fill, Border, Corners, ms_PanelRounding);

	if(!IsQuit && Sel > 0.02f)
	{
		CUIRect Line = *pRect;
		Line.HSplitBottom(2.0f, 0, &Line);
		vec4 Accent = ms_ColorAccent;
		Accent.a *= Sel;
		RenderTools()->DrawUIRect(&Line, Accent, 0, 0.0f);
	}
	else if(IsQuit)
	{
		CUIRect Edge = *pRect;
		Edge.HSplitBottom(2.0f, 0, &Edge);
		RenderTools()->DrawUIRect(&Edge, ms_ColorDanger, 0, 0.0f);
	}

	CUIRect Temp;
	pRect->HMargin(2.0f, &Temp);
	float FontSize = min(Temp.h*ms_FontmodHeight, 13.0f);
	FontSize = FitLabelFontSize(TextRender(), pText, FontSize, Temp.w - 8.0f);
	TextRender()->TextColor(
		0.96f + 0.02f * Sel,
		0.96f + 0.01f * Sel,
		0.94f - 0.01f * Sel,
		1.0f);
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
	UI()->DoLabel(&t, pText, min(pRect->h*ms_FontmodHeight, 12.0f), -1);
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
		Inner.Margin(c.h*0.22f, &Inner);
		vec4 Mark = ms_ColorAccent;
		Mark.a *= Sel;
		RenderTools()->DrawUIRect(&Inner, Mark, CUI::CORNER_ALL, 2.0f);
	}
	else if(pBoxText[0] && pBoxText[0] != 'X')
	{
		TextRender()->TextColor(0.98f, 0.98f, 0.96f, 1.0f);
		UI()->DoLabel(&c, pBoxText, min(pRect->h*ms_FontmodHeight*0.6f, 12.0f), 0);
	}
	TextRender()->TextColor(0.96f, 0.96f, 0.94f, 1.0f);
	UI()->DoLabel(&t, pText, min(pRect->h*ms_FontmodHeight*0.8f, 13.0f), -1);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	return UI()->DoButtonLogic(pID, pText, 0, pRect);
}

int CMenus::DoButton_CheckBox(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	return DoButton_CheckBox_Common(pID, pText, Checked?"X":"", pRect);
}


int CMenus::DoButton_CheckBox_Number(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
{
	char aBuf[16];
	str_format(aBuf, sizeof(aBuf), "%d", Checked);
	return DoButton_CheckBox_Common(pID, pText, aBuf, pRect);
}

int CMenus::DoEditBox(void *pID, const CUIRect *pRect, char *pStr, unsigned StrSize, float FontSize, float *Offset, bool Hidden, int Corners)
{
	enum { MAX_EDIT_BINDINGS = 64 };
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

	Handle.y += (pRect->h-Handle.h)*Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		float Min = pRect->y;
		float Max = pRect->h-Handle.h;
		float Cur = UI()->MouseY()-OffsetY;
		ReturnValue = (Cur-Min)/Max;
		if(ReturnValue < 0.0f) ReturnValue = 0.0f;
		if(ReturnValue > 1.0f) ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetY = UI()->MouseY()-Handle.y;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	DrawMenuBorder(pRect, vec4(0.11f, 0.12f, 0.15f, 0.98f), vec4(0.31f, 0.34f, 0.40f, 0.96f), CUI::CORNER_ALL, ms_ControlRounding);

	CUIRect Rail;
	pRect->VMargin(5.0f, &Rail);
	DrawMenuBorder(&Rail, vec4(0.012f, 0.015f, 0.020f, 1.0f), vec4(0.24f, 0.27f, 0.32f, 0.96f), CUI::CORNER_ALL, ms_ControlRounding);

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

	Handle.x += (pRect->w-Handle.w)*Current;

	// logic
	float ReturnValue = Current;
	int Inside = UI()->MouseInside(&Handle);

	if(UI()->ActiveItem() == pID)
	{
		if(!UI()->MouseButton(0))
			UI()->SetActiveItem(0);

		float Min = pRect->x;
		float Max = pRect->w-Handle.w;
		float Cur = UI()->MouseX()-OffsetX;
		ReturnValue = (Cur-Min)/Max;
		if(ReturnValue < 0.0f) ReturnValue = 0.0f;
		if(ReturnValue > 1.0f) ReturnValue = 1.0f;
	}
	else if(UI()->HotItem() == pID)
	{
		if(UI()->MouseButton(0))
		{
			UI()->SetActiveItem(pID);
			OffsetX = UI()->MouseX()-Handle.x;
		}
	}

	if(Inside)
		UI()->SetHotItem(pID);

	// render
	DrawMenuBorder(pRect, vec4(0.11f, 0.12f, 0.15f, 0.98f), vec4(0.31f, 0.34f, 0.40f, 0.96f), CUI::CORNER_ALL, ms_ControlRounding);

	CUIRect Rail;
	pRect->HMargin(5.0f, &Rail);
	DrawMenuBorder(&Rail, vec4(0.012f, 0.015f, 0.020f, 1.0f), vec4(0.24f, 0.27f, 0.32f, 0.96f), CUI::CORNER_ALL, ms_ControlRounding);

	CUIRect Slider = Handle;
	Slider.Margin(3.0f, &Slider);
	const float Interaction = max(AnimHover(pID), UI()->ActiveItem() == pID ? 1.0f : 0.0f);
	const vec4 SliderIdle = vec4(0.66f, 0.70f, 0.76f, 1.0f);
	const vec4 SliderCol = MixColor(SliderIdle, ms_ColorAccent, Interaction * 0.72f);
	const vec4 SliderBorder = MixColor(vec4(0.88f, 0.90f, 0.94f, 1.0f), ms_ColorAccent, Interaction);
	DrawMenuBorder(&Slider, SliderCol, SliderBorder, CUI::CORNER_ALL, ms_ControlRounding);

	return ReturnValue;
}

float CMenus::DoIndependentDropdownMenu(void *pID, CUIRect *pRect, const char *pStr, float HeaderHeight, FDropdownCallback pfnCallback, bool *pActive)
{
	CUIRect View = *pRect;
	CUIRect Header;
	View.HSplitTop(HeaderHeight, &Header, &View);

	RenderTools()->DrawUIRect(&Header, vec4(0.06f, 0.07f, 0.09f, 0.95f), *pActive ? CUI::CORNER_T : CUI::CORNER_ALL, ms_ControlRounding);
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
	UI()->DoLabel(&Icon, aIcon, min(HeaderHeight*0.65f, 12.0f), 0);

	UI()->DoLabel(&Header, pStr, min(HeaderHeight*0.65f, 12.0f), -1);

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
	if (UI()->ActiveItem() == pID && ButtonUsed == 0)
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
	RenderTools()->DrawUIRect(&Art, vec4(Color.r * .10f, Color.g * .10f, Color.b * .10f, .96f), CUI::CORNER_T, ms_ControlRounding);
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	Graphics()->SetColor(Color.r, Color.g, Color.b, .16f);
	IGraphics::CLineItem aGrid[18];
	int Num = 0;
	for(int i = 1; i < 9; i++)
	{
		const float gx = Art.x + Art.w * i / 9.0f;
		aGrid[Num++] = IGraphics::CLineItem(gx, Art.y, gx, Art.y + Art.h);
	}
	for(int i = 1; i < 5; i++)
	{
		const float gy = Art.y + Art.h * i / 5.0f;
		aGrid[Num++] = IGraphics::CLineItem(Art.x, gy, Art.x + Art.w, gy);
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
	Scan.y += fmodf(Client()->LocalTime() * 18.0f, max(1.0f, Art.h));
	Scan.h = 1.0f;
	RenderTools()->DrawUIRect(&Scan, vec4(Color.r, Color.g, Color.b, .30f), 0, 0.0f);
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
			Rect.x, Rect.y, Rect.x + Rect.w, Rect.y,
			Rect.x, Rect.y + Rect.h, Rect.x + Rect.w, Rect.y + Rect.h);
		Graphics()->QuadsDrawFreeform(&Image, 1);
		Graphics()->QuadsEnd();
	}
	else
	{
		TextRender()->TextColor(ms_ColorAccentDim.r, ms_ColorAccentDim.g, ms_ColorAccentDim.b, 1.0f);
		UI()->DoLabelScaled(&Rect, pImage, FitLabelFontSize(TextRender(), pImage, 8.0f, Rect.w - 8.0f), 0);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}

void CMenus::DrawStatusBadge(CUIRect Rect, const char *pText, const vec4 &Color)
{
	RenderTools()->DrawUIRect(&Rect, vec4(Color.r * .18f, Color.g * .18f, Color.b * .18f, .96f), CUI::CORNER_ALL, 8.0f);
	TextRender()->TextColor(Color.r, Color.g, Color.b, 1.0f);
	UI()->DoLabelScaled(&Rect, pText, FitLabelFontSize(TextRender(), pText, 9.0f, Rect.w - 12.0f), 0);
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
	const bool Compact = UI()->Screen()->w < 900.0f;
	CClientAsyncStatus SteamHostStatus;
	Client()->SteamHostedGameStatus(&SteamHostStatus);
	const bool ManagedLocalGameActive = m_LocalServerState == LOCAL_SERVER_STARTING || m_LocalServerState == LOCAL_SERVER_RUNNING;
	const bool SteamHostedGameActive = SteamHostStatus.m_State == CLIENT_ASYNC_WORKING || SteamHostStatus.m_State == CLIENT_ASYNC_SUCCEEDED;
	const char *apOfflineLabels[] = {"Play", "Character", "Progress", "Mods", "Replays", "Settings"};
	const int aOfflinePages[] = {PAGE_FRONT, PAGE_CUSTOMIZE, PAGE_RESEARCH, PAGE_MODS, PAGE_DEMOS, PAGE_SETTINGS};
	const char *apGameLabels[] = {"Continue", "Game", InGameRoomActionLabel(ManagedLocalGameActive, SteamHostedGameActive), "Players", "Server", "Vote", "Progress", "Settings", "Leave"};
	const int aGamePages[] = {-2, PAGE_GAME, PAGE_LOCAL_SERVER, PAGE_PLAYERS, PAGE_SERVER_INFO, PAGE_CALLVOTE, PAGE_RESEARCH, PAGE_SETTINGS, -3};
	const int aOfflineIcons[] = {0, 1, 2, 3, 4, 5};
	const int aGameIcons[] = {4, 0, 7, 1, 3, 2, 2, 5, 6};
	const char **apLabels = Offline ? apOfflineLabels : apGameLabels;
	const int *pPages = Offline ? aOfflinePages : aGamePages;
	const int Count = Offline ? 6 : 9;
	static int s_aNavigationButtons[9];

	for(int i = 0; i < m_NumInputEvents; i++)
	{
		const IInput::CEvent &Event = m_aInputEvents[i];
		if(!(Event.m_Flags & IInput::FLAG_PRESS)) continue;
		if(Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT) m_NavigationHasFocus = true;
		if(Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT) m_NavigationHasFocus = false;
		if(m_NavigationHasFocus && (Event.m_Key == KEY_UP || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP)) m_NavigationFocus = (m_NavigationFocus + Count - 1) % Count;
		if(m_NavigationHasFocus && (Event.m_Key == KEY_DOWN || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN)) m_NavigationFocus = (m_NavigationFocus + 1) % Count;
	}
	m_NavigationFocus = clamp(m_NavigationFocus, 0, Count - 1);

	DrawMenuPanel(&r, CUI::CORNER_ALL);
	CUIRect Box = r;
	Box.Margin(8.0f, &Box);
	CUIRect Brand;
	Box.HSplitTop(46.0f, &Brand, &Box);
	UI()->DoLabelScaled(&Brand, Compact ? "N" : "NINSLASH", Compact ? 22.0f : 16.0f, 0);
	Box.HSplitTop(8.0f, 0, &Box);
	int NewPage = -1;
	const char *pTooltip = 0;
	CUIRect TooltipAnchor;
	for(int i = 0; i < Count; i++)
	{
		if((Offline && (i == 3 || i == 5)) || (!Offline && (i == 1 || i == 6)))
		{
			CUIRect Group;
			Box.HSplitTop(Compact ? 8.0f : 20.0f, &Group, &Box);
			if(!Compact)
			{
				const char *pGroup = Offline ? (i == 3 ? "CONTENT" : "SYSTEM") : (i == 1 ? "SESSION" : "PROFILE");
				TextRender()->TextColor(.48f, .52f, .58f, 1.0f);
				UI()->DoLabelScaled(&Group, Localize(pGroup), 8.0f, -1);
				TextRender()->TextColor(1, 1, 1, 1);
			}
			else
			{
				Group.HMargin(3.0f, &Group);
				RenderTools()->DrawUIRect(&Group, vec4(.20f, .22f, .26f, .65f), 0, 0.0f);
			}
		}
		CUIRect Button;
		Box.HSplitTop(36.0f, &Button, &Box);
		const char *pText = Compact ? "" : Localize(apLabels[i]);
		const bool Focused = m_NavigationHasFocus && m_NavigationFocus == i;
		const bool PageSelected = pPages[i] == m_ActivePage || (pPages[i] == PAGE_FRONT && m_ActivePage == PAGE_TUTORIAL_SELECT);
		const bool Activated = DoButton_Menu(&s_aNavigationButtons[i], pText, PageSelected || Focused, &Button, pPages[i] == -3 ? BUTTONSTYLE_DANGER : BUTTONSTYLE_NORMAL) || (Focused && m_LastInputDevice != 0 && m_EnterPressed);
		if(PageSelected)
		{
			CUIRect Indicator = Button;
			Indicator.w = 3.0f;
			Indicator.HMargin(7.0f, &Indicator);
			RenderTools()->DrawUIRect(&Indicator, ms_ColorAccent, CUI::CORNER_R, 2.0f);
		}
		if(Compact)
		{
			const int Icon = Offline ? aOfflineIcons[i] : aGameIcons[i];
			DrawNavigationIcon(Button, Icon, PageSelected || Focused);
			if(UI()->MouseInside(&Button))
			{
				pTooltip = Localize(apLabels[i]);
				TooltipAnchor = Button;
			}
		}
		if(Activated)
		{
			m_EnterPressed = false;
			m_NavigationFocus = i;
			if(pPages[i] == -2) SetActive(false);
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
			else NewPage = pPages[i];
		}
		Box.HSplitTop(5.0f, 0, &Box);
	}
	if(pTooltip)
	{
		CUIRect Tip = TooltipAnchor;
		Tip.x = r.x + r.w + 7.0f;
		Tip.w = max(92.0f, TextRender()->TextWidth(0, 10.0f, pTooltip, -1) + 20.0f);
		DrawMenuBorder(&Tip, ms_ColorBgDeep, vec4(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, .75f), CUI::CORNER_ALL, 3.0f);
		UI()->DoLabelScaled(&Tip, pTooltip, 10.0f, 0);
	}

	CUIRect Footer;
	r.HSplitBottom(104.0f, 0, &Footer);
	Footer.Margin(10.0f, &Footer);
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	char aIdentity[128];
	if(pPlatform && pPlatform->Available())
	{
		if(Compact)
			str_copy(aIdentity, "STEAM\nONLINE", sizeof(aIdentity));
		else
			str_format(aIdentity, sizeof(aIdentity), "%s  %llu\n%s", "Steam", pPlatform->LocalUserID(), Localize("Online"));
	}
	else if(Compact)
		str_copy(aIdentity, "NET\nUDP", sizeof(aIdentity));
	else
		str_format(aIdentity, sizeof(aIdentity), "%s\n%s", Localize("Standalone network"), Localize("UDP available"));
	TextRender()->TextColor(ms_ColorAccentDim.r, ms_ColorAccentDim.g, ms_ColorAccentDim.b, 1.0f);
	UI()->DoLabelScaled(&Footer, aIdentity, Compact ? 8.0f : 9.0f, -1);
	TextRender()->TextColor(1, 1, 1, 1);
	if(Offline)
	{
		CUIRect Quit;
		Footer.HSplitBottom(30.0f, 0, &Quit);
		static int s_QuitButton;
		if(DoButton_Menu(&s_QuitButton, Compact ? "" : Localize("Quit"), 0, &Quit, BUTTONSTYLE_DANGER)) m_Popup = POPUP_QUIT;
		if(Compact)
			DrawNavigationIcon(Quit, 6, false);
	}

	if(NewPage != -1)
	{
		if(Client()->State() == IClient::STATE_OFFLINE)
			g_Config.m_UiPage = NewPage;
		else
		{
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
	float Percent = m_LoadCurrent++/(float)m_LoadTotal;

	// make sure that we don't render for each little thing we load
	// because that will slow down loading if we have vsync
	if(time_get()-LastLoadRender < time_freq()/60)
		return;

	LastLoadRender = time_get();

	// need up date this here to get correct
	//vec3 Rgb = HslToRgb(vec3(g_Config.m_UiColorHue/255.0f, g_Config.m_UiColorSat/255.0f, g_Config.m_UiColorLht/255.0f));
	//ms_GuiColor = vec4(Rgb.r, Rgb.g, Rgb.b, g_Config.m_UiColorAlpha/255.0f);
	ms_GuiColor = vec4(0.2f, 0.25f, 0.3f, 0.75f);

	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);

	RenderBackground();

	float w = 700;
	float h = 200;
	float x = Screen.w/2-w/2;
	float y = Screen.h/2-h/2;

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
	r.y = y+20;
	r.w = w;
	r.h = h;
	UI()->DoLabel(&r, pCaption, 48.0f, 0, -1);

	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,0.75f);
	RenderTools()->DrawRoundRect(x+40, y+h-75, (w-80)*Percent, 25, 5.0f);
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
		if(m_aModes[i].m_Width / G == DesktopWidthG &&
			m_aModes[i].m_Height / G == DesktopHeightG &&
			m_aModes[i].m_Width <= Graphics()->DesktopWidth() &&
			m_aModes[i].m_Height <= Graphics()->DesktopHeight())
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
			g_Config.m_ClTutorialChapter = TutorialChapterFromLegacy(g_Config.m_ClTutorialState, g_Config.m_ClTutorialCheckpoint);
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

	// setup load amount
	m_LoadCurrent = 0;
	m_LoadTotal = g_pData->m_NumImages;
	if(!g_Config.m_ClThreadsoundloading)
		m_LoadTotal += g_pData->m_NumSounds;
}

void CMenus::OnConsoleInit()
{
	Console()->Register("local_game_start", "?i", CFGFLAG_CLIENT, ConLocalGameStart, this, "Start a local game; pass 0 to stay in the menu");
	Console()->Register("local_game_stop", "", CFGFLAG_CLIENT, ConLocalGameStop, this, "Stop the managed local game server");
	Console()->Register("local_game_restart", "", CFGFLAG_CLIENT, ConLocalGameRestart, this, "Restart and rejoin the managed local game server");
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


static int gs_TextureLogo = -1;

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

static void ApplyLocalGameModeDefaults(int Mode)
{
	Mode = clamp(Mode, (int)LOCAL_MODE_INVASION, (int)LOCAL_MODE_COUNT - 1);
	const CRoomModeDefaults Defaults = RoomModeDefaults(Mode);
	g_Config.m_ClLocalServerMode = Mode;
	g_Config.m_ClLocalServerMap = 0;
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
	if(Floor <= 10)
		return "cfg/invasion1.cfg";
	if(Floor <= 20)
		return "cfg/invasion2.cfg";
	if(Floor <= 30)
		return "cfg/invasion3.cfg";
	if(Floor <= 40)
		return "cfg/invasion4.cfg";
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
	return 0;
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
	pSettings->m_pMapName = pSettings->m_pMode->m_SelectableMap ? pSettings->m_pMode->m_ppMapNames[pSettings->m_Map] : "Automatic by Invasion floor";
	pSettings->m_pMapCommand = pSettings->m_pMode->m_SelectableMap ? pSettings->m_pMode->m_ppMapCommands[pSettings->m_Map] : 0;
	pSettings->m_Port = clamp(g_Config.m_ClLocalServerPort, 1024, 65535);
	pSettings->m_MaxClients = clamp(g_Config.m_ClLocalServerMaxClients, 1, 16);
	pSettings->m_Bots = pSettings->m_pMode->m_Pve ? 0 : clamp(g_Config.m_ClLocalServerBots, 0, 16);
	pSettings->m_Difficulty = clamp(g_Config.m_ClLocalServerDifficulty, 1, 50);
	pSettings->m_BotLevel = clamp(pSettings->m_Difficulty, 1, 30);
	pSettings->m_InvasionStart = clamp(g_Config.m_ClLocalServerInvasionStart, (int)LOCAL_INVASION_TEAM_CHECKPOINT, (int)LOCAL_INVASION_CUSTOM_FLOOR);
	pSettings->m_InvasionFloor = clamp(g_Config.m_ClLocalServerInvasionFloor, 1, max(1, g_Config.m_ClPveHighestInvasion));
	pSettings->m_Lan = g_Config.m_ClLocalServerLan != 0;
	pSettings->m_RandomSeed = g_Config.m_ClLocalServerRandomSeed != 0;
	pSettings->m_MapGen = pSettings->m_pMode->m_MapGen;
	pSettings->m_Seed = clamp(g_Config.m_ClLocalServerSeed, 0, 32767);
	pSettings->m_Roguelite = pSettings->m_pMode->m_Pve && g_Config.m_ClLocalServerRoguelite != 0;
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
		pSettings->m_UseCheckpoint = pSettings->m_Roguelite && pSettings->m_InvasionStart == LOCAL_INVASION_TEAM_CHECKPOINT;
		pSettings->m_MapLevel = pSettings->m_InvasionStart == LOCAL_INVASION_CUSTOM_FLOOR ? pSettings->m_InvasionFloor : 1;
		int TemplateFloor = pSettings->m_MapLevel;
		if(pSettings->m_UseCheckpoint)
		{
			const int MaxCheckpoint = g_Config.m_ClPveHighestInvasion >= 10 ? (g_Config.m_ClPveHighestInvasion / 10) * 10 + 1 : 1;
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

static void FormatLocalServerSummary(const CLocalServerLaunchSettings &Settings, int Port, char *pBuffer, int BufferSize)
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
	else
		str_copy(aRule, Localize(Settings.m_Roguelite ? "Roguelite" : "Classic PvE"), sizeof(aRule));
	if(Settings.m_RandomSeed)
		str_copy(aSeed, Localize("Random seed"), sizeof(aSeed));
	else
		str_format(aSeed, sizeof(aSeed), Localize("Seed %d"), Settings.m_Seed);
	str_format(aSlots, sizeof(aSlots), Localize("%d human slots"), Settings.m_MaxClients);
	if(!Settings.m_pMode->m_Pve)
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
		str_format(pBuffer, BufferSize, "%s · %s · %s · %s · %s · %s · %s · 127.0.0.1:%d / LAN:%d",
			Localize(Settings.m_pMode->m_pName), Localize(Settings.m_pMapName), aStart, aRule, aPopulation, aSeed, aSlots, Port, Port);
	else if(Settings.m_Lan)
		str_format(pBuffer, BufferSize, "%s · %s · %s · %s · %s · %s · 127.0.0.1:%d / LAN:%d",
			Localize(Settings.m_pMode->m_pName), Localize(Settings.m_pMapName), aStart, aRule, aSeed, aSlots, Port, Port);
	else if(aPopulation[0])
		str_format(pBuffer, BufferSize, "%s · %s · %s · %s · %s · %s · %s · 127.0.0.1:%d",
			Localize(Settings.m_pMode->m_pName), Localize(Settings.m_pMapName), aStart, aRule, aPopulation, aSeed, aSlots, Port);
	else
		str_format(pBuffer, BufferSize, "%s · %s · %s · %s · %s · %s · 127.0.0.1:%d",
			Localize(Settings.m_pMode->m_pName), Localize(Settings.m_pMapName), aStart, aRule, aSeed, aSlots, Port);
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
}

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
	if(m_LocalServerActualPort <= 0 || Client()->State() == IClient::STATE_OFFLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return false;
	NETADDR Address;
	Client()->GetServerAddress(&Address);
	return net_addr_comp(&Address, &m_LocalServerAddress) == 0;
}

void CMenus::RefreshLocalServerErrorDetail()
{
	ReadLocalServerLogTail(m_aLocalServerLogPath, m_aLocalServerErrorDetail, sizeof(m_aLocalServerErrorDetail));
	if(!m_aLocalServerErrorDetail[0] && m_LocalServerExitCode > 0)
		str_format(m_aLocalServerErrorDetail, sizeof(m_aLocalServerErrorDetail), "Server exited with code %d", m_LocalServerExitCode);
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

void CMenus::StartPvpPractice()
{
	g_Config.m_ClTutorialActive = 0;
	g_Config.m_ClLocalServerMode = LOCAL_MODE_DM;
	g_Config.m_ClLocalServerMap = 0;
	g_Config.m_ClLocalServerMaxClients = 5;
	g_Config.m_ClLocalServerBots = 4;
	g_Config.m_ClLocalServerDifficulty = 3;
	g_Config.m_ClLocalServerDmScore = 15;
	str_copy(g_Config.m_ClLocalServerName, "Local PvP", sizeof(g_Config.m_ClLocalServerName));
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
		const int Candidate = Settings.m_Port + Offset <= 65535 ? Settings.m_Port + Offset : 1024 + Settings.m_Port + Offset - 65536;
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
		str_copy(m_aLocalServerErrorDetail, Localize("The preferred port and the next nine ports are already in use."), sizeof(m_aLocalServerErrorDetail));
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
	str_format(aRelativeLogPath, sizeof(aRelativeLogPath), "logs/local_server_%s_%06d.log", aTimestamp, (int)(time_get() % 1000000));
	Storage()->GetCompletePath(IStorage::TYPE_SAVE, aRelativeLogPath, m_aLocalServerLogPath, sizeof(m_aLocalServerLogPath));

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
	str_format(aRoguelite, sizeof(aRoguelite), "sv_pve_roguelite %d", Settings.m_Roguelite);
	str_format(aContracts, sizeof(aContracts), "sv_pve_contracts %d", Settings.m_Contracts);
	str_format(aCheckpoint, sizeof(aCheckpoint), "sv_invasion_use_checkpoint %d", Settings.m_UseCheckpoint);
	str_format(aTutorialChapter, sizeof(aTutorialChapter), "sv_tutorial_chapter %d", g_Config.m_ClTutorialChapter);
	str_format(aTutorialStep, sizeof(aTutorialStep), "sv_tutorial_step %d", g_Config.m_ClTutorialStep);
	str_format(aTutorialMode, sizeof(aTutorialMode), "sv_tutorial_mode %d", g_Config.m_ClTutorialActive ? 1 : 0);
	str_format(aTutorialCompleted, sizeof(aTutorialCompleted), "sv_tutorial_completed_mask %d", g_Config.m_ClTutorialCompletedMask);
	if(LocalRuleUsesScoreLimit(Settings.m_pMode->m_Rule))
		str_format(aModeRule, sizeof(aModeRule), "sv_scorelimit %d", Settings.m_ModeRule);
	else if(Settings.m_pMode->m_Rule == LOCAL_RULE_EXTRACTION)
		str_format(aModeRule, sizeof(aModeRule), "sv_timelimit %d", Settings.m_ModeRule);
	EscapeLocalServerValue(Settings.m_aName, aNameValue, sizeof(aNameValue));
	EscapeLocalServerValue(Settings.m_aPassword, aPasswordValue, sizeof(aPasswordValue));
	EscapeLocalServerValue(m_aLocalServerLogPath, aLogValue, sizeof(aLogValue));
	str_format(aName, sizeof(aName), "sv_name %s", aNameValue);
	str_format(aPassword, sizeof(aPassword), "password %s", aPasswordValue);
	str_format(aLog, sizeof(aLog), "logfile %s", aLogValue);

	const char *apArguments[34];
	int NumArguments = 0;
	apArguments[NumArguments++] = aExecutable;
	apArguments[NumArguments++] = "-s";
	apArguments[NumArguments++] = "-f";
	apArguments[NumArguments++] = Settings.m_pConfig;
	apArguments[NumArguments++] = "sv_register 0";
	apArguments[NumArguments++] = "sv_register_steam 0";
	apArguments[NumArguments++] = "sv_steam_auth 1";
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
		dbg_msg("local-server", "started: %s; preferred port %d, actual port %d; log: %s", m_aLocalServerSummary, g_Config.m_ClLocalServerPort, Settings.m_Port, m_aLocalServerLogPath);
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
		if(IsConnectedToLocalServer() && (Client()->State() == IClient::STATE_LOADING || Client()->State() == IClient::STATE_ONLINE))
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
		const int Action = g_Config.m_ClTutorialStep < 2 ? TUTORIAL_ACTION_UI_ROOM_CREATE : TUTORIAL_ACTION_UI_ROOM_JOIN;
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
		if(m_LocalServerProcess && (m_LocalServerState == LOCAL_SERVER_RUNNING || m_LocalServerState == LOCAL_SERVER_STARTING))
			StopLocalServer(true);
		else
			StartLocalServer(true);
		return;
	}

	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	if(!pPlatform || !pPlatform->Available())
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
	Settings.m_MaxClients = Preview.m_MaxClients;
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
	const float LayoutDivisor = max(1.0f, UI()->Scale());
	auto L = [LayoutDivisor](float Value) { return Value / LayoutDivisor; };
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
			const bool Confirm = Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A;
			if(Event.m_Key == KEY_GAMEPAD_BUTTON_B)
			{
				if(m_CreateRoomStep == CREATE_ROOM_CONFIGURE)
					m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
				continue;
			}
			if(m_CreateRoomStep == CREATE_ROOM_CHOOSE_MODE)
			{
				m_LocalServerFocus = clamp(m_LocalServerFocus, (int)LOCAL_MODE_INVASION, (int)LOCAL_MODE_COUNT - 1);
				if(Left || Right || Up || Down)
				{
					const bool InPve = LocalGameMode(m_LocalServerFocus).m_Pve;
					const int *pModes = InPve ? s_aLocalPveModes : s_aLocalPvpModes;
					const int Count = InPve ? (int)(sizeof(s_aLocalPveModes) / sizeof(s_aLocalPveModes[0])) : (int)(sizeof(s_aLocalPvpModes) / sizeof(s_aLocalPvpModes[0]));
					int Index = 0;
					while(Index + 1 < Count && pModes[Index] != m_LocalServerFocus)
						Index++;
					if(Up || Down)
						m_LocalServerFocus = pModes[clamp(Index + (Down ? 1 : -1), 0, Count - 1)];
					else
					{
						const int *pOther = InPve ? s_aLocalPvpModes : s_aLocalPveModes;
						const int OtherCount = InPve ? (int)(sizeof(s_aLocalPvpModes) / sizeof(s_aLocalPvpModes[0])) : (int)(sizeof(s_aLocalPveModes) / sizeof(s_aLocalPveModes[0]));
						if((InPve && Right) || (!InPve && Left))
							m_LocalServerFocus = pOther[min(Index, OtherCount - 1)];
					}
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
				while(!SteamAvailable && (Visibility == ROOM_VISIBILITY_FRIENDS || Visibility == ROOM_VISIBILITY_PUBLIC));
				if(g_Config.m_ClRoomVisibility != ROOM_VISIBILITY_SOLO)
					m_CreateRoomPreviousSlots = max(2, g_Config.m_ClLocalServerMaxClients);
				const bool LeavingSolo = g_Config.m_ClRoomVisibility == ROOM_VISIBILITY_SOLO;
				g_Config.m_ClRoomVisibility = Visibility;
				g_Config.m_ClLocalServerMaxClients = Visibility == ROOM_VISIBILITY_SOLO ? 1 : LeavingSolo ? clamp(m_CreateRoomPreviousSlots, 2, 16) : g_Config.m_ClLocalServerMaxClients;
			}
		}
	}

	if(m_EscapePressed)
	{
		if(m_CreateRoomStep == CREATE_ROOM_CONFIGURE)
			m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
		m_EscapePressed = false;
	}

	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(L(12.0f), &MainView);
	CUIRect Header, Body, Footer;
	const float LargeScale = max(0.0f, UI()->Scale() - 1.0f);
	const float HeaderHeight = L(48.0f + LargeScale * 84.0f);
	const float StepOffset = L(27.0f + LargeScale * 66.0f);
	MainView.HSplitTop(HeaderHeight, &Header, &Body);
	Body.HSplitBottom(L(72.0f), &Body, &Footer);
	const char *pTitle = m_CreateRoomStep == CREATE_ROOM_CHOOSE_MODE ? "Choose a game mode" : "Configure room";
	UI()->DoLabelScaled(&Header, Localize(pTitle), 22.0f, -1);
	CUIRect StepLabel = Header;
	StepLabel.y += StepOffset;
	UI()->DoLabelScaled(&StepLabel, Localize(m_CreateRoomStep == CREATE_ROOM_CHOOSE_MODE ? "Step 1 of 2" : "Step 2 of 2"), 10.0f, -1);
	DrawAccentUnderline(&Header);

	if(m_CreateRoomStep == CREATE_ROOM_CHOOSE_MODE)
	{
		Body.HMargin(L(6.0f), &Body);
		const bool SingleColumn = Body.w < 650.0f;
		static CScrollRegion s_ModeScrollRegion;
		vec2 ScrollOffset(0.0f, 0.0f);
		CUIRect ModeContent = Body;
		CScrollRegionParams ScrollParams;
		ConfigureScrollRegion(&ScrollParams);
		ScrollParams.m_ClipBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		ScrollParams.m_ScrollUnit = L(94.0f);
		s_ModeScrollRegion.Begin(&Body, &ScrollOffset, &ScrollParams);
		ModeContent.y += ScrollOffset.y;
		ModeContent.VSplitRight(L(20.0f), &ModeContent, 0);
		CUIRect Pve = ModeContent, Pvp;
		const int PveCount = (int)(sizeof(s_aLocalPveModes) / sizeof(s_aLocalPveModes[0]));
		const int PvpCount = (int)(sizeof(s_aLocalPvpModes) / sizeof(s_aLocalPvpModes[0]));
		const float PveHeight = L(26.0f + PveCount * 94.0f);
		const float PvpHeight = L(26.0f + PvpCount * 94.0f);
		if(SingleColumn)
		{
			ModeContent.HSplitTop(PveHeight, &Pve, &Pvp);
			Pvp.HSplitTop(L(12.0f), 0, &Pvp);
		}
		else
		{
			ModeContent.VSplitMid(&Pve, &Pvp);
			Pve.VSplitRight(L(4.0f), &Pve, 0);
			Pvp.VSplitLeft(L(4.0f), 0, &Pvp);
		}
		auto DrawModeGroup = [&](CUIRect Group, const char *pGroupName, const int *pModes, int Count) {
			CUIRect GroupTitle;
			Group.HSplitTop(L(26.0f), &GroupTitle, &Group);
			UI()->DoLabelScaled(&GroupTitle, Localize(pGroupName), 14.0f, -1);
			for(int Index = 0; Index < Count; Index++)
			{
				const int Mode = pModes[Index];
				CUIRect Card, Top, Description, Meta, Mechanics, Select;
				Group.HSplitTop(L(88.0f), &Card, &Group);
				Group.HSplitTop(L(6.0f), 0, &Group);
				DrawMenuInset(&Card, CUI::CORNER_ALL);
				Card.Margin(L(7.0f), &Card);
				CUIRect Preview, Content;
				Card.VSplitLeft(L(88.0f), &Preview, &Content);
				const float PreviewHeight = min(Preview.h, L(44.0f));
				Preview.y += (Preview.h - PreviewHeight) * 0.5f;
				Preview.h = PreviewHeight;
				DrawModeVoteImage(Preview, s_aLocalGameModes[Mode].m_pGameVoteImage, m_LocalServerFocus == Mode);
				Content.VSplitLeft(L(8.0f), 0, &Card);
				Card.HSplitTop(L(20.0f), &Top, &Card);
				Top.VSplitRight(L(76.0f), &Top, &Select);
				UI()->DoLabelScaled(&Top, Localize(s_aLocalGameModes[Mode].m_pName), 14.0f, -1);
				if(DoButton_Menu(&s_aModeButtons[Mode], Localize("Select"), m_LocalServerFocus == Mode, &Select, BUTTONSTYLE_ACCENT))
				{
					ApplyLocalGameModeDefaults(Mode);
					m_CreateRoomPreviousSlots = g_Config.m_ClLocalServerMaxClients;
					m_CreateRoomStep = CREATE_ROOM_CONFIGURE;
				}
				Card.HSplitTop(L(25.0f), &Description, &Card);
				UI()->DoLabelScaled(&Description, Localize(s_aLocalGameModes[Mode].m_pDescription), 9.0f, -1, (int)Description.w);
				Card.HSplitTop(L(16.0f), &Meta, &Card);
				char aMeta[160];
				str_format(aMeta, sizeof(aMeta), Localize("Recommended: %s players  ·  %s  ·  %s difficulty"), s_aLocalGameModes[Mode].m_pRecommendedPlayers, Localize(s_aLocalGameModes[Mode].m_pDuration), Localize(s_aLocalGameModes[Mode].m_pRecommendedDifficulty));
				UI()->DoLabelScaled(&Meta, aMeta, 8.5f, -1);
				Card.HSplitTop(L(15.0f), &Mechanics, &Card);
				UI()->DoLabelScaled(&Mechanics, Localize(s_aLocalGameModes[Mode].m_pMechanics), 8.5f, -1);
			}
		};
		DrawModeGroup(Pve, "PVE", s_aLocalPveModes, PveCount);
		DrawModeGroup(Pvp, "PVP", s_aLocalPvpModes, PvpCount);
		CUIRect ScrollContent = ModeContent;
		ScrollContent.h = SingleColumn ? PveHeight + L(12.0f) + PvpHeight : max(PveHeight, PvpHeight);
		s_ModeScrollRegion.AddRect(ScrollContent);
		s_ModeScrollRegion.End();
		DrawMenuInset(&Footer, CUI::CORNER_ALL);
		Footer.Margin(L(9.0f), &Footer);
		UI()->DoLabelScaled(&Footer, Localize("Training is available from the Play hub."), 10.0f, -1);
		return;
	}

	const int Mode = clamp(g_Config.m_ClLocalServerMode, (int)LOCAL_MODE_INVASION, (int)LOCAL_MODE_COUNT - 1);
	const CLocalGameMode &ModeDef = LocalGameMode(Mode);
	g_Config.m_ClLocalServerMap = clamp(g_Config.m_ClLocalServerMap, 0, ModeDef.m_MapCount - 1);
	CUIRect Summary, Change;
	Body.HSplitTop(L(42.0f), &Summary, &Body);
	Summary.VSplitRight(L(124.0f), &Summary, &Change);
	char aModeSummary[256];
	str_format(aModeSummary, sizeof(aModeSummary), "%s  ·  %s  ·  %s", Localize(ModeDef.m_pName), Localize(ModeDef.m_pDescription), Localize(ModeDef.m_pMechanics));
	UI()->DoLabelScaled(&Summary, aModeSummary, FitLabelFontSize(TextRender(), aModeSummary, 11.0f, Summary.w), -1);
	if(DoButton_Menu(&s_ChangeMode, Localize("Change mode"), 0, &Change))
		m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
	Body.HSplitTop(L(8.0f), 0, &Body);

	CUIRect MainSettings, Identity;
	Body.VSplitMid(&MainSettings, &Identity);
	MainSettings.VSplitRight(L(5.0f), &MainSettings, 0);
	Identity.VSplitLeft(L(5.0f), 0, &Identity);
	DrawMenuInset(&MainSettings, CUI::CORNER_ALL);
	DrawMenuInset(&Identity, CUI::CORNER_ALL);
	MainSettings.Margin(L(9.0f), &MainSettings);
	Identity.Margin(L(9.0f), &Identity);
	CUIRect Row, Label, Control, Previous, Next, Value;
	char aLabel[160];
	auto SplitRow = [&](CUIRect &Area, CUIRect *pLabel, CUIRect *pControl) {
		Area.HSplitTop(L(31.0f), &Row, &Area);
		Area.HSplitTop(L(4.0f), 0, &Area);
		Row.VSplitLeft(Row.w * 0.39f, pLabel, pControl);
		pControl->VSplitLeft(L(6.0f), 0, pControl);
	};
	auto Stepper = [&](CUIRect Rect, int *pPrevious, int *pNext, const char *pValue) {
		Rect.VSplitLeft(L(29.0f), &Previous, &Value);
		Value.VSplitRight(L(29.0f), &Value, &Next);
		const int Delta = (DoButton_Menu(pPrevious, "-", 0, &Previous) ? -1 : 0) + (DoButton_Menu(pNext, "+", 0, &Next) ? 1 : 0);
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
		if(DoButton_Menu(&s_aVisibilityButtons[i], Localize(apVisibility[i]), g_Config.m_ClRoomVisibility == i, &Button, g_Config.m_ClRoomVisibility == i ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL) && !Disabled)
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
	int Delta = Stepper(Control, &s_MapPrevious, &s_MapNext, Localize(ModeDef.m_SelectableMap ? ModeDef.m_ppMapNames[g_Config.m_ClLocalServerMap] : "Automatic by Invasion floor"));
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
	UI()->DoLabelScaled(&Label, Localize(ModeDef.m_Pve ? "Mission difficulty" : "AI difficulty"), 11.0f, -1);
	str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerDifficulty);
	Delta = Stepper(Control, &s_DifficultyPrevious, &s_DifficultyNext, aLabel);
	g_Config.m_ClLocalServerDifficulty = clamp(g_Config.m_ClLocalServerDifficulty + Delta, 1, 50);

	if(!ModeDef.m_Pve)
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
		Delta = Stepper(Control, &s_InvasionPrevious, &s_InvasionNext, Localize(LocalInvasionStartName(g_Config.m_ClLocalServerInvasionStart)));
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
		const int Step = ModeDef.m_Rule == LOCAL_RULE_CTF_SCORE ? 25 : ModeDef.m_Rule == LOCAL_RULE_BALL_SCORE ? 1 : ModeDef.m_Rule >= LOCAL_RULE_DM_SCORE ? 5 : 1;
		const int Minimum = ModeDef.m_Rule == LOCAL_RULE_HORDE ? 0 : ModeDef.m_Rule == LOCAL_RULE_EXTRACTION ? 2 : 1;
		const int Maximum = ModeDef.m_Rule == LOCAL_RULE_EXTRACTION ? 15 : ModeDef.m_Rule == LOCAL_RULE_HORDE || ModeDef.m_Rule == LOCAL_RULE_BALL_SCORE ? 100 : 1000;
		*pRule = clamp(*pRule + Delta * Step, Minimum, Maximum);
	}

	Identity.HSplitTop(L(20.0f), &Row, &Identity);
	UI()->DoLabelScaled(&Row, Localize("Room details"), 12.0f, -1);
	SplitRow(Identity, &Label, &Control);
	UI()->DoLabelScaled(&Label, Localize("Room name"), 11.0f, -1);
	DoEditBox(g_Config.m_ClLocalServerName, &Control, g_Config.m_ClLocalServerName, sizeof(g_Config.m_ClLocalServerName), 11.0f, &s_NameOffset);
	SplitRow(Identity, &Label, &Control);
	UI()->DoLabelScaled(&Label, Localize("Password (optional)"), 11.0f, -1);
	DoEditBox(g_Config.m_ClLocalServerPassword, &Control, g_Config.m_ClLocalServerPassword, sizeof(g_Config.m_ClLocalServerPassword), 11.0f, &s_PasswordOffset, true);
	Identity.HSplitTop(L(7.0f), 0, &Identity);
	Identity.HSplitTop(L(31.0f), &Row, &Identity);
	if(DoButton_Menu(&s_Advanced, Localize(g_Config.m_ClLocalServerAdvanced ? "Hide advanced settings" : "Advanced settings"), g_Config.m_ClLocalServerAdvanced, &Row))
		g_Config.m_ClLocalServerAdvanced ^= 1;
	if(g_Config.m_ClLocalServerAdvanced)
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
				g_Config.m_ClLocalServerSeed = clamp(str_toint(s_aSeedText), 0, 32767);
		}
		if(ModeDef.m_Pve)
		{
			SplitRow(Identity, &Label, &Control);
			if(DoButton_CheckBox(&s_Roguelite, Localize("Roguelite Director"), g_Config.m_ClLocalServerRoguelite, &Label))
				g_Config.m_ClLocalServerRoguelite ^= 1;
			if(DoButton_CheckBox(&s_Contracts, Localize("Team contracts"), g_Config.m_ClLocalServerContracts && g_Config.m_ClLocalServerRoguelite, &Control) && g_Config.m_ClLocalServerRoguelite)
				g_Config.m_ClLocalServerContracts ^= 1;
		}
		SplitRow(Identity, &Label, &Control);
		UI()->DoLabelScaled(&Label, Localize("Port"), 10.0f, -1);
		str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerPort);
		Delta = Stepper(Control, &s_PortPrevious, &s_PortNext, aLabel);
		g_Config.m_ClLocalServerPort = clamp(g_Config.m_ClLocalServerPort + Delta, 1024, 65535);
		Identity.HSplitTop(L(18.0f), &Row, &Identity);
		UI()->DoLabelScaled(&Row, Localize(g_Config.m_ClRoomVisibility == ROOM_VISIBILITY_LAN ? "LAN binding" : "Managed automatically"), 9.0f, -1);
	}

	CLocalServerLaunchSettings Preview;
	BuildLocalServerLaunchSettings(&Preview);
	DrawMenuInset(&Footer, CUI::CORNER_ALL);
	Footer.Margin(L(8.0f), &Footer);
	CUIRect Status, Action;
	Footer.VSplitRight(L(164.0f), &Status, &Action);
	const char *pVisibility = apVisibility[g_Config.m_ClRoomVisibility];
	char aFinalSummary[512];
	if(Preview.m_pMode->m_Pve)
		str_format(aFinalSummary, sizeof(aFinalSummary), Localize("%s  ·  %s  ·  %s  ·  %d human slots  ·  Unofficial"), Localize(Preview.m_pMode->m_pName), Localize(pVisibility), Localize(Preview.m_pMapName), Preview.m_MaxClients);
	else
	{
		char aPopulation[96];
		if(Preview.m_Bots <= 0)
			str_copy(aPopulation, Localize("No bots"), sizeof(aPopulation));
		else
			str_format(aPopulation, sizeof(aPopulation), Localize("%s: %d"), Localize(LocalGamePopulationLabel(Mode)), Preview.m_Bots);
		str_format(aFinalSummary, sizeof(aFinalSummary), Localize("%s  ·  %s  ·  %s  ·  %d human slots  ·  %s  ·  Unofficial"), Localize(Preview.m_pMode->m_pName), Localize(pVisibility), Localize(Preview.m_pMapName), Preview.m_MaxClients, aPopulation);
	}
	UI()->DoLabelScaled(&Status, aFinalSummary, FitLabelFontSize(TextRender(), aFinalSummary, 10.0f, Status.w), -1);
	CClientAsyncStatus SteamStatus;
	Client()->SteamHostedGameStatus(&SteamStatus);
	CUIRect StatusDetail = Status;
	StatusDetail.y += L(23.0f);
	if(SteamStatus.m_State == CLIENT_ASYNC_FAILED)
		UI()->DoLabelScaled(&StatusDetail, Localize(SteamStatus.m_aErrorKey), 8.5f, -1, (int)StatusDetail.w);
	else if(m_LocalServerState == LOCAL_SERVER_FAILED)
		UI()->DoLabelScaled(&StatusDetail, Localize("Room creation failed. Your settings were kept; retry or open the log."), 8.5f, -1, (int)StatusDetail.w);
	const bool RelayUnavailable = !SteamAvailable && RoomVisibilityRequiresSteam(g_Config.m_ClRoomVisibility);
	CUIRect PrimaryAction = Action, SecondaryAction;
	const bool SteamHostActive = SteamStatus.m_State == CLIENT_ASYNC_WORKING || SteamStatus.m_State == CLIENT_ASYNC_SUCCEEDED;
	const bool ShowSecondary = m_LocalServerState == LOCAL_SERVER_RUNNING || m_LocalServerState == LOCAL_SERVER_STARTING || SteamHostActive || (m_LocalServerState == LOCAL_SERVER_FAILED && m_aLocalServerLogPath[0]);
	if(ShowSecondary)
	{
		Action.HSplitTop(L(25.0f), &SecondaryAction, &PrimaryAction);
		PrimaryAction.HSplitTop(L(5.0f), 0, &PrimaryAction);
	}
	const int PrimaryState = SteamStatus.m_State == CLIENT_ASYNC_WORKING ? ROOM_PRIMARY_CREATING_STEAM :
		m_LocalServerState == LOCAL_SERVER_STARTING ? ROOM_PRIMARY_STARTING_LOCAL :
		m_LocalServerState == LOCAL_SERVER_STOPPING ? ROOM_PRIMARY_STOPPING_LOCAL :
		m_LocalServerState == LOCAL_SERVER_RUNNING ? ROOM_PRIMARY_RESTART_LOCAL :
		SteamStatus.m_State == CLIENT_ASYNC_SUCCEEDED ? ROOM_PRIMARY_RESTART_STEAM : ROOM_PRIMARY_CREATE;
	if(DoButton_Menu(&s_Create, Localize(RoomPrimaryActionLabel(PrimaryState)), 0, &PrimaryAction, BUTTONSTYLE_ACCENT) && !RelayUnavailable && RoomPrimaryActionEnabled(PrimaryState))
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
	auto L = [LayoutDivisor](float Value) { return Value / LayoutDivisor; };

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
	auto FocusAvailable = [&](int Focus) {
		const int Mode = clamp(g_Config.m_ClLocalServerMode, 0, LocalGameModeCount() - 1);
		const bool Pve = LocalGameMode(Mode).m_Pve;
		if(Focus == FOCUS_SECTION_SERVER || Focus == FOCUS_SECTION_MAP || Focus == FOCUS_SECTION_RULES)
			return true;
		if(Focus >= FOCUS_SLOTS && Focus <= FOCUS_LAN)
			return s_LocalSection == 0;
		if(Focus >= FOCUS_MAP && Focus <= FOCUS_SEED)
		{
			if(s_LocalSection != 1)
				return false;
			if(Focus == FOCUS_MAP)
				return LocalGameMode(Mode).m_SelectableMap;
			if(Focus == FOCUS_INVASION_START)
				return Mode == LOCAL_MODE_INVASION;
			if(Focus == FOCUS_INVASION_FLOOR)
				return Mode == LOCAL_MODE_INVASION && g_Config.m_ClLocalServerInvasionStart == LOCAL_INVASION_CUSTOM_FLOOR;
			if(Focus == FOCUS_DIFFICULTY)
				return Mode != LOCAL_MODE_INVASION && Mode != LOCAL_MODE_TUTORIAL;
			if(Focus == FOCUS_BOTS)
				return !Pve;
			if(Focus == FOCUS_SEED)
				return !g_Config.m_ClLocalServerRandomSeed;
		}
		if(Focus >= FOCUS_ROGUELITE && Focus <= FOCUS_MODE_RULE)
		{
			if(s_LocalSection != 2)
				return false;
			if(Focus == FOCUS_ROGUELITE || Focus == FOCUS_CONTRACTS)
				return Pve;
			if(Focus == FOCUS_MODE_RULE)
				return Mode != LOCAL_MODE_INVASION && Mode != LOCAL_MODE_TUTORIAL;
		}
		if((Focus == FOCUS_RESTART || Focus == FOCUS_STOP) && m_LocalServerState != LOCAL_SERVER_RUNNING)
			return false;
		return true;
	};
	auto SectionFocus = [](int Section) {
		return Section == 0 ? FOCUS_SECTION_SERVER : (Section == 1 ? FOCUS_SECTION_MAP : FOCUS_SECTION_RULES);
	};
	auto AdjustModeRule = [&](int Direction) {
		const int Mode = clamp(g_Config.m_ClLocalServerMode, 0, LocalGameModeCount() - 1);
		if(Mode == LOCAL_MODE_HORDE)
		{
			if(Direction > 0)
				g_Config.m_ClLocalServerHordeWaves = g_Config.m_ClLocalServerHordeWaves <= 0 ? 4 : min(100, g_Config.m_ClLocalServerHordeWaves + 4);
			else
				g_Config.m_ClLocalServerHordeWaves = g_Config.m_ClLocalServerHordeWaves <= 4 ? 0 : g_Config.m_ClLocalServerHordeWaves - 4;
		}
		else if(Mode == LOCAL_MODE_EXTRACTION)
			g_Config.m_ClLocalServerExtractionTime = clamp(g_Config.m_ClLocalServerExtractionTime + Direction, 2, 15);
		else if(Mode == LOCAL_MODE_DM)
			g_Config.m_ClLocalServerDmScore = clamp(g_Config.m_ClLocalServerDmScore + Direction * 5, 1, 1000);
		else if(Mode == LOCAL_MODE_TDM)
			g_Config.m_ClLocalServerTdmScore = clamp(g_Config.m_ClLocalServerTdmScore + Direction * 5, 1, 1000);
		else if(Mode == LOCAL_MODE_CTF)
			g_Config.m_ClLocalServerCtfScore = clamp(g_Config.m_ClLocalServerCtfScore + Direction * 25, 1, 1000);
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
			const bool Down = Event.m_Key == KEY_DOWN || Event.m_Key == KEY_TAB || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN;
			const bool Left = Event.m_Key == KEY_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT;
			const bool Right = Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT;
			const bool Confirm = Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A || Event.m_Key == KEY_GAMEPAD_BUTTON_START;
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
				}
				while(!FocusAvailable(m_LocalServerFocus));
				continue;
			}
			if(Left || Right)
			{
				const int Direction = Right ? 1 : -1;
				if(m_LocalServerFocus == FOCUS_MODE)
				{
					const int ModeCount = LocalGameModeCount();
					g_Config.m_ClLocalServerMode = (g_Config.m_ClLocalServerMode + Direction + ModeCount) % ModeCount;
					g_Config.m_ClLocalServerMap = clamp(g_Config.m_ClLocalServerMap, 0, LocalGameMode(g_Config.m_ClLocalServerMode).m_MapCount - 1);
				}
				else if(m_LocalServerFocus == FOCUS_MAP)
				{
					const int Count = LocalGameMode(g_Config.m_ClLocalServerMode).m_MapCount;
					g_Config.m_ClLocalServerMap = (g_Config.m_ClLocalServerMap + Direction + Count) % Count;
				}
				else if(m_LocalServerFocus == FOCUS_INVASION_START)
					g_Config.m_ClLocalServerInvasionStart = (g_Config.m_ClLocalServerInvasionStart + Direction + 3) % 3;
				else if(m_LocalServerFocus == FOCUS_INVASION_FLOOR)
					g_Config.m_ClLocalServerInvasionFloor = clamp(g_Config.m_ClLocalServerInvasionFloor + Direction, 1, max(1, g_Config.m_ClPveHighestInvasion));
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
				else if(m_LocalServerFocus == FOCUS_SECTION_SERVER || m_LocalServerFocus == FOCUS_SECTION_MAP || m_LocalServerFocus == FOCUS_SECTION_RULES)
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
					g_Config.m_ClLocalServerSeed = clamp(g_Config.m_ClLocalServerSeed + Direction, 0, 32767);
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
		FormatLocalServerSummary(PreviewSettings, m_LocalServerActualPort > 0 ? m_LocalServerActualPort : PreviewSettings.m_Port, m_aLocalServerSummary, sizeof(m_aLocalServerSummary));
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
	UI()->DoLabelScaled(&Header, Localize("Choose a mode, expand a category, then start and join in one click."), 11.0f, -1);
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
	Modes.HSplitTop(L(20.0f), &Row, &Modes);
	UI()->DoLabelScaled(&Row, Localize("ROGUELITE PVE"), 12.0f, -1);
	for(int i = 0; i < LocalGameModeCount(); i++)
	{
		if(i == LOCAL_MODE_DM)
		{
			Modes.HSplitTop(L(8.0f), 0, &Modes);
			Modes.HSplitTop(L(20.0f), &Row, &Modes);
			UI()->DoLabelScaled(&Row, Localize("COMPETITIVE"), 12.0f, -1);
		}
		Modes.HSplitTop(L(30.0f), &Row, &Modes);
		if(DoButton_Menu(&s_aModeButtons[i], Localize(s_aLocalGameModes[i].m_pName), g_Config.m_ClLocalServerMode == i, &Row, g_Config.m_ClLocalServerMode == i ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
		{
			g_Config.m_ClLocalServerMode = i;
			g_Config.m_ClLocalServerMap = clamp(g_Config.m_ClLocalServerMap, 0, LocalGameMode(i).m_MapCount - 1);
		}
		Modes.HSplitTop(L(4.0f), 0, &Modes);
	}
	Modes.HSplitTop(L(8.0f), 0, &Modes);
	UI()->DoLabelScaled(&Modes, Localize(s_aLocalGameModes[g_Config.m_ClLocalServerMode].m_pDescription), 10.0f, -1, (int)Modes.w);

	const CLocalGameMode &ModeDef = LocalGameMode(g_Config.m_ClLocalServerMode);
	const int MapCount = ModeDef.m_MapCount;
	g_Config.m_ClLocalServerMap = clamp(g_Config.m_ClLocalServerMap, 0, MapCount - 1);
	const char *pMapName = ModeDef.m_ppMapNames[g_Config.m_ClLocalServerMap];

	auto SplitSettingRow = [&Settings, &L](CUIRect *pLabel, CUIRect *pControl) {
		CUIRect Full;
		Settings.HSplitTop(L(31.0f), &Full, &Settings);
		Settings.HSplitTop(L(4.0f), 0, &Settings);
		const float LabelWidth = clamp(Full.w * 0.34f, 162.0f, 210.0f);
		Full.VSplitLeft(L(LabelWidth), pLabel, pControl);
		pControl->VSplitLeft(L(8.0f), 0, pControl);
	};
	auto DrawFocusMarker = [this](const CUIRect &Rect, int Focus) {
		if(m_LocalServerFocus != Focus)
			return;
		CUIRect Marker = Rect;
		Marker.w = 3.0f * UI()->Scale();
		RenderTools()->DrawUIRect(&Marker, ms_ColorAccent, CUI::CORNER_ALL, 1.0f);
	};

	CUIRect Label, Control, Previous, Next, Value;
	char aLabel[128];
	auto DrawSectionHeader = [&](int Section, const char *pTitle, int Focus) {
		CUIRect SectionHeader;
		Settings.HSplitTop(L(31.0f), &SectionHeader, &Settings);
		Settings.HSplitTop(L(4.0f), 0, &Settings);
		const bool Expanded = s_LocalSection == Section;
		char aSectionTitle[96];
		str_format(aSectionTitle, sizeof(aSectionTitle), "%s  %s", Expanded ? "−" : "+", Localize(pTitle));
		if(DoButton_Menu(&s_aSectionButtons[Section], aSectionTitle, Expanded, &SectionHeader, Expanded ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
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
		DoEditBox(g_Config.m_ClLocalServerName, &Control, g_Config.m_ClLocalServerName, sizeof(g_Config.m_ClLocalServerName), 12.0f, &s_NameOffset);

		SplitSettingRow(&Label, &Control);
		UI()->DoLabelScaled(&Label, Localize("Password (optional)"), 12.0f, -1);
		DoEditBox(g_Config.m_ClLocalServerPassword, &Control, g_Config.m_ClLocalServerPassword, sizeof(g_Config.m_ClLocalServerPassword), 12.0f, &s_PasswordOffset, true);

		SplitSettingRow(&Label, &Control);
		DrawFocusMarker(Label, FOCUS_SLOTS);
		str_format(aLabel, sizeof(aLabel), "%s: %d", Localize("Human slots"), g_Config.m_ClLocalServerMaxClients);
		UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
		Control.HMargin(L(5.0f), &Control);
		g_Config.m_ClLocalServerMaxClients = 1 + (int)(DoScrollbarH(&g_Config.m_ClLocalServerMaxClients, &Control, (g_Config.m_ClLocalServerMaxClients - 1) / 15.0f) * 15.0f + 0.5f);

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
			UI()->DoLabelScaled(&Value, Localize(LocalInvasionStartName(g_Config.m_ClLocalServerInvasionStart)), 11.0f, 0);

			if(g_Config.m_ClLocalServerInvasionStart == LOCAL_INVASION_CUSTOM_FLOOR)
			{
				const int MaxFloor = max(1, g_Config.m_ClPveHighestInvasion);
				g_Config.m_ClLocalServerInvasionFloor = clamp(g_Config.m_ClLocalServerInvasionFloor, 1, MaxFloor);
				SplitSettingRow(&Label, &Control);
				DrawFocusMarker(Label, FOCUS_INVASION_FLOOR);
				str_format(aLabel, sizeof(aLabel), "%s: %d", Localize("Starting floor"), g_Config.m_ClLocalServerInvasionFloor);
				UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
				Control.VSplitLeft(L(30.0f), &Previous, &Value);
				Value.VSplitRight(L(30.0f), &Value, &Next);
				if(DoButton_Menu(&s_InvasionFloorPrevious, "-", 0, &Previous))
					g_Config.m_ClLocalServerInvasionFloor = max(1, g_Config.m_ClLocalServerInvasionFloor - 1);
				if(DoButton_Menu(&s_InvasionFloorNext, "+", 0, &Next))
					g_Config.m_ClLocalServerInvasionFloor = min(MaxFloor, g_Config.m_ClLocalServerInvasionFloor + 1);
				if(MaxFloor > 1)
					g_Config.m_ClLocalServerInvasionFloor = 1 + (int)(DoScrollbarH(&g_Config.m_ClLocalServerInvasionFloor, &Value, (g_Config.m_ClLocalServerInvasionFloor - 1) / (float)(MaxFloor - 1)) * (MaxFloor - 1) + 0.5f);
				else
					UI()->DoLabelScaled(&Value, Localize("Complete more floors to unlock"), 9.0f, 0);
			}
		}
		else
		{
			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_DIFFICULTY);
			const char *pDifficultyLabel = ModeDef.m_Pve ? "Mission difficulty" : "AI difficulty";
			str_format(aLabel, sizeof(aLabel), "%s: %d", Localize(pDifficultyLabel), g_Config.m_ClLocalServerDifficulty);
			UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
			Control.HMargin(L(5.0f), &Control);
			g_Config.m_ClLocalServerDifficulty = 1 + (int)(DoScrollbarH(&g_Config.m_ClLocalServerDifficulty, &Control, (g_Config.m_ClLocalServerDifficulty - 1) / 49.0f) * 49.0f + 0.5f);
		}

		SplitSettingRow(&Label, &Control);
		if(ModeDef.m_Pve)
		{
			UI()->DoLabelScaled(&Label, Localize("Enemy scaling"), 12.0f, -1);
			UI()->DoLabelScaled(&Control, Localize(g_Config.m_ClLocalServerMode == LOCAL_MODE_INVASION || g_Config.m_ClLocalServerMode == LOCAL_MODE_TUTORIAL ? "Automatic by floor and party size" : "Health, elites and party size"), 11.0f, -1);
		}
		else
		{
			DrawFocusMarker(Label, FOCUS_BOTS);
			g_Config.m_ClLocalServerBots = clamp(g_Config.m_ClLocalServerBots, 0, 16);
			if(g_Config.m_ClLocalServerBots == 0)
				str_format(aLabel, sizeof(aLabel), "%s: %s", Localize(LocalGamePopulationLabel(g_Config.m_ClLocalServerMode)), Localize("No bots"));
			else
				str_format(aLabel, sizeof(aLabel), "%s: %d", Localize(LocalGamePopulationLabel(g_Config.m_ClLocalServerMode)), g_Config.m_ClLocalServerBots);
			UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
			Control.HMargin(L(5.0f), &Control);
			const int MaxBots = 16;
			if(MaxBots > 0)
				g_Config.m_ClLocalServerBots = (int)(DoScrollbarH(&g_Config.m_ClLocalServerBots, &Control, g_Config.m_ClLocalServerBots / (float)MaxBots) * MaxBots + 0.5f);
			else
				g_Config.m_ClLocalServerBots = 0;
		}

		SplitSettingRow(&Label, &Control);
		DrawFocusMarker(Label, FOCUS_RANDOM_SEED);
		if(DoButton_CheckBox(&s_RandomSeedButton, Localize("Random map seed"), g_Config.m_ClLocalServerRandomSeed, &Label))
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
				g_Config.m_ClLocalServerSeed = clamp(str_toint(s_aSeedText), 0, 32767);
				s_SeedTextValue = g_Config.m_ClLocalServerSeed;
			}
			if(!CLineInput::GetActiveInput() && !s_aSeedText[0])
				s_SeedTextValue = -1;
		}
	}

	if(DrawSectionHeader(2, ModeDef.m_Pve ? "PvE rules" : "Match rules", FOCUS_SECTION_RULES))
	{
		if(ModeDef.m_Pve)
		{
			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_ROGUELITE);
			if(DoButton_CheckBox(&s_RogueliteButton, Localize("Roguelite Director"), g_Config.m_ClLocalServerRoguelite, &Label))
				g_Config.m_ClLocalServerRoguelite ^= 1;
			UI()->DoLabelScaled(&Control, Localize("Perks, resources and research"), 10.0f, -1);

			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_CONTRACTS);
			if(DoButton_CheckBox(&s_ContractsButton, Localize("Team contracts"), g_Config.m_ClLocalServerContracts && g_Config.m_ClLocalServerRoguelite, &Label) && g_Config.m_ClLocalServerRoguelite)
				g_Config.m_ClLocalServerContracts ^= 1;
			UI()->DoLabelScaled(&Control, Localize(g_Config.m_ClLocalServerRoguelite ? "Offer optional team challenges" : "Requires Roguelite Director"), 10.0f, -1);
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
		UI()->DoLabelScaled(&RuleNote, Localize("Rules apply the next time the server starts."), 10.0f, -1, (int)RuleNote.w);
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
	const char *pSummary = (m_LocalServerState == LOCAL_SERVER_STOPPED || !m_aLocalServerSummary[0]) ? aPreviewSummary : m_aLocalServerSummary;
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
		if(DoButton_Menu(&s_StartButton, Localize("Start and join"), m_LocalServerFocus == FOCUS_PRIMARY_ACTION, &Button, BUTTONSTYLE_ACCENT))
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
		if(DoButton_Menu(&s_StopButton, Localize("Stop"), m_LocalServerFocus == FOCUS_STOP, &Button, BUTTONSTYLE_DANGER))
			StopLocalServer(false);
		Actions.VSplitRight(L(6.0f), &Actions, 0);
		Actions.VSplitRight(L(92.0f), &Actions, &Button);
		if(DoButton_Menu(&s_RestartButton, Localize("Restart"), m_LocalServerFocus == FOCUS_RESTART, &Button))
			StopLocalServer(true);
		Actions.VSplitRight(L(6.0f), &Actions, 0);
		if(!IsConnectedToLocalServer())
		{
			Actions.VSplitRight(L(92.0f), &Actions, &Button);
			if(DoButton_Menu(&s_JoinButton, Localize("Join"), m_LocalServerFocus == FOCUS_PRIMARY_ACTION, &Button, BUTTONSTYLE_ACCENT))
				JoinLocalServer();
		}
	}
	else
	{
		Actions.VSplitRight(L(110.0f), 0, &Button);
		if(DoButton_Menu(&s_StopButton, Localize("Cancel"), m_LocalServerFocus == FOCUS_PRIMARY_ACTION, &Button, BUTTONSTYLE_DANGER))
			StopLocalServer(false);
	}
}


void CMenus::RenderFront(CUIRect MainView)
{
	s_ResetMenu = false;
	const float ScaleDivisor = max(1.0f, UI()->Scale());
	auto L = [ScaleDivisor](float Value) { return Value / ScaleDivisor; };
	CUIRect Screen = MainView;
	DrawMenuPanel(&Screen, CUI::CORNER_ALL);
	Screen.Margin(L(14.0f), &Screen);
	CUIRect Header, Hero, Status, Cards;
	Screen.HSplitTop(L(46.0f), &Header, &Screen);
	if(gs_TextureLogo == -1)
		gs_TextureLogo = Graphics()->LoadTexture("logo.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	Graphics()->TextureSet(gs_TextureLogo);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem Logo(Header.x, Header.y + L(2.0f), min(L(226.0f), Header.w * 0.34f), L(34.0f));
	Graphics()->QuadsDrawTL(&Logo, 1);
	Graphics()->QuadsEnd();
	CUIRect HeaderText;
	Header.VSplitLeft(min(L(245.0f), Header.w * 0.37f), 0, &HeaderText);
	UI()->DoLabelScaled(&HeaderText, Localize("TACTICAL COMMAND"), L(9.0f), -1);
	HeaderText.HSplitTop(L(17.0f), 0, &HeaderText);
	UI()->DoLabelScaled(&HeaderText, Localize("Choose how to deploy"), L(15.0f), -1);
	DrawAccentUnderline(&Header);

	Screen.HSplitTop(L(8.0f), 0, &Screen);
	Screen.HSplitTop(L(126.0f), &Hero, &Screen);
	DrawMenuInset(&Hero, CUI::CORNER_ALL);
	CUIRect HeroAccent = Hero;
	HeroAccent.w = L(5.0f);
	RenderTools()->DrawUIRect(&HeroAccent, ms_ColorAccent, CUI::CORNER_L, ms_ControlRounding);
	Hero.Margin(L(16.0f), &Hero);
	CMenuHomeState HomeState = {
		m_LocalServerState == LOCAL_SERVER_STARTING,
		m_LocalServerState == LOCAL_SERVER_RUNNING,
		IsConnectedToLocalServer(),
		g_Config.m_ClTutorialState == 1,
		g_Config.m_ClTutorialChapter};
	const CMenuHomePrimary Primary = ResolveMenuHomePrimary(HomeState);
	CUIRect HeroActions, HeroCopy = Hero;
	Hero.VSplitRight(min(L(238.0f), Hero.w * .38f), &HeroCopy, &HeroActions);
	CUIRect Eyebrow, Title, Description;
	HeroCopy.HSplitTop(L(18.0f), &Eyebrow, &HeroCopy);
	TextRender()->TextColor(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, 1.0f);
	UI()->DoLabelScaled(&Eyebrow, Localize("RECOMMENDED ACTION"), L(8.5f), -1);
	TextRender()->TextColor(1, 1, 1, 1);
	HeroCopy.HSplitTop(L(31.0f), &Title, &Description);
	UI()->DoLabelScaled(&Title, Localize(Primary.m_pTitle), L(21.0f), -1);
	char aDescription[192];
	if(Primary.m_Chapter)
		str_format(aDescription, sizeof(aDescription), Localize(Primary.m_pDescription), Primary.m_Chapter);
	else
		str_copy(aDescription, Localize(Primary.m_pDescription), sizeof(aDescription));
	UI()->DoLabelScaled(&Description, aDescription, L(10.0f), -1, (int)Description.w);
	HeroActions.VSplitLeft(L(10.0f), 0, &HeroActions);
	CUIRect PrimaryButton, SecondaryButton;
	HeroActions.HSplitTop(L(42.0f), &PrimaryButton, &HeroActions);
	HeroActions.HSplitTop(L(7.0f), 0, &HeroActions);
	HeroActions.HSplitTop(L(31.0f), &SecondaryButton, &HeroActions);
	static int s_Primary, s_Browse;
	const bool KeyboardActivate = !m_NavigationHasFocus && m_LastInputDevice != 0 && m_EnterPressed;
	if(DoButton_Menu(&s_Primary, Localize(Primary.m_pTitle), true, &PrimaryButton, BUTTONSTYLE_ACCENT) || KeyboardActivate)
	{
		m_EnterPressed = false;
		if(Primary.m_Action == MENU_HOME_JOIN_LOCAL)
			JoinLocalServer();
		else if(Primary.m_Action == MENU_HOME_SHOW_LOCAL)
		{
			if(IsConnectedToLocalServer()) SetActive(false);
			else { m_PlayTab = 1; g_Config.m_UiPage = PAGE_LOCAL_SERVER; }
		}
		else if(Primary.m_Action == MENU_HOME_CONTINUE_TUTORIAL)
			StartTutorial(Primary.m_Chapter, true);
		else
		{
			m_PlayTab = 1;
			g_Config.m_UiPage = PAGE_LOCAL_SERVER;
		}
	}
	if(DoButton_Menu(&s_Browse, Localize("Browse rooms"), 0, &SecondaryButton))
	{
		m_PlayTab = 0;
		ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
		g_Config.m_UiPage = PAGE_INTERNET;
	}

	Screen.HSplitTop(L(8.0f), 0, &Screen);
	Screen.HSplitTop(L(28.0f), &Status, &Screen);
	const float BadgeGap = L(6.0f);
	CUIRect TutorialBadge, ServerBadge, NetworkBadge;
	Status.VSplitLeft((Status.w - BadgeGap * 2.0f) / 3.0f, &TutorialBadge, &Status);
	Status.VSplitLeft(BadgeGap, 0, &Status);
	Status.VSplitLeft((Status.w - BadgeGap) / 2.0f, &ServerBadge, &Status);
	Status.VSplitLeft(BadgeGap, 0, &Status);
	NetworkBadge = Status;
	char aTutorial[96];
	if(g_Config.m_ClTutorialState == 1)
		str_format(aTutorial, sizeof(aTutorial), Localize("Training · chapter %d/6"), clamp(g_Config.m_ClTutorialChapter, 1, 6));
	else
		str_copy(aTutorial, Localize(g_Config.m_ClTutorialState == 2 ? "Training · complete" : g_Config.m_ClTutorialState == 3 ? "Training · skipped" : "Training · not started"), sizeof(aTutorial));
	DrawStatusBadge(TutorialBadge, aTutorial, g_Config.m_ClTutorialState == 2 ? ms_ColorAccentDim : ms_ColorAccent);
	const char *pServerStatus = m_LocalServerState == LOCAL_SERVER_RUNNING ? "Local server · running" : m_LocalServerState == LOCAL_SERVER_STARTING ? "Local server · starting" : m_LocalServerState == LOCAL_SERVER_FAILED ? "Local server · attention" : "Local server · idle";
	DrawStatusBadge(ServerBadge, Localize(pServerStatus), m_LocalServerState == LOCAL_SERVER_FAILED ? ms_ColorDanger : (m_LocalServerState == LOCAL_SERVER_RUNNING ? ms_ColorAccentDim : vec4(.62f, .66f, .72f, 1.0f)));
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	DrawStatusBadge(NetworkBadge, Localize(pPlatform && pPlatform->Available() ? "Steam · online" : "Standalone · UDP ready"), ms_ColorAccentDim);

	Screen.HSplitTop(L(8.0f), 0, &Cards);
	const float Gap = L(8.0f);
	// Use effective width so raising UI scale selects the same denser layout as
	// a physically smaller window.
	const float ResponsiveWidth = UI()->Screen()->w / max(1.0f, UI()->Scale());
	const bool Wide = ResponsiveWidth >= 1050.0f;
	const bool Medium = !Wide && ResponsiveWidth >= 700.0f;
	CUIRect CardRects[3];
	static CScrollRegion s_HomeScroll;
	bool HomeScrollActive = false;
	if(Wide)
	{
		CUIRect Remaining = Cards;
		Remaining.VSplitLeft((Remaining.w - Gap * 2.0f) / 3.0f, &CardRects[0], &Remaining);
		Remaining.VSplitLeft(Gap, 0, &Remaining);
		Remaining.VSplitLeft((Remaining.w - Gap) / 2.0f, &CardRects[1], &Remaining);
		Remaining.VSplitLeft(Gap, 0, &Remaining);
		CardRects[2] = Remaining;
	}
	else if(Medium)
	{
		Cards.HSplitTop((Cards.h - Gap) * .54f, &CardRects[0], &CardRects[2]);
		CUIRect FirstRow = CardRects[0];
		FirstRow.VSplitLeft((FirstRow.w - Gap) * .5f, &CardRects[0], &CardRects[1]);
		CardRects[1].VSplitLeft(Gap, 0, &CardRects[1]);
		CardRects[2].HSplitTop(Gap, 0, &CardRects[2]);
	}
	else
	{
		vec2 ScrollOffset(0.0f, 0.0f);
		CScrollRegionParams Params;
		ConfigureScrollRegion(&Params);
		Params.m_ScrollUnit = L(116.0f);
		s_HomeScroll.Begin(&Cards, &ScrollOffset, &Params);
		HomeScrollActive = true;
		Cards.y += ScrollOffset.y;
		Cards.VSplitRight(L(18.0f), &Cards, 0);
		for(int i = 0; i < 3; i++)
		{
			Cards.HSplitTop(L(150.0f), &CardRects[i], &Cards);
			Cards.HSplitTop(Gap, 0, &Cards);
		}
	}

	static int s_Training, s_Pve, s_Pvp;
	auto Card = [&](CUIRect Rect, int Mode, const vec4 &Color, const char *pTitle, const char *pBody, const char *pMeta, const void *pButtonID, const char *pButton, int Action) {
		DrawMenuInset(&Rect, CUI::CORNER_ALL);
		CUIRect Art, Content = Rect;
		const float ArtHeight = min(L(58.0f), Rect.h * .37f);
		Content.HSplitTop(ArtHeight, &Art, &Content);
		DrawPlayArtwork(Art, Mode, Color);
		Content.Margin(L(9.0f), &Content);
		CUIRect Line, Button;
		Content.HSplitTop(L(19.0f), &Line, &Content);
		UI()->DoLabelScaled(&Line, Localize(pTitle), L(13.0f), -1);
		Content.HSplitTop(L(15.0f), &Line, &Content);
		TextRender()->TextColor(Color.r, Color.g, Color.b, 1.0f);
		UI()->DoLabelScaled(&Line, Localize(pMeta), FitLabelFontSize(TextRender(), Localize(pMeta), L(8.5f), Line.w), -1);
		TextRender()->TextColor(1, 1, 1, 1);
		Content.HSplitBottom(L(28.0f), &Line, &Button);
		UI()->DoLabelScaled(&Line, Localize(pBody), L(8.5f), -1, (int)Line.w);
		if(DoButton_Menu(pButtonID, Localize(pButton), 0, &Button))
		{
			if(Action == 1) OpenTutorialChapterSelect();
			else if(Action == 2 || Action == 3)
			{
				m_PlayTab = 1;
				m_CreateRoomStep = CREATE_ROOM_CHOOSE_MODE;
				m_LocalServerFocus = Action == 2 ? LOCAL_MODE_INVASION : LOCAL_MODE_DM;
				g_Config.m_UiPage = PAGE_LOCAL_SERVER;
			}
		}
	};
	Card(CardRects[0], 0, ms_ColorAccent, "Training", "Six guided chapters, always replayable.", "SOLO · 30–45 MIN", &s_Training, "Choose chapter", 1);
	Card(CardRects[1], 1, ms_ColorAccentDim, "Co-op PvE", "Invasion, Horde, Extraction and Reactor Defense.", "4 PVE MODES · CO-OP", &s_Pve, "Configure PvE", 2);
	Card(CardRects[2], 2, vec4(.78f, .32f, .28f, 1.0f), "Local PvP", "Eight competitive modes with adjustable match population.", "8 PVP MODES · SOLO / LAN / STEAM", &s_Pvp, "Choose PvP mode", 3);
	if(HomeScrollActive)
	{
		CUIRect ScrollContent = CardRects[0];
		ScrollContent.h = CardRects[2].y + CardRects[2].h - CardRects[0].y;
		s_HomeScroll.AddRect(ScrollContent);
		s_HomeScroll.End();
	}
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

	const char *apNames[NUM_TUTORIAL_CHAPTERS] = {
		"First Deployment", "Combat and Recovery", "PvE Mission", "Forge and Build", "Build and Growth", "Multiplayer Ready"};
	const char *apDescriptions[NUM_TUTORIAL_CHAPTERS] = {
		"Movement, weapons and the training target.",
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
		CUIRect Card = {Body.x + (Index % 3) * (ColumnWidth + Gap), Body.y + (Index / 3) * (RowHeight + Gap), ColumnWidth, RowHeight};
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
		const bool Unlocked = TutorialChapterUnlocked(Chapter, g_Config.m_ClTutorialCompletedMask, g_Config.m_ClTutorialChapter, HasCurrentProgress);
		char aStatus[96];
		const char *pAction;
		if(InProgress)
		{
			str_format(aStatus, sizeof(aStatus), Localize("Step %d of %d"), clamp(g_Config.m_ClTutorialStep + 1, 1, TutorialStepCount(Chapter)), TutorialStepCount(Chapter));
			pAction = "Continue";
		}
		else if(Completed)
		{
			str_copy(aStatus, Localize("Completed"), sizeof(aStatus));
			pAction = "Replay";
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
		if(DoButton_Menu(&s_aChapterButtons[Index], Localize(pAction), InProgress, &Button, InProgress || (!Completed && Unlocked) ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL) && Unlocked)
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
	UI()->DoLabelScaled(&Subtitle, Localize(g_Config.m_ClTutorialStep == 1 ? "Configure a simulated room, then create it." : "Filter the simulated room list, then join the training room."), 9.5f, -1);

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
			if(DoButton_Menu(&s_aVisibility[i], Localize(apVisibility[i]), g_Config.m_ClRoomVisibility == i, &Button, g_Config.m_ClRoomVisibility == i ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
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
			DoButton_Menu(&s_aFilters[i], Localize(apFilters[i]), i == 0, &Button, i == 0 ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL);
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
	UI()->DoLabelScaled(&Footer, Localize("This practice does not publish a real room or contact external services."), 9.0f, -1);
}




void CMenus::RenderSteam(CUIRect MainView)
{
	// Compatibility entry point for old saved PAGE_STEAM values. The player UI
	// intentionally exposes only Mod consumption/management.
	RenderMods(MainView);
	return;
	#if 0
	IPlatformServices *pPlatform=Kernel()->RequestInterface<IPlatformServices>();
	DrawMenuPanel(&MainView,CUI::CORNER_ALL);MainView.Margin(8.0f,&MainView);
	if(!pPlatform||!pPlatform->Available()){UI()->DoLabelScaled(&MainView,Localize("Steam unavailable"),16.0f,0);return;}
	static int s_View=0,s_RoomTab=0,s_WorkshopTab=0;CUIRect Tabs,Body,Left,Right;MainView.HSplitTop(28.0f,&Tabs,&Body);Tabs.VSplitLeft(120.0f,&Left,&Tabs);Tabs.VSplitLeft(120.0f,&Right,&Tabs);
	if(DoButton_MenuTab(&s_RoomTab,Localize("Rooms"),s_View==0,&Left,CUI::CORNER_TL))s_View=0;
	if(DoButton_MenuTab(&s_WorkshopTab,"Workshop",s_View==1,&Right,CUI::CORNER_TR))s_View=1;
	Body.HSplitTop(6.0f,0,&Body);
	if(s_View==0)
	{
		static int s_Refresh=0,s_Private=0,s_Friends=0,s_Public=0,s_Invite=0,s_Leave=0,s_Selected=-1;static float s_Scroll=0.0f;CUIRect Toolbar,List,Actions,Button;
		Body.HSplitTop(32.0f,&Toolbar,&List);List.HSplitBottom(42.0f,&List,&Actions);
		Toolbar.VSplitLeft(86.0f,&Button,&Toolbar);if(DoButton_Menu(&s_Refresh,Localize("Refresh"),0,&Button))pPlatform->RefreshLobbyList();
		Toolbar.VSplitLeft(4.0f,0,&Toolbar);Toolbar.VSplitLeft(94.0f,&Button,&Toolbar);if(DoButton_Menu(&s_Private,Localize("Invite only"),0,&Button))Console()->ExecuteLine("steam_lobby_create invite");
		Toolbar.VSplitLeft(4.0f,0,&Toolbar);Toolbar.VSplitLeft(94.0f,&Button,&Toolbar);if(DoButton_Menu(&s_Friends,Localize("Friends"),0,&Button,BUTTONSTYLE_ACCENT))Console()->ExecuteLine("steam_lobby_create friends");
		Toolbar.VSplitLeft(4.0f,0,&Toolbar);Toolbar.VSplitLeft(94.0f,&Button,&Toolbar);if(DoButton_Menu(&s_Public,Localize("Public"),0,&Button))Console()->ExecuteLine("steam_lobby_create public");
		Toolbar.VSplitLeft(4.0f,0,&Toolbar);Toolbar.VSplitLeft(86.0f,&Button,&Toolbar);if(DoButton_Menu(&s_Invite,Localize("Invite"),0,&Button))pPlatform->OpenLobbyInviteDialog();
		Toolbar.VSplitLeft(4.0f,0,&Toolbar);Toolbar.VSplitLeft(86.0f,&Button,&Toolbar);if(DoButton_Menu(&s_Leave,Localize("Leave"),0,&Button))pPlatform->LeaveLobby();
		static int s_aRoomIDs[128];for(int i=0;i<128;i++)s_aRoomIDs[i]=i;
		UiDoListboxStart(&s_aRoomIDs,&List,34.0f,Localize("Steam rooms"),"",pPlatform->LobbyCount(),1,s_Selected,s_Scroll);
		for(int i=0;i<pPlatform->LobbyCount();i++){CPlatformLobbyInfo Info;pPlatform->LobbyInfo(i,&Info);CListboxItem Item=UiDoListboxNextItem(&s_aRoomIDs[i],s_Selected==i);if(Item.m_Visible){char aLine[512];str_format(aLine,sizeof(aLine),"%s  |  %s  |  %s  |  %d/%d%s%s%s",Info.m_aHostName[0]?Info.m_aHostName:"Steam host",Info.m_aGameType,Info.m_aMap,Info.m_Members,Info.m_MaxMembers,Info.m_FriendHosted?"  FRIEND":"",Info.m_Password?"  PASSWORD":"",Info.m_Modded?"  MODDED":"");Item.m_Rect.Margin(6.0f,&Item.m_Rect);UI()->DoLabelScaled(&Item.m_Rect,aLine,11.0f,-1);}}
		s_Selected=UiDoListboxEnd(&s_Scroll,0);s_Selected=clamp(s_Selected,-1,max(-1,pPlatform->LobbyCount()-1));
		Actions.VSplitRight(110.0f,&Actions,&Button);static int s_Join=0;if(DoButton_Menu(&s_Join,Localize("Join"),0,&Button,BUTTONSTYLE_ACCENT)&&s_Selected>=0){CPlatformLobbyInfo Info;if(pPlatform->LobbyInfo(s_Selected,&Info))pPlatform->JoinLobby(Info.m_LobbyID);}
		if(s_Selected>=0){CPlatformLobbyInfo Info;if(pPlatform->LobbyInfo(s_Selected,&Info)){char aDetail[512];str_format(aDetail,sizeof(aDetail),"SteamID %llu  |  %s  |  %s",Info.m_HostSteamID,Info.m_aRegion,Info.m_aModHash);UI()->DoLabelScaled(&Actions,aDetail,10.0f,-1);}}
	}
	else
	{
		static int s_Refresh=0,s_Create=0,s_Select=0,s_Enable=0,s_Disable=0,s_Remove=0,s_Community=0,s_Publish=0,s_Selected=-1;static float s_Scroll=0.0f,s_ContentOffset=0.0f,s_PreviewOffset=0.0f;static char s_aContent[512]="",s_aPreview[512]="";CUIRect Toolbar,List,Detail,Button,Row,Label,Edit;
		Body.HSplitTop(32.0f,&Toolbar,&Body);Toolbar.VSplitLeft(100.0f,&Button,&Toolbar);if(DoButton_Menu(&s_Refresh,Localize("Refresh"),0,&Button))pPlatform->RefreshWorkshopItems();Toolbar.VSplitLeft(6.0f,0,&Toolbar);Toolbar.VSplitLeft(130.0f,&Button,&Toolbar);if(DoButton_Menu(&s_Create,Localize("Create item"),0,&Button))pPlatform->CreateWorkshopItem();
		Body.VSplitLeft(Body.w*0.62f,&List,&Detail);List.VSplitRight(8.0f,&List,0);static int s_aItemIDs[256];for(int i=0;i<256;i++)s_aItemIDs[i]=i;
		UiDoListboxStart(&s_aItemIDs,&List,40.0f,"Workshop","",pPlatform->WorkshopItemCount(),1,s_Selected,s_Scroll);
		for(int i=0;i<pPlatform->WorkshopItemCount();i++){CPlatformWorkshopItem Info;pPlatform->WorkshopItem(i,&Info);CListboxItem Item=UiDoListboxNextItem(&s_aItemIDs[i],s_Selected==i);if(Item.m_Visible){char aLine[512];const int Percent=Info.m_Total?(int)(Info.m_Downloaded*100/Info.m_Total):0;str_format(aLine,sizeof(aLine),"%llu  %s  v%s  %s  %d%%",Info.m_PublishedFileID,Info.m_aName,Info.m_aVersion,Info.m_Valid?"VALID":Info.m_aError,Percent);Item.m_Rect.Margin(6.0f,&Item.m_Rect);UI()->DoLabelScaled(&Item.m_Rect,aLine,10.0f,-1);}}
		s_Selected=UiDoListboxEnd(&s_Scroll,0);s_Selected=clamp(s_Selected,-1,max(-1,pPlatform->WorkshopItemCount()-1));
		Detail.HSplitTop(34.0f,&Row,&Detail);Row.VSplitLeft(Row.w/2-2,&Button,&Row);if(DoButton_Menu(&s_Select,Localize("Use collection"),0,&Button,BUTTONSTYLE_ACCENT)&&s_Selected>=0){CPlatformWorkshopItem Info;if(pPlatform->WorkshopItem(s_Selected,&Info)){str_format(g_Config.m_ClModIds,sizeof(g_Config.m_ClModIds),"%llu",Info.m_PublishedFileID);pPlatform->RefreshWorkshopItems();}}Row.VSplitLeft(4.0f,0,&Row);if(DoButton_Menu(&s_Disable,Localize("Disable"),0,&Row)&&s_Selected>=0){CPlatformWorkshopItem Info;if(pPlatform->WorkshopItem(s_Selected,&Info))pPlatform->SetWorkshopItemDisabled(Info.m_PublishedFileID,true);}
		Detail.HSplitTop(4.0f,0,&Detail);Detail.HSplitTop(34.0f,&Row,&Detail);Row.VSplitLeft(Row.w/2-2,&Button,&Row);if(DoButton_Menu(&s_Enable,Localize("Enable"),0,&Button)&&s_Selected>=0){CPlatformWorkshopItem Info;if(pPlatform->WorkshopItem(s_Selected,&Info))pPlatform->SetWorkshopItemDisabled(Info.m_PublishedFileID,false);}Row.VSplitLeft(4.0f,0,&Row);if(DoButton_Menu(&s_Remove,Localize("Unsubscribe"),0,&Row)&&s_Selected>=0){CPlatformWorkshopItem Info;if(pPlatform->WorkshopItem(s_Selected,&Info))pPlatform->UnsubscribeWorkshopItem(Info.m_PublishedFileID);}
		Detail.HSplitTop(4.0f,0,&Detail);Detail.HSplitTop(34.0f,&Button,&Detail);if(DoButton_Menu(&s_Community,Localize("Community page / Report"),0,&Button)&&s_Selected>=0){CPlatformWorkshopItem Info;if(pPlatform->WorkshopItem(s_Selected,&Info))pPlatform->OpenWorkshopItemPage(Info.m_PublishedFileID);}
		Detail.HSplitTop(16.0f,0,&Detail);Detail.HSplitTop(24.0f,&Label,&Detail);UI()->DoLabelScaled(&Label,Localize("Content directory"),10.0f,-1);Detail.HSplitTop(28.0f,&Edit,&Detail);DoEditBox(s_aContent,&Edit,s_aContent,sizeof(s_aContent),10.0f,&s_ContentOffset);
		Detail.HSplitTop(8.0f,0,&Detail);Detail.HSplitTop(24.0f,&Label,&Detail);UI()->DoLabelScaled(&Label,Localize("Preview file"),10.0f,-1);Detail.HSplitTop(28.0f,&Edit,&Detail);DoEditBox(s_aPreview,&Edit,s_aPreview,sizeof(s_aPreview),10.0f,&s_PreviewOffset);
		Detail.HSplitTop(10.0f,0,&Detail);Detail.HSplitTop(34.0f,&Button,&Detail);if(DoButton_Menu(&s_Publish,Localize("Publish update"),0,&Button,BUTTONSTYLE_ACCENT)&&s_Selected>=0){CPlatformWorkshopItem Info;if(pPlatform->WorkshopItem(s_Selected,&Info))pPlatform->UpdateWorkshopItem(Info.m_PublishedFileID,s_aContent,s_aPreview);}
		CPlatformWorkshopPublishStatus Status;pPlatform->WorkshopPublishStatus(&Status);char aStatus[512];const int Percent=Status.m_Total?(int)(Status.m_Processed*100/Status.m_Total):0;if(Status.m_Active)str_format(aStatus,sizeof(aStatus),"%s  %d%%  ID %llu",Status.m_aStatus,Percent,Status.m_PublishedFileID);else str_format(aStatus,sizeof(aStatus),"%s  ID %llu",Status.m_aStatus,Status.m_PublishedFileID);Detail.HSplitTop(12.0f,0,&Detail);UI()->DoLabelScaled(&Detail,aStatus,10.0f,-1);
	}
	#endif
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
	const char *apTabs[] = {"Browse rooms", "Create room"};
	static int s_aTabs[2];
	for(int i = 0; i < 2; i++)
	{
		CUIRect Button;
		Tabs.VSplitLeft(min(150.0f, Tabs.w / (2 - i)), &Button, &Tabs);
		if(DoButton_MenuTab(&s_aTabs[i], Localize(apTabs[i]), m_PlayTab == i, &Button, i == 0 ? CUI::CORNER_TL : CUI::CORNER_TR))
		{
			m_PlayTab = i;
			if(i == 0) { ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET); if(pPlatform && pPlatform->Available()) pPlatform->RefreshLobbyList(); }
		}
	}
	Body.HSplitTop(8.0f, 0, &Body);

	if(m_PlayTab == 1)
	{
		RenderCreateRoom(Body);
		return;
	}

	UpdatePlaySnapshots();
	CUIRect StatusBar, Filters, List, Detail, Actions, Button;
	Body.HSplitTop(30.0f, &StatusBar, &Body);
	CClientAsyncStatus Connection; Client()->ConnectionStatus(&Connection);
	CPlatformOperationStatus LobbyStatus; mem_zero(&LobbyStatus, sizeof(LobbyStatus)); if(pPlatform) pPlatform->LobbyOperationStatus(&LobbyStatus);
	if(LobbyStatus.m_State == CLIENT_ASYNC_WORKING && LobbyStatus.m_Stage == CLIENT_STAGE_JOINING_ROOM) { CUIRect Cancel; StatusBar.VSplitRight(90.0f, &StatusBar, &Cancel); UI()->DoLabelScaled(&StatusBar, Localize("Joining room"), 10.0f, -1); static int s_CancelJoin; if(DoButton_Menu(&s_CancelJoin, Localize("Cancel"), 0, &Cancel, BUTTONSTYLE_DANGER) || m_EscapePressed) { pPlatform->LeaveLobby(); m_EscapePressed = false; } }
	else if(Connection.m_State == CLIENT_ASYNC_FAILED) { TextRender()->TextColor(ms_ColorDanger.r, ms_ColorDanger.g, ms_ColorDanger.b, 1.0f); UI()->DoLabelScaled(&StatusBar, Localize(Connection.m_aErrorKey), 10.0f, -1); TextRender()->TextColor(1, 1, 1, 1); }
	else if(ServerBrowser()->IsRefreshing()) { char aStatus[96]; str_format(aStatus, sizeof(aStatus), "%s  %d%%", Localize("Refreshing"), ServerBrowser()->LoadingProgression()); UI()->DoLabelScaled(&StatusBar, aStatus, 10.0f, -1); }
	else if(!pPlatform || !pPlatform->Available()) UI()->DoLabelScaled(&StatusBar, Localize("Steam unavailable — dedicated servers, Favorites and direct connection remain available."), 10.0f, -1);
	else UI()->DoLabelScaled(&StatusBar, Localize("Dedicated servers, LAN, Favorites and Steam rooms"), 10.0f, -1);

	Body.HSplitTop(28.0f, &Filters, &Body);
	const char *apFilters[] = {"All", "Official", "Community", "Friends", "Modded", "Favorites", "LAN"}; static int s_aFilters[7], s_FilterButton;
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
			m_PlayBrowserCollection = i == 6 ? PLAY_COLLECTION_LAN : i == 5 ? PLAY_COLLECTION_FAVORITES : PLAY_COLLECTION_INTERNET;
			ServerBrowser()->Refresh(m_PlayBrowserCollection == PLAY_COLLECTION_LAN ? IServerBrowser::TYPE_LAN : m_PlayBrowserCollection == PLAY_COLLECTION_FAVORITES ? IServerBrowser::TYPE_FAVORITES : IServerBrowser::TYPE_INTERNET);
			if(pPlatform && pPlatform->Available()) pPlatform->RefreshLobbyList();
		}
		FilterTabs.VSplitLeft(3.0f, 0, &FilterTabs);
	}
	const int ActiveFilterCount =
		(g_Config.m_BrFilterEmpty != 0) + (g_Config.m_BrFilterSpectators != 0) + (g_Config.m_BrFilterFull != 0) +
		(g_Config.m_BrFilterFriends != 0) + (g_Config.m_BrFilterPw != 0) + (g_Config.m_BrFilterCompatversion != 0) +
		(g_Config.m_BrFilterPure != 0) + (g_Config.m_BrFilterPureMap != 0) + (g_Config.m_BrFilterGametypeStrict != 0) +
		(g_Config.m_BrFilterPing < 999) + (g_Config.m_BrFilterGametype[0] != 0) + (g_Config.m_BrFilterServerAddress[0] != 0) +
		(g_Config.m_BrFilterCountry != 0);
	char aFilterLabel[64];
	if(ActiveFilterCount > 0)
		str_format(aFilterLabel, sizeof(aFilterLabel), "%s  %d", Localize("Filter"), ActiveFilterCount);
	else
		str_copy(aFilterLabel, Localize("Filter"), sizeof(aFilterLabel));
	if(DoButton_Menu(&s_FilterButton, aFilterLabel, m_PlayFiltersOpen || ActiveFilterCount > 0, &FilterAnchor, ActiveFilterCount > 0 ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
	{
		m_PlayFiltersOpen = !m_PlayFiltersOpen;
		if(!m_PlayFiltersOpen)
		{
			m_FilterPresetMenuOpen = false;
			m_FilterPresetRenameSlot = -1;
		}
	}
	Body.HSplitTop(30.0f, &Filters, &Body);
	CUIRect SearchLabel, Search; Filters.VSplitLeft(52.0f, &SearchLabel, &Search); UI()->DoLabelScaled(&SearchLabel, Localize("Search"), 10.0f, -1);
	static float s_SearchOffset; if(DoEditBox(&g_Config.m_BrFilterString, &Search, g_Config.m_BrFilterString, sizeof(g_Config.m_BrFilterString), 10.0f, &s_SearchOffset)) Client()->ServerBrowserUpdate();

	Body.HSplitTop(6.0f, 0, &Body); Body.HSplitBottom(72.0f, &Body, &Actions);
	CUIRect FilterPopup;
	if(m_PlayFiltersOpen)
	{
		int VisiblePresetCount = 0;
		for(int Slot = 0; Slot < NUM_UI_FILTER_PRESETS; Slot++)
			if(Slot < UI_FILTER_PRESET_CUSTOM_START || m_aFilterPresets[Slot].m_Used)
				VisiblePresetCount++;
		const float PopupWidth = min(PageBounds.w, 420.0f * UI()->Scale());
		const float PresetMenuHeight = min(350.0f, 130.0f + VisiblePresetCount * 22.0f);
		const float DesiredHeight = (m_FilterPresetMenuOpen ? PresetMenuHeight : (m_PlayFiltersAdvanced ? 376.0f : 224.0f)) * UI()->Scale();
		FilterPopup.w = PopupWidth;
		FilterPopup.x = clamp(FilterAnchor.x + FilterAnchor.w - PopupWidth, PageBounds.x, PageBounds.x + PageBounds.w - PopupWidth);
		FilterPopup.y = Filters.y + Filters.h + 4.0f * UI()->Scale();
		FilterPopup.h = min(DesiredHeight, max(120.0f * UI()->Scale(), PageBounds.y + PageBounds.h - FilterPopup.y - 6.0f * UI()->Scale()));
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
	if(!Compact) { Body.VSplitRight(max(220.0f, Body.w * 0.30f), &List, &Detail); List.VSplitRight(6.0f, &List, 0); }
	else { List = Body; Detail = CUIRect(); }

	CPlayRoomEntry aEntries[512]; int EntryCount = 0;
	auto AddServer = [&](const CPlayServerSnapshot *pServer) {
		if(!pServer || EntryCount >= 512) return;
		const bool Show = s_Filter == 0 || (s_Filter == 1 && pServer->m_Official) || (s_Filter == 2 && !pServer->m_Official) || (s_Filter == 4 && pServer->m_Modded) || (s_Filter == 5 && pServer->m_Favorite) || (s_Filter == 6 && pServer->m_Collection == PLAY_COLLECTION_LAN);
		if(!Show || (g_Config.m_BrFilterString[0] && !str_find_nocase(pServer->m_aName, g_Config.m_BrFilterString) && !str_find_nocase(pServer->m_aMap, g_Config.m_BrFilterString) && !str_find_nocase(pServer->m_aAddress, g_Config.m_BrFilterString))) return;
		for(int i = 0; i < EntryCount; i++)
			if(!str_comp(aEntries[i].m_aStableID, pServer->m_aAddress))
			{
				if(pServer->m_Favorite)
					aEntries[i].m_pServer = pServer;
				return;
			}
		CPlayRoomEntry &Entry = aEntries[EntryCount++]; Entry.m_Source = CPlayRoomEntry::SOURCE_DEDICATED; Entry.m_pServer = pServer; Entry.m_pLobby = 0; str_copy(Entry.m_aStableID, pServer->m_aAddress, sizeof(Entry.m_aStableID));
	};
	for(int Collection = 0; Collection < NUM_PLAY_COLLECTIONS; Collection++) for(int i = 0; i < m_aPlayServerSnapshotCount[Collection]; i++) AddServer(&m_aaPlayServerSnapshots[Collection][i]);
	for(int i = 0; i < m_PlayLobbySnapshotCount && EntryCount < 512; i++)
	{
		const CPlatformLobbyInfo &Info = m_aPlayLobbySnapshots[i].m_Info;
		const bool Show = s_Filter == 0 || s_Filter == 2 || (s_Filter == 3 && Info.m_FriendHosted) || (s_Filter == 4 && Info.m_Modded);
		if(Show && (!g_Config.m_BrFilterString[0] || str_find_nocase(Info.m_aHostName, g_Config.m_BrFilterString) || str_find_nocase(Info.m_aMap, g_Config.m_BrFilterString))) { CPlayRoomEntry &Entry = aEntries[EntryCount++]; Entry.m_Source = CPlayRoomEntry::SOURCE_STEAM_LOBBY; Entry.m_pServer = 0; Entry.m_pLobby = &m_aPlayLobbySnapshots[i]; str_format(Entry.m_aStableID, sizeof(Entry.m_aStableID), "lobby:%llu", Info.m_LobbyID); }
	}
	for(int i = 1; i < EntryCount; i++)
	{
		CPlayRoomEntry Key = aEntries[i]; int j = i - 1;
		auto Compare = [&](const CPlayRoomEntry &A, const CPlayRoomEntry &B) {
			const char *pA = A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_aName : A.m_pLobby->m_Info.m_aHostName;
			const char *pB = B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_aName : B.m_pLobby->m_Info.m_aHostName;
			int Result = str_comp_nocase(pA, pB);
			if(g_Config.m_BrSort == IServerBrowser::SORT_MAP) Result = str_comp_nocase(A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_aMap : A.m_pLobby->m_Info.m_aMap, B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_aMap : B.m_pLobby->m_Info.m_aMap);
			else if(g_Config.m_BrSort == IServerBrowser::SORT_GAMETYPE) Result = str_comp_nocase(A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_aGameType : A.m_pLobby->m_Info.m_aGameType, B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_aGameType : B.m_pLobby->m_Info.m_aGameType);
			else if(g_Config.m_BrSort == IServerBrowser::SORT_NUMPLAYERS) Result = (A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_NumClients : A.m_pLobby->m_Info.m_Members) - (B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_NumClients : B.m_pLobby->m_Info.m_Members);
			else if(g_Config.m_BrSort == IServerBrowser::SORT_PING) { const int PingA = A.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? A.m_pServer->m_Latency : 10000; const int PingB = B.m_Source == CPlayRoomEntry::SOURCE_DEDICATED ? B.m_pServer->m_Latency : 10000; Result = PingA - PingB; }
			return g_Config.m_BrSortOrder ? Result < 0 : Result > 0;
		};
		while(j >= 0 && Compare(aEntries[j], Key)) { aEntries[j + 1] = aEntries[j]; j--; } aEntries[j + 1] = Key;
	}
	auto SelectPlayEntry = [&](int Index) {
		if(Index < 0 || Index >= EntryCount)
			return;
		str_copy(m_aPlaySelectedID, aEntries[Index].m_aStableID, sizeof(m_aPlaySelectedID));
		if(aEntries[Index].m_pServer)
			str_copy(g_Config.m_UiServerAddress, aEntries[Index].m_pServer->m_aAddress, sizeof(g_Config.m_UiServerAddress));
		else
			g_Config.m_UiServerAddress[0] = 0;
	};
	if(!m_aPlaySelectedID[0] && EntryCount) SelectPlayEntry(0);
	s_Selected = -1; for(int i = 0; i < EntryCount; i++) if(!str_comp(m_aPlaySelectedID, aEntries[i].m_aStableID)) { s_Selected = i; break; }
	if(s_Selected < 0 && EntryCount) { s_Selected = 0; SelectPlayEntry(s_Selected); }
	const int SelectionBeforeKeyboard = s_Selected;
	if(m_PlayListHasFocus && !m_PlayFiltersOpen) for(int i = 0; i < m_NumInputEvents; i++) if(m_aInputEvents[i].m_Flags & IInput::FLAG_PRESS) { const int Key = m_aInputEvents[i].m_Key; if((Key == KEY_UP || Key == KEY_GAMEPAD_BUTTON_DPAD_UP) && s_Selected > 0) s_Selected--; if((Key == KEY_DOWN || Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN) && s_Selected + 1 < EntryCount) s_Selected++; }
	if(s_Selected >= 0 && s_Selected != SelectionBeforeKeyboard) SelectPlayEntry(s_Selected);

	CUIRect Headers; List.HSplitTop(20.0f, &Headers, &List); DrawSectionHeader(&Headers, CUI::CORNER_T);
	struct CColumn { const char *m_pName; int m_Sort; float m_Width; CUIRect m_Rect; }; CColumn aColumns[] = {{"Source", -1, Compact ? 66.0f : 76.0f, {}}, {"Name", IServerBrowser::SORT_NAME, Compact ? 0.0f : 0.0f, {}}, {"Type", IServerBrowser::SORT_GAMETYPE, Compact ? 0.0f : 70.0f, {}}, {"Map", IServerBrowser::SORT_MAP, Compact ? 0.0f : 100.0f, {}}, {"Players", IServerBrowser::SORT_NUMPLAYERS, 62.0f, {}}, {"Ping", IServerBrowser::SORT_PING, 56.0f, {}}};
	CUIRect Remaining = Headers;
	Remaining.VSplitLeft(aColumns[0].m_Width, &aColumns[0].m_Rect, &Remaining);
	Remaining.VSplitRight(aColumns[5].m_Width, &Remaining, &aColumns[5].m_Rect);
	Remaining.VSplitRight(aColumns[4].m_Width, &Remaining, &aColumns[4].m_Rect);
	if(!Compact)
	{
		Remaining.VSplitRight(aColumns[3].m_Width, &Remaining, &aColumns[3].m_Rect);
		Remaining.VSplitRight(aColumns[2].m_Width, &Remaining, &aColumns[2].m_Rect);
	}
	aColumns[1].m_Rect = Remaining;
	for(int i = 0; i < 6; i++) if(aColumns[i].m_Rect.w > 0.0f && DoButton_GridHeader(&aColumns[i], Localize(aColumns[i].m_pName), g_Config.m_BrSort == aColumns[i].m_Sort, &aColumns[i].m_Rect, !BlockPlayListInput) && aColumns[i].m_Sort >= 0) { if(g_Config.m_BrSort == aColumns[i].m_Sort) g_Config.m_BrSortOrder ^= 1; else { g_Config.m_BrSort = aColumns[i].m_Sort; g_Config.m_BrSortOrder = 0; } }
	static int s_EntryListID;
	static int s_aEntryIDs[512]; for(int i = 0; i < 512; i++) s_aEntryIDs[i] = i;
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
		CListboxItem Item = UiDoListboxNextItem(&s_aEntryIDs[i], s_Selected == i, !BlockPlayListInput); if(!Item.m_Visible) continue; CUIRect Row = Item.m_Rect; Row.Margin(4.0f, &Row);
		for(int Column = 0; Column < 6; Column++) { CUIRect Cell = aColumns[Column].m_Rect; Cell.x = Row.x + (Cell.x - Headers.x); Cell.y = Row.y; Cell.h = Row.h; const CPlayRoomEntry &Entry = aEntries[i]; const CPlayServerSnapshot *pServer = Entry.m_pServer; const CPlatformLobbyInfo *pLobby = Entry.m_pLobby ? &Entry.m_pLobby->m_Info : 0; char aValue[128]; if(Column == 0) str_copy(aValue, Entry.m_Source == CPlayRoomEntry::SOURCE_STEAM_LOBBY ? (pLobby->m_FriendHosted ? Localize("FRIEND") : "STEAM") : pServer->m_Collection == PLAY_COLLECTION_LAN ? "LAN" : pServer->m_Official ? Localize("OFFICIAL") : Localize("COMMUNITY"), sizeof(aValue)); else if(Column == 1) str_copy(aValue, pServer ? pServer->m_aName : pLobby->m_aHostName, sizeof(aValue)); else if(Column == 2) str_copy(aValue, pServer ? pServer->m_aGameType : pLobby->m_aGameType, sizeof(aValue)); else if(Column == 3) str_copy(aValue, pServer ? pServer->m_aMap : pLobby->m_aMap, sizeof(aValue)); else if(Column == 4) str_format(aValue, sizeof(aValue), "%d/%d", pServer ? pServer->m_NumClients : pLobby->m_Members, pServer ? pServer->m_MaxClients : pLobby->m_MaxMembers); else str_copy(aValue, pServer ? (pServer->m_Latency >= 0 ? "" : "-") : "RELAY", sizeof(aValue)); if(Column == 5 && pServer && pServer->m_Latency >= 0) str_format(aValue, sizeof(aValue), "%dms", pServer->m_Latency); UI()->DoLabelScaled(&Cell, aValue, FitLabelFontSize(TextRender(), aValue, 10.0f, Cell.w), -1); }
	}
	const int NewSelection = UiDoListboxEnd(&s_Scroll, 0); if(NewSelection >= 0 && NewSelection < EntryCount) { if(NewSelection != s_Selected) { s_Selected = NewSelection; SelectPlayEntry(s_Selected); } m_PlayListHasFocus = true; }

	auto RenderDetail = [&](CUIRect View) { DrawMenuInset(&View, CUI::CORNER_ALL); View.Margin(10.0f, &View); if(s_Selected < 0) UI()->DoLabelScaled(&View, Localize("No servers found"), 11.0f, -1); else { const CPlayRoomEntry &Entry = aEntries[s_Selected]; char aDetail[768]; if(Entry.m_pServer) { const CPlayServerSnapshot *pInfo = Entry.m_pServer; str_format(aDetail, sizeof(aDetail), "%s\n\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s", pInfo->m_aName, Localize("Address"), pInfo->m_aAddress, Localize("Version"), pInfo->m_aVersion, Localize("Source"), pInfo->m_Collection == PLAY_COLLECTION_LAN ? "LAN" : pInfo->m_DiscoverySources & IServerBrowser::DISCOVERY_STEAM ? "Steam GameServer + UDP" : "UDP", Localize("Mods"), pInfo->m_Modded ? Localize("Required") : Localize("None"), Localize("Authentication"), pInfo->m_AuthPolicy ? Localize("Required") : Localize("Open"), Localize("Region"), pInfo->m_aMap); } else { const CPlatformLobbyInfo &Info = Entry.m_pLobby->m_Info; str_format(aDetail, sizeof(aDetail), "%s\n\nLobbyID: %llu\n%s: %llu\n%s: %s\n%s: %s\nMod hash: %s\nRelay / Steam authentication", Info.m_aHostName, Info.m_LobbyID, Localize("Host"), Info.m_HostSteamID, Localize("Region"), Info.m_aRegion, Localize("Source"), Info.m_FriendHosted ? Localize("Friend room") : Localize("Steam local room"), Info.m_aModHash); } UI()->DoLabelScaled(&View, aDetail, 10.5f, -1); } };
	if(!Compact) RenderDetail(Detail);
	Actions.HSplitTop(6.0f, 0, &Actions); CUIRect Direct, ActionButtons; Actions.HSplitTop(30.0f, &Direct, &ActionButtons);
	CUIRect DirectLabel, DirectBox; Direct.VSplitLeft(76.0f, &DirectLabel, &DirectBox); UI()->DoLabelScaled(&DirectLabel, Localize("Host address"), 10.0f, -1); static float s_DirectOffset; if(!(BlockPlayListInput && UI()->MouseInside(&DirectBox))) DoEditBox(&g_Config.m_UiServerAddress, &DirectBox, g_Config.m_UiServerAddress, sizeof(g_Config.m_UiServerAddress), 10.0f, &s_DirectOffset);
	static int s_Join, s_Refresh, s_Copy, s_Favorite, s_Details; CUIRect JoinButton, RefreshButton, CopyButton, FavoriteButton, DetailButton;
	ActionButtons.VSplitRight(100.0f, &ActionButtons, &JoinButton); ActionButtons.VSplitRight(4.0f, &ActionButtons, 0); ActionButtons.VSplitRight(80.0f, &ActionButtons, &RefreshButton); ActionButtons.VSplitRight(4.0f, &ActionButtons, 0); ActionButtons.VSplitRight(70.0f, &ActionButtons, &CopyButton); ActionButtons.VSplitRight(4.0f, &ActionButtons, 0); ActionButtons.VSplitRight(92.0f, &ActionButtons, &FavoriteButton); if(Compact) { ActionButtons.VSplitRight(4.0f, &ActionButtons, 0); ActionButtons.VSplitRight(74.0f, &ActionButtons, &DetailButton); }
	const bool HasDedicated = s_Selected >= 0 && aEntries[s_Selected].m_pServer;
	const bool JoinBlocked = BlockPlayListInput && UI()->MouseInside(&JoinButton);
	const bool RefreshBlocked = BlockPlayListInput && UI()->MouseInside(&RefreshButton);
	const bool CopyBlocked = BlockPlayListInput && UI()->MouseInside(&CopyButton);
	const bool FavoriteBlocked = BlockPlayListInput && UI()->MouseInside(&FavoriteButton);
	const bool DetailBlocked = BlockPlayListInput && UI()->MouseInside(&DetailButton);
	if((!JoinBlocked && DoButton_Menu(&s_Join, Localize("Join"), 0, &JoinButton, BUTTONSTYLE_ACCENT)) || (!m_PlayFiltersOpen && m_PlayListHasFocus && m_EnterPressed && s_Selected >= 0)) { if(s_Selected >= 0) { const CPlayRoomEntry &Entry = aEntries[s_Selected]; if(Entry.m_pServer) Client()->Connect(Entry.m_pServer->m_aAddress); else if(pPlatform) pPlatform->JoinLobby(Entry.m_pLobby->m_Info.m_LobbyID); } else if(g_Config.m_UiServerAddress[0]) Client()->Connect(g_Config.m_UiServerAddress); m_EnterPressed = false; }
	if(!RefreshBlocked && DoButton_Menu(&s_Refresh, Localize("Refresh"), 0, &RefreshButton)) { ServerBrowser()->Refresh(m_PlayBrowserCollection == PLAY_COLLECTION_LAN ? IServerBrowser::TYPE_LAN : m_PlayBrowserCollection == PLAY_COLLECTION_FAVORITES ? IServerBrowser::TYPE_FAVORITES : IServerBrowser::TYPE_INTERNET); if(pPlatform && pPlatform->Available()) pPlatform->RefreshLobbyList(); }
	if(!CopyBlocked && DoButton_Menu(&s_Copy, Localize("Copy"), 0, &CopyButton) && s_Selected >= 0) Input()->SetClipboardText(HasDedicated ? aEntries[s_Selected].m_pServer->m_aAddress : aEntries[s_Selected].m_aStableID);
	if(HasDedicated && !FavoriteBlocked && DoButton_Menu(&s_Favorite, Localize(aEntries[s_Selected].m_pServer->m_Favorite ? "Unfavorite" : "Favorite"), 0, &FavoriteButton)) { const CPlayServerSnapshot *pInfo = aEntries[s_Selected].m_pServer; if(pInfo->m_Favorite) ServerBrowser()->RemoveFavorite(pInfo->m_NetAddr); else ServerBrowser()->AddFavorite(pInfo->m_NetAddr); }
	if(Compact && !DetailBlocked && DoButton_Menu(&s_Details, Localize("Details"), m_PlayDetailOpen, &DetailButton)) m_PlayDetailOpen = !m_PlayDetailOpen;
	if(Compact && m_PlayDetailOpen) { CUIRect Overlay = Body; Overlay.Margin(8.0f, &Overlay); RenderDetail(Overlay); if(m_EscapePressed) { m_PlayDetailOpen = false; m_EscapePressed = false; } }

	if(m_PlayFiltersOpen)
	{
		DrawMenuBorder(&FilterPopup, vec4(0.035f, 0.043f, 0.052f, 0.99f), vec4(ms_ColorAccent.r, ms_ColorAccent.g, ms_ColorAccent.b, 0.72f), CUI::CORNER_ALL, ms_PanelRounding);
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
		const bool CustomPreset = m_ActiveFilterPreset >= UI_FILTER_PRESET_CUSTOM_START && m_aFilterPresets[m_ActiveFilterPreset].m_Used;
		if(m_FilterPresetRenameSlot >= UI_FILTER_PRESET_CUSTOM_START)
		{
			CUIRect RenameBox, SaveButton, CancelButton;
			PresetRow.VSplitRight(108.0f, &RenameBox, &SaveButton);
			SaveButton.VSplitLeft(52.0f, &SaveButton, &CancelButton);
			CancelButton.VSplitLeft(4.0f, 0, &CancelButton);
			static float s_RenameOffset;
			DoEditBox(&m_aFilterPresetRenameBuf, &RenameBox, m_aFilterPresetRenameBuf, sizeof(m_aFilterPresetRenameBuf), 10.0f, &s_RenameOffset);
			static int s_SavePresetName, s_CancelPresetName;
			if((DoButton_Menu(&s_SavePresetName, Localize("Save"), 0, &SaveButton, BUTTONSTYLE_ACCENT) || m_EnterPressed) && m_aFilterPresetRenameBuf[0])
			{
				str_copy(m_aFilterPresets[m_FilterPresetRenameSlot].m_aName, m_aFilterPresetRenameBuf, sizeof(m_aFilterPresets[m_FilterPresetRenameSlot].m_aName));
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
			if(DoButton_Menu(&s_PresetSelector, aPresetLabel, m_FilterPresetMenuOpen, &PresetRow, m_FilterPresetMenuOpen ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
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
				if(!s_PresetScrollRegion.IsRectClipped(Row) && DoButton_Menu(&s_aPresetIds[Slot], pName, Slot == m_ActiveFilterPreset, &Row, Slot == m_ActiveFilterPreset ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
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
					if(!m_aFilterPresets[i].m_Used) { Slot = i; break; }
				if(Slot >= 0)
				{
					m_aFilterPresets[Slot].m_Used = true;
					str_format(m_aFilterPresets[Slot].m_aName, sizeof(m_aFilterPresets[Slot].m_aName), "%s %d", Localize("Preset"), Slot - UI_FILTER_PRESET_CUSTOM_START + 1);
					SnapshotConfigToFilterPreset(Slot);
					SwitchFilterPreset(Slot);
					m_FilterPresetRenameSlot = Slot;
					str_copy(m_aFilterPresetRenameBuf, m_aFilterPresets[Slot].m_aName, sizeof(m_aFilterPresetRenameBuf));
					m_FilterPresetMenuOpen = false;
				}
			}
			if(DoButton_Menu(&s_RenamePreset, Localize("Rename"), 0, &RenameButton) && CustomPreset)
			{
				m_FilterPresetRenameSlot = m_ActiveFilterPreset;
				str_copy(m_aFilterPresetRenameBuf, m_aFilterPresets[m_ActiveFilterPreset].m_aName, sizeof(m_aFilterPresetRenameBuf));
				m_FilterPresetMenuOpen = false;
			}
			if(DoButton_Menu(&s_DeletePreset, Localize("Delete"), 0, &DeleteButton, CustomPreset ? BUTTONSTYLE_DANGER : BUTTONSTYLE_NORMAL) && CustomPreset)
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

			auto RenderToggleRow = [&](int &LeftValue, const char *pLeftText, int &RightValue, const char *pRightText) {
				CUIRect Row, Left, Right;
				Content.HSplitTop(22.0f, &Row, &Content);
				Row.VSplitMid(&Left, &Right);
				Right.VSplitLeft(4.0f, 0, &Right);
				if(!s_PlayFilterScrollRegion.IsRectClipped(Left) && DoButton_CheckBox(&LeftValue, Localize(pLeftText), LeftValue, &Left)) LeftValue ^= 1;
				if(!s_PlayFilterScrollRegion.IsRectClipped(Right) && DoButton_CheckBox(&RightValue, Localize(pRightText), RightValue, &Right)) RightValue ^= 1;
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
			if(!s_PlayFilterScrollRegion.IsRectClipped(AdvancedButton) && DoButton_Menu(&s_AdvancedFilters, Localize("Advanced filters"), m_PlayFiltersAdvanced, &AdvancedButton, m_PlayFiltersAdvanced ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
				m_PlayFiltersAdvanced = !m_PlayFiltersAdvanced;
			s_PlayFilterScrollRegion.AddRect(AdvancedButton);

			if(m_PlayFiltersAdvanced)
			{
				Content.HSplitTop(4.0f, 0, &Content);
				RenderToggleRow(g_Config.m_BrFilterSpectators, "Count players only", g_Config.m_BrFilterCompatversion, "Compatible version");
				RenderToggleRow(g_Config.m_BrFilterPure, "Standard gametype", g_Config.m_BrFilterPureMap, "Standard map");
				CUIRect StrictRow;
				Content.HSplitTop(22.0f, &StrictRow, &Content);
				if(!s_PlayFilterScrollRegion.IsRectClipped(StrictRow) && DoButton_CheckBox(&g_Config.m_BrFilterGametypeStrict, Localize("Strict gametype filter"), g_Config.m_BrFilterGametypeStrict, &StrictRow)) g_Config.m_BrFilterGametypeStrict ^= 1;
				s_PlayFilterScrollRegion.AddRect(StrictRow);

				auto RenderTextFilter = [&](void *pID, const char *pLabel, char *pBuffer, unsigned BufferSize, float *pOffset) {
					CUIRect Row, Label, Edit;
					Content.HSplitTop(3.0f, 0, &Content);
					Content.HSplitTop(24.0f, &Row, &Content);
					Row.VSplitLeft(112.0f, &Label, &Edit);
					if(!s_PlayFilterScrollRegion.IsRectClipped(Row))
					{
						UI()->DoLabelScaled(&Label, Localize(pLabel), 10.0f, -1);
						if(DoEditBox(pID, &Edit, pBuffer, BufferSize, 10.0f, pOffset)) Client()->ServerBrowserUpdate();
						UI()->ClipEnable(&FilterContent);
					}
					s_PlayFilterScrollRegion.AddRect(Row);
				};
				static float s_GameTypeOffset, s_AddressOffset;
				RenderTextFilter(&g_Config.m_BrFilterGametype, "Game types:", g_Config.m_BrFilterGametype, sizeof(g_Config.m_BrFilterGametype), &s_GameTypeOffset);
				RenderTextFilter(&g_Config.m_BrFilterServerAddress, "Server address:", g_Config.m_BrFilterServerAddress, sizeof(g_Config.m_BrFilterServerAddress), &s_AddressOffset);

				CUIRect CountryRow, CountryLabel, Flag;
				Content.HSplitTop(3.0f, 0, &Content);
				Content.HSplitTop(24.0f, &CountryRow, &Content);
				CountryRow.VSplitRight(54.0f, &CountryLabel, &Flag);
				const bool CountryVisible = !s_PlayFilterScrollRegion.IsRectClipped(CountryRow);
				if(CountryVisible && DoButton_CheckBox(&g_Config.m_BrFilterCountry, Localize("Player country:"), g_Config.m_BrFilterCountry, &CountryLabel)) g_Config.m_BrFilterCountry ^= 1;
				CUIRect FlagImage = Flag;
				FlagImage.Margin(3.0f, &FlagImage);
				const float OldWidth = FlagImage.w;
				FlagImage.w = FlagImage.h * 2.0f;
				FlagImage.x += (OldWidth - FlagImage.w) * 0.5f;
				vec4 FlagColor(1, 1, 1, g_Config.m_BrFilterCountry ? 1.0f : 0.45f);
				if(CountryVisible)
					m_pClient->m_pCountryFlags->Render(g_Config.m_BrFilterCountryIndex, &FlagColor, FlagImage.x, FlagImage.y, FlagImage.w, FlagImage.h);
				if(CountryVisible && g_Config.m_BrFilterCountry && UI()->DoButtonLogic(&g_Config.m_BrFilterCountryIndex, "", 0, &Flag)) m_Popup = POPUP_COUNTRY;
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
	DrawMenuPanel(&MainView, CUI::CORNER_ALL); MainView.Margin(10.0f, &MainView);
	CUIRect Title, Toolbar, Body, List, Detail, Button, Row; MainView.HSplitTop(36.0f, &Title, &MainView); UI()->DoLabelScaled(&Title, Localize("Mods"), 22.0f, -1);
	if(!pPlatform || !pPlatform->Available()) { UI()->DoLabelScaled(&MainView, Localize("Steam Workshop is unavailable. Installed game data remains unchanged."), 12.0f, -1); return; }
	CPlatformOperationStatus ModStatus; pPlatform->WorkshopOperationStatus(&ModStatus);
	if(ModStatus.m_State == CLIENT_ASYNC_WORKING) { CUIRect Progress; MainView.HSplitTop(18.0f, &Progress, &MainView); char aProgress[96]; str_format(aProgress, sizeof(aProgress), "%s  %d%%", Localize("Synchronizing Mods"), (int)(ModStatus.m_Progress * 100)); UI()->DoLabelScaled(&Progress, aProgress, 10.0f, -1); MainView.HSplitTop(4.0f, 0, &MainView); }
	static int s_Tab = 0, s_Selected = -1; static float s_Scroll = 0.0f; static int s_aTabs[3]; const char *apTabs[] = {"Installed", "Needs update", "Disabled"};
	MainView.HSplitTop(30.0f, &Toolbar, &Body); for(int i = 0; i < 3; i++) { Toolbar.VSplitLeft(130.0f, &Button, &Toolbar); if(DoButton_MenuTab(&s_aTabs[i], Localize(apTabs[i]), s_Tab == i, &Button, 0)) s_Tab = i; Toolbar.VSplitLeft(4.0f, 0, &Toolbar); }
	Body.HSplitTop(8.0f, 0, &Body); Body.VSplitLeft(Body.w * 0.60f, &List, &Detail); List.VSplitRight(8.0f, &List, 0);
	int aItems[256], Count = 0; for(int i = 0; i < pPlatform->WorkshopItemCount(); i++) { CPlatformWorkshopItem Info; pPlatform->WorkshopItem(i, &Info); const bool Disabled = (Info.m_State & 128) != 0; const bool NeedsUpdate = (Info.m_State & 8) != 0 || (Info.m_Total > 0 && Info.m_Downloaded < Info.m_Total); if((s_Tab == 0 && !Disabled && !NeedsUpdate) || (s_Tab == 1 && NeedsUpdate) || (s_Tab == 2 && Disabled)) aItems[Count++] = i; }
	static int s_aIDs[256]; for(int i = 0; i < 256; i++) s_aIDs[i] = i; UiDoListboxStart(&s_aIDs, &List, 42.0f, Localize("Player Mods"), "", Count, 1, s_Selected, s_Scroll);
	for(int i = 0; i < Count; i++) { CPlatformWorkshopItem Info; pPlatform->WorkshopItem(aItems[i], &Info); CListboxItem Item = UiDoListboxNextItem(&s_aIDs[i], s_Selected == i); if(Item.m_Visible) { char aLine[512]; const int Percent = Info.m_Total ? (int)(Info.m_Downloaded * 100 / Info.m_Total) : (Info.m_Valid ? 100 : 0); str_format(aLine, sizeof(aLine), "%s  v%s  |  %s  |  %d%%", Info.m_aName[0] ? Info.m_aName : Localize("Downloading Mod"), Info.m_aVersion[0] ? Info.m_aVersion : "-", Info.m_Valid ? Localize("VALID") : Localize("CHECK REQUIRED"), Percent); Item.m_Rect.Margin(6.0f, &Item.m_Rect); UI()->DoLabelScaled(&Item.m_Rect, aLine, FitLabelFontSize(TextRender(), aLine, 10.5f, Item.m_Rect.w), -1); } }
	s_Selected = UiDoListboxEnd(&s_Scroll, 0); s_Selected = clamp(s_Selected, -1, max(-1, Count - 1)); DrawMenuInset(&Detail, CUI::CORNER_ALL); Detail.Margin(10.0f, &Detail);
	if(s_Selected < 0) UI()->DoLabelScaled(&Detail, Localize("No Mods in this category."), 11.0f, -1);
	else { CPlatformWorkshopItem Info; pPlatform->WorkshopItem(aItems[s_Selected], &Info); char aDetail[768], aID[32]; str_format(aID, sizeof(aID), "%llu", Info.m_PublishedFileID); str_format(aDetail, sizeof(aDetail), "%s\nWorkshop ID: %llu\n%s: %s\n%s: %s\n%s: %s\n%s", Info.m_aName, Info.m_PublishedFileID, Localize("Version"), Info.m_aVersion, Localize("Protocol / content hash"), Info.m_Valid ? Localize("Verified for this build") : Localize("Not verified"), Localize("Current collection"), str_find(g_Config.m_ClModIds, aID) ? Localize("Active") : Localize("Inactive"), Info.m_aError); UI()->DoLabelScaled(&Detail, aDetail, 10.5f, -1); Detail.HSplitBottom(126.0f, &Detail, &Row); static int s_Use, s_Toggle, s_Remove, s_Community; Row.HSplitTop(28.0f, &Button, &Row); if(DoButton_Menu(&s_Use, Localize("Use collection"), 0, &Button, BUTTONSTYLE_ACCENT)) { str_format(g_Config.m_ClModIds, sizeof(g_Config.m_ClModIds), "%llu", Info.m_PublishedFileID); pPlatform->RefreshWorkshopItems(); } Row.HSplitTop(4.0f, 0, &Row); Row.HSplitTop(28.0f, &Button, &Row); const bool Disabled = (Info.m_State & 128) != 0; if(DoButton_Menu(&s_Toggle, Localize(Disabled ? "Enable" : "Disable"), 0, &Button)) pPlatform->SetWorkshopItemDisabled(Info.m_PublishedFileID, !Disabled); Row.HSplitTop(4.0f, 0, &Row); Row.HSplitTop(28.0f, &Button, &Row); Button.VSplitLeft(Button.w / 2 - 2.0f, &Button, &Row); if(DoButton_Menu(&s_Remove, Localize("Unsubscribe"), 0, &Button, BUTTONSTYLE_DANGER)) pPlatform->UnsubscribeWorkshopItem(Info.m_PublishedFileID); Row.VSplitLeft(4.0f, 0, &Row); if(DoButton_Menu(&s_Community, Localize("Community / Report"), 0, &Row)) pPlatform->OpenWorkshopItemPage(Info.m_PublishedFileID); }
	CUIRect Browse; MainView.HSplitBottom(34.0f, &MainView, &Browse); static int s_Refresh, s_Browse; Browse.VSplitLeft(110.0f, &Button, &Browse); if(DoButton_Menu(&s_Refresh, Localize("Refresh"), 0, &Button)) pPlatform->RefreshWorkshopItems(); Browse.VSplitLeft(6.0f, 0, &Browse); Browse.VSplitLeft(170.0f, &Button, &Browse); if(DoButton_Menu(&s_Browse, Localize("Browse Workshop"), 0, &Button, BUTTONSTYLE_ACCENT)) pPlatform->OpenWorkshopBrowsePage();
}

int CMenus::Render()
{
	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
	
	static bool s_First = true;
	if(s_First)
	{
		if(g_Config.m_UiPage == PAGE_STEAM || g_Config.m_UiPage == PAGE_INTERNET)
		{
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
			IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
			if(pPlatform && pPlatform->Available()) pPlatform->RefreshLobbyList();
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

	Screen.Margin(g_Config.m_UiWideview ? 6.0f : 10.0f, &Screen);
	LayoutCenterPanel(&Screen, &Screen);

	static bool s_SoundCheck = false;
	if(!s_SoundCheck && m_Popup == POPUP_NONE)
	{
		if(Client()->SoundInitFailed())
			m_Popup = POPUP_SOUNDERROR;
		s_SoundCheck = true;
	}

	if(m_Popup == POPUP_NONE)
	{
		const float NavigationWidth = Screen.w < 900.0f ? 62.0f : 172.0f;
		Screen.VSplitLeft(NavigationWidth, &Navigation, &MainView);
		MainView.VSplitLeft(8.0f, 0, &MainView);

		// render current page
		if(Client()->State() != IClient::STATE_OFFLINE)
		{
			if(m_GamePage == PAGE_LOCAL_SERVER && g_Config.m_ClTutorialActive && g_Config.m_ClTutorialChapter == TUTORIAL_CHAPTER_MULTIPLAYER && g_Config.m_ClTutorialStep >= 1)
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
		else if(g_Config.m_UiPage == PAGE_INTERNET || g_Config.m_UiPage == PAGE_LAN || g_Config.m_UiPage == PAGE_FAVORITES || g_Config.m_UiPage == PAGE_LOCAL_SERVER)
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

		// Draw navigation last so compact hover labels can float over the page.
		RenderMenubar(Navigation);
	}
	else
	{
		// make sure that other windows doesn't do anything funnay!
		//UI()->SetHotItem(0);
		//UI()->SetActiveItem(0);
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
		else if(m_Popup == POPUP_CONNECTING)
		{
			CClientAsyncStatus Status;
			Client()->ConnectionStatus(&Status);
			if(Status.m_Stage == CLIENT_STAGE_LOADING_MAP) pTitle = Localize("Loading map");
			else if(Status.m_Stage == CLIENT_STAGE_AUTHENTICATING) pTitle = Localize("Authenticating with Steam");
			else if(Status.m_Stage == CLIENT_STAGE_SYNCING_MODS) pTitle = Localize("Synchronizing Mods");
			else pTitle = Localize("Connecting to server");
			pExtraText = g_Config.m_UiServerAddress; // TODO: query the client about the address
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
			pExtraText = Localize("Your last completed checkpoint is saved locally. You can continue now, return to the hub, or skip training.");
			ExtraAlign = -1;
		}
		else if(m_Popup == POPUP_FIRST_LAUNCH)
		{
			pTitle = Localize("Welcome to Ninslash");
			pExtraText = Localize("As this is the first time you launch the game, please enter your nick name below. It's recommended that you check the settings to adjust them to your liking before joining a server.");
			pButtonText = Localize("Ok");
			ExtraAlign = -1;
		}

		CUIRect Box, Part;
		Box = Screen;
		Box.VMargin(150.0f/UI()->Scale(), &Box);
		Box.HMargin(150.0f/UI()->Scale(), &Box);

		// render the box
		DrawMenuBorder(&Box, ms_ColorBgPanel, ms_ColorAccentDim, CUI::CORNER_ALL, 15.0f);

		Box.HSplitTop(20.f/UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f/UI()->Scale(), &Part, &Box);
		{
			vec4 Accent = ms_ColorAccent;
			TextRender()->TextColor(Accent.r, Accent.g, Accent.b, 1.0f);
		}
		UI()->DoLabelScaled(&Part, pTitle, 18.f, 0);
		{
			vec4 TextCol = ms_ColorText;
			TextRender()->TextColor(TextCol.r, TextCol.g, TextCol.b, 1.0f);
		}
		Box.HSplitTop(20.f/UI()->Scale(), &Part, &Box);
		Box.HSplitTop(24.f/UI()->Scale(), &Part, &Box);
		Part.VMargin(20.f/UI()->Scale(), &Part);

		if(ExtraAlign == -1)
			UI()->DoLabelScaled(&Part, pExtraText, 14.f, -1, (int)Part.w);
		else
			UI()->DoLabelScaled(&Part, pExtraText, 14.f, 0, -1);

		if(m_Popup == POPUP_TUTORIAL_EXIT)
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
			if(DoButton_Menu(&s_ContinueTraining, Localize("Continue"), 0, &Continue, BUTTONSTYLE_ACCENT) || m_EscapePressed)
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
			if(DoButton_Menu(&s_SkipTraining, Localize(g_Config.m_ClTutorialState == 2 ? "Return to hub" : "Skip training"), 0, &Skip, g_Config.m_ClTutorialState == 2 ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_DANGER))
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
			Box.VMargin(20.f/UI()->Scale(), &Box);
			if(m_pClient->Editor()->HasUnsavedData())
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "%s\n%s", Localize("There's an unsaved map in the editor, you might want to save it before you quit the game."), Localize("Quit anyway?"));
				UI()->DoLabelScaled(&Box, aBuf, 20.f, -1, Part.w-20.0f);
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
				Client()->Connect(g_Config.m_UiServerAddress);
			}

			Box.HSplitBottom(60.f, &Box, &Part);
			Box.HSplitBottom(24.f, &Box, &Part);

			Part.VSplitLeft(60.0f, 0, &Label);
			Label.VSplitLeft(100.0f, 0, &TextBox);
			TextBox.VSplitLeft(20.0f, 0, &TextBox);
			TextBox.VSplitRight(60.0f, &TextBox, 0);
			UI()->DoLabel(&Label, Localize("Password"), 18.0f, -1);
			static float Offset = 0.0f;
			DoEditBox(&g_Config.m_Password, &TextBox, g_Config.m_Password, sizeof(g_Config.m_Password), 12.0f, &Offset, true);
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
				if(Now-m_DownloadLastCheckTime >= time_freq())
				{
					if(m_DownloadLastCheckSize > Client()->MapDownloadAmount())
					{
						// map downloaded restarted
						m_DownloadLastCheckSize = 0;
					}

					// update download speed
					float Diff = (Client()->MapDownloadAmount()-m_DownloadLastCheckSize)/((int)((Now-m_DownloadLastCheckTime)/time_freq()));
					float StartDiff = m_DownloadLastCheckSize-0.0f;
					if(StartDiff+Diff > 0.0f)
						m_DownloadSpeed = (Diff/(StartDiff+Diff))*(Diff/1.0f) + (StartDiff/(Diff+StartDiff))*m_DownloadSpeed;
					else
						m_DownloadSpeed = 0.0f;
					m_DownloadLastCheckTime = Now;
					m_DownloadLastCheckSize = Client()->MapDownloadAmount();
				}

				Box.HSplitTop(64.f, 0, &Box);
				Box.HSplitTop(24.f, &Part, &Box);
				str_format(aBuf, sizeof(aBuf), "%d/%d KiB (%.1f KiB/s)", Client()->MapDownloadAmount()/1024, Client()->MapDownloadTotalsize()/1024,	m_DownloadSpeed/1024.0f);
				UI()->DoLabel(&Part, aBuf, 20.f, 0, -1);

				// time left
				const char *pTimeLeftString;
				int TimeLeft = max(1, m_DownloadSpeed > 0.0f ? static_cast<int>((Client()->MapDownloadTotalsize()-Client()->MapDownloadAmount())/m_DownloadSpeed) : 1);
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
				Part.w = max(10.0f, (Part.w*Client()->MapDownloadAmount())/Client()->MapDownloadTotalsize());
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
				m_Popup = POPUP_FIRST_LAUNCH;
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
			UiDoListboxStart(&s_ScrollValue, &Box, 50.0f, Localize("Country"), "", m_pClient->m_pCountryFlags->Num(), 6, OldSelected, s_ScrollValue);

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
					Item.m_Rect.w = Item.m_Rect.h*2;
					Item.m_Rect.x += (OldWidth-Item.m_Rect.w)/ 2.0f;
					vec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
					m_pClient->m_pCountryFlags->Render(pEntry->m_CountryCode, &Color, Item.m_Rect.x, Item.m_Rect.y, Item.m_Rect.w, Item.m_Rect.h);
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
					str_format(aBuf, sizeof(aBuf), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
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
					str_format(aBufOld, sizeof(aBufOld), "%s/%s", m_aCurrentDemoFolder, m_lDemos[m_DemolistSelectedIndex].m_aFilename);
					int Length = str_length(m_aCurrentDemoFile);
					char aBufNew[512];
					if(Length <= 4 || m_aCurrentDemoFile[Length-5] != '.' || str_comp_nocase(m_aCurrentDemoFile+Length-4, "demo"))
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
					PopupMessage(Localize("Error"), str_comp(pError, "error loading demo") ? pError : Localize("Error loading demo"), Localize("Ok"));
				}
				else if(!Client()->VideoStart(m_aVideoOutputName, g_Config.m_ClVideoFps))
				{
					Client()->Disconnect();
					m_Popup = POPUP_NONE;
					PopupMessage(Localize("Error"), Localize("Failed to start video render. Is FFmpeg installed?"), Localize("Ok"));
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
			DoEditBox(&g_Config.m_PlayerName, &TextBox, g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), 12.0f, &Offset);
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
	g_Config.m_PlayerColorBody = rand()%(0xFFFFFF/10)*1000;;
	g_Config.m_PlayerColorFeet = rand()%(0xFFFFFF/10)*1000;;
	g_Config.m_PlayerColorTopper = rand()%(0xFFFFFF/10)*1000;
	
	str_copy(g_Config.m_PlayerBody, "default", 24);
	str_copy(g_Config.m_PlayerHead, "default", 24);
	str_copy(g_Config.m_PlayerHand, "default", 24);
	str_copy(g_Config.m_PlayerFoot, "default", 24);
	
	
	switch (rand()%7)
	{
		case 0: str_copy(g_Config.m_PlayerTopper, "basic", 24); break;	
		case 1: str_copy(g_Config.m_PlayerTopper, "casual", 24); break;
		case 2: str_copy(g_Config.m_PlayerTopper, "dr", 24); break;
		case 3: str_copy(g_Config.m_PlayerTopper, "emo", 24); break;
		case 4: str_copy(g_Config.m_PlayerTopper, "nerd2", 24); break;
		case 5: str_copy(g_Config.m_PlayerTopper, "nerd", 24); break;
		default: str_copy(g_Config.m_PlayerTopper, "default", 24);
	};
		
	switch (rand()%5)
		{
		case 0: str_copy(g_Config.m_PlayerEye, "cyan", 24); break;	
		case 1: str_copy(g_Config.m_PlayerEye, "lsd", 24); break;
		case 2: str_copy(g_Config.m_PlayerEye, "sleepy", 24); break;
		case 3: str_copy(g_Config.m_PlayerEye, "diag", 24); break;
		default: str_copy(g_Config.m_PlayerEye, "default", 24);
	};
}

void CMenus::SetActive(bool Active)
{
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
	if(e.m_Flags&IInput::FLAG_PRESS)
	{
		if(e.m_Key == KEY_GAMEPAD_BUTTON_B && IsActive())
		{
			m_EscapePressed = true;
			const bool LocalGameSubpage = m_GamePage == PAGE_LOCAL_SERVER && m_CreateRoomStep == CREATE_ROOM_CONFIGURE &&
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
				const bool LocalGameSubpage = m_GamePage == PAGE_LOCAL_SERVER && m_CreateRoomStep == CREATE_ROOM_CONFIGURE &&
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

		if(e.m_Flags&IInput::FLAG_PRESS)
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
		//client_serverinfo_request();
	}
	else if(NewState == IClient::STATE_CONNECTING)
		m_Popup = POPUP_CONNECTING;
	else if (NewState == IClient::STATE_ONLINE || NewState == IClient::STATE_DEMOPLAYBACK)
	{
		m_Popup = POPUP_NONE;
		SetActive(false);
	}
}

extern "C" void font_debug_render();

void CMenus::OnRender()
{
	UpdateLocalServer();

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

	// update colors — higher-contrast dark punk
	ms_GuiColor = vec4(0.04f, 0.05f, 0.06f, 0.95f);
	const float A = MenuAlpha();
	ms_ColorBgDeep = vec4(0.012f, 0.014f, 0.017f, 0.96f * A);
	ms_ColorBgPanel = vec4(0.044f, 0.048f, 0.058f, 0.96f * A);
	ms_ColorBgInset = vec4(0.026f, 0.028f, 0.034f, 0.92f * A);
	ms_ColorAccent = vec4(0.96f, 0.67f, 0.20f, 1.0f); // warm industrial yellow
	ms_ColorAccentDim = vec4(0.18f, 0.72f, 0.78f, 1.0f); // trusted/online cyan
	ms_ColorDanger = vec4(0.92f, 0.24f, 0.30f, 1.0f);
	ms_ColorText = vec4(0.97f, 0.97f, 0.95f, 1.0f);

	ms_ColorTabbarInactiveOutgame = vec4(0.08f, 0.09f, 0.11f, 0.92f * A);
	ms_ColorTabbarActiveOutgame = vec4(0.12f, 0.13f, 0.16f, 0.98f * A);
	ms_ColorTabbarInactiveIngame = vec4(0.08f, 0.09f, 0.11f, 0.94f * A);
	ms_ColorTabbarActiveIngame = vec4(0.12f, 0.13f, 0.16f, 0.98f * A);

	// update the ui
	CUIRect *pScreen = UI()->Screen();
	float mx = (m_MousePos.x/(float)Graphics()->ScreenWidth())*pScreen->w;
	float my = (m_MousePos.y/(float)Graphics()->ScreenHeight())*pScreen->h;

	int Buttons = 0;
	if(m_UseMouseButtons)
	{
		if(Input()->KeyPressed(KEY_MOUSE_1)) Buttons |= 1;
		if(Input()->KeyPressed(KEY_MOUSE_2)) Buttons |= 2;
		if(Input()->KeyPressed(KEY_MOUSE_3)) Buttons |= 4;
	}

	UI()->Update(mx,my,mx*3.0f,my*3.0f,Buttons);

	// render
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
		Render();
	m_pClient->m_pPveRoguelite->RenderMenuDebugOverlay();
	m_pClient->m_pPveRoguelite->RenderBuildDebug();

	// render cursor
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1,1,1,1);
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
	if ((currentTime-m_LastUpdate > time_freq()) || (m_LastUpdate == 0))
		m_LastUpdate = currentTime;
		
	int step = time_freq()/60;
	
	if (step <= 0)
		step = 1;
	
	int i = 0;
	
	for (;m_LastUpdate < currentTime; m_LastUpdate += step)
	{
		if (Client()->Loaded())
			s_ShaderIntensity += 0.05f;
		
		if (i++ > 1)
		{
			m_LastUpdate = currentTime;
			break;
		}
	}
	
	
	// menu background effect
	vec2 s = vec2(Graphics()->ScreenWidth(), Graphics()->ScreenHeight())/8;
	Graphics()->MapScreen(0, 0, s.x, s.y);
	
	if (g_Config.m_GfxMultiBuffering && Client()->Loaded())
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

	// render background color
	Graphics()->TextureSet(-1);
	Graphics()->QuadsBegin();
		//vec4 Bottom(ms_GuiColor.r, ms_GuiColor.g, ms_GuiColor.b, 1.0f);
		//vec4 Top(ms_GuiColor.r, ms_GuiColor.g, ms_GuiColor.b, 1.0f);
		
		/*
		vec4 Bottom(0.2f, 0.25f, 0.3f, 1.0f);
		vec4 Top(0.2f, 0.25f, 0.3f, 1.0f);
		IGraphics::CColorVertex Array[4] = {
			IGraphics::CColorVertex(0, Top.r, Top.g, Top.b, Top.a),
			IGraphics::CColorVertex(1, Top.r, Top.g, Top.b, Top.a),
			IGraphics::CColorVertex(2, Bottom.r, Bottom.g, Bottom.b, Bottom.a),
			IGraphics::CColorVertex(3, Bottom.r, Bottom.g, Bottom.b, Bottom.a)};
		Graphics()->SetColorVertex(Array, 4);
			*/
			
		Graphics()->SetColor(0.2f, 0.25f, 0.3f, 1.0f);
		IGraphics::CQuadItem QuadItem(0, 0, s.x, s.y);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	if (g_Config.m_GfxMultiBuffering && Client()->Loaded())
	{
		Graphics()->TextureSet(-2, RENDERBUFFER_MENU);
		Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			QuadItem = IGraphics::CQuadItem(0, 0, s.x, s.y);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
	
	// restore screen
	{CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);}
}
