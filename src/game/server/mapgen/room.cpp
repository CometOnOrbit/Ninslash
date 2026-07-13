#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>
#include <engine/shared/config.h>

#include "room.h"
#include "gen_layer.h"

int CRoom::MinSize() const
{
	if(str_comp(g_Config.m_SvGametype, "extract") == 0)
		return 6;
	return 8;
}

bool CRoom::TooSmall() const
{
	return m_W < MinSize() || m_H < MinSize();
}

// bsp map, acts as template for rooms
CRoom::CRoom(int x, int y, int w, int h)
{
	m_Open = false;
	
	m_X = x;
	m_Y = y;
	m_W = w;
	m_H = h;
	
	m_pChild1 = NULL;
	m_pChild2 = NULL;
	
	int RoomSize = 7+rand()%6;
	if(str_comp(g_Config.m_SvGametype, "extract") == 0)
		RoomSize = 5+rand()%3;
	
	if (m_H < m_W)
	{
		if (m_W > RoomSize+3)
			Split(false);
		if (m_H > RoomSize)
			Split(true);
	}
	else
	{
		if (m_H > RoomSize)
			Split(true);
		if (m_W > RoomSize+3)
			Split(false);
	}
}

CRoom::~CRoom()
{
	if (m_pChild1)
		delete m_pChild1;
	if (m_pChild2)
		delete m_pChild2;
}

void CRoom::Split(bool Vertical)
{
	if (TooSmall())
		return;
		
	if (Vertical)
	{
		int h2 = m_H;
		
		if (m_W < 32)
		{
			const int SplitRange = m_H-6;
			if (SplitRange <= 0)
				return;
			m_H = 3 + rand()%SplitRange;
		}
		else
			m_H = m_H/(2 + rand()%2);
		
		if (!m_pChild1)
			m_pChild1 = new CRoom(m_X, m_Y, m_W, m_H);
		
		if (!m_pChild2)
			m_pChild2 = new CRoom(m_X, m_Y+m_H, m_W, h2-m_H);
	}
	else
	{
		int w2 = m_W;
		
		if (m_H < 32)
		{
			const int SplitRange = m_W-6;
			if (SplitRange <= 0)
				return;
			m_W = 3 + rand()%SplitRange;
		}
		else
			m_W = m_W/(2 + rand()%2);

		if (!m_pChild1)
			m_pChild1 = new CRoom(m_X, m_Y, m_W, m_H);
		
		if (!m_pChild2)
			m_pChild2 = new CRoom(m_X+m_W, m_Y, w2-m_W, m_H);
	}
}


bool CRoom::Open(int x, int y)
{
	bool c1 = false;
	bool c2 = false;
	
	if (m_pChild1)
		c1 = m_pChild1->Open(x, y);
	
	if (m_pChild2)
		c2 = m_pChild2->Open(x, y);
	
	if (m_X <= x && m_X+m_W >= x &&
		m_Y <= y && m_Y+m_H >= y)
	{
		m_Open = true;
		return (!TooSmall() || c1 || c2);
	}
	
	return false;
}

void CRoom::Generate(CGenLayer *pTiles)
{
	//if (TooSmall())
	//	return;
	
	if (!m_pChild1 && m_Open)
		Fill(pTiles, 0, m_X, m_Y, m_W, m_H);
	
	if (m_pChild1)
		m_pChild1->Generate(pTiles);
	
	if (m_pChild2)
		m_pChild2->Generate(pTiles);
}


void CRoom::Fill(CGenLayer *pTiles, int Index, int x, int y, int w, int h)
{
	for(int py = y; py < y+h; py++)
		for(int px = x; px < x+w; px++)
		{
			pTiles->Set(Index, px, py);
		}
}
