#ifndef GAME_SERVER_ENTITIES_BUILDING_H
#define GAME_SERVER_ENTITIES_BUILDING_H

#include <game/server/entity.h>
#include <game/nodes.h>
#include <game/weapon_catalog.h>

const int BuildingPhysSize = 32;
const int TurretPhysSize = 32;
const int TeslacoilPhysSize = 36;
const int SawbladePhysSize = 32;
const int MinePhysSize = 6;
const int BarrelPhysSize = 28;
const int LazerPhysSize = 10;
const int PowerupperPhysSize = 10;
const int BasePhysSize = 10;
const int StandPhysSize = 20;
const int LightningWallPhysSize = 20;
const int FlametrapPhysSize = 20;
const int SwitchPhysSize = 10;
const int DoorPhysSize = 40;
const int ReactorPhysSize = 50;
const int JumppadPhysSize = 60;
const int GeneratorPhysSize = 50;

class CBuilding : public CEntity
{
  public:
	CBuilding(CGameWorld *pGameWorld, vec2 Pos, int Type, int Team);
	CBuilding(CGameWorld *pGameWorld, vec2 Pos, int NodesType, int Team, int Owner, int Health, bool Alive, bool Free);

	virtual void Reset();
	virtual void Tick();
	virtual void SurvivalReset();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	virtual CWeaponSpec GetItem(int Slot) { return {}; }

	virtual void ClearItem(int Slot) {}

	int m_Type;
	int m_Team;
	int m_Life;
	int m_MaxLife;

	bool Repair(int Amount = 10);

	int m_aStatus[NUM_BSTATUS];
	int m_Status;

	bool m_Collision;
	// Runtime-generated Roam traps hurt players but must never become a solid
	// plug in a narrow race corridor. Persisted through block unload/reload.
	bool m_NonBlockingHazard;

	vec2 m_Center;

	bool m_Mirror;

	bool m_CanMove;
	bool m_Moving;
	vec2 m_Vel;

	void Move();
	void DoFallCheck();

	int m_DamageOwner;
	int m_DeathTimer;
	int m_PveBuilder;
	int m_PveKitCost;
	bool m_PveRefunded;
	bool m_PveSwitchActive;
	bool m_PveReactorObjective;
	bool m_PveDestroyObjective;
	int m_SwitchHoldTicks;

	bool m_DestructionTriggered;

	bool Jumppad();
	void Trigger();
	void SetPveSwitchActive(bool Active);
	void SetPveReactorObjective(bool Active, int MaxLife = 0);
	virtual void TakeDamage(int Damage, const CAttackSource &Source, vec2 Force = vec2(0, 0));
	virtual void Destroy();

	vec2 m_DamagePos;

	bool m_NodesMode;
	int m_NodesType;
	int m_NodesOwner;
	int m_NodesHealth;
	int m_NodesMaxHealth;
	bool m_NodesAlive;
	bool m_NodesPower;
	bool m_NodesFree;
	bool m_NodesDeconstruction;
	bool m_NodesDestroyed;
	int m_NodesAttackTick;
	int m_NodesAnimationFrame;

	bool IsNodesBuilding() const { return m_NodesMode; }
	int NodesType() const { return m_NodesType; }
	int NodesOwner() const { return m_NodesOwner; }
	int NodesHealth() const { return m_NodesHealth; }
	int NodesMaxHealth() const { return m_NodesMaxHealth; }
	bool NodesAlive() const { return m_NodesAlive; }
	bool NodesPower() const { return m_NodesPower; }
	bool NodesFree() const { return m_NodesFree; }
	bool NodesDeconstruction() const { return m_NodesDeconstruction; }
	void SetNodesDeconstruction(bool Value) { m_NodesDeconstruction = Value; }
	void SetNodesPower(bool Value) { m_NodesPower = Value; }
	void SetNodesAlive(bool Value) { m_NodesAlive = Value; }
	void SetNodesHealth(int Value) { m_NodesHealth = clamp(Value, 0, m_NodesMaxHealth); }
	void SetNodesOwner(int Owner) { m_NodesOwner = Owner; }
	void SetNodesAnimationFrame(int Frame) { m_NodesAnimationFrame = clamp(Frame, 0, 15); }

	void TickNodes();

  protected:
	void UpdateStatus();
	float m_Bounciness;
	bool m_AttachOnFall;
	bool m_DestroyOnFall;

	int m_TriggerTimer;
	vec2 m_BoxSize;

  private:
	int m_SetTimer;

	// lightning wall
	void CreateLightningWallTop();
	int m_Height;
	int m_LightningBlockCheckTick;
};

#endif
