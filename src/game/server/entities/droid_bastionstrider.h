#ifndef GAME_SERVER_ENTITIES_DROID_BASTIONSTRIDER_H
#define GAME_SERVER_ENTITIES_DROID_BASTIONSTRIDER_H

#include <game/bastion_strider.h>
#include "droid_bossv5.h"

class CBastionStrider : public CBossV5
{
  public:
	CBastionStrider(CGameWorld *pGameWorld, vec2 Pos);

  protected:
	const CBossV5Act &ActInfo(int Act) override { return StriderAct(Act); }
	void Think() override;
	void TickAct(int Elapsed) override;
	void MoveBody() override;
	void TickShot(int i) override;
	int IncomingDamage(int Dmg, int Part, vec2 Pos, int *pPartDmg) override;
	void OnActStart(int Act) override;
	void TickTimers() override;
	void OnActFinished(int Act) override;
	bool Exposed() override;
	int StatusFlags() override;
	void SnapShot(const CShot &s, CNetObj_BossShot *pShot) override;
	void OnShotDestroyed(int i) override;
	void OnPartBroken(int Part) override;
	int PhaseAct() override { return STRIDER_ACT_ROAR; }
	int StaggerAct() override { return STRIDER_ACT_STAGGER; }
	void ExtraDebug(char *pBuffer, int Size) override;

  private:
	void FireShell(int Shot);
	void Slam(vec2 At, bool Waves);
	void ExplodeShell(int i, vec2 At);
	bool FloorAhead(float Dist);
	bool WallAhead(int Dir);

	int m_Move;
	int m_ChargeCooldown;
	int m_MortarCooldown;
	int m_StompCooldown;
	int m_LeapCooldown;
	int m_VentUntil;
	bool m_Airborne;
	bool m_Dashing;
	int m_LaunchTick;
	vec2 m_PreVel;
};

#endif
