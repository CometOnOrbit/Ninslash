#include <math.h>
#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>

#include <game/server/gamecontext.h>
#include "blockentities.h"

CStoredEntity::CStoredEntity(int ObjType, int Type, int Subtype, int x, int y)
{
	m_pNext = 0;
	m_aStats[0] = ObjType;
	m_aStats[1] = Type;
	m_aStats[2] = Subtype;
	m_aStats[3] = x;
	m_aStats[4] = y;
}

CStoredEntity::~CStoredEntity()
{
	if(m_pNext)
		delete m_pNext;
}

void CStoredEntity::Add(int ObjType, int Type, int Subtype, int x, int y)
{
	if(m_pNext)
		m_pNext->Add(ObjType, Type, Subtype, x, y);
	else
		m_pNext = new CStoredEntity(ObjType, Type, Subtype, x, y);
}

void CStoredEntity::Restore(CGameContext *pGameServer)
{
	pGameServer->RestoreEntity(m_aStats[0], m_aStats[1], m_aStats[2], m_aStats[3], m_aStats[4]);

	if(m_pNext)
		m_pNext->Restore(pGameServer);
}

CBlockEntities::~CBlockEntities()
{
	if(m_pStoredEntities)
		delete m_pStoredEntities;

	// Only free the right side; DestroyChain walks left first.
	delete m_pNext;
	m_pNext = 0;
}

void CBlockEntities::DestroyChain(CBlockEntities *pAny)
{
	if(!pAny)
		return;

	while(pAny->m_pPrev)
		pAny = pAny->m_pPrev;

	delete pAny;
}

bool CBlockEntities::AddSpawnLocal(vec2 Pos)
{
	if(m_NumSpawns >= 9)
		return false;
	m_aSpawn[m_NumSpawns++] = Pos;
	return true;
}

bool CBlockEntities::AddSpawn(vec2 Pos)
{
	if(Pos.x < m_X)
	{
		if(!m_pPrev)
			return false;

		return m_pPrev->AddSpawn(Pos);
	}

	if(Pos.x >= m_X + m_SizeX)
	{
		if(!m_pNext)
			return false;

		return m_pNext->AddSpawn(Pos);
	}

	return AddSpawnLocal(Pos);
}

bool CBlockEntities::GetSpawn(vec2 *Pos)
{
	for(CBlockEntities *pChunk = this; pChunk; pChunk = pChunk->m_pPrev)
	{
		if(pChunk->m_NumSpawns <= 0)
			continue;
		*Pos = pChunk->m_aSpawn[rand() % pChunk->m_NumSpawns] * 32;
		return true;
	}
	return false;
}

CBlockEntities::CBlockEntities(CGameContext *pGameServer, int X, int SizeX, CBlockEntities *pPrev, bool Activate)
{
	m_X = X;
	m_SizeX = SizeX;
	m_EntitiesCreated = false;

	m_pPrev = pPrev;
	m_pNext = 0;

	// if (Activate)
	//	pGameServer->CreateEntitiesForBlock(m_X/m_SizeX);

	m_pStoredEntities = 0;
	m_NumSpawns = 0;
}

void CBlockEntities::StoreEntity(int ObjType, int Type, int Subtype, int x, int y)
{
	if(m_pStoredEntities)
		m_pStoredEntities->Add(ObjType, Type, Subtype, x, y);
	else
		m_pStoredEntities = new CStoredEntity(ObjType, Type, Subtype, x, y);
}

CBlockEntities *CBlockEntities::GetBlockEntities(CGameContext *pGameServer, int X, bool Activate)
{
	if(Activate)
	{
		if(!m_EntitiesCreated)
		{
			pGameServer->CreateEntitiesForBlock(m_X / m_SizeX);
			m_EntitiesCreated = true;
		}

		if(m_pStoredEntities)
		{
			m_pStoredEntities->Restore(pGameServer);
			delete m_pStoredEntities;
			m_pStoredEntities = 0;

			// pGameServer->CreateEnemiesForBlock(m_X/m_SizeX);
		}
	}

	if(X < m_X)
	{
		if(!m_pPrev)
		{
			m_pPrev = new CBlockEntities(pGameServer, m_X - m_SizeX, m_SizeX, 0, Activate);
			m_pPrev->m_pNext = this;
		}
		return m_pPrev->GetBlockEntities(pGameServer, X, Activate);
	}

	if(X >= m_X + m_SizeX)
	{
		if(!m_pNext)
			m_pNext = new CBlockEntities(pGameServer, m_X + m_SizeX, m_SizeX, this, Activate);

		return m_pNext->GetBlockEntities(pGameServer, X, Activate);
	}

	if(Activate)
	{
		/*
		if (m_pStoredEntities)
		{
			m_pStoredEntities->Restore(pGameServer);
			delete m_pStoredEntities;
			m_pStoredEntities = 0;
		}
		*/
	}

	return this;
}

CBlockEntities *CBlockEntities::FreeOutside(CGameContext *pGameServer, int LowX, int HighX)
{
	if(LowX > HighX)
	{
		const int Tmp = LowX;
		LowX = HighX;
		HighX = Tmp;
	}

	CBlockEntities *pKeep = GetBlockEntities(pGameServer, (LowX + HighX) / 2, false);
	CBlockEntities *pLeft = pKeep;
	while(pLeft->m_pPrev)
		pLeft = pLeft->m_pPrev;

	// ponytail: activated chunks retain persistent entity state; only empty
	// speculative tails are safe to reclaim until that state has an archive.
	while(pLeft != pKeep && pLeft->m_X + pLeft->m_SizeX < LowX &&
		  !pLeft->m_EntitiesCreated && !pLeft->m_pStoredEntities)
	{
		CBlockEntities *pNext = pLeft->m_pNext;
		pLeft->m_pNext = 0;
		pNext->m_pPrev = 0;
		delete pLeft;
		pLeft = pNext;
	}

	CBlockEntities *pRight = pKeep;
	while(pRight->m_pNext)
		pRight = pRight->m_pNext;
	while(pRight != pKeep && pRight->m_X > HighX &&
		  !pRight->m_EntitiesCreated && !pRight->m_pStoredEntities)
	{
		CBlockEntities *pPrev = pRight->m_pPrev;
		pPrev->m_pNext = 0;
		pRight->m_pPrev = 0;
		delete pRight;
		pRight = pPrev;
	}

	return pKeep;
}