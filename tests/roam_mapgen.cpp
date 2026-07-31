#include <game/server/roam_mapgen_layout.h>
#include <game/pathfinding.h>
#include <engine/shared/mappath.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static void TestTemplates()
{
	// I HATE MAPGEN
	assert(RoamMapGen::CHUNK_W == 40 && RoamMapGen::CHUNK_H == 32);
	assert(RoamMapGen::TEMPLATE_COUNT == 48);
	assert(RoamMapGen::TEMPL_FINISH_TOP == 44);
	int PairVariants = 0, Shortcuts = 0;
	unsigned Hashes[RoamMapGen::TEMPLATE_COUNT];
	for(int Template = 0; Template < RoamMapGen::TEMPLATE_COUNT; Template++)
	{
		const RoamMapGen::CTemplateSpec Spec = RoamMapGen::TemplateSpec(Template);
		RoamMapGen::CTemplateGrid Grid;
		RoamMapGen::GenerateTemplate(Spec, &Grid);
		assert(Spec.m_Index == Template);
		assert(RoamMapGen::ValidateTemplate(Spec, Grid));
		Hashes[Template] = RoamMapGen::TemplateGeometryHash(Grid);
		Shortcuts += Spec.m_HasShortcut;
		for(int i = 0; i < Grid.m_HazardCount; i++)
		{
			const RoamMapGen::CHazardSpec &H = Grid.m_aHazards[i];
			bool IntersectsMain = false;
			bool WarnsMain = false;
			for(int y = H.m_MinY; y <= H.m_MaxY; y++)
				for(int x = H.m_MinX; x <= H.m_MaxX; x++)
					if(x >= 0 && y >= 0 && x < RoamMapGen::CHUNK_W && y < RoamMapGen::CHUNK_H)
						IntersectsMain |= Grid.m_aMainRoute[y * RoamMapGen::CHUNK_W + x] != 0;
			for(int y = H.m_WarnMinY; y <= H.m_WarnMaxY; y++)
				for(int x = H.m_WarnMinX; x <= H.m_WarnMaxX; x++)
					if(x >= 0 && y >= 0 && x < RoamMapGen::CHUNK_W && y < RoamMapGen::CHUNK_H)
						WarnsMain |= Grid.m_aMainRoute[y * RoamMapGen::CHUNK_W + x] != 0;
			assert(IntersectsMain);
			assert(WarnsMain);
			assert(H.m_SafeAction >= RoamMapGen::ACTION_JUMP && H.m_SafeAction <= RoamMapGen::ACTION_DIVERT);
			if(H.m_Type == RoamMapGen::HAZARD_SAW)
			{
				assert(!Grid.Solid(H.m_X, H.m_Y));
				assert(Grid.Solid(H.m_X, H.m_Y + 1));
				for(int DY = 0; DY <= 5; DY++)
					for(int DX = -1; DX <= 1; DX++)
						assert(!Grid.Solid(H.m_X + DX, H.m_Y - DY));
			}
			else if(H.m_Type == RoamMapGen::HAZARD_FLAME)
			{
				assert(!Grid.Solid(H.m_X, H.m_Y));
				assert(Grid.Solid(H.m_X - 1, H.m_Y) != Grid.Solid(H.m_X + 1, H.m_Y));
			}
			else if(H.m_Type == RoamMapGen::HAZARD_LASER)
			{
				assert(!Grid.Solid(H.m_X, H.m_Y));
				assert(Grid.Solid(H.m_X, H.m_Y - 1));
			}
		}
		assert((Spec.m_Start || Spec.m_Finish) ? Grid.m_HazardCount == 0 : Grid.m_HazardCount >= 1);

		// Every declared port has the canonical eight-tile opening; all other
		// boundary spans remain closed and therefore cannot leak into a neighbour.
		for(int Dir = 0; Dir < 4; Dir++)
		{
			const bool HasPort = Dir == Spec.m_EntryDir || (!Spec.m_Finish && Dir == Spec.m_ExitDir);
			for(int Across = 0; Across < RoamMapGen::PORT_WIDTH; Across++)
			{
				const int X = Dir == MAPPATH_DIR_LEFT ? 0 : Dir == MAPPATH_DIR_RIGHT ? RoamMapGen::CHUNK_W - 1 : 17 + Across;
				const int Y = Dir == MAPPATH_DIR_UP ? 0 : Dir == MAPPATH_DIR_DOWN ? RoamMapGen::CHUNK_H - 1 : 10 + Across;
				assert(Grid.Solid(X, Y) != HasPort);
			}
		}
	}
	// Every index must produce distinct collision or hazard geometry.
	for(int i = 0; i < RoamMapGen::TEMPLATE_COUNT; i++)
		for(int j = i + 1; j < RoamMapGen::TEMPLATE_COUNT; j++)
			assert(Hashes[i] != Hashes[j]);
	for(int Entry = 0; Entry < 4; Entry++) for(int Exit = 0; Exit < 4; Exit++) if(Entry != Exit)
	{
		const int Count = RoamMapGen::MiddleVariantCount(Entry, Exit);
		assert(Count == 3 || Count == 4);
		PairVariants += Count;
		for(int Variant = 0; Variant < Count; Variant++)
		{
			int GotEntry, GotExit;
			RoamMapGen::TemplatePorts(RoamMapGen::MiddleTemplateIndex(Entry, Exit, Variant), &GotEntry, &GotExit);
			assert(GotEntry == Entry && GotExit == Exit);
		}
	}
	assert(PairVariants == 41);
	assert(Shortcuts == 5);
	for(int Entry = 0; Entry < 4; Entry++) assert(RoamMapGen::FinishTemplateIndex(Entry) == RoamMapGen::TEMPL_FINISH_TOP + Entry);
}

