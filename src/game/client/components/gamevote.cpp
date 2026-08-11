#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
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
	static const char *s_apNames[NUM_GAMEVOTE_CATEGORIES] = {"PvE", "PvP", "Mini games"};
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
	m_CurrentVote = -1;
	m_RecommendedCount = 0;
	m_ShowAllVotes = false;
	m_RowCount = 0;
	m_FocusRow = 0;
	m_FocusCol = 0;
	m_HoverCol = -1;
	for(int i = 0; i < NUM_GAMEVOTE_CATEGORIES; i++)
	{
		m_aRows[i] = -1;
		m_aColScroll[i] = 0.0f;
		m_aColVel[i] = 0.0f;
		m_aOverScroll[i] = 0.0f;
		m_aColFocusAlpha[i] = 0.0f;
	}
	m_DetailCardAppear = 0.0f;
	m_DetailCardVote = -1;
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
	if(pResult->NumArguments() > 1)
	{
		const int Forced = clamp(pResult->GetInteger(1), 0, pSelf->m_GameVoteCount - 1);
		pSelf->m_aGameVoteDetails[Forced].m_IsCurrentMode = true;
		pSelf->m_CurrentVote = Forced;
		pSelf->m_RecommendedCount = 0;
		for(int k = 1; k <= 3 && pSelf->m_RecommendedCount < 3; k++)
		{
			const int Idx = (Forced + k) % pSelf->m_GameVoteCount;
			if(Idx != Forced)
			{
				pSelf->m_aGameVoteDetails[Idx].m_RecommendedRank = k;
				pSelf->m_aRecommended[pSelf->m_RecommendedCount++] = Idx;
			}
		}
	}
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
	if(str_find_nocase(pImage, "ball") || str_find_nocase(pImage, "roam"))
		return GAMEVOTE_CATEGORY_ARCADE;
	return GAMEVOTE_CATEGORY_PVP;
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

// Full-browser list geometry (Stage 232 - detail 62 - header 24 - button 34).
float CGameVoteDisplay::ListRowH()
{
	return 30.0f;
}

float CGameVoteDisplay::ListAreaH()
{
	return 232.0f - 8.0f - 62.0f - 6.0f - 24.0f - 6.0f - 34.0f;
}

// W/S: moves the focus up/down inside the focused column (wrapping).
void CGameVoteDisplay::MoveFocusRow(int Direction)
{
	const int Count = RowVoteCount(m_FocusCol);
	if(Count <= 0)
		return;
	m_FocusRow = (m_FocusRow + Direction + Count) % Count;
	EnsureFocusedRowVisible();
}

// A/D: moves the focus between columns (categories), keeping the row.
void CGameVoteDisplay::MoveFocusCol(int Direction)
{
	if(m_RowCount <= 1)
		return;
	const int OldRow = m_FocusRow;
	m_FocusCol = (m_FocusCol + Direction + m_RowCount) % m_RowCount;
	m_FocusRow = clamp(OldRow, 0, max(0, RowVoteCount(m_FocusCol) - 1));
	EnsureFocusedRowVisible();
}

