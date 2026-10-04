#include <base/system.h>
#include <base/math.h>
#include <base/vmath.h>
#include <engine/shared/config.h>

#include "room.h"
#include "gen_layer.h"

int CRoomGenerated::MinSize() const
{
	if(str_comp(g_Config.m_SvGametype, "extract") == 0)
		return 6;
	return 8;
}

bool CRoomGenerated::TooSmall() const
{
	return m_W < MinSize() || m_H < MinSize();
}

CRoomGenerated *CRoomGenerated::Join(int x, int y, int w, int h, CRoomGenerated *pA, CRoomGenerated *pB)
{
	CRoomGenerated *p = new CRoomGenerated(x, y, w, h, ROOM_LEAF);
	p->m_pChild1 = pA;
	p->m_pChild2 = pB;
	return p;
}

CRoomGenerated *CRoomGenerated::CreateBoss(int RoomX, int RoomY, int RoomW, int RoomH)
{
	int ArenaX, ArenaY, ArenaW, ArenaH;
	if(!BossArenaRect(RoomX, RoomY, RoomW, RoomH, &ArenaX, &ArenaY, &ArenaW, &ArenaH))
		return new CRoomGenerated(RoomX, RoomY, RoomW, RoomH);

	CRoomGenerated *pBand = new CRoomGenerated(ArenaX, ArenaY, ArenaW, ArenaH, ROOM_LEAF);
	const int LeftW = ArenaX - RoomX;
	if(LeftW >= 8)
		pBand = Join(RoomX, ArenaY, LeftW + ArenaW, ArenaH, new CRoomGenerated(RoomX, ArenaY, LeftW, ArenaH), pBand);
	const int RightX = ArenaX + ArenaW;
	const int RightW = RoomX + RoomW - RightX;
	if(RightW >= 8)
		pBand = Join(pBand->m_X, ArenaY, RoomX + RoomW - pBand->m_X, ArenaH, pBand, new CRoomGenerated(RightX, ArenaY, RightW, ArenaH));

	CRoomGenerated *pAll = pBand;
	const int TopH = ArenaY - RoomY;
	if(TopH >= 8)
		pAll = Join(RoomX, RoomY, RoomW, ArenaY + ArenaH - RoomY, new CRoomGenerated(RoomX, RoomY, RoomW, TopH), pAll);
	const int BotY = ArenaY + ArenaH;
	const int BotH = RoomY + RoomH - BotY;
	if(BotH >= 8)
		pAll = Join(RoomX, pAll->m_Y, RoomW, RoomY + RoomH - pAll->m_Y, pAll, new CRoomGenerated(RoomX, BotY, RoomW, BotH));
	return pAll;
}

// bsp map, acts as template for rooms
CRoomGenerated::CRoomGenerated(int x, int y, int w, int h, int Kind)
{
	m_Open = false;

	m_X = x;
	m_Y = y;
	m_W = w;
	m_H = h;

	m_pChild1 = 0;
	m_pChild2 = 0;

	if(Kind == ROOM_LEAF)
		return;

	int RoomSize = 7 + irandom(6);
	if(str_comp(g_Config.m_SvGametype, "extract") == 0)
		RoomSize = 5 + irandom(3);

	if(m_H < m_W)
	{
		if(m_W > RoomSize + 3)
			Split(false);
		if(m_H > RoomSize)
			Split(true);
	}
	else
	{
		if(m_H > RoomSize)
			Split(true);
		if(m_W > RoomSize + 3)
			Split(false);
	}
}

CRoomGenerated::~CRoomGenerated()
{
	if(m_pChild1)
		delete m_pChild1;
	if(m_pChild2)
		delete m_pChild2;
}

void CRoomGenerated::Split(bool Vertical)
{
	if(TooSmall())
		return;

	if(Vertical)
	{
		int h2 = m_H;

		if(m_W < 32)
		{
			const int SplitRange = m_H - 6;
			if(SplitRange <= 0)
				return;
			m_H = 3 + irandom(SplitRange);
		}
		else
			m_H = m_H / (2 + irandom(2));

		if(!m_pChild1)
			m_pChild1 = new CRoomGenerated(m_X, m_Y, m_W, m_H);

		if(!m_pChild2)
			m_pChild2 = new CRoomGenerated(m_X, m_Y + m_H, m_W, h2 - m_H);
	}
	else
	{
		int w2 = m_W;

		if(m_H < 32)
		{
			const int SplitRange = m_W - 6;
			if(SplitRange <= 0)
				return;
			m_W = 3 + irandom(SplitRange);
		}
		else
			m_W = m_W / (2 + irandom(2));

		if(!m_pChild1)
			m_pChild1 = new CRoomGenerated(m_X, m_Y, m_W, m_H);

		if(!m_pChild2)
			m_pChild2 = new CRoomGenerated(m_X + m_W, m_Y, w2 - m_W, m_H);
	}
}

bool CRoomGenerated::Open(int x, int y)
{
	bool c1 = false;
	bool c2 = false;

	if(m_pChild1)
		c1 = m_pChild1->Open(x, y);

	if(m_pChild2)
		c2 = m_pChild2->Open(x, y);

	if(m_X <= x && m_X + m_W >= x && m_Y <= y && m_Y + m_H >= y)
	{
		m_Open = true;
		return (!TooSmall() || c1 || c2);
	}

	return false;
}

void CRoomGenerated::Generate(CGenLayer *pTiles)
{
	// if (TooSmall())
	//	return;

	if(!m_pChild1 && m_Open)
		Fill(pTiles, 0, m_X, m_Y, m_W, m_H);

	if(m_pChild1)
		m_pChild1->Generate(pTiles);

	if(m_pChild2)
		m_pChild2->Generate(pTiles);
}

void CRoomGenerated::Fill(CGenLayer *pTiles, int Index, int x, int y, int w, int h)
{
	for(int py = y; py < y + h; py++)
		for(int px = x; px < x + w; px++)
		{
			pTiles->Set(Index, px, py);
		}
}
