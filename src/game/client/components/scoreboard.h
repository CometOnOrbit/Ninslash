

#ifndef GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#define GAME_CLIENT_COMPONENTS_SCOREBOARD_H
#include <game/client/component.h>
#include <game/client/ui.h>

class CScoreboard : public CComponent
{
	float RenderGoals(float x, float y, float w);
	float RenderSpectators(float x, float y, float w);
	float RenderScoreboard(float x, float y, float w, int Team, const char *pTitle);
	void RenderRecordingNotification(float x);

	static void ConKeyScoreboard(IConsole::IResult *pResult, void *pUserData);
	static void ConDebugScoreboard(IConsole::IResult *pResult, void *pUserData);

	const char *GetClanName(int Team);

	bool m_Active;
	bool m_DebugActive;
	float m_AppearAmount;
	int64 m_LastAnimationTime;
	CUIRect m_TotalRect;

  public:
	CScoreboard();
	virtual void OnReset();
	virtual void OnConsoleInit();
	virtual void OnRender();
	virtual void OnRelease();

	bool Active();
	const CUIRect &GetScoreboardRect() const { return m_TotalRect; }
};

#endif
