#include "mod_runtime.h"

#include <base/system.h>

#if defined(CONF_LUA_MODS)
extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <stdlib.h>

namespace
{
class CLuaModRuntime : public ILuaModRuntime
{
	lua_State *m_pState;
	CModApiDescriptor m_Descriptor;
	unsigned int m_RandomState;
	int m_MemoryUsed;
	int m_InstructionCount;
	bool m_Active;

	static void *Allocator(void *pUser, void *pPointer, size_t OldSize, size_t NewSize)
	{
		CLuaModRuntime *pSelf = static_cast<CLuaModRuntime *>(pUser);
		if(!pPointer) OldSize = 0;
		if(NewSize == 0)
		{
			free(pPointer);
			pSelf->m_MemoryUsed -= (int)OldSize;
			if(pSelf->m_MemoryUsed < 0) pSelf->m_MemoryUsed = 0;
			return 0;
		}
		const long long NewTotal = (long long)pSelf->m_MemoryUsed - (long long)OldSize + (long long)NewSize;
		if(NewTotal > LUA_MOD_MEMORY_LIMIT)
			return 0;
		void *pResult = realloc(pPointer, NewSize);
		if(pResult) pSelf->m_MemoryUsed = (int)NewTotal;
		return pResult;
	}

	static void InstructionHook(lua_State *pState, lua_Debug *pDebug)
	{
		(void)pDebug;
		void *pUser = 0;
		lua_getallocf(pState, &pUser);
		CLuaModRuntime *pSelf = static_cast<CLuaModRuntime *>(pUser);
		pSelf->m_InstructionCount += 1000;
		if(pSelf->m_InstructionCount > LUA_MOD_INSTRUCTION_BUDGET)
			luaL_error(pState, "mod instruction budget exceeded");
	}

	static int LuaRandom(lua_State *pState)
	{
		CLuaModRuntime *pSelf = static_cast<CLuaModRuntime *>(lua_touserdata(pState, lua_upvalueindex(1)));
		unsigned int X = pSelf->m_RandomState ? pSelf->m_RandomState : 0x6d2b79f5u;
		X ^= X << 13; X ^= X >> 17; X ^= X << 5;
		pSelf->m_RandomState = X;
		const int Args = lua_gettop(pState);
		if(Args == 0) { lua_pushnumber(pState, X / 4294967296.0); return 1; }
		lua_Integer Low = 1, High = luaL_checkinteger(pState, 1);
		if(Args >= 2) { Low = High; High = luaL_checkinteger(pState, 2); }
		if(High < Low) return luaL_error(pState, "empty deterministic random interval");
		lua_pushinteger(pState, Low + (lua_Integer)(X % (unsigned long long)(High - Low + 1)));
		return 1;
	}

	void RegisterSandbox()
	{
		struct CLibrary { const char *m_pName; lua_CFunction m_Open; };
		const CLibrary aLibraries[] = {
			{LUA_GNAME, luaopen_base}, {LUA_COLIBNAME, luaopen_coroutine}, {LUA_TABLIBNAME, luaopen_table},
			{LUA_STRLIBNAME, luaopen_string}, {LUA_MATHLIBNAME, luaopen_math}, {LUA_UTF8LIBNAME, luaopen_utf8}};
		for(unsigned i = 0; i < sizeof(aLibraries) / sizeof(aLibraries[0]); i++)
		{
			luaL_requiref(m_pState, aLibraries[i].m_pName, aLibraries[i].m_Open, 1);
			lua_pop(m_pState, 1);
		}
		const char *apDisabled[] = {"io", "os", "debug", "package", "require", "loadfile", "dofile"};
		for(unsigned i = 0; i < sizeof(apDisabled) / sizeof(apDisabled[0]); i++) { lua_pushnil(m_pState); lua_setglobal(m_pState, apDisabled[i]); }
		lua_newtable(m_pState);
		lua_pushlightuserdata(m_pState, this);
		lua_pushcclosure(m_pState, LuaRandom, 1);
		lua_setfield(m_pState, -2, "random");
		lua_pushinteger(m_pState, ModApiCurrentVersion());
		lua_setfield(m_pState, -2, "api_version");
		lua_setglobal(m_pState, "ninslash");
		lua_getglobal(m_pState, "math");
		if(lua_istable(m_pState, -1))
		{
			lua_pushlightuserdata(m_pState, this);
			lua_pushcclosure(m_pState, LuaRandom, 1);
			lua_setfield(m_pState, -2, "random");
			lua_pushnil(m_pState);
			lua_setfield(m_pState, -2, "randomseed");
		}
		lua_pop(m_pState, 1);
	}

