#include "weapon_script_runtime.h"

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
	MAX_WEAPON_SCRIPT_HANDLERS = 1024,
	SCRIPT_MEMORY_LIMIT = 8 * 1024 * 1024,
	SCRIPT_INSTRUCTION_LIMIT = 10000,
	SCRIPT_EVENT_COUNT = static_cast<int>(EWeaponScriptEvent::Count),
	LUA_HOOK_GRANULARITY = 100,
};

struct CHandler
{
	char m_aStableId[128];
	int m_aLuaRefs[SCRIPT_EVENT_COUNT];
};

lua_State *gs_pState;
CHandler gs_aHandlers[MAX_WEAPON_SCRIPT_HANDLERS];
int gs_HandlerCount;
IWeaponScriptHost *gs_pHost;
int gs_MemoryUsed;
int gs_Instructions;
int gs_CommandCount;
char gs_aPackageId[32];

struct CReloadSnapshot
{
	lua_State *m_pState;
	CHandler m_aHandlers[MAX_WEAPON_SCRIPT_HANDLERS];
	int m_HandlerCount;
	IWeaponScriptHost *m_pHost;
	int m_MemoryUsed;
	int m_Instructions;
	int m_CommandCount;
	char m_aPackageId[32];
};

CReloadSnapshot *gs_pReloadSnapshot;

bool HostCommand(const CWeaponScriptCommand &Command)
{
	if(!gs_pHost || ++gs_CommandCount > 64)
		return false;
	return gs_pHost->ScriptCommand(Command);
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
	if(Total > SCRIPT_MEMORY_LIMIT)
		return 0;
	void *pResult = realloc(pPointer, NewSize);
	if(pResult)
		gs_MemoryUsed = (int)Total;
	return pResult;
}

void Hook(lua_State *pState, lua_Debug *)
{
	gs_Instructions += LUA_HOOK_GRANULARITY;
	if(gs_Instructions > SCRIPT_INSTRUCTION_LIMIT)
		luaL_error(pState, "weapon script instruction budget exceeded");
}

bool ValidStableId(const char *pId)
{
	if(!pId || str_length(pId) >= (int)sizeof(gs_aHandlers[0].m_aStableId))
		return false;
	char aPrefix[48];
	if(str_comp(gs_aPackageId, "official") == 0)
		str_copy(aPrefix, "official:", sizeof(aPrefix));
	else
		str_format(aPrefix, sizeof(aPrefix), "workshop:%s:", gs_aPackageId);
	return str_comp_num(pId, aPrefix, str_length(aPrefix)) == 0;
}

void SetError(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText ? pText : "weapon script error", ErrorSize);
}

int ReadInteger(lua_State *pState, int Index, const char *pName, int Default, int Min, int Max)
{
	lua_getfield(pState, Index, pName);
	int Value = Default;
	if(!lua_isnil(pState, -1))
	{
		if(!lua_isinteger(pState, -1))
			luaL_error(pState, "weapon script field '%s' must be an integer", pName);
		Value = (int)lua_tointeger(pState, -1);
	}
	lua_pop(pState, 1);
	if(Value < Min || Value > Max)
		luaL_error(pState, "weapon script field '%s' is out of range", pName);
	return Value;
}

int ArgumentBase(lua_State *pState)
{
	return lua_istable(pState, 1) ? 2 : 1;
}

CHandler *FindHandler(const char *pStableId)
{
	if(!pStableId)
		return 0;
	for(int i = 0; i < gs_HandlerCount; ++i)
		if(str_comp(gs_aHandlers[i].m_aStableId, pStableId) == 0)
			return &gs_aHandlers[i];
	return 0;
}

