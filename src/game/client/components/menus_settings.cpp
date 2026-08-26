

#include <base/math.h>
#include <cstring>

#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/storage.h>
#include <engine/platform_services.h>
#include <engine/textrender.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/components/sounds.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/client/render.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include "binds.h"
#include "countryflags.h"
#include "menus.h"
#include "skins.h"

CMenusKeyBinder CMenus::m_Binder;

static void ResetThemeDefaults()
{
	g_Config.m_UiColorHue = 150;
	g_Config.m_UiColorSat = 16;
	g_Config.m_UiColorLht = 188;
	g_Config.m_UiColorAlpha = 222;
	g_Config.m_UiColorHue2 = 150;
	g_Config.m_UiColorSat2 = 10;
	g_Config.m_UiColorLht2 = 128;
	g_Config.m_UiColorAlpha2 = 190;
}

namespace
{
const char *HudPresetLabel(int Preset)
{
	static const char *const s_apLabels[] = {"Minimal", "Standard", "Full"};
	return s_apLabels[clamp(Preset, 0, 2)];
}

int CurrentHudPreset()
{
	if(g_Config.m_ClShowhud && g_Config.m_ClShowhudHealthAmmo && !g_Config.m_ClShowhudScore &&
	   g_Config.m_ClShowhudTimer && !g_Config.m_ClShowhudSpectatorCount && !g_Config.m_ClShowfps &&
	   !g_Config.m_ClShowhudPlayerPosition && !g_Config.m_ClShowhudPlayerSpeed && !g_Config.m_ClShowhudPlayerAngle &&
	   g_Config.m_ClPveObjectiveDisplay == 2)
		return 0;
	if(g_Config.m_ClShowhud && g_Config.m_ClShowhudHealthAmmo && g_Config.m_ClShowhudScore &&
	   g_Config.m_ClShowhudTimer && !g_Config.m_ClShowhudSpectatorCount && !g_Config.m_ClShowfps &&
	   !g_Config.m_ClShowhudPlayerPosition && !g_Config.m_ClShowhudPlayerSpeed && !g_Config.m_ClShowhudPlayerAngle &&
	   g_Config.m_ClPveObjectiveDisplay == 2)
		return 1;
	if(g_Config.m_ClShowhud && g_Config.m_ClShowhudHealthAmmo && g_Config.m_ClShowhudScore &&
	   g_Config.m_ClShowhudTimer && g_Config.m_ClShowhudSpectatorCount && g_Config.m_ClShowfps &&
	   g_Config.m_ClShowhudPlayerPosition && g_Config.m_ClShowhudPlayerSpeed && g_Config.m_ClShowhudPlayerAngle &&
	   g_Config.m_ClPveObjectiveDisplay == 1)
		return 2;
	return -1;
}

void ApplyHudPreset(int Preset)
{
	Preset = clamp(Preset, 0, 2);
	g_Config.m_ClShowhud = 1;
	g_Config.m_ClShowhudHealthAmmo = 1;
	g_Config.m_ClShowhudTimer = 1;
	g_Config.m_ClShowhudScore = Preset != 0;
	g_Config.m_ClShowhudSpectatorCount = Preset == 2;
	g_Config.m_ClShowfps = Preset == 2;
	g_Config.m_ClShowhudPlayerPosition = Preset == 2;
	g_Config.m_ClShowhudPlayerSpeed = Preset == 2;
	g_Config.m_ClShowhudPlayerAngle = Preset == 2;
	g_Config.m_ClPveObjectiveDisplay = Preset == 2 ? 1 : 2;
}

const char *GraphicsQualityPresetLabel(int Preset)
{
	static const char *const s_apLabels[] = {"Low", "Balanced", "High"};
	return s_apLabels[clamp(Preset, 0, 2)];
}

int CurrentGraphicsQualityPreset()
{
	if(!g_Config.m_GfxTextureQuality && !g_Config.m_GfxHighDetail && !g_Config.m_GfxFsaaSamples &&
	   !g_Config.m_GfxAsyncRender && !g_Config.m_GfxMultiBuffering && g_Config.m_GfxTextureCompression)
		return 0;
	if(g_Config.m_GfxTextureQuality && g_Config.m_GfxHighDetail && !g_Config.m_GfxFsaaSamples &&
	   !g_Config.m_GfxAsyncRender && g_Config.m_GfxMultiBuffering && !g_Config.m_GfxTextureCompression)
		return 1;
	if(g_Config.m_GfxTextureQuality && g_Config.m_GfxHighDetail && g_Config.m_GfxFsaaSamples == 4 &&
	   g_Config.m_GfxAsyncRender && g_Config.m_GfxMultiBuffering && !g_Config.m_GfxTextureCompression)
		return 2;
	return -1;
}

void ApplyGraphicsQualityPreset(int Preset)
{
	Preset = clamp(Preset, 0, 2);
	if(Preset == 0)
	{
		g_Config.m_GfxTextureQuality = 0;
		g_Config.m_GfxTextureCompression = 1;
		g_Config.m_GfxHighDetail = 0;
		g_Config.m_GfxFsaaSamples = 0;
		g_Config.m_GfxAsyncRender = 0;
		g_Config.m_GfxMultiBuffering = 0;
		g_Config.m_ClLighting = 0;
	}
	else
	{
		g_Config.m_GfxTextureQuality = 1;
		g_Config.m_GfxTextureCompression = 0;
		g_Config.m_GfxHighDetail = 1;
		g_Config.m_GfxFsaaSamples = Preset == 2 ? 4 : 0;
		g_Config.m_GfxAsyncRender = Preset == 2;
		g_Config.m_GfxMultiBuffering = 1;
		g_Config.m_ClLighting = 1;
	}
}

int CurrentGfxDisplayMode()
{
	if(g_Config.m_GfxFullscreen)
		return 0;
	if(g_Config.m_GfxBorderless)
		return 1;
	return 2;
}

void ApplyGfxDisplayMode(int Mode)
{
	Mode = clamp(Mode, 0, 2);
	g_Config.m_GfxFullscreen = Mode == 0 ? 1 : 0;
	g_Config.m_GfxBorderless = Mode == 1 ? 1 : 0;
}
} // namespace

CMenusKeyBinder::CMenusKeyBinder()
{
	m_TakeKey = false;
	m_GotKey = false;
}

bool CMenusKeyBinder::OnInput(IInput::CEvent Event)
{
	if(m_TakeKey)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS)
		{
			m_Key = Event;
			m_GotKey = true;
			m_TakeKey = false;
		}
		return true;
	}

	return false;
}

void CMenus::RenderSettingsGeneral(CUIRect MainView)
{
	char aBuf[128];
	CUIRect Label, Button, Left, Right, TabBar, Content;

	static int s_GeneralSubPage = 0;
	const char *apTabs[] = {Localize("Language"), Localize("Chat"), Localize("Theme")};
	const int NumTabs = (int)(sizeof(apTabs) / sizeof(apTabs[0]));

	MainView.HSplitTop(24.0f, &TabBar, &MainView);
	MainView.HSplitTop(6.0f, 0, &MainView);
	Content = MainView;

	{
		CUIRect Tab;
		float TabW = TabBar.w / NumTabs;
		for(int i = 0; i < NumTabs; i++)
		{
			TabBar.VSplitLeft(TabW, &Tab, &TabBar);
			Tab.VMargin(2.0f, &Tab);
			if(DoButton_MenuTab(apTabs[i], apTabs[i], s_GeneralSubPage == i, &Tab, CUI::CORNER_T))
				s_GeneralSubPage = i;
		}
	}

	DrawMenuInset(&Content, CUI::CORNER_B | CUI::CORNER_TR);
	Content.Margin(8.0f, &Content);
	if(s_GeneralSubPage == 0)
	{
		RenderLanguageSelection(Content);
		return;
	}

	Content.VSplitMid(&Left, &Right);
	Left.VSplitRight(6.0f, &Left, 0);
	Right.VMargin(6.0f, &Right);

	if(s_GeneralSubPage == 1) // Chat
	{
		Left.HSplitTop(18.0f, &Button, &Left);
		static int s_ShowChatButton = 0;
		static const char *s_apShowChat[] = {Localize("Off"), Localize("On"), Localize("Always")};
		str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Show chat"), s_apShowChat[g_Config.m_ClShowChat]);
		if(DoButton_Menu(&s_ShowChatButton, aBuf, 0, &Button))
			g_Config.m_ClShowChat = (g_Config.m_ClShowChat + 1) % 3;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClShowsocial, Localize("Show social data"), g_Config.m_ClShowsocial, &Button))
			g_Config.m_ClShowsocial ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		static int s_FilterChatButton = 0;
		static const char *s_apFilterChat[] = {Localize("Everyone"), Localize("Friends only"), Localize("No one")};
		str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Chat messages"), s_apFilterChat[g_Config.m_ClFilterchat]);
		if(DoButton_Menu(&s_FilterChatButton, aBuf, 0, &Button))
		{
			g_Config.m_ClFilterchat = (g_Config.m_ClFilterchat + 1) % 3;
			if(g_Config.m_ClFilterchat != 1)
				g_Config.m_ClShowChatFriends = 0;
		}

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClShowChatTeamMembersOnly,
							 Localize("Only team chat"),
							 g_Config.m_ClShowChatTeamMembersOnly,
							 &Button))
			g_Config.m_ClShowChatTeamMembersOnly ^= 1;

		Right.HSplitTop(18.0f, &Button, &Right);
		if(DoButton_CheckBox(
			   &g_Config.m_ClShowChatSystem, Localize("System messages"), g_Config.m_ClShowChatSystem, &Button))
			g_Config.m_ClShowChatSystem ^= 1;

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(18.0f, &Button, &Right);
		if(DoButton_CheckBox(
			   &g_Config.m_ClShowKillMessages, Localize("Kill messages"), g_Config.m_ClShowKillMessages, &Button))
			g_Config.m_ClShowKillMessages ^= 1;

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(18.0f, &Button, &Right);
		if(DoButton_CheckBox(
			   &g_Config.m_ClDisableWhisper, Localize("Disable whisper"), g_Config.m_ClDisableWhisper, &Button))
			g_Config.m_ClDisableWhisper ^= 1;
	}
	else // Theme
	{
		CUIRect ThemeView = Content, ThemeFooter, ThemeLeft, ThemeRight, ThemeButton;
		ThemeView.HSplitBottom(34.0f, &ThemeView, &ThemeFooter);
		ThemeView.VSplitMid(&ThemeLeft, &ThemeRight);
		ThemeLeft.VSplitRight(6.0f, &ThemeLeft, 0);
		ThemeRight.VMargin(6.0f, &ThemeRight);

		auto DrawThemeGroup = [&](CUIRect View, const char *pTitle, int *pHue, int *pSat, int *pLht, int *pAlpha)
		{
			CUIRect Label, Row, ItemLabel, Swatch;
			View.HSplitTop(18.0f, &Label, &View);
			UI()->DoLabelScaled(&Label, pTitle, 12.0f, -1);
			View.HSplitTop(4.0f, 0, &View);

			const char *apLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht."), Localize("Alpha")};
			int *apValues[] = {pHue, pSat, pLht, pAlpha};
			for(int i = 0; i < 4; ++i)
			{
				View.HSplitTop(18.0f, &Row, &View);
				Row.VSplitLeft(72.0f, &ItemLabel, &Row);
				UI()->DoLabelScaled(&ItemLabel, apLabels[i], 11.0f, -1);
				Row.HMargin(1.0f, &Row);
				float k = (*apValues[i]) / 255.0f;
				k = DoScrollbarH(apValues[i], &Row, k);
				*apValues[i] = (int)(k * 255.0f + 0.5f);
				View.HSplitTop(4.0f, 0, &View);
			}

			View.HSplitTop(18.0f, &Row, &View);
			Row.VSplitRight(58.0f, &Row, &Swatch);
			vec3 Rgb = HslToRgb(vec3(*pHue / 255.0f, *pSat / 255.0f, *pLht / 255.0f));
			RenderTools()->DrawUIRect(&Swatch, vec4(Rgb.r, Rgb.g, Rgb.b, *pAlpha / 255.0f), CUI::CORNER_ALL, 4.0f);
			UI()->DoLabelScaled(&Row, pTitle, 11.0f, -1);
		};

		DrawThemeGroup(ThemeLeft,
					   Localize("Primary color"),
					   &g_Config.m_UiColorHue,
					   &g_Config.m_UiColorSat,
					   &g_Config.m_UiColorLht,
					   &g_Config.m_UiColorAlpha);
		DrawThemeGroup(ThemeRight,
					   Localize("Secondary color"),
					   &g_Config.m_UiColorHue2,
					   &g_Config.m_UiColorSat2,
					   &g_Config.m_UiColorLht2,
					   &g_Config.m_UiColorAlpha2);

		ThemeFooter.HSplitTop(6.0f, 0, &ThemeFooter);
		ThemeButton = ThemeFooter;
		ThemeButton.VSplitLeft(220.0f, 0, &ThemeButton);
		ThemeButton.VSplitRight(220.0f, &ThemeButton, 0);
		ThemeButton.HSplitTop(18.0f, &ThemeButton, 0);
		static int s_ResetThemeButton = 0;
		if(DoButton_Menu(&s_ResetThemeButton, Localize("Reset theme to defaults"), 0, &ThemeButton))
			ResetThemeDefaults();
	}
}

