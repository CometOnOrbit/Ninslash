#include <cstdio>
#include <game/abyss_angler.h>

static int Fail(const char *pWhat)
{
	fprintf(stderr, "abyss angler: %s\n", pWhat);
	return 1;
}

int main()
{
	if(DROIDTYPE_BOSSANGLER != 18 || NUM_DROIDTYPES != 23)
		return Fail("type id");
	for(int Act = 0; Act < ANGLER_ACT_DEATH; Act++)
		if(AnglerActFromSnap(AnglerAct(Act).m_Anim, DROIDSTATUS_IDLE) != Act)
			return Fail("act round trip");
	if(AnglerActFromSnap(DROIDANIM_MOVE, DROIDSTATUS_TERMINATED) != ANGLER_ACT_DEATH)
		return Fail("death act");
	if(AnglerBiteLungeTick() >= AnglerBiteChompTick() ||
	   AnglerBiteChompTick() >= WardenTicks(AnglerAct(ANGLER_ACT_BITE).m_Duration) ||
	   AnglerLureChompTick() >= WardenTicks(AnglerAct(ANGLER_ACT_LURE).m_Duration) ||
	   AnglerDiveEndTick() >= WardenTicks(AnglerAct(ANGLER_ACT_DIVE).m_Duration) ||
	   AnglerSpitTick(2) >= WardenTicks(AnglerAct(ANGLER_ACT_SPIT).m_Duration))
		return Fail("event inside clip");
	if(AnglerClipTime(ANGLER_ACT_SWIM, 0, 100, 0.0f) != 2.0f - 1.6f || AnglerClipTime(ANGLER_ACT_BITE, 0, 500, 0.0f) != 1.3f)
		return Fail("clip time");
	// Mirrored: the lure sits forward and up whichever way it faces.
	if(AnglerPartAt(106.0f, -126.0f, 1) != ANGLER_PART_LURE || AnglerPartAt(-106.0f, -126.0f, -1) != ANGLER_PART_LURE ||
	   AnglerPartAt(-106.0f, -126.0f, 1) == ANGLER_PART_LURE)
		return Fail("lure mirror");
	if(AnglerPartAt(-225.0f, 0.0f, 1) != ANGLER_PART_FINS || AnglerPartAt(90.0f, 36.0f, 1) != ANGLER_PART_JAW ||
	   AnglerPartAt(0.0f, 0.0f, -1) != ANGLER_PART_CORE || AnglerPartAt(0.0f, -900.0f, 1) != ANGLER_PART_CORE)
		return Fail("part lookup");
	if(AnglerIncomingDamage(10, ANGLER_PART_LURE, false) != 15 || AnglerIncomingDamage(20, ANGLER_PART_CORE, true) != 27 ||
	   AnglerIncomingDamage(20, ANGLER_PART_CORE, false) != 20 || AnglerIncomingDamage(20, ANGLER_PART_FINS, true) != 20)
		return Fail("part damage");
	printf("abyss angler: PASS\n");
	return 0;
}