int RegisterHandler(lua_State *pState, int Event)
{
	const char *pId = luaL_checkstring(pState, 1);
	luaL_checktype(pState, 2, LUA_TFUNCTION);
	if(!ValidStableId(pId))
		return luaL_error(pState, "weapon event requires a stable ID owned by this package");
	CHandler *pHandler = FindHandler(pId);
	if(!pHandler)
	{
		if(gs_HandlerCount >= MAX_WEAPON_SCRIPT_HANDLERS)
			return luaL_error(pState, "too many weapon script handlers");
		pHandler = &gs_aHandlers[gs_HandlerCount++];
		str_copy(pHandler->m_aStableId, pId, sizeof(pHandler->m_aStableId));
		for(int i = 0; i < SCRIPT_EVENT_COUNT; ++i)
			pHandler->m_aLuaRefs[i] = LUA_NOREF;
	}
	if(pHandler->m_aLuaRefs[Event] != LUA_NOREF)
		return luaL_error(pState, "weapon event handler already registered");
	lua_pushvalue(pState, 2);
	pHandler->m_aLuaRefs[Event] = luaL_ref(pState, LUA_REGISTRYINDEX);
	return 0;
}

int LuaOnEvent(lua_State *pState)
{
	return RegisterHandler(pState, (int)lua_tointeger(pState, lua_upvalueindex(1)));
}

int LuaStateGet(lua_State *pState)
{
	if(!gs_pHost)
		return luaL_error(pState, "weapon context is unavailable");
	const int Index = (int)luaL_checkinteger(pState, ArgumentBase(pState));
	if(Index < 0 || Index >= 8)
		return luaL_error(pState, "weapon state index must be 0..7");
	lua_pushinteger(pState, gs_pHost->ScriptStateGet(Index));
	return 1;
}

int LuaStateSet(lua_State *pState)
{
	if(!gs_pHost)
		return luaL_error(pState, "weapon context is unavailable");
	const int Base = ArgumentBase(pState);
	const int Index = (int)luaL_checkinteger(pState, Base);
	const int Value = (int)luaL_checkinteger(pState, Base + 1);
	if(Index < 0 || Index >= 8)
		return luaL_error(pState, "weapon state index must be 0..7");
	gs_pHost->ScriptStateSet(Index, Value);
	return 0;
}

int LuaRandom(lua_State *pState)
{
	if(!gs_pHost)
		return luaL_error(pState, "weapon context is unavailable");
	const int Base = ArgumentBase(pState);
	const int Count = lua_gettop(pState) - Base + 1;
	const uint32_t Value = gs_pHost->ScriptRandom();
	if(Count == 0)
	{
		lua_pushinteger(pState, (lua_Integer)Value);
		return 1;
	}
	const int Low = (int)luaL_checkinteger(pState, Base);
	const int High = Count > 1 ? (int)luaL_checkinteger(pState, Base + 1) : Low;
	if(High < Low)
		return luaL_error(pState, "weapon random interval is empty");
	lua_pushinteger(pState, Low + (int)(Value % (uint32_t)(High - Low + 1)));
	return 1;
}

int LuaSpawn(lua_State *pState)
{
	if(!gs_pHost)
		return luaL_error(pState, "weapon context is unavailable");
	const int Kind = (int)lua_tointeger(pState, lua_upvalueindex(1));
	const int Base = ArgumentBase(pState);
	luaL_checktype(pState, Base, LUA_TTABLE);
	CWeaponScriptSpawn Spawn{};
	Spawn.m_Kind = Kind;
	Spawn.m_Speed = ReadInteger(pState,
								Base,
								Kind == WEAPON_SCRIPT_SPAWN_RAY ? "range" : "speed",
								Kind == WEAPON_SCRIPT_SPAWN_RAY ? 600 : 900,
								0,
								4000);
	Spawn.m_LifeTicks = ReadInteger(pState, Base, "life", Kind == WEAPON_SCRIPT_SPAWN_RAY ? 3 : 100, 1, 1800);
	Spawn.m_Damage = ReadInteger(pState, Base, "damage", 1, 0, 1000);
	Spawn.m_Radius = ReadInteger(pState, Base, "radius", 6, 0, 512);
	Spawn.m_Bounces = ReadInteger(pState, Base, "bounces", 0, 0, 32);
	Spawn.m_Gravity = ReadInteger(pState, Base, "gravity", 0, -1000, 1000);
	Spawn.m_Count = ReadInteger(pState, Base, "count", 1, 1, 16);
	CWeaponScriptCommand Command{};
	Command.m_Kind = WEAPON_SCRIPT_COMMAND_SPAWN_PROJECTILE + Kind;
	Command.m_aArgs[0] = Spawn.m_Speed;
	Command.m_aArgs[1] = Spawn.m_LifeTicks;
	Command.m_aArgs[2] = Spawn.m_Damage;
	Command.m_aArgs[3] = Spawn.m_Radius;
	Command.m_aArgs[4] = Spawn.m_Bounces;
	Command.m_aArgs[5] = Spawn.m_Gravity;
	Command.m_aArgs[6] = Spawn.m_Count;
	lua_pushboolean(pState, HostCommand(Command));
	return 1;
}

