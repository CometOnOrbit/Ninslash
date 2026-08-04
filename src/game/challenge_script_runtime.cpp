#include "challenge_script_runtime.h"

#include <base/system.h>

extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdlib>
#include <climits>

namespace
{
enum
{
	SCRIPT_MEMORY_LIMIT = 2 * 1024 * 1024,
	SCRIPT_INSTRUCTION_LIMIT = 20000,
	SCRIPT_COMMAND_LIMIT = 32,
	SCRIPT_HOOK_GRANULARITY = 100,
};

const char *EventName(EChallengeScriptEvent Event)
{
	static const char *s_apNames[] = {"on_round_start",
		"on_round_end",
		"on_tick",
		"on_player_spawn",
		"on_player_death",
		"on_damage",
		"on_pickup",
		"on_build",
		"on_forge",
		"on_floor_complete"};
	const int Index = static_cast<int>(Event);
	return Index >= 0 && Index < (int)(sizeof(s_apNames) / sizeof(s_apNames[0])) ? s_apNames[Index] : "";
}

void SetError(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText ? pText : "challenge script error", ErrorSize);
}

} // namespace

struct CChallengeScriptRuntime::CImpl
{
	lua_State *m_pState;
	CChallengeScriptState m_State;
	CChallengeScriptCommand m_aCommands[SCRIPT_COMMAND_LIMIT];
	int m_CommandCount;
	int m_MemoryUsed;
	int m_InstructionCount;
	bool m_Active;

	CImpl() : m_pState(0), m_CommandCount(0), m_MemoryUsed(0), m_InstructionCount(0), m_Active(false)
	{
		mem_zero(&m_State, sizeof(m_State));
		m_State.m_RandomState = 1;
	}

	static void *Allocator(void *pUser, void *pPointer, size_t OldSize, size_t NewSize)
	{
		CImpl *pSelf = static_cast<CImpl *>(pUser);
		if(!pPointer)
			OldSize = 0;
		if(NewSize == 0)
		{
			free(pPointer);
			pSelf->m_MemoryUsed -= (int)OldSize;
			if(pSelf->m_MemoryUsed < 0)
				pSelf->m_MemoryUsed = 0;
			return 0;
		}
		const long long Total = (long long)pSelf->m_MemoryUsed - (long long)OldSize + (long long)NewSize;
		if(Total > SCRIPT_MEMORY_LIMIT)
			return 0;
		void *pResult = realloc(pPointer, NewSize);
		if(pResult)
			pSelf->m_MemoryUsed = (int)Total;
		return pResult;
	}

	static void InstructionHook(lua_State *pState, lua_Debug *)
	{
		void *pUser = 0;
		lua_getallocf(pState, &pUser);
		CImpl *pSelf = static_cast<CImpl *>(pUser);
		pSelf->m_InstructionCount += SCRIPT_HOOK_GRANULARITY;
		if(pSelf->m_InstructionCount > SCRIPT_INSTRUCTION_LIMIT)
			luaL_error(pState, "challenge script instruction budget exceeded");
	}

	static CImpl *Self(lua_State *pState)
	{
		void *pUser = 0;
		lua_getallocf(pState, &pUser);
		return static_cast<CImpl *>(pUser);
	}

	static int StateGet(lua_State *pState)
	{
		CImpl *pSelf = Self(pState);
		const int Index = (int)luaL_checkinteger(pState, 1);
		if(Index < 0 || Index >= CChallengeScriptState::GLOBAL_STATE_COUNT)
			return luaL_error(pState, "challenge global state index must be 0..7");
		lua_pushinteger(pState, pSelf->m_State.m_aGlobal[Index]);
		return 1;
	}

	static int StateSet(lua_State *pState)
	{
		CImpl *pSelf = Self(pState);
		const int Index = (int)luaL_checkinteger(pState, 1);
		const lua_Integer Value = luaL_checkinteger(pState, 2);
		if(Index < 0 || Index >= CChallengeScriptState::GLOBAL_STATE_COUNT)
			return luaL_error(pState, "challenge global state index must be 0..7");
		if(Value < -1000000 || Value > 1000000)
			return luaL_error(pState, "challenge state value is out of range");
		pSelf->m_State.m_aGlobal[Index] = (int)Value;
		return 0;
	}

