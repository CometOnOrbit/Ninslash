#include <cassert>
#include <cstdio>

#include <game/npc.h>

int main()
{
	bool aUsed[4] = {true, false, true, false};
	assert(NpcAllocSlot(aUsed, 4) == 1);
	aUsed[1] = true;
	assert(NpcAllocSlot(aUsed, 4) == 3);
	aUsed[3] = true;
	assert(NpcAllocSlot(aUsed, 4) == -1);

	assert(NpcCoreIndex(0) == MAX_CLIENTS);
	assert(NpcCoreIndex(MAX_NPCS - 1) == MAX_CHARACTERS - 1);
	assert(IsNpcCoreIndex(NpcCoreIndex(0)));
	assert(!IsNpcCoreIndex(0));
	assert(!IsNpcCoreIndex(MAX_CLIENTS - 1));
	assert(!IsNpcCoreIndex(MAX_CHARACTERS));
	assert(NpcSlotFromCore(NpcCoreIndex(5)) == 5);
	assert(NpcCoreIndex(0) >= MAX_CLIENTS);
	assert(NpcCoreIndex(0) < MAX_CHARACTERS);

	bool aCount[3] = {true, false, true};
	assert(NpcCountUsed(aCount, 3) == 2);
	assert(NpcCountUsed(aUsed, 4) == 4);

	bool aPresent[MAX_CHARACTERS];
	for(int i = 0; i < MAX_CHARACTERS; i++)
		aPresent[i] = i < MAX_CLIENTS;
	assert(NpcCoreIndex(0) >= MAX_CLIENTS);
	assert(!aPresent[NpcCoreIndex(0)]);
	for(int i = 0; i < MAX_CHARACTERS; i++)
		aPresent[i] = true;
	assert(aPresent[NpcCoreIndex(0)]);
	return 0;
}
