#ifndef GAME_CLIENT_MENU_HOME_H
#define GAME_CLIENT_MENU_HOME_H

enum EMenuHomeAction
{
	MENU_HOME_JOIN_LOCAL,
	MENU_HOME_SHOW_LOCAL,
	MENU_HOME_CONTINUE_TUTORIAL,
	MENU_HOME_EXPEDITION,
};

struct CMenuHomeState
{
	bool m_LocalServerStarting;
	bool m_LocalServerRunning;
	bool m_ConnectedToLocalServer;
	bool m_TutorialInProgress;
	int m_TutorialChapter;
};

struct CMenuHomePrimary
{
	int m_Action;
	const char *m_pTitle;
	const char *m_pDescription;
	int m_Chapter;
};

inline CMenuHomePrimary ResolveMenuHomePrimary(const CMenuHomeState &State)
{
	if(State.m_LocalServerRunning)
	{
		if(State.m_ConnectedToLocalServer)
			return {MENU_HOME_SHOW_LOCAL,
					"Continue local mission",
					"Your local server is running and you are connected.",
					0};
		return {MENU_HOME_JOIN_LOCAL,
				"Join local mission",
				"Your local server is ready. Rejoin without changing its setup.",
				0};
	}
	if(State.m_LocalServerStarting)
		return {MENU_HOME_SHOW_LOCAL, "Local server starting", "Open server status while the mission is prepared.", 0};
	if(State.m_TutorialInProgress)
		return {MENU_HOME_CONTINUE_TUTORIAL,
				"Continue training",
				"Resume chapter %d of 6 from your latest step.",
				State.m_TutorialChapter < 1	  ? 1
				: State.m_TutorialChapter > 6 ? 6
											  : State.m_TutorialChapter};
	return {MENU_HOME_EXPEDITION,
			"Expedition Invasion",
			"Pick a save, invite friends, then start.",
			0};
}

#endif
