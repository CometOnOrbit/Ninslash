#include <engine/shared/mapchunk.h>
#include <engine/shared/mappath.h>

#include <assert.h>
#include <stdio.h>

static void TestModPositive()
{
	assert(CMapChunk::ModPositive(5, 32) == 5);
	assert(CMapChunk::ModPositive(32, 32) == 0);
	assert(CMapChunk::ModPositive(-1, 32) == 31);
	assert(CMapChunk::ModPositive(-32, 32) == 0);
	assert(CMapChunk::ModPositive(-33, 32) == 31);
}

static void TestZeroRulesFallback()
{
	int aRules[3 * 4] = {0};
	CMapChunk *pRoot = new CMapChunk(0, 32, 3, aRules, 0);
	assert(pRoot->GetIndex() == 0);

	CMapChunk *pRight = pRoot->GetMapChunk(40);
	assert(pRight != pRoot);
	assert(pRight->GetIndex() >= 0 && pRight->GetIndex() < 3);
	assert(pRight->GetX() == 32);

	CMapChunk::DestroyChain(pRoot);
}

static void TestGrowRightAndLeft()
{
	int aRules[4 * 4] = {
		0, 1, 2, 3, // chunk 0
		0, 1, 2, 3, // chunk 1
		0, 1, 2, 3, // chunk 2
		0, 1, 2, 3, // chunk 3
	};

	CMapChunk *pRoot = new CMapChunk(0, 16, 4, aRules, 0);
	assert(pRoot->GetIndex() == 0);

	for(int i = 1; i <= 5; i++)
	{
		CMapChunk *p = pRoot->GetMapChunk(i * 16);
		assert(p->GetX() == i * 16);
		assert(p->GetIndex() >= 0 && p->GetIndex() < 4);
	}

	for(int i = 1; i <= 5; i++)
	{
		CMapChunk *p = pRoot->GetMapChunk(-i * 16);
		assert(p->GetX() == -i * 16);
		assert(p->GetIndex() >= 0 && p->GetIndex() < 4);
	}

	// Mapping stays consistent for negative world tiles.
	CMapChunk *pLeft = pRoot->GetMapChunk(-20);
	const int Size = pLeft->GetSize();
	const int Mapped = CMapChunk::ModPositive(-20, Size) + pLeft->GetIndex() * Size;
	assert(Mapped >= 0);
	assert(Mapped < 4 * Size);

	CMapChunk::DestroyChain(pLeft); // destroy from any node
}

static void TestOutOfRangeRulesSkipped()
{
	int aRules[2 * 4] = {
		2, 2, 2, 2, // invalid (>= NumChunks) — should fall back to 0
		0, 0, 0, 0,
	};

	CMapChunk *pRoot = new CMapChunk(0, 8, 2, aRules, 0);
	CMapChunk *pRight = pRoot->GetMapChunk(8);
	assert(pRight->GetIndex() == 0);
	CMapChunk::DestroyChain(pRoot);
}

static void TestFreeOutside()
{
	int aRules[4 * 4] = {
		0, 1, 2, 3,
		0, 1, 2, 3,
		0, 1, 2, 3,
		0, 1, 2, 3,
	};
	CMapChunk *pRoot = new CMapChunk(0, 16, 4, aRules, 0);
	pRoot->GetMapChunk(-160);
	pRoot->GetMapChunk(160);

	CMapChunk *pKept = pRoot->FreeOutside(-32, 32);
	assert(pKept->GetX() == -32);
	assert(pKept->GetMapChunk(-32)->GetX() == -32);
	assert(pKept->GetMapChunk(32)->GetX() == 32);

	CMapChunk::DestroyChain(pKept);
}

static void TestPathologicalLookupRejected()
{
	int aRules[4] = {0, 0, 0, 0};
	CMapChunk *pRoot = new CMapChunk(0, 16, 1, aRules, 0);
	assert(pRoot->GetMapChunk(-67108864) == pRoot);
	assert(pRoot->GetMapChunk(67108864) == pRoot);
	CMapChunk::DestroyChain(pRoot);
}

