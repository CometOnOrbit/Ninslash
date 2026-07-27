#include <engine/shared/mod_runtime.h>

#include <assert.h>
#include <string.h>

int main()
{
	ILuaModRuntime *pRuntime = CreateLuaModRuntime();
	assert(pRuntime);
	CModApiDescriptor Descriptor = {ModApiCurrentVersion(), MOD_CAPABILITY_GAMEPLAY_RULES};
	assert(pRuntime->Activate(Descriptor) == MOD_ACTIVATION_OK);
	char aError[128];
	const char *pSafe = "value=0; function on_event(e,c,v) value=value+ninslash.random(1,1)+v end";
	assert(pRuntime->LoadScript("safe", pSafe, (int)strlen(pSafe), aError, sizeof(aError)));
	pRuntime->OnModEvent(MOD_EVENT_ROUND_START, 0, 2);
	assert(pRuntime->Active());
	const char *pUnsafe = "return io.open('bad','w')";
	assert(!pRuntime->LoadScript("unsafe", pUnsafe, (int)strlen(pUnsafe), aError, sizeof(aError)));
	assert(!pRuntime->Active());
	assert(pRuntime->Activate(Descriptor) == MOD_ACTIVATION_OK);
	const char *pLoop = "function on_event() while true do end end";
	assert(pRuntime->LoadScript("loop", pLoop, (int)strlen(pLoop), aError, sizeof(aError)));
	pRuntime->OnModEvent(MOD_EVENT_ROUND_START, 0, 0);
	assert(!pRuntime->Active());
	delete pRuntime;
	return 0;
}