static void TestRaceGates()
{
	for(int Dir = 0; Dir < 4; Dir++)
	{
		const RoamMapGen::CTileAabb Checkpoint = RoamMapGen::RaceGateLocalAabb(Dir, false);
		const RoamMapGen::CTileAabb Finish = RoamMapGen::RaceGateLocalAabb(Dir, true);
		const int CheckW = Checkpoint.m_MaxX - Checkpoint.m_MinX + 1;
		const int CheckH = Checkpoint.m_MaxY - Checkpoint.m_MinY + 1;
		assert((CheckW == 2 && CheckH == 8) || (CheckW == 8 && CheckH == 2));
		const int FinishW = Finish.m_MaxX - Finish.m_MinX + 1;
		const int FinishH = Finish.m_MaxY - Finish.m_MinY + 1;
		assert((FinishW == 2 && FinishH == 8) || (FinishW == 8 && FinishH == 2));
	}

	const vec2 Min(100.0f, 200.0f), Max(164.0f, 456.0f);
	assert(RoamMapGen::SegmentIntersectsAabb(vec2(0, 300), vec2(400, 300), Min, Max)); // high-speed crossing
	assert(RoamMapGen::SegmentIntersectsAabb(vec2(0, 200), vec2(400, 200), Min, Max)); // inclusive edge
	assert(!RoamMapGen::SegmentIntersectsAabb(vec2(0, 199), vec2(400, 199), Min, Max));

	// Only the next ordered gate is eligible: crossing a future gate first is
	// ignored, then crossing gates 0 and 1 advances normally.
	const vec2 aMin[2] = {vec2(100, 0), vec2(300, 0)};
	const vec2 aMax[2] = {vec2(132, 256), vec2(332, 256)};
	int Next = 0;
	if(RoamMapGen::SegmentIntersectsAabb(vec2(250, 128), vec2(350, 128), aMin[Next], aMax[Next])) Next++;
	assert(Next == 0);
	if(RoamMapGen::SegmentIntersectsAabb(vec2(50, 128), vec2(150, 128), aMin[Next], aMax[Next])) Next++;
	if(RoamMapGen::SegmentIntersectsAabb(vec2(250, 128), vec2(350, 128), aMin[Next], aMax[Next])) Next++;
	assert(Next == 2);
}

