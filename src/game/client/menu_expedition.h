#ifndef GAME_CLIENT_MENU_EXPEDITION_H
#define GAME_CLIENT_MENU_EXPEDITION_H

#include <base/system.h>

#include <game/expedition_save.h>

enum
{
	EXPEDITION_UI_SLOTS = 0,
	EXPEDITION_UI_LOBBY,
};

struct CExpeditionSlotCard
{
	int m_Slot;
	bool m_Occupied;
	bool m_Corrupt;
	int m_Floor;
	int m_NumPlayers;
	char m_aPlayerLine[96];
};

inline void ExpeditionFormatPlayerLine(const CExpeditionPlayer *pPlayers, int NumPlayers, char *pBuf, int Size)
{
	if(!pBuf || Size <= 0)
		return;
	pBuf[0] = 0;
	if(!pPlayers || NumPlayers <= 0)
		return;
	int Used = 0;
	const int Count = NumPlayers < 3 ? NumPlayers : 3;
	for(int i = 0; i < Count; i++)
	{
		if(i && Used + 2 < Size)
		{
			pBuf[Used++] = ',';
			pBuf[Used++] = ' ';
			pBuf[Used] = 0;
		}
		const char *pName = pPlayers[i].m_aName;
		while(*pName && Used + 1 < Size)
			pBuf[Used++] = *pName++;
		pBuf[Used] = 0;
	}
	if(NumPlayers > 3 && Used + 2 < Size)
	{
		pBuf[Used++] = ' ';
		pBuf[Used++] = '+';
		pBuf[Used] = 0;
	}
}

inline void FillExpeditionSlotCard(int Slot,
								   int LoadResult,
								   int Floor,
								   int NumPlayers,
								   const CExpeditionPlayer *pPlayers,
								   CExpeditionSlotCard *pCard)
{
	pCard->m_Slot = Slot;
	pCard->m_Occupied = LoadResult == EXPEDITION_LOAD_OK;
	pCard->m_Corrupt = LoadResult == EXPEDITION_LOAD_CORRUPT || LoadResult == EXPEDITION_LOAD_FUTURE_VERSION;
	pCard->m_Floor = pCard->m_Occupied && Floor > 0 ? Floor : 1;
	pCard->m_NumPlayers = pCard->m_Occupied ? NumPlayers : 0;
	pCard->m_aPlayerLine[0] = 0;
	if(pCard->m_Occupied)
		ExpeditionFormatPlayerLine(pPlayers, NumPlayers, pCard->m_aPlayerLine, sizeof(pCard->m_aPlayerLine));
}

#endif