void CMenus::RenderSettingsGameplay(CUIRect MainView)
{
	char aBuf[128];
	CUIRect Label, Button, Left, Right, TabBar, Content;

	static int s_GameplaySubPage = 0;
	const char *apTabs[] = {Localize("General"), Localize("Camera"), Localize("HUD")};
	const int NumTabs = (int)(sizeof(apTabs) / sizeof(apTabs[0]));

	MainView.HSplitTop(24.0f, &TabBar, &MainView);
	MainView.HSplitTop(6.0f, 0, &MainView);
	Content = MainView;

	{
		CUIRect Tab;
		float TabW = TabBar.w / NumTabs;
		for(int i = 0; i < NumTabs; i++)
		{
			TabBar.VSplitLeft(TabW, &Tab, &TabBar);
			Tab.VMargin(2.0f, &Tab);
			if(DoButton_MenuTab(apTabs[i], apTabs[i], s_GameplaySubPage == i, &Tab, CUI::CORNER_T))
				s_GameplaySubPage = i;
		}
	}

	DrawMenuInset(&Content, CUI::CORNER_B | CUI::CORNER_TR);
	Content.Margin(8.0f, &Content);
	Content.VSplitMid(&Left, &Right);
	Left.VSplitRight(6.0f, &Left, 0);
	Right.VMargin(6.0f, &Right);

	if(s_GameplaySubPage == 0) // General: nameplates + misc
	{
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClNameplates, Localize("Show name plates"), g_Config.m_ClNameplates, &Button))
			g_Config.m_ClNameplates ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(
			   &g_Config.m_ClNameplatesAlways, Localize("Always show"), g_Config.m_ClNameplatesAlways, &Button))
			g_Config.m_ClNameplatesAlways ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(16.0f, &Label, &Left);
		Left.HSplitTop(16.0f, &Button, &Left);
		str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Size"), g_Config.m_ClNameplatesSize);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClNameplatesSize =
			(int)(DoScrollbarH(&g_Config.m_ClNameplatesSize, &Button, g_Config.m_ClNameplatesSize / 100.0f) * 100.0f +
				  0.1f);

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(
			   &g_Config.m_ClNameplatesTeamcolors, Localize("Team colors"), g_Config.m_ClNameplatesTeamcolors, &Button))
			g_Config.m_ClNameplatesTeamcolors ^= 1;

		Right.HSplitTop(18.0f, &Button, &Right);
		if(DoButton_CheckBox(
			   &g_Config.m_ClNamePlatesOwn, Localize("Own name plate"), g_Config.m_ClNamePlatesOwn, &Button))
			g_Config.m_ClNamePlatesOwn ^= 1;

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(18.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClNamePlatesFriendMark,
							 Localize("Friend marks"),
							 g_Config.m_ClNamePlatesFriendMark,
							 &Button))
			g_Config.m_ClNamePlatesFriendMark ^= 1;

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(18.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClNamePlatesClan, Localize("Show clan"), g_Config.m_ClNamePlatesClan, &Button))
			g_Config.m_ClNamePlatesClan ^= 1;

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(18.0f, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClNamePlatesIds, Localize("Show IDs"), g_Config.m_ClNamePlatesIds, &Button))
			g_Config.m_ClNamePlatesIds ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClAutoswitchWeapons,
							 Localize("Switch weapon on pickup"),
							 g_Config.m_ClAutoswitchWeapons,
							 &Button))
			g_Config.m_ClAutoswitchWeapons ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClShowNotifications,
							 Localize("Desktop notifications"),
							 g_Config.m_ClShowNotifications,
							 &Button))
			g_Config.m_ClShowNotifications ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClStreamerMode, Localize("Streamer mode"), g_Config.m_ClStreamerMode, &Button))
			g_Config.m_ClStreamerMode ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPing, Localize("AntiPing"), g_Config.m_ClAntiPing, &Button))
			g_Config.m_ClAntiPing ^= 1;

		Right.HSplitTop(16.0f, &Label, &Right);
		UI()->DoLabelScaled(&Label, Localize("Zoom with + / - keys"), 12.0f, -1);

		Right.HSplitTop(8.0f, 0, &Right);
		Right.HSplitTop(16.0f, &Label, &Right);
		Right.HSplitTop(16.0f, &Button, &Right);
		str_format(aBuf, sizeof(aBuf), "%s: %i%%", Localize("Menu opacity"), g_Config.m_ClMenuAlpha);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClMenuAlpha =
			(int)(DoScrollbarH(&g_Config.m_ClMenuAlpha, &Button, g_Config.m_ClMenuAlpha / 100.0f) * 100.0f + 0.1f);

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(18.0f, &Button, &Right);
		static int s_WideviewButton = 0;
		if(DoButton_CheckBox(&s_WideviewButton, Localize("Wide menu"), g_Config.m_UiWideview, &Button))
			g_Config.m_UiWideview ^= 1;

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(16.0f, &Label, &Right);
		Right.HSplitTop(16.0f, &Button, &Right);
		str_format(aBuf, sizeof(aBuf), "%s: %i%%", Localize("Interface scale"), g_Config.m_UiScale);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_UiScale = (int)(
			DoScrollbarH(&g_Config.m_UiScale, &Button, (g_Config.m_UiScale - 50) / 150.0f) * 150.0f + 50.0f + 0.1f);
	}
	else if(s_GameplaySubPage == 1) // Camera
	{
		Left.HSplitTop(18.0f, &Button, &Left);
		static int s_DynamicCameraButton = 0;
		if(DoButton_CheckBox(
			   &s_DynamicCameraButton, Localize("Dynamic Camera"), g_Config.m_ClMouseDeadzone != 0, &Button))
		{
			if(g_Config.m_ClMouseDeadzone)
			{
				g_Config.m_ClMouseFollowfactor = 0;
				g_Config.m_ClMouseMaxDistance = 400;
				g_Config.m_ClMouseDeadzone = 0;
			}
			else
			{
				g_Config.m_ClMouseFollowfactor = 60;
				g_Config.m_ClMouseMaxDistance = 1000;
				g_Config.m_ClMouseDeadzone = 300;
			}
		}

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(16.0f, &Label, &Left);
		Left.HSplitTop(16.0f, &Button, &Left);
		str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Deadzone"), g_Config.m_ClMouseDeadzone);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClMouseDeadzone =
			(int)(DoScrollbarH(&g_Config.m_ClMouseDeadzone, &Button, g_Config.m_ClMouseDeadzone / 3000.0f) * 3000.0f +
				  0.1f);

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(16.0f, &Label, &Left);
		Left.HSplitTop(16.0f, &Button, &Left);
		str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Follow factor"), g_Config.m_ClMouseFollowfactor);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClMouseFollowfactor =
			(int)(DoScrollbarH(&g_Config.m_ClMouseFollowfactor, &Button, g_Config.m_ClMouseFollowfactor / 200.0f) *
					  200.0f +
				  0.1f);

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(16.0f, &Label, &Left);
		Left.HSplitTop(16.0f, &Button, &Left);
		str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Max distance"), g_Config.m_ClMouseMaxDistance);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClMouseMaxDistance =
			(int)(DoScrollbarH(&g_Config.m_ClMouseMaxDistance, &Button, g_Config.m_ClMouseMaxDistance / 5000.0f) *
					  5000.0f +
				  0.1f);

		Right.HSplitTop(16.0f, &Label, &Right);
		Right.HSplitTop(16.0f, &Button, &Right);
		str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Smoothness"), g_Config.m_ClDyncamSmoothness);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClDyncamSmoothness =
			(int)(DoScrollbarH(&g_Config.m_ClDyncamSmoothness, &Button, g_Config.m_ClDyncamSmoothness / 100.0f) *
					  100.0f +
				  0.1f);

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(16.0f, &Label, &Right);
		Right.HSplitTop(16.0f, &Button, &Right);
		str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("Stabilizing"), g_Config.m_ClDyncamStabilizing);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClDyncamStabilizing =
			(int)(DoScrollbarH(&g_Config.m_ClDyncamStabilizing, &Button, g_Config.m_ClDyncamStabilizing / 100.0f) *
					  100.0f +
				  0.1f);

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(16.0f, &Label, &Right);
		Right.HSplitTop(16.0f, &Button, &Right);
		str_format(aBuf, sizeof(aBuf), "%s: %i ms", Localize("Spectate smooth"), g_Config.m_ClSmoothSpectatingTime);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClSmoothSpectatingTime =
			(int)(DoScrollbarH(
					  &g_Config.m_ClSmoothSpectatingTime, &Button, g_Config.m_ClSmoothSpectatingTime / 5000.0f) *
					  5000.0f +
				  0.1f);

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(18.0f, &Button, &Right);
		static int s_SpectatorDirector = 0;
		if(DoButton_CheckBox(&s_SpectatorDirector,
				Localize("Automatic director"),
				g_Config.m_ClSpectatorDirector,
				&Button))
			g_Config.m_ClSpectatorDirector ^= 1;

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(16.0f, &Label, &Right);
		Right.HSplitTop(16.0f, &Button, &Right);
		str_format(aBuf, sizeof(aBuf), "%s: %.1fx", Localize("Zoom"), g_Config.m_ClZoom / 10.0f);
		UI()->DoLabelScaled(&Label, aBuf, 12.0f, -1);
		Button.HMargin(1.0f, &Button);
		g_Config.m_ClZoom = (int)(DoScrollbarH(&g_Config.m_ClZoom, &Button, g_Config.m_ClZoom / 30.0f) * 30.0f + 0.1f);
		if(g_Config.m_ClZoom < 1)
			g_Config.m_ClZoom = 1;

		Right.HSplitTop(4.0f, 0, &Right);
		Right.HSplitTop(16.0f, &Label, &Right);
		UI()->DoLabelScaled(&Label, Localize("Use +/- keys to zoom (rebindable in Controls)"), 12.0f, -1);
	}
	else // HUD
	{
		const bool Advanced = g_Config.m_UiAdvancedSettings != 0;
		static int s_aHudPresetButtons[3];
		const int ActivePreset = CurrentHudPreset();
		Left.HSplitTop(16.0f, &Label, &Left);
		UI()->DoLabelScaled(&Label, Localize("HUD preset"), 12.0f, -1);
		Left.HSplitTop(28.0f, &Button, &Left);
		for(int Preset = 0; Preset < 3; Preset++)
		{
			CUIRect PresetButton;
			Button.VSplitLeft(Button.w / (3 - Preset), &PresetButton, &Button);
			PresetButton.VMargin(Preset == 0 ? 0.0f : 2.0f, &PresetButton);
			if(DoButton_Menu(&s_aHudPresetButtons[Preset],
							 Localize(HudPresetLabel(Preset)),
							 ActivePreset == Preset,
							 &PresetButton,
							 ActivePreset == Preset ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
				ApplyHudPreset(Preset);
		}
		Left.HSplitTop(8.0f, 0, &Left);

		Left.HSplitTop(18.0f, &Button, &Left);
		if(DoButton_CheckBox(&g_Config.m_ClShowhud, Localize("Show ingame HUD"), g_Config.m_ClShowhud, &Button))
			g_Config.m_ClShowhud ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		static int s_ShowhudHealthAmmo = 0;
		if(DoButton_CheckBox(&s_ShowhudHealthAmmo, Localize("Health + Ammo"), g_Config.m_ClShowhudHealthAmmo, &Button))
			g_Config.m_ClShowhudHealthAmmo ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		static int s_ShowhudScore = 0;
		if(DoButton_CheckBox(&s_ShowhudScore, Localize("Score"), g_Config.m_ClShowhudScore, &Button))
			g_Config.m_ClShowhudScore ^= 1;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		static int s_PveObjectiveDisplay = 0;
		static const char *s_apPveObjectiveDisplay[] = {"With scoreboard", "Always visible", "On objective updates"};
		str_format(aBuf,
				   sizeof(aBuf),
				   "%s: %s",
				   Localize("PvE objective display"),
				   Localize(s_apPveObjectiveDisplay[g_Config.m_ClPveObjectiveDisplay]));
		if(DoButton_Menu(&s_PveObjectiveDisplay, aBuf, 0, &Button))
			g_Config.m_ClPveObjectiveDisplay = (g_Config.m_ClPveObjectiveDisplay + 1) % 3;

		Left.HSplitTop(4.0f, 0, &Left);
		Left.HSplitTop(18.0f, &Button, &Left);
		static int s_ShowhudTimer = 0;
		if(DoButton_CheckBox(&s_ShowhudTimer, Localize("Timer"), g_Config.m_ClShowhudTimer, &Button))
			g_Config.m_ClShowhudTimer ^= 1;

		if(Advanced)
		{
			Right.HSplitTop(18.0f, &Button, &Right);
			static int s_ShowhudSpectatorCount = 0;
			if(DoButton_CheckBox(
				   &s_ShowhudSpectatorCount, Localize("Spectator count"), g_Config.m_ClShowhudSpectatorCount, &Button))
				g_Config.m_ClShowhudSpectatorCount ^= 1;

			Right.HSplitTop(4.0f, 0, &Right);
			Right.HSplitTop(18.0f, &Button, &Right);
			if(DoButton_CheckBox(&g_Config.m_ClShowfps, Localize("FPS counter"), g_Config.m_ClShowfps, &Button))
				g_Config.m_ClShowfps ^= 1;

			Right.HSplitTop(4.0f, 0, &Right);
			Right.HSplitTop(18.0f, &Button, &Right);
			static int s_ShowhudPlayerPosition = 0;
			if(DoButton_CheckBox(
				   &s_ShowhudPlayerPosition, Localize("Player position"), g_Config.m_ClShowhudPlayerPosition, &Button))
				g_Config.m_ClShowhudPlayerPosition ^= 1;

			Right.HSplitTop(4.0f, 0, &Right);
			Right.HSplitTop(18.0f, &Button, &Right);
			static int s_ShowhudPlayerSpeed = 0;
			if(DoButton_CheckBox(
				   &s_ShowhudPlayerSpeed, Localize("Player speed"), g_Config.m_ClShowhudPlayerSpeed, &Button))
				g_Config.m_ClShowhudPlayerSpeed ^= 1;

			Right.HSplitTop(4.0f, 0, &Right);
			Right.HSplitTop(18.0f, &Button, &Right);
			static int s_ShowhudPlayerAngle = 0;
			if(DoButton_CheckBox(
				   &s_ShowhudPlayerAngle, Localize("Player angle"), g_Config.m_ClShowhudPlayerAngle, &Button))
				g_Config.m_ClShowhudPlayerAngle ^= 1;

			Left.HSplitTop(4.0f, 0, &Left);
			Left.HSplitTop(18.0f, &Button, &Left);
			if(DoButton_CheckBox(
				   &g_Config.m_ClHideSelfScore, Localize("Hide player's score"), g_Config.m_ClHideSelfScore, &Button))
				g_Config.m_ClHideSelfScore ^= 1;

			Left.HSplitTop(4.0f, 0, &Left);
			Left.HSplitTop(18.0f, &Button, &Left);
			if(DoButton_CheckBox(
				   &g_Config.m_ClScoreboardUserId, Localize("Show user IDs"), g_Config.m_ClScoreboardUserId, &Button))
				g_Config.m_ClScoreboardUserId ^= 1;
		}
	}
}

void CMenus::RenderSettingsPlayer(CUIRect MainView)
{
	static int s_PlayerSubPage = 0;
	const char *apTabs[] = {Localize("Player"), Localize("Customize")};
	const int NumTabs = (int)(sizeof(apTabs) / sizeof(apTabs[0]));
	CUIRect TabBar;
	MainView.HSplitTop(24.0f, &TabBar, &MainView);
	MainView.HSplitTop(6.0f, 0, &MainView);
	{
		CUIRect Tab;
		float TabW = TabBar.w / NumTabs;
		for(int i = 0; i < NumTabs; i++)
		{
			TabBar.VSplitLeft(TabW, &Tab, &TabBar);
			Tab.VMargin(2.0f, &Tab);
			if(DoButton_MenuTab(apTabs[i], apTabs[i], s_PlayerSubPage == i, &Tab, CUI::CORNER_T))
				s_PlayerSubPage = i;
		}
	}
	if(s_PlayerSubPage == 1)
	{
		RenderCustomization(MainView);
		return;
	}

	CUIRect Button, Label;
	MainView.HSplitTop(10.0f, 0, &MainView);

	// player name
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(150.0f, &Button, 0);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Name"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0, -1);
	static float s_OffsetName = 0.0f;
	if(DoEditBox(
		   g_Config.m_PlayerName, &Button, g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), 14.0f, &s_OffsetName))
		m_NeedSendinfo = true;

	// player clan
	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(80.0f, &Label, &Button);
	Button.VSplitLeft(150.0f, &Button, 0);
	str_format(aBuf, sizeof(aBuf), "%s:", Localize("Clan"));
	UI()->DoLabelScaled(&Label, aBuf, 14.0, -1);
	static float s_OffsetClan = 0.0f;
	if(DoEditBox(
		   g_Config.m_PlayerClan, &Button, g_Config.m_PlayerClan, sizeof(g_Config.m_PlayerClan), 14.0f, &s_OffsetClan))
		m_NeedSendinfo = true;

	// country flag selector
	MainView.HSplitTop(20.0f, 0, &MainView);
	static float s_ScrollValue = 0.0f;
	int OldSelected = -1;
	UiDoListboxStart(&s_ScrollValue,
					 &MainView,
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
		if(pEntry->m_CountryCode == g_Config.m_PlayerCountry)
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
			if(pEntry->m_Texture != -1)
				UI()->DoLabel(&Label, pEntry->m_aCountryCodeString, 10.0f, 0);
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		g_Config.m_PlayerCountry = m_pClient->m_pCountryFlags->GetByIndex(NewSelected)->m_CountryCode;
		m_NeedSendinfo = true;
	}
}

