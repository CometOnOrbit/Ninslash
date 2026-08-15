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
				 "build chapter research advances research step");
	Ok &= Expect(!TutorialGameplayEventMatches(TUTORIAL_CHAPTER_BUILD, 1, TUTORIAL_EVENT_PERK),
				 "build chapter rejects an event from another step");
	for(int Chapter = TUTORIAL_CHAPTER_DEPLOYMENT; Chapter <= NUM_TUTORIAL_CHAPTERS; Chapter++)
	{
		const int Last = TutorialLastStep(Chapter);
		Ok &= Expect(TutorialStepIsDoor(Chapter, Last), "last step of every chapter is the door");
		Ok &= Expect(TutorialGameplayEventMatches(Chapter, Last, TUTORIAL_EVENT_DOOR),
					 "door event matches the last step");
		Ok &= Expect(!TutorialGameplayEventMatches(Chapter, Last, TUTORIAL_EVENT_OBJECTIVE),
					 "earlier events cannot finish the door step");
		Ok &= Expect(!TutorialStepIsDoor(Chapter, Last - 1), "penultimate step is not the door");
	}
	Ok &= Expect(!TutorialGameplayEventMatches(TUTORIAL_CHAPTER_OBJECTIVES, 4, TUTORIAL_EVENT_OBJECTIVE),
				 "a fifth switch cannot complete the objectives chapter");
	Tutorial.Start(TUTORIAL_CHAPTER_DEPLOYMENT, TutorialLastStep(TUTORIAL_CHAPTER_DEPLOYMENT), 0);
	int DoorNonce = Tutorial.State().m_Nonce;
	Ok &= Expect(!Tutorial.OnAction(TUTORIAL_ACTION_UI_CONTINUE, DoorNonce) && Tutorial.State().m_Active,
				 "continue cannot skip the door");
	Ok &= Expect(Tutorial.AddProgress() && !Tutorial.State().m_Active, "door progress completes the chapter");
	Tutorial.Start(TUTORIAL_CHAPTER_MULTIPLAYER, 1, 31);
	int RoomNonce = Tutorial.State().m_Nonce;
	Ok &= Expect(!Tutorial.OnAction(TUTORIAL_ACTION_UI_ROOM_JOIN, RoomNonce) && Tutorial.State().m_Step == 1,
				 "room join cannot replace room creation");
	Tutorial.OnAction(TUTORIAL_ACTION_UI_ROOM_CREATE, RoomNonce);
	Ok &= Expect(Tutorial.State().m_Step == 2 && Tutorial.State().m_Active, "room creation advances to join step");
	RoomNonce = Tutorial.State().m_Nonce;
	Ok &= Expect(!Tutorial.OnAction(TUTORIAL_ACTION_UI_ROOM_CREATE, RoomNonce) && Tutorial.State().m_Step == 2,
				 "room creation cannot replace room join");
	Tutorial.OnAction(TUTORIAL_ACTION_UI_ROOM_JOIN, RoomNonce);
	Ok &= Expect(Tutorial.State().m_Active &&
					 Tutorial.State().m_Step == TutorialLastStep(TUTORIAL_CHAPTER_MULTIPLAYER),
				 "room join advances to the door");
	RoomNonce = Tutorial.State().m_Nonce;
	Ok &= Expect(!Tutorial.OnAction(TUTORIAL_ACTION_UI_CONTINUE, RoomNonce) && Tutorial.State().m_Active,
				 "continue cannot skip the multiplayer door");
	Ok &= Expect(Tutorial.AddProgress() && !Tutorial.State().m_Active, "door completes multiplayer chapter");
	Tutorial.Start(99, 99, 0);
	Ok &= Expect(Tutorial.State().m_Chapter == TUTORIAL_CHAPTER_DEPLOYMENT && Tutorial.State().m_Step == 0,
				 "sanitize resume state");

	Ok &= Expect(TutorialPickupSpotOk(0, 1) && !TutorialPickupSpotOk(0, 0) && !TutorialPickupSpotOk(1, 1),
				 "kit spots are empty tiles on a floor");
	{
		// 8x4 corridor: reserved-looking walkable air must still accept kits.
		const int W = 8;
		const int H = 4;
		unsigned char aSolid[32];
		for(int i = 0; i < 32; i++)
			aSolid[i] = 0;
		for(int x = 0; x < W; x++)
		{
			aSolid[0 * W + x] = 1;
			aSolid[3 * W + x] = 1;
		}
		int aX[4];
		int aY[4];
		Ok &= Expect(TutorialPickKitSpots(aSolid, W, H, 4, aX, aY) == 4, "low corridor still places four kits");
		for(int i = 0; i < 4; i++)
			Ok &= Expect(aY[i] == 2 && TutorialPickupSpotOk(aSolid[aY[i] * W + aX[i]], aSolid[(aY[i] + 1) * W + aX[i]]),
						 "kits sit on the floor row");
		for(int i = 0; i < 32; i++)
			aSolid[i] = 1;
		Ok &= Expect(TutorialPickKitSpots(aSolid, W, H, 4, aX, aY) == 0, "solid grid places no kits");
		Ok &= Expect(TutorialPickKitSpots(aSolid, W, H, 4, aX, aY, 2) == 0, "solid grid places no standable spots");
	}
	{
		const int W = 8;
		const int H = 4;
		unsigned char aSolid[32];
		for(int i = 0; i < 32; i++)
			aSolid[i] = 0;
		for(int x = 0; x < W; x++)
		{
			aSolid[0 * W + x] = 1;
			aSolid[3 * W + x] = 1;
		}
		int aX[4];
		int aY[4];
		Ok &= Expect(TutorialPickKitSpots(aSolid, W, H, 2, aX, aY, 2) == 0,
					 "one-tile corridor is not a player spawn");
	}
	{
		const int W = 8;
		const int H = 6;
		unsigned char aSolid[48];
		for(int i = 0; i < 48; i++)
			aSolid[i] = 0;
		for(int x = 0; x < W; x++)
		{
			aSolid[0 * W + x] = 1;
			aSolid[5 * W + x] = 1;
		}
		int aX[2];
		int aY[2];
		Ok &= Expect(TutorialPickKitSpots(aSolid, W, H, 2, aX, aY, 2) == 2, "tall corridor places standable spots");
		for(int i = 0; i < 2; i++)
			Ok &= Expect(aY[i] == 4 && TutorialDoorSpotOk(aSolid[aY[i] * W + aX[i]], aSolid[(aY[i] + 1) * W + aX[i]],
														 aSolid[(aY[i] - 1) * W + aX[i]],
														 aSolid[(aY[i] - 2) * W + aX[i]]),
						 "standable spots sit on the floor with headroom");
	}
	{
		// Floor air that is actually a crate (FGOBJECTS) must not be a player spawn,
		// and the 32px-wide player must not clip the crate from the next tile.
		const int W = 12;
		const int H = 8;
		unsigned char aSolid[96];
		for(int i = 0; i < 96; i++)
			aSolid[i] = 0;
		for(int x = 0; x < W; x++)
		{
			aSolid[0 * W + x] = 1;
			aSolid[7 * W + x] = 1;
		}
		for(int yy = 4; yy <= 6; yy++)
			for(int xx = 4; xx <= 6; xx++)
				aSolid[yy * W + xx] = 1;
		int aX[2];
		int aY[2];
		const int N = TutorialPickKitSpots(aSolid, W, H, 2, aX, aY, 2);
		Ok &= Expect(N == 2, "crate on the floor still leaves standable spots");
		for(int i = 0; i < N; i++)
		{
			const bool InsideCrate = aX[i] >= 4 && aX[i] <= 6 && aY[i] >= 4 && aY[i] <= 6;
			Ok &= Expect(!InsideCrate && aSolid[aY[i] * W + aX[i]] == 0, "player spawn is not inside the crate");
			if(aY[i] == 6)
				Ok &= Expect(aX[i] < 3 || aX[i] > 7, "floor spawn stays a tile away from the crate");
		}
	}
	{
		const int W = 8;
		const int H = 6;
		unsigned char aSolid[48];
		for(int i = 0; i < 48; i++)
			aSolid[i] = 0;
		for(int x = 0; x < W; x++)
		{
			aSolid[0 * W + x] = 1;
			aSolid[5 * W + x] = 1;
		}
		int DoorX = -1;
		int DoorY = -1;
		Ok &= Expect(TutorialPickDoorSpot(aSolid, W, H, &DoorX, &DoorY), "low corridor still places a door");
		Ok &= Expect(DoorY == 4 && TutorialDoorSpotOk(aSolid[DoorY * W + DoorX], aSolid[(DoorY + 1) * W + DoorX],
													 aSolid[(DoorY - 1) * W + DoorX], aSolid[(DoorY - 2) * W + DoorX]),
					 "door sits on the floor with player-height air");
		for(int i = 0; i < 48; i++)
			aSolid[i] = 1;
		Ok &= Expect(!TutorialPickDoorSpot(aSolid, W, H, &DoorX, &DoorY), "solid grid places no door");
	}
	return Ok ? 0 : 1;
}
