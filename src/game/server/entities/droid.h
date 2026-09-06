#ifndef GAME_SERVER_ENTITIES_DROID_H
#define GAME_SERVER_ENTITIES_DROID_H

#include <game/server/entity.h>
#include <game/weapons/weapon_catalog.h>

const int DroidPhysSize = 60;

class CDroid : public CEntity
{
  public:
	CDroid(CGameWorld *pGameWorld, vec2 Pos, int Type);
	virtual ~CDroid();

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	int Controller() const { return m_Controller; }
	bool IsPlayerAvatar() const { return m_Controller >= 0; }
	void SetController(int ClientID) { m_Controller = ClientID; }
	void DropController();
	vec2 GetVel() const { return m_Vel; }
	CAttackSource ShotSource() const;

	virtual void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos);
	int m_Health;
	int m_MaxHealth;

	vec2 m_Center;
	int m_Type;

	enum State
	{
		IDLE,
		MOVE,
		TURN,
		TAKEOFF,
		FLY,
	};

  protected:
	int m_State;
	int m_NextState;
	int m_StateChangeTick;

	int m_Anim;
	int m_Mode;

	int m_AttackTimer;

	vec2 m_Vel;

	int m_FlyTargetTick;

	vec2 m_Target;
	vec2 m_NewTarget;

	int m_Status;
	int m_Dir;

	vec2 m_StartPos;

	int m_DamageTakenTick;
	int m_DeathTick;

	int m_FireDelay;
	int m_FireCount;

	void SetState(int State);

	virtual bool FindTarget();
	virtual bool Target();
	virtual void Fire();

	bool TakeControl();
	virtual bool TickControlled();
	bool TickWalkerControl(int CoreRad);
	bool TickCrawlerControl(const struct CDroidCrawlerControl &Control,
							int *pMove,
							int *pJumpTick,
							float *pJumpForce,
							int *pAttackCount);
	bool TickFlyerControl(vec2 Box, int CoreRad);

	int m_Controller;
	int m_TargetIndex;
	int m_AttackTick;
	int m_TargetTimer;

	int m_ReloadTimer;
};

#endif
