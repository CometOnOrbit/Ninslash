#include <game/tutorial.h>
#include <cstdio>

static bool Expect(bool Value, const char *pWhat)
{
	if(!Value)
		std::fprintf(stderr, "failed: %s\n", pWhat);
	return Value;
}

int main()
{
	bool Ok = true;
	CTutorialStateMachine Tutorial;
	int CompletedMask = 0;
	for(int Chapter = TUTORIAL_CHAPTER_DEPLOYMENT; Chapter <= NUM_TUTORIAL_CHAPTERS; Chapter++)
	{
		Tutorial.Start(Chapter, 0, CompletedMask);
		for(int Step = 0; Step < TutorialStepCount(Chapter); Step++)
		{
			const int Nonce = Tutorial.State().m_Nonce;
			Ok &= Expect(!Tutorial.OnAction(TUTORIAL_ACTION_UI_CONTINUE, Nonce - 1), "reject stale nonce");
			Ok &= Expect(!Tutorial.OnAction(99, Nonce), "reject illegal action");
			Tutorial.RetryCurrentStep();
			Ok &= Expect(Tutorial.State().m_Progress == 0 && Tutorial.State().m_Nonce != Nonce,
						 "death resets only current step");
			while(Tutorial.State().m_Active && Tutorial.State().m_Step == Step)
				Tutorial.AddProgress();
		}
		CompletedMask = Tutorial.State().m_CompletedMask;
		Ok &= Expect((CompletedMask & (1 << (Chapter - 1))) != 0, "chapter completion bit");
	}
	Ok &= Expect(CompletedMask == 63, "all chapters preserve completion bits");
	Ok &= Expect(TutorialChapterFromLegacy(1, 0) == 1 && TutorialChapterFromLegacy(1, 5) == 6,
				 "legacy checkpoints map to chapters");
	Ok &= Expect(TutorialChapterFromLegacy(2, 6) == 1, "legacy completion does not complete new chapters");
	Ok &= Expect(TutorialChapterUnlocked(1, 0, 1, false), "initial chapter is unlocked");
	Ok &= Expect(!TutorialChapterUnlocked(2, 0, 1, false), "initial later chapters are locked");
	Ok &= Expect(TutorialChapterUnlocked(2, 1, 1, false), "completion unlocks next chapter");
	for(int Chapter = 1; Chapter <= NUM_TUTORIAL_CHAPTERS; Chapter++)
	{
		const int PrefixMask = (1 << (Chapter - 1)) - 1;
		Ok &= Expect(TutorialChapterUnlocked(Chapter, PrefixMask, 1, false),
					 "completion prefix unlocks each chapter in order");
		if(Chapter < NUM_TUTORIAL_CHAPTERS)
			Ok &= Expect(!TutorialChapterUnlocked(Chapter + 1, PrefixMask, 1, false),
						 "completion prefix leaves the following chapter locked");
	}
	Ok &= Expect(!TutorialChapterUnlocked(4, 1 | 4, 1, false), "non-contiguous completion bits do not skip chapters");
	Ok &= Expect(TutorialChapterUnlocked(4, 0, 4, true) && TutorialChapterUnlocked(2, 0, 4, true),
				 "legacy in-progress chapter and predecessors stay unlocked");
	Ok &= Expect(!TutorialChapterUnlocked(5, 0, 4, true), "legacy progress does not unlock a later chapter");
	Ok &= Expect(TutorialChapterIsReplay(2, 2), "completed chapter starts as replay");
	Ok &= Expect(TutorialNextChapter(1, 1, false) == 2, "first completion advances to next chapter");
	Ok &= Expect(TutorialNextChapter(1, 1, true) == 0, "replay does not advance to next chapter");
	Ok &= Expect(TutorialNextChapter(6, 63, false) == 0, "final chapter returns to selection");
	for(int Chapter = 1; Chapter <= NUM_TUTORIAL_CHAPTERS; Chapter++)
		Ok &= Expect(TutorialFixedSeed(Chapter) == 4241 + Chapter, "chapter seed is fixed");
	Ok &= Expect(TutorialChapterForcesBuilding(TUTORIAL_CHAPTER_FORGE),
				 "forge tutorial forces building kits even when server defaults disable building");
	Ok &= Expect(!TutorialChapterForcesBuilding(TUTORIAL_CHAPTER_BUILD),
				 "build-and-growth chapter keeps the existing building policy");
	Ok &= Expect(TutorialGameplayEventMatches(TUTORIAL_CHAPTER_COMBAT, 0, TUTORIAL_EVENT_KILL),
				 "combat kills advance encounter");
	Ok &= Expect(TutorialGameplayEventMatches(TUTORIAL_CHAPTER_COMBAT, 1, TUTORIAL_EVENT_RECOVER),
				 "combat recovery advances encounter");
	Ok &= Expect(!TutorialGameplayEventMatches(TUTORIAL_CHAPTER_COMBAT, 2, TUTORIAL_EVENT_KILL, false),
				 "final combat requires respawn");
	Ok &= Expect(TutorialGameplayEventMatches(TUTORIAL_CHAPTER_COMBAT, 2, TUTORIAL_EVENT_KILL, true),
				 "post-respawn kill completes combat");
	Ok &= Expect(TutorialGameplayEventMatches(TUTORIAL_CHAPTER_BUILD, 0, TUTORIAL_EVENT_PERK),
				 "build chapter perk advances first step");
	Ok &= Expect(TutorialGameplayEventMatches(TUTORIAL_CHAPTER_BUILD, 1, TUTORIAL_EVENT_DRONE),
				 "build chapter drone switch advances second step");
	Ok &= Expect(TutorialGameplayEventMatches(TUTORIAL_CHAPTER_BUILD, 2, TUTORIAL_EVENT_RESEARCH),
				 "build chapter research advances final step");
	Ok &= Expect(!TutorialGameplayEventMatches(TUTORIAL_CHAPTER_BUILD, 1, TUTORIAL_EVENT_PERK),
				 "build chapter rejects an event from another step");
	Tutorial.Start(TUTORIAL_CHAPTER_MULTIPLAYER, 1, 31);
	int RoomNonce = Tutorial.State().m_Nonce;
	Ok &= Expect(!Tutorial.OnAction(TUTORIAL_ACTION_UI_ROOM_JOIN, RoomNonce) && Tutorial.State().m_Step == 1,
				 "room join cannot replace room creation");
	Tutorial.OnAction(TUTORIAL_ACTION_UI_ROOM_CREATE, RoomNonce);
	Ok &= Expect(Tutorial.State().m_Step == 2 && Tutorial.State().m_Active, "room creation advances to join step");
	RoomNonce = Tutorial.State().m_Nonce;
	Ok &= Expect(!Tutorial.OnAction(TUTORIAL_ACTION_UI_ROOM_CREATE, RoomNonce) && Tutorial.State().m_Step == 2,
				 "room creation cannot replace room join");
	Ok &= Expect(Tutorial.OnAction(TUTORIAL_ACTION_UI_ROOM_JOIN, RoomNonce) && !Tutorial.State().m_Active,
				 "room join completes multiplayer chapter");
	Tutorial.Start(99, 99, 0);
	Ok &= Expect(Tutorial.State().m_Chapter == TUTORIAL_CHAPTER_DEPLOYMENT && Tutorial.State().m_Step == 0,
				 "sanitize resume state");
	return Ok ? 0 : 1;
}
