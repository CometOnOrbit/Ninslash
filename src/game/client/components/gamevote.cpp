#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/keys.h>
#include <generated/protocol.h>
#include <generated/game_data.h>

#include <game/client/gameclient.h>
#include <game/client/components/menus.h>
#include <game/client/local_game_modes.h>

#include <game/client/components/motd.h>
#include <game/client/components/scoreboard.h>

#include "skins.h"

#include <game/client/customstuff.h>
#include <game/client/customstuff/playerinfo.h>

#include "gamevote.h"

#include <math.h>

static int GameVoteLocalModeByName(const char *pName)
{
	static const struct
	{
		const char *m_pName;
		int m_Mode;
	} s_aNameAliases[] = {
		{"Reactor Bomber", LOCAL_MODE_REACTOR_ASSAULT},
		{"Reactor Defence", LOCAL_MODE_REACTOR_DEFENSE},
		{"Horde Survival", LOCAL_MODE_HORDE},
		{"Instakill CTF", LOCAL_MODE_INSTAGIB_CTF},
		{"Grenade Deathmatch", LOCAL_MODE_GRENADE_DM},
	};
	for(unsigned i = 0; i < sizeof(s_aNameAliases) / sizeof(s_aNameAliases[0]); i++)
		if(str_comp(pName, Localize(s_aNameAliases[i].m_pName)) == 0)
			return s_aNameAliases[i].m_Mode;
	for(int i = 1; i < LOCAL_MODE_COUNT; i++)
		if(str_comp(pName, Localize(s_aLocalGameModes[i].m_pName)) == 0)
			return i;
	return -1;
}

// Resolves a server game-vote entry to a local mode, preferring the localized
// name and falling back to the thumbnail name.
static int GameVoteLocalMode(const char *pName, const char *pImage)
{
	int Mode = GameVoteLocalModeByName(pName);
	if(Mode < 0)
		Mode = LocalGameModeFromImage(pImage);
	return Mode;
}

static const char *GameVoteCategoryName(int Category)
{
	static const char *s_apNames[NUM_GAMEVOTE_CATEGORIES] = {"Co-op", "Team", "Free-for-all", "Arcade"};
	return s_apNames[clamp(Category, 0, NUM_GAMEVOTE_CATEGORIES - 1)];
}

CGameVoteDisplay::CGameVoteDisplay() : m_DebugScreenshotFrames(0)
{
	OnReset();
}

void CGameVoteDisplay::OnReset()
{
	if(m_DebugScreenshotFrames > 0)
		return;

	m_GameVoteCount = 0;
	m_DisplayCount = 0;
	m_PageCapacity = 4;

	for(int i = 0; i < MAX_GAME_VOTES; i++)
	{
		m_aGameVoteDetails[i].m_Valid = false;
		m_aGameVoteDetails[i].m_Votes = 0;
	}

	m_MouseTrigger = false;
	m_SelectorMouse = vec2(150, 150);
	m_Selected = -1;
	m_Focused = 0;
	m_ActiveCategory = GAMEVOTE_CATEGORY_PVE;
	m_AppearAmount = 0.0f;
	m_SelectionPulse = 0.0f;
	m_TimeLeft = 0;
	m_VoteDuration = 0;
	m_LastVoteMessageTime = time_get();
}

void CGameVoteDisplay::OnConsoleInit()
{
	Console()->Register("gamevote_debug_preview",
						"?i",
						CFGFLAG_CLIENT,
						ConDebugPreview,
						this,
						"Preview the mode vote input overlay and optionally capture a screenshot");
}

void CGameVoteDisplay::ConDebugPreview(IConsole::IResult *pResult, void *pUserData)
{
	CGameVoteDisplay *pSelf = (CGameVoteDisplay *)pUserData;
	pSelf->m_DebugScreenshotFrames = 0;
	pSelf->OnReset();
	static const char *s_apNames[] = {"Invasion",
									  "Horde Survival",
									  "Extraction",
									  "Reactor Defence",
									  "Deathmatch",
									  "Team deathmatch",
									  "Capture the flag",
									  "Reactor Bomber",
									  "Ball",
									  "Battle Royale",
									  "Grenade Deathmatch",
									  "Instakill CTF",
									  "Roam Race"};
	static const char *s_apDescriptions[] = {"Roguelite expedition",
											 "Endless defense",
											 "Activate and evacuate",
											 "Fortify the reactor",
											 "Generated arena",
											 "Two-team battle",
											 "Objective combat",
											 "Protect the reactor",
											 "Team ball sport",
											 "Last survivor wins",
											 "Explosive free-for-all",
											 "One-shot team combat",
											 "Race through a modular course"};
	static const char *s_apImages[] = {"invasion1",
									   "invasion7",
									   "invasion6",
									   "reactor_def1",
									   "dm1",
									   "tdm1",
									   "ctf1",
									   "reactor1",
									   "ball1",
									   "br1",
									   "grenade1",
									   "ictf1",
									   "invasion1"};
	pSelf->m_GameVoteCount = 13;
	for(int i = 0; i < pSelf->m_GameVoteCount; i++)
	{
		pSelf->m_aGameVoteDetails[i].m_Valid = true;
		pSelf->m_aGameVoteDetails[i].m_Votes = i == 1 ? 2 : (i == 3 ? 1 : 0);
		str_copy(pSelf->m_aGameVoteDetails[i].m_aName, s_apNames[i], sizeof(pSelf->m_aGameVoteDetails[i].m_aName));
		str_copy(pSelf->m_aGameVoteDetails[i].m_aDescription,
				 s_apDescriptions[i],
				 sizeof(pSelf->m_aGameVoteDetails[i].m_aDescription));
		str_copy(pSelf->m_aGameVoteDetails[i].m_aImage, s_apImages[i], sizeof(pSelf->m_aGameVoteDetails[i].m_aImage));
		// Startup console arguments run before vote thumbnails are loaded. Keep
		// this deterministic preview texture-free instead of touching skin data.
		pSelf->m_aGameVoteDetails[i].m_Texture = -1;
	}
	pSelf->m_ActiveCategory = GAMEVOTE_CATEGORY_PVE;
	pSelf->m_Focused = 0;
	pSelf->m_Selected = -1;
	pSelf->m_AppearAmount = 1.0f;
	pSelf->m_TimeLeft = 24;
	pSelf->m_TimeLeftTick = 0;
	pSelf->m_VoteDuration = 30;
	pSelf->m_LastVoteMessageTime = time_get();
	pSelf->m_SelectorMouse = vec2(150.0f, 150.0f);
	pSelf->RebuildDisplayOrder();
	pSelf->m_DebugScreenshotFrames = pResult->NumArguments() && pResult->GetInteger(0) != 0 ? 20 : 0;
}

