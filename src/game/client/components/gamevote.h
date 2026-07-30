#ifndef GAME_CLIENT_COMPONENTS_GAMEVOTE_H
#define GAME_CLIENT_COMPONENTS_GAMEVOTE_H
#include <game/client/component.h>
#include <game/gamevote.h>

struct CGameVoteDetails
{
	char m_aName[32];
	char m_aDescription[128];
	char m_aImage[32];

	bool m_Valid;
	int m_Texture;
	int m_Votes;
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
	}
};

class CGameVoteDisplay : public CComponent
{
	int m_GameVoteCount;
	CGameVoteDetails m_aGameVoteDetails[MAX_GAME_VOTES];

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

	void RenderMouse();
	void SendVote();
	int VoteCategory(int Vote) const;
	int CategoryVoteCount(int Category) const;
	int FirstVoteInCategory(int Category) const;
	void ChangeCategory(int Direction);
	void MoveFocus(int Direction);
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