void CMenus::RenderCustomization(CUIRect MainView)
{

	// back to menu button
	/*
	CUIRect BackButton;
	MainView.HSplitTop(30, &BackButton, &MainView);

	BackButton.VSplitLeft(300, 0, &BackButton);
	BackButton.VSplitRight(300, &BackButton, 0);

	static int s_FrontPageButton=0;
	if(DoButton_Menu(&s_FrontPageButton, Localize("Back to main menu"), 0, &BackButton))
		g_Config.m_UiPage = PAGE_INTERNET;

	*/
	MainView.HSplitTop(20.0f, 0, &MainView);

	CUIRect LeftView;
	MainView.VSplitMid(&LeftView, &MainView);
	MainView.VSplitRight(12.0f, &MainView, 0);
	LeftView.VSplitLeft(12.0f, 0, &LeftView);
	LeftView.VSplitRight(8.0f, &LeftView, 0);
	MainView.VSplitLeft(8.0f, 0, &MainView);

	// color select
	static int s_CustomizationColor = 0;
	const char *aColor[] = {Localize("Body"), Localize("Feet"), Localize("Skin"), Localize("Hair / hat")};
	int NumColors = (int)(sizeof(aColor) / sizeof(*aColor));

	CUIRect ColorRect, L;
	LeftView.HSplitTop(18.0f, &L, &LeftView);
	UI()->DoLabelScaled(&L, Localize("Change color of"), 14.0f, -1);

	LeftView.HSplitTop(4.0f, 0, &LeftView);
	LeftView.HSplitTop(22.0f, &ColorRect, &LeftView);

	{
		const float TabW = ColorRect.w / NumColors;
		for(int i = 0; i < NumColors; i++)
		{
			CUIRect Button;
			ColorRect.VSplitLeft(TabW, &Button, &ColorRect);
			Button.VMargin(1.0f, &Button);
			if(DoButton_MenuTab(&aColor[i], aColor[i], s_CustomizationColor == i, &Button, CUI::CORNER_ALL))
				s_CustomizationColor = i;
		}
	}

	CUIRect Slider;
	CUIRect Button, Label;

	LeftView.HSplitTop(8.0f, 0, &LeftView);
	LeftView.HSplitTop(70.0f, &Slider, &LeftView);

	int *pColors;
	if(s_CustomizationColor == 0)
		pColors = &g_Config.m_PlayerColorBody;
	else if(s_CustomizationColor == 1)
		pColors = &g_Config.m_PlayerColorFeet;
	else if(s_CustomizationColor == 2)
		pColors = &g_Config.m_PlayerColorSkin;
	else if(s_CustomizationColor == 3)
		pColors = &g_Config.m_PlayerColorTopper;
	else
		pColors = &g_Config.m_PlayerColorBody;

	const char *paLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht.")};
	static int s_aColorSlider[3] = {0};

	int PrevColor = *pColors;

	// color sliders
	int Color = 0;
	for(int s = 0; s < 3; s++)
	{
		Slider.HSplitTop(22.0f, &Label, &Slider);
		Label.VSplitLeft(56.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);

		float k = ((PrevColor >> ((2 - s) * 8)) & 0xff) / 255.0f;
		k = DoScrollbarH(&s_aColorSlider[s], &Button, k);
		Color <<= 8;
		Color += clamp((int)(k * 255), 0, 255);
		UI()->DoLabelScaled(&Label, paLabels[s], 12.0f, -1);
	}

	if(PrevColor != Color)
		m_NeedSendinfo = true;

	*pColors = Color;

	LeftView.HSplitTop(8.0f, 0, &LeftView);

	// blood color select
	const char *aBlood[] = {Localize("Red"), Localize("Green"), Localize("Black")};
	int NumBloods = (int)(sizeof(aBlood) / sizeof(*aBlood));

	CUIRect BloodRect, B;
	LeftView.HSplitTop(18.0f, &B, &LeftView);
	UI()->DoLabelScaled(&B, Localize("Blood color"), 14.0f, -1);

	LeftView.HSplitTop(4.0f, 0, &LeftView);
	LeftView.HSplitTop(22.0f, &BloodRect, &LeftView);

	{
		const float TabW = BloodRect.w / NumBloods;
		for(int i = 0; i < NumBloods; i++)
		{
			CUIRect Button;
			BloodRect.VSplitLeft(TabW, &Button, &BloodRect);
			Button.VMargin(1.0f, &Button);
			if(DoButton_MenuTab(&aBlood[i], aBlood[i], g_Config.m_PlayerBloodColor == i, &Button, CUI::CORNER_ALL))
			{
				g_Config.m_PlayerBloodColor = i;
			}
		}
	}

	// dedicated preview area — keep clear of tabs/sliders/skin list
	LeftView.HSplitTop(14.0f, 0, &LeftView);
	CUIRect Preview;
	LeftView.HSplitTop(160.0f, &Preview, &LeftView);
	DrawMenuInset(&Preview, CUI::CORNER_ALL);
	{
		CTeeRenderInfo Info;
		Info.m_ColorBody = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorBody);
		Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorFeet);
		Info.m_Body = 0; // g_Config.m_PlayerBody;
		Info.m_TopperTexture =
			m_pClient->m_pSkins->GetTopper(m_pClient->m_pSkins->FindTopper(g_Config.m_PlayerTopper))->m_Texture;
		Info.m_EyeTexture = m_pClient->m_pSkins->GetEye(m_pClient->m_pSkins->FindEye(g_Config.m_PlayerEye))->m_Texture;
		Info.m_HeadTexture =
			m_pClient->m_pSkins->GetHead(m_pClient->m_pSkins->FindHead(g_Config.m_PlayerHead))->m_Texture;
		Info.m_BodyTexture =
			m_pClient->m_pSkins->GetBody(m_pClient->m_pSkins->FindBody(g_Config.m_PlayerBody))->m_Texture;
		Info.m_HandTexture =
			m_pClient->m_pSkins->GetHand(m_pClient->m_pSkins->FindHand(g_Config.m_PlayerHand))->m_Texture;
		Info.m_FootTexture =
			m_pClient->m_pSkins->GetFoot(m_pClient->m_pSkins->FindFoot(g_Config.m_PlayerFoot))->m_Texture;
		Info.m_ColorTopper = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorTopper);
		Info.m_ColorSkin = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
		Info.m_Size = UI()->Scale() * 56.0f;

		RenderTools()->RenderStaticPlayer(&Info, vec2(Preview.x + Preview.w * 0.5f, Preview.y + Preview.h * 0.58f));
	}

	// skin type select
	static int s_SkinType = 0;
	const char *aSkinType[] = {Localize("Head"),
							   Localize("Hair / hat"),
							   Localize("Eyes"),
							   Localize("Body"),
							   Localize("Hands"),
							   Localize("Feet")};
	int NumSkinTypes = (int)(sizeof(aSkinType) / sizeof(*aSkinType));

	CUIRect SkinTypeLabel, SkinSelect;
	MainView.HSplitTop(18.0f, &SkinTypeLabel, &MainView);
	UI()->DoLabelScaled(&SkinTypeLabel, Localize("Change skin of"), 14.0f, -1);

	// saving skins, helper for creating bot skins
	if(Input()->KeyDown(KEY_S) && (Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL)))
	{
		SaveSkin();
	}

	MainView.HSplitTop(4.0f, 0, &MainView);
	MainView.HSplitTop(48.0f, &SkinSelect, &MainView);

	{
		CUIRect Row1, Row2;
		SkinSelect.HSplitMid(&Row1, &Row2);
		Row1.HSplitBottom(2.0f, &Row1, 0);
		Row2.HSplitTop(2.0f, 0, &Row2);
		const int PerRow = 3;
		for(int row = 0; row < 2; row++)
		{
			CUIRect Row = row == 0 ? Row1 : Row2;
			const float TabW = Row.w / PerRow;
			for(int col = 0; col < PerRow; col++)
			{
				const int i = row * PerRow + col;
				if(i >= NumSkinTypes)
					break;
				CUIRect Button;
				Row.VSplitLeft(TabW, &Button, &Row);
				Button.VMargin(1.0f, &Button);
				if(DoButton_MenuTab(&aSkinType[i], aSkinType[i], s_SkinType == i, &Button, CUI::CORNER_ALL))
					s_SkinType = i;
			}
		}
	}

	MainView.HSplitTop(6.0f, 0, &MainView);

	// eye selector
	if(s_SkinType == 2)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumEyes(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetEye(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Eyes"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerEye) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_EyeTexture = s->m_Texture;
				Info.m_Size = UI()->Scale() * 50.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderEye(&Info,
										 vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerEye, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerEye));
			m_NeedSendinfo = true;
		}
	}

	// topper selector
	if(s_SkinType == 1)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumToppers(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetTopper(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Hair / hat"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerTopper) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_TopperTexture = s->m_Texture;
				Info.m_ColorTopper = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorTopper);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderTopper(&Info,
											vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerTopper, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerTopper));
			m_NeedSendinfo = true;
		}
	}

	// head selector
	if(s_SkinType == 0)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumHeads(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetHead(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Head"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerHead) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_HeadTexture = s->m_Texture;
				// Info.m_ColorHead = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderHead(&Info,
										  vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerHead, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerHead));
			m_NeedSendinfo = true;
		}
	}

	// hand selector
	if(s_SkinType == 4)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumHands(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetHand(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Hands"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerHand) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_HandTexture = s->m_Texture;
				// Info.m_ColorHead = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderHand(&Info,
										  vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerHand, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerHand));
			m_NeedSendinfo = true;
		}
	}

	// foot selector
	if(s_SkinType == 5)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumFeet(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetFoot(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Feet"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerFoot) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_FootTexture = s->m_Texture;
				// Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderFoot(&Info,
										  vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerFoot, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerFoot));
			m_NeedSendinfo = true;
		}
	}

	// body selector
	if(s_SkinType == 3)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumBodies(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetBody(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Body"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerBody) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_BodyTexture = s->m_Texture;
				// Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderBody(&Info,
										  vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerBody, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerBody));
			m_NeedSendinfo = true;
		}
	}

	return;

	/*

	MainView.HSplitTop(10.0f, 0, &MainView);


	// color select
	static int s_CustomizationColor = 0;
	const char *aColor[] = {Localize("Body"), Localize("Feet"), Localize("Skin"), Localize("Hair / hat")};
	int NumColors = (int)(sizeof(aColor)/sizeof(*aColor));

	CUIRect ColorRect, L;
	MainView.HSplitTop(0.0f, &L, 0);

	UI()->DoLabelScaled(&L, Localize("Change color of"), 14.0f, -1);

	MainView.HSplitTop(20.0f, 0, &ColorRect);
	ColorRect.HSplitTop(20.0f, &ColorRect, 0);

	for(int i = 0; i < NumColors; i++)
	{
		CUIRect Button;
		ColorRect.VSplitLeft(80.0f, &Button, &ColorRect);
		if(DoButton_MenuTab(&aColor[i], aColor[i], s_CustomizationColor == i, &Button, CUI::CORNER_BR))
			s_CustomizationColor = i;
	}


	CUIRect Slider;
	CUIRect Button, Label;


	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(82.5f, &Label, &MainView);


	int *pColors;
	if (s_CustomizationColor == 0)
		pColors = &g_Config.m_PlayerColorBody;
	else if (s_CustomizationColor == 1)
		pColors = &g_Config.m_PlayerColorFeet;
	else if (s_CustomizationColor == 2)
		pColors = &g_Config.m_PlayerColorSkin;
	else if (s_CustomizationColor == 3)
		pColors = &g_Config.m_PlayerColorTopper;
	else
		pColors = &g_Config.m_PlayerColorBody;

	const char *paLabels[] = {
		Localize("Hue"),
		Localize("Sat."),
		Localize("Lht.")};
	static int s_aColorSlider[3] = {0};

	MainView.HSplitTop(20.0f, 0, &Slider);
	MainView.VSplitMid(&Slider, 0);

	int PrevColor = *pColors;

	// color sliders
	int Color = 0;
	for(int s = 0; s < 3; s++)
	{
		Slider.HSplitTop(20.0f, &Label, &Slider);
		Label.VSplitLeft(100.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);

		float k = ((PrevColor>>((2-s)*8))&0xff) / 255.0f;
		k = DoScrollbarH(&s_aColorSlider[s], &Button, k);
		Color <<= 8;
		Color += clamp((int)(k*255), 0, 255);
		UI()->DoLabelScaled(&Label, paLabels[s], 14.0f, -1);
	}

	if(PrevColor != Color)
		m_NeedSendinfo = true;

	*pColors = Color;


	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(82.5f, &Label, &MainView);


	// render player
	{
		CTeeRenderInfo Info;
		Info.m_ColorBody = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorBody);
		Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorFeet);
		Info.m_TopperTexture =
	m_pClient->m_pSkins->GetTopper(m_pClient->m_pSkins->FindTopper(g_Config.m_PlayerTopper))->m_Texture;
		Info.m_EyeTexture = m_pClient->m_pSkins->GetEye(m_pClient->m_pSkins->FindEye(g_Config.m_PlayerEye))->m_Texture;
		Info.m_HeadTexture =
	m_pClient->m_pSkins->GetHead(m_pClient->m_pSkins->FindHead(g_Config.m_PlayerHead))->m_Texture; Info.m_BodyTexture =
	m_pClient->m_pSkins->GetBody(m_pClient->m_pSkins->FindBody(g_Config.m_PlayerBody))->m_Texture; Info.m_HandTexture =
	m_pClient->m_pSkins->GetHand(m_pClient->m_pSkins->FindHand(g_Config.m_PlayerHand))->m_Texture; Info.m_FootTexture =
	m_pClient->m_pSkins->GetFoot(m_pClient->m_pSkins->FindFoot(g_Config.m_PlayerFoot))->m_Texture; Info.m_ColorTopper =
	m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorTopper); Info.m_ColorSkin =
	m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin); Info.m_Size = UI()->Scale()*50.0f;

		RenderTools()->RenderStaticPlayer(&Info, vec2(600, 200));
	}


	// skin type select
	static int s_SkinType = 0;
	const char *aSkinType[] = {Localize("Head"), Localize("Hair / hat"), Localize("Eyes"), Localize("Body"),
	Localize("Hands"), Localize("Feet")}; int NumSkinTypes = (int)(sizeof(aSkinType)/sizeof(*aSkinType));


	CUIRect SkinTypeLabel, SkinSelect;
	MainView.HSplitTop(0.0f, &SkinTypeLabel, 0);

	UI()->DoLabelScaled(&SkinTypeLabel, Localize("Change skin of"), 14.0f, -1);

	// saving skins, helper for creating bot skins
	if(Input()->KeyDown(KEY_S) && (Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL)))
	{
		SaveSkin();
	}

	MainView.HSplitTop(20.0f, 0, &SkinSelect);
	SkinSelect.HSplitTop(20.0f, &SkinSelect, 0);

	for(int i = 0; i < NumSkinTypes; i++)
	{
		CUIRect Button;
		SkinSelect.VSplitLeft(80.0f, &Button, &SkinSelect);
		if(DoButton_MenuTab(&aSkinType[i], aSkinType[i], s_SkinType == i, &Button, CUI::CORNER_BR))
			s_SkinType = i;
	}


	MainView.HSplitTop(5.0f, 0, &MainView);
	MainView.HSplitTop(20.5f, &Label, &MainView);

	// eye selector
	if (s_SkinType == 1)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumEyes(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetEye(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist, &MainView, 50.0f, Localize("Eyes"), "", s_paSkinList.size(), 4, OldSelected,
	s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerEye) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_EyeTexture = s->m_Texture;
				Info.m_Size = UI()->Scale()*50.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderEye(&Info, vec2(Item.m_Rect.x+Item.m_Rect.w/2, Item.m_Rect.y+Item.m_Rect.h/2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerEye, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerEye));
			m_NeedSendinfo = true;
		}
	}

	// topper selector
	if (s_SkinType == 2)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumToppers(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetTopper(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist, &MainView, 50.0f, Localize("Hair / hat"), "", s_paSkinList.size(), 4,
	OldSelected, s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerTopper) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_TopperTexture = s->m_Texture;
				Info.m_ColorTopper = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorTopper);
				Info.m_Size = UI()->Scale()*80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderTopper(&Info, vec2(Item.m_Rect.x+Item.m_Rect.w/2, Item.m_Rect.y+Item.m_Rect.h/2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerTopper, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerTopper));
			m_NeedSendinfo = true;
		}
	}

		// Head selector
	if (s_SkinType == 0)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumHeads(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetHead(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist, &MainView, 50.0f, Localize("Heads"), "", s_paSkinList.size(), 4, OldSelected,
	s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerHead) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_HeadTexture = s->m_Texture;
				Info.m_Size = UI()->Scale()*50.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderHead(&Info, vec2(Item.m_Rect.x+Item.m_Rect.w/2, Item.m_Rect.y+Item.m_Rect.h/2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerHead, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerHead));
			m_NeedSendinfo = true;
		}
	}

		// Body selector
	if (s_SkinType == 3)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumBodies(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetBody(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist, &MainView, 50.0f, Localize("Bodies"), "", s_paSkinList.size(), 4, OldSelected,
	s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerBody) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_BodyTexture = s->m_Texture;
				Info.m_Size = UI()->Scale()*50.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderBody(&Info, vec2(Item.m_Rect.x+Item.m_Rect.w/2, Item.m_Rect.y+Item.m_Rect.h/2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerBody, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerBody));
			m_NeedSendinfo = true;
		}
	}

		// Hand selector
	if (s_SkinType == 4)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumHands(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetHand(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist, &MainView, 50.0f, Localize("Hands"), "", s_paSkinList.size(), 4, OldSelected,
	s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerHand) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_HandTexture = s->m_Texture;
				Info.m_Size = UI()->Scale()*50.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderHand(&Info, vec2(Item.m_Rect.x+Item.m_Rect.w/2, Item.m_Rect.y+Item.m_Rect.h/2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerHand, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerHand));
			m_NeedSendinfo = true;
		}
	}

		// Foot selector
	if (s_SkinType == 5)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumFeet(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetFoot(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist, &MainView, 50.0f, Localize("Feet"), "", s_paSkinList.size(), 4, OldSelected,
	s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerFoot) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_FootTexture = s->m_Texture;
				Info.m_Size = UI()->Scale()*50.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderFoot(&Info, vec2(Item.m_Rect.x+Item.m_Rect.w/2, Item.m_Rect.y+Item.m_Rect.h/2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerFoot, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerFoot));
			m_NeedSendinfo = true;
		}
	}

	// body selector
	if (s_SkinType == 0)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static float s_ScrollValue = 0.0f;
		static bool s_InitSkinlist = false;

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist, &MainView, 50.0f, Localize("Body"), "", NUM_BODIES, 4, OldSelected,
	s_ScrollValue);

		const int s[NUM_BODIES] = {0, 1, 2, 3, 4, 5};

		for (int i = 0; i < NUM_BODIES; i++)
		{
			//if (i == 0)
			//	continue;

			if (i == g_Config.m_PlayerBody)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s[i], OldSelected == i);
			if (Item.m_Visible)
			{
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_BODIES].m_Id);
				Graphics()->QuadsBegin();
				Graphics()->QuadsSetRotation(0);
				Graphics()->SetColor(1, 1, 1, 1);

				RenderTools()->SelectSprite(SPRITE_BODY1+i);
				IGraphics::CQuadItem QuadItem(Item.m_Rect.x+Item.m_Rect.w/2, Item.m_Rect.y+Item.m_Rect.h/2,
	UI()->Scale()*40.0f, UI()->Scale()*40.0f); Graphics()->QuadsDraw(&QuadItem, 1);

				Graphics()->QuadsEnd();
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			//mem_copy(g_Config.m_PlayerTopper, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerTopper));
			g_Config.m_PlayerBody = NewSelected;
			m_NeedSendinfo = true;

			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "new selected: '%d'", NewSelected);
			Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "skin", aBuf);
		}
	}
	*/

	// skin selector
	/*
	MainView.HSplitTop(20.0f, 0, &MainView);
	static bool s_InitSkinlist = true;
	static sorted_array<const CSkins::CSkin *> s_paSkinList;
	static float s_ScrollValue = 0.0f;
	if(s_InitSkinlist)
	{
		s_paSkinList.clear();
		for(int i = 0; i < m_pClient->m_pSkins->Num(); ++i)
		{
			const CSkins::CSkin *s = m_pClient->m_pSkins->Get(i);
			// no special skins
			if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
				continue;
			s_paSkinList.add(s);
		}
		s_InitSkinlist = false;
	}

	//MainView.HSplitTop(82.5f, &Label, &MainView);

	int OldSelected = -1;
	UiDoListboxStart(&s_InitSkinlist, &MainView, 50.0f, Localize("Skins"), "", s_paSkinList.size(), 4, OldSelected,
	s_ScrollValue);

	for(int i = 0; i < s_paSkinList.size(); ++i)
	{
		const CSkins::CSkin *s = s_paSkinList[i];
		if(s == 0)
			continue;

		if(str_comp(s->m_aName, g_Config.m_PlayerSkin) == 0)
			OldSelected = i;

		CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
		if(Item.m_Visible)
		{
			CTeeRenderInfo Info;
			if(g_Config.m_PlayerUseCustomColor)
			{
				Info.m_Texture = s->m_ColorTexture;
				Info.m_ColorBody = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorBody);
				Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorFeet);
			}
			else
			{
				Info.m_Texture = s->m_OrgTexture;
				Info.m_ColorBody = vec4(1.0f, 1.0f, 1.0f, 1.0f);
				Info.m_ColorFeet = vec4(1.0f, 1.0f, 1.0f, 1.0f);
			}

			Info.m_Size = UI()->Scale()*50.0f;
			Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top
			//RenderTools()->RenderTee(CAnimState::GetIdle(), &Info, 0, vec2(1.0f, 0.0f),
	vec2(Item.m_Rect.x+Item.m_Rect.w/2, Item.m_Rect.y+Item.m_Rect.h/2));

			if(g_Config.m_Debug)
			{
				vec3 BloodColor = g_Config.m_PlayerUseCustomColor ?
	m_pClient->m_pSkins->GetColorV3(g_Config.m_PlayerColorBody) : s->m_BloodColor; Graphics()->TextureSet(-1);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(BloodColor.r, BloodColor.g, BloodColor.b, 1.0f);
				IGraphics::CQuadItem QuadItem(Item.m_Rect.x, Item.m_Rect.y, 12.0f, 12.0f);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
			}
		}
	}

	const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
	if(OldSelected != NewSelected)
	{
		mem_copy(g_Config.m_PlayerSkin, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerSkin));
		m_NeedSendinfo = true;
	}
	*/

}

