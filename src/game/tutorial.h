#ifndef GAME_TUTORIAL_H
#define GAME_TUTORIAL_H

// Shared tutorial contract.  It deliberately contains no game-world types so
// the server director, client persistence and unit tests agree on transitions.
enum ETutorialChapter
{
	TUTORIAL_CHAPTER_DEPLOYMENT = 1,
	TUTORIAL_CHAPTER_COMBAT,
	TUTORIAL_CHAPTER_OBJECTIVES,
	TUTORIAL_CHAPTER_FORGE,
	TUTORIAL_CHAPTER_BUILD,
	TUTORIAL_CHAPTER_MULTIPLAYER,
	NUM_TUTORIAL_CHAPTERS = TUTORIAL_CHAPTER_MULTIPLAYER,
};

enum
{
	TUTORIAL_CONTENT_VERSION = 2
};

inline int TutorialChapterFromLegacy(int LegacyState, int LegacyCheckpoint)
{
	if(LegacyState != 1)
		return TUTORIAL_CHAPTER_DEPLOYMENT;
	if(LegacyCheckpoint < 0)
		LegacyCheckpoint = 0;
	if(LegacyCheckpoint >= NUM_TUTORIAL_CHAPTERS)
		LegacyCheckpoint = NUM_TUTORIAL_CHAPTERS - 1;
	return LegacyCheckpoint + 1;
}

inline int TutorialFixedSeed(int Chapter)
{
	if(Chapter < TUTORIAL_CHAPTER_DEPLOYMENT)
		Chapter = TUTORIAL_CHAPTER_DEPLOYMENT;
	if(Chapter > NUM_TUTORIAL_CHAPTERS)
		Chapter = NUM_TUTORIAL_CHAPTERS;
	return 4241 + Chapter;
}

inline bool TutorialChapterForcesBuilding(int Chapter)
{
	return Chapter == TUTORIAL_CHAPTER_FORGE;
}

inline int TutorialCompletedMaskLimit()
{
	return (1 << NUM_TUTORIAL_CHAPTERS) - 1;
}

inline bool TutorialChapterCompleted(int Chapter, int CompletedMask)
{
	return Chapter >= TUTORIAL_CHAPTER_DEPLOYMENT && Chapter <= NUM_TUTORIAL_CHAPTERS &&
		   (CompletedMask & (1 << (Chapter - 1))) != 0;
}

// Legacy versions only stored the chapter currently in progress. Keep that
// chapter and all earlier chapters accessible even when no completion bits
// exist, while requiring a complete prefix for newly unlocked chapters.
inline bool TutorialChapterUnlocked(int Chapter, int CompletedMask, int CurrentChapter, bool HasCurrentProgress)
{
	if(Chapter < TUTORIAL_CHAPTER_DEPLOYMENT || Chapter > NUM_TUTORIAL_CHAPTERS)
		return false;
	CompletedMask &= TutorialCompletedMaskLimit();
	if(Chapter == TUTORIAL_CHAPTER_DEPLOYMENT || TutorialChapterCompleted(Chapter, CompletedMask))
		return true;
	if(HasCurrentProgress && CurrentChapter >= TUTORIAL_CHAPTER_DEPLOYMENT && CurrentChapter <= NUM_TUTORIAL_CHAPTERS &&
	   Chapter <= CurrentChapter)
		return true;
	const int RequiredMask = (1 << (Chapter - 1)) - 1;
	return (CompletedMask & RequiredMask) == RequiredMask;
}

inline bool TutorialChapterIsReplay(int Chapter, int CompletedMask)
{
	return TutorialChapterCompleted(Chapter, CompletedMask);
}

inline int TutorialNextChapter(int CompletedChapter, int CompletedMask, bool IsReplay)
{
	if(IsReplay || CompletedChapter < TUTORIAL_CHAPTER_DEPLOYMENT || CompletedChapter >= NUM_TUTORIAL_CHAPTERS ||
	   !TutorialChapterCompleted(CompletedChapter, CompletedMask))
		return 0;
	const int NextChapter = CompletedChapter + 1;
	return TutorialChapterCompleted(NextChapter, CompletedMask) ? 0 : NextChapter;
}

enum ETutorialAction
{
	TUTORIAL_ACTION_NONE = 0,
	TUTORIAL_ACTION_UI_READY,
	TUTORIAL_ACTION_UI_CONTINUE,
	TUTORIAL_ACTION_UI_ROOM_CREATE,
	TUTORIAL_ACTION_UI_ROOM_JOIN,
};

enum ETutorialGameplayEvent
{
	TUTORIAL_EVENT_KILL = 1,
	TUTORIAL_EVENT_RECOVER,
	TUTORIAL_EVENT_OBJECTIVE,
	TUTORIAL_EVENT_MATERIAL,
	TUTORIAL_EVENT_FORGE,
	TUTORIAL_EVENT_BUILD,
	TUTORIAL_EVENT_PERK,
	TUTORIAL_EVENT_DRONE,
	TUTORIAL_EVENT_RESEARCH,
	TUTORIAL_EVENT_WEAPON_SWITCH,
	TUTORIAL_EVENT_TARGET_HIT,
};

