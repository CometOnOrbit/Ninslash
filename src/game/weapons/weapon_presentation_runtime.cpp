#include "weapon_presentation_runtime.h"

#include <base/system.h>

extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdlib>

namespace
{
enum
{
	MAX_PRESENTATION_HANDLERS = 1024,
	PRESENTATION_MEMORY_LIMIT = 4 * 1024 * 1024,
	PRESENTATION_INSTRUCTION_LIMIT = 5000,
	LUA_HOOK_GRANULARITY = 100,
};

struct CHandler
{
	char m_aStableId[128];
	int m_LuaRef;
};

lua_State *gs_pState;
CHandler gs_aHandlers[MAX_PRESENTATION_HANDLERS];
int gs_HandlerCount;
int gs_MemoryUsed;
int gs_Instructions;
char gs_aPackageId[32];
IWeaponPresentationHost *gs_pHost;

struct CReloadSnapshot
{
	lua_State *m_pState;
	CHandler m_aHandlers[MAX_PRESENTATION_HANDLERS];
	int m_HandlerCount;
	int m_MemoryUsed;
	int m_Instructions;
	char m_aPackageId[32];
	IWeaponPresentationHost *m_pHost;
};

CReloadSnapshot *gs_pReloadSnapshot;

void SetError(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText ? pText : "presentation script error", ErrorSize);
}

void *Allocate(void *, void *pPointer, size_t OldSize, size_t NewSize)
{
	if(!pPointer)
		OldSize = 0;
	if(NewSize == 0)
	{
		free(pPointer);
		gs_MemoryUsed -= (int)OldSize;
		return 0;
	}
	const long long Total = (long long)gs_MemoryUsed - (long long)OldSize + (long long)NewSize;
	if(Total > PRESENTATION_MEMORY_LIMIT)
		return 0;
	void *pResult = realloc(pPointer, NewSize);
	if(pResult)
		gs_MemoryUsed = (int)Total;
	return pResult;
}

void InstructionHook(lua_State *pState, lua_Debug *)
{
	gs_Instructions += LUA_HOOK_GRANULARITY;
	if(gs_Instructions > PRESENTATION_INSTRUCTION_LIMIT)
		luaL_error(pState, "presentation script instruction budget exceeded");
}

int ArgumentBase(lua_State *pState)
{
	return lua_istable(pState, 1) ? 2 : 1;
}

bool ValidStableId(const char *pStableId)
{
	if(!pStableId || str_length(pStableId) >= (int)sizeof(gs_aHandlers[0].m_aStableId))
		return false;
	char aPrefix[48];
	str_format(aPrefix, sizeof(aPrefix), "workshop:%s:", gs_aPackageId);
	return str_comp_num(pStableId, aPrefix, str_length(aPrefix)) == 0;
}

int RegisterHudHandler(lua_State *pState)
{
	const char *pStableId = luaL_checkstring(pState, 1);
	luaL_checktype(pState, 2, LUA_TFUNCTION);
	if(!ValidStableId(pStableId) || gs_HandlerCount >= MAX_PRESENTATION_HANDLERS)
		return luaL_error(pState, "invalid presentation.on_hud registration");
	for(int i = 0; i < gs_HandlerCount; ++i)
		if(str_comp(gs_aHandlers[i].m_aStableId, pStableId) == 0)
			return luaL_error(pState, "duplicate presentation handler");
	CHandler &Handler = gs_aHandlers[gs_HandlerCount++];
	str_copy(Handler.m_aStableId, pStableId, sizeof(Handler.m_aStableId));
	lua_pushvalue(pState, 2);
	Handler.m_LuaRef = luaL_ref(pState, LUA_REGISTRYINDEX);
	return 0;
}

bool RequireHost(lua_State *pState)
{
	if(gs_pHost)
		return true;
	luaL_error(pState, "presentation context unavailable");
	return false;
}