void CMenus::SaveSkin()
{
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "skin", "Saving skin");

	char aPath[512];
	// str_format(aPath, sizeof(aPath), "%s%s", "server/pvp_bots", "test.npc");
	str_format(aPath, sizeof(aPath), "%s%s%s", "data/server/pvp_bots/", g_Config.m_PlayerName, ".npc");

	/*
	# example npc profile
	name: Tigrrr
	head: default
	body: simple
	hand: default
	foot: default
	topper: tigger
	eye: sleepy

	# write /color in in-game chat to see your current colors
	skin-color: 2293760
	topper-color: 1835007
	body-color: 2345472
	foot-color: 13238016

	# red, black or green
	blood-color: red
	*/

	IOHANDLE file = io_open(aPath, IOFLAG_WRITE);

	char buf[1024];
	str_format(buf, sizeof(buf), "name: %s", g_Config.m_PlayerName);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);

	if(g_Config.m_PlayerBloodColor == 0)
	{
		str_format(buf, sizeof(buf), "blood-color: %s", "red");
		io_write(file, buf, strlen(buf));
		io_write_newline(file);
		io_write_newline(file);
	}
	if(g_Config.m_PlayerBloodColor == 1)
	{
		str_format(buf, sizeof(buf), "blood-color: %s", "green");
		io_write(file, buf, strlen(buf));
		io_write_newline(file);
		io_write_newline(file);
	}
	if(g_Config.m_PlayerBloodColor == 2)
	{
		str_format(buf, sizeof(buf), "blood-color: %s", "black");
		io_write(file, buf, strlen(buf));
		io_write_newline(file);
		io_write_newline(file);
	}

	str_format(buf, sizeof(buf), "head: %s", g_Config.m_PlayerHead);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	str_format(buf, sizeof(buf), "body: %s", g_Config.m_PlayerBody);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	str_format(buf, sizeof(buf), "hand: %s", g_Config.m_PlayerHand);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	str_format(buf, sizeof(buf), "foot: %s", g_Config.m_PlayerFoot);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	str_format(buf, sizeof(buf), "topper: %s", g_Config.m_PlayerTopper);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	str_format(buf, sizeof(buf), "eye: %s", g_Config.m_PlayerEye);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	io_write_newline(file);

	str_format(buf, sizeof(buf), "skin-color: %u", g_Config.m_PlayerColorSkin);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	str_format(buf, sizeof(buf), "topper-color: %u", g_Config.m_PlayerColorTopper);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	str_format(buf, sizeof(buf), "body-color: %u", g_Config.m_PlayerColorBody);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);
	str_format(buf, sizeof(buf), "foot-color: %u", g_Config.m_PlayerColorFeet);
	io_write(file, buf, strlen(buf));
	io_write_newline(file);

	io_close(file);

	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "skin", "Skin saved");
}

typedef void (*pfnAssignFuncCallback)(CConfiguration *pConfig, int Value);

typedef struct
{
	CLocConstString m_Name;
	const char *m_pCommand;
	int m_KeyId;
	int m_SecondaryKeyId;
} CKeyInfo;

enum ControlSettings
{
	START_MOVEMENT = 0,
	START_WEAPONS = 6,
	START_VOTING = 16,
	START_CHAT = 18,
	START_MISC = 21,
	END_CONTROL = 34,
};

static CKeyInfo gs_aKeys[] = {
	// movement
	{"Move left", "+left", 0}, // Localize - these strings are localized within CLocConstString
	{"Move right", "+right", 0},
	{"Slide / down", "+down", 0},
	{"Jump", "+jump", 0},
	{"Fire", "+fire", 0},
	//{ "Charge", "+charge", 0 },
	{"Hook", "+turbo", 0},
	//{ "Build", "+build", 0 },

	// weapons
	//{ "Tool", "+weapon1", 0 },
	{"Item slot 1", "+weapon2", 0},
	{"Item slot 2", "+weapon3", 0},
	{"Item slot 3", "+weapon4", 0},
	{"Item slot 4", "+weapon5", 0},

	{"Inventory", "+inventory", 0},
	{"Build menu", "+buildmenu", 0},

	//{ "Fast weapon change", "+lastweapon", 0 },
	//{ "Switch weapon", "+switch", 0 },
	{"Next weapon", "+nextweapon", 0},
	{"Prev. weapon", "+prevweapon", 0},
	{"Drop weapon", "+dropweapon", 0},
	{"Inventory roll", "+inventoryroll", 0},

	// voting
	{"Vote yes", "vote yes", 0},
	{"Vote no", "vote no", 0},

	// chat
	{"Chat", "chat all", 0},
	{"Team chat", "chat team", 0},
	{"Whisper", "chat whisper", 0},
	{"Show chat", "+show_chat", 0},

	// misc
	//{ "Item picker", "+itempicker", 0 },
	{"Emoticon", "+emote", 0},
	{"Drone wheel", "+dronewheel", 0},
	{"Spectator mode", "+spectate", 0},
	{"Spectate next", "spectate_next", 0},
	{"Spectate previous", "spectate_previous", 0},
	{"Console", "toggle_local_console", 0},
	{"Remote console", "toggle_remote_console", 0},
	{"Screenshot", "screenshot", 0},
	{"Scoreboard", "+scoreboard", 0},
	{"Ready", "ready_change", 0},
	{"Zoom in", "zoom+", 0},
	{"Zoom out", "zoom-", 0},
};

/*	This is for scripts/update_localization.py to work, don't remove!
	Localize("Move left");Localize("Move right");Localize("Jump");Localize("Fire");Localize("Hook");Localize("Hammer");
	Localize("Pistol");Localize("Shotgun");Localize("Grenade");Localize("Rifle");Localize("Next weapon");Localize("Prev.
   weapon"); Localize("Vote yes");Localize("Vote no");Localize("Chat");Localize("Team
   chat");Localize("Whisper");Localize("Show chat");Localize("Emoticon"); Localize("Spectator mode");Localize("Spectate
   next");Localize("Spectate previous");Localize("Console");Localize("Remote console");
	Localize("Screenshot");Localize("Scoreboard");Localize("Ready");Localize("Zoom in");Localize("Zoom
   out");Localize("Respawn");Localize("Slide / down");Localize("Drop weapon"); Localize("Item slot 1");Localize("Item
   slot 2");Localize("Item slot 3");Localize("Item slot 4");Localize("Inventory");Localize("Inventory roll");
	Localize("Build menu");Localize("Drone wheel");
*/

const int g_KeyCount = sizeof(gs_aKeys) / sizeof(CKeyInfo);

