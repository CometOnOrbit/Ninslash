#include <cassert>

#include <base/system.h>
#include <game/challenge_script_runtime.h>

int main()
{
	const CModApiDescriptor Descriptor{ModApiCurrentVersion(), MOD_CAPABILITY_GAMEPLAY_RULES};
	const char *pScript =
		"function on_tick(client, value, tick) "
		"challenge.state_set(0, challenge.state_get(0) + challenge.random(1, 1)) "
		"challenge.player_state_set(0, 0, tick) "
		"end";
	CChallengeScriptRuntime A;
	CChallengeScriptRuntime B;
	char aError[256];
	assert(A.Activate(Descriptor, 1234, aError, sizeof(aError)));
	assert(B.Activate(Descriptor, 1234, aError, sizeof(aError)));
	assert(A.LoadScript("a.challenge.lua", pScript, (int)str_length(pScript), aError, sizeof(aError)));
	assert(B.LoadScript("b.challenge.lua", pScript, (int)str_length(pScript), aError, sizeof(aError)));
	for(int Tick = 0; Tick < 8; ++Tick)
	{
		assert(A.Dispatch(EChallengeScriptEvent::Tick, -1, 0, aError, sizeof(aError)));
		assert(B.Dispatch(EChallengeScriptEvent::Tick, -1, 0, aError, sizeof(aError)));
	}
	assert(A.State().m_aGlobal[0] == 8);
	assert(B.State().m_aGlobal[0] == 8);
	assert(A.State().m_aPlayer[0][0] == B.State().m_aPlayer[0][0]);
	assert(A.Checksum() == B.Checksum());
	const char *pRangeScript =
		"function on_tick() local value = challenge.random(3); "
		"if value < 1 or value > 3 then challenge.command(0) end end";
	CChallengeScriptRuntime Range;
	assert(Range.Activate(Descriptor, 9, aError, sizeof(aError)));
	assert(Range.LoadScript("range.challenge.lua", pRangeScript, (int)str_length(pRangeScript), aError, sizeof(aError)));
	assert(Range.Dispatch(EChallengeScriptEvent::Tick, -1, 0, aError, sizeof(aError)));
	assert(Range.CommandCount() == 0);

	CChallengeScriptRuntime Forbidden;
	assert(Forbidden.Activate(Descriptor, 7, aError, sizeof(aError)));
	const char *pForbidden = "function on_tick() io.open('x') end";
	assert(Forbidden.LoadScript("forbidden.challenge.lua", pForbidden, (int)str_length(pForbidden), aError, sizeof(aError)));
	assert(!Forbidden.Dispatch(EChallengeScriptEvent::Tick, -1, 0, aError, sizeof(aError)));
	return 0;
}
