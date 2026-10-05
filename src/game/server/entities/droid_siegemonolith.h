#ifndef GAME_SERVER_ENTITIES_DROID_SIEGEMONOLITH_H
#define GAME_SERVER_ENTITIES_DROID_SIEGEMONOLITH_H

#include <game/siege_monolith.h>
#include "droid_bossv5.h"

class CSiegeMonolith : public CBossV5
{
  public:
	CSiegeMonolith(CGameWorld *pGameWorld, vec2 Pos);

  protected:
	const CBossV5Act &ActInfo(int Act) override { return MonolithAct(Act); }
	void Think() override;
	void TickAct(int Elapsed) override;
	void MoveBody() override;
	void TickShot(int i) override;
	int IncomingDamage(int Dmg, int Part, vec2 Pos, int *pPartDmg) override;
	bool HitCircleActive(int Part) override;
	void OnActStart(int Act) override;
	void OnActFinished(int Act) override;
	void OnPartBroken(int Part) override;
	void TickTimers() override;
	bool Exposed() override;
	int StatusFlags() override;
	vec2 StatusAim() override;
	void SnapShot(const CShot &s, CNetObj_BossShot *pShot) override;
	void OnShotDestroyed(int i) override;
	int PhaseAct() override { return MONOLITH_ACT_ROAR; }
	int StaggerAct() override { return MONOLITH_ACT_STAGGER; }
	void ExtraDebug(char *pBuffer, int Size) override;

  private:
	bool WallAhead(int Dir);
	int AliveDrones();
	vec2 Muzzle();
	void FireShell(int Shot);
	void Explode(int i, vec2 At, float Radius, int Dmg);
	void Blast(vec2 At);
	void SlamImpact();
	float AimAngle(vec2 To); // facing-space angle from the turret pivot

	bool m_Air;
	bool m_Landed; // slam has hit the floor
	bool m_Falling;
	int m_FormActs;
	int m_Move;
	int m_SweepCooldown;
	int m_MortarCooldown;
	int m_BombCooldown;
	int m_DroneCooldown;
	int m_SlamCooldown;
	int m_FormCooldown;
	float m_SweepFrom;
	float m_SweepTo;
	float m_TurretAngle;
	int m_BombDir;
	vec2 m_FlyTo;
};

#endif
