

#ifndef GAME_CLIENT_COMPONENTS_SPECTATOR_H
#define GAME_CLIENT_COMPONENTS_SPECTATOR_H
#include <base/vmath.h>

#include <game/client/component.h>

struct CNetMsg_Sv_KillMsg;

class CSpectator : public CComponent
{
	enum
	{
		NO_SELECTION = -2,
	};

	bool m_Active;
	bool m_WasActive;

	int m_SelectedSpectatorID;
	vec2 m_SelectorMouse;

	bool m_AutoDirectorActive;
	int m_AutoDirectorEndTick;
	int m_AutoDirectorReturnID;

	static void ConKeySpectator(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectate(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectateNext(IConsole::IResult *pResult, void *pUserData);
	static void ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData);

	void CancelAutoDirector();
	void SpectateInternal(int SpectatorID, bool Automatic);
	void RenderStatsPanel();

  public:
	CSpectator();

	virtual void OnConsoleInit();
	virtual bool OnMouseMove(float x, float y);
	virtual void OnRender();
	virtual void OnRelease();
	virtual void OnReset();

	void Spectate(int SpectatorID);
	void OnKillEvent(const CNetMsg_Sv_KillMsg *pMsg);
	bool IsActive() const { return m_Active; }
};

#endif
