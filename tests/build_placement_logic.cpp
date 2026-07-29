#include <game/client/components/build_placement_logic.h>

#include <cstdlib>
#include <cstdio>

static void Check(bool Condition, int Line)
{
	if(!Condition)
	{
		std::fprintf(stderr, "check failed at line %d\n", Line);
		std::abort();
	}
}

#define CHECK(Condition) Check((Condition), __LINE__)

int main()
{
	using namespace BuildPlacementLogic;
	const CBuildPlacementResult EmptyResult;
	CHECK(!EmptyResult.m_HasAnchor);
	CHECK(WheelSector(vec2(0, 37), 38.0f) == -1);
	for(int Sector = 0; Sector < 9; ++Sector)
	{
		const float Angle = -pi / 2.0f + 2.0f * pi * Sector / 9.0f;
		CHECK(WheelSector(vec2(cosf(Angle), sinf(Angle)) * 100.0f, 38.0f) == Sector);
	}

	CStateMachine State;
	CHECK(State.State() == STATE_IDLE);
	State.OpenWheel();
	CHECK(State.State() == STATE_WHEEL);
	CHECK(!State.ReleaseWheel(-1, true) && State.State() == STATE_IDLE);
	State.OpenWheel();
	CHECK(State.ReleaseWheel(3, true));
	CHECK(State.State() == STATE_PLACEMENT && State.Selected() == 3);
	State.OpenWheel();
	CHECK(!State.ReleaseWheel(5, false));
	CHECK(State.State() == STATE_PLACEMENT && State.Selected() == 3);
	State.OpenWheel();
	CHECK(!State.ReleaseWheel(-1, true));
	CHECK(State.State() == STATE_PLACEMENT && State.Selected() == 3);
	State.Cancel();
	CHECK(State.State() == STATE_IDLE && State.Selected() == -1);

	CPlacementTrigger Trigger;
	Trigger.SetDown(true);
	CHECK(Trigger.ShouldSend(false, true, 1, 1));
	CHECK(!Trigger.ShouldSend(false, false, 2, 1));
	CHECK(Trigger.ShouldSend(true, false, 1, 1));
	CHECK(!Trigger.ShouldSend(true, false, 1, 1));
	CHECK(Trigger.ShouldSend(true, false, 2, 1));
	Trigger.SetDown(false);
	Trigger.SetDown(true);
	CHECK(Trigger.ShouldSend(true, true, 2, 1));
	return 0;
}