static void TestCourse(int Seed)
{
	RoamMapGen::CCoursePlacement Course[RoamMapGen::COURSE_LENGTH];
	assert(RoamMapGen::GenerateCourse(Seed, Course, RoamMapGen::COURSE_LENGTH) == RoamMapGen::COURSE_LENGTH);
	int Turns = 0, Lefts = 0, Verticals = 0, MaxDifficulty[4] = {0, 0, 0, 0};
	int CheckpointGates = 0, FinishGates = 0;
	int WaypointSamples = 0;
	for(int i = 0; i < RoamMapGen::COURSE_LENGTH; i++)
	{
		assert(Course[i].m_CourseIndex == i);
		assert(Course[i].m_GridY >= 0 && Course[i].m_GridY < RoamMapGen::VERTICAL_BAND);
		assert(Course[i].m_TemplateIndex >= 0 && Course[i].m_TemplateIndex < RoamMapGen::TEMPLATE_COUNT);
		RoamMapGen::CTemplateGrid WaypointGrid;
		RoamMapGen::GenerateTemplate(RoamMapGen::TemplateSpec(Course[i].m_TemplateIndex), &WaypointGrid);
		for(int X = 2; X < RoamMapGen::CHUNK_W - 2; X++)
			for(int Y = 2; Y < RoamMapGen::CHUNK_H - 2; Y++)
			{
				const bool Passable = !WaypointGrid.Solid(X, Y) && !WaypointGrid.Solid(X - 1, Y) &&
					!WaypointGrid.Solid(X + 1, Y) && !WaypointGrid.Solid(X, Y - 1);
				if(!Passable)
					continue;
				const bool Grounded = WaypointGrid.Solid(X, Y + 1);
				const bool Edge = Grounded && (!WaypointGrid.Solid(X - 1, Y + 1) || !WaypointGrid.Solid(X + 1, Y + 1));
				WaypointSamples += Edge || (Grounded && X % 4 == 0) || (X % 4 == 0 && Y % 4 == 0);
			}
		CheckpointGates += i > 0;
		FinishGates += i == RoamMapGen::COURSE_LENGTH - 1;
		if(i == 0) assert(Course[i].m_TemplateIndex == RoamMapGen::StartTemplateIndex(Course[i].m_ExitDir));
		else if(i == RoamMapGen::COURSE_LENGTH - 1) assert(Course[i].m_TemplateIndex == RoamMapGen::FinishTemplateIndex(Course[i].m_EntryDir));
		else
		{
			assert(RoamMapGen::EntryExitCompatible(Course[i].m_EntryDir, Course[i].m_ExitDir));
			const int Tier = RoamMapGen::DifficultyTier(i);
			const int Difficulty = RoamMapGen::TemplateSpec(Course[i].m_TemplateIndex).m_Difficulty;
			if(Difficulty > MaxDifficulty[Tier]) MaxDifficulty[Tier] = Difficulty;
			RoamMapGen::CTemplateGrid Grid;
			RoamMapGen::GenerateTemplate(RoamMapGen::TemplateSpec(Course[i].m_TemplateIndex), &Grid);
			const int ActiveHazards = i < 12 ? 0 : i <= 17 ? 1 : (Grid.m_HazardCount < 2 ? Grid.m_HazardCount : 2);
			if(i < 12) assert(ActiveHazards == 0);
			else if(i <= 17) assert(Grid.m_HazardCount >= 1 && ActiveHazards == 1);
			else assert(Grid.m_HazardCount >= 1 && ActiveHazards >= 1 && ActiveHazards <= 2);
		}
		if(i > 0)
		{
			int DX, DY; CMapPath::DirOffset(Course[i - 1].m_ExitDir, &DX, &DY);
			assert(Course[i].m_GridX == Course[i - 1].m_GridX + DX && Course[i].m_GridY == Course[i - 1].m_GridY + DY);
			assert(Course[i].m_EntryDir == RoamMapGen::Opposite(Course[i - 1].m_ExitDir));
			Turns += Course[i].m_ExitDir != Course[i - 1].m_ExitDir;
		}
		Lefts += Course[i].m_ExitDir == MAPPATH_DIR_LEFT;
		Verticals += Course[i].m_ExitDir == MAPPATH_DIR_UP || Course[i].m_ExitDir == MAPPATH_DIR_DOWN;
		for(int j = 0; j < i; j++)
		{
			assert(Course[i].m_GridX != Course[j].m_GridX || Course[i].m_GridY != Course[j].m_GridY);
			const int Distance = abs(Course[i].m_GridX - Course[j].m_GridX) + abs(Course[i].m_GridY - Course[j].m_GridY);
			assert(Distance != 1 || j == i - 1);
		}
	}
	assert(Course[RoamMapGen::COURSE_LENGTH - 1].m_GridX >= 8);
	assert(WaypointSamples > 0 && WaypointSamples < MAX_WAYPOINTS);
	assert(CheckpointGates == RoamMapGen::CHECKPOINT_COUNT && FinishGates == 1);
	assert(Lefts <= RoamMapGen::MAX_LEFT_EXITS && Turns >= 5 && Turns <= 13 && Verticals >= 4 && Verticals <= 9);
	assert(MaxDifficulty[2] >= MaxDifficulty[0]);
	assert(MaxDifficulty[3] >= MaxDifficulty[2]);

	CMapPathInfoData Info; RoamMapGen::FillPathInfo(&Info, RoamMapGen::COURSE_LENGTH);
	CMapPathPlacementData Placements[RoamMapGen::COURSE_LENGTH];
	for(int i = 0; i < RoamMapGen::COURSE_LENGTH; i++) RoamMapGen::ToPathPlacement(Course[i], &Placements[i]);
	CMapPath Path; assert(Path.Init(&Info, Placements, RoamMapGen::COURSE_LENGTH));
	assert(Path.Info().m_ChunkWidth == 40 && Path.Info().m_ChunkHeight == 32);
}