int LuaState(lua_State *pState)
{
	if(!RequireHost(pState))
		return 0;
	const int Index = (int)luaL_checkinteger(pState, ArgumentBase(pState));
	if(Index < 0 || Index >= 8)
		return luaL_error(pState, "invalid presentation state");
	lua_pushinteger(pState, gs_pHost->PresentationStateGet(Index));
	return 1;
}

int LuaText(lua_State *pState)
{
	if(!RequireHost(pState))
		return 0;
	const int Base = ArgumentBase(pState);
	gs_pHost->PresentationText(luaL_checkstring(pState, Base),
							   (int)luaL_checkinteger(pState, Base + 1),
							   (int)luaL_checkinteger(pState, Base + 2),
							   (int)luaL_optinteger(pState, Base + 3, 12));
	return 0;
}

int LuaBar(lua_State *pState)
{
	if(!RequireHost(pState))
		return 0;
	const int Base = ArgumentBase(pState);
	gs_pHost->PresentationBar((int)luaL_checkinteger(pState, Base),
							  (int)luaL_checkinteger(pState, Base + 1),
							  (int)luaL_checkinteger(pState, Base + 2),
							  (int)luaL_checkinteger(pState, Base + 3),
							  (int)luaL_checkinteger(pState, Base + 4),
							  (int)luaL_optinteger(pState, Base + 5, 6));
	return 0;
}

void PushContext()
{
	lua_newtable(gs_pState);
	lua_pushcfunction(gs_pState, LuaState);
	lua_setfield(gs_pState, -2, "state");
	lua_pushcfunction(gs_pState, LuaText);
	lua_setfield(gs_pState, -2, "text");
	lua_pushcfunction(gs_pState, LuaBar);
	lua_setfield(gs_pState, -2, "bar");
}

void RegisterSandbox()
{
	luaL_requiref(gs_pState, LUA_GNAME, luaopen_base, 1);
	lua_pop(gs_pState, 1);
	const char *apForbidden[] = {"io",
								 "os",
								 "debug",
								 "package",
								 "require",
								 "load",
								 "loadfile",
								 "dofile",
								 "collectgarbage",
								 "coroutine",
								 "math",
								 "pairs",
								 "next"};
	for(const char *pName : apForbidden)
	{
		lua_pushnil(gs_pState);
		lua_setglobal(gs_pState, pName);
	}
	lua_newtable(gs_pState);
	lua_pushcfunction(gs_pState, RegisterHudHandler);
	lua_setfield(gs_pState, -2, "on_hud");
	lua_setglobal(gs_pState, "presentation");
}

bool RunChunk(const char *pName, const char *pSource, int SourceSize, char *pError, int ErrorSize)
{
	gs_Instructions = 0;
	lua_sethook(gs_pState, InstructionHook, LUA_MASKCOUNT, LUA_HOOK_GRANULARITY);
	const int Result = luaL_loadbufferx(gs_pState, pSource, SourceSize, pName ? pName : "presentation", "t") == LUA_OK
						   ? lua_pcall(gs_pState, 0, 0, 0)
						   : LUA_ERRSYNTAX;
	lua_sethook(gs_pState, 0, 0, 0);
	if(Result == LUA_OK)
		return true;
	SetError(pError, ErrorSize, lua_tostring(gs_pState, -1));
	lua_pop(gs_pState, 1);
	return false;
}

int FindHandler(const char *pStableId)
{
	if(!pStableId)
		return -1;
	for(int i = 0; i < gs_HandlerCount; ++i)
		if(str_comp(gs_aHandlers[i].m_aStableId, pStableId) == 0)
			return i;
	return -1;
}
} // namespace

void CWeaponPresentationRuntime::Reset()
{
	if(gs_pState)
		lua_close(gs_pState);
	gs_pState = 0;
	gs_HandlerCount = 0;
	gs_MemoryUsed = 0;
	gs_Instructions = 0;
	gs_aPackageId[0] = 0;
	gs_pHost = 0;
}

