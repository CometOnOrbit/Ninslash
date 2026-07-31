#include <engine/shared/mapchunk.h>

#include <assert.h>

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

int main()
{
	TestModPositive();
	TestZeroRulesFallback();
	TestGrowRightAndLeft();
	TestOutOfRangeRulesSkipped();
	return 0;
}