void CMenus::UiDoGetButtons(int Start, int Stop, CUIRect View)
{
	for(int i = Start; i < Stop; i++)
	{
		CKeyInfo &Key = gs_aKeys[i];
		CUIRect Button, Label, Primary, Secondary;
		View.HSplitTop(20.0f, &Button, &View);
		Button.VSplitLeft(135.0f, &Label, &Button);
		Button.VSplitMid(&Primary, &Secondary);
		Primary.w -= 2.0f;
		Secondary.x += 2.0f;
		Secondary.w -= 2.0f;

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s:", (const char *)Key.m_Name);

		UI()->DoLabelScaled(&Label, aBuf, 13.0f, -1);
		auto ApplyBinding = [&](int OldId, int NewId) {
			if(NewId == OldId)
				return;
			char aDisplaced[128];
			aDisplaced[0] = 0;
			if(NewId)
				str_copy(aDisplaced, m_pClient->m_pBinds->Get(NewId), sizeof(aDisplaced));
			if(OldId)
				m_pClient->m_pBinds->Bind(OldId, "");
			if(NewId)
			{
				if(aDisplaced[0] && str_comp(aDisplaced, Key.m_pCommand) != 0 && OldId)
					m_pClient->m_pBinds->Bind(OldId, aDisplaced);
				m_pClient->m_pBinds->Bind(NewId, Key.m_pCommand);
			}
		};
		const int NewPrimary = DoKeyReader((void *)&Key.m_KeyId, &Primary, Key.m_KeyId);
		if(NewPrimary != Key.m_KeyId)
			ApplyBinding(Key.m_KeyId, NewPrimary);
		const int NewSecondary = DoKeyReader((void *)&Key.m_SecondaryKeyId, &Secondary, Key.m_SecondaryKeyId);
		if(NewSecondary != Key.m_SecondaryKeyId)
		{
			ApplyBinding(Key.m_SecondaryKeyId, NewSecondary);
		}
		View.HSplitTop(5.0f, 0, &View);
	}
}

float CMenus::RenderSettingsControlsMovement(CUIRect View)
{
	CUIRect Button, Label;
	char aBuf[64];

	View.HSplitTop(20.0f, &Button, &View);
	Button.VSplitLeft(200.0f, &Label, &Button);
	str_format(aBuf, sizeof(aBuf), "%s: %i", Localize("In-game mouse sens."), g_Config.m_InpMousesens);
	UI()->DoLabel(&Label, aBuf, 14.0f * UI()->Scale(), -1);
	Button.HMargin(2.0f, &Button);
	if(!(m_pUiClipScrollRegion && m_pUiClipScrollRegion->IsRectClipped(Button)))
		g_Config.m_InpMousesens =
			(int)(DoScrollbarH(&g_Config.m_InpMousesens, &Button, (g_Config.m_InpMousesens - 1) / 500.0f) * 500.0f) + 1;

	View.HSplitTop(10.0f, 0, &View);
	UiDoGetButtons(START_MOVEMENT, START_WEAPONS, View);
	return 30.0f + (START_WEAPONS - START_MOVEMENT) * 25.0f;
}

float CMenus::RenderSettingsControlsWeapons(CUIRect View)
{
	UiDoGetButtons(START_WEAPONS, START_VOTING, View);
	return (START_VOTING - START_WEAPONS) * 25.0f;
}

float CMenus::RenderSettingsControlsVoting(CUIRect View)
{
	UiDoGetButtons(START_VOTING, START_CHAT, View);
	return (START_CHAT - START_VOTING) * 25.0f;
}

float CMenus::RenderSettingsControlsChat(CUIRect View)
{
	UiDoGetButtons(START_CHAT, START_MISC, View);
	return (START_MISC - START_CHAT) * 25.0f;
}

float CMenus::RenderSettingsControlsMisc(CUIRect View)
{
	UiDoGetButtons(START_MISC, END_CONTROL, View);
	return (END_CONTROL - START_MISC) * 25.0f;
}

void CMenus::RenderSettingsControls(CUIRect MainView)
{
	static int s_aInputTabIds[2];
	CUIRect InputTabs, KeyboardTab, GamepadTab;
	MainView.HSplitTop(26.0f, &InputTabs, &MainView);
	InputTabs.VSplitMid(&KeyboardTab, &GamepadTab);
	KeyboardTab.VMargin(2.0f, &KeyboardTab);
	GamepadTab.VMargin(2.0f, &GamepadTab);
	if(DoButton_MenuTab(&s_aInputTabIds[0], Localize("Keyboard & mouse"), g_Config.m_UiInputPage == 0, &KeyboardTab, CUI::CORNER_ALL))
		g_Config.m_UiInputPage = 0;
	if(DoButton_MenuTab(&s_aInputTabIds[1], Localize("Controller & advanced"), g_Config.m_UiInputPage == 1, &GamepadTab, CUI::CORNER_ALL))
		g_Config.m_UiInputPage = 1;
	MainView.HSplitTop(8.0f, 0, &MainView);
	if(g_Config.m_UiInputPage == 1)
	{
		RenderSettingsGamepad(MainView);
		return;
	}
	// this is kinda slow, but whatever
	for(int i = 0; i < g_KeyCount; i++)
	{
		gs_aKeys[i].m_KeyId = 0;
		gs_aKeys[i].m_SecondaryKeyId = 0;
	}

	for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
	{
		const char *pBind = m_pClient->m_pBinds->Get(KeyId);
		if(!pBind[0])
			continue;

		for(int i = 0; i < g_KeyCount; i++)
			if(str_comp(pBind, gs_aKeys[i].m_pCommand) == 0)
			{
				if(!gs_aKeys[i].m_KeyId)
					gs_aKeys[i].m_KeyId = KeyId;
				else if(!gs_aKeys[i].m_SecondaryKeyId)
					gs_aKeys[i].m_SecondaryKeyId = KeyId;
				break;
			}
	}

	CUIRect BottomView, ResetButton;
	MainView.HSplitBottom(50.0f, &MainView, &BottomView);

	const float HeaderHeight = 20.0f;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ConfigureScrollRegion(&ScrollParams);
	ScrollParams.m_ClipBgColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
	ScrollParams.m_ScrollUnit = 60.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);
	MainView.y += ScrollOffset.y;
	m_pUiClipScrollRegion = &s_ScrollRegion;

	CUIRect LastExpandRect;
	static bool s_MovementActive = true;
	float Split = DoIndependentDropdownMenu(&s_MovementActive,
											&MainView,
											Localize("Movement"),
											HeaderHeight,
											&CMenus::RenderSettingsControlsMovement,
											&s_MovementActive);
	MainView.HSplitTop(Split + 10.0f, &LastExpandRect, &MainView);
	s_ScrollRegion.AddRect(LastExpandRect);

	static bool s_WeaponActive = true;
	Split = DoIndependentDropdownMenu(&s_WeaponActive,
									  &MainView,
									  Localize("Weapons & Items"),
									  HeaderHeight,
									  &CMenus::RenderSettingsControlsWeapons,
									  &s_WeaponActive);
	MainView.HSplitTop(Split + 10.0f, &LastExpandRect, &MainView);
	s_ScrollRegion.AddRect(LastExpandRect);

	static bool s_VotingActive = true;
	Split = DoIndependentDropdownMenu(&s_VotingActive,
									  &MainView,
									  Localize("Voting"),
									  HeaderHeight,
									  &CMenus::RenderSettingsControlsVoting,
									  &s_VotingActive);
	MainView.HSplitTop(Split + 10.0f, &LastExpandRect, &MainView);
	s_ScrollRegion.AddRect(LastExpandRect);

	static bool s_ChatActive = true;
	Split = DoIndependentDropdownMenu(
		&s_ChatActive, &MainView, Localize("Chat"), HeaderHeight, &CMenus::RenderSettingsControlsChat, &s_ChatActive);
	MainView.HSplitTop(Split + 10.0f, &LastExpandRect, &MainView);
	s_ScrollRegion.AddRect(LastExpandRect);

	static bool s_MiscActive = true;
	Split = DoIndependentDropdownMenu(&s_MiscActive,
									  &MainView,
									  Localize("Miscellaneous"),
									  HeaderHeight,
									  &CMenus::RenderSettingsControlsMisc,
									  &s_MiscActive);
	MainView.HSplitTop(Split + 10.0f, &LastExpandRect, &MainView);
	s_ScrollRegion.AddRect(LastExpandRect);

	s_ScrollRegion.End();
	m_pUiClipScrollRegion = 0;

	// reset to defaults
	{
		ResetButton = BottomView;
		ResetButton.VSplitLeft(250.0f, 0, &ResetButton);
		ResetButton.VSplitRight(250.0f, &ResetButton, 0);

		ResetButton.HSplitTop(10.0f, 0, &ResetButton);
		RenderTools()->DrawUIRect(&ResetButton, vec4(0.08f, 0.09f, 0.11f, 0.9f), CUI::CORNER_ALL, 10.0f);
		ResetButton.HMargin(10.0f, &ResetButton);
		ResetButton.VMargin(30.0f, &ResetButton);
		ResetButton.HSplitTop(20.0f, &ResetButton, 0);
		static int s_DefaultButton = 0;
		if(DoButton_Menu((void *)&s_DefaultButton, Localize("Reset to defaults"), 0, &ResetButton))
			m_pClient->m_pBinds->SetDefaults();
	}
}

bool CMenus::DoResolutionList(CUIRect *pRect,
							  const void *pID,
							  float *pScrollValue,
							  const sorted_array<CVideoMode> &lModes)
{
	CUIRect View = *pRect;
	CUIRect Scroll, Row;
	char aBuf[32];
	int OldSelected = -1;

	DrawMenuInset(&View, 0);
	View.VSplitRight(15.0f, &View, &Scroll);

	const float RowHeight = 24.0f;
	View.HSplitTop(RowHeight, &Row, 0);
	int NumViewable = max(1, (int)(View.h / Row.h));
	int ScrollNum = lModes.size() - NumViewable + 1;
	if(ScrollNum < 0)
		ScrollNum = 0;

	if(ScrollNum > 0)
	{
		if(UI()->MouseInside(&View))
		{
			if(Input()->KeyPresses(KEY_MOUSE_WHEEL_UP))
				*pScrollValue -= 3.0f / ScrollNum;
			if(Input()->KeyPresses(KEY_MOUSE_WHEEL_DOWN))
				*pScrollValue += 3.0f / ScrollNum;
		}
		*pScrollValue = clamp(*pScrollValue, 0.0f, 1.0f);
		Scroll.HMargin(5.0f, &Scroll);
		*pScrollValue = DoScrollbarV(pID, &Scroll, *pScrollValue);
	}
	else
	{
		*pScrollValue = 0.0f;
	}

	UI()->ClipEnable(&View);
	View.y -= (*pScrollValue) * ScrollNum * Row.h;

	bool Changed = false;
	for(int i = 0; i < lModes.size(); ++i)
	{
		if(g_Config.m_GfxScreenWidth == lModes[i].m_Width && g_Config.m_GfxScreenHeight == lModes[i].m_Height)
			OldSelected = i;

		CUIRect Item = View;
		Item.y = View.y + i * Row.h;
		Item.h = Row.h - 2.0f;
		Item.VMargin(4.0f, &Item);

		const bool Selected = OldSelected == i;
		if(Selected)
			RenderTools()->DrawUIRect(
				&Item, ms_ColorAccent * vec4(1, 1, 1, 0.32f), CUI::CORNER_ALL, ms_ControlRounding);
		else if(UI()->MouseInside(&Item))
			RenderTools()->DrawUIRect(&Item,
									  vec4(0.10f, 0.11f, 0.13f, 0.22f) * ButtonColorMul(&lModes[i]),
									  CUI::CORNER_ALL,
									  ms_ControlRounding);

		if(UI()->DoButtonLogic(&lModes[i], "", Selected, &Item))
		{
			g_Config.m_GfxScreenWidth = lModes[i].m_Width;
			g_Config.m_GfxScreenHeight = lModes[i].m_Height;
			g_Config.m_GfxColorDepth = 24;
			Changed = true;
		}

		const int G = gcd(lModes[i].m_Width, lModes[i].m_Height);
		str_format(aBuf,
				   sizeof(aBuf),
				   "%dx%d (%d:%d)",
				   lModes[i].m_Width,
				   lModes[i].m_Height,
				   lModes[i].m_Width / G,
				   lModes[i].m_Height / G);
		UI()->DoLabelScaled(&Item, aBuf, 14.0f, 0);
	}
	UI()->ClipDisable();
	return Changed;
}