void CWeaponPresentationRuntime::BeginReload()
{
	dbg_assert(!gs_pReloadSnapshot, "weapon presentation reload already active");
	CReloadSnapshot *pSnapshot = new CReloadSnapshot;
	pSnapshot->m_pState = gs_pState;
	mem_copy(pSnapshot->m_aHandlers, gs_aHandlers, sizeof(gs_aHandlers));
	pSnapshot->m_HandlerCount = gs_HandlerCount;
	pSnapshot->m_MemoryUsed = gs_MemoryUsed;
	pSnapshot->m_Instructions = gs_Instructions;
	str_copy(pSnapshot->m_aPackageId, gs_aPackageId, sizeof(pSnapshot->m_aPackageId));
	pSnapshot->m_pHost = gs_pHost;
	gs_pReloadSnapshot = pSnapshot;
	gs_pState = 0;
	Reset();
}

void CWeaponPresentationRuntime::CommitReload()
{
	if(!gs_pReloadSnapshot)
		return;
	if(gs_pReloadSnapshot->m_pState)
	{
		const int CandidateMemoryUsed = gs_MemoryUsed;
		gs_MemoryUsed = gs_pReloadSnapshot->m_MemoryUsed;
		lua_close(gs_pReloadSnapshot->m_pState);
		gs_MemoryUsed = CandidateMemoryUsed;
	}
	delete gs_pReloadSnapshot;
	gs_pReloadSnapshot = 0;
}

void CWeaponPresentationRuntime::RollbackReload()
{
	if(!gs_pReloadSnapshot)
		return;
	Reset();
	CReloadSnapshot *pSnapshot = gs_pReloadSnapshot;
	gs_pState = pSnapshot->m_pState;
	mem_copy(gs_aHandlers, pSnapshot->m_aHandlers, sizeof(gs_aHandlers));
	gs_HandlerCount = pSnapshot->m_HandlerCount;
	gs_MemoryUsed = pSnapshot->m_MemoryUsed;
	gs_Instructions = pSnapshot->m_Instructions;
	str_copy(gs_aPackageId, pSnapshot->m_aPackageId, sizeof(gs_aPackageId));
	gs_pHost = pSnapshot->m_pHost;
	delete pSnapshot;
	gs_pReloadSnapshot = 0;
}

bool CWeaponPresentationRuntime::LoadPackageScript(
	const char *pPackageId, const char *pName, const char *pSource, int SourceSize, char *pError, int ErrorSize)
{
	if(!pSource || SourceSize <= 0 || SourceSize > 1024 * 1024)
	{
		SetError(pError, ErrorSize, "invalid presentation script size");
		return false;
	}
	if(!gs_pState)
	{
		gs_pState = lua_newstate(Allocate, 0);
		if(!gs_pState)
		{
			SetError(pError, ErrorSize, "unable to create presentation runtime");
			return false;
		}
		RegisterSandbox();
	}
	str_copy(gs_aPackageId, pPackageId ? pPackageId : "", sizeof(gs_aPackageId));
	return RunChunk(pName, pSource, SourceSize, pError, ErrorSize);
}

bool CWeaponPresentationRuntime::HasHudHandler(const char *pStableId)
{
	return FindHandler(pStableId) >= 0;
}

bool CWeaponPresentationRuntime::RenderHud(const char *pStableId,
										   IWeaponPresentationHost *pHost,
										   char *pError,
										   int ErrorSize)
{
	if(!gs_pState || !pHost)
		return false;
	const int HandlerIndex = FindHandler(pStableId);
	if(HandlerIndex < 0)
		return false;
	gs_pHost = pHost;
	gs_Instructions = 0;
	lua_rawgeti(gs_pState, LUA_REGISTRYINDEX, gs_aHandlers[HandlerIndex].m_LuaRef);
	PushContext();
	lua_sethook(gs_pState, InstructionHook, LUA_MASKCOUNT, LUA_HOOK_GRANULARITY);
	const int Result = lua_pcall(gs_pState, 1, 0, 0);
	lua_sethook(gs_pState, 0, 0, 0);
	gs_pHost = 0;
	if(Result == LUA_OK)
		return true;
	SetError(pError, ErrorSize, lua_tostring(gs_pState, -1));
	lua_pop(gs_pState, 1);
	return false;
}