	bool CopyError(char *pError, int ErrorSize)
	{
		const char *pMessage = m_pState ? lua_tostring(m_pState, -1) : "Lua runtime unavailable";
		if(pError && ErrorSize > 0) str_copy(pError, pMessage ? pMessage : "Lua error", ErrorSize);
		if(m_pState) lua_pop(m_pState, 1);
		m_Active = false;
		return false;
	}

public:
	CLuaModRuntime() : m_pState(0), m_RandomState(1), m_MemoryUsed(0), m_InstructionCount(0), m_Active(false) { mem_zero(&m_Descriptor, sizeof(m_Descriptor)); }
	~CLuaModRuntime() { Deactivate(); }
	EModActivationResult Activate(const CModApiDescriptor &Descriptor)
	{
		Deactivate();
		const EModActivationResult Result = ModApiCanActivate(Descriptor);
		if(Result != MOD_ACTIVATION_OK) return Result;
		m_pState = lua_newstate(Allocator, this);
		if(!m_pState) return MOD_ACTIVATION_RUNTIME_UNAVAILABLE;
		m_Descriptor = Descriptor;
		RegisterSandbox();
		m_Active = true;
		return MOD_ACTIVATION_OK;
	}
	void Deactivate()
	{
		m_Active = false;
		if(m_pState) { lua_close(m_pState); m_pState = 0; }
		m_MemoryUsed = 0;
	}
	bool Active() const { return m_Active; }
	bool LoadScript(const char *pName, const char *pSource, int SourceSize, char *pError, int ErrorSize)
	{
		if(!m_Active || !m_pState || !pSource || SourceSize <= 0) return false;
		m_InstructionCount = 0;
		lua_sethook(m_pState, InstructionHook, LUA_MASKCOUNT, 1000);
		if(luaL_loadbufferx(m_pState, pSource, SourceSize, pName ? pName : "mod", "t") != LUA_OK || lua_pcall(m_pState, 0, 0, 0) != LUA_OK)
			return CopyError(pError, ErrorSize);
		lua_sethook(m_pState, 0, 0, 0);
		return true;
	}
	void OnModEvent(EModEvent Event, int ClientID, int Value)
	{
		if(!m_Active || !m_pState) return;
		lua_getglobal(m_pState, "on_event");
		if(!lua_isfunction(m_pState, -1)) { lua_pop(m_pState, 1); return; }
		lua_pushinteger(m_pState, Event);
		lua_pushinteger(m_pState, ClientID);
		lua_pushinteger(m_pState, Value);
		m_InstructionCount = 0;
		lua_sethook(m_pState, InstructionHook, LUA_MASKCOUNT, 1000);
		if(lua_pcall(m_pState, 3, 0, 0) != LUA_OK) CopyError(0, 0);
		lua_sethook(m_pState, 0, 0, 0);
	}
	void SetRandomSeed(unsigned int Seed) { m_RandomState = Seed ? Seed : 1; }
	int MemoryUsed() const { return m_MemoryUsed; }
};
}

ILuaModRuntime *CreateLuaModRuntime() { return new CLuaModRuntime(); }
#else
ILuaModRuntime *CreateLuaModRuntime() { return 0; }
#endif
