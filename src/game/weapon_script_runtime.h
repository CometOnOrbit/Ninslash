#ifndef GAME_WEAPON_SCRIPT_RUNTIME_H
#define GAME_WEAPON_SCRIPT_RUNTIME_H

#include <cstdint>

// Stable, fixed-width data exchanged between the Lua sandbox and the game.
// The host translates these requests into real server entities or predicted
// client entities; scripts never receive engine pointers.
enum EWeaponScriptSpawnKind
{
	WEAPON_SCRIPT_SPAWN_PROJECTILE,
	WEAPON_SCRIPT_SPAWN_RAY,
	WEAPON_SCRIPT_SPAWN_AREA,
	WEAPON_SCRIPT_SPAWN_SUMMON,
};

struct CWeaponScriptSpawn
{
	int m_Kind;
	int m_Speed;
	int m_LifeTicks;
	int m_Damage;
	int m_Radius;
	int m_Bounces;
	int m_Gravity;
	int m_Count;
};

enum EWeaponScriptCommandKind
{
	WEAPON_SCRIPT_COMMAND_SPAWN_PROJECTILE,
	WEAPON_SCRIPT_COMMAND_SPAWN_RAY,
	WEAPON_SCRIPT_COMMAND_SPAWN_AREA,
	WEAPON_SCRIPT_COMMAND_SPAWN_SUMMON,
	WEAPON_SCRIPT_COMMAND_VISUAL,
	WEAPON_SCRIPT_COMMAND_TIMER_SET,
	WEAPON_SCRIPT_COMMAND_AMMO_ADD,
	WEAPON_SCRIPT_COMMAND_CHARGE_SET,
	WEAPON_SCRIPT_COMMAND_EXPLOSION,
	WEAPON_SCRIPT_COMMAND_RELEASE,
	WEAPON_SCRIPT_COMMAND_CONTROLLER_TRIGGER,
	WEAPON_SCRIPT_COMMAND_DROP_PICKUP,
	WEAPON_SCRIPT_COMMAND_ELECTROWALL,
	WEAPON_SCRIPT_COMMAND_SOUND,
	WEAPON_SCRIPT_COMMAND_BOMB_TRIGGER,
};

struct CWeaponScriptCommand
{
	int32_t m_Kind;
	int32_t m_aArgs[8];
};

enum class EWeaponScriptEvent : uint8_t
{
	Fire,
	Tick,
	Charge,
	Release,
	Trigger,
	Throw,
	Activate,
	Collision,
	Destroy,
	Count,
};

class IWeaponScriptHost
{
  public:
	virtual ~IWeaponScriptHost() {}
	virtual int ScriptStateGet(int Index) const = 0;
	virtual void ScriptStateSet(int Index, int Value) = 0;
	virtual uint32_t ScriptRandom() = 0;
	virtual bool ScriptCommand(const CWeaponScriptCommand &Command) = 0;
};

// A persistent sandbox containing only event registrations. The callbacks use
// integer/fixed-point parameters and an explicit host, so their observable
// simulation state can be snapshotted and replayed on another machine.
class CWeaponScriptRuntime
{
  public:
	static void Reset();
	static void BeginReload();
	static void CommitReload();
	static void RollbackReload();
	static bool LoadPackageScript(
		const char *pPackageId, const char *pName, const char *pSource, int SourceSize, char *pError, int ErrorSize);
	static bool HasHandler(const char *pStableId, EWeaponScriptEvent Event);
	static bool Dispatch(
		const char *pStableId, EWeaponScriptEvent Event, IWeaponScriptHost *pHost, char *pError = 0, int ErrorSize = 0);
	static int RegisteredWeaponCount();
};

#endif