inline bool TutorialGameplayEventMatches(int Chapter, int Step, int Event, bool CombatRespawnReady = false)
{
	if(Chapter == TUTORIAL_CHAPTER_COMBAT)
		return (Step == 0 && Event == TUTORIAL_EVENT_KILL) || (Step == 1 && Event == TUTORIAL_EVENT_RECOVER) ||
			   (Step == 2 && CombatRespawnReady && Event == TUTORIAL_EVENT_KILL);
	if(Chapter == TUTORIAL_CHAPTER_OBJECTIVES)
		return Event == TUTORIAL_EVENT_OBJECTIVE;
	if(Chapter == TUTORIAL_CHAPTER_FORGE)
		return (Step == 0 && Event == TUTORIAL_EVENT_MATERIAL) || (Step == 1 && Event == TUTORIAL_EVENT_FORGE) ||
			   (Step == 2 && Event == TUTORIAL_EVENT_BUILD);
	if(Chapter == TUTORIAL_CHAPTER_BUILD)
		return (Step == 0 && Event == TUTORIAL_EVENT_PERK) || (Step == 1 && Event == TUTORIAL_EVENT_DRONE) ||
			   (Step == 2 && Event == TUTORIAL_EVENT_RESEARCH);
	if(Chapter == TUTORIAL_CHAPTER_MULTIPLAYER)
		return Step == 0 && Event == TUTORIAL_EVENT_KILL;
	return false;
}

struct CTutorialState
{
	int m_Chapter;
	int m_Step;
	int m_Progress;
	int m_Target;
	int m_Nonce;
	int m_CompletedMask;
	bool m_Active;

	CTutorialState()
		: m_Chapter(TUTORIAL_CHAPTER_DEPLOYMENT), m_Step(0), m_Progress(0), m_Target(1), m_Nonce(1), m_CompletedMask(0),
		  m_Active(false)
	{
	}
};

inline int TutorialStepCount(int Chapter)
{
	static const int s_aSteps[] = {0, 3, 3, 4, 3, 3, 3};
	return Chapter >= TUTORIAL_CHAPTER_DEPLOYMENT && Chapter <= NUM_TUTORIAL_CHAPTERS ? s_aSteps[Chapter] : 0;
}

inline int TutorialTargetForStep(int Chapter, int Step)
{
	// Targets are intentionally modest and deterministic. World-specific hooks
	// feed progress; UI-only room actions use the final multiplayer steps.
	static const int s_aaTargets[7][4] = {
		{0, 0, 0, 0},
		{1, 1, 2, 0},
		{3, 1, 1, 0},
		{1, 1, 1, 1},
		{1, 1, 1, 0},
		{1, 1, 1, 0},
		{1, 1, 1, 0},
	};
	if(Chapter < TUTORIAL_CHAPTER_DEPLOYMENT || Chapter > NUM_TUTORIAL_CHAPTERS || Step < 0 ||
	   Step >= TutorialStepCount(Chapter))
		return 0;
	return s_aaTargets[Chapter][Step];
}

class CTutorialStateMachine
{
	CTutorialState m_State;
	void ResetStep()
	{
		m_State.m_Progress = 0;
		m_State.m_Target = TutorialTargetForStep(m_State.m_Chapter, m_State.m_Step);
		m_State.m_Nonce = m_State.m_Nonce == 0x7fffffff ? 1 : m_State.m_Nonce + 1;
	}

  public:
	const CTutorialState &State() const { return m_State; }
	void Start(int Chapter, int Step, int CompletedMask)
	{
		m_State.m_Chapter = Chapter < TUTORIAL_CHAPTER_DEPLOYMENT || Chapter > NUM_TUTORIAL_CHAPTERS
								? TUTORIAL_CHAPTER_DEPLOYMENT
								: Chapter;
		m_State.m_Step = Step < 0 || Step >= TutorialStepCount(m_State.m_Chapter) ? 0 : Step;
		m_State.m_CompletedMask = CompletedMask & TutorialCompletedMaskLimit();
		m_State.m_Active = true;
		ResetStep();
	}
	bool AddProgress(int Amount = 1)
	{
		if(!m_State.m_Active || Amount <= 0 || m_State.m_Target <= 0)
			return false;
		m_State.m_Progress += Amount;
		if(m_State.m_Progress < m_State.m_Target)
			return false;
		m_State.m_Step++;
		if(m_State.m_Step < TutorialStepCount(m_State.m_Chapter))
		{
			ResetStep();
			return false;
		}
		m_State.m_CompletedMask |= 1 << (m_State.m_Chapter - 1);
		m_State.m_Active = false;
		m_State.m_Progress = m_State.m_Target;
		return true;
	}
	bool OnAction(int Action, int Nonce)
	{
		if(!m_State.m_Active || Nonce != m_State.m_Nonce)
			return false;
		const bool IsRoomStep = m_State.m_Chapter == TUTORIAL_CHAPTER_MULTIPLAYER && m_State.m_Step >= 1;
		if((Action == TUTORIAL_ACTION_UI_READY && m_State.m_Step == 0) ||
		   (m_State.m_Chapter == TUTORIAL_CHAPTER_MULTIPLAYER && m_State.m_Step == 1 &&
			Action == TUTORIAL_ACTION_UI_ROOM_CREATE) ||
		   (m_State.m_Chapter == TUTORIAL_CHAPTER_MULTIPLAYER && m_State.m_Step == 2 &&
			Action == TUTORIAL_ACTION_UI_ROOM_JOIN) ||
		   (!IsRoomStep && Action == TUTORIAL_ACTION_UI_CONTINUE))
			return AddProgress();
		return false;
	}
	void RetryCurrentStep()
	{
		if(m_State.m_Active)
			ResetStep();
	}
};

#endif