void CMenus::RenderSettingsGraphics(CUIRect MainView)
{
	CUIRect Button;
	char aBuf[128];
	bool CheckSettings = false;

	static int s_LastScreen = -1;
	if(s_LastScreen != g_Config.m_GfxScreen || m_NumModes <= 0)
	{
		UpdateVideoModeSettings();
		s_LastScreen = g_Config.m_GfxScreen;
	}

	static int s_GfxFsaaSamples = g_Config.m_GfxFsaaSamples;
	static int s_GfxTextureQuality = g_Config.m_GfxTextureQuality;

	const bool Advanced = g_Config.m_UiAdvancedSettings != 0;
	const bool Compact = MainView.w < 560.0f;
	CUIRect ModeList;
	CUIRect CompactCanvas;
	CUIRect CompactResolution;
	static bool s_CompactResolutionOpen = false;
	static int s_CompactResolutionButton = 0;
	static float s_RecScroll = 0.0f;
	static float s_OthScroll = 0.0f;
	static int s_RecScrollId = 0;
	static int s_OthScrollId = 0;

	const auto RenderResolutionPicker = [&](CUIRect Picker) {
		CUIRect Header, HeaderLeft, HeaderRight, ListRec, ListOth, Footer;
		Picker.HSplitTop(20.0f, &Header, &Picker);
		DrawSectionHeader(&Header, CUI::CORNER_T);
		UI()->DoLabel(&Header, Localize("Resolution"), min(Header.h * ms_FontmodHeight, 12.0f), 0);

		Picker.HSplitTop(4.0f, 0, &Picker);
		Picker.HSplitTop(20.0f, &Header, &Picker);
		Header.VSplitMid(&HeaderLeft, &HeaderRight);
		HeaderRight.VSplitLeft(3.0f, 0, &HeaderRight);
		DrawSectionHeader(&HeaderLeft, CUI::CORNER_T);
		DrawSectionHeader(&HeaderRight, CUI::CORNER_T);
		UI()->DoLabel(&HeaderLeft, Localize("Recommended"), min(HeaderLeft.h * ms_FontmodHeight, 11.0f), 0);
		UI()->DoLabel(&HeaderRight, Localize("Other"), min(HeaderRight.h * ms_FontmodHeight, 11.0f), 0);

		Picker.HSplitBottom(20.0f, &Picker, &Footer);
		Picker.HSplitBottom(4.0f, &Picker, 0);
		Picker.VSplitMid(&ListRec, &ListOth);
		ListOth.VSplitLeft(3.0f, 0, &ListOth);

		const int OldWidth = g_Config.m_GfxScreenWidth;
		const int OldHeight = g_Config.m_GfxScreenHeight;
		const int OldColorDepth = g_Config.m_GfxColorDepth;
		const bool RecommendedChanged = DoResolutionList(&ListRec, &s_RecScrollId, &s_RecScroll, m_lRecommendedVideoModes);
		const bool OtherChanged = DoResolutionList(&ListOth, &s_OthScrollId, &s_OthScroll, m_lOtherVideoModes);
		const bool ResolutionChanged = RecommendedChanged || OtherChanged;
		if(ResolutionChanged)
		{
			if(!m_pClient->ApplyWindowSettings())
			{
				g_Config.m_GfxScreenWidth = OldWidth;
				g_Config.m_GfxScreenHeight = OldHeight;
				g_Config.m_GfxColorDepth = OldColorDepth;
			}
			else
				CheckSettings = true;
		}

		DrawMenuInset(&Footer, CUI::CORNER_B);
		const int G = gcd(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight);
		str_format(aBuf,
				   sizeof(aBuf),
				   Localize("Current: %dx%d (%d:%d)"),
				   g_Config.m_GfxScreenWidth,
				   g_Config.m_GfxScreenHeight,
				   g_Config.m_GfxScreenWidth / G,
				   g_Config.m_GfxScreenHeight / G);
		UI()->DoLabel(&Footer, aBuf, min(Footer.h * ms_FontmodHeight, 11.0f), 0);
		return ResolutionChanged;
	};

	if(Compact)
	{
		CompactCanvas = MainView;
		MainView.HSplitTop(30.0f, &CompactResolution, &MainView);
		const int G = gcd(g_Config.m_GfxScreenWidth, g_Config.m_GfxScreenHeight);
		str_format(aBuf,
				   sizeof(aBuf),
				   "%s  %dx%d (%d:%d)",
				   Localize("Resolution"),
				   g_Config.m_GfxScreenWidth,
				   g_Config.m_GfxScreenHeight,
				   g_Config.m_GfxScreenWidth / G,
				   g_Config.m_GfxScreenHeight / G);
		if(DoButton_Menu(&s_CompactResolutionButton,
				 aBuf,
				 s_CompactResolutionOpen,
				 &CompactResolution,
				 s_CompactResolutionOpen ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_GHOST))
			s_CompactResolutionOpen = !s_CompactResolutionOpen;
		MainView.HSplitTop(8.0f, 0, &MainView);
	}
	else
	{
		MainView.VSplitLeft(300.0f, &MainView, &ModeList);
		ModeList.VSplitLeft(10.0f, 0, &ModeList);
		RenderResolutionPicker(ModeList);
		s_CompactResolutionOpen = false;
	}

	// display mode
	MainView.HSplitTop(18.0f, &Button, &MainView);
	UI()->DoLabelScaled(&Button, Localize("Display mode"), 12.0f, -1);
	MainView.HSplitTop(28.0f, &Button, &MainView);
	static int s_aDisplayModeButtons[3];
	const int ActiveDisplayMode = CurrentGfxDisplayMode();
	for(int Mode = 0; Mode < 3; Mode++)
	{
		CUIRect ModeButton;
		Button.VSplitLeft(Button.w / (3 - Mode), &ModeButton, &Button);
		ModeButton.VMargin(Mode == 0 ? 0.0f : 2.0f, &ModeButton);
		const char *pLabel = Mode == 0 ? Localize("Fullscreen")
							 : Mode == 1 ? Localize("Borderless window")
										 : Localize("Windowed");
		if(DoButton_Menu(&s_aDisplayModeButtons[Mode],
						 pLabel,
						 ActiveDisplayMode == Mode,
						 &ModeButton,
						 ActiveDisplayMode == Mode ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
		{
			const int OldBorderless = g_Config.m_GfxBorderless;
			const int OldFullscreen = g_Config.m_GfxFullscreen;
			ApplyGfxDisplayMode(Mode);
			if(!m_pClient->ApplyWindowSettings())
			{
				g_Config.m_GfxBorderless = OldBorderless;
				g_Config.m_GfxFullscreen = OldFullscreen;
			}
			else
				CheckSettings = true;
		}
	}
	MainView.HSplitTop(8.0f, 0, &MainView);

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxVsync, Localize("V-Sync"), g_Config.m_GfxVsync, &Button))
	{
		const int OldVsync = g_Config.m_GfxVsync;
		g_Config.m_GfxVsync ^= 1;
		if(!m_pClient->ApplyVSync())
			g_Config.m_GfxVsync = OldVsync;
		else
			CheckSettings = true;
	}

	if(!Advanced)
	{
		static int s_aQualityButtons[3];
		const int ActivePreset = CurrentGraphicsQualityPreset();
		MainView.HSplitTop(10.0f, 0, &MainView);
		MainView.HSplitTop(18.0f, &Button, &MainView);
		UI()->DoLabelScaled(&Button, Localize("Graphics quality"), 12.0f, -1);
		MainView.HSplitTop(30.0f, &Button, &MainView);
		for(int Preset = 0; Preset < 3; Preset++)
		{
			CUIRect PresetButton;
			Button.VSplitLeft(Button.w / (3 - Preset), &PresetButton, &Button);
			PresetButton.VMargin(Preset == 0 ? 0.0f : 2.0f, &PresetButton);
			if(DoButton_Menu(&s_aQualityButtons[Preset],
							 Localize(GraphicsQualityPresetLabel(Preset)),
							 ActivePreset == Preset,
							 &PresetButton,
							 ActivePreset == Preset ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_NORMAL))
			{
				const int OldCompression = g_Config.m_GfxTextureCompression;
				const int OldMultiBuffering = g_Config.m_GfxMultiBuffering;
				ApplyGraphicsQualityPreset(Preset);
				if(OldCompression != g_Config.m_GfxTextureCompression && !m_pClient->ApplyTextureSettings())
				{
					g_Config.m_GfxTextureCompression = OldCompression;
					m_pClient->ApplyTextureSettings();
				}
				if(OldMultiBuffering != g_Config.m_GfxMultiBuffering)
					m_pClient->ApplyMultiBuffering();
				CheckSettings = true;
			}
		}

		if(Compact && s_CompactResolutionOpen)
		{
			CUIRect Popup = CompactCanvas;
			Popup.y += 36.0f;
			Popup.h -= 36.0f;
			Popup.Margin(4.0f, &Popup);
			DrawMenuPanel(&Popup, CUI::CORNER_ALL);
			Popup.Margin(6.0f, &Popup);
			if(RenderResolutionPicker(Popup))
				s_CompactResolutionOpen = false;
		}

		if(CheckSettings)
			m_NeedRestartGraphics =
				s_GfxFsaaSamples != g_Config.m_GfxFsaaSamples || s_GfxTextureQuality != g_Config.m_GfxTextureQuality;
		return;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox_Number(
		   &g_Config.m_GfxFsaaSamples, Localize("FSAA samples"), g_Config.m_GfxFsaaSamples, &Button))
	{
		g_Config.m_GfxFsaaSamples = (g_Config.m_GfxFsaaSamples + 1) % 17;
		CheckSettings = true;
	}

	/*
	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxThreaded, Localize("Threaded rendering"), g_Config.m_GfxThreaded, &Button))
	{
		g_Config.m_GfxThreaded ^= 1;
		CheckSettings = true;
	}
	*/

	MainView.HSplitTop(20.0f, &Button, &MainView);
	// if(g_Config.m_GfxThreaded)
	{
		Button.VSplitLeft(20.0f, 0, &Button);
		if(DoButton_CheckBox(&g_Config.m_GfxAsyncRender,
							 Localize("Handle rendering async from updates"),
							 g_Config.m_GfxAsyncRender,
							 &Button))
		{
			g_Config.m_GfxAsyncRender ^= 1;
			CheckSettings = true;
		}
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(
		   &g_Config.m_GfxTextureQuality, Localize("Quality Textures"), g_Config.m_GfxTextureQuality, &Button))
	{
		g_Config.m_GfxTextureQuality ^= 1;
		CheckSettings = true;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxTextureCompression,
						 Localize("Texture Compression"),
						 g_Config.m_GfxTextureCompression,
						 &Button))
	{
		const int OldCompression = g_Config.m_GfxTextureCompression;
		g_Config.m_GfxTextureCompression ^= 1;
		if(!m_pClient->ApplyTextureSettings())
		{
			g_Config.m_GfxTextureCompression = OldCompression;
			m_pClient->ApplyTextureSettings();
		}
		else
			CheckSettings = true;
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxHighDetail, Localize("High Detail"), g_Config.m_GfxHighDetail, &Button))
		g_Config.m_GfxHighDetail ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_GfxMultiBuffering,
						 Localize("Multi framebuffering"),
						 g_Config.m_GfxMultiBuffering,
						 &Button))
	{
		g_Config.m_GfxMultiBuffering ^= 1;
		if(!g_Config.m_GfxMultiBuffering)
			g_Config.m_ClLighting = 0;
		else
			g_Config.m_ClLighting = 1;
		m_pClient->ApplyMultiBuffering();
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	Button.VSplitLeft(20.0f, 0, &Button);
	if(DoButton_CheckBox(&g_Config.m_ClLighting, Localize("Dynamic lighting"), g_Config.m_ClLighting, &Button))
		g_Config.m_ClLighting ^= 1;

	// check if the new settings require a restart
	if(CheckSettings)
		m_NeedRestartGraphics = s_GfxFsaaSamples != g_Config.m_GfxFsaaSamples ||
			s_GfxTextureQuality != g_Config.m_GfxTextureQuality;

	MainView.HSplitTop(20.0f, &Button, &MainView);

	// blood slider
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Blood amount"), 14.0f, -1);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		g_Config.m_GoreBlood =
			(int)(DoScrollbarH(&g_Config.m_GoreBlood, &Button, g_Config.m_GoreBlood / 100.0f) * 100.0f);
		MainView.HSplitTop(20.0f, 0, &MainView);
	}

	// hit feedback slider
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Hit feedback strength"), 14.0f, -1);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		g_Config.m_ClHitFeedback =
			(int)(DoScrollbarH(&g_Config.m_ClHitFeedback, &Button, g_Config.m_ClHitFeedback / 100.0f) * 100.0f);
		MainView.HSplitTop(20.0f, 0, &MainView);
	}

	// movement feedback slider
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Movement feedback strength"), 14.0f, -1);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		g_Config.m_ClMovementFeedback =
			(int)(DoScrollbarH(&g_Config.m_ClMovementFeedback, &Button, g_Config.m_ClMovementFeedback / 100.0f) *
				  100.0f);
		MainView.HSplitTop(20.0f, 0, &MainView);
	}

	//

	CUIRect Text;
	MainView.HSplitTop(20.0f, 0, &MainView);
	MainView.HSplitTop(20.0f, &Text, &MainView);
	// text.VSplitLeft(15.0f, 0, &text);

	/*
	UI()->DoLabelScaled(&Text, Localize("UI Color"), 14.0f, -1);

	const char *paLabels[] = {
		Localize("Hue"),
		Localize("Sat."),
		Localize("Lht."),
		Localize("Alpha")};
	int *pColorSlider[4] = {&g_Config.m_UiColorHue, &g_Config.m_UiColorSat, &g_Config.m_UiColorLht,
	&g_Config.m_UiColorAlpha}; for(int s = 0; s < 4; s++)
	{
		CUIRect Text;
		MainView.HSplitTop(19.0f, &Button, &MainView);
		Button.VMargin(15.0f, &Button);
		Button.VSplitLeft(100.0f, &Text, &Button);
		//Button.VSplitRight(5.0f, &Button, 0);
		Button.HSplitTop(4.0f, 0, &Button);

		float k = (*pColorSlider[s]) / 255.0f;
		k = DoScrollbarH(pColorSlider[s], &Button, k);
		*pColorSlider[s] = (int)(k*255.0f);
		UI()->DoLabelScaled(&Text, paLabels[s], 15.0f, -1);
	}
	*/

	if(Compact && s_CompactResolutionOpen)
	{
		CUIRect Popup = CompactCanvas;
		Popup.y += 36.0f;
		Popup.h -= 36.0f;
		Popup.Margin(4.0f, &Popup);
		DrawMenuPanel(&Popup, CUI::CORNER_ALL);
		Popup.Margin(6.0f, &Popup);
		if(RenderResolutionPicker(Popup))
			s_CompactResolutionOpen = false;
	}
}

void CMenus::RenderSettingsSound(CUIRect MainView)
{
	CUIRect Button;
	MainView.VSplitMid(&MainView, 0);
	static int s_AppliedSndRate = g_Config.m_SndRate;
	static bool s_SndRatePending = false;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndEnable, Localize("Use sounds"), g_Config.m_SndEnable, &Button))
	{
		const int OldEnable = g_Config.m_SndEnable;
		g_Config.m_SndEnable ^= 1;
		if(!m_pClient->ApplySoundSettings())
		{
			g_Config.m_SndEnable = OldEnable;
			m_pClient->ApplySoundSettings();
		}
		else if(g_Config.m_SndEnable)
		{
			if(g_Config.m_SndMusic)
				m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
		}
		else
			m_pClient->m_pSounds->Stop(SOUND_MENU);
	}

	if(!g_Config.m_SndEnable)
		return;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	/*if(DoButton_CheckBox(&g_Config.m_SndEnvironmental, Localize("Environmental sounds"), g_Config.m_SndEnvironmental,
	   &Button)) g_Config.m_SndEnvironmental ^= 1;*/

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_SndMusic, Localize("Play background music"), g_Config.m_SndMusic, &Button))
	{
		g_Config.m_SndMusic ^= 1;
		if(Client()->State() == IClient::STATE_OFFLINE)
		{
			if(g_Config.m_SndMusic)
				m_pClient->m_pSounds->Play(CSounds::CHN_MUSIC, SOUND_MENU, 1.0f);
			else
				m_pClient->m_pSounds->Stop(SOUND_MENU);
		}
	}

	// music volume slider
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Music volume"), 14.0f, -1);
		g_Config.m_SndMusicVolume =
			(int)(DoScrollbarH(&g_Config.m_SndMusicVolume, &Button, g_Config.m_SndMusicVolume / 100.0f) * 100.0f);
		m_pClient->m_pSounds->SetMusicVolume(g_Config.m_SndMusicVolume / 100.0f);
		MainView.HSplitTop(20.0f, 0, &MainView);
	}

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(
		   &g_Config.m_SndNonactiveMute, Localize("Mute when not active"), g_Config.m_SndNonactiveMute, &Button))
		g_Config.m_SndNonactiveMute ^= 1;

	MainView.HSplitTop(20.0f, &Button, &MainView);
	if(DoButton_CheckBox(&g_Config.m_ClThreadsoundloading,
						 Localize("Threaded sound loading"),
						 g_Config.m_ClThreadsoundloading,
						 &Button))
		g_Config.m_ClThreadsoundloading ^= 1;

	// sample rate box
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d", g_Config.m_SndRate);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		UI()->DoLabelScaled(&Button, Localize("Sample rate"), 14.0f, -1);
		Button.VSplitLeft(190.0f, 0, &Button);
		static float Offset = 0.0f;
		const int OldRate = g_Config.m_SndRate;
		const bool Edited = DoEditBox(&g_Config.m_SndRate, &Button, aBuf, sizeof(aBuf), 14.0f, &Offset) != 0;
		g_Config.m_SndRate = max(1, str_toint(aBuf));
		if(Edited && OldRate != g_Config.m_SndRate)
			s_SndRatePending = true;
		if(s_SndRatePending && !UI()->ActiveItem())
		{
			const int PendingRate = g_Config.m_SndRate;
			if(!m_pClient->ApplySoundSettings())
			{
				g_Config.m_SndRate = s_AppliedSndRate;
				m_pClient->ApplySoundSettings();
			}
			else
				s_AppliedSndRate = PendingRate;
			s_SndRatePending = false;
		}
	}

	// volume slider
	{
		CUIRect Button, Label;
		MainView.HSplitTop(5.0f, &Button, &MainView);
		MainView.HSplitTop(20.0f, &Button, &MainView);
		Button.VSplitLeft(190.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);
		UI()->DoLabelScaled(&Label, Localize("Sound volume"), 14.0f, -1);
		g_Config.m_SndVolume =
			(int)(DoScrollbarH(&g_Config.m_SndVolume, &Button, g_Config.m_SndVolume / 100.0f) * 100.0f);
		MainView.HSplitTop(20.0f, 0, &MainView);
	}
}

