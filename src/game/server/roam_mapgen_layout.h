#ifndef GAME_SERVER_ROAM_MAPGEN_LAYOUT_H
#define GAME_SERVER_ROAM_MAPGEN_LAYOUT_H

#include <base/vmath.h>
#include <engine/shared/mappath.h>

namespace RoamMapGen
{
enum
{
	CHUNK_W = 40,
	CHUNK_H = 32,
	ATLAS_COLS = 6,
	ATLAS_ROWS = 8,
	TEMPLATE_COUNT = 48,
	MIN_CHECKPOINT_COUNT = 3,
	DEFAULT_CHECKPOINT_COUNT = 24,
	MAX_CHECKPOINT_COUNT = MAPPATH_MAX_PLACEMENTS - 1,
	MAX_COURSE_LENGTH = MAX_CHECKPOINT_COUNT + 1,
	// Backward-compatible aliases for the default course used by existing maps
	// and tests. Generated Roam courses use the configured checkpoint count.
	CHECKPOINT_COUNT = DEFAULT_CHECKPOINT_COUNT,
	COURSE_LENGTH = DEFAULT_CHECKPOINT_COUNT + 1,
	VERTICAL_BAND = 8,
	MAX_VERTICAL_STREAK = 2,
	MAX_LEFT_EXITS = 2,
	ROUTE_ATTEMPTS = 96,
	PORT_CENTER_X = 20,
	PORT_CENTER_Y = 16,
	PORT_WIDTH = 8,
	VISUAL_SWATCH_COLUMNS = 4,
	TEMPL_START_RIGHT = 0,
	TEMPL_START_UP = 1,
	TEMPL_START_DOWN = 2,
	TEMPL_MIDDLE_FIRST = 3,
	TEMPL_FINISH_TOP = 44,
	TEMPL_FINISH_RIGHT = 45,
	TEMPL_FINISH_BOTTOM = 46,
	TEMPL_FINISH_LEFT = 47,
};

enum EHazardType
{
	HAZARD_SAW,
	HAZARD_FLAME,
	HAZARD_LASER,
};

enum ESafeAction
{
	ACTION_JUMP,
	ACTION_SWITCH_WALL,
	ACTION_JETPACK,
	ACTION_WAIT,
	ACTION_DIVERT,
};

struct CHazardSpec
{
	int m_Type;
	int m_X, m_Y;
	int m_MinX, m_MinY, m_MaxX, m_MaxY;
	int m_WarnMinX, m_WarnMinY, m_WarnMaxX, m_WarnMaxY;
	int m_SafeAction;
};

enum ETemplateFamily
{
	FAMILY_RUNWAY,
	FAMILY_GAPS,
	FAMILY_STEPS,
	FAMILY_SLIDE,
	FAMILY_WALLJUMP,
	FAMILY_JETPACK,
	FAMILY_HOOK,
	FAMILY_DROP,
	FAMILY_SHORTCUT,
};

struct CTemplateSpec
{
	int m_Index;
	int m_EntryDir;
	int m_ExitDir;
	int m_Variant;
	int m_Family;
	int m_Difficulty;
	bool m_Start;
	bool m_Finish;
	bool m_HasShortcut;
};

struct CTemplateGrid
{
	unsigned char m_aSolid[CHUNK_W * CHUNK_H];
	unsigned char m_aMainRoute[CHUNK_W * CHUNK_H];
	unsigned char m_aSafeRoute[CHUNK_W * CHUNK_H];
	CHazardSpec m_aHazards[8];
	int m_HazardCount;

