#ifndef GAME_CLIENT_COMPONENTS_GAMEVOTE_H
#define GAME_CLIENT_COMPONENTS_GAMEVOTE_H
#include <game/client/component.h>
#include <game/gamevote.h>
#include <game/client/local_game_modes.h>

struct CGameVoteDetails
{
	char m_aName[32];
	char m_aDescription[128];
	char m_aImage[32];

	bool m_Valid;
	int m_Texture;
	int m_Votes;
	int m_RecommendedRank;
	bool m_IsCurrentMode;
	float m_NameWidth;
	float m_DescriptionWidth;
	float m_VotesWidth;

	CGameVoteDetails()
	{
		m_aName[0] = 0;
		m_aDescription[0] = 0;
		m_aImage[0] = 0;
		m_Valid = false;
		m_Texture = -1;
		m_Votes = 0;
		m_RecommendedRank = 0;
		m_IsCurrentMode = false;
	}
};

class CGameVoteDisplay : public CComponent
{
	int m_GameVoteCount;
	CGameVoteDetails m_aGameVoteDetails[MAX_GAME_VOTES];
	// Server vote indices in display order (sorted by local mode, matching the
	// room-creation mode picker). Voting still uses the server indices.
	int m_aDisplayOrder[MAX_GAME_VOTES];
	int m_DisplayCount;

	int m_Selected;
	int m_Focused;
	int m_ActiveCategory;
	float m_AppearAmount;
	float m_SelectionPulse;
	int m_TimeLeft;
	int m_TimeLeftTick;
	int m_VoteDuration;
	int64 m_LastVoteMessageTime;

	bool m_MouseTrigger;
	vec2 m_SelectorMouse;
	int m_DebugScreenshotFrames;

	int m_CurrentVote;
	int m_aRecommended[3];
	int m_RecommendedCount;

	bool m_ShowAllVotes;

	// One column per category with votes (language-list style).
	int m_RowCount;
	int m_aRows[NUM_GAMEVOTE_CATEGORIES];
	// m_FocusCol = column (category index), m_FocusRow = row inside the column.
	int m_FocusRow;
	int m_FocusCol;
	float m_aColScroll[NUM_GAMEVOTE_CATEGORIES];
	// Per-column scroll velocity in rows/second (inertial scrolling).
	float m_aColVel[NUM_GAMEVOTE_CATEGORIES];
	// Per-column overscroll (rows past the ends, decays smoothly like a
	// spring; added to m_aColScroll for display).
	float m_aOverScroll[NUM_GAMEVOTE_CATEGORIES];
	// Per-column header focus transition (0..1, eases toward focused state).
	float m_aColFocusAlpha[NUM_GAMEVOTE_CATEGORIES];
	// Detail-card slide-in animation (0..1) and the vote it was shown for.
	float m_DetailCardAppear;
	int m_DetailCardVote;
	int m_HoverCol;

	int FocusSlotVote(int Slot) const;
	// Full-browser list geometry (shared by OnRender and OnInput).
	static float ListRowH();
	static float ListAreaH();

	// Fallback recommendation builder used only when the server sent no
	// recommendation flags at all (older server / malformed data).
	void BuildFallbackRecommendations();
	int FindCurrentVote() const;
	int RowVoteCount(int Row) const;
	int VoteAtRowSlot(int Row, int Col) const;
	void MoveFocusRow(int Direction);
	void MoveFocusCol(int Direction);
	void EnsureFocusedRowVisible();

	void RenderMouse();
	void SendVote();
	int VoteCategory(int Vote) const;
	int CategoryVoteCount(int Category) const;
	int FirstVoteInCategory(int Category) const;
	int DisplaySortKey(int Vote) const;
	void RebuildDisplayOrder();
	static void ConDebugPreview(IConsole::IResult *pResult, void *pUserData);

  public:
	CGameVoteDisplay();
	virtual void OnReset();
	virtual void OnConsoleInit();
	virtual void OnRender();
	virtual void OnMessage(int MsgType, void *pRawMsg);
	virtual bool OnMouseMove(float x, float y);
	virtual bool OnInput(IInput::CEvent Event);

	bool IsActive() const { return m_GameVoteCount > 0; }
};

#endif
