#ifndef GAME_NPC_H
#define GAME_NPC_H

#include <engine/shared/protocol.h>

inline int NpcCoreIndex(int Slot)
{
	return MAX_CLIENTS + Slot;
}

inline bool IsNpcCoreIndex(int Index)
{
	return Index >= MAX_CLIENTS && Index < MAX_CHARACTERS;
}

inline int NpcSlotFromCore(int Index)
{
	return Index - MAX_CLIENTS;
}

inline int NpcAllocSlot(const bool *pUsed, int Num)
{
	for(int i = 0; i < Num; i++)
	{
		if(!pUsed[i])
			return i;
	}
	return -1;
}

inline int NpcCountUsed(const bool *pUsed, int Num)
{
	int n = 0;
	for(int i = 0; i < Num; i++)
	{
		if(pUsed[i])
			n++;
	}
	return n;
}

#endif