int LuaVisual(lua_State *pState)
{
	if(!gs_pHost)
		return luaL_error(pState, "weapon context is unavailable");
	const int Base = ArgumentBase(pState);
	CWeaponScriptCommand Command{};
	Command.m_Kind = WEAPON_SCRIPT_COMMAND_VISUAL;
	Command.m_aArgs[0] = (int)luaL_checkinteger(pState, Base);
	Command.m_aArgs[1] = (int)luaL_optinteger(pState, Base + 1, 0);
	if(!HostCommand(Command))
		return luaL_error(pState, "visual command rejected by host");
	return 0;
}

int LuaCommand(lua_State *pState)
{
	if(!gs_pHost)
		return luaL_error(pState, "weapon context is unavailable");
	const int Base = ArgumentBase(pState);
	CWeaponScriptCommand Command{};
	Command.m_Kind = (int)lua_tointeger(pState, lua_upvalueindex(1));
	const int ArgumentCount = lua_gettop(pState) - Base + 1;
	if(ArgumentCount > 8)
		return luaL_error(pState, "weapon command accepts at most 8 integer arguments");
	const int Count = ArgumentCount < 8 ? ArgumentCount : 8;
	for(int Index = 0; Index < Count; ++Index)
		Command.m_aArgs[Index] = (int)luaL_checkinteger(pState, Base + Index);
	lua_pushboolean(pState, HostCommand(Command));
	return 1;
}

void RegisterSandbox()
{
	const struct
	{
		const char *m_pName;
		lua_CFunction m_Open;
	} aLibraries[] = {
		{LUA_GNAME, luaopen_base},
		{LUA_TABLIBNAME, luaopen_table},
		{LUA_STRLIBNAME, luaopen_string},
		{LUA_MATHLIBNAME, luaopen_math},
		{LUA_UTF8LIBNAME, luaopen_utf8},
	};
	for(const auto &Library : aLibraries)
	{
		luaL_requiref(gs_pState, Library.m_pName, Library.m_Open, 1);
		lua_pop(gs_pState, 1);
	}
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
	const char *apEvents[SCRIPT_EVENT_COUNT] = {"on_fire",
												"on_tick",
												"on_charge",
												"on_release",
												"on_trigger",
												"on_throw",
												"on_activate",
												"on_collision",
												"on_destroy"};
	for(int Event = 0; Event < SCRIPT_EVENT_COUNT; ++Event)
	{
		lua_pushinteger(gs_pState, Event);
		lua_pushcclosure(gs_pState, LuaOnEvent, 1);
		lua_setfield(gs_pState, -2, apEvents[Event]);
	}
	lua_setglobal(gs_pState, "weapon");
}

