#ifndef GAME_SERVER_ENTITIES_DROID_SKITTERMATRIARCH_H
#define GAME_SERVER_ENTITIES_DROID_SKITTERMATRIARCH_H

#include <game/skitter_matriarch.h>
#include "boss_body.h"
#include "droid.h"

// Hovering six-legged spider tank. The body rides a ray spring over the floor like a
// crawler, so slopes and steps never snag it; legs are purely visual (client IK).
class CSkitterMatriarch : public CDroid
{
  public:
	CSkitterMatriarch(CGameWorld *pGameWorld, vec2 Pos);
	virtual ~CSkitterMatriarch();

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;
	bool HitSegment(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pAt) override;

	// Debug / test hooks.
	void DebugAct(int Act);
	void DebugState(char *pBuffer, int Size);

  private:
	struct CShot
	{
		int m_ID;
		int m_Kind;
		vec2 m_Pos;
		vec2 m_Vel;
		int m_Life;
	};

	void StartAct(int Act);
	void SetLocomotion(int Act);
	void Think();
	void TickAct();
	void TickPounce(int Elapsed);
	void LaunchPounce(bool Counter);
	void LandPounce();
	void FinishAct();
	void TickDeath();
	void MoveBody();
	void SyncGround();
	bool AcquireTarget(bool NeedSight);
	bool PartAlive(int Part) const { return Part == MATRIARCH_PART_CORE || m_aPartHealth[Part] > 0; }
	void BreakPart(int Part);
	int HurtPlayers(vec2 Pos, float Radius, int Dmg, float Knock, float Lift = 0.25f);
	int HurtSegment(vec2 From, vec2 To, float Radius, int Dmg, float Knock);
	bool SacExposed();
	vec2 Mouth() const;
	vec2 SacPos() const;
	vec2 LocalToWorld(float x, float y) const { return m_Pos + vec2(x * m_Dir, y); }
	int AliveBrood();

	int AddShot(int Kind, vec2 Pos, vec2 Vel);
	void RemoveShot(int i);
	void ClearShots();
	void TickShots();
	void FireGlob(int Shot);

	int m_Act;
	int m_ActTick;
	int m_Events;
	int m_Phase;
	int m_Stagger;
	int m_ActsDone;
	int m_PounceCooldown;
	int m_SpitCooldown;
	int m_BroodCooldown;
	int m_ThrashCooldown;
	float m_DamageScale; // depth scaling of outgoing damage
	float m_Pressure;	  // recent damage taken (decays), triggers a counter-attack
	int ScaleDamage(int Dmg);
	int m_aPartHealth[NUM_MATRIARCH_PARTS];
	int m_aPartMax[NUM_MATRIARCH_PARTS];

	int m_Move;
	int m_MoveTimer;
	int m_StuckTicks;
	int m_BlockedTime; // walking but not getting anywhere
	CBossBody m_Body;
	float m_LastX;
	bool m_Supported;
	bool m_Climbing;
	vec2 m_PreVel;
	float m_Ground;

	// Pounce state.
	bool m_Airborne;
	bool m_Chained;
	bool m_ContactHit;
	int m_LaunchTick;
	int m_LandTick;
	int m_ExposeUntil;
	vec2 m_Aim;

	CShot m_aShots[MATRIARCH_MAX_SHOTS];
};

#endif
