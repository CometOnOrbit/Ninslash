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
	assert(pRuntime->Activate(Descriptor) == MOD_ACTIVATION_OK);
	const char *pBytecode = "return load(string.dump(function() end))";
	assert(!pRuntime->LoadScript("bytecode", pBytecode, (int)strlen(pBytecode), aError, sizeof(aError)));
	assert(pRuntime->Activate(Descriptor) == MOD_ACTIVATION_OK);
	const char *pPattern =
		"function on_event() return string.rep('a', 40):match(string.rep('a*', 40) .. 'b') end";
	assert(pRuntime->LoadScript("pattern", pPattern, (int)strlen(pPattern), aError, sizeof(aError)));
	pRuntime->OnModEvent(MOD_EVENT_ROUND_START, 0, 0);
	assert(!pRuntime->Active());
	assert(pRuntime->Activate(Descriptor) == MOD_ACTIVATION_OK);
	const char *pPlain =
		"function on_event() local s = string.rep('a', 200000) return s:find(string.rep('a', 100000) .. 'b', 1, true) end";
	assert(pRuntime->LoadScript("plain", pPlain, (int)strlen(pPlain), aError, sizeof(aError)));
	pRuntime->OnModEvent(MOD_EVENT_ROUND_START, 0, 0);
	assert(!pRuntime->Active());
	delete pRuntime;
	return 0;
}