int CGameVoteDisplay::VoteCategory(int Vote) const
{
	if(Vote < 0 || Vote >= m_GameVoteCount || !m_aGameVoteDetails[Vote].m_Valid)
		return GAMEVOTE_CATEGORY_ARCADE;

	const CGameVoteDetails &Details = m_aGameVoteDetails[Vote];
	const int Mode = GameVoteLocalMode(Details.m_aName, Details.m_aImage);
	if(Mode >= 0)
		return LocalGameModeVoteCategory(Mode);

	// Fallback for vote entries that do not map to any local mode: infer the
	// category from the thumbnail file name.
	const char *pImage = Details.m_aImage;
	if(str_find_nocase(pImage, "invasion") || str_find_nocase(pImage, "horde") || str_find_nocase(pImage, "extract"))
		return GAMEVOTE_CATEGORY_PVE;
	if(str_find_nocase(pImage, "tdm") || str_find_nocase(pImage, "ctf"))
		return GAMEVOTE_CATEGORY_TEAM;
	if(str_find_nocase(pImage, "dm") || str_find_nocase(pImage, "grenade") || str_find_nocase(pImage, "br"))
		return GAMEVOTE_CATEGORY_SOLO;
	return GAMEVOTE_CATEGORY_ARCADE;
}

int CGameVoteDisplay::CategoryVoteCount(int Category) const
{
	int Count = 0;
	for(int d = 0; d < m_DisplayCount; d++)
	{
		const int i = m_aDisplayOrder[d];
		if(m_aGameVoteDetails[i].m_Valid && VoteCategory(i) == Category)
			Count++;
	}
	return Count;
}

int CGameVoteDisplay::FirstVoteInCategory(int Category) const
{
	for(int d = 0; d < m_DisplayCount; d++)
	{
		const int i = m_aDisplayOrder[d];
		if(m_aGameVoteDetails[i].m_Valid && VoteCategory(i) == Category)
			return i;
	}
	return -1;
}

void CGameVoteDisplay::ChangeCategory(int Direction)
{
	for(int Step = 1; Step <= NUM_GAMEVOTE_CATEGORIES; Step++)
	{
		const int Category = (m_ActiveCategory + Direction * Step + NUM_GAMEVOTE_CATEGORIES) % NUM_GAMEVOTE_CATEGORIES;
		if(CategoryVoteCount(Category) <= 0)
			continue;
		m_ActiveCategory = Category;
		if(m_Selected >= 0 && VoteCategory(m_Selected) == Category)
			m_Focused = m_Selected;
		else
			m_Focused = FirstVoteInCategory(Category);
		return;
	}
}

void CGameVoteDisplay::MoveFocus(int Direction)
{
	int aVotes[MAX_GAME_VOTES];
	int Count = 0;
	int Position = 0;
	for(int d = 0; d < m_DisplayCount; d++)
	{
		const int i = m_aDisplayOrder[d];
		if(!m_aGameVoteDetails[i].m_Valid || VoteCategory(i) != m_ActiveCategory)
			continue;
		if(i == m_Focused)
			Position = Count;
		aVotes[Count++] = i;
	}
	if(Count <= 0)
		return;
	Position = (Position + Direction + Count) % Count;
	m_Focused = aVotes[Position];
}

int CGameVoteDisplay::DisplaySortKey(int Vote) const
{
	const CGameVoteDetails &Details = m_aGameVoteDetails[Vote];
	const int Mode = GameVoteLocalMode(Details.m_aName, Details.m_aImage);
	return LocalGameModeSortKey(Mode < 0 ? -1 : Mode);
}

