#include <base/system.h>

#include <assert.h>

namespace
{
struct CWorker
{
	int m_ID;
};

void Worker(void *pUser)
{
	const CWorker *pWorker = static_cast<const CWorker *>(pUser);
	void *apAllocations[64] = {0};
	for(int i = 0; i < 50000; i++)
	{
		const int Slot = (i * 17 + pWorker->m_ID * 11) % 64;
		mem_free(apAllocations[Slot]);
		const unsigned Size = 1 + (unsigned)((i * 37 + pWorker->m_ID * 101) % 2048);
		apAllocations[Slot] = mem_alloc(Size, 16);
		assert(apAllocations[Slot]);
		mem_zero(apAllocations[Slot], Size);
	}
	for(void *pAllocation : apAllocations)
		mem_free(pAllocation);
}
}

int main()
{
	CWorker aWorkers[8];
	void *apThreads[8];
	for(int i = 0; i < 8; i++)
	{
		aWorkers[i].m_ID = i;
		apThreads[i] = thread_init(Worker, &aWorkers[i]);
		assert(apThreads[i]);
	}
	for(void *pThread : apThreads)
		thread_wait(pThread);
	mem_check();
	return 0;
}