void PushContext()
{
	lua_newtable(gs_pState);
	lua_pushcfunction(gs_pState, LuaStateGet);
	lua_setfield(gs_pState, -2, "state_get");
	lua_pushcfunction(gs_pState, LuaStateSet);
	lua_setfield(gs_pState, -2, "state_set");
	lua_pushcfunction(gs_pState, LuaRandom);
	lua_setfield(gs_pState, -2, "random");
	for(int Kind = WEAPON_SCRIPT_SPAWN_PROJECTILE; Kind <= WEAPON_SCRIPT_SPAWN_SUMMON; ++Kind)
	{
		const char *pName = Kind == WEAPON_SCRIPT_SPAWN_PROJECTILE ? "spawn_projectile"
							: Kind == WEAPON_SCRIPT_SPAWN_RAY	   ? "spawn_ray"
							: Kind == WEAPON_SCRIPT_SPAWN_AREA	   ? "spawn_area"
																   : "spawn_summon";
		lua_pushinteger(gs_pState, Kind);
		lua_pushcclosure(gs_pState, LuaSpawn, 1);
		lua_setfield(gs_pState, -2, pName);
	}
	lua_pushcfunction(gs_pState, LuaVisual);
	lua_setfield(gs_pState, -2, "visual");
	const struct
	{
		const char *m_pName;
		int m_Command;
	} aCommands[] = {
		{"timer_set", WEAPON_SCRIPT_COMMAND_TIMER_SET},
		{"ammo_add", WEAPON_SCRIPT_COMMAND_AMMO_ADD},
		{"charge_set", WEAPON_SCRIPT_COMMAND_CHARGE_SET},
		{"explode", WEAPON_SCRIPT_COMMAND_EXPLOSION},
		{"release_weapon", WEAPON_SCRIPT_COMMAND_RELEASE},
		{"controller_trigger", WEAPON_SCRIPT_COMMAND_CONTROLLER_TRIGGER},
		{"drop_pickup", WEAPON_SCRIPT_COMMAND_DROP_PICKUP},
		{"create_electrowall", WEAPON_SCRIPT_COMMAND_ELECTROWALL},
		{"sound", WEAPON_SCRIPT_COMMAND_SOUND},
		{"bomb_trigger", WEAPON_SCRIPT_COMMAND_BOMB_TRIGGER},
	};
	for(const auto &Command : aCommands)
	{
		lua_pushinteger(gs_pState, Command.m_Command);
		lua_pushcclosure(gs_pState, LuaCommand, 1);
		lua_setfield(gs_pState, -2, Command.m_pName);
	}
}

bool FinishProtectedCall(int Result, char *pError, int ErrorSize)
{
	lua_sethook(gs_pState, 0, 0, 0);
	if(Result == LUA_OK)
		return true;
	SetError(pError, ErrorSize, lua_tostring(gs_pState, -1));
	lua_pop(gs_pState, 1);
	return false;
}
} // namespace

void CWeaponScriptRuntime::Reset()
{
	if(gs_pState)
		lua_close(gs_pState);
	gs_pState = 0;
	gs_HandlerCount = 0;
	gs_pHost = 0;
	gs_MemoryUsed = 0;
	gs_Instructions = 0;
	gs_CommandCount = 0;
	gs_aPackageId[0] = 0;
}

void CWeaponScriptRuntime::BeginReload()
{
	dbg_assert(!gs_pReloadSnapshot, "weapon script reload already active");
	CReloadSnapshot *pSnapshot = new CReloadSnapshot;
	pSnapshot->m_pState = gs_pState;
	mem_copy(pSnapshot->m_aHandlers, gs_aHandlers, sizeof(gs_aHandlers));
	pSnapshot->m_HandlerCount = gs_HandlerCount;
	pSnapshot->m_pHost = gs_pHost;
	pSnapshot->m_MemoryUsed = gs_MemoryUsed;
	pSnapshot->m_Instructions = gs_Instructions;
	pSnapshot->m_CommandCount = gs_CommandCount;
	str_copy(pSnapshot->m_aPackageId, gs_aPackageId, sizeof(pSnapshot->m_aPackageId));
	gs_pReloadSnapshot = pSnapshot;
	gs_pState = 0;
	Reset();
}

void CWeaponScriptRuntime::CommitReload()
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