void CGameVoteDisplay::RebuildDisplayOrder()
{
	m_DisplayCount = 0;
	for(int i = 0; i < m_GameVoteCount; i++)
		if(m_aGameVoteDetails[i].m_Valid)
			m_aDisplayOrder[m_DisplayCount++] = i;

	// Stable insertion sort by local-mode order; same-mode variants keep
	// their relative order. Sort keys are cached to avoid re-parsing the
	// vote name and image on every comparison.
	int aKeys[MAX_GAME_VOTES];
	for(int i = 0; i < m_DisplayCount; i++)
		aKeys[i] = DisplaySortKey(m_aDisplayOrder[i]);
	for(int i = 1; i < m_DisplayCount; i++)
	{
		const int Key = aKeys[i];
		int j = i;
		while(j > 0 && aKeys[j - 1] > Key)
		{
			const int Tmp = m_aDisplayOrder[j - 1];
			m_aDisplayOrder[j - 1] = m_aDisplayOrder[j];
			m_aDisplayOrder[j] = Tmp;
			aKeys[j - 1] = aKeys[j];
			aKeys[j] = Key;
			j--;
		}
	}
}

bool CGameVoteDisplay::OnInput(IInput::CEvent Event)
{
	if(!IsActive())
		return false;

	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		int Direction = 0;
		if(Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_LEFT || Event.m_Key == KEY_UP ||
		   Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP)
			Direction = -1;
		else if(Event.m_Key == KEY_MOUSE_WHEEL_DOWN || Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_DOWN ||
				Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN)
			Direction = 1;

		if(Direction)
		{
			MoveFocus(Direction);
			return true;
		}

		if(Event.m_Key == KEY_TAB || Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT ||
		   Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT)
		{
			ChangeCategory(Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT ? -1 : 1);
			return true;
		}

		int DirectChoice = -1;
		if(Event.m_Key >= KEY_1 && Event.m_Key <= KEY_9)
			DirectChoice = Event.m_Key - KEY_1;
		else if(Event.m_Key >= KEY_KP_1 && Event.m_Key <= KEY_KP_9)
			DirectChoice = Event.m_Key - KEY_KP_1;
		if(DirectChoice >= 0)
		{
			// Number shortcuts select the card at the same slot on the visible
			// page, matching the per-card labels.
			int aVotes[MAX_GAME_VOTES];
			int Count = 0;
			int FocusPosition = 0;
			for(int d = 0; d < m_DisplayCount; d++)
			{
				const int i = m_aDisplayOrder[d];
				if(!m_aGameVoteDetails[i].m_Valid || VoteCategory(i) != m_ActiveCategory)
					continue;
				if(i == m_Focused)
					FocusPosition = Count;
				aVotes[Count++] = i;
			}
			const int Page = Count > 0 ? FocusPosition / m_PageCapacity : 0;
			const int PageStart = Page * m_PageCapacity;
			const int PageCount = min(m_PageCapacity, max(0, Count - PageStart));
			if(DirectChoice < PageCount)
			{
				const int Vote = aVotes[PageStart + DirectChoice];
				m_ActiveCategory = VoteCategory(Vote);
				m_Focused = Vote;
				m_Selected = Vote;
				SendVote();
				return true;
			}
		}

		if((Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_GAMEPAD_BUTTON_A) &&
		   m_Focused >= 0 && m_Focused < m_GameVoteCount && m_aGameVoteDetails[m_Focused].m_Valid)
		{
			m_Selected = m_Focused;
			SendVote();
			return true;
		}
	}

	if(Event.m_Key == KEY_MOUSE_1)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS)
			m_MouseTrigger = true;
		return true;
	}

	// A mode vote is a full-screen focused overlay. Consume unhandled presses
	// and releases so movement, firing and weapon binds cannot leak through.
	return true;
}

bool CGameVoteDisplay::OnMouseMove(float x, float y)
{
	if(!IsActive())
		return false;

	Input()->SetMouseModes(IInput::MOUSE_MODE_WARP_CENTER);

	Input()->GetRelativePosition(&x, &y);
	m_SelectorMouse += vec2(x, y) * 0.5f;

	return true;
}

void CGameVoteDisplay::RenderMouse()
{
	// cursor
	Graphics()->TextureSet(g_pData->m_aImages[IMAGE_CURSOR].m_Id);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1, 1, 1, 1);
	IGraphics::CQuadItem QuadItem(m_SelectorMouse.x, m_SelectorMouse.y, 16, 16);
	Graphics()->QuadsDrawTL(&QuadItem, 1);
	Graphics()->QuadsEnd();
}

void CGameVoteDisplay::SendVote()
{
	m_SelectionPulse = 1.0f;
	CNetMsg_Cl_VoteGameMode Msg;
	Msg.m_Vote = m_Selected;
	Client()->SendPackMsg(&Msg, MSGFLAG_VITAL);
}

