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
	TUTORIAL_CONTENT_VERSION = 3
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

// Pickups only need air on a floor. Player-height clearance (two empty tiles)
// fails in compact tutorial corridors.
inline bool TutorialPickupSpotOk(int Tile, int Below)
{
	return Tile == 0 && Below != 0;
}

// Door / player spawn need player-height air. Compact tutorial corridors still qualify.
inline bool TutorialDoorSpotOk(int Tile, int Below, int Above1, int Above2)
{
	return Tile == 0 && Below != 0 && Above1 == 0 && Above2 == 0;
}

inline int TutorialPickKitSpots(const unsigned char *pSolid, int W, int H, int Wanted, int *pOutX, int *pOutY,
							   int Headroom = 0)
{
	int Placed = 0;
	if(!pSolid || !pOutX || !pOutY || W < 3 || H < 3 || Wanted <= 0)
		return 0;
	const int MinSep = 4;
	const int Y0 = 1 + (Headroom > 0 ? Headroom : 0);
	for(int Pass = 0; Pass < 2 && Placed < Wanted; Pass++)
	{
		for(int Slot = Placed; Slot < Wanted; Slot++)
		{
			const int DesiredX = (Slot + 1) * W / (Wanted + 1);
			int BestX = -1;
			int BestY = -1;
			int BestScore = 0x7fffffff;
			for(int y = Y0; y < H - 1; y++)
				for(int x = 1; x < W - 1; x++)
				{
					if(Headroom >= 2)
					{
						// Player box is 32x74; one tile of air next to a crate still clips.
						bool Clear = true;
						for(int dx = -1; dx <= 1 && Clear; dx++)
							if(!TutorialDoorSpotOk(pSolid[y * W + x + dx], pSolid[(y + 1) * W + x + dx],
												   pSolid[(y - 1) * W + x + dx], pSolid[(y - 2) * W + x + dx]))
								Clear = false;
						if(!Clear)
							continue;
					}
					else if(!TutorialPickupSpotOk(pSolid[y * W + x], pSolid[(y + 1) * W + x]))
						continue;
					bool Taken = false;
					for(int i = 0; i < Placed; i++)
						if(pOutX[i] == x && pOutY[i] == y)
						{
							Taken = true;
							break;
						}
					if(Taken)
						continue;
					if(Pass == 0)
					{
						bool Far = true;
						for(int i = 0; i < Placed; i++)
						{
							int Dx = pOutX[i] - x;
							if(Dx < 0)
								Dx = -Dx;
							if(Dx < MinSep)
							{
								Far = false;
								break;
							}
						}
						if(!Far)
							continue;
					}
					int Dx = x - DesiredX;
					if(Dx < 0)
						Dx = -Dx;
					const int Score = Dx * 100 + y;
					if(Score < BestScore)
					{
						BestX = x;
						BestY = y;
						BestScore = Score;
					}
				}
			if(BestX < 0)
				continue;
			pOutX[Placed] = BestX;
			pOutY[Placed] = BestY;
			Placed++;
		}
	}
	return Placed;
}

inline bool TutorialPickDoorSpot(const unsigned char *pSolid, int W, int H, int *pOutX, int *pOutY)
{
	if(!pSolid || !pOutX || !pOutY || W < 5 || H < 5)
		return false;
	int BestX = -1;
	int BestY = -1;
	int BestScore = 0x7fffffff;
	const int DesiredX = W * 3 / 4;
	for(int y = 2; y < H - 1; y++)
		for(int x = 2; x < W - 2; x++)
		{
			if(!TutorialDoorSpotOk(pSolid[y * W + x], pSolid[(y + 1) * W + x], pSolid[(y - 1) * W + x],
								   pSolid[(y - 2) * W + x]))
				continue;
			int Dx = x - DesiredX;
			if(Dx < 0)
				Dx = -Dx;
			const int Score = Dx * 100 + (H - y);
			if(Score < BestScore)
			{
				BestX = x;
				BestY = y;
				BestScore = Score;
			}
		}
	if(BestX < 0)
		return false;
	*pOutX = BestX;
	*pOutY = BestY;
	return true;
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
	TUTORIAL_EVENT_DOOR,
};

inline int TutorialStepCount(int Chapter)
{
	static const int s_aSteps[] = {0, 4, 4, 5, 4, 4, 4};
	return Chapter >= TUTORIAL_CHAPTER_DEPLOYMENT && Chapter <= NUM_TUTORIAL_CHAPTERS ? s_aSteps[Chapter] : 0;
}

inline int TutorialLastStep(int Chapter)
{
	return TutorialStepCount(Chapter) - 1;
}

inline bool TutorialStepIsDoor(int Chapter, int Step)
{
	return TutorialStepCount(Chapter) > 0 && Step == TutorialLastStep(Chapter);
}

inline bool TutorialGameplayEventMatches(int Chapter, int Step, int Event, bool CombatRespawnReady = false)
{
	if(TutorialStepIsDoor(Chapter, Step))
		return Event == TUTORIAL_EVENT_DOOR;
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

inline int TutorialTargetForStep(int Chapter, int Step)
{
	// Targets are intentionally modest and deterministic. World-specific hooks
	// feed progress; UI-only room actions use the multiplayer middle steps.
	// The last step of every chapter is always the door.
	static const int s_aaTargets[7][5] = {
		{0, 0, 0, 0, 0},
		{1, 1, 2, 1, 0},
		{3, 1, 1, 1, 0},
		{1, 1, 1, 1, 1},
		{1, 1, 1, 1, 0},
		{1, 1, 1, 1, 0},
		{1, 1, 1, 1, 0},
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
		const bool IsDoorStep = TutorialStepIsDoor(m_State.m_Chapter, m_State.m_Step);
		const bool IsRoomStep = m_State.m_Chapter == TUTORIAL_CHAPTER_MULTIPLAYER && m_State.m_Step >= 1 && !IsDoorStep;
		if((Action == TUTORIAL_ACTION_UI_READY && m_State.m_Step == 0) ||
		   (m_State.m_Chapter == TUTORIAL_CHAPTER_MULTIPLAYER && m_State.m_Step == 1 &&
			Action == TUTORIAL_ACTION_UI_ROOM_CREATE) ||
		   (m_State.m_Chapter == TUTORIAL_CHAPTER_MULTIPLAYER && m_State.m_Step == 2 &&
			Action == TUTORIAL_ACTION_UI_ROOM_JOIN) ||
		   (!IsRoomStep && !IsDoorStep && Action == TUTORIAL_ACTION_UI_CONTINUE))
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