// custom menu for the client
void CMenus::RenderSettingsGamepad(CUIRect MainView)
{
	CUIRect Left, Right, Button;
	MainView.VSplitMid(&Left, &Right);
	Left.w -= 10.0f;
	Right.x += 10.0f;
	Right.w -= 10.0f;
	auto Slider = [&](CUIRect &View, int *pValue, int Min, int Max, const char *pLabel, float DisplayScale) {
		CUIRect Label, Bar;
		View.HSplitTop(20.0f, &Label, &View);
		char aText[128];
		if(DisplayScale == 1.0f)
			str_format(aText, sizeof(aText), "%s: %d", pLabel, *pValue);
		else
			str_format(aText, sizeof(aText), "%s: %.2f", pLabel, *pValue * DisplayScale);
		UI()->DoLabelScaled(&Label, aText, 13.0f, -1);
		View.HSplitTop(18.0f, &Bar, &View);
		*pValue = Min + (int)(DoScrollbarH(pValue, &Bar, (*pValue - Min) / (float)(Max - Min)) * (Max - Min));
		View.HSplitTop(6.0f, 0, &View);
	};
	Slider(Left, &g_Config.m_ClGamepadAimSensitivity, 25, 300, Localize("Aim sensitivity"), 1.0f);
	Slider(Left, &g_Config.m_ClGamepadMoveDeadzone, 10, 80, Localize("Movement deadzone"), 1.0f);
	Slider(Left, &g_Config.m_ClGamepadAimDeadzone, 0, 60, Localize("Aim deadzone"), 1.0f);
	Slider(Left, &g_Config.m_ClGamepadAimCurve, 50, 300, Localize("Aim response curve"), 0.01f);
	Slider(Left, &g_Config.m_ClGamepadAimAssist, 0, 100, Localize("Aim assist strength"), 1.0f);
	Left.HSplitTop(22.0f, &Button, &Left);
	UI()->DoLabelScaled(&Button, Localize("PvE: slowdown and light magnetism · PvP: slowdown only"), 11.0f, -1);

	IPlatformServices *pPlatform = Kernel()->RequestInterface<IPlatformServices>();
	static int s_OpenSteamInput;
	Right.HSplitTop(22.0f, &Button, &Right);
	if(DoButton_Menu(
		   &s_OpenSteamInput, Localize("Open Steam controller configuration"), 0, &Button, BUTTONSTYLE_ACCENT) &&
	   pPlatform)
		pPlatform->OpenInputConfiguration();
	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClGamepadInvertY, Localize("Invert gamepad vertical aim"), g_Config.m_ClGamepadInvertY, &Button))
		g_Config.m_ClGamepadInvertY ^= 1;
	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(
		   &g_Config.m_ClSteamGyro, Localize("Steam Input gyroscope aiming"), g_Config.m_ClSteamGyro, &Button))
		g_Config.m_ClSteamGyro ^= 1;
	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClSteamGyroInvert,
						 Localize("Invert gyroscope vertical aim"),
						 g_Config.m_ClSteamGyroInvert,
						 &Button))
		g_Config.m_ClSteamGyroInvert ^= 1;
	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(
		   &g_Config.m_ClSteamRumble, Localize("Steam Input vibration"), g_Config.m_ClSteamRumble, &Button))
		g_Config.m_ClSteamRumble ^= 1;
	Slider(Right, &g_Config.m_ClSteamGyroSensitivity, 1, 1000, Localize("Gyroscope sensitivity"), 1.0f);
	Right.HSplitTop(20.0f, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClInputDebug, Localize("Show input diagnostics"), g_Config.m_ClInputDebug, &Button))
		g_Config.m_ClInputDebug ^= 1;
	float AimX = 0.0f, AimY = 0.0f;
	Input()->GetGamepadAim(&AimX, &AimY);
	Right.HSplitTop(24.0f, &Button, &Right);
	char aPreview[128];
	str_format(aPreview, sizeof(aPreview), "%s  X %.2f  Y %.2f", Localize("Processed aim"), AimX, AimY);
	UI()->DoLabelScaled(&Button, aPreview, 13.0f, -1);
	Right.HSplitTop(22.0f, &Button, &Right);
	static int s_ResetGamepad;
	if(DoButton_Menu(&s_ResetGamepad, Localize("Reset controller defaults"), 0, &Button))
	{
		g_Config.m_ClGamepadAimSensitivity = 100;
		g_Config.m_ClGamepadMoveDeadzone = 35;
		g_Config.m_ClGamepadAimDeadzone = 18;
		g_Config.m_ClGamepadAimCurve = 150;
		g_Config.m_ClGamepadAimAssist = 35;
		g_Config.m_ClGamepadInvertY = 0;
	}
	struct CPadBinding
	{
		const char *m_pName;
		const char *m_pCommand;
		int m_Key;
	};
	static CPadBinding s_aPadBindings[] = {
		{"Jump", "+gamepadjump", 0}, {"Fire", "+gamepadfire", 0}, {"Hook", "+gamepadturbo", 0},
		{"Next weapon", "+gamepadnextweapon", 0}, {"Prev. weapon", "+gamepadprevweapon", 0},
		{"Drop weapon", "+gamepaddropweapon", 0}, {"Build menu", "+buildmenu", 0}, {"Emoticon", "+gamepademote", 0}};
	for(auto &Binding : s_aPadBindings)
	{
		Binding.m_Key = 0;
		for(int Key = KEY_GAMEPAD_BUTTON_A; Key < KEY_LAST; Key++)
			if(str_comp(m_pClient->m_pBinds->Get(Key), Binding.m_pCommand) == 0)
			{
				Binding.m_Key = Key;
				break;
			}
	}
	Right.HSplitTop(18.0f, &Button, &Right);
	UI()->DoLabelScaled(&Button, Localize("Controller bindings"), 13.0f, -1);
	for(auto &Binding : s_aPadBindings)
	{
		CUIRect Label, Reader;
		Right.HSplitTop(18.0f, &Button, &Right);
		Button.VSplitLeft(120.0f, &Label, &Reader);
		UI()->DoLabelScaled(&Label, Localize(Binding.m_pName), 11.0f, -1);
		const int OldKey = Binding.m_Key;
		const int NewKey = DoKeyReader(&Binding, &Reader, OldKey);
		if(NewKey != OldKey)
		{
			char aDisplaced[128];
			aDisplaced[0] = 0;
			if(NewKey)
				str_copy(aDisplaced, m_pClient->m_pBinds->Get(NewKey), sizeof(aDisplaced));
			if(OldKey)
				m_pClient->m_pBinds->Bind(OldKey, "");
			if(NewKey)
			{
				if(aDisplaced[0] && OldKey)
					m_pClient->m_pBinds->Bind(OldKey, aDisplaced);
				m_pClient->m_pBinds->Bind(NewKey, Binding.m_pCommand);
			}
		}
	}
}

// custom menu for the client
void CMenus::RenderSettingsCustom(CUIRect MainView)
{
}

class CLanguage
{
  public:
	CLanguage() {}
	CLanguage(const char *n, const char *f, int Code) : m_Name(n), m_FileName(f), m_CountryCode(Code) {}

	string m_Name;
	string m_FileName;
	int m_CountryCode;

	bool operator<(const CLanguage &Other) { return m_Name < Other.m_Name; }
};

void LoadLanguageIndexfile(IStorage *pStorage, IConsole *pConsole, sorted_array<CLanguage> *pLanguages)
{
	IOHANDLE File = pStorage->OpenFile("languages/index.txt", IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "couldn't open index file");
		return;
	}

	char aOrigin[128];
	char aReplacement[128];
	CLineReader LineReader;
	LineReader.Init(File);
	char *pLine;
	while((pLine = LineReader.Get()))
	{
		if(!str_length(pLine) || pLine[0] == '#') // skip empty lines and comments
			continue;

		str_copy(aOrigin, pLine, sizeof(aOrigin));

		pLine = LineReader.Get();
		if(!pLine)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			(void)LineReader.Get();
			continue;
		}
		str_copy(aReplacement, pLine + 3, sizeof(aReplacement));

		pLine = LineReader.Get();
		if(!pLine)
		{
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", "unexpected end of index file");
			break;
		}

		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "malform replacement for index '%s'", aOrigin);
			pConsole->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "localization", aBuf);
			continue;
		}

		char aFileName[128];
		str_format(aFileName, sizeof(aFileName), "languages/%s.txt", aOrigin);
		pLanguages->add(CLanguage(aReplacement, aFileName, str_toint(pLine + 3)));
	}
	io_close(File);
}

void CMenus::RenderLanguageSelection(CUIRect MainView)
{
	static int s_LanguageList = 0;
	static int s_SelectedLanguage = 0;
	static sorted_array<CLanguage> s_Languages;
	static float s_ScrollValue = 0;

	if(s_Languages.size() == 0)
	{
		s_Languages.add(CLanguage("English", "", 826));
		LoadLanguageIndexfile(Storage(), Console(), &s_Languages);
		for(int i = 0; i < s_Languages.size(); i++)
			if(str_comp(s_Languages[i].m_FileName, g_Config.m_ClLanguagefile) == 0)
			{
				s_SelectedLanguage = i;
				break;
			}
	}

	int OldSelected = s_SelectedLanguage;

	UiDoListboxStart(&s_LanguageList,
					 &MainView,
					 24.0f,
					 Localize("Language"),
					 "",
					 s_Languages.size(),
					 1,
					 s_SelectedLanguage,
					 s_ScrollValue);

	for(sorted_array<CLanguage>::range r = s_Languages.all(); !r.empty(); r.pop_front())
	{
		CListboxItem Item = UiDoListboxNextItem(&r.front());

		if(Item.m_Visible)
		{
			CUIRect Rect;
			Item.m_Rect.VSplitLeft(Item.m_Rect.h * 2.0f, &Rect, &Item.m_Rect);
			Rect.VMargin(6.0f, &Rect);
			Rect.HMargin(3.0f, &Rect);
			vec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
			m_pClient->m_pCountryFlags->Render(r.front().m_CountryCode, &Color, Rect.x, Rect.y, Rect.w, Rect.h);
			Item.m_Rect.HSplitTop(2.0f, 0, &Item.m_Rect);
			UI()->DoLabelScaled(&Item.m_Rect, r.front().m_Name, 16.0f, -1);
		}
	}

	s_SelectedLanguage = UiDoListboxEnd(&s_ScrollValue, 0);

	if(OldSelected != s_SelectedLanguage)
	{
		str_copy(
			g_Config.m_ClLanguagefile, s_Languages[s_SelectedLanguage].m_FileName, sizeof(g_Config.m_ClLanguagefile));
		g_Config.m_ClLanguagecode = s_Languages[s_SelectedLanguage].m_CountryCode;
		g_Localization.Load(s_Languages[s_SelectedLanguage].m_FileName, Storage(), Console());
	}
}