	static int PlayerStateGet(lua_State *pState)
	{
		CImpl *pSelf = Self(pState);
		const int ClientID = (int)luaL_checkinteger(pState, 1);
		const int Index = (int)luaL_checkinteger(pState, 2);
		if(ClientID < 0 || ClientID >= MAX_CLIENTS || Index < 0 ||
		   Index >= CChallengeScriptState::PLAYER_STATE_COUNT)
			return luaL_error(pState, "challenge player state index is invalid");
		lua_pushinteger(pState, pSelf->m_State.m_aPlayer[ClientID][Index]);
		return 1;
	}

	static int PlayerStateSet(lua_State *pState)
	{
		CImpl *pSelf = Self(pState);
		const int ClientID = (int)luaL_checkinteger(pState, 1);
		const int Index = (int)luaL_checkinteger(pState, 2);
		const lua_Integer Value = luaL_checkinteger(pState, 3);
		if(ClientID < 0 || ClientID >= MAX_CLIENTS || Index < 0 ||
		   Index >= CChallengeScriptState::PLAYER_STATE_COUNT)
			return luaL_error(pState, "challenge player state index is invalid");
		if(Value < -1000000 || Value > 1000000)
			return luaL_error(pState, "challenge player state value is out of range");
		pSelf->m_State.m_aPlayer[ClientID][Index] = (int)Value;
		return 0;
	}

	static int Random(lua_State *pState)
	{
		CImpl *pSelf = Self(pState);
		uint32_t X = pSelf->m_State.m_RandomState ? pSelf->m_State.m_RandomState : 0x6d2b79f5u;
		X ^= X << 13;
		X ^= X >> 17;
		X ^= X << 5;
		pSelf->m_State.m_RandomState = X;
		const int Count = lua_gettop(pState);
		if(Count == 0)
		{
			lua_pushinteger(pState, (lua_Integer)X);
			return 1;
		}
		if(Count > 2)
			return luaL_error(pState, "challenge random expects zero, one, or two bounds");
		const lua_Integer LowValue = Count > 1 ? luaL_checkinteger(pState, 1) : 1;
		const lua_Integer HighValue = luaL_checkinteger(pState, Count > 1 ? 2 : 1);
		if(LowValue < INT_MIN || LowValue > INT_MAX || HighValue < INT_MIN || HighValue > INT_MAX)
			return luaL_error(pState, "challenge random bound is out of range");
		const int Low = (int)LowValue;
		const int High = (int)HighValue;
		if(High < Low)
			return luaL_error(pState, "challenge random interval is empty");
		const uint64_t Span = (uint64_t)((int64_t)High - (int64_t)Low) + 1;
		lua_pushinteger(pState, (lua_Integer)((int64_t)Low + (int64_t)((uint64_t)X % Span)));
		return 1;
	}

	static int Command(lua_State *pState)
	{
		CImpl *pSelf = Self(pState);
		if(pSelf->m_CommandCount >= SCRIPT_COMMAND_LIMIT)
		{
			lua_pushboolean(pState, 0);
			return 1;
		}
		CChallengeScriptCommand &Command = pSelf->m_aCommands[pSelf->m_CommandCount];
		Command.m_Kind = (int)luaL_checkinteger(pState, 1);
		Command.m_ClientID = (int)luaL_optinteger(pState, 2, -1);
		Command.m_Arg0 = (int)luaL_optinteger(pState, 3, 0);
		Command.m_Arg1 = (int)luaL_optinteger(pState, 4, 0);
		if(Command.m_Kind <= CHALLENGE_COMMAND_NONE || Command.m_Kind > CHALLENGE_COMMAND_SPAWN_EVENT ||
		   Command.m_ClientID < -1 || Command.m_ClientID >= MAX_CLIENTS)
		{
			lua_pushboolean(pState, 0);
			return 1;
		}
		pSelf->m_CommandCount++;
		lua_pushboolean(pState, 1);
		return 1;
	}

