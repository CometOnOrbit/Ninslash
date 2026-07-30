#include <game/weapon_script_runtime.h>

#include <assert.h>
#include <string.h>

class CTestHost final : public IWeaponScriptHost
{
  public:
	int m_aState[8]{};
	uint32_t m_Random = 1;
	CWeaponScriptSpawn m_aSpawns[16]{};
	int m_SpawnCount = 0;
	int m_VisualKind = -1;
	int m_LastCommand = -1;

	int ScriptStateGet(int Index) const override { return m_aState[Index]; }
	void ScriptStateSet(int Index, int Value) override { m_aState[Index] = Value; }
	uint32_t ScriptRandom() override
	{
		m_Random ^= m_Random << 13;
		m_Random ^= m_Random >> 17;
		m_Random ^= m_Random << 5;
		return m_Random;
	}
	bool ScriptCommand(const CWeaponScriptCommand &Command) override
	{
		if(Command.m_Kind == WEAPON_SCRIPT_COMMAND_VISUAL)
		{
			m_VisualKind = Command.m_aArgs[0];
			return true;
		}
		if(Command.m_Kind < WEAPON_SCRIPT_COMMAND_SPAWN_PROJECTILE ||
		   Command.m_Kind > WEAPON_SCRIPT_COMMAND_SPAWN_SUMMON)
		{
			m_LastCommand = Command.m_Kind;
			return true;
		}
		if(m_SpawnCount >= 16)
			return false;
		CWeaponScriptSpawn Spawn{};
		Spawn.m_Kind = Command.m_Kind - WEAPON_SCRIPT_COMMAND_SPAWN_PROJECTILE;
		Spawn.m_Speed = Command.m_aArgs[0];
		Spawn.m_LifeTicks = Command.m_aArgs[1];
		Spawn.m_Damage = Command.m_aArgs[2];
		Spawn.m_Radius = Command.m_aArgs[3];
		Spawn.m_Bounces = Command.m_aArgs[4];
		Spawn.m_Gravity = Command.m_aArgs[5];
		Spawn.m_Count = Command.m_aArgs[6];
		m_aSpawns[m_SpawnCount++] = Spawn;
		return true;
	}
};

int main()
{
	char aError[256];
	CWeaponScriptRuntime::Reset();
	const char *pSource = "weapon.on_fire('workshop:42:arc', function(ctx) "
						  "ctx:state_set(0, ctx:state_get(0) + 1); "
						  "ctx:spawn_projectile{speed=1200, life=90, damage=9, radius=7, bounces=2, count=2}; "
						  "ctx:spawn_ray{range=800, damage=4}; ctx:visual(1) end) "
						  "weapon.on_tick('workshop:42:arc', function(ctx) ctx:state_set(1, ctx:state_get(1) + 1) end)";
	assert(CWeaponScriptRuntime::LoadPackageScript(
		"42", "arc.weapon_runtime.lua", pSource, (int)strlen(pSource), aError, sizeof(aError)));
	assert(CWeaponScriptRuntime::RegisteredWeaponCount() == 1);
	assert(CWeaponScriptRuntime::HasHandler("workshop:42:arc", EWeaponScriptEvent::Fire));
	assert(CWeaponScriptRuntime::HasHandler("workshop:42:arc", EWeaponScriptEvent::Tick));
	CTestHost Host;
	assert(CWeaponScriptRuntime::Dispatch("workshop:42:arc", EWeaponScriptEvent::Fire, &Host, aError, sizeof(aError)));
	assert(Host.m_aState[0] == 1);
	assert(Host.m_SpawnCount == 2);
	assert(Host.m_aSpawns[0].m_Kind == WEAPON_SCRIPT_SPAWN_PROJECTILE && Host.m_aSpawns[0].m_Count == 2 &&
		   Host.m_aSpawns[0].m_Bounces == 2);
	assert(Host.m_aSpawns[1].m_Kind == WEAPON_SCRIPT_SPAWN_RAY && Host.m_aSpawns[1].m_Speed == 800);
	assert(Host.m_VisualKind == 1);
	assert(CWeaponScriptRuntime::Dispatch("workshop:42:arc", EWeaponScriptEvent::Fire, &Host, aError, sizeof(aError)) &&
		   Host.m_aState[0] == 2);
	assert(CWeaponScriptRuntime::Dispatch("workshop:42:arc", EWeaponScriptEvent::Tick, &Host, aError, sizeof(aError)) &&
		   Host.m_aState[1] == 1);

	const char *pLifecycle = "weapon.on_fire('official:static:gun1', function(ctx) ctx:visual(0) end) "
							 "weapon.on_tick('official:static:gun1', function(ctx) ctx:state_set(4, 1) end) "
							 "weapon.on_charge('official:static:gun1', function(ctx) ctx:state_set(2, 1) end) "
							 "weapon.on_release('official:static:gun1', function(ctx) ctx:ammo_add(1) end) "
							 "weapon.on_trigger('official:static:gun1', function(ctx) ctx:charge_set(1) end) "
							 "weapon.on_throw('official:static:gun1', function(ctx) ctx:release_weapon() end) "
							 "weapon.on_destroy('official:static:gun1', function(ctx) ctx:state_set(3, 1) end) "
							 "weapon.on_activate('official:static:gun1', function(ctx) ctx:timer_set(12) end) "
							 "weapon.on_collision('official:static:gun1', function(ctx) ctx:sound(1) end)";
	assert(CWeaponScriptRuntime::LoadPackageScript(
		"official", "official_runtime.lua", pLifecycle, (int)strlen(pLifecycle), aError, sizeof(aError)));
	assert(CWeaponScriptRuntime::HasHandler("official:static:gun1", EWeaponScriptEvent::Charge));
	for(int Event = 0; Event < static_cast<int>(EWeaponScriptEvent::Count); ++Event)
		assert(CWeaponScriptRuntime::HasHandler("official:static:gun1", static_cast<EWeaponScriptEvent>(Event)));
	assert(CWeaponScriptRuntime::Dispatch(
			   "official:static:gun1", EWeaponScriptEvent::Charge, &Host, aError, sizeof(aError)) &&
		   Host.m_aState[2] == 1);
	assert(CWeaponScriptRuntime::Dispatch(
			   "official:static:gun1", EWeaponScriptEvent::Activate, &Host, aError, sizeof(aError)) &&
		   Host.m_LastCommand == WEAPON_SCRIPT_COMMAND_TIMER_SET);
	const char *pBudget = "weapon.on_fire('official:static:gun2', function(ctx) for i=1,65 do ctx:visual(0) end end)";
	assert(CWeaponScriptRuntime::LoadPackageScript(
		"official", "budget.lua", pBudget, (int)strlen(pBudget), aError, sizeof(aError)));
	assert(!CWeaponScriptRuntime::Dispatch(
		"official:static:gun2", EWeaponScriptEvent::Fire, &Host, aError, sizeof(aError)));

	CWeaponScriptRuntime::Reset();
	const char *pUnsafe = "os.execute('not allowed')";
	assert(!CWeaponScriptRuntime::LoadPackageScript(
		"42", "bad.weapon_runtime.lua", pUnsafe, (int)strlen(pUnsafe), aError, sizeof(aError)));
	CWeaponScriptRuntime::Reset();
	return 0;
}