void CMenus::RenderCustomize(CUIRect MainView)
{
	/*
	CUIRect Temp, TabBar, RestartWarning;
	MainView.HSplitBottom(15.0f, &MainView, &RestartWarning);
	MainView.VSplitRight(120.0f, &MainView, &TabBar);
	RenderTools()->DrawUIRect(&MainView, ms_ColorTabbarActive, CUI::CORNER_B|CUI::CORNER_TL, 10.0f);
	TabBar.HSplitTop(50.0f, &Temp, &TabBar);
	RenderTools()->DrawUIRect(&Temp, ms_ColorTabbarActive, CUI::CORNER_R, 10.0f);

	MainView.HSplitTop(10.0f, 0, &MainView);
	*/

	DrawMenuPanel(&MainView, CUI::CORNER_ALL);
	MainView.Margin(16.0f, &MainView);
	RenderPageHeader(&MainView, "Customize");

	CUIRect LeftView;
	MainView.VSplitMid(&LeftView, &MainView);
	MainView.VSplitRight(12.0f, &MainView, 0);
	LeftView.VSplitLeft(12.0f, 0, &LeftView);
	LeftView.VSplitRight(8.0f, &LeftView, 0);
	MainView.VSplitLeft(8.0f, 0, &MainView);

	// color select
	static int s_CustomizationColor = 0;
	const char *aColor[] = {Localize("Body"), Localize("Feet"), Localize("Skin"), Localize("Hair / hat")};
	int NumColors = (int)(sizeof(aColor) / sizeof(*aColor));

	CUIRect ColorRect, L;
	LeftView.HSplitTop(18.0f, &L, &LeftView);
	UI()->DoLabelScaled(&L, Localize("Change color of"), 14.0f, -1);

	LeftView.HSplitTop(4.0f, 0, &LeftView);
	LeftView.HSplitTop(22.0f, &ColorRect, &LeftView);

	{
		const float TabW = ColorRect.w / NumColors;
		for(int i = 0; i < NumColors; i++)
		{
			CUIRect Button;
			ColorRect.VSplitLeft(TabW, &Button, &ColorRect);
			Button.VMargin(1.0f, &Button);
			if(DoButton_MenuTab(&aColor[i], aColor[i], s_CustomizationColor == i, &Button, CUI::CORNER_ALL))
				s_CustomizationColor = i;
		}
	}

	CUIRect Slider;
	CUIRect Button, Label;

	LeftView.HSplitTop(8.0f, 0, &LeftView);
	LeftView.HSplitTop(70.0f, &Slider, &LeftView);

	int *pColors;
	if(s_CustomizationColor == 0)
		pColors = &g_Config.m_PlayerColorBody;
	else if(s_CustomizationColor == 1)
		pColors = &g_Config.m_PlayerColorFeet;
	else if(s_CustomizationColor == 2)
		pColors = &g_Config.m_PlayerColorSkin;
	else if(s_CustomizationColor == 3)
		pColors = &g_Config.m_PlayerColorTopper;
	else
		pColors = &g_Config.m_PlayerColorBody;

	const char *paLabels[] = {Localize("Hue"), Localize("Sat."), Localize("Lht.")};
	static int s_aColorSlider[3] = {0};

	int PrevColor = *pColors;

	// color sliders
	int Color = 0;
	for(int s = 0; s < 3; s++)
	{
		Slider.HSplitTop(22.0f, &Label, &Slider);
		Label.VSplitLeft(56.0f, &Label, &Button);
		Button.HMargin(2.0f, &Button);

		float k = ((PrevColor >> ((2 - s) * 8)) & 0xff) / 255.0f;
		k = DoScrollbarH(&s_aColorSlider[s], &Button, k);
		Color <<= 8;
		Color += clamp((int)(k * 255), 0, 255);
		UI()->DoLabelScaled(&Label, paLabels[s], 12.0f, -1);
	}

	if(PrevColor != Color)
		m_NeedSendinfo = true;

	*pColors = Color;

	LeftView.HSplitTop(8.0f, 0, &LeftView);

	// blood color select
	const char *aBlood[] = {Localize("Red"), Localize("Green"), Localize("Black")};
	int NumBloods = (int)(sizeof(aBlood) / sizeof(*aBlood));

	CUIRect BloodRect, B;
	LeftView.HSplitTop(18.0f, &B, &LeftView);
	UI()->DoLabelScaled(&B, Localize("Blood color"), 14.0f, -1);

	LeftView.HSplitTop(4.0f, 0, &LeftView);
	LeftView.HSplitTop(22.0f, &BloodRect, &LeftView);

	{
		const float TabW = BloodRect.w / NumBloods;
		for(int i = 0; i < NumBloods; i++)
		{
			CUIRect Button;
			BloodRect.VSplitLeft(TabW, &Button, &BloodRect);
			Button.VMargin(1.0f, &Button);
			if(DoButton_MenuTab(&aBlood[i], aBlood[i], g_Config.m_PlayerBloodColor == i, &Button, CUI::CORNER_ALL))
			{
				g_Config.m_PlayerBloodColor = i;
			}
		}
	}

	// dedicated preview area — keep clear of tabs/sliders/skin list
	LeftView.HSplitTop(14.0f, 0, &LeftView);
	CUIRect Preview;
	LeftView.HSplitTop(160.0f, &Preview, &LeftView);
	DrawMenuInset(&Preview, CUI::CORNER_ALL);
	{
		CTeeRenderInfo Info;
		Info.m_ColorBody = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorBody);
		Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorFeet);
		Info.m_Body = 0; // g_Config.m_PlayerBody;
		Info.m_TopperTexture =
			m_pClient->m_pSkins->GetTopper(m_pClient->m_pSkins->FindTopper(g_Config.m_PlayerTopper))->m_Texture;
		Info.m_EyeTexture = m_pClient->m_pSkins->GetEye(m_pClient->m_pSkins->FindEye(g_Config.m_PlayerEye))->m_Texture;
		Info.m_HeadTexture =
			m_pClient->m_pSkins->GetHead(m_pClient->m_pSkins->FindHead(g_Config.m_PlayerHead))->m_Texture;
		Info.m_BodyTexture =
			m_pClient->m_pSkins->GetBody(m_pClient->m_pSkins->FindBody(g_Config.m_PlayerBody))->m_Texture;
		Info.m_HandTexture =
			m_pClient->m_pSkins->GetHand(m_pClient->m_pSkins->FindHand(g_Config.m_PlayerHand))->m_Texture;
		Info.m_FootTexture =
			m_pClient->m_pSkins->GetFoot(m_pClient->m_pSkins->FindFoot(g_Config.m_PlayerFoot))->m_Texture;
		Info.m_ColorTopper = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorTopper);
		Info.m_ColorSkin = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
		Info.m_Size = UI()->Scale() * 56.0f;

		RenderTools()->RenderStaticPlayer(&Info, vec2(Preview.x + Preview.w * 0.5f, Preview.y + Preview.h * 0.58f));
	}

	// skin type select
	static int s_SkinType = 0;
	const char *aSkinType[] = {Localize("Head"),
							   Localize("Hair / hat"),
							   Localize("Eyes"),
							   Localize("Body"),
							   Localize("Hands"),
							   Localize("Feet")};
	int NumSkinTypes = (int)(sizeof(aSkinType) / sizeof(*aSkinType));

	CUIRect SkinTypeLabel, SkinSelect;
	MainView.HSplitTop(18.0f, &SkinTypeLabel, &MainView);
	UI()->DoLabelScaled(&SkinTypeLabel, Localize("Change skin of"), 14.0f, -1);

	// saving skins, helper for creating bot skins
	if(Input()->KeyDown(KEY_S) && (Input()->KeyPressed(KEY_LCTRL) || Input()->KeyPressed(KEY_RCTRL)))
	{
		SaveSkin();
	}

	MainView.HSplitTop(4.0f, 0, &MainView);
	MainView.HSplitTop(48.0f, &SkinSelect, &MainView);

	{
		CUIRect Row1, Row2;
		SkinSelect.HSplitMid(&Row1, &Row2);
		Row1.HSplitBottom(2.0f, &Row1, 0);
		Row2.HSplitTop(2.0f, 0, &Row2);
		const int PerRow = 3;
		for(int row = 0; row < 2; row++)
		{
			CUIRect Row = row == 0 ? Row1 : Row2;
			const float TabW = Row.w / PerRow;
			for(int col = 0; col < PerRow; col++)
			{
				const int i = row * PerRow + col;
				if(i >= NumSkinTypes)
					break;
				CUIRect Button;
				Row.VSplitLeft(TabW, &Button, &Row);
				Button.VMargin(1.0f, &Button);
				if(DoButton_MenuTab(&aSkinType[i], aSkinType[i], s_SkinType == i, &Button, CUI::CORNER_ALL))
					s_SkinType = i;
			}
		}
	}

	MainView.HSplitTop(6.0f, 0, &MainView);

	// eye selector
	if(s_SkinType == 2)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumEyes(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetEye(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Eyes"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerEye) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_EyeTexture = s->m_Texture;
				Info.m_Size = UI()->Scale() * 50.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderEye(&Info,
										 vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerEye, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerEye));
			m_NeedSendinfo = true;
		}
	}

	// topper selector
	if(s_SkinType == 1)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumToppers(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetTopper(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Hair / hat"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerTopper) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_TopperTexture = s->m_Texture;
				Info.m_ColorTopper = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorTopper);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderTopper(&Info,
											vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerTopper, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerTopper));
			m_NeedSendinfo = true;
		}
	}

	// head selector
	if(s_SkinType == 0)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumHeads(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetHead(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Head"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerHead) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_HeadTexture = s->m_Texture;
				// Info.m_ColorHead = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderHead(&Info,
										  vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerHead, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerHead));
			m_NeedSendinfo = true;
		}
	}

	// hand selector
	if(s_SkinType == 4)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumHands(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetHand(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Hands"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerHand) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_HandTexture = s->m_Texture;
				// Info.m_ColorHead = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderHand(&Info,
										  vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerHand, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerHand));
			m_NeedSendinfo = true;
		}
	}

	// foot selector
	if(s_SkinType == 5)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumFeet(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetFoot(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Feet"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerFoot) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_FootTexture = s->m_Texture;
				// Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderFoot(&Info,
										  vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerFoot, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerFoot));
			m_NeedSendinfo = true;
		}
	}

	// body selector
	if(s_SkinType == 3)
	{
		MainView.HSplitTop(20.0f, 0, &MainView);
		static bool s_InitSkinlist = true;
		static sorted_array<const CSkins::CSkinPart *> s_paSkinList;
		static float s_ScrollValue = 0.0f;
		if(s_InitSkinlist)
		{
			s_paSkinList.clear();
			for(int i = 0; i < m_pClient->m_pSkins->NumBodies(); ++i)
			{
				const CSkins::CSkinPart *s = m_pClient->m_pSkins->GetBody(i);
				// no special toppers
				if(s->m_aName[0] == 'x' && s->m_aName[1] == '_')
					continue;
				s_paSkinList.add(s);
			}
			s_InitSkinlist = false;
		}

		int OldSelected = -1;
		UiDoListboxStart(&s_InitSkinlist,
						 &MainView,
						 50.0f,
						 Localize("Body"),
						 "",
						 s_paSkinList.size(),
						 4,
						 OldSelected,
						 s_ScrollValue);

		for(int i = 0; i < s_paSkinList.size(); ++i)
		{
			const CSkins::CSkinPart *s = s_paSkinList[i];
			if(s == 0)
				continue;

			if(str_comp(s->m_aName, g_Config.m_PlayerBody) == 0)
				OldSelected = i;

			CListboxItem Item = UiDoListboxNextItem(&s_paSkinList[i], OldSelected == i);
			if(Item.m_Visible)
			{
				CTeeRenderInfo Info;
				Info.m_BodyTexture = s->m_Texture;
				// Info.m_ColorFeet = m_pClient->m_pSkins->GetColorV4(g_Config.m_PlayerColorSkin);
				Info.m_Size = UI()->Scale() * 80.0f;
				Item.m_Rect.HSplitTop(5.0f, 0, &Item.m_Rect); // some margin from the top

				RenderTools()->RenderBody(&Info,
										  vec2(Item.m_Rect.x + Item.m_Rect.w / 2, Item.m_Rect.y + Item.m_Rect.h / 2));
			}
		}

		const int NewSelected = UiDoListboxEnd(&s_ScrollValue, 0);
		if(OldSelected != NewSelected)
		{
			mem_copy(g_Config.m_PlayerBody, s_paSkinList[NewSelected]->m_aName, sizeof(g_Config.m_PlayerBody));
			m_NeedSendinfo = true;
		}
	}

	if(m_EscapePressed)
		g_Config.m_UiPage = PAGE_INTERNET;
}

void CMenus::RenderSettingsCloud(CUIRect MainView)
{
	CUIRect Title, StatusLine, Action;
	MainView.HSplitTop(34.0f, &Title, &MainView);
	UI()->DoLabelScaled(&Title, Localize("Steam Cloud"), 20.0f, -1);
	MainView.HSplitTop(44.0f, &StatusLine, &MainView);
	UI()->DoLabelScaled(&StatusLine,
						Localize(m_aCloudStatus[0] ? m_aCloudStatus : "Steam Cloud is unavailable"),
						13.0f,
						-1,
						(int)StatusLine.w);
	MainView.HSplitTop(8.0f, 0, &MainView);
	MainView.HSplitTop(28.0f, &Action, &MainView);
	static int s_CloudAction;
	const char *pAction = m_CloudConflict ? Localize("Resolve conflict") : Localize("Sync now");
	if(DoButton_Menu(&s_CloudAction, pAction, 0, &Action, BUTTONSTYLE_ACCENT))
	{
		if(m_CloudConflict)
			m_Popup = POPUP_CLOUD_CONFLICT;
		else
		{
			m_CloudPaused = false;
			m_CloudDirty = true;
			PumpCloudProfile(true);
		}
	}
	MainView.HSplitTop(16.0f, 0, &MainView);
	UI()->DoLabelScaled(&MainView,
						Localize("Progress, appearance, portable preferences and custom key bindings are synchronized. "
								 "Display, device and local network settings stay on this device."),
						12.0f,
						-1,
						(int)MainView.w);
}

void CMenus::RenderSettings(CUIRect MainView)
{
	CUIRect ModuleRail, Content, RestartWarning;
	MainView.HSplitBottom(18.0f, &MainView, &RestartWarning);
	DrawOpenPageFrame(&MainView);
	MainView.Margin(8.0f, &MainView);
	CUIRect Header, Workbench;
	MainView.HSplitTop(44.0f, &Header, &Workbench);
	RenderOfflineBackButton(&Header);
	CUIRect HeaderTitle, AdvancedToggle;
	Header.VSplitRight(170.0f, &HeaderTitle, &AdvancedToggle);
	UI()->DoLabelScaled(&HeaderTitle, Localize("Settings"), 20.0f, -1);
	DrawAccentUnderline(&HeaderTitle);
	AdvancedToggle.VMargin(2.0f, &AdvancedToggle);
	if(DoButton_CheckBox(&g_Config.m_UiAdvancedSettings,
						 Localize("Advanced options"),
						 g_Config.m_UiAdvancedSettings,
						 &AdvancedToggle))
		g_Config.m_UiAdvancedSettings ^= 1;
	Workbench.HSplitTop(6.0f, 0, &Workbench);
	Workbench.VSplitLeft(168.0f, &ModuleRail, &Content);
	Content.VSplitLeft(8.0f, 0, &Content);
	DrawTechShape(&ModuleRail, vec4(ms_ColorBgInset.r, ms_ColorBgInset.g, ms_ColorBgInset.b, .22f), ms_PanelRounding);
	DrawTechShape(&Content, vec4(ms_ColorBgInset.r, ms_ColorBgInset.g, ms_ColorBgInset.b, .14f), ms_PanelRounding);
	ModuleRail.Margin(8.0f, &ModuleRail);
	Content.Margin(10.0f, &Content);

	const char *aTabs[] = {Localize("General"),
						   Localize("Gameplay"),
						   Localize("Player"),
						   Localize("Controls"),
						   Localize("Graphics"),
						   Localize("Sound")};

	static int s_aModuleButtons[6];
	const int NumTabs = 6;
	const float ModuleGap = 6.0f;
	const float ModuleH = max(28.0f, (ModuleRail.h - ModuleGap * (NumTabs - 1)) / NumTabs);
	for(int i = 0; i < NumTabs; i++)
	{
		CUIRect Tile = {ModuleRail.x, ModuleRail.y + i * (ModuleH + ModuleGap), ModuleRail.w, ModuleH};
		const bool Selected = g_Config.m_UiSettingsPage == i;
		if(DoButton_Menu(&s_aModuleButtons[i],
						 aTabs[i],
						 Selected,
						 &Tile,
						 Selected ? BUTTONSTYLE_ACCENT : BUTTONSTYLE_GHOST))
			g_Config.m_UiSettingsPage = i;
	}

	if(g_Config.m_UiSettingsPage < 0 || g_Config.m_UiSettingsPage >= NumTabs)
		g_Config.m_UiSettingsPage = 0;
	if(g_Config.m_UiSettingsPage == 0)
		RenderSettingsGeneral(Content);
	else if(g_Config.m_UiSettingsPage == 1)
		RenderSettingsGameplay(Content);
	else if(g_Config.m_UiSettingsPage == 2)
		RenderSettingsPlayer(Content);
	else if(g_Config.m_UiSettingsPage == 3)
		RenderSettingsControls(Content);
	else if(g_Config.m_UiSettingsPage == 4)
		RenderSettingsGraphics(Content);
	else
		RenderSettingsSound(Content);

	if(m_NeedRestartGraphics)
	{
		TextRender()->TextColor(ms_ColorDanger.r, ms_ColorDanger.g, ms_ColorDanger.b, 1.0f);
		UI()->DoLabel(
			&RestartWarning, Localize("You must restart the game for all settings to take effect."), 12.0f, -1);
		TextRender()->TextColor(1, 1, 1, 1);
	}
}