	void RegisterSandbox()
	{
		const struct
		{
			const char *m_pName;
			lua_CFunction m_Open;
		} aLibraries[] = {{LUA_GNAME, luaopen_base},
			{LUA_TABLIBNAME, luaopen_table},
			{LUA_STRLIBNAME, luaopen_string},
			{LUA_MATHLIBNAME, luaopen_math},
			{LUA_UTF8LIBNAME, luaopen_utf8}};
		for(const auto &Library : aLibraries)
		{
			luaL_requiref(m_pState, Library.m_pName, Library.m_Open, 1);
			lua_pop(m_pState, 1);
		}
		const char *apForbidden[] = {"io", "os", "debug", "package", "require", "load", "loadfile", "dofile",
			"collectgarbage", "coroutine", "pairs", "next"};
		for(const char *pName : apForbidden)
		{
			lua_pushnil(m_pState);
			lua_setglobal(m_pState, pName);
		}
		lua_getglobal(m_pState, "math");
		if(lua_istable(m_pState, -1))
		{
			lua_pushlightuserdata(m_pState, this);
			lua_pushcclosure(m_pState, Random, 1);
			lua_setfield(m_pState, -2, "random");
			lua_pushnil(m_pState);
			lua_setfield(m_pState, -2, "randomseed");
		}
		lua_pop(m_pState, 1);

		lua_newtable(m_pState);
		lua_pushcfunction(m_pState, StateGet);
		lua_setfield(m_pState, -2, "state_get");
		lua_pushcfunction(m_pState, StateSet);
		lua_setfield(m_pState, -2, "state_set");
		lua_pushcfunction(m_pState, PlayerStateGet);
		lua_setfield(m_pState, -2, "player_state_get");
		lua_pushcfunction(m_pState, PlayerStateSet);
		lua_setfield(m_pState, -2, "player_state_set");
		lua_pushlightuserdata(m_pState, this);
		lua_pushcclosure(m_pState, Random, 1);
		lua_setfield(m_pState, -2, "random");
		lua_pushcfunction(m_pState, Command);
		lua_setfield(m_pState, -2, "command");
		lua_pushinteger(m_pState, ModApiCurrentVersion());
		lua_setfield(m_pState, -2, "api_version");
		lua_setglobal(m_pState, "challenge");
	}

	uint32_t ComputeChecksum() const
	{
		uint32_t Hash = 2166136261u;
		auto Add = [&Hash](uint32_t Value) {
			for(int i = 0; i < 4; ++i)
			{
				Hash ^= (Value >> (i * 8)) & 0xffu;
				Hash *= 16777619u;
			}
		};
		Add((uint32_t)m_State.m_Tick);
		Add(m_State.m_RandomState);
		for(int i = 0; i < CChallengeScriptState::GLOBAL_STATE_COUNT; ++i)
			Add((uint32_t)m_State.m_aGlobal[i]);
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ++ClientID)
			for(int i = 0; i < CChallengeScriptState::PLAYER_STATE_COUNT; ++i)
				Add((uint32_t)m_State.m_aPlayer[ClientID][i]);
		return Hash;
	}
};

CChallengeScriptRuntime::CChallengeScriptRuntime() : m_pImpl(new CImpl) {}

CChallengeScriptRuntime::~CChallengeScriptRuntime()
{
	Deactivate();
	delete m_pImpl;
}

bool CChallengeScriptRuntime::Activate(const CModApiDescriptor &Descriptor, uint32_t Seed, char *pError, int ErrorSize)
{
	Deactivate();
	mem_zero(&m_pImpl->m_State, sizeof(m_pImpl->m_State));
	m_pImpl->m_State.m_RandomState = Seed ? Seed : 1;
	if(Descriptor.m_ApiVersion != ModApiCurrentVersion() ||
	   !(Descriptor.m_Capabilities & MOD_CAPABILITY_GAMEPLAY_RULES) ||
	   (Descriptor.m_Capabilities & ~MOD_CAPABILITY_GAMEPLAY_RULES))
	{
		SetError(pError, ErrorSize, "challenge script requires gameplay_rules API only");
		return false;
	}
	m_pImpl->m_pState = lua_newstate(CImpl::Allocator, m_pImpl);
	if(!m_pImpl->m_pState)
	{
		SetError(pError, ErrorSize, "unable to create challenge Lua runtime");
		return false;
	}
	m_pImpl->RegisterSandbox();
	m_pImpl->m_Active = true;
	return true;
}

void CChallengeScriptRuntime::Deactivate()
{
	if(!m_pImpl)
		return;
	m_pImpl->m_Active = false;
	if(m_pImpl->m_pState)
	{
		lua_close(m_pImpl->m_pState);
		m_pImpl->m_pState = 0;
	}
	m_pImpl->m_MemoryUsed = 0;
	m_pImpl->m_CommandCount = 0;
}

