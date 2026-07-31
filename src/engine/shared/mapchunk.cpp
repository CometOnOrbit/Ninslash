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
	if(X < m_X)
	{
		if(!m_pPrev)
		{
			m_pPrev = new CMapChunk(m_X - m_SizeX, m_SizeX, m_NumChunks, m_apGenerationRules, 0);
			m_pPrev->m_ChunkIndex = PickIndex(GetIndex(), m_pPrev->m_X, m_NumChunks, m_apGenerationRules);
			m_pPrev->m_pNext = this;
		}
		return m_pPrev->GetMapChunk(X);
	}

	if(X >= m_X + m_SizeX)
	{
		if(!m_pNext)
			m_pNext = new CMapChunk(m_X + m_SizeX, m_SizeX, m_NumChunks, m_apGenerationRules, this);

		return m_pNext->GetMapChunk(X);
	}

	return this;
}
