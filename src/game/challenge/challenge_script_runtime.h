#ifndef GAME_CHALLENGE_SCRIPT_RUNTIME_H
#define GAME_CHALLENGE_SCRIPT_RUNTIME_H

#include <cstdint>

#include <engine/shared/mod_api.h>
#include <engine/shared/protocol.h>

// Challenge scripts deliberately receive a small, fixed event vocabulary. No
// engine object or pointer crosses this boundary, which makes the same script
// safe to run for client prediction and for the authoritative server.
enum class EChallengeScriptEvent : uint8_t
{
	RoundStart,
	RoundEnd,
	Tick,
	PlayerSpawn,
	PlayerDeath,
	Damage,
	Pickup,
	Build,
	Forge,
	FloorComplete,
	Count,
};

enum EChallengeScriptCommandKind
{
	CHALLENGE_COMMAND_NONE,
	CHALLENGE_COMMAND_SET_VARIANT,
	CHALLENGE_COMMAND_ADD_SCORE,
	CHALLENGE_COMMAND_SPAWN_EVENT,
};

struct CChallengeScriptCommand
{
	int32_t m_Kind;
	int32_t m_ClientID;
	int32_t m_Arg0;
	int32_t m_Arg1;
};

struct CChallengeScriptState
{
	enum
	{
		GLOBAL_STATE_COUNT = 8,
		PLAYER_STATE_COUNT = 4,
	};

	int m_Tick;
	uint32_t m_RandomState;
	int m_aGlobal[GLOBAL_STATE_COUNT];
	int m_aPlayer[MAX_CLIENTS][PLAYER_STATE_COUNT];
	uint32_t m_Checksum;
};

class CChallengeScriptRuntime
{
	struct CImpl;
	CImpl *m_pImpl;

  public:
	CChallengeScriptRuntime();
	~CChallengeScriptRuntime();

	CChallengeScriptRuntime(const CChallengeScriptRuntime &) = delete;
	CChallengeScriptRuntime &operator=(const CChallengeScriptRuntime &) = delete;

	// Challenge packages may request gameplay_rules only. A different API
	// capability (weapons, IO, etc.) is rejected before Lua is created.
	bool Activate(const CModApiDescriptor &Descriptor, uint32_t Seed, char *pError = 0, int ErrorSize = 0);
	void Deactivate();
	bool Active() const;

	bool LoadScript(const char *pName, const char *pSource, int SourceSize, char *pError, int ErrorSize);
	bool Dispatch(EChallengeScriptEvent Event, int ClientID = -1, int Value = 0, char *pError = 0, int ErrorSize = 0);

	void SetTick(int Tick);
	const CChallengeScriptState &State() const;
	void ApplyAuthoritativeState(const CChallengeScriptState &State);
	uint32_t Checksum() const;

	int CommandCount() const;
	const CChallengeScriptCommand *CommandAt(int Index) const;
};

#endif
