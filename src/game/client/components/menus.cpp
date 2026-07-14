

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

	g_Config.m_UiPage = PAGE_FRONT;

	m_NeedRestartGraphics = false;
	m_NeedRestartSound = false;
	m_NeedSendinfo = false;
	m_MenuActive = true;
	m_UseMouseButtons = true;

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

static vec4 ColorFromUiConfig(int Hue, int Sat, int Lht, int Alpha)
{
	vec3 Rgb = HslToRgb(vec3(Hue / 255.0f, Sat / 255.0f, Lht / 255.0f));
	return vec4(Rgb.r, Rgb.g, Rgb.b, Alpha / 255.0f);
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
	const float MaxW = g_Config.m_UiWideview ? 900.0f : 780.0f;
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
	UI()->DoEditBox(pLineInput, pRect, FontSize, Corners, &Changed);
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
	CUIRect Box = r;
	CUIRect Button;
	// CUIRect split helpers apply UI()->Scale() internally. Divide it out here
	// so the complete menubar scales to the actual available width instead of
	// growing back into the buttons anchored on the right at UI scales > 100%.
	const float MenuScale = clamp(r.w / 700.0f, 0.68f, 1.0f) / max(0.01f, UI()->Scale());

	
	if (s_ResetMenu)
	{
		g_Config.m_UiPage = PAGE_FRONT;
		s_ResetMenu = false;
	}
	
	m_ActivePage = g_Config.m_UiPage;
	int NewPage = -1;

	if(Client()->State() != IClient::STATE_OFFLINE)
		m_ActivePage = m_GamePage;

	if(Client()->State() == IClient::STATE_OFFLINE)
	{
		Box.VSplitLeft(90.0f * MenuScale, &Button, &Box);
		static int s_InternetButton=0;
		if(DoButton_MenuTab(&s_InternetButton, Localize("Internet"), m_ActivePage==PAGE_INTERNET, &Button, CUI::CORNER_TL))
		{
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
			m_ActiveFilterPreset = UI_FILTER_PRESET_ALL;
			NewPage = PAGE_INTERNET;
		}

		Box.VSplitLeft(4.0f * MenuScale, 0, &Box);
		Box.VSplitLeft(70.0f * MenuScale, &Button, &Box);
		static int s_LanButton=0;
		if(DoButton_MenuTab(&s_LanButton, Localize("LAN"), m_ActivePage==PAGE_LAN, &Button, 0))
		{
			ServerBrowser()->Refresh(IServerBrowser::TYPE_LAN);
			m_ActiveFilterPreset = UI_FILTER_PRESET_ALL;
			NewPage = PAGE_LAN;
		}

		Box.VSplitLeft(4.0f * MenuScale, 0, &Box);
		Box.VSplitLeft(100.0f * MenuScale, &Button, &Box);
		static int s_FavoritesButton=0;
		if(DoButton_MenuTab(&s_FavoritesButton, Localize("Favorites"), m_ActivePage==PAGE_FAVORITES, &Button, CUI::CORNER_TR))
		{
			ServerBrowser()->Refresh(IServerBrowser::TYPE_FAVORITES);
			m_ActiveFilterPreset = UI_FILTER_PRESET_FAVORITES;
			NewPage = PAGE_FAVORITES;
		}

		Box.VSplitLeft(12.0f * MenuScale, 0, &Box);
		Box.VSplitLeft(80.0f * MenuScale, &Button, &Box);
		static int s_DemosButton=0;
		if(DoButton_MenuTab(&s_DemosButton, Localize("Demos"), m_ActivePage==PAGE_DEMOS, &Button, CUI::CORNER_T))
		{
			DemolistPopulate();
			NewPage = PAGE_DEMOS;
		}

		Box.VSplitLeft(6.0f * MenuScale, 0, &Box);
		Box.VSplitLeft(96.0f * MenuScale, &Button, &Box);
		static int s_ResearchButton=0;
		if(DoButton_MenuTab(&s_ResearchButton, Localize("Research"), m_ActivePage==PAGE_RESEARCH, &Button, CUI::CORNER_T))
			NewPage = PAGE_RESEARCH;

		Box.VSplitRight(110.0f * MenuScale, &Box, &Button);
		static int s_MenuButton=0;
		if(DoButton_MenuTab(&s_MenuButton, Localize("Main menu"), m_ActivePage==PAGE_FRONT, &Button, CUI::CORNER_T) || m_EscapePressed)
			NewPage = PAGE_FRONT;

		Box.VSplitRight(6.0f * MenuScale, &Box, 0);
		Box.VSplitRight(90.0f * MenuScale, &Box, &Button);
		static int s_SettingsButton=0;
		if(DoButton_MenuTab(&s_SettingsButton, Localize("Settings"), m_ActivePage==PAGE_SETTINGS, &Button, CUI::CORNER_T))
			NewPage = PAGE_SETTINGS;
	}
	else
	{
		Box.VSplitLeft(80.0f * MenuScale, &Button, &Box);
		static int s_GameButton=0;
		if(DoButton_MenuTab(&s_GameButton, Localize("Game"), m_ActivePage==PAGE_GAME, &Button, CUI::CORNER_TL))
			NewPage = PAGE_GAME;

		Box.VSplitLeft(4.0f * MenuScale, 0, &Box);
		Box.VSplitLeft(80.0f * MenuScale, &Button, &Box);
		static int s_PlayersButton=0;
		if(DoButton_MenuTab(&s_PlayersButton, Localize("Players"), m_ActivePage==PAGE_PLAYERS, &Button, 0))
			NewPage = PAGE_PLAYERS;

		Box.VSplitLeft(4.0f * MenuScale, 0, &Box);
		Box.VSplitLeft(110.0f * MenuScale, &Button, &Box);
		static int s_ServerInfoButton=0;
		if(DoButton_MenuTab(&s_ServerInfoButton, Localize("Server info"), m_ActivePage==PAGE_SERVER_INFO, &Button, 0))
			NewPage = PAGE_SERVER_INFO;

		Box.VSplitLeft(4.0f * MenuScale, 0, &Box);
		Box.VSplitLeft(110.0f * MenuScale, &Button, &Box);
		static int s_CallVoteButton=0;
		if(DoButton_MenuTab(&s_CallVoteButton, Localize("Call vote"), m_ActivePage==PAGE_CALLVOTE, &Button, CUI::CORNER_TR))
			NewPage = PAGE_CALLVOTE;

		Box.VSplitLeft(4.0f * MenuScale, 0, &Box);
		Box.VSplitLeft(96.0f * MenuScale, &Button, &Box);
		static int s_ResearchButton=0;
		if(DoButton_MenuTab(&s_ResearchButton, Localize("Research"), m_ActivePage==PAGE_RESEARCH, &Button, CUI::CORNER_T))
			NewPage = PAGE_RESEARCH;
		
		Box.VSplitRight(90.0f * MenuScale, &Box, &Button);
		static int s_QuitButton=0;
		if(DoButton_MenuTab(&s_QuitButton, Localize("Quit"), 0, &Button, CUI::CORNER_T))
			m_Popup = POPUP_QUIT;
		
		Box.VSplitRight(6.0f * MenuScale, &Box, 0);
		Box.VSplitRight(90.0f * MenuScale, &Box, &Button);
		static int s_SettingsButton=0;
		if(DoButton_MenuTab(&s_SettingsButton, Localize("Settings"), m_ActivePage==PAGE_SETTINGS, &Button, CUI::CORNER_T))
			NewPage = PAGE_SETTINGS;
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




int CMenus::Render()
{
	CUIRect Screen = *UI()->Screen();
	Graphics()->MapScreen(Screen.x, Screen.y, Screen.w, Screen.h);
	
	static bool s_First = true;
	if(s_First)
	{
		if(g_Config.m_UiPage == PAGE_INTERNET)
			ServerBrowser()->Refresh(IServerBrowser::TYPE_INTERNET);
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

	CUIRect TabBar;
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
		Screen.HSplitTop(28.0f, &TabBar, &MainView);

		if(g_Config.m_UiPage == PAGE_FRONT)
		{
			RenderFront(MainView);
			return 0;
		}
		else if(g_Config.m_UiPage == PAGE_CUSTOMIZE)
		{
			RenderCustomize(MainView);
			return 0;
		}
		
		RenderMenubar(TabBar);

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
		else if(g_Config.m_UiPage == PAGE_INTERNET)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_LAN)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_DEMOS)
			RenderDemoList(MainView);
		else if(g_Config.m_UiPage == PAGE_FAVORITES)
			RenderServerbrowser(MainView);
		else if(g_Config.m_UiPage == PAGE_SETTINGS)
			RenderSettings(MainView);
		else if(g_Config.m_UiPage == PAGE_RESEARCH)
			m_pClient->m_pPveRoguelite->RenderResearch(MainView);
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
			pTitle = Localize("Connecting to");
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

	// special handle esc and enter for popup purposes
	if(e.m_Flags&IInput::FLAG_PRESS)
	{
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
			m_EscapePressed = true;
			SetActive(!IsActive());
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
			if(e.m_Key == KEY_RETURN || e.m_Key == KEY_KP_ENTER)
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
	ms_ColorAccent = ColorFromUiConfig(g_Config.m_UiColorHue, g_Config.m_UiColorSat, g_Config.m_UiColorLht, g_Config.m_UiColorAlpha);
	ms_ColorAccentDim = ColorFromUiConfig(g_Config.m_UiColorHue2, g_Config.m_UiColorSat2, g_Config.m_UiColorLht2, g_Config.m_UiColorAlpha2);
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