static void TestVariableCourseLengths()
{
	const int aCheckpoints[] = {RoamMapGen::MIN_CHECKPOINT_COUNT, 7, 15, RoamMapGen::DEFAULT_CHECKPOINT_COUNT, 47, RoamMapGen::MAX_CHECKPOINT_COUNT};
	for(int CheckpointIndex = 0; CheckpointIndex < (int)(sizeof(aCheckpoints) / sizeof(aCheckpoints[0])); CheckpointIndex++)
	{
		const int Count = aCheckpoints[CheckpointIndex] + 1;
		for(int Seed = 0; Seed < 20; Seed++)
		{
			RoamMapGen::CCoursePlacement Course[RoamMapGen::MAX_COURSE_LENGTH];
			assert(RoamMapGen::GenerateCourse(Seed, Course, Count) == Count);
			assert(Course[0].m_TemplateIndex == RoamMapGen::StartTemplateIndex(Course[0].m_ExitDir));
			assert(Course[Count - 1].m_TemplateIndex == RoamMapGen::FinishTemplateIndex(Course[Count - 1].m_EntryDir));
			assert(Course[Count - 1].m_CourseIndex == Count - 1);
		}
	}
}

int main()
{
	TestTemplates();
	TestRaceGates();
	TestVariableCourseLengths();
	for(int Seed = 0; Seed < 1000; Seed++)
	{
		TestCourse(Seed);
		RoamMapGen::CCoursePlacement A[RoamMapGen::COURSE_LENGTH], B[RoamMapGen::COURSE_LENGTH];
		assert(RoamMapGen::GenerateCourse(Seed, A, RoamMapGen::COURSE_LENGTH) == RoamMapGen::COURSE_LENGTH);
		assert(RoamMapGen::GenerateCourse(Seed, B, RoamMapGen::COURSE_LENGTH) == RoamMapGen::COURSE_LENGTH);
		for(int i = 0; i < RoamMapGen::COURSE_LENGTH; i++)
			assert(A[i].m_GridX == B[i].m_GridX && A[i].m_GridY == B[i].m_GridY && A[i].m_TemplateIndex == B[i].m_TemplateIndex);
	}
	printf("roam_mapgen ok\n");
}
