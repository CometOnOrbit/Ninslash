#ifndef GAME_SERVER_ENTITIES_DROID_BOSSV5_H
#define GAME_SERVER_ENTITIES_DROID_BOSSV5_H

#include <game/boss_v5.h>
#include "boss_body.h"
#include "droid.h"

class CCharacter;

// Common plumbing for the v5 bosses (Bastion Strider, Storm Seraph, Siege Monolith):
// act clock, targeting, damage routing through breakable parts, a projectile pool that is
// snapped as BossShot, the boss bar and the death/loot sequence. Each boss supplies the AI,
// body movement and its part rules.
class CBossV5 : public CDroid
{
  public:
	CBossV5(CGameWorld *pGameWorld, vec2 Pos, int Type, int BaseHealth, const float *pPartShare,
		const CBossV5Hit *pHits, int NumHits, int DeathAct);
	virtual ~CBossV5();

	void Tick() override;
	void TickPaused() override {}
	void Snap(int SnappingClient) override;
	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;
	bool HitSegment(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pAt) override;

	// Debug / test hooks (ib_test_* console commands).
	virtual void DebugAct(int Act);
	virtual void DebugState(char *pBuffer, int Size);
	int Act() { return m_Act; }

  protected:
	struct CShot
	{
		int m_ID;
		int m_Kind;
		vec2 m_Pos;
		vec2 m_Vel;
		int m_Life;
		int m_Data; // per-kind payload (direction, node index, ...)
		int m_Hp;	// >0: can be shot down
	};

	// ---- per boss ----
	virtual const CBossV5Act &ActInfo(int Act) = 0;
	virtual void Think() = 0;			   // pick the next act (called when idle/moving)
	virtual void TickAct(int Elapsed) = 0; // run the current act
	virtual void MoveBody() = 0;
	virtual void TickShot(int i) = 0;
	virtual void OnActStart(int Act) {}
	virtual void TickTimers() {}
	virtual bool IsLocomotion(int Act) { return Act <= 1; }
	// Returns the hull damage; *pPartDmg (default: same) is what the hit part loses.
	virtual int IncomingDamage(int Dmg, int Part, vec2 Pos, int *pPartDmg) = 0;
	virtual bool HitCircleActive(int Part) { return Part == 0 || PartAlive(Part); }
	virtual void OnPartBroken(int Part) {}
	virtual void OnActFinished(int Act) {}
	virtual bool Exposed() = 0;
	virtual int StatusFlags();
	virtual vec2 StatusAim() { return m_Aim; }
	virtual void SnapShot(const CShot &s, CNetObj_BossShot *pShot);
	virtual bool ShotHittable(const CShot &s) { return s.m_Hp > 0; }
	virtual void OnShotDestroyed(int i) {}
	virtual void OnDeathStart() {}
	virtual void OnDeathTick(int Elapsed) {}
	virtual bool Invulnerable() { return false; }
	virtual int PhaseAct() = 0;	// roar
	virtual int StaggerAct() = 0; // stagger
	virtual void ExtraDebug(char *pBuffer, int Size) { pBuffer[0] = 0; }

	// ---- helpers ----
	void StartAct(int Act);
	void SetLocomotion(int Act);
	void FinishAct();
	bool AcquireTarget(bool NeedSight, bool Face = true);
	CCharacter *TargetChr();
	int HurtPlayers(vec2 Pos, float Radius, int Dmg, float Knock, float Lift = 0.25f);
	int HurtSegment(vec2 From, vec2 To, float Radius, int Dmg, float Knock, int *pCooldowns = 0, int Cooldown = 0);
	bool PartAlive(int Part) { return Part == 0 || m_aPartHealth[Part] > 0; }
	vec2 LocalToWorld(float x, float y) { return m_Pos + vec2(x * m_Dir, y); }
	int AddShot(int Kind, vec2 Pos, vec2 Vel, int Data = 0, int Hp = 0);
	void RemoveShot(int i);
	void ClearShots();
	int CountShots(int Kind);
	bool ProbeGround(float Hover, float Spread, float Extra); // fills m_Ground, m_Supported
	// Crawler-style ride height: the floor is felt under the middle, the box edges (+-Spread) and
	// the feet (+-Far). The body rides at the AVERAGE of middle and feet (so on a slope it sits down
	// between its legs instead of perching on the uphill point) but keeps its box BoxHalfH+margin clear
	// of the highest floor under it. Fills m_Ground (floor under the middle) and m_Supported.
	float ProbeRide(float Hover, float Spread, float Far, float BoxHalfH, float Extra);
	void HoverSpringTo(float TargetY);
	void HoverSpring(float Hover);
	// ---- full-silhouette body (collision box == art) ----
	// Ground bosses: gravity + full-box move. Walk = steer toward WantVx with Accel, step up slopes,
	// stick to the ground downhill, hop onto ledges up to MaxJump px, drop through one-way platforms
	// when the target is below, and never stride off a drop unless the target is down there.
	// Returns CBossBody::MOVE_* flags (plus WALK_HOPPED); keeps m_Ground/m_Supported in sync.
	enum
	{
		WALK_HOPPED = 64,
	};
	int WalkBody(float WantVx, float Accel, float Gravity, bool Walk, float MaxJump);
	void SyncGround();
	// A wall the body cannot step over within Dist px ahead.
	bool BodyWallAhead(int Dir, float Dist);
	vec2 FeetPos() { return vec2(m_Pos.x, m_Body.Feet(m_Pos)); }
	CBossBody m_Body;
	bool m_BlockedX;
	void DropLoot();
	int Elapsed();
	int ScaleDamage(int Dmg); // depth + phase scaling for every hit a boss lands
	bool Pressured() { return m_Pressure > m_MaxHealth * BOSSV5_PRESSURE_TRIGGER; }
	void ClearPressure() { m_Pressure = 0.0f; }

	const CBossV5Hit *m_pHits;
	int m_NumHits;
	int m_DeathAct;

	int m_Act;
	int m_ActTick;
	int m_Events;
	int m_Phase;
	int m_Stagger;
	int m_ActsDone;
	int m_aPartHealth[BOSSV5_NUM_PARTS];
	int m_aPartMax[BOSSV5_NUM_PARTS];
	vec2 m_Aim;
	float m_Ground;
	bool m_Supported;
	int m_StuckTicks;
	int m_BlockedTime; // walking but not getting anywhere (decays while moving freely)
	bool StuckLong() const { return m_BlockedTime > SERVER_TICK_SPEED * 4; } // 4 s: switch to ranged moves
	float m_LastX;
	int m_aHitCooldown[MAX_CHARACTERS];
	float m_DamageScale;
	float m_Pressure;

	CShot m_aShots[BOSSV5_MAX_SHOTS];
};

#endif
