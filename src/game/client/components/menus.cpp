

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
#include <game/client/skelebank.h>
#include <game/localization.h>
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
	m_NavigationHasFocus = false;

	g_Config.m_UiPage = PAGE_INTERNET;

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
	str_copy(m_aFilterPresets[UI_FILTER_PRESET_ALL].m_aName, "All", sizeof(m_aFilterPresets[UI_FILTER_PRESET_ALL].m_aName));
	str_copy(m_aFilterPresets[UI_FILTER_PRESET_FAVORITES].m_aName, "Favorites", sizeof(m_aFilterPresets[UI_FILTER_PRESET_FAVORITES].m_aName));
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

int CMenus::DoButton_GridHeader(const void *pID, const char *pText, int Checked, const CUIRect *pRect)
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
	return UI()->DoButtonLogic(pID, pText, Checked, pRect);
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


int CMenus::RenderMenubar(CUIRect r)
{
	if(s_ResetMenu)
	{
		g_Config.m_UiPage = PAGE_INTERNET;
		s_ResetMenu = false;
	}
	const bool Offline = Client()->State() == IClient::STATE_OFFLINE;
	m_ActivePage = Offline ? g_Config.m_UiPage : m_GamePage;
	const bool Compact = UI()->Screen()->w < 900.0f;
	const char *apOfflineLabels[] = {"Play", "Character", "Progress", "Mods", "Replays", "Settings"};
	const int aOfflinePages[] = {PAGE_INTERNET, PAGE_CUSTOMIZE, PAGE_RESEARCH, PAGE_MODS, PAGE_DEMOS, PAGE_SETTINGS};
	const char *apGameLabels[] = {"Continue", "Game", "Players", "Server", "Vote", "Progress", "Settings", "Leave"};
	const char *apOfflineIcons[] = {">", "@", "+", "#", "R", "*"};
	const char *apGameIcons[] = {"<", ">", "P", "S", "V", "+", "*", "X"};
	const int aGamePages[] = {-2, PAGE_GAME, PAGE_PLAYERS, PAGE_SERVER_INFO, PAGE_CALLVOTE, PAGE_RESEARCH, PAGE_SETTINGS, -3};
	const char **apLabels = Offline ? apOfflineLabels : apGameLabels;
	const int *pPages = Offline ? aOfflinePages : aGamePages;
	const int Count = Offline ? 6 : 8;
	static int s_aNavigationButtons[8];

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
	for(int i = 0; i < Count; i++)
	{
		CUIRect Button;
		Box.HSplitTop(36.0f, &Button, &Box);
		const char *pText = Compact ? (Offline ? apOfflineIcons[i] : apGameIcons[i]) : Localize(apLabels[i]);
		const bool Focused = m_NavigationHasFocus && m_NavigationFocus == i;
		const bool Activated = DoButton_Menu(&s_aNavigationButtons[i], pText, pPages[i] == m_ActivePage || Focused, &Button, pPages[i] == -3 ? BUTTONSTYLE_DANGER : BUTTONSTYLE_NORMAL) || (Focused && m_LastInputDevice != 0 && m_EnterPressed);
		if(Activated)
		{
			m_EnterPressed = false;
			m_NavigationFocus = i;
			if(pPages[i] == -2) SetActive(false);
			else if(pPages[i] == -3) m_Popup = POPUP_QUIT;
			else NewPage = pPages[i];
		}
		Box.HSplitTop(5.0f, 0, &Box);
	}

	CUIRect Footer;
	r.HSplitBottom(104.0f, 0, &Footer);
	Footer.Margin(10.0f, &Footer);
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	char aIdentity[128];
	if(pPlatform && pPlatform->Available()) str_format(aIdentity, sizeof(aIdentity), Compact ? "STEAM\nONLINE" : "Steam  %llu\nONLINE", pPlatform->LocalUserID());
	else str_copy(aIdentity, Compact ? "NET\nUDP" : "Standalone network\nUDP available", sizeof(aIdentity));
	TextRender()->TextColor(ms_ColorAccentDim.r, ms_ColorAccentDim.g, ms_ColorAccentDim.b, 1.0f);
	UI()->DoLabelScaled(&Footer, aIdentity, Compact ? 8.0f : 9.0f, -1);
	TextRender()->TextColor(1, 1, 1, 1);
	if(Offline)
	{
		CUIRect Quit;
		Footer.HSplitBottom(30.0f, 0, &Quit);
		static int s_QuitButton;
		if(DoButton_Menu(&s_QuitButton, Compact ? "X" : Localize("Quit"), 0, &Quit, BUTTONSTYLE_DANGER)) m_Popup = POPUP_QUIT;
	}

	if(NewPage != -1)
	{
		if(Client()->State() == IClient::STATE_OFFLINE)
			g_Config.m_UiPage = NewPage;
		else
			m_GamePage = NewPage;
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
	LOCAL_MODE_INVASION = 0,
	LOCAL_MODE_HORDE,
	LOCAL_MODE_EXTRACTION,
	LOCAL_MODE_DM,
	LOCAL_MODE_TDM,
	LOCAL_MODE_CTF,
};

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

static const char *s_apLocalMaps[] = {
	"City I", "City II", "Space", "Large I", "Large II", "Large III", "Blue planet", "Foundry"};
static const char *s_apLocalMapCommands[] = {
	"generate_city1", "generate_city2", "generate_space1", "generate_large1", "generate_large2", "generate_large3", "generate_blueplanet1", "generate_foundry1"};
static const char *s_apLocalCtfMaps[] = {"Compact", "Standard"};
static const char *s_apLocalCtfMapCommands[] = {"generate_ctf_small1", "generate_ctf_medium1"};

struct CLocalGameMode
{
	const char *m_pName;
	const char *m_pDescription;
	const char *m_pConfig;
	bool m_Pve;
	bool m_SelectableMap;
	const char *const *m_ppMapNames;
	const char *const *m_ppMapCommands;
	int m_MapCount;
};

static const CLocalGameMode s_aLocalGameModes[] = {
	{"Invasion", "Explore generated floors, complete objectives and keep your build between maps.", "cfg/invasion_root.cfg", true, false, s_apLocalMaps, s_apLocalMapCommands, (int)(sizeof(s_apLocalMaps) / sizeof(s_apLocalMaps[0]))},
	{"Horde", "Defend, build and survive increasingly dangerous enemy waves.", "cfg/horde_root.cfg", true, true, s_apLocalMaps, s_apLocalMapCommands, (int)(sizeof(s_apLocalMaps) / sizeof(s_apLocalMaps[0]))},
	{"Extraction", "Finish the mission and reach the extraction zone before time runs out.", "cfg/extract_root.cfg", true, true, s_apLocalMaps, s_apLocalMapCommands, (int)(sizeof(s_apLocalMaps) / sizeof(s_apLocalMaps[0]))},
	{"Deathmatch", "Free-for-all combat with configurable AI opponents.", "cfg/dm_root.cfg", false, true, s_apLocalMaps, s_apLocalMapCommands, (int)(sizeof(s_apLocalMaps) / sizeof(s_apLocalMaps[0]))},
	{"Team deathmatch", "Team combat with building and configurable AI opponents.", "cfg/tdm_root.cfg", false, true, s_apLocalMaps, s_apLocalMapCommands, (int)(sizeof(s_apLocalMaps) / sizeof(s_apLocalMaps[0]))},
	{"Capture the flag", "Capture the enemy flag on a generated team map.", "cfg/ctf_root.cfg", false, true, s_apLocalCtfMaps, s_apLocalCtfMapCommands, (int)(sizeof(s_apLocalCtfMaps) / sizeof(s_apLocalCtfMaps[0]))},
};

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

static int LocalGameModeCount()
{
	return (int)(sizeof(s_aLocalGameModes) / sizeof(s_aLocalGameModes[0]));
}

static const CLocalGameMode &LocalGameMode(int Mode)
{
	return s_aLocalGameModes[clamp(Mode, 0, LocalGameModeCount() - 1)];
}

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
	pSettings->m_Bots = pSettings->m_pMode->m_Pve ? 0 : clamp(g_Config.m_ClLocalServerBots, 0, max(0, pSettings->m_MaxClients - 1));
	pSettings->m_Difficulty = clamp(g_Config.m_ClLocalServerDifficulty, 1, 50);
	pSettings->m_BotLevel = clamp(pSettings->m_Difficulty, 1, 30);
	pSettings->m_InvasionStart = clamp(g_Config.m_ClLocalServerInvasionStart, (int)LOCAL_INVASION_TEAM_CHECKPOINT, (int)LOCAL_INVASION_CUSTOM_FLOOR);
	pSettings->m_InvasionFloor = clamp(g_Config.m_ClLocalServerInvasionFloor, 1, max(1, g_Config.m_ClPveHighestInvasion));
	pSettings->m_Lan = g_Config.m_ClLocalServerLan != 0;
	pSettings->m_RandomSeed = g_Config.m_ClLocalServerRandomSeed != 0;
	pSettings->m_Seed = clamp(g_Config.m_ClLocalServerSeed, 0, 32767);
	pSettings->m_Roguelite = pSettings->m_pMode->m_Pve && g_Config.m_ClLocalServerRoguelite != 0;
	pSettings->m_Contracts = pSettings->m_Roguelite && g_Config.m_ClLocalServerContracts != 0;
	pSettings->m_MapLevel = pSettings->m_Difficulty;
	pSettings->m_UseCheckpoint = false;
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
	if(pSettings->m_Mode == LOCAL_MODE_HORDE)
		pSettings->m_ModeRule = clamp(g_Config.m_ClLocalServerHordeWaves, 0, 100);
	else if(pSettings->m_Mode == LOCAL_MODE_EXTRACTION)
		pSettings->m_ModeRule = clamp(g_Config.m_ClLocalServerExtractionTime, 2, 15);
	else if(pSettings->m_Mode == LOCAL_MODE_DM)
		pSettings->m_ModeRule = clamp(g_Config.m_ClLocalServerDmScore, 1, 1000);
	else if(pSettings->m_Mode == LOCAL_MODE_TDM)
		pSettings->m_ModeRule = clamp(g_Config.m_ClLocalServerTdmScore, 1, 1000);
	else if(pSettings->m_Mode == LOCAL_MODE_CTF)
		pSettings->m_ModeRule = clamp(g_Config.m_ClLocalServerCtfScore, 1, 1000);
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
	if(Settings.m_Mode == LOCAL_MODE_INVASION)
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
	else if(Settings.m_Mode >= LOCAL_MODE_DM)
		str_format(aRule, sizeof(aRule), Localize("Score %d"), Settings.m_ModeRule);
	else
		str_copy(aRule, Localize(Settings.m_Roguelite ? "Roguelite" : "Classic PvE"), sizeof(aRule));
	if(Settings.m_RandomSeed)
		str_copy(aSeed, Localize("Random seed"), sizeof(aSeed));
	else
		str_format(aSeed, sizeof(aSeed), Localize("Seed %d"), Settings.m_Seed);
	str_format(aSlots, sizeof(aSlots), Localize("%d slots"), Settings.m_MaxClients);

	if(Settings.m_Lan)
		str_format(pBuffer, BufferSize, "%s · %s · %s · %s · %s · %s · 127.0.0.1:%d / LAN:%d",
			Localize(Settings.m_pMode->m_pName), Localize(Settings.m_pMapName), aStart, aRule, aSeed, aSlots, Port, Port);
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
	char aDifficulty[64];
	char aBots[64];
	char aBotLevel[64];
	char aRandomSeed[64];
	char aSeed[64];
	char aRoguelite[64];
	char aContracts[64];
	char aCheckpoint[64];
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
	str_format(aDifficulty, sizeof(aDifficulty), "sv_mapgen_level %d", Settings.m_MapLevel);
	str_format(aBots, sizeof(aBots), "sv_bots %d", Settings.m_Bots);
	str_format(aBotLevel, sizeof(aBotLevel), "sv_botlevel %d", Settings.m_BotLevel);
	str_format(aRandomSeed, sizeof(aRandomSeed), "sv_mapgen_random_seed %d", Settings.m_RandomSeed);
	str_format(aSeed, sizeof(aSeed), "sv_mapgen_seed %d", Settings.m_Seed);
	str_format(aRoguelite, sizeof(aRoguelite), "sv_pve_roguelite %d", Settings.m_Roguelite);
	str_format(aContracts, sizeof(aContracts), "sv_pve_contracts %d", Settings.m_Contracts);
	str_format(aCheckpoint, sizeof(aCheckpoint), "sv_invasion_use_checkpoint %d", Settings.m_UseCheckpoint);
	if(Settings.m_Mode == LOCAL_MODE_HORDE)
		str_format(aModeRule, sizeof(aModeRule), "sv_scorelimit %d", Settings.m_ModeRule);
	else if(Settings.m_Mode == LOCAL_MODE_EXTRACTION)
		str_format(aModeRule, sizeof(aModeRule), "sv_timelimit %d", Settings.m_ModeRule);
	else if(Settings.m_Mode >= LOCAL_MODE_DM)
		str_format(aModeRule, sizeof(aModeRule), "sv_scorelimit %d", Settings.m_ModeRule);
	else
		str_format(aModeRule, sizeof(aModeRule), "sv_scorelimit 0");
	EscapeLocalServerValue(Settings.m_aName, aNameValue, sizeof(aNameValue));
	EscapeLocalServerValue(Settings.m_aPassword, aPasswordValue, sizeof(aPasswordValue));
	EscapeLocalServerValue(m_aLocalServerLogPath, aLogValue, sizeof(aLogValue));
	str_format(aName, sizeof(aName), "sv_name %s", aNameValue);
	str_format(aPassword, sizeof(aPassword), "password %s", aPasswordValue);
	str_format(aLog, sizeof(aLog), "logfile %s", aLogValue);

	const char *apArguments[30];
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
	apArguments[NumArguments++] = "sv_mapgen 1";
	apArguments[NumArguments++] = aDifficulty;
	apArguments[NumArguments++] = aBots;
	apArguments[NumArguments++] = aBotLevel;
	apArguments[NumArguments++] = aRandomSeed;
	apArguments[NumArguments++] = aSeed;
	apArguments[NumArguments++] = aRoguelite;
	apArguments[NumArguments++] = aContracts;
	apArguments[NumArguments++] = aCheckpoint;
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
				return Mode != LOCAL_MODE_INVASION;
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
				return Mode != LOCAL_MODE_INVASION;
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
					g_Config.m_ClLocalServerBots = clamp(g_Config.m_ClLocalServerBots + Direction, 0, max(0, g_Config.m_ClLocalServerMaxClients - 1));
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
		str_format(aLabel, sizeof(aLabel), "%s: %d", Localize("Player slots"), g_Config.m_ClLocalServerMaxClients);
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
		else
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
			UI()->DoLabelScaled(&Control, Localize(g_Config.m_ClLocalServerMode == LOCAL_MODE_INVASION ? "Automatic by floor and party size" : "Health, elites and party size"), 11.0f, -1);
		}
		else
		{
			DrawFocusMarker(Label, FOCUS_BOTS);
			g_Config.m_ClLocalServerBots = clamp(g_Config.m_ClLocalServerBots, 0, max(0, g_Config.m_ClLocalServerMaxClients - 1));
			str_format(aLabel, sizeof(aLabel), "%s: %d", Localize("AI players"), g_Config.m_ClLocalServerBots);
			UI()->DoLabelScaled(&Label, aLabel, 12.0f, -1);
			Control.HMargin(L(5.0f), &Control);
			const int MaxBots = max(0, g_Config.m_ClLocalServerMaxClients - 1);
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

		if(g_Config.m_ClLocalServerMode != LOCAL_MODE_INVASION)
		{
			SplitSettingRow(&Label, &Control);
			DrawFocusMarker(Label, FOCUS_MODE_RULE);
			const char *pRuleLabel = g_Config.m_ClLocalServerMode == LOCAL_MODE_HORDE ? "Target waves" : (g_Config.m_ClLocalServerMode == LOCAL_MODE_EXTRACTION ? "Mission time" : "Score limit");
			UI()->DoLabelScaled(&Label, Localize(pRuleLabel), 12.0f, -1);
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
			else if(g_Config.m_ClLocalServerMode == LOCAL_MODE_DM)
				str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerDmScore);
			else if(g_Config.m_ClLocalServerMode == LOCAL_MODE_TDM)
				str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerTdmScore);
			else
				str_format(aLabel, sizeof(aLabel), "%d", g_Config.m_ClLocalServerCtfScore);
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
	
	if(gs_TextureLogo == -1)
		gs_TextureLogo = Graphics()->LoadTexture("logo.png", IStorage::TYPE_ALL, CImageInfo::FORMAT_AUTO, 0);
	
	vec2 s = vec2(800, 120) * 0.72f;
	
	CUIRect Screen = *UI()->Screen();
	
	Graphics()->TextureSet(gs_TextureLogo);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	
	IGraphics::CQuadItem QuadItem((Screen.w-s.x)/2, 90, s.x, s.y);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();

	CUIRect ButtonCol;
	ButtonCol.w = 220.0f;
	ButtonCol.h = 200.0f;
	ButtonCol.x = (Screen.w - ButtonCol.w) * 0.5f;
	ButtonCol.y = 240.0f;

	CUIRect Button;
	ButtonCol.HSplitTop(32.0f, &Button, &ButtonCol);
	static int s_PlayButton=0;
	if(DoButton_Menu(&s_PlayButton, Localize("Play"), 0, &Button, BUTTONSTYLE_ACCENT) || m_EnterPressed)
	{
		ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
		g_Config.m_UiPage = PAGE_INTERNET;
	}
	
	ButtonCol.HSplitTop(8.0f, 0, &ButtonCol);
	ButtonCol.HSplitTop(32.0f, &Button, &ButtonCol);
	static int s_LocalButton=0;
	if(DoButton_Menu(&s_LocalButton, Localize("Local game"), 0, &Button, BUTTONSTYLE_ACCENT))
		g_Config.m_UiPage = PAGE_LOCAL_SERVER;

	ButtonCol.HSplitTop(8.0f, 0, &ButtonCol);
	ButtonCol.HSplitTop(32.0f, &Button, &ButtonCol);
	static int s_CustomizeButton=0;
	if(DoButton_Menu(&s_CustomizeButton, Localize("Customize"), 0, &Button))
		g_Config.m_UiPage = PAGE_CUSTOMIZE;
	
	ButtonCol.HSplitTop(8.0f, 0, &ButtonCol);
	ButtonCol.HSplitTop(32.0f, &Button, &ButtonCol);
	static int s_SettingsButton=0;
	static int s_ResearchButton=0;
	if(DoButton_Menu(&s_ResearchButton, Localize("Research"), 0, &Button))
		g_Config.m_UiPage = PAGE_RESEARCH;

	ButtonCol.HSplitTop(8.0f, 0, &ButtonCol);
	ButtonCol.HSplitTop(32.0f, &Button, &ButtonCol);
	if(DoButton_Menu(&s_SettingsButton, Localize("Settings"), 0, &Button))
		g_Config.m_UiPage = PAGE_SETTINGS;

	ButtonCol.HSplitTop(8.0f, 0, &ButtonCol);
	ButtonCol.HSplitTop(32.0f, &Button, &ButtonCol);
	static int s_QuitButton=0;
	if(DoButton_Menu(&s_QuitButton, Localize("Quit"), 0, &Button, BUTTONSTYLE_DANGER))
		m_Popup = POPUP_QUIT;
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
	static int s_Visibility = PLATFORM_LOBBY_FRIENDS;
	static int s_HostType = 0;
	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();

	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(10.0f, &MainView);
	CUIRect Title, Tabs, Body;
	MainView.HSplitTop(36.0f, &Title, &MainView);
	UI()->DoLabelScaled(&Title, Localize("Play"), 22.0f, -1);
	MainView.HSplitTop(32.0f, &Tabs, &Body);
	const char *apTabs[] = {"Browse rooms", "Create room", "LAN & direct"};
	static int s_aTabs[3];
	for(int i = 0; i < 3; i++)
	{
		CUIRect Button;
		Tabs.VSplitLeft(min(150.0f, Tabs.w / (3 - i)), &Button, &Tabs);
		if(DoButton_MenuTab(&s_aTabs[i], Localize(apTabs[i]), m_PlayTab == i, &Button, i == 0 ? CUI::CORNER_TL : i == 2 ? CUI::CORNER_TR : 0))
		{
			m_PlayTab = i;
			if(i == 0) { ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET); if(pPlatform && pPlatform->Available()) pPlatform->RefreshLobbyList(); }
			if(i == 2) ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
		}
	}
	Body.HSplitTop(8.0f, 0, &Body);

	if(m_PlayTab == 1)
	{
		CUIRect Choice, Content, Button, Row, Label, Edit;
		Body.HSplitTop(36.0f, &Choice, &Content);
		static int s_SteamHost, s_LocalHost;
		Choice.VSplitLeft(170.0f, &Button, &Choice);
		if(DoButton_Menu(&s_SteamHost, Localize("Steam room"), s_HostType == 0, &Button, BUTTONSTYLE_ACCENT)) s_HostType = 0;
		Choice.VSplitLeft(6.0f, 0, &Choice);
		Choice.VSplitLeft(190.0f, &Button, &Choice);
		if(DoButton_Menu(&s_LocalHost, Localize("Local / LAN game"), s_HostType == 1, &Button)) s_HostType = 1;
		Content.HSplitTop(8.0f, 0, &Content);
		if(s_HostType == 1)
		{
			RenderLocalServer(Content);
			return;
		}
		DrawMenuInset(&Content, CUI::CORNER_ALL);
		Content.Margin(12.0f, &Content);
		if(!pPlatform || !pPlatform->Available())
		{
			CUIRect Banner;
			Content.HSplitTop(42.0f, &Banner, &Content);
			DrawMenuBorder(&Banner, ms_ColorBgInset, ms_ColorDanger, CUI::CORNER_ALL, ms_ControlRounding);
			Banner.Margin(8.0f, &Banner);
			UI()->DoLabelScaled(&Banner, Localize("Steam hosting is unavailable. Start Steam or create a LAN game."), 11.0f, -1);
		}
		Content.HSplitTop(30.0f, &Row, &Content); Row.VSplitLeft(110.0f, &Label, &Edit); UI()->DoLabelScaled(&Label, Localize("Room name"), 11.0f, -1); static float s_NameOffset; DoEditBox(&g_Config.m_ClLocalServerName, &Edit, g_Config.m_ClLocalServerName, sizeof(g_Config.m_ClLocalServerName), 11.0f, &s_NameOffset);
		Content.HSplitTop(8.0f, 0, &Content);
		Content.HSplitTop(30.0f, &Row, &Content); Row.VSplitLeft(110.0f, &Label, &Edit); UI()->DoLabelScaled(&Label, Localize("Password"), 11.0f, -1); static float s_PasswordOffset; DoEditBox(&g_Config.m_ClLocalServerPassword, &Edit, g_Config.m_ClLocalServerPassword, sizeof(g_Config.m_ClLocalServerPassword), 11.0f, &s_PasswordOffset, true);
		Content.HSplitTop(12.0f, 0, &Content);
		Content.HSplitTop(34.0f, &Row, &Content);
		const char *apVisibility[] = {"Invite only", "Friends", "Public"}; static int s_aVisibility[3];
		for(int i = 0; i < 3; i++) { Row.VSplitLeft(120.0f, &Button, &Row); if(DoButton_Menu(&s_aVisibility[i], Localize(apVisibility[i]), s_Visibility == i, &Button)) s_Visibility = i; Row.VSplitLeft(5.0f, 0, &Row); }
		Content.HSplitTop(14.0f, 0, &Content);
		char aSummary[512];
		str_format(aSummary, sizeof(aSummary), Localize("Relay Listen Server  |  %s  |  %s on %s  |  %d players  |  Steam authentication  |  Unofficial"), Localize(apVisibility[s_Visibility]), g_Config.m_SvGametype, g_Config.m_SvMap, g_Config.m_ClLocalServerMaxClients);
		CUIRect Summary; Content.HSplitTop(60.0f, &Summary, &Content); DrawMenuInset(&Summary, CUI::CORNER_ALL); Summary.Margin(8.0f, &Summary); UI()->DoLabelScaled(&Summary, aSummary, FitLabelFontSize(TextRender(), aSummary, 11.0f, Summary.w), -1);
		Content.HSplitTop(12.0f, 0, &Content); Content.HSplitTop(36.0f, &Button, &Content); static int s_Create;
		if(DoButton_Menu(&s_Create, Localize("Create Steam room"), 0, &Button, BUTTONSTYLE_ACCENT) && pPlatform && pPlatform->Available())
		{
			CHostGameSettings Settings; mem_zero(&Settings, sizeof(Settings)); Settings.m_Visibility = s_Visibility; Settings.m_MaxClients = g_Config.m_ClLocalServerMaxClients;
			str_copy(Settings.m_aName, g_Config.m_ClLocalServerName, sizeof(Settings.m_aName)); str_copy(Settings.m_aPassword, g_Config.m_ClLocalServerPassword, sizeof(Settings.m_aPassword)); str_copy(Settings.m_aMap, g_Config.m_SvMap, sizeof(Settings.m_aMap)); str_copy(Settings.m_aGameType, g_Config.m_SvGametype, sizeof(Settings.m_aGameType)); str_copy(Settings.m_aModHash, g_Config.m_ClModHash, sizeof(Settings.m_aModHash)); str_copy(Settings.m_aModIDs, g_Config.m_ClModIds, sizeof(Settings.m_aModIDs));
			Client()->StartSteamHostedGame(Settings);
		}
		CClientAsyncStatus Status; Client()->SteamHostedGameStatus(&Status);
		if(Status.m_State == CLIENT_ASYNC_WORKING || Status.m_State == CLIENT_ASYNC_FAILED) { Content.HSplitTop(10.0f, 0, &Content); char aStatus[256]; if(Status.m_State == CLIENT_ASYNC_FAILED) str_copy(aStatus, Localize(Status.m_aErrorKey), sizeof(aStatus)); else str_format(aStatus, sizeof(aStatus), "%s  %d%%", Localize(Status.m_Stage == CLIENT_STAGE_STARTING_SERVER ? "Starting server" : "Creating room"), (int)(Status.m_Progress * 100)); UI()->DoLabelScaled(&Content, aStatus, 11.0f, -1); }
		return;
	}

	if(m_PlayTab == 2)
	{
		CUIRect Toolbar, Button;
		Body.HSplitTop(36.0f, &Toolbar, &Body);
		static int s_RefreshLan, s_CreateLocal;
		Toolbar.VSplitLeft(120.0f, &Button, &Toolbar); if(DoButton_Menu(&s_RefreshLan, Localize("Refresh LAN"), 0, &Button)) ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
		Toolbar.VSplitLeft(6.0f, 0, &Toolbar); Toolbar.VSplitLeft(160.0f, &Button, &Toolbar); if(DoButton_Menu(&s_CreateLocal, Localize("Create local game"), 0, &Button, BUTTONSTYLE_ACCENT)) { m_PlayTab = 1; s_HostType = 1; }
		Body.HSplitTop(6.0f, 0, &Body);
		RenderServerbrowser(Body);
		return;
	}

	CUIRect StatusBar, Filters, List, Detail, Actions, Button;
	Body.HSplitTop(30.0f, &StatusBar, &Body);
	CClientAsyncStatus Connection; Client()->ConnectionStatus(&Connection);
	CPlatformOperationStatus LobbyStatus; mem_zero(&LobbyStatus, sizeof(LobbyStatus)); if(pPlatform) pPlatform->LobbyOperationStatus(&LobbyStatus);
	if(LobbyStatus.m_State == CLIENT_ASYNC_WORKING && LobbyStatus.m_Stage == CLIENT_STAGE_JOINING_ROOM) { CUIRect Cancel; StatusBar.VSplitRight(90.0f, &StatusBar, &Cancel); UI()->DoLabelScaled(&StatusBar, Localize("Joining room"), 10.0f, -1); static int s_CancelJoin; if(DoButton_Menu(&s_CancelJoin, Localize("Cancel"), 0, &Cancel, BUTTONSTYLE_DANGER) || m_EscapePressed) { pPlatform->LeaveLobby(); m_EscapePressed = false; } }
	else if(Connection.m_State == CLIENT_ASYNC_FAILED) { TextRender()->TextColor(ms_ColorDanger.r, ms_ColorDanger.g, ms_ColorDanger.b, 1.0f); UI()->DoLabelScaled(&StatusBar, Localize(Connection.m_aErrorKey), 10.0f, -1); TextRender()->TextColor(1, 1, 1, 1); }
	else if(!pPlatform || !pPlatform->Available()) UI()->DoLabelScaled(&StatusBar, Localize("Steam unavailable — dedicated servers, Favorites and direct connection remain available."), 10.0f, -1);
	else UI()->DoLabelScaled(&StatusBar, Localize("Dedicated servers and Steam rooms"), 10.0f, -1);
	Body.HSplitTop(28.0f, &Filters, &Body);
	const char *apFilters[] = {"All", "Official", "Community", "Friends", "Modded", "Favorites"}; static int s_aFilters[6];
	for(int i = 0; i < 6; i++) { Filters.VSplitLeft(86.0f, &Button, &Filters); if(DoButton_MenuTab(&s_aFilters[i], Localize(apFilters[i]), s_Filter == i, &Button, 0)) s_Filter = i; Filters.VSplitLeft(3.0f, 0, &Filters); }
	Body.HSplitTop(6.0f, 0, &Body); Body.HSplitBottom(40.0f, &Body, &Actions); Body.VSplitRight(max(220.0f, Body.w * 0.30f), &List, &Detail); List.VSplitRight(6.0f, &List, 0);
	CPlayRoomEntry aEntries[512]; int EntryCount = 0;
	for(int i = 0; i < ServerBrowser()->NumSortedServers() && EntryCount < 512; i++) { const CServerInfo *pInfo = ServerBrowser()->SortedGet(i); if(!pInfo) continue; const bool Show = s_Filter == 0 || (s_Filter == 1 && pInfo->m_Official) || (s_Filter == 2 && !pInfo->m_Official) || (s_Filter == 4 && pInfo->m_Modded) || (s_Filter == 5 && pInfo->m_Favorite); if(Show) { CPlayRoomEntry &Entry = aEntries[EntryCount++]; Entry.m_Source = CPlayRoomEntry::SOURCE_DEDICATED; Entry.m_SourceIndex = i; str_copy(Entry.m_aStableID, pInfo->m_aAddress, sizeof(Entry.m_aStableID)); } }
	if(pPlatform && pPlatform->Available()) for(int i = 0; i < pPlatform->LobbyCount() && EntryCount < 512; i++) { CPlatformLobbyInfo Info; pPlatform->LobbyInfo(i, &Info); const bool Show = s_Filter == 0 || (s_Filter == 2) || (s_Filter == 3 && Info.m_FriendHosted) || (s_Filter == 4 && Info.m_Modded); if(Show) { CPlayRoomEntry &Entry = aEntries[EntryCount++]; Entry.m_Source = CPlayRoomEntry::SOURCE_STEAM_LOBBY; Entry.m_SourceIndex = i; str_format(Entry.m_aStableID, sizeof(Entry.m_aStableID), "lobby:%llu", Info.m_LobbyID); } }
	static int s_aEntryIDs[512]; for(int i = 0; i < 512; i++) s_aEntryIDs[i] = i;
	UiDoListboxStart(&s_aEntryIDs, &List, 34.0f, Localize("Rooms"), "", EntryCount, 1, s_Selected, s_Scroll);
	for(int i = 0; i < EntryCount; i++) { CListboxItem Item = UiDoListboxNextItem(&s_aEntryIDs[i], s_Selected == i); if(!Item.m_Visible) continue; char aLine[512]; if(aEntries[i].m_Source == CPlayRoomEntry::SOURCE_DEDICATED) { const CServerInfo *pInfo = ServerBrowser()->SortedGet(aEntries[i].m_SourceIndex); str_format(aLine, sizeof(aLine), "%s  %s  |  %s  %s  |  %d/%d  |  %dms%s%s", pInfo->m_Official ? Localize("OFFICIAL") : Localize("COMMUNITY"), pInfo->m_aName, pInfo->m_aGameType, pInfo->m_aMap, pInfo->m_NumClients, pInfo->m_MaxClients, pInfo->m_Latency, pInfo->m_Flags ? "  LOCK" : "", pInfo->m_Modded ? "  MOD" : ""); } else { CPlatformLobbyInfo Info; pPlatform->LobbyInfo(aEntries[i].m_SourceIndex, &Info); str_format(aLine, sizeof(aLine), "%s  %s  |  %s  %s  |  %d/%d  |  RELAY%s%s", Info.m_FriendHosted ? Localize("FRIEND") : Localize("STEAM"), Info.m_aHostName, Info.m_aGameType, Info.m_aMap, Info.m_Members, Info.m_MaxMembers, Info.m_Password ? "  LOCK" : "", Info.m_Modded ? "  MOD" : ""); } Item.m_Rect.Margin(6.0f, &Item.m_Rect); UI()->DoLabelScaled(&Item.m_Rect, aLine, FitLabelFontSize(TextRender(), aLine, 10.5f, Item.m_Rect.w), -1); }
	s_Selected = UiDoListboxEnd(&s_Scroll, 0); s_Selected = clamp(s_Selected, -1, max(-1, EntryCount - 1));
	DrawMenuInset(&Detail, CUI::CORNER_ALL); Detail.Margin(10.0f, &Detail);
	if(s_Selected < 0) UI()->DoLabelScaled(&Detail, Localize("Select a room to see address, region, version, Mods and join requirements."), 11.0f, -1);
	else { char aDetail[768]; const CPlayRoomEntry &Entry = aEntries[s_Selected]; if(Entry.m_Source == CPlayRoomEntry::SOURCE_DEDICATED) { const CServerInfo *pInfo = ServerBrowser()->SortedGet(Entry.m_SourceIndex); str_format(aDetail, sizeof(aDetail), "%s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s", pInfo->m_aName, Localize("Address"), pInfo->m_aAddress, Localize("Version"), pInfo->m_aVersion, Localize("Source"), pInfo->m_DiscoverySources & IServerBrowser::DISCOVERY_STEAM ? "Steam GameServer + UDP" : "UDP", Localize("Mods"), pInfo->m_Modded ? Localize("Required") : Localize("None"), Localize("Authentication"), pInfo->m_AuthPolicy ? Localize("Required") : Localize("Open")); } else { CPlatformLobbyInfo Info; pPlatform->LobbyInfo(Entry.m_SourceIndex, &Info); str_format(aDetail, sizeof(aDetail), "%s\nLobbyID: %llu\n%s: %llu\n%s: %s\n%s: %s\nMod hash: %s\nRelay / Steam authentication", Info.m_aHostName, Info.m_LobbyID, Localize("Host"), Info.m_HostSteamID, Localize("Region"), Info.m_aRegion, Localize("Source"), Info.m_FriendHosted ? Localize("Friend room") : Localize("Steam local room"), Info.m_aModHash); } UI()->DoLabelScaled(&Detail, aDetail, 10.5f, -1); }
	Actions.HSplitTop(6.0f, 0, &Actions); Actions.VSplitRight(110.0f, &Actions, &Button); static int s_Join; if(DoButton_Menu(&s_Join, Localize("Join"), 0, &Button, BUTTONSTYLE_ACCENT) && s_Selected >= 0) { const CPlayRoomEntry &Entry = aEntries[s_Selected]; if(Entry.m_Source == CPlayRoomEntry::SOURCE_DEDICATED) Client()->Connect(ServerBrowser()->SortedGet(Entry.m_SourceIndex)->m_aAddress); else { CPlatformLobbyInfo Info; if(pPlatform->LobbyInfo(Entry.m_SourceIndex, &Info)) pPlatform->JoinLobby(Info.m_LobbyID); } }
	Actions.VSplitRight(6.0f, &Actions, 0); Actions.VSplitRight(100.0f, &Actions, &Button); static int s_Refresh; if(DoButton_Menu(&s_Refresh, Localize("Refresh"), 0, &Button)) { ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET); if(pPlatform && pPlatform->Available()) pPlatform->RefreshLobbyList(); }
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
		if(g_Config.m_UiPage == PAGE_FRONT || g_Config.m_UiPage == PAGE_STEAM || g_Config.m_UiPage == PAGE_INTERNET)
		{
			g_Config.m_UiPage = PAGE_INTERNET;
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
			IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
			if(pPlatform && pPlatform->Available()) pPlatform->RefreshLobbyList();
		}
		else if(g_Config.m_UiPage == PAGE_LAN)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
		else if(g_Config.m_UiPage == PAGE_FAVORITES)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
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
		RenderMenubar(Navigation);

		// render current page
		if(Client()->State() != IClient::STATE_OFFLINE)
		{
			if(m_GamePage == PAGE_GAME)
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

		if(m_Popup == POPUP_QUIT)
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
			if(m_Popup == POPUP_NONE && Client()->State() != IClient::STATE_OFFLINE)
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
				if(m_Popup == POPUP_NONE && Client()->State() != IClient::STATE_OFFLINE)
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
