#include <math.h>
#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include "mapchunk.h"

inline int GetCalculated(float x, float y, int Max)
{
	if(Max <= 0)
		return 0;
	return min(Max, abs(int(sin((x + 6531.38593f) * 337.123f * cos((y + 8641.5123f) * 6173.147f)) * Max)));
}

int CMapChunk::ModPositive(int X, int M)
{
	if(M <= 0)
		return 0;
	int R = X % M;
	return R < 0 ? R + M : R;
}

int CMapChunk::PickIndex(int AnchorIndex, int X, int NumChunks, const int *apGenerationRules)
{
	if(NumChunks <= 0 || !apGenerationRules)
		return 0;

	int aCandidates[4];
	int Count = 0;
	if(AnchorIndex >= 0 && AnchorIndex < NumChunks)
	{
		for(int i = 0; i < 4; i++)
		{
			const int Rule = apGenerationRules[AnchorIndex * 4 + i];
			if(Rule >= 0 && Rule < NumChunks)
				aCandidates[Count++] = Rule;
		}
	}

	if(Count <= 0)
		return 0;

	return aCandidates[GetCalculated(X * 3.1467f, X / 3.0f, 64) % Count];
}

CMapChunk::CMapChunk(int X, int SizeX, int NumChunks, int *apGenerationRules, CMapChunk *pPrev)
{
	m_X = X;
	m_SizeX = SizeX;

	m_pPrev = pPrev;
	m_pNext = 0;

	m_NumChunks = NumChunks;
	m_apGenerationRules = apGenerationRules;

	if(m_pPrev)
		m_ChunkIndex = PickIndex(m_pPrev->GetIndex(), X, NumChunks, apGenerationRules);
	else
		m_ChunkIndex = 0;
}

CMapChunk::~CMapChunk()
{
	// Only free the right side; DestroyChain walks left first.
	delete m_pNext;
	m_pNext = 0;
}

void CMapChunk::DestroyChain(CMapChunk *pAny)
{
	if(!pAny)
		return;

	while(pAny->m_pPrev)
		pAny = pAny->m_pPrev;

	delete pAny;
}

CMapChunk *CMapChunk::GetMapChunk(int X)
{
	// ponytail: reject one-shot teleports over 128 chunks. Continuous movement
	// remains unbounded; a sparse indexed chain can replace this ceiling if needed.
	const int MaxLookupChunks = 128;
	const int64 Delta = X < m_X ? (int64)m_X - X :
		(X >= m_X + m_SizeX ? (int64)X - (m_X + m_SizeX) + 1 : 0);
	if(m_SizeX <= 0 || Delta > (int64)m_SizeX * MaxLookupChunks)
		return this;

	CMapChunk *pChunk = this;
	while(X < pChunk->m_X)
	{
		if(!pChunk->m_pPrev)
		{
			pChunk->m_pPrev =
				new CMapChunk(pChunk->m_X - pChunk->m_SizeX, pChunk->m_SizeX, pChunk->m_NumChunks, pChunk->m_apGenerationRules, 0);
			pChunk->m_pPrev->m_ChunkIndex =
				PickIndex(pChunk->GetIndex(), pChunk->m_pPrev->m_X, pChunk->m_NumChunks, pChunk->m_apGenerationRules);
			pChunk->m_pPrev->m_pNext = pChunk;
		}
		pChunk = pChunk->m_pPrev;
	}
	while(X >= pChunk->m_X + pChunk->m_SizeX)
	{
		if(!pChunk->m_pNext)
			pChunk->m_pNext =
				new CMapChunk(pChunk->m_X + pChunk->m_SizeX, pChunk->m_SizeX, pChunk->m_NumChunks, pChunk->m_apGenerationRules, pChunk);
		pChunk = pChunk->m_pNext;
	}
	return pChunk;
}

CMapChunk *CMapChunk::FreeOutside(int LowX, int HighX)
{
	if(LowX > HighX)
	{
		const int Tmp = LowX;
		LowX = HighX;
		HighX = Tmp;
	}

	CMapChunk *pFirst = GetMapChunk(LowX);
	while(pFirst->m_pPrev && pFirst->m_pPrev->m_X + pFirst->m_pPrev->m_SizeX > LowX)
		pFirst = pFirst->m_pPrev;

	if(pFirst->m_pPrev)
	{
		CMapChunk *pDiscard = pFirst->m_pPrev;
		pFirst->m_pPrev = 0;
		pDiscard->m_pNext = 0;
		DestroyChain(pDiscard);
	}

	CMapChunk *pLast = pFirst;
	while(pLast->m_pNext && pLast->m_pNext->m_X <= HighX)
		pLast = pLast->m_pNext;
	if(pLast->m_pNext)
	{
		CMapChunk *pDiscard = pLast->m_pNext;
		pLast->m_pNext = 0;
		pDiscard->m_pPrev = 0;
		delete pDiscard;
	}

	return pFirst;
}