	void Fill(bool Solid)
	{
		for(int i = 0; i < CHUNK_W * CHUNK_H; i++) { m_aSolid[i] = Solid ? 1 : 0; m_aMainRoute[i] = 0; m_aSafeRoute[i] = 0; }
		m_HazardCount = 0;
	}
	bool Solid(int X, int Y) const { return X < 0 || Y < 0 || X >= CHUNK_W || Y >= CHUNK_H || m_aSolid[Y * CHUNK_W + X] != 0; }
	void SetSolid(int X, int Y, bool Solid)
	{
		if(X >= 0 && Y >= 0 && X < CHUNK_W && Y < CHUNK_H) m_aSolid[Y * CHUNK_W + X] = Solid ? 1 : 0;
	}
	void Rect(int X0, int Y0, int X1, int Y1, bool Solid)
	{
		for(int Y = Y0; Y <= Y1; Y++) for(int X = X0; X <= X1; X++) SetSolid(X, Y, Solid);
	}
	void AddHazard(int Type, int X, int Y, int MinX, int MinY, int MaxX, int MaxY,
		int WarnMinX, int WarnMinY, int WarnMaxX, int WarnMaxY, int SafeAction)
	{
		if(m_HazardCount >= 8) return;
		CHazardSpec &H = m_aHazards[m_HazardCount++];
		H.m_Type=Type; H.m_X=X; H.m_Y=Y; H.m_MinX=MinX; H.m_MinY=MinY; H.m_MaxX=MaxX; H.m_MaxY=MaxY;
		H.m_WarnMinX=WarnMinX; H.m_WarnMinY=WarnMinY; H.m_WarnMaxX=WarnMaxX; H.m_WarnMaxY=WarnMaxY; H.m_SafeAction=SafeAction;
	}
};

struct CCoursePlacement
{
	int m_GridX, m_GridY, m_TemplateIndex, m_CourseIndex, m_EntryDir, m_ExitDir;
};

struct CTileAabb
{
	int m_MinX, m_MinY, m_MaxX, m_MaxY;
};

inline CTileAabb RaceGateLocalAabb(int EntryDir, bool Finish)
{
	CTileAabb Box;
	if(Finish)
	{
		if(EntryDir == MAPPATH_DIR_LEFT || EntryDir == MAPPATH_DIR_RIGHT)
			{ Box.m_MinX = 19; Box.m_MaxX = 20; Box.m_MinY = 10; Box.m_MaxY = 17; }
		else
			{ Box.m_MinX = 17; Box.m_MaxX = 24; Box.m_MinY = 15; Box.m_MaxY = 16; }
	}
	else if(EntryDir == MAPPATH_DIR_LEFT)
		{ Box.m_MinX = 1; Box.m_MaxX = 2; Box.m_MinY = 10; Box.m_MaxY = 17; }
	else if(EntryDir == MAPPATH_DIR_RIGHT)
		{ Box.m_MinX = CHUNK_W - 3; Box.m_MaxX = CHUNK_W - 2; Box.m_MinY = 10; Box.m_MaxY = 17; }
	else if(EntryDir == MAPPATH_DIR_UP)
		{ Box.m_MinX = 17; Box.m_MaxX = 24; Box.m_MinY = 1; Box.m_MaxY = 2; }
	else
		{ Box.m_MinX = 17; Box.m_MaxX = 24; Box.m_MinY = CHUNK_H - 3; Box.m_MaxY = CHUNK_H - 2; }
	return Box;
}

inline bool PointInAabb(vec2 Pos, vec2 Min, vec2 Max)
{
	return Pos.x >= Min.x && Pos.x <= Max.x && Pos.y >= Min.y && Pos.y <= Max.y;
}

inline bool SegmentIntersectsAabb(vec2 From, vec2 To, vec2 Min, vec2 Max)
{
	if(PointInAabb(From, Min, Max) || PointInAabb(To, Min, Max)) return true;
	float TMin = 0.0f, TMax = 1.0f;
	const float aFrom[2] = {From.x, From.y};
	const float aDelta[2] = {To.x - From.x, To.y - From.y};
	const float aMin[2] = {Min.x, Min.y};
	const float aMax[2] = {Max.x, Max.y};
	for(int Axis = 0; Axis < 2; Axis++)
	{
		if(aDelta[Axis] > -0.000001f && aDelta[Axis] < 0.000001f)
		{
			if(aFrom[Axis] < aMin[Axis] || aFrom[Axis] > aMax[Axis]) return false;
			continue;
		}
		float A = (aMin[Axis] - aFrom[Axis]) / aDelta[Axis];
		float B = (aMax[Axis] - aFrom[Axis]) / aDelta[Axis];
		if(A > B) { const float Tmp = A; A = B; B = Tmp; }
		if(A > TMin) TMin = A;
		if(B < TMax) TMax = B;
		if(TMin > TMax) return false;
	}
	return true;
}

inline int Variation(int Seed, int Chunk, int Index, int Range)
{
	unsigned V = (unsigned)Seed * 1103515245u + (unsigned)Chunk * 2654435761u + (unsigned)Index * 2246822519u;
	V ^= V >> 13;
	return Range > 0 ? (int)(V % (unsigned)Range) : 0;
}
inline int Opposite(int Dir) { return CMapPath::OppositeDir(Dir); }
inline int AtlasX(int Template) { return Template % ATLAS_COLS * CHUNK_W; }
inline int AtlasY(int Template) { return Template / ATLAS_COLS * CHUNK_H; }
inline int AtlasLogicalWidth() { return ATLAS_COLS * CHUNK_W; }
inline int AtlasWidth() { return AtlasLogicalWidth() + VISUAL_SWATCH_COLUMNS; }
inline int AtlasHeight() { return ATLAS_ROWS * CHUNK_H; }
inline bool EntryExitCompatible(int Entry, int Exit) { return Entry != Exit; }

inline int MiddlePairIndex(int Entry, int Exit)
{
	if(!EntryExitCompatible(Entry, Exit)) return -1;
	int Pair = 0;
	for(int E = 0; E < 4; E++) for(int X = 0; X < 4; X++) if(E != X)
	{
		if(E == Entry && X == Exit) return Pair;
		Pair++;
	}
	return -1;
}
inline bool FourVariantPair(int Entry, int Exit)
{
	return (Entry == MAPPATH_DIR_LEFT && Exit == MAPPATH_DIR_RIGHT) ||
		(Entry == MAPPATH_DIR_RIGHT && Exit == MAPPATH_DIR_LEFT) ||
		(Entry == MAPPATH_DIR_DOWN && Exit == MAPPATH_DIR_UP) ||
		(Entry == MAPPATH_DIR_LEFT && Exit == MAPPATH_DIR_UP) ||
		(Entry == MAPPATH_DIR_RIGHT && Exit == MAPPATH_DIR_UP);
}
inline int MiddleVariantCount(int Entry, int Exit) { return FourVariantPair(Entry, Exit) ? 4 : 3; }
inline int MiddleTemplateIndex(int Entry, int Exit, int Variant)
{
	if(!EntryExitCompatible(Entry, Exit)) return TEMPL_MIDDLE_FIRST;
	int Offset = 0;
	for(int E = 0; E < 4; E++) for(int X = 0; X < 4; X++) if(E != X)
	{
		if(E == Entry && X == Exit) return TEMPL_MIDDLE_FIRST + Offset + Variant % MiddleVariantCount(E, X);
		Offset += MiddleVariantCount(E, X);
	}
	return TEMPL_MIDDLE_FIRST;
}
inline int StartTemplateIndex(int Exit)
{
	return Exit == MAPPATH_DIR_UP ? TEMPL_START_UP : Exit == MAPPATH_DIR_DOWN ? TEMPL_START_DOWN : TEMPL_START_RIGHT;
}
inline int FinishTemplateIndex(int Entry)
{
	return TEMPL_FINISH_TOP + (Entry & 3);
}
inline void TemplatePorts(int Template, int *pEntry, int *pExit)
{
	if(Template <= TEMPL_START_DOWN)
	{
		*pEntry = MAPPATH_DIR_LEFT;
		*pExit = Template == TEMPL_START_UP ? MAPPATH_DIR_UP : Template == TEMPL_START_DOWN ? MAPPATH_DIR_DOWN : MAPPATH_DIR_RIGHT;
		return;
	}
	if(Template >= TEMPL_FINISH_TOP)
	{
		*pEntry = Template - TEMPL_FINISH_TOP;
		*pExit = *pEntry;
		return;
	}
	int Offset = Template - TEMPL_MIDDLE_FIRST;
	for(int E = 0; E < 4; E++) for(int X = 0; X < 4; X++) if(E != X)
	{
		const int Count = MiddleVariantCount(E, X);
		if(Offset < Count) { *pEntry = E; *pExit = X; return; }
		Offset -= Count;
	}
	*pEntry = MAPPATH_DIR_LEFT; *pExit = MAPPATH_DIR_RIGHT;
}
inline int TemplateVariant(int Template)
{
	if(Template < TEMPL_MIDDLE_FIRST || Template >= TEMPL_FINISH_TOP) return 0;
	int Entry, Exit; TemplatePorts(Template, &Entry, &Exit);
	return Template - MiddleTemplateIndex(Entry, Exit, 0);
}
inline CTemplateSpec TemplateSpec(int Template)
{
	CTemplateSpec S;
	S.m_Index = Template; TemplatePorts(Template, &S.m_EntryDir, &S.m_ExitDir);
	S.m_Variant = TemplateVariant(Template);
	S.m_Start = Template <= TEMPL_START_DOWN;
	S.m_Finish = Template >= TEMPL_FINISH_TOP;
	S.m_HasShortcut = !S.m_Start && !S.m_Finish && S.m_Variant == 3;
	const bool Horizontal = (S.m_EntryDir == MAPPATH_DIR_LEFT || S.m_EntryDir == MAPPATH_DIR_RIGHT) &&
		(S.m_ExitDir == MAPPATH_DIR_LEFT || S.m_ExitDir == MAPPATH_DIR_RIGHT);
	const bool Vertical = (S.m_EntryDir == MAPPATH_DIR_UP || S.m_EntryDir == MAPPATH_DIR_DOWN) &&
		(S.m_ExitDir == MAPPATH_DIR_UP || S.m_ExitDir == MAPPATH_DIR_DOWN);
	if(S.m_Start || S.m_Finish) S.m_Family = FAMILY_RUNWAY;
	else if(S.m_HasShortcut) S.m_Family = FAMILY_SHORTCUT;
	else if(Horizontal) S.m_Family = S.m_Variant == 0 ? FAMILY_RUNWAY : S.m_Variant == 1 ? FAMILY_GAPS : FAMILY_STEPS;
	else if(Vertical) S.m_Family = S.m_ExitDir == MAPPATH_DIR_UP ? (S.m_Variant == 0 ? FAMILY_WALLJUMP : FAMILY_JETPACK) : FAMILY_DROP;
	else S.m_Family = S.m_Variant == 0 ? FAMILY_RUNWAY : S.m_Variant == 1 ? FAMILY_STEPS : FAMILY_HOOK;
	S.m_Difficulty = S.m_Start || S.m_Finish ? 0 : S.m_Variant + (S.m_Family == FAMILY_HOOK || S.m_Family == FAMILY_JETPACK ? 1 : 0);
	return S;
}

inline void CarvePort(CTemplateGrid *pGrid, int Dir)
{
	if(Dir == MAPPATH_DIR_LEFT) pGrid->Rect(0, 10, 19, 17, false);
	else if(Dir == MAPPATH_DIR_RIGHT) pGrid->Rect(20, 10, CHUNK_W - 1, 17, false);
	else if(Dir == MAPPATH_DIR_UP) pGrid->Rect(17, 0, 24, 16, false);
	else pGrid->Rect(17, 15, 24, CHUNK_H - 1, false);
}
inline void AddVerticalRestLedges(CTemplateGrid *pGrid, int Variant)
{
	for(int Y = 10 + Variant % 2; Y < CHUNK_H - 8; Y += 7)
	{
		if(((Y / 7) + Variant) & 1) pGrid->Rect(17, Y, 19, Y, true);
		else pGrid->Rect(21, Y, 23, Y, true);
	}
}
inline void GenerateTemplate(const CTemplateSpec &S, CTemplateGrid *pGrid)
{
	pGrid->Fill(true);
	pGrid->Rect(14, 10, 26, 18, false);
	CarvePort(pGrid, S.m_EntryDir);
	if(!S.m_Finish) CarvePort(pGrid, S.m_ExitDir);

	// Safe ground on the horizontal racing line and side-port approaches.
	pGrid->Rect(0, 18, CHUNK_W - 1, 18, true);
	if(S.m_EntryDir == MAPPATH_DIR_DOWN || S.m_ExitDir == MAPPATH_DIR_DOWN) pGrid->Rect(17, 18, 23, 18, false);
	if(S.m_EntryDir == MAPPATH_DIR_UP || S.m_ExitDir == MAPPATH_DIR_UP) pGrid->Rect(17, 6, 19, 6, true);
	if(S.m_EntryDir == MAPPATH_DIR_DOWN || S.m_ExitDir == MAPPATH_DIR_DOWN) pGrid->Rect(21, 25, 23, 25, true);
	// Directional recovery bays make reverse traversal a distinct racing
	// segment, not merely the same collision room with swapped metadata.
	if(!S.m_Start && !S.m_Finish)
	{
		if(S.m_EntryDir == MAPPATH_DIR_LEFT) pGrid->Rect(3, 8, 9, 12, false);
		else if(S.m_EntryDir == MAPPATH_DIR_RIGHT) pGrid->Rect(30, 8, 36, 12, false);
		else if(S.m_EntryDir == MAPPATH_DIR_UP) pGrid->Rect(23, 2, 29, 7, false);
		else pGrid->Rect(10, 24, 16, 29, false);
	}

	if(S.m_Finish)
	{
		// A broad, unmistakable finish arena reached from the entry port.
		pGrid->Rect(8, 8, 31, 17, false);
		pGrid->Rect(8, 18, 31, 18, true);
		if(S.m_EntryDir == MAPPATH_DIR_DOWN) pGrid->Rect(17, 18, 23, 18, false);
		return;
	}
	if(S.m_Start)
	{
		pGrid->Rect(2, 7, 15, 17, false);
		pGrid->Rect(2, 18, 15, 18, true);
		return;
	}

	const bool Horizontal = (S.m_EntryDir == MAPPATH_DIR_LEFT || S.m_EntryDir == MAPPATH_DIR_RIGHT) &&
		(S.m_ExitDir == MAPPATH_DIR_LEFT || S.m_ExitDir == MAPPATH_DIR_RIGHT);
	const bool HasVertical = S.m_EntryDir == MAPPATH_DIR_UP || S.m_EntryDir == MAPPATH_DIR_DOWN ||
		S.m_ExitDir == MAPPATH_DIR_UP || S.m_ExitDir == MAPPATH_DIR_DOWN;
	if(HasVertical) AddVerticalRestLedges(pGrid, S.m_Variant);

	if(Horizontal && S.m_Variant == 1)
	{
		// A readable four-tile jump with a deep, non-blocking recovery pit.
		pGrid->Rect(17, 18, 20, 29, false);
		pGrid->Rect(13, 21, 16, 21, true);
		pGrid->Rect(21, 20, 25, 20, true);
	}
	else if(Horizontal && S.m_Variant == 2)
	{
		// Three broad stepping platforms; jetpack remains a recovery option.
		pGrid->Rect(9, 18, 30, 29, false);
		pGrid->Rect(9, 21, 14, 21, true);
		pGrid->Rect(17, 18, 22, 18, true);
		pGrid->Rect(25, 22, 30, 22, true);
	}
	else if(Horizontal && S.m_Variant == 3)
	{
		// Main route uses forgiving lower platforms.  The upper straight is a
		// shorter hook/jetpack shortcut with side-mounted hazards.
		pGrid->Rect(8, 18, 31, 29, false);
		pGrid->Rect(8, 22, 14, 22, true);
		pGrid->Rect(17, 20, 23, 20, true);
		pGrid->Rect(26, 22, 31, 22, true);
		pGrid->Rect(6, 3, 33, 8, false);
		pGrid->Rect(6, 9, 33, 9, true);
		pGrid->Rect(6, 8, 8, 17, false);
		pGrid->Rect(31, 8, 33, 17, false);
	}
	else if(HasVertical && S.m_Variant >= 2)
	{
		// A side chamber creates a genuine hook/jetpack line distinct from the
		// ledge-based main shaft while retaining the central safe route.
		const int SideX0 = (S.m_Index & 1) ? 25 : 5;
		const int SideX1 = SideX0 + 9;
		pGrid->Rect(SideX0, 4, SideX1, 13, false);
		pGrid->Rect(SideX0, 14, SideX1, 14, true);
		pGrid->Rect(SideX0 < 17 ? SideX1 : 23, 8, SideX0 < 17 ? 17 : SideX0, 12, false);
	}
	else if(!Horizontal && S.m_Variant == 1)
	{
		// Turn templates use offset landing decks instead of the straight
		// center-floor geometry.
		if(S.m_Index & 1) pGrid->Rect(9, 14, 16, 14, true);
		else pGrid->Rect(24, 14, 31, 14, true);
	}

	// Hazard descriptions are placement-independent candidates. Runtime uses
	// none before segment 12, one for segments 12-17 and up to two thereafter.
	for(int y = 1; y < CHUNK_H - 1; y++)
		for(int x = 1; x < CHUNK_W - 1; x++)
		{
			const bool Clear = !pGrid->Solid(x, y) && !pGrid->Solid(x - 1, y) && !pGrid->Solid(x + 1, y) && !pGrid->Solid(x, y - 1);
			if(Clear && ((y >= 10 && y <= 17) || (x >= 17 && x <= 24)))
				pGrid->m_aMainRoute[y * CHUNK_W + x] = 1;
		}

	// Pick a real floor/ledge instead of a fixed point. A saw and the player
	// together need almost two tiles of radius, so reserve a five-by-five
	// danger footprint and six clear tiles above the mounting surface.
	int SawX = -1, SawY = -1, BestSawScore = 1000000;
	const int DesiredX = Horizontal ? (S.m_Variant == 1 ? 13 : S.m_Variant == 2 ? 20 : 23) : PORT_CENTER_X;
	for(int y = 4; y < CHUNK_H - 3; y++)
		for(int x = 5; x < CHUNK_W - 5; x++)
		{
			if(!pGrid->m_aMainRoute[y * CHUNK_W + x] || pGrid->Solid(x, y) || !pGrid->Solid(x, y + 1)) continue;
			bool Clear = true;
			for(int DY = 0; DY <= 5; DY++)
				for(int DX = -1; DX <= 1; DX++)
					Clear &= !pGrid->Solid(x + DX, y - DY);
			if(!Clear) continue;
			const int Score = abs(x - DesiredX) * 4 + abs(y - 15) + ((x + y + S.m_Index) & 1);
			if(Score < BestSawScore) { BestSawScore = Score; SawX = x; SawY = y; }
		}
	if(SawX >= 0)
	{
		int WarnMinX = SawX - 5, WarnMaxX = SawX - 3, WarnMinY = SawY - 5, WarnMaxY = SawY;
		if(S.m_EntryDir == MAPPATH_DIR_RIGHT) { WarnMinX = SawX + 3; WarnMaxX = SawX + 5; }
		else if(S.m_EntryDir == MAPPATH_DIR_UP) { WarnMinX = SawX - 2; WarnMaxX = SawX + 2; WarnMinY = SawY - 5; WarnMaxY = SawY - 3; }
		else if(S.m_EntryDir == MAPPATH_DIR_DOWN) { WarnMinX = SawX - 2; WarnMaxX = SawX + 2; WarnMinY = SawY + 3; WarnMaxY = SawY + 5; }
		pGrid->AddHazard(HAZARD_SAW, SawX, SawY, SawX - 2, SawY - 2, SawX + 2, SawY + 2,
			WarnMinX, WarnMinY, WarnMaxX, WarnMaxY,
			Horizontal ? ACTION_JUMP : S.m_ExitDir == MAPPATH_DIR_UP ? ACTION_SWITCH_WALL : ACTION_DIVERT);
	}

	// A second trap is optional. Only add a flame when an actual side wall is
	// present, the emitter itself is in air, and it is separated from the saw.
	if(S.m_Variant >= 2 && Horizontal && SawX >= 0)
	{
		for(int y = 12; y <= 16 && pGrid->m_HazardCount < 2; y++)
			for(int x = 5; x < CHUNK_W - 5 && pGrid->m_HazardCount < 2; x++)
			{
				if(pGrid->Solid(x, y) || !pGrid->m_aMainRoute[y * CHUNK_W + x] || abs(x - SawX) < 8) continue;
				const bool WallLeft = pGrid->Solid(x - 1, y);
				const bool WallRight = pGrid->Solid(x + 1, y);
				if(WallLeft == WallRight) continue;
				const int DangerMinX = WallLeft ? x : x - 4;
				const int DangerMaxX = WallLeft ? x + 4 : x;
				pGrid->AddHazard(HAZARD_FLAME, x, y, DangerMinX, y - 2, DangerMaxX, y + 2,
					x - 3, y - 5, x + 3, y - 3, ACTION_WAIT);
			}
	}
	for(int y = 1; y < CHUNK_H - 1; y++)
		for(int x = 1; x < CHUNK_W - 1; x++)
		{
			bool Dangerous = false;
			for(int Hazard = 0; Hazard < pGrid->m_HazardCount; Hazard++)
			{
				const CHazardSpec &H = pGrid->m_aHazards[Hazard];
				Dangerous |= x >= H.m_MinX && x <= H.m_MaxX && y >= H.m_MinY && y <= H.m_MaxY;
			}
			pGrid->m_aSafeRoute[y * CHUNK_W + x] = !Dangerous && !pGrid->Solid(x, y) ? 1 : 0;
		}
}

inline void PortInside(int Dir, int *pX, int *pY)
{
	if(Dir == MAPPATH_DIR_LEFT) { *pX = 2; *pY = PORT_CENTER_Y - 1; }
	else if(Dir == MAPPATH_DIR_RIGHT) { *pX = CHUNK_W - 3; *pY = PORT_CENTER_Y - 1; }
	else if(Dir == MAPPATH_DIR_UP) { *pX = PORT_CENTER_X; *pY = 2; }
	else { *pX = PORT_CENTER_X; *pY = CHUNK_H - 3; }
}
inline void EntryRespawnLocal(int Dir, int *pX, int *pY)
{
	if(Dir == MAPPATH_DIR_LEFT) { *pX = 5; *pY = 17; }
	else if(Dir == MAPPATH_DIR_RIGHT) { *pX = CHUNK_W - 6; *pY = 17; }
	else if(Dir == MAPPATH_DIR_UP) { *pX = 18; *pY = 5; }
	else { *pX = 22; *pY = 24; }
}
inline bool Passable(const CTemplateGrid &G, int X, int Y)
{
	return !G.Solid(X, Y) && !G.Solid(X - 1, Y) && !G.Solid(X + 1, Y) && !G.Solid(X, Y - 1);
}
inline bool ValidateTemplate(const CTemplateSpec &S, const CTemplateGrid &G)
{
	int SX, SY, TX, TY;
	PortInside(S.m_EntryDir, &SX, &SY);
	if(S.m_Finish) { TX = PORT_CENTER_X; TY = 16; }
	else PortInside(S.m_ExitDir, &TX, &TY);
	if(!Passable(G, SX, SY) || !Passable(G, TX, TY)) return false;
	bool Visited[CHUNK_W * CHUNK_H]; int QX[CHUNK_W * CHUNK_H], QY[CHUNK_W * CHUNK_H];
	for(int i = 0; i < CHUNK_W * CHUNK_H; i++) Visited[i] = false;
	int Head = 0, Tail = 0; QX[Tail] = SX; QY[Tail++] = SY; Visited[SY * CHUNK_W + SX] = true;
	bool Reached = false;
	while(Head < Tail)
	{
		const int X = QX[Head], Y = QY[Head++];
		if(X == TX && Y == TY) { Reached = true; break; }
		for(int Dir = 0; Dir < 4; Dir++)
		{
			int DX, DY; CMapPath::DirOffset(Dir, &DX, &DY); const int NX = X + DX, NY = Y + DY;
			if(NX < 1 || NY < 1 || NX >= CHUNK_W - 1 || NY >= CHUNK_H - 1 || Visited[NY * CHUNK_W + NX] || !Passable(G, NX, NY)) continue;
			Visited[NY * CHUNK_W + NX] = true; QX[Tail] = NX; QY[Tail++] = NY;
		}
	}
	if(!Reached) return false;
	if(S.m_Start || S.m_Finish) return G.m_HazardCount == 0;
	if(G.m_HazardCount < 1) return false;

	// Every danger volume must overlap the actual racing corridor.
	for(int Hazard = 0; Hazard < G.m_HazardCount; Hazard++)
	{
		const CHazardSpec &H = G.m_aHazards[Hazard];
		bool IntersectsMain = false;
		bool WarnsMain = false;
		for(int Y = max(0, H.m_MinY); Y <= min(CHUNK_H - 1, H.m_MaxY); Y++)
			for(int X = max(0, H.m_MinX); X <= min(CHUNK_W - 1, H.m_MaxX); X++)
				IntersectsMain |= G.m_aMainRoute[Y * CHUNK_W + X] != 0;
		for(int Y = max(0, H.m_WarnMinY); Y <= min(CHUNK_H - 1, H.m_WarnMaxY); Y++)
			for(int X = max(0, H.m_WarnMinX); X <= min(CHUNK_W - 1, H.m_WarnMaxX); X++)
				WarnsMain |= G.m_aMainRoute[Y * CHUNK_W + X] != 0;
		if(!IntersectsMain || !WarnsMain || H.m_SafeAction < ACTION_JUMP || H.m_SafeAction > ACTION_DIVERT) return false;
	}

	// Treat every active danger volume as blocked and require a static safe
	// action chain. This is stricter than runtime timing-based flame/laser use.
	bool SafeVisited[CHUNK_W * CHUNK_H];
	for(int i = 0; i < CHUNK_W * CHUNK_H; i++) SafeVisited[i] = false;
	Head = Tail = 0; QX[Tail] = SX; QY[Tail++] = SY; SafeVisited[SY * CHUNK_W + SX] = true;
	while(Head < Tail)
	{
		const int X = QX[Head], Y = QY[Head++];
		if(X == TX && Y == TY) return true;
		for(int Dir = 0; Dir < 4; Dir++)
		{
			int DX, DY; CMapPath::DirOffset(Dir, &DX, &DY); const int NX = X + DX, NY = Y + DY;
			if(NX < 1 || NY < 1 || NX >= CHUNK_W - 1 || NY >= CHUNK_H - 1 || SafeVisited[NY * CHUNK_W + NX] || !Passable(G, NX, NY)) continue;
			bool Dangerous = false;
			for(int Hazard = 0; Hazard < G.m_HazardCount; Hazard++)
			{
				const CHazardSpec &H = G.m_aHazards[Hazard];
				Dangerous |= NX >= H.m_MinX && NX <= H.m_MaxX && NY >= H.m_MinY && NY <= H.m_MaxY;
			}
			if(Dangerous) continue;
			SafeVisited[NY * CHUNK_W + NX] = true; QX[Tail] = NX; QY[Tail++] = NY;
		}
	}
	return false;
}
inline unsigned TemplateGeometryHash(const CTemplateGrid &G)
{
	unsigned H = 2166136261u;
	for(int i = 0; i < CHUNK_W * CHUNK_H; i++) { H ^= G.m_aSolid[i]; H *= 16777619u; }
	for(int i = 0; i < G.m_HazardCount; i++) { H ^= (unsigned)(G.m_aHazards[i].m_X + G.m_aHazards[i].m_Y * CHUNK_W + G.m_aHazards[i].m_Type * 131); H *= 16777619u; }
	return H;
}

inline bool GridOccupied(const CCoursePlacement *p, int Count, int X, int Y)
{
	for(int i = 0; i < Count; i++) if(p[i].m_GridX == X && p[i].m_GridY == Y) return true;
	return false;
}
inline bool TouchesNonParent(const CCoursePlacement *p, int Count, int Parent, int X, int Y)
{
	for(int Dir = 0; Dir < 4; Dir++)
	{
		int DX, DY; CMapPath::DirOffset(Dir, &DX, &DY);
		for(int i = 0; i < Count; i++) if(i != Parent && p[i].m_GridX == X + DX && p[i].m_GridY == Y + DY) return true;
	}
	return false;
}
inline int ClampCheckpointCount(int Count)
{
	return Count < MIN_CHECKPOINT_COUNT ? MIN_CHECKPOINT_COUNT : Count > MAX_CHECKPOINT_COUNT ? MAX_CHECKPOINT_COUNT : Count;
}
inline int DifficultyTier(int Segment, int CourseLength)
{
	if(CourseLength <= 1) return 0;
	const int Tier = Segment * 4 / CourseLength;
	return Tier > 3 ? 3 : Tier;
}
inline int DifficultyTier(int Segment) { return DifficultyTier(Segment, COURSE_LENGTH); }
inline int ActiveHazardCount(int Segment, int CourseLength, int CandidateCount)
{
	const int Tier = DifficultyTier(Segment, CourseLength);
	return Tier < 2 ? 0 : min(CandidateCount, Tier == 2 ? 1 : 2);
}
inline int PickMiddleTemplate(int Seed, int Segment, int Entry, int Exit, const CCoursePlacement *pCourse, int CourseLength)
{
	const int Count = MiddleVariantCount(Entry, Exit), Tier = DifficultyTier(Segment, CourseLength);
	int First = Tier == 0 ? 0 : Tier == 1 ? 0 : Tier == 2 ? 1 : Count - 1;
	int Last = Tier == 0 ? (Count > 1 ? 1 : 0) : Tier == 1 ? (Count > 2 ? 2 : Count - 1) : Count - 1;
	if(First > Last) First = Last;
	const int Options = Last - First + 1;
	for(int Try = 0; Try < Options; Try++)
	{
		const int Variant = First + (Variation(Seed, Segment, 31 + Try, Options) + Try) % Options;
		const int Template = MiddleTemplateIndex(Entry, Exit, Variant);
		bool Recent = false;
		for(int i = Segment - 1; i >= 1 && i >= Segment - 4; i--) if(pCourse[i].m_TemplateIndex == Template) Recent = true;
		if(!Recent) return Template;
	}
	return MiddleTemplateIndex(Entry, Exit, First + Variation(Seed, Segment, 55, Options));
}
inline bool BuildRoute(int Seed, int Segment, int X, int Y, int Entry, int PrevTravel, int VerticalStreak,
	int Lefts, int Turns, int Verticals, int StraightRun, CCoursePlacement *pOut, int CourseLength)
{
	pOut[Segment].m_GridX = X; pOut[Segment].m_GridY = Y; pOut[Segment].m_CourseIndex = Segment; pOut[Segment].m_EntryDir = Entry;
	if(Segment == CourseLength - 1)
	{
		const int Checkpoints = CourseLength - 1;
		const int MinForward = max(1, (Checkpoints + 2) / 3);
		const int MinTurns = max(1, (Checkpoints + 3) / 4);
		const int MaxTurns = max(MinTurns, (Checkpoints + 1) / 2);
		const int MinVerticals = max(1, (Checkpoints + 2) / 6);
		const int MaxVerticals = max(MinVerticals, Checkpoints * 2 / 5);
		const int MaxLefts = max(1, Checkpoints / 10);
		if(X < MinForward || Lefts > MaxLefts || Turns < MinTurns || Turns > MaxTurns ||
			Verticals < MinVerticals || Verticals > MaxVerticals) return false;
		pOut[Segment].m_ExitDir = Entry; pOut[Segment].m_TemplateIndex = FinishTemplateIndex(Entry); return true;
	}
	int Choices[3], Num = 0;
	for(int Dir = 0; Dir < 4; Dir++) if(EntryExitCompatible(Entry, Dir)) Choices[Num++] = Dir;
	for(int i = Num - 1; i > 0; i--) { const int J = Variation(Seed, Segment, 80 + i, i + 1); const int T = Choices[i]; Choices[i] = Choices[J]; Choices[J] = T; }
	// Bias each four-segment beat toward forward flow, while retaining seeded vertical transitions.
	for(int Pass = 0; Pass < 2; Pass++) for(int i = 0; i < Num; i++)
	{
		const int Exit = Choices[i];
		if(Pass == 0 && Exit != MAPPATH_DIR_RIGHT && !((Segment + Seed) % 4 == 2 && (Exit == MAPPATH_DIR_UP || Exit == MAPPATH_DIR_DOWN))) continue;
		if(Pass == 1 && Exit == MAPPATH_DIR_RIGHT) continue;
		const bool Vertical = Exit == MAPPATH_DIR_UP || Exit == MAPPATH_DIR_DOWN;
		if(Vertical && VerticalStreak >= MAX_VERTICAL_STREAK) continue;
		const int Checkpoints = CourseLength - 1;
		const int LeftStart = max(2, Checkpoints / 3);
		const int LeftEnd = Checkpoints - max(2, Checkpoints / 8);
		if(Exit == MAPPATH_DIR_LEFT && (Segment < LeftStart || Segment > LeftEnd || Lefts >= max(1, Checkpoints / 10))) continue;
		if(Exit == PrevTravel && StraightRun >= 3) continue;
		int DX, DY; CMapPath::DirOffset(Exit, &DX, &DY); const int NX = X + DX, NY = Y + DY;
		if(NY < 0 || NY >= VERTICAL_BAND || NX < -2 || GridOccupied(pOut, Segment + 1, NX, NY) || TouchesNonParent(pOut, Segment + 1, Segment, NX, NY)) continue;
		pOut[Segment].m_ExitDir = Exit;
		pOut[Segment].m_TemplateIndex = Segment == 0 ? StartTemplateIndex(Exit) : PickMiddleTemplate(Seed, Segment, Entry, Exit, pOut, CourseLength);
		if(BuildRoute(Seed, Segment + 1, NX, NY, Opposite(Exit), Exit, Vertical ? VerticalStreak + 1 : 0,
			Lefts + (Exit == MAPPATH_DIR_LEFT), Turns + (Segment > 0 && Exit != PrevTravel), Verticals + Vertical,
			Exit == PrevTravel ? StraightRun + 1 : 1, pOut, CourseLength)) return true;
	}
	return false;
}
inline int RouteScore(const CCoursePlacement *p, int CourseLength)
{
	int Turns = 0, Vertical = 0, Lefts = 0, RepeatFamily = 0;
	for(int i = 1; i < CourseLength - 1; i++)
	{
		Turns += p[i].m_ExitDir != p[i - 1].m_ExitDir;
		Vertical += p[i].m_ExitDir == MAPPATH_DIR_UP || p[i].m_ExitDir == MAPPATH_DIR_DOWN;
		Lefts += p[i].m_ExitDir == MAPPATH_DIR_LEFT;
		RepeatFamily += TemplateSpec(p[i].m_TemplateIndex).m_Family == TemplateSpec(p[i - 1].m_TemplateIndex).m_Family;
	}
	const int Checkpoints = CourseLength - 1;
	const int TargetTurns = max(1, Checkpoints / 3);
	const int TargetVerticals = max(1, Checkpoints / 4);
	const int TargetLefts = max(0, Checkpoints / 12);
	int Score = 300 - abs(Turns - TargetTurns) * 8 - abs(Vertical - TargetVerticals) * 6;
	Score -= abs(Lefts - TargetLefts) * 5 + RepeatFamily * 2;
	Score += p[CourseLength - 1].m_GridX * 4;
	return Score;
}
inline int GenerateCourse(int Seed, CCoursePlacement *pOut, int CourseLength)
{
	if(!pOut || CourseLength < MIN_CHECKPOINT_COUNT + 1 || CourseLength > MAX_COURSE_LENGTH) return 0;
	CCoursePlacement Best[MAX_COURSE_LENGTH]; int BestScore = -1000000; bool Found = false;
	for(int Attempt = 0; Attempt < ROUTE_ATTEMPTS; Attempt++)
	{
		CCoursePlacement Candidate[MAX_COURSE_LENGTH]; const int AttemptSeed = Seed + Attempt * 7919;
		if(!BuildRoute(AttemptSeed, 0, 0, VERTICAL_BAND / 2, MAPPATH_DIR_LEFT, MAPPATH_DIR_RIGHT, 0, 0, 0, 0, 0, Candidate, CourseLength)) continue;
		const int Score = RouteScore(Candidate, CourseLength);
		if(!Found || Score > BestScore) { for(int i = 0; i < CourseLength; i++) Best[i] = Candidate[i]; BestScore = Score; Found = true; }
	}
	if(!Found) return 0;
	for(int i = 0; i < CourseLength; i++) pOut[i] = Best[i];
	return CourseLength;
}

inline void FillPathInfo(CMapPathInfoData *p, int Count) { p->m_Version=1; p->m_ChunkWidth=CHUNK_W; p->m_ChunkHeight=CHUNK_H; p->m_AtlasColumns=ATLAS_COLS; p->m_TemplateCount=TEMPLATE_COUNT; p->m_PlacementCount=Count; }
inline void ToPathPlacement(const CCoursePlacement &S, CMapPathPlacementData *D) { D->m_GridX=S.m_GridX; D->m_GridY=S.m_GridY; D->m_TemplateIndex=S.m_TemplateIndex; D->m_CourseIndex=S.m_CourseIndex; D->m_EntryDir=S.m_EntryDir; D->m_ExitDir=S.m_ExitDir; }
inline int SpawnLocalX(int Index) { return 5 + Index * 3; }
inline int SpawnLocalY() { return 17; }
}
#endif