bool CChallengeScriptRuntime::Active() const
{
	return m_pImpl && m_pImpl->m_Active && m_pImpl->m_pState;
}

bool CChallengeScriptRuntime::LoadScript(const char *pName,
	const char *pSource,
	int SourceSize,
	char *pError,
	int ErrorSize)
{
	if(!Active() || !pSource || SourceSize <= 0 || SourceSize > 1024 * 1024)
	{
		SetError(pError, ErrorSize, "invalid challenge script size");
		return false;
	}
	m_pImpl->m_InstructionCount = 0;
	lua_sethook(m_pImpl->m_pState, CImpl::InstructionHook, LUA_MASKCOUNT, SCRIPT_HOOK_GRANULARITY);
	const int Result = luaL_loadbufferx(m_pImpl->m_pState, pSource, SourceSize, pName ? pName : "challenge", "t") ==
								LUA_OK
								? lua_pcall(m_pImpl->m_pState, 0, 0, 0)
								: LUA_ERRSYNTAX;
	 lua_sethook(m_pImpl->m_pState, 0, 0, 0);
	if(Result != LUA_OK)
	{
		SetError(pError, ErrorSize, lua_tostring(m_pImpl->m_pState, -1));
		lua_pop(m_pImpl->m_pState, 1);
		m_pImpl->m_Active = false;
		return false;
	}
	return true;
}

bool CChallengeScriptRuntime::Dispatch(EChallengeScriptEvent Event,
	int ClientID,
	int Value,
	char *pError,
	int ErrorSize)
{
	if(!Active())
		return false;
	if(static_cast<int>(Event) >= static_cast<int>(EChallengeScriptEvent::Count))
	{
		SetError(pError, ErrorSize, "invalid challenge script event");
		return false;
	}
	if(Event == EChallengeScriptEvent::Tick)
		m_pImpl->m_State.m_Tick++;
	m_pImpl->m_CommandCount = 0;
	lua_getglobal(m_pImpl->m_pState, EventName(Event));
	if(!lua_isfunction(m_pImpl->m_pState, -1))
	{
		lua_pop(m_pImpl->m_pState, 1);
		m_pImpl->m_State.m_Checksum = m_pImpl->ComputeChecksum();
		return true;
	}
	m_pImpl->m_InstructionCount = 0;
	lua_pushinteger(m_pImpl->m_pState, ClientID);
	lua_pushinteger(m_pImpl->m_pState, Value);
	lua_pushinteger(m_pImpl->m_pState, m_pImpl->m_State.m_Tick);
	lua_sethook(m_pImpl->m_pState, CImpl::InstructionHook, LUA_MASKCOUNT, SCRIPT_HOOK_GRANULARITY);
	const int Result = lua_pcall(m_pImpl->m_pState, 3, 0, 0);
	lua_sethook(m_pImpl->m_pState, 0, 0, 0);
	if(Result != LUA_OK)
	{
		SetError(pError, ErrorSize, lua_tostring(m_pImpl->m_pState, -1));
		lua_pop(m_pImpl->m_pState, 1);
		m_pImpl->m_Active = false;
		return false;
	}
	m_pImpl->m_State.m_Checksum = m_pImpl->ComputeChecksum();
	return true;
}

void CChallengeScriptRuntime::SetTick(int Tick)
{
	if(m_pImpl)
		m_pImpl->m_State.m_Tick = Tick;
}

const CChallengeScriptState &CChallengeScriptRuntime::State() const
{
	return m_pImpl->m_State;
}

void CChallengeScriptRuntime::ApplyAuthoritativeState(const CChallengeScriptState &State)
{
	if(!m_pImpl)
		return;
	m_pImpl->m_State = State;
}

uint32_t CChallengeScriptRuntime::Checksum() const
{
	return m_pImpl ? m_pImpl->ComputeChecksum() : 0;
}

int CChallengeScriptRuntime::CommandCount() const
{
	return m_pImpl ? m_pImpl->m_CommandCount : 0;
}

const CChallengeScriptCommand *CChallengeScriptRuntime::CommandAt(int Index) const
{
	return m_pImpl && Index >= 0 && Index < m_pImpl->m_CommandCount ? &m_pImpl->m_aCommands[Index] : 0;
}