// Keeps the focused row inside the visible window of its column. Called only
// when the focus moved; wheel scrolling is free and never pulled back.
void CGameVoteDisplay::EnsureFocusedRowVisible()
{
	const int Count = RowVoteCount(m_FocusCol);
	if(Count <= 0)
		return;
	const int Visible = max(1, (int)(ListAreaH() / ListRowH()));
	if(m_FocusRow < (int)m_aColScroll[m_FocusCol])
	{
		m_aColScroll[m_FocusCol] = (float)m_FocusRow;
		m_aColVel[m_FocusCol] = 0.0f;
	}
	else if(m_FocusRow >= (int)m_aColScroll[m_FocusCol] + Visible)
	{
		m_aColScroll[m_FocusCol] = (float)(m_FocusRow - Visible + 1);
		m_aColVel[m_FocusCol] = 0.0f;
	}
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

// Vote whose local mode matches the server game type ("coop" -> Invasion),
// or -1. Only used when the server sent no recommendation flags.
int CGameVoteDisplay::FindCurrentVote() const
{
	CServerInfo Info;
	Client()->GetServerInfo(&Info);
	if(Info.m_aGameType[0] == 0)
		return -1;

	for(int i = 1; i < LOCAL_MODE_COUNT; i++)
	{
		const CLocalGameMode &Mode = s_aLocalGameModes[i];
		if(!Mode.m_pGameType || str_comp(Info.m_aGameType, Mode.m_pGameType) != 0)
			continue;
		for(int d = 0; d < m_DisplayCount; d++)
		{
			const int Vote = m_aDisplayOrder[d];
			const CGameVoteDetails &Details = m_aGameVoteDetails[Vote];
			if(!Details.m_Valid)
				continue;
			if(GameVoteLocalModeByName(Details.m_aName) == i ||
				(Mode.m_pGameVoteImage && str_comp(Details.m_aImage, Mode.m_pGameVoteImage) == 0))
				return Vote;
		}
	}
	return -1;
}

// Fallback for servers that do not send recommendation flags: picks the
// current mode plus three deterministic (tick-seeded) picks, category first.
void CGameVoteDisplay::BuildFallbackRecommendations()
{
	m_RecommendedCount = 0;
	m_CurrentVote = FindCurrentVote();

	int aCandidates[MAX_GAME_VOTES];
	int CandidateCount = 0;
	for(int d = 0; d < m_DisplayCount; d++)
	{
		const int Vote = m_aDisplayOrder[d];
		if(!m_aGameVoteDetails[Vote].m_Valid || Vote == m_CurrentVote)
			continue;
		if(VoteCategory(Vote) == m_ActiveCategory)
			aCandidates[CandidateCount++] = Vote;
	}
	const int CategoryCount = CandidateCount;
	for(int d = 0; d < m_DisplayCount && CandidateCount < MAX_GAME_VOTES; d++)
	{
		const int Vote = m_aDisplayOrder[d];
		if(!m_aGameVoteDetails[Vote].m_Valid || Vote == m_CurrentVote)
			continue;
		if(VoteCategory(Vote) != m_ActiveCategory)
			aCandidates[CandidateCount++] = Vote;
	}

	const int Seed = m_TimeLeftTick > 0 ? m_TimeLeftTick : 1;
	unsigned State = (unsigned)(Seed * 1103515245 + 12345);
	auto NextRand = [&]()
	{
		State = State * 1103515245 + 12345;
		return (State / 65536) % 32768;
	};
	for(int i = CandidateCount - 1; i > 0; i--)
	{
		const int j = (int)(NextRand() % (unsigned)(i + 1));
		const int Tmp = aCandidates[i];
		aCandidates[i] = aCandidates[j];
		aCandidates[j] = Tmp;
	}

	const int Want = CandidateCount > CategoryCount ? 3 : min(3, CandidateCount);
	for(int i = 0; i < Want && i < 3; i++)
	{
		m_aRecommended[m_RecommendedCount++] = aCandidates[i];
		m_aGameVoteDetails[aCandidates[i]].m_RecommendedRank = i + 1;
	}
	if(m_CurrentVote >= 0)
		m_aGameVoteDetails[m_CurrentVote].m_IsCurrentMode = true;
}

// Vote index for a slot of the recommended carousel (slot 0 = continue,
// 1-3 = recommended). Returns -1 for invalid slots.
int CGameVoteDisplay::FocusSlotVote(int Slot) const
{
	if(Slot == 0 && m_CurrentVote >= 0)
		return m_CurrentVote;
	const int Offset = m_CurrentVote >= 0 ? 1 : 0;
	if(Slot >= Offset && Slot - Offset < m_RecommendedCount)
		return m_aRecommended[Slot - Offset];
	return -1;
}

// Number of votes in a grid row (category).
int CGameVoteDisplay::RowVoteCount(int Row) const
{
	if(Row < 0 || Row >= m_RowCount)
		return 0;
	const int Category = m_aRows[Row];
	int Count = 0;
	for(int d = 0; d < m_DisplayCount; d++)
	{
		const int i = m_aDisplayOrder[d];
		if(m_aGameVoteDetails[i].m_Valid && VoteCategory(i) == Category)
			Count++;
	}
	return Count;
}

// Vote index at a grid position (row, column). Returns -1 when invalid.
int CGameVoteDisplay::VoteAtRowSlot(int Row, int Col) const
{
	if(Row < 0 || Row >= m_RowCount)
		return -1;
	const int Category = m_aRows[Row];
	int Count = 0;
	for(int d = 0; d < m_DisplayCount; d++)
	{
		const int i = m_aDisplayOrder[d];
		if(!m_aGameVoteDetails[i].m_Valid || VoteCategory(i) != Category)
			continue;
		if(Count == Col)
			return i;
		Count++;
	}
	return -1;
}

bool CGameVoteDisplay::OnInput(IInput::CEvent Event)
{
	if(!IsActive())
		return false;

	if(Event.m_Flags & IInput::FLAG_PRESS)
	{
		const bool LeftPress = Event.m_Key == KEY_LEFT || Event.m_Key == KEY_A || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_LEFT;
		const bool RightPress = Event.m_Key == KEY_RIGHT || Event.m_Key == KEY_D || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_RIGHT;
		const bool UpPress = Event.m_Key == KEY_UP || Event.m_Key == KEY_W || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_UP;
		const bool DownPress = Event.m_Key == KEY_DOWN || Event.m_Key == KEY_S || Event.m_Key == KEY_GAMEPAD_BUTTON_DPAD_DOWN;

		if(Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
		{
			const int Direction = Event.m_Key == KEY_MOUSE_WHEEL_UP ? -1 : 1;
			if(m_ShowAllVotes)
			{
				// No hovered list -> wheel does nothing to the lists.
				if(m_HoverCol < 0)
					return true;
				const int Count = RowVoteCount(m_HoverCol);
				if(Count > 0)
					m_aColVel[m_HoverCol] += Direction * 3.2f;
			}
			else
			{
				const int Count = (m_CurrentVote >= 0 ? 1 : 0) + m_RecommendedCount;
				if(Count > 0)
					m_FocusCol = (m_FocusCol + Direction + Count) % Count;
			}
			return true;
		}

		if(LeftPress || RightPress || UpPress || DownPress)
		{
			if(m_ShowAllVotes)
			{
				if(LeftPress || RightPress)
					MoveFocusCol(LeftPress ? -1 : 1);
				else if(UpPress || DownPress)
					MoveFocusRow(UpPress ? -1 : 1);
			}
			else
			{
				if(LeftPress || RightPress)
				{
					const int Count = (m_CurrentVote >= 0 ? 1 : 0) + m_RecommendedCount;
					if(Count > 0)
						m_FocusCol = (m_FocusCol + (LeftPress ? -1 : 1) + Count) % Count;
				}
				else if(DownPress)
					m_ShowAllVotes = true;
			}
			return true;
		}

		if(Event.m_Key == KEY_TAB || Event.m_Key == KEY_GAMEPAD_SHOULDER_RIGHT ||
		   Event.m_Key == KEY_GAMEPAD_SHOULDER_LEFT)
		{
			m_ShowAllVotes = !m_ShowAllVotes;
			return true;
		}

		if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER || Event.m_Key == KEY_SPACE ||
		   Event.m_Key == KEY_GAMEPAD_BUTTON_A)
		{
			const int Vote = m_ShowAllVotes ? VoteAtRowSlot(m_FocusRow, m_FocusCol) : FocusSlotVote(m_FocusCol);
			if(Vote >= 0 && m_aGameVoteDetails[Vote].m_Valid)
			{
				m_ActiveCategory = VoteCategory(Vote);
				m_Focused = Vote;
				m_Selected = Vote;
				SendVote();
				return true;
			}
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
			case GAMEVOTE_CATEGORY_PVP:
				return vec4(0.34f, 0.64f, 0.96f, 1.0f);
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

	// Clip helpers: Graphics()->ClipEnable works in SCREEN PIXELS, while this
	// overlay renders in the 0..ScreenWidth x 0..300 game coordinate space,
	// so the rect must be scaled before enabling the scissor.
	auto ClipRect = [&](const CUIRect &Rect)
	{
		const float XScale = Graphics()->ScreenWidth() / ScreenWidth;
		const float YScale = Graphics()->ScreenHeight() / 300.0f;
		Graphics()->ClipEnable((int)(Rect.x * XScale), (int)(Rect.y * YScale), (int)(Rect.w * XScale), (int)(Rect.h * YScale));
	};
	auto Unclip = [&]() { Graphics()->ClipDisable(); };

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
	CUIRect Stage = {StageMargin, 56.0f, ScreenWidth - StageMargin * 2.0f, 232.0f};

	// Shared 2:1 preview thumbnail renderer (with fallback for missing art).
	static const char *s_apCategoryMarks[NUM_GAMEVOTE_CATEGORIES] = {"PVE", "PVP", "FUN"};
	auto DrawPreview = [&](CUIRect Preview, int Vote, vec4 Accent, bool Bright)
		{
			DrawRect(Preview, vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.88f * Appear), 4.0f);
			const CGameVoteDetails &Details = m_aGameVoteDetails[Vote];
			if(Details.m_Texture >= 0)
			{
				Graphics()->TextureSet(Details.m_Texture);
				Graphics()->QuadsBegin();
				const float Brightness = Bright ? 0.92f : 0.68f;
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
		};
	if(!m_ShowAllVotes)
	{
		// ----- Recommended view: continue + 3 server picks -----
		const int SlotCount = (m_CurrentVote >= 0 ? 1 : 0) + m_RecommendedCount;
		m_FocusCol = SlotCount > 0 ? clamp(m_FocusCol, 0, SlotCount - 1) : 0;
		const int FocusedVote = FocusSlotVote(m_FocusCol);
		if(FocusedVote >= 0)
			m_Focused = FocusedVote;

		// Carousel animation (row-scroll slot 0 is reused as the carousel pos).
		{
			const float RecDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
			const float Target = (float)m_FocusCol;
			m_aColScroll[0] += (Target - m_aColScroll[0]) * (1.0f - expf(-12.0f * RecDt));
			if(fabsf(m_aColScroll[0] - Target) < 0.0005f)
				m_aColScroll[0] = Target;
		}

		// Portrait cards: large 2:1 image, name, description (no overlap).
		const float RecSpacing = clamp(ScreenWidth * 0.24f, 104.0f, 132.0f);
		const float RecCenterY = Stage.y + Stage.h * 0.5f - 16.0f;
		const float RecHalfW = 72.0f;
		const float RecHalfH = 70.0f;
		auto GetRecLayout = [&](float Slot, vec2 &Center, float &Scale, float &Alpha) -> bool
		{
			const float Offset = Slot - m_aColScroll[0];
			if(fabsf(Offset) > 3.25f)
				return false;
			Scale = clamp(1.0f - fabsf(Offset) * 0.19f, 0.50f, 1.0f);
			Alpha = clamp(1.0f - fabsf(Offset) * 0.28f, 0.12f, 1.0f) * Appear;
			Center = vec2(ScreenWidth * 0.5f + Offset * RecSpacing, RecCenterY);
			return Center.x + RecHalfW * Scale > 0.0f && Center.x - RecHalfW * Scale < ScreenWidth;
		};

		int HoveredSlot = -1;
		for(int Slot = 0; Slot < SlotCount; Slot++)
		{
			vec2 Center;
			float Scale, Alpha;
			if(!GetRecLayout(Slot, Center, Scale, Alpha))
				continue;
			CUIRect CardRect = {Center.x - RecHalfW * Scale,
								Center.y - RecHalfH * Scale,
								RecHalfW * 2.0f * Scale,
								RecHalfH * 2.0f * Scale};
			if(MouseInside(CardRect))
			{
				HoveredSlot = Slot;
				break;
			}
		}

		// Bottom-center switch to the full browser.
		const float RecButtonW = min(240.0f, Stage.w - 24.0f);
		const float RecButtonH = 22.0f;
		CUIRect RecButtonRect = {Stage.x + (Stage.w - RecButtonW) * 0.5f, Stage.y + Stage.h - 34.0f, RecButtonW, RecButtonH};
		const bool RecButtonHovered = MouseInside(RecButtonRect);

		if(m_MouseTrigger)
		{
			if(HoveredSlot >= 0)
			{
				m_FocusCol = HoveredSlot;
				const int Vote = FocusSlotVote(HoveredSlot);
				if(Vote >= 0)
				{
					m_Focused = Vote;
					m_Selected = Vote;
					SendVote();
				}
			}
			else if(RecButtonHovered)
				m_ShowAllVotes = true;
			m_MouseTrigger = false;
		}

		// Panel.
		DrawRect({Stage.x, Stage.y + 3.0f, Stage.w, Stage.h}, vec4(0.0f, 0.0f, 0.0f, 0.40f * Appear), 12.0f);
		DrawRect(Stage, vec4(ColorBgInset.r, ColorBgInset.g, ColorBgInset.b, 0.94f * Appear), 12.0f);

		// Cards, banded back-to-front.
		auto DrawRecCard = [&](int Slot)
		{
			vec2 Center;
			float Scale, Alpha;
			if(!GetRecLayout(Slot, Center, Scale, Alpha))
				return;
			const int Vote = FocusSlotVote(Slot);
			if(Vote < 0)
				return;
			const CGameVoteDetails &Details = m_aGameVoteDetails[Vote];
			const bool Focused = Slot == m_FocusCol;
			const bool Hovered = Slot == HoveredSlot;
			const bool Selected = Vote == m_Selected;
			const vec4 Accent = CategoryColor(VoteCategory(Vote));
			const float Pulse = m_SelectionPulse * (0.5f + 0.5f * sinf((1.0f - m_SelectionPulse) * 18.0f));

			const float W = RecHalfW * 2.0f * Scale;
			const float H = RecHalfH * 2.0f * Scale;
			CUIRect Frame = {Center.x - W * 0.5f, Center.y - H * 0.5f, W, H};

			DrawRect({Frame.x + 2.0f, Frame.y + 4.0f, Frame.w, Frame.h}, vec4(0.0f, 0.0f, 0.0f, 0.42f * Alpha), 12.0f);
			if(Focused || Selected)
				DrawRect({Frame.x - 3.0f * Scale, Frame.y - 3.0f * Scale, Frame.w + 6.0f * Scale, Frame.h + 6.0f * Scale},
						 vec4(Accent.r, Accent.g, Accent.b, (0.13f + 0.18f * Pulse) * Alpha), 14.0f * Scale);
			DrawRect({Frame.x - 1.0f * Scale, Frame.y - 1.0f * Scale, Frame.w + 2.0f * Scale, Frame.h + 2.0f * Scale},
					 vec4(Accent.r, Accent.g, Accent.b, (Selected ? 0.90f : (Focused ? 0.78f : (Hovered ? 0.40f : 0.13f))) * Alpha),
					 11.5f * Scale);
			DrawRect(Frame, vec4(0.030f, 0.048f, 0.055f, 0.98f * Alpha), 10.5f * Scale);
			if(Focused || Selected)
				DrawRect({Frame.x + 5.0f * Scale, Frame.y, Frame.w - 10.0f * Scale, 1.0f * Scale},
						 vec4(Accent.r, Accent.g, Accent.b, 0.85f * Alpha), 1.0f);

			// Large 2:1 image on top.
			const float ImgW = Frame.w - 16.0f;
			const float ImgH = ImgW * 0.5f;
			CUIRect Preview = {Frame.x + 8.0f, Frame.y + 8.0f, ImgW, ImgH};
			DrawPreview(Preview, Vote, Accent, Focused);

			// Name and description, stacked with fixed offsets (no overlap).
			DrawFitText(Frame.x + 6.0f * Scale,
						Frame.y + ImgH + 15.0f * Scale,
						Frame.w - 12.0f * Scale,
						7.0f * Scale,
						5.2f * Scale,
						Details.m_aName,
						vec4(ColorText.r, ColorText.g, ColorText.b, Alpha));
			DrawFitText(Frame.x + 6.0f * Scale,
						Frame.y + ImgH + 31.0f * Scale,
						Frame.w - 12.0f * Scale,
						4.6f * Scale,
						3.8f * Scale,
						Details.m_aDescription,
						vec4(ColorMuted.r, ColorMuted.g, ColorMuted.b, Alpha));

			// CONTINUE badge (slot 0 = running mode).
			if(Slot == 0 && m_CurrentVote >= 0)
			{
				const float BadgeW = 52.0f * Scale;
				CUIRect Badge = {Frame.x + 8.0f * Scale, Frame.y + 8.0f * Scale, BadgeW, 13.0f * Scale};
				DrawRect(Badge, vec4(Accent.r, Accent.g, Accent.b, 0.88f * Alpha), 6.0f * Scale);
				DrawCenteredText(Badge.x + Badge.w * 0.5f,
								 Badge.y + 3.0f * Scale,
								 4.6f * Scale,
								 Localize("CONTINUE"),
								 vec4(0.0f, 0.0f, 0.0f, 0.85f * Alpha));
			}
			// Star badge (server recommendation, top-right of the image).
			if(Details.m_RecommendedRank > 0)
			{
				const float StarW = 15.0f * Scale;
				CUIRect Star = {Frame.x + Frame.w - 8.0f * Scale - StarW, Frame.y + 8.0f * Scale, StarW, 13.0f * Scale};
				DrawRect(Star, vec4(1.0f, 0.82f, 0.30f, 0.92f * Alpha), 6.0f * Scale);
				DrawCenteredText(Star.x + Star.w * 0.5f,
								 Star.y + 2.0f * Scale,
								 5.6f * Scale,
								 "*",
								 vec4(0.1f, 0.08f, 0.0f, 0.9f * Alpha));
			}
		};

		// Cards are clipped to the stage so the halves sticking out of the
		// left/right frame are masked off (only the in-frame half shows).
		ClipRect(Stage);
		for(int Band = 3; Band >= 0; Band--)
			for(int Slot = 0; Slot < SlotCount; Slot++)
				if(Slot != m_FocusCol && min(3, (int)fabsf(Slot - m_aColScroll[0])) == Band)
					DrawRecCard(Slot);
		if(m_FocusCol < SlotCount)
			DrawRecCard(m_FocusCol);
		Unclip();

		// "Show all modes" button.
		{
			const vec4 RecColor = RecButtonHovered ? ColorText : ColorMuted;
			DrawRect(RecButtonRect,
					 vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, (RecButtonHovered ? 0.92f : 0.70f) * Appear),
					 6.0f);
			DrawCenteredText(RecButtonRect.x + RecButtonRect.w * 0.5f,
							 RecButtonRect.y + 4.0f,
							 5.6f,
							 Localize("Show all modes"),
							 vec4(RecColor.r, RecColor.g, RecColor.b, Appear));
		}
	}
	else
	{
		const float BackButtonW = min(240.0f, Stage.w - 24.0f);
		const float BackButtonH = 22.0f;
		CUIRect BackButtonRect = {Stage.x + (Stage.w - BackButtonW) * 0.5f, Stage.y + Stage.h - 34.0f, BackButtonW, BackButtonH};
		const bool BackButtonHovered = MouseInside(BackButtonRect);
		if(m_MouseTrigger && BackButtonHovered)
		{
			m_ShowAllVotes = false;
			m_MouseTrigger = false;
		}

		// Build the column layout: one column per category that has votes.
		m_RowCount = 0;
		for(int Category = 0; Category < NUM_GAMEVOTE_CATEGORIES; Category++)
			if(CategoryVoteCount(Category) > 0)
				m_aRows[m_RowCount++] = Category;
		if(m_RowCount <= 0)
			return;

		// Focus sanity: m_FocusCol = column, m_FocusRow = row inside column.
		m_FocusCol = clamp(m_FocusCol, 0, m_RowCount - 1);
		m_FocusRow = clamp(m_FocusRow, 0, max(0, RowVoteCount(m_FocusCol) - 1));
		const int FocusedVote = VoteAtRowSlot(m_FocusCol, m_FocusRow);
		if(FocusedVote >= 0)
			m_Focused = FocusedVote;

		// Column geometry. Top of the stage shows a detail card for the
		// focused mode, then the category headers, then the lists.
		const float DetailTop = Stage.y + 8.0f;
		const float DetailH = 62.0f;
		const float HeaderH = 24.0f;
		const float ListTop = DetailTop + DetailH + 6.0f + HeaderH + 2.0f;
		const float ListH = ListAreaH();
		const float RowH = ListRowH();
		const float ColGap = 10.0f;
		const float ColsX = Stage.x + 14.0f;
		const float ColsW = Stage.w - 28.0f;
		const float ColW = (ColsW - ColGap * max(0, m_RowCount - 1)) / m_RowCount;

		// Inertial scroll: velocity decays, position clamps at the ends and
		// the overshoot springs back (rubber-band).
		{
			const float ScrollDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
			for(int Col = 0; Col < m_RowCount; Col++)
			{
				const int Count = RowVoteCount(Col);
				// MaxScroll must be in rows (Pos is), not pixels.
				const float MaxScroll = max(0.0f, Count - ListH / RowH);
				float &Pos = m_aColScroll[Col];
				float &Vel = m_aColVel[Col];
				float &Over = m_aOverScroll[Col];

				// Cap the wheel impulse so spam cannot launch the list.
				Vel = clamp(Vel, -12.0f, 12.0f);
				// Friction + integration.
				Vel *= expf(-9.0f * ScrollDt);
				Pos += Vel * ScrollDt;

				// Clamp + bounded overshoot; absorb outward velocity.
				float NewOver = 0.0f;
				if(Pos < 0.0f)
				{
					NewOver = clamp(Pos, -0.6f, 0.0f);
					Pos = 0.0f;
					Vel = 0.0f;
				}
				else if(MaxScroll <= 0.0f)
				{
					// Nothing to scroll: hold still.
					Pos = 0.0f;
					Vel = 0.0f;
				}
				else if(Pos > MaxScroll)
				{
					NewOver = clamp(Pos - MaxScroll, 0.0f, 0.6f);
					Pos = MaxScroll;
					Vel = 0.0f;
				}

				// Spring: the overshoot decays smoothly back to zero.
				Over += (NewOver - Over) * (1.0f - expf(-11.0f * ScrollDt));
				if(fabsf(Over) < 0.005f)
					Over = 0.0f;
			}
		}

		m_HoverCol = -1;
		int HoveredRow = -1;
		for(int Col = 0; Col < m_RowCount; Col++)
		{
			const float ColX = ColsX + Col * (ColW + ColGap);
			CUIRect ColRect = {ColX, ListTop, ColW, ListH};
			if(!MouseInside(ColRect))
				continue;
			m_HoverCol = Col;
			const int Count = RowVoteCount(Col);
			const float DisplayPos = m_aColScroll[Col] + m_aOverScroll[Col];
			for(int Row = 0; Row < Count; Row++)
			{
				const float RowY = ListTop + (Row - DisplayPos) * RowH;
				if(RowY + RowH < ListTop || RowY > ListTop + ListH)
					continue;
				CUIRect RowRect = {ColX, RowY, ColW, RowH};
				if(MouseInside(RowRect))
				{
					HoveredRow = Row;
					break;
				}
			}
			break;
		}

		if(m_MouseTrigger)
		{
			if(m_HoverCol >= 0 && HoveredRow >= 0)
			{
				m_FocusCol = m_HoverCol;
				m_FocusRow = HoveredRow;
				const int Vote = VoteAtRowSlot(m_FocusCol, m_FocusRow);
				if(Vote >= 0)
				{
					if(Vote == m_Focused)
					{
						m_Selected = Vote;
						SendVote();
					}
					else
						m_Focused = Vote;
				}
			}
			m_MouseTrigger = false;
		}

		// Panel (border is painted LAST so nothing sits on the frame).
		DrawRect({Stage.x, Stage.y + 3.0f, Stage.w, Stage.h}, vec4(0.0f, 0.0f, 0.0f, 0.40f * Appear), 12.0f);
		DrawRect(Stage, vec4(ColorBgInset.r, ColorBgInset.g, ColorBgInset.b, 0.94f * Appear), 12.0f);

		const float AniDt = clamp(Client()->RenderFrameTime(), 0.0f, 0.05f);
		for(int Col = 0; Col < m_RowCount; Col++)
		{
			const float TargetFocus = Col == m_FocusCol ? 1.0f : 0.0f;
			m_aColFocusAlpha[Col] += (TargetFocus - m_aColFocusAlpha[Col]) * (1.0f - expf(-10.0f * AniDt));
			if(fabsf(m_aColFocusAlpha[Col] - TargetFocus) < 0.005f)
				m_aColFocusAlpha[Col] = TargetFocus;
		}
		if(FocusedVote != m_DetailCardVote)
		{
			m_DetailCardVote = FocusedVote;
			m_DetailCardAppear = 0.0f;
		}
		m_DetailCardAppear = min(1.0f, m_DetailCardAppear + AniDt * 5.0f);
		const float Pulse = 0.5f + 0.5f * sinf(Client()->LocalTime() * 4.0f);

		if(FocusedVote >= 0)
		{
			const CGameVoteDetails &FDetails = m_aGameVoteDetails[FocusedVote];
			const vec4 FAccent = CategoryColor(VoteCategory(FocusedVote));
			const bool FRecommended = FDetails.m_RecommendedRank > 0;
			const float CardW = min(360.0f, Stage.w - 40.0f);
			const float CardSlide = 1.0f - m_DetailCardAppear;
			CUIRect DCard = {Stage.x + (Stage.w - CardW) * 0.5f, DetailTop + 5.0f * CardSlide, CardW, DetailH};
			// Body + accent border (border fades in with the slide).
			DrawRect({DCard.x + 1.0f, DCard.y + 2.0f, DCard.w, DCard.h}, vec4(0.0f, 0.0f, 0.0f, 0.35f * Appear), 8.0f);
			DrawRect({DCard.x - 1.0f, DCard.y - 1.0f, DCard.w + 2.0f, DCard.h + 2.0f},
					 vec4(FAccent.r, FAccent.g, FAccent.b, 0.55f * m_DetailCardAppear * Appear), 9.0f);
			DrawRect(DCard, vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, 0.96f * Appear), 8.0f);
			// 2:1 image on the left.
			const float DImgW = (DetailH - 12.0f) * 2.0f;
			const float DImgH = DetailH - 12.0f;
			CUIRect DImg = {DCard.x + 6.0f, DCard.y + 6.0f, DImgW, DImgH};
			DrawPreview(DImg, FocusedVote, FAccent, true);
			// Name (gold when recommended) + description on the right.
			const float DTextX = DImg.x + DImg.w + 12.0f;
			const float DTextW = DCard.x + DCard.w - 10.0f - DTextX;
			const vec4 DNameColor = FRecommended
				? vec4(1.0f, 0.82f, 0.30f, Appear)
				: vec4(ColorText.r, ColorText.g, ColorText.b, Appear);
			DrawFitText(DTextX, DCard.y + 8.0f, DTextW, 6.5f, 5.0f, FDetails.m_aName, DNameColor);
			// Ball's two map variants share a name; only the description differs.
			{
				CTextCursor Cursor;
				TextRender()->SetCursor(&Cursor, DTextX, DCard.y + 28.0f, 4.2f, TEXTFLAG_RENDER | TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = DTextW;
				TextRender()->TextColor(ColorMuted.r, ColorMuted.g, ColorMuted.b, Appear);
				TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.42f * Appear);
				TextRender()->TextEx(&Cursor, FDetails.m_aDescription, -1);
				TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
				TextRender()->TextOutlineColor(0.0f, 0.0f, 0.0f, 0.3f);
			}
			if(FDetails.m_IsCurrentMode)
			{
				CUIRect Tag = {DImg.x + DImg.w - 24.0f, DImg.y + 2.0f, 22.0f, 11.0f};
				DrawRect(Tag, vec4(FAccent.r, FAccent.g, FAccent.b, 0.92f * Appear), 5.0f);
				DrawCenteredText(Tag.x + Tag.w * 0.5f, Tag.y + 2.0f, 4.0f, Localize("CONTINUE"), vec4(0.0f, 0.0f, 0.0f, 0.9f * Appear));
			}
		}

		// ---- Category headers + lists. ----
		for(int Col = 0; Col < m_RowCount; Col++)
		{
			const float ColX = ColsX + Col * (ColW + ColGap);
			const int Category = m_aRows[Col];
			const vec4 ColAccent = CategoryColor(Category);

			{
				char aCategory[64];
				str_copy(aCategory, Localize(GameVoteCategoryName(Category)), sizeof(aCategory));
				const float FocusBlend = m_aColFocusAlpha[Col];
				const vec4 HeadColor = vec4(ColorText.r * FocusBlend + ColorMuted.r * (1.0f - FocusBlend),
											ColorText.g * FocusBlend + ColorMuted.g * (1.0f - FocusBlend),
											ColorText.b * FocusBlend + ColorMuted.b * (1.0f - FocusBlend),
											Appear);
				DrawCenteredText(ColX + ColW * 0.5f,
								 DetailTop + DetailH + 6.0f,
								 6.5f,
								 aCategory,
								 HeadColor);
				DrawRect({ColX + 4.0f, DetailTop + DetailH + 6.0f + 14.0f, ColW - 8.0f, 1.0f},
						 vec4(ColAccent.r, ColAccent.g, ColAccent.b, (0.30f + 0.60f * FocusBlend) * Appear),
						 1.0f);
			}

			ClipRect({ColX, ListTop, ColW, ListH});
			const int Count = RowVoteCount(Col);
			const float DisplayPos = m_aColScroll[Col] + m_aOverScroll[Col];
			for(int Row = 0; Row < Count; Row++)
			{
				const float RowY = ListTop + (Row - DisplayPos) * RowH;
				if(RowY + RowH < ListTop || RowY > ListTop + ListH)
					continue;
				const int Vote = VoteAtRowSlot(Col, Row);
				if(Vote < 0)
					continue;
				const CGameVoteDetails &Details = m_aGameVoteDetails[Vote];
				const bool Focused = Col == m_FocusCol && Row == m_FocusRow;
				const bool Hovered = Col == m_HoverCol && Row == HoveredRow;
				const vec4 Accent = CategoryColor(VoteCategory(Vote));
				const bool Recommended = Details.m_RecommendedRank > 0;
				CUIRect RowRect = {ColX, RowY, ColW, RowH};

				if(Focused || Hovered)
					DrawRect(RowRect,
							 vec4(Accent.r, Accent.g, Accent.b, (Hovered ? 0.18f : 0.22f + 0.10f * Pulse) * Appear),
							 4.0f);
				if(Focused)
					DrawRect({RowRect.x, RowRect.y, 2.5f + 1.5f * Pulse, RowRect.h},
							 vec4(Accent.r, Accent.g, Accent.b, (0.75f + 0.20f * Pulse) * Appear),
							 1.0f);

				// 2:1 thumbnail icon.
				const float IconH = RowH - 10.0f;
				const float IconW = IconH * 2.0f;
				CUIRect Icon = {RowRect.x + 4.0f, RowRect.y + 5.0f, IconW, IconH};
				DrawPreview(Icon, Vote, Accent, Focused);

				// Name (gold when recommended) + description below it, so
				// same-name variants (Ball maps) stay distinguishable.
				const float TextX = RowRect.x + IconW + 10.0f;
				const float TextW = RowRect.w - IconW - 22.0f;
				const vec4 NameColor = Recommended
					? vec4(1.0f, 0.82f, 0.30f, Appear)
					: vec4((Focused ? ColorText : ColorMuted).r,
						   (Focused ? ColorText : ColorMuted).g,
						   (Focused ? ColorText : ColorMuted).b,
						   Appear);
				DrawFitText(TextX, RowRect.y + 2.0f, TextW, 4.8f, 3.8f, Details.m_aName, NameColor);
				DrawFitText(TextX, RowRect.y + 16.0f, TextW, 3.4f, 2.8f, Details.m_aDescription,
							vec4(ColorMuted.r, ColorMuted.g, ColorMuted.b, 0.85f * Appear));

				if(Details.m_IsCurrentMode)
				{
					CUIRect Tag = {RowRect.x + IconW - 24.0f, RowRect.y + 2.0f, 22.0f, 9.0f};
					DrawRect(Tag, vec4(Accent.r, Accent.g, Accent.b, 0.92f * Appear), 4.0f);
					DrawCenteredText(Tag.x + Tag.w * 0.5f, Tag.y + 1.5f, 3.4f, "CONT", vec4(0.0f, 0.0f, 0.0f, 0.9f * Appear));
				}
			}
			Unclip();

			// Scroll bar (only when the column overflows).
			const float MaxScroll = max(0.0f, Count * RowH - ListH);
			if(MaxScroll > 0.0f)
			{
				CUIRect Track = {ColX + ColW - 4.0f, ListTop, 3.0f, ListH};
				DrawRect(Track, vec4(ColorBgPanel.r, ColorBgPanel.g, ColorBgPanel.b, 0.70f * Appear), 1.5f);
				const float ThumbH = max(14.0f, Track.h * (ListH / (Count * RowH)));
				// Pos is in rows, MaxScroll is in pixels -> multiply by RowH
				// so the thumb maps to the same normalised range 0..1.
				const float Ratio = clamp((m_aColScroll[Col] + m_aOverScroll[Col]) * RowH / MaxScroll,
					0.0f, 1.0f);
				CUIRect Thumb = {Track.x, Track.y + (Track.h - ThumbH) * Ratio, Track.w, ThumbH};
				DrawRect(Thumb, vec4(ColAccent.r, ColAccent.g, ColAccent.b, 0.90f * Appear), 1.5f);
			}
		}

		// "Back to recommended" button.
		{
			const vec4 BackColor = BackButtonHovered ? ColorText : ColorMuted;
			DrawRect(BackButtonRect,
					 vec4(ColorBgDeep.r, ColorBgDeep.g, ColorBgDeep.b, (BackButtonHovered ? 0.92f : 0.70f) * Appear),
					 6.0f);
			DrawCenteredText(BackButtonRect.x + BackButtonRect.w * 0.5f,
							 BackButtonRect.y + 4.0f,
							 5.6f,
							 Localize("Back to recommended"),
							 vec4(BackColor.r, BackColor.g, BackColor.b, Appear));
		}
	}

	// Complete four-sided border painted last, over the cards.
	{
		const float BorderW = 1.2f;
		const vec4 BorderColor = vec4(ColorAccent.r, ColorAccent.g, ColorAccent.b, 0.50f * Appear);
		DrawRect({Stage.x, Stage.y, Stage.w, BorderW}, BorderColor, 1.0f);
		DrawRect({Stage.x, Stage.y + Stage.h - BorderW, Stage.w, BorderW}, BorderColor, 1.0f);
		DrawRect({Stage.x, Stage.y, BorderW, Stage.h}, BorderColor, 1.0f);
		DrawRect({Stage.x + Stage.w - BorderW, Stage.y, BorderW, Stage.h}, BorderColor, 1.0f);
	}

	const char *pHint = m_ShowAllVotes
		? Localize("W / S: browse · A / D: column · Space: vote · Tab: back")
		: Localize("A / D: mode · Space: vote · Tab: all modes");
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

		// Server-side selection flags: the running mode ("continue" card)
		// and the recommended slots (1-3).
		m_aGameVoteDetails[i].m_RecommendedRank = clamp(pMsg->m_RecommendedRank, 0, 3);
		m_aGameVoteDetails[i].m_IsCurrentMode = pMsg->m_IsCurrentMode != 0;
		if(m_aGameVoteDetails[i].m_RecommendedRank > 0)
		{
			const int Slot = m_aGameVoteDetails[i].m_RecommendedRank - 1;
			m_aRecommended[Slot] = i;
			m_RecommendedCount = max(m_RecommendedCount, Slot + 1);
		}
		if(m_aGameVoteDetails[i].m_IsCurrentMode)
			m_CurrentVote = i;

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