void CGameVoteDisplay::OnRender()
{
	if(m_GameVoteCount <= 0)
		return;
	if(m_DebugScreenshotFrames > 0 && m_TimeLeftTick == 0)
		m_TimeLeftTick = Client()->GameTick();

	if(m_TimeLeft + (m_TimeLeftTick - Client()->GameTick()) / Client()->GameTickSpeed() < 0 &&
	   time_get() > m_LastVoteMessageTime + time_freq() * 8)
	{
		OnReset();
		return;
	}

	const float Aspect = Graphics()->ScreenAspect();
	const float ScreenWidth = 300.0f * Aspect;
	Graphics()->MapScreen(0, 0, ScreenWidth, 300.0f);

	const float Dt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
	m_AppearAmount += (1.0f - m_AppearAmount) * (1.0f - expf(-8.0f * Dt));
	if(m_AppearAmount > 0.999f)
		m_AppearAmount = 1.0f;
	m_SelectionPulse = max(0.0f, m_SelectionPulse - Dt * 1.8f);
	const float Appear = clamp(m_AppearAmount, 0.0f, 1.0f);

	if(CategoryVoteCount(m_ActiveCategory) <= 0)
	{
		for(int Category = 0; Category < NUM_GAMEVOTE_CATEGORIES; Category++)
		{
			if(CategoryVoteCount(Category) > 0)
			{
				m_ActiveCategory = Category;
				break;
			}
		}
	}
	if(m_Focused < 0 || m_Focused >= m_GameVoteCount || !m_aGameVoteDetails[m_Focused].m_Valid ||
	   VoteCategory(m_Focused) != m_ActiveCategory)
		m_Focused = FirstVoteInCategory(m_ActiveCategory);

	m_SelectorMouse.x = clamp(m_SelectorMouse.x, 3.0f, ScreenWidth - 5.0f);
	m_SelectorMouse.y = clamp(m_SelectorMouse.y, 3.0f, 295.0f);
	Graphics()->BlendNormal();

	const vec4 ColorAccent = CMenus::ThemeAccent();
	const vec4 ColorText = CMenus::ThemeText();
	const vec4 ColorMuted = vec4(CMenus::ThemeText().r * 0.48f + CMenus::ThemeBgPanel().r * 0.52f,
								 CMenus::ThemeText().g * 0.48f + CMenus::ThemeBgPanel().g * 0.52f,
								 CMenus::ThemeText().b * 0.48f + CMenus::ThemeBgPanel().b * 0.52f,
								 1.0f);
	const vec4 ColorBgDeep = CMenus::ThemeBgDeep();
	const vec4 ColorBgPanel = CMenus::ThemeBgPanel();
	const vec4 ColorBgInset = CMenus::ThemeBgInset();

	auto CategoryColor = [&](int Category)
	{
		switch(Category)
		{
			case GAMEVOTE_CATEGORY_PVE:
				return vec4(0.28f, 0.78f, 0.58f, 1.0f);
			case GAMEVOTE_CATEGORY_TEAM:
				return vec4(0.34f, 0.64f, 0.96f, 1.0f);
			case GAMEVOTE_CATEGORY_SOLO:
				return vec4(0.96f, 0.61f, 0.28f, 1.0f);
			default:
				return vec4(0.72f, 0.48f, 0.94f, 1.0f);
		}
	};

	auto DrawRect = [&](const CUIRect &Rect, vec4 Color, float Rounding)
	{
		if(Rect.w <= 0.0f || Rect.h <= 0.0f || Color.a <= 0.0f)
			return;
		RenderTools()->DrawUIRect(&Rect, Color, CUI::CORNER_ALL, min(Rounding, min(Rect.w, Rect.h) * 0.5f));
	};

	auto DrawGradientRect = [&](float X, float Y, float Width, float Height, vec4 Top, vec4 Bottom)
	{
		if(Width <= 0.0f || Height <= 0.0f)
			return;
		Graphics()->TextureSet(-1);
		Graphics()->QuadsBegin();
		IGraphics::CColorVertex aColors[4] = {IGraphics::CColorVertex(0, Top.r, Top.g, Top.b, Top.a),
											  IGraphics::CColorVertex(1, Top.r, Top.g, Top.b, Top.a),
											  IGraphics::CColorVertex(2, Bottom.r, Bottom.g, Bottom.b, Bottom.a),
											  IGraphics::CColorVertex(3, Bottom.r, Bottom.g, Bottom.b, Bottom.a)};
		Graphics()->SetColorVertex(aColors, 4);
		IGraphics::CFreeformItem Item(X, Y, X + Width, Y, X, Y + Height, X + Width, Y + Height);
		Graphics()->QuadsDrawFreeform(&Item, 1);
		Graphics()->QuadsEnd();
	};

	auto DrawCenteredText = [&](float CenterX, float Y, float FontSize, const char *pText, vec4 Color)
	{
		const float Width = TextRender()->TextWidth(0, FontSize, pText, -1);
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.42f * Color.a);
		TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a);
		TextRender()->Text(0, CenterX - Width * 0.5f, Y, FontSize, pText, -1);
	};

	auto DrawFitText =
		[&](float X, float Y, float MaxWidth, float FontSize, float MinFontSize, const char *pText, vec4 Color)
	{
		while(FontSize > MinFontSize && TextRender()->TextWidth(0, FontSize, pText, -1) > MaxWidth)
			FontSize -= 0.25f;
		TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.42f * Color.a);
		TextRender()->TextColor(Color.r, Color.g, Color.b, Color.a);
		TextRender()->Text(0, X, Y, FontSize, pText, -1);
	};

	auto MouseInside = [&](const CUIRect &Rect)
	{
		return m_SelectorMouse.x >= Rect.x && m_SelectorMouse.x <= Rect.x + Rect.w && m_SelectorMouse.y >= Rect.y &&
			   m_SelectorMouse.y <= Rect.y + Rect.h;
	};

	DrawGradientRect(0.0f,
					 0.0f,
					 ScreenWidth,
					 300.0f,
					 vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.94f * Appear),
					 vec4(ColorBgPanel.r, ColorBgPanel.g, ColorBgPanel.b, 0.96f * Appear));

	DrawCenteredText(ScreenWidth * 0.5f,
					 9.0f,
					 10.5f,
					 Localize("Choose the next mode"),
					 vec4(ColorText.r, ColorText.g, ColorText.b, Appear));

	int Time = m_TimeLeft + (m_TimeLeftTick - Client()->GameTick()) / Client()->GameTickSpeed();
	const bool ChangingMap = Time < 0;
	Time = max(0, Time);
	char aTimer[64];
	if(ChangingMap)
		str_copy(aTimer, Localize("Server is changing map"), sizeof(aTimer));
	else
		str_format(aTimer, sizeof(aTimer), Localize("Vote ends in %d..."), Time);
	const vec4 TimerColor = Time <= 5 && !ChangingMap ? CMenus::ThemeDanger() : ColorAccent;
	const float TimerFontSize = 6.2f;
	const float TimerTextWidth = TextRender()->TextWidth(0, TimerFontSize, aTimer, -1);
	CUIRect TimerRect = {ScreenWidth * 0.5f - TimerTextWidth * 0.5f - 9.0f, 29.0f, TimerTextWidth + 18.0f, 15.0f};
	DrawRect(TimerRect, vec4(ColorBgInset.r, ColorBgInset.g, ColorBgInset.b, 0.94f * Appear), 7.5f);
	DrawCenteredText(
		ScreenWidth * 0.5f, 32.0f, TimerFontSize, aTimer, vec4(TimerColor.r, TimerColor.g, TimerColor.b, Appear));

	const float TimerBarWidth = min(120.0f, ScreenWidth * 0.30f);
	const float TimerRatio = m_VoteDuration > 0 ? clamp(Time / (float)m_VoteDuration, 0.0f, 1.0f) : 0.0f;
	CUIRect TimerBar = {ScreenWidth * 0.5f - TimerBarWidth * 0.5f, 48.0f, TimerBarWidth, 2.0f};
	DrawRect(TimerBar, vec4(ColorBgPanel.r, ColorBgPanel.g, ColorBgPanel.b, 0.72f * Appear), 1.0f);
	if(TimerRatio > 0.0f)
	{
		TimerBar.w *= TimerRatio;
		DrawRect(TimerBar, vec4(TimerColor.r, TimerColor.g, TimerColor.b, 0.95f * Appear), 1.0f);
	}

	const float StageMargin = ScreenWidth < 440.0f ? 9.0f : 14.0f;
	CUIRect Stage = {StageMargin, 56.0f, ScreenWidth - StageMargin * 2.0f, 220.0f};
	CUIRect StageShadow = {Stage.x, Stage.y + 3.0f, Stage.w, Stage.h};
	DrawRect(StageShadow, vec4(0.0f, 0.0f, 0.0f, 0.40f * Appear), 12.0f);
	DrawRect(Stage, vec4(ColorBgInset.r, ColorBgInset.g, ColorBgInset.b, 0.94f * Appear), 12.0f);

	int aCategories[NUM_GAMEVOTE_CATEGORIES];
	int CategoryCount = 0;
	for(int Category = 0; Category < NUM_GAMEVOTE_CATEGORIES; Category++)
		if(CategoryVoteCount(Category) > 0)
			aCategories[CategoryCount++] = Category;

	const float TabGap = 3.0f;
	const float TabsX = Stage.x + 7.0f;
	const float TabsY = Stage.y + 7.0f;
	const float TabsWidth = Stage.w - 14.0f;
	const float TabWidth = (TabsWidth - TabGap * max(0, CategoryCount - 1)) / max(1, CategoryCount);
	int HoveredCategory = -1;
	for(int Slot = 0; Slot < CategoryCount; Slot++)
	{
		const int Category = aCategories[Slot];
		CUIRect Tab = {TabsX + Slot * (TabWidth + TabGap), TabsY, TabWidth, 24.0f};
		if(MouseInside(Tab))
			HoveredCategory = Category;
	}

	CUIRect Content = {Stage.x + 7.0f, Stage.y + 36.0f, Stage.w - 14.0f, 153.0f};
	const bool WideLayout = Content.w >= 440.0f;
	const int Columns = WideLayout ? 3 : 2;
	const int PageCapacity = Columns * 2;
	m_PageCapacity = PageCapacity;
	int aVotes[MAX_GAME_VOTES];
	int VoteCount = 0;
	int FocusPosition = 0;
	for(int d = 0; d < m_DisplayCount; d++)
	{
		const int i = m_aDisplayOrder[d];
		if(!m_aGameVoteDetails[i].m_Valid || VoteCategory(i) != m_ActiveCategory)
			continue;
		if(i == m_Focused)
			FocusPosition = VoteCount;
		aVotes[VoteCount++] = i;
	}
	const int Page = VoteCount > 0 ? FocusPosition / PageCapacity : 0;
	const int PageStart = Page * PageCapacity;
	const int PageCount = min(PageCapacity, max(0, VoteCount - PageStart));
	const int Rows = max(1, (PageCount + Columns - 1) / Columns);
	const float CardGap = 5.0f;
	const float CardWidth = (Content.w - CardGap * (Columns - 1)) / Columns;
	const float CardHeight = min(70.0f, (Content.h - CardGap * (Rows - 1)) / Rows);
	const float CardsHeight = Rows * CardHeight + (Rows - 1) * CardGap;
	const float CardsY = Content.y + max(0.0f, (Content.h - CardsHeight) * 0.5f);

	CUIRect aCardRects[6];
	int HoveredVote = -1;
	for(int Slot = 0; Slot < PageCount; Slot++)
	{
		const int Column = Slot % Columns;
		const int Row = Slot / Columns;
		aCardRects[Slot] = {
			Content.x + Column * (CardWidth + CardGap), CardsY + Row * (CardHeight + CardGap), CardWidth, CardHeight};
		if(MouseInside(aCardRects[Slot]))
			HoveredVote = aVotes[PageStart + Slot];
	}

	// Page controls let mouse users browse every playable mode in the category
	// instead of being stuck on the first page of cards.
	const int TotalPages = max(1, (VoteCount + PageCapacity - 1) / PageCapacity);
	CUIRect PageLeftRect = {0, 0, 0, 0};
	CUIRect PageRightRect = {0, 0, 0, 0};
	bool PageLeftHovered = false;
	bool PageRightHovered = false;
	if(VoteCount > PageCapacity)
	{
		const float PageY = Stage.y + Stage.h - 28.0f;
		PageLeftRect = {Stage.x + Stage.w - 76.0f, PageY, 16.0f, 16.0f};
		PageRightRect = {Stage.x + Stage.w - 24.0f, PageY, 16.0f, 16.0f};
		PageLeftHovered = MouseInside(PageLeftRect);
		PageRightHovered = MouseInside(PageRightRect);
	}

	if(m_MouseTrigger)
	{
		if(HoveredCategory >= 0)
		{
			m_ActiveCategory = HoveredCategory;
			if(m_Selected >= 0 && VoteCategory(m_Selected) == HoveredCategory)
				m_Focused = m_Selected;
			else
				m_Focused = FirstVoteInCategory(HoveredCategory);
		}
		else if(HoveredVote >= 0)
		{
			m_Focused = HoveredVote;
			m_Selected = HoveredVote;
			SendVote();
		}
		else if(VoteCount > PageCapacity)
		{
			if(PageLeftHovered && Page > 0)
				m_Focused = aVotes[(Page - 1) * PageCapacity];
			else if(PageRightHovered && Page + 1 < TotalPages)
				m_Focused = aVotes[min(VoteCount - 1, (Page + 1) * PageCapacity)];
		}
		m_MouseTrigger = false;
	}

	for(int Slot = 0; Slot < CategoryCount; Slot++)
	{
		const int Category = aCategories[Slot];
		const bool Active = Category == m_ActiveCategory;
		const bool Hovered = Category == HoveredCategory;
		const vec4 CategoryAccent = CategoryColor(Category);
		CUIRect Tab = {TabsX + Slot * (TabWidth + TabGap), TabsY, TabWidth, 24.0f};
		DrawRect(Tab,
				 Active ? vec4(CategoryAccent.r, CategoryAccent.g, CategoryAccent.b, 0.24f * Appear)
						: vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, (Hovered ? 0.88f : 0.66f) * Appear),
				 6.0f);
		if(Active)
		{
			CUIRect Indicator = {Tab.x + 8.0f, Tab.y + Tab.h - 2.0f, Tab.w - 16.0f, 2.0f};
			DrawRect(Indicator, vec4(CategoryAccent.r, CategoryAccent.g, CategoryAccent.b, 0.98f * Appear), 1.0f);
		}

		char aCategory[64];
		str_copy(aCategory, Localize(GameVoteCategoryName(Category)), sizeof(aCategory));
		float CategoryFontSize = 6.7f;
		while(CategoryFontSize > 5.0f && TextRender()->TextWidth(0, CategoryFontSize, aCategory, -1) > Tab.w - 8.0f)
			CategoryFontSize -= 0.25f;
		DrawCenteredText(Tab.x + Tab.w * 0.5f,
						 Tab.y + 7.0f,
						 CategoryFontSize,
						 aCategory,
						 vec4((Active ? ColorText : ColorMuted).r,
							  (Active ? ColorText : ColorMuted).g,
							  (Active ? ColorText : ColorMuted).b,
							  Appear));
	}

	DrawRect(Content, vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.54f * Appear), 8.0f);
	const vec4 ActiveAccent = CategoryColor(m_ActiveCategory);
	CUIRect ContentAccent = {Content.x, Content.y, 2.0f, Content.h};
	DrawRect(ContentAccent, vec4(ActiveAccent.r, ActiveAccent.g, ActiveAccent.b, 0.78f * Appear), 1.0f);

	static const char *s_apCategoryMarks[NUM_GAMEVOTE_CATEGORIES] = {"PVE", "VS", "FFA", "FUN"};
	for(int Slot = 0; Slot < PageCount; Slot++)
	{
		const int Vote = aVotes[PageStart + Slot];
		const CGameVoteDetails &Details = m_aGameVoteDetails[Vote];
		CUIRect Card = aCardRects[Slot];
		const bool Focused = Vote == m_Focused;
		const bool Selected = Vote == m_Selected;
		const bool Hovered = Vote == HoveredVote;
		const float Pulse = m_SelectionPulse * (0.5f + 0.5f * sinf((1.0f - m_SelectionPulse) * 18.0f));
		const vec4 Accent = CategoryColor(VoteCategory(Vote));

		if(Hovered)
			Card.y -= 1.0f;
		CUIRect CardBorder = {Card.x - 1.0f, Card.y - 1.0f, Card.w + 2.0f, Card.h + 2.0f};
		DrawRect(CardBorder,
				 vec4(Accent.r,
					  Accent.g,
					  Accent.b,
					  (Selected ? 0.92f : (Focused ? 0.70f : (Hovered ? 0.48f : 0.16f))) * Appear + Pulse * 0.12f),
				 7.0f);
		DrawRect(Card,
				 vec4(ColorBgPanel.r, ColorBgPanel.g, ColorBgPanel.b, (Focused || Hovered ? 0.98f : 0.88f) * Appear),
				 6.0f);
		CUIRect Stripe = {Card.x, Card.y, 3.0f, Card.h};
		DrawRect(Stripe, vec4(Accent.r, Accent.g, Accent.b, (Focused || Selected ? 0.95f : 0.52f) * Appear), 1.5f);

		// Game vote previews are authored at 2:1. Keep that aspect ratio and
		// vertically center the image together with its vote badge.
		const float PreviewWidth = clamp(Card.w * 0.34f, 48.0f, 58.0f);
		const float PreviewHeight = PreviewWidth * 0.5f;
		const float VoteBadgeHeight = 9.0f;
		const float PreviewGroupHeight = PreviewHeight + 4.0f + VoteBadgeHeight;
		CUIRect Preview = {Card.x + 8.0f, Card.y + (Card.h - PreviewGroupHeight) * 0.5f, PreviewWidth, PreviewHeight};
		DrawRect(Preview, vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.88f * Appear), 4.0f);
		if(Details.m_Texture >= 0)
		{
			Graphics()->TextureSet(Details.m_Texture);
			Graphics()->QuadsBegin();
			const float Brightness = Focused || Selected ? 0.92f : 0.68f;
			Graphics()->SetColor(Brightness, Brightness, Brightness, Appear);
			Graphics()->QuadsSetSubsetFree(0, 0, 1, 0, 0, 1, 1, 1);
			IGraphics::CFreeformItem Image(Preview.x,
										   Preview.y,
										   Preview.x + Preview.w,
										   Preview.y,
										   Preview.x,
										   Preview.y + Preview.h,
										   Preview.x + Preview.w,
										   Preview.y + Preview.h);
			Graphics()->QuadsDrawFreeform(&Image, 1);
			Graphics()->QuadsEnd();
		}
		else
		{
			float MarkSize = 6.0f;
			while(MarkSize > 4.0f &&
				  TextRender()->TextWidth(0, MarkSize, s_apCategoryMarks[VoteCategory(Vote)], -1) > Preview.w - 6.0f)
				MarkSize -= 0.25f;
			DrawCenteredText(Preview.x + Preview.w * 0.5f,
							 Preview.y + Preview.h * 0.5f - MarkSize * 0.55f,
							 MarkSize,
							 s_apCategoryMarks[VoteCategory(Vote)],
							 vec4(Accent.r, Accent.g, Accent.b, 0.92f * Appear));
		}

		char aVotesText[24];
		str_format(aVotesText, sizeof(aVotesText), "%d", Details.m_Votes);
		const float VoteBadgeWidth = min(28.0f, Preview.w - 6.0f);
		CUIRect VoteBadge = {Preview.x + (Preview.w - VoteBadgeWidth) * 0.5f,
							 Preview.y + Preview.h + 4.0f,
							 VoteBadgeWidth,
							 VoteBadgeHeight};
		DrawRect(VoteBadge, vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.88f * Appear), 5.0f);
		DrawCenteredText(VoteBadge.x + VoteBadge.w * 0.5f,
						 VoteBadge.y + 1.5f,
						 5.0f,
						 aVotesText,
						 vec4(ColorText.r, ColorText.g, ColorText.b, Appear));

		const float TextX = Preview.x + Preview.w + 7.0f;
		const float TextRight = Card.x + Card.w - 7.0f;
		DrawFitText(TextX,
					Card.y + 8.0f,
					max(10.0f, TextRight - TextX - 12.0f),
					7.8f,
					5.4f,
					Details.m_aName,
					vec4(ColorText.r, ColorText.g, ColorText.b, Appear));
		char aShortcut[8];
		if(Slot < 9)
		{
			str_format(aShortcut, sizeof(aShortcut), "%d", Slot + 1);
			DrawCenteredText(Card.x + Card.w - 8.0f,
							 Card.y + 7.0f,
							 4.8f,
							 aShortcut,
							 vec4(ColorMuted.r, ColorMuted.g, ColorMuted.b, 0.82f * Appear));
		}

	}

	if(VoteCount > PageCapacity)
	{
		const float PageY = Stage.y + Stage.h - 28.0f;
		const float PageButtonSize = 16.0f;
		CUIRect PageLeft = {Stage.x + Stage.w - 76.0f, PageY, PageButtonSize, PageButtonSize};
		CUIRect PageRight = {Stage.x + Stage.w - 24.0f, PageY, PageButtonSize, PageButtonSize};
		const bool LeftEnabled = Page > 0;
		const bool RightEnabled = Page + 1 < TotalPages;
		DrawRect(PageLeft,
				 vec4(ColorBgDeep.r,
					  ColorBgDeep.g,
					  ColorBgDeep.b,
					  (PageLeftHovered && LeftEnabled ? 0.94f : 0.72f) * Appear),
				 4.0f);
		DrawRect(PageRight,
				 vec4(ColorBgDeep.r,
					  ColorBgDeep.g,
					  ColorBgDeep.b,
					  (PageRightHovered && RightEnabled ? 0.94f : 0.72f) * Appear),
				 4.0f);
		DrawCenteredText(PageLeft.x + PageLeft.w * 0.5f,
						 PageLeft.y + 1.0f,
						 6.5f,
						 "<",
						 vec4((LeftEnabled ? ColorText : ColorMuted).r,
							  (LeftEnabled ? ColorText : ColorMuted).g,
							  (LeftEnabled ? ColorText : ColorMuted).b,
							  Appear));
		DrawCenteredText(PageRight.x + PageRight.w * 0.5f,
						 PageRight.y + 1.0f,
						 6.5f,
						 ">",
						 vec4((RightEnabled ? ColorText : ColorMuted).r,
							  (RightEnabled ? ColorText : ColorMuted).g,
							  (RightEnabled ? ColorText : ColorMuted).b,
							  Appear));
		char aPage[32];
		str_format(aPage, sizeof(aPage), "%d / %d", Page + 1, TotalPages);
		DrawCenteredText(Stage.x + Stage.w - 50.0f,
						 Stage.y + Stage.h - 23.0f,
						 5.5f,
						 aPage,
						 vec4(ColorMuted.r, ColorMuted.g, ColorMuted.b, Appear));
	}

	const char *pHint = Localize("Tab / LB / RB: category · Arrows / Wheel: mode · A / Enter: vote");
	float HintSize = 5.7f;
	while(HintSize > 4.2f && TextRender()->TextWidth(0, HintSize, pHint, -1) > Stage.w - 20.0f)
		HintSize -= 0.25f;
	DrawCenteredText(ScreenWidth * 0.5f,
					 Stage.y + Stage.h - 10.0f,
					 HintSize,
					 pHint,
					 vec4(ColorMuted.r, ColorMuted.g, ColorMuted.b, Appear));

	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);
	RenderMouse();
	if(m_DebugScreenshotFrames > 0 && Client()->State() == IClient::STATE_ONLINE && --m_DebugScreenshotFrames == 0)
		Graphics()->TakeScreenshot(0);
}
void CGameVoteDisplay::OnMessage(int MsgType, void *pRawMsg)
{
	if(MsgType == NETMSGTYPE_SV_GAMEVOTESTATUS)
	{
		CNetMsg_Sv_GameVoteStatus *pMsg = (CNetMsg_Sv_GameVoteStatus *)pRawMsg;
		if(pMsg->m_Index >= 0 && pMsg->m_Index < MAX_GAME_VOTES)
			m_aGameVoteDetails[pMsg->m_Index].m_Votes = pMsg->m_Votes;
		m_LastVoteMessageTime = time_get();
	}

	if(MsgType == NETMSGTYPE_SV_GAMEVOTE)
	{
		CNetMsg_Sv_GameVote *pMsg = (CNetMsg_Sv_GameVote *)pRawMsg;
		m_LastVoteMessageTime = time_get();

		int i = pMsg->m_Index;
		if(i < 0 || i >= MAX_GAME_VOTES)
			return;

		m_TimeLeft = pMsg->m_TimeLeft;
		m_TimeLeftTick = Client()->GameTick();
		m_VoteDuration = max(m_VoteDuration, pMsg->m_TimeLeft);

		if(m_aGameVoteDetails[i].m_Valid)
			return;

		const bool FirstVote = m_GameVoteCount == 0;
		if(FirstVote)
			m_SelectorMouse = vec2(150.0f * Graphics()->ScreenAspect(), 150.0f);

		m_aGameVoteDetails[i].m_Valid = true;
		m_aGameVoteDetails[i].m_Texture =
			m_pClient->m_pSkins->GetGameVote(m_pClient->m_pSkins->FindGameVote(pMsg->m_pImage))->m_Texture;

		str_copy(m_aGameVoteDetails[i].m_aName, pMsg->m_pName, sizeof(m_aGameVoteDetails[i].m_aName));
		str_copy(
			m_aGameVoteDetails[i].m_aDescription, pMsg->m_pDescription, sizeof(m_aGameVoteDetails[i].m_aDescription));
		str_copy(m_aGameVoteDetails[i].m_aImage, pMsg->m_pImage, sizeof(m_aGameVoteDetails[i].m_aImage));

		m_GameVoteCount = max(m_GameVoteCount, i + 1);
		if(FirstVote)
		{
			m_ActiveCategory = VoteCategory(i);
			m_Focused = i;
		}

		{
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, 10.0f, TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = 300 * Graphics()->ScreenAspect();
			TextRender()->TextEx(&Cursor, m_aGameVoteDetails[i].m_aName, -1);
			m_aGameVoteDetails[i].m_NameWidth = Cursor.m_X / 2;
		}
		{
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, 6.0f, TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = 300 * Graphics()->ScreenAspect();
			TextRender()->TextEx(&Cursor, m_aGameVoteDetails[i].m_aDescription, -1);
			m_aGameVoteDetails[i].m_DescriptionWidth = Cursor.m_X / 2;
		}
		{
			CTextCursor Cursor;
			TextRender()->SetCursor(&Cursor, 0, 0, 10.0f, TEXTFLAG_STOP_AT_END);
			Cursor.m_LineWidth = 300 * Graphics()->ScreenAspect();
			TextRender()->TextEx(&Cursor, "0", -1);
			m_aGameVoteDetails[i].m_VotesWidth = Cursor.m_X / 2;
		}

		RebuildDisplayOrder();
	}
}
