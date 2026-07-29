#include <engine/platform_events.h>
#include <engine/shared/platform_event_queue.h>

#include <assert.h>

int main()
{
	CPlatformEventQueue Queue;
	assert(Queue.Add(PLATFORM_EVENT_FIRST_INVASION, 0, false));
	assert(Queue.Add(PLATFORM_EVENT_FIRST_INVASION, 0, true));
	assert(Queue.Add(PLATFORM_EVENT_LB_INVASION_FLOOR, 10, true));
	assert(Queue.Add(PLATFORM_EVENT_LB_INVASION_FLOOR, 30, true));
	assert(Queue.Add(PLATFORM_EVENT_LB_FIXED_SEED_TIME_MS, 5000, true));
	assert(Queue.Add(PLATFORM_EVENT_LB_FIXED_SEED_TIME_MS, 4000, true));
	assert(Queue.Add(PLATFORM_EVENT_STAT_COOP_COMPLETIONS, 1, false));
	assert(Queue.Add(PLATFORM_EVENT_STAT_COOP_COMPLETIONS, 1, false));
	assert(Queue.Count() == 4);
	char aText[1024];
	assert(Queue.WriteText(aText, sizeof(aText)) > 0);
	CPlatformEventQueue Loaded;
	assert(Loaded.ReadText(aText));
	assert(Loaded.Count() == 4);
	Loaded.RemoveFirst();
	assert(Loaded.Count() == 3);
	assert(!Loaded.ReadText("bad input\n"));
	return 0;
}