void CWeaponScriptRuntime::RollbackReload()
{
	if(!gs_pReloadSnapshot)
		return;
	Reset();
	CReloadSnapshot *pSnapshot = gs_pReloadSnapshot;
	gs_pState = pSnapshot->m_pState;
	mem_copy(gs_aHandlers, pSnapshot->m_aHandlers, sizeof(gs_aHandlers));
	gs_HandlerCount = pSnapshot->m_HandlerCount;
	gs_pHost = pSnapshot->m_pHost;
	gs_MemoryUsed = pSnapshot->m_MemoryUsed;
	gs_Instructions = pSnapshot->m_Instructions;
	gs_CommandCount = pSnapshot->m_CommandCount;
	str_copy(gs_aPackageId, pSnapshot->m_aPackageId, sizeof(gs_aPackageId));
	delete pSnapshot;
	gs_pReloadSnapshot = 0;
}

bool CWeaponScriptRuntime::LoadPackageScript(
	const char *pPackageId, const char *pName, const char *pSource, int SourceSize, char *pError, int ErrorSize)
{
	if(!pSource || SourceSize <= 0 || SourceSize > 1024 * 1024)
	{
		SetError(pError, ErrorSize, "invalid weapon script size");
		return false;
	}
	if(!gs_pState)
	{
		gs_pState = lua_newstate(Allocate, 0);
		if(!gs_pState)
		{
			SetError(pError, ErrorSize, "unable to create weapon script runtime");
			return false;
		}
		RegisterSandbox();
	}
	str_copy(gs_aPackageId, pPackageId ? pPackageId : "", sizeof(gs_aPackageId));
	gs_Instructions = 0;
	gs_CommandCount = 0;
	lua_sethook(gs_pState, Hook, LUA_MASKCOUNT, LUA_HOOK_GRANULARITY);
	const int Result = luaL_loadbufferx(gs_pState, pSource, SourceSize, pName ? pName : "weapon script", "t") == LUA_OK
						   ? lua_pcall(gs_pState, 0, 0, 0)
						   : LUA_ERRSYNTAX;
	return FinishProtectedCall(Result, pError, ErrorSize);
}

bool CWeaponScriptRuntime::HasHandler(const char *pStableId, EWeaponScriptEvent Event)
{
	const CHandler *pHandler = FindHandler(pStableId);
	const int Value = static_cast<int>(Event);
	return pHandler && Value >= 0 && Value < SCRIPT_EVENT_COUNT && pHandler->m_aLuaRefs[Value] != LUA_NOREF;
}

bool Call(const char *pStableId, int Event, IWeaponScriptHost *pHost, char *pError, int ErrorSize)
{
	if(!gs_pState || !pHost)
		return false;
	const CHandler *pHandler = FindHandler(pStableId);
	const int Ref = pHandler ? pHandler->m_aLuaRefs[Event] : LUA_NOREF;
	if(Ref == LUA_NOREF)
		return false;
	gs_pHost = pHost;
	gs_Instructions = 0;
	gs_CommandCount = 0;
	lua_rawgeti(gs_pState, LUA_REGISTRYINDEX, Ref);
	PushContext();
	lua_sethook(gs_pState, Hook, LUA_MASKCOUNT, LUA_HOOK_GRANULARITY);
	const int Result = lua_pcall(gs_pState, 1, 0, 0);
	gs_pHost = 0;
	return FinishProtectedCall(Result, pError, ErrorSize);
}

bool CWeaponScriptRuntime::Dispatch(
	const char *pStableId, EWeaponScriptEvent Event, IWeaponScriptHost *pHost, char *pError, int ErrorSize)
{
	const int Value = static_cast<int>(Event);
	return Value >= 0 && Value < SCRIPT_EVENT_COUNT && Call(pStableId, Value, pHost, pError, ErrorSize);
}

int CWeaponScriptRuntime::RegisteredWeaponCount()
{
	return gs_HandlerCount;
}
