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

	bool aCount[3] = {true, false, true};
	assert(NpcCountUsed(aCount, 3) == 2);
	assert(NpcCountUsed(aUsed, 4) == 4);
	return 0;
}