static void TestMapPath2D()
{
	CMapPathInfoData Info = {};
	Info.m_Version = 1;
	Info.m_ChunkWidth = 56;
	Info.m_ChunkHeight = 20;
	Info.m_AtlasColumns = 7;
	Info.m_TemplateCount = 27;
	Info.m_PlacementCount = 3;

	CMapPathPlacementData aPlacements[3] = {
		{0, 2, 1, 0, MAPPATH_DIR_LEFT, MAPPATH_DIR_UP},
		{0, 1, 12, 1, MAPPATH_DIR_DOWN, MAPPATH_DIR_RIGHT},
		{1, 1, 24, 2, MAPPATH_DIR_LEFT, MAPPATH_DIR_RIGHT},
	};

	CMapPath Path;
	assert(Path.Init(&Info, aPlacements, 3));
	assert(Path.PlacementCount() == 3);
	assert(Path.Placement(0)->m_TemplateIndex == 1);
	assert(Path.PlacementAtGrid(0, 1)->m_CourseIndex == 1);
	assert(!Path.PlacementAtGrid(9, 9));

	assert(CMapPath::FloorDiv(-1, 56) == -1);
	assert(CMapPath::FloorDiv(-56, 56) == -1);
	assert(CMapPath::FloorDiv(-57, 56) == -2);
	assert(CMapPath::ModPositive(-1, 56) == 55);
	assert(CMapPath::ModPositive(-57, 56) == 55);

	int AtlasX = -1, AtlasY = -1, Course = -1;
	assert(Path.ResolveTile(-1, 2 * 20 + 5, &AtlasX, &AtlasY, &Course) == false);

	assert(Path.ResolveTile(3, 2 * 20 + 5, &AtlasX, &AtlasY, &Course));
	assert(Course == 0);
	assert(AtlasX == (1 % 7) * 56 + 3);
	assert(AtlasY == (1 / 7) * 20 + 5);

	assert(Path.ResolveTile(56 + 4, 20 + 6, &AtlasX, &AtlasY, &Course));
	assert(Course == 2);
	assert(AtlasX == (24 % 7) * 56 + 4);
	assert(AtlasY == (24 / 7) * 20 + 6);

	assert(!Path.HasPlacementAtWorldTile(2000, 2000));
	assert(Path.HasPlacementAtWorldTile(10, 45));

	// Visual lookups add background everywhere and exactly one foreground
	// swatch tile next to a placement without changing collision resolution.
	assert(Path.ResolveVisualTile(-1, 45, &AtlasX, &AtlasY, &Course));
	assert(AtlasX == Info.m_AtlasColumns * Info.m_ChunkWidth + 3);
	assert(Course == -1);
	assert(!Path.ResolveTile(-1, 45, &AtlasX, &AtlasY));
	assert(Path.ResolveVisualTile(-2, 45, &AtlasX, &AtlasY, &Course));
	assert(AtlasX >= Info.m_AtlasColumns * Info.m_ChunkWidth && AtlasX < Info.m_AtlasColumns * Info.m_ChunkWidth + 3);

	// Invalid atlas references and ambiguous world cells must not turn blank
	// atlas space into a walkable/rendered MapPath cell.
	CMapPathPlacementData BadTemplate[1] = {{0, 0, 27, 0, MAPPATH_DIR_LEFT, MAPPATH_DIR_RIGHT}};
	CMapPathInfoData One = Info;
	One.m_PlacementCount = 1;
	assert(!Path.Init(&One, BadTemplate, 1));
	CMapPathPlacementData Duplicate[2] = {
		{0, 0, 1, 0, MAPPATH_DIR_LEFT, MAPPATH_DIR_RIGHT},
		{0, 0, 2, 1, MAPPATH_DIR_LEFT, MAPPATH_DIR_RIGHT},
	};
	One.m_PlacementCount = 2;
	assert(!Path.Init(&One, Duplicate, 2));
}

static void TestMapPathCourseFreezeMarkers()
{
	// Minimal stand-in for roam freeze: once frozen, marker count must stay put.
	struct Marker
	{
		int CourseOrdinal;
	};
	Marker aCP[16];
	int NumCP = 0;
	bool Frozen = false;

	auto Add = [&](int Ordinal) {
		if(Frozen)
			return;
		for(int i = 0; i < NumCP; i++)
			if(aCP[i].CourseOrdinal == Ordinal)
				return;
		aCP[NumCP++].CourseOrdinal = Ordinal;
	};

	for(int i = 0; i < 15; i++)
		Add(i);
	assert(NumCP == 15);
	Frozen = true;
	Add(0);
	Add(99);
	assert(NumCP == 15);
}

int main()
{
	TestModPositive();
	TestZeroRulesFallback();
	TestGrowRightAndLeft();
	TestOutOfRangeRulesSkipped();
	TestFreeOutside();
	TestPathologicalLookupRejected();
	TestMapPath2D();
	TestMapPathCourseFreezeMarkers();
	printf("mapchunk ok\n");
	return 0;
}
