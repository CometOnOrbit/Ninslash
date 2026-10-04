#ifndef GAME_SERVER_ENTITIES_DROID_FOUNDRYWARDEN_H
#define GAME_SERVER_ENTITIES_DROID_FOUNDRYWARDEN_H

#include <game/foundry_warden.h>
#include "droid.h"

const int FoundryWardenPhysSize = 120;

class CFoundryWarden : public CDroid
{
  public:
	CFoundryWarden(CGameWorld *pGameWorld, vec2 Pos);
	virtual ~CFoundryWarden();

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;
	bool HitSegment(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pAt) override;

  private:
	enum
	{
		ARM_IN = 0,
		ARM_EXTEND,
		ARM_PULL,
		ARM_RETRACT,
	};

	void StartAct(int Act);
	void SetLocomotion(int Act);
	void Think();
	void TickAct();
	void TickGrab();
	void TickCharge(int Elapsed);
	void EndCharge(float Radius, int Dmg, float Knock);
	bool Thrusting() const { return m_Act == WARDEN_ACT_CHARGE && (m_Events & 1) && !(m_Events & 2); }
	void FinishAct();
	void TickDeath();
	void MoveBody();
	bool GroundAt(float x);
	bool AcquireTarget(bool NeedSight);
	bool PartAlive(int Part) const { return Part == WARDEN_PART_CORE || m_aPartHealth[Part] > 0; }
	void BreakPart(int Part);
	void ReleaseGrab();
	void HurtPlayers(vec2 Pos, float Radius, int Dmg, float Knock, bool Front, float Slow = 0.0f);
	void FireRocket(float Side);
	void TickRockets();
	void ExplodeRocket(int i, bool Shot);
	void ClearRockets();

	int m_Act;
	int m_ActTick;
	int m_Events;
	int m_Phase;
	int m_Stagger;
	int m_ActsDone;
	int m_GrabCooldown;
	int m_RocketCooldown;
	int m_ChargeCooldown;
	int m_aPartHealth[NUM_WARDEN_PARTS];
	int m_aPartMax[NUM_WARDEN_PARTS];

	int m_ArmState;
	vec2 m_ArmTip;
	vec2 m_ArmVel;
	int m_GrabCID;
	int m_GrabBreak;

	int m_aRocketID[WARDEN_MAX_ROCKETS];
	vec2 m_aRocketPos[WARDEN_MAX_ROCKETS];
	vec2 m_aRocketVel[WARDEN_MAX_ROCKETS];
	int m_aRocketLife[WARDEN_MAX_ROCKETS];
	int m_aRocketTarget[WARDEN_MAX_ROCKETS];
};

#endif
