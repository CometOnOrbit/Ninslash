#ifndef GAME_SERVER_ENTITIES_DROID_RAILSTAR_H
#define GAME_SERVER_ENTITIES_DROID_RAILSTAR_H

#include <game/server/entity.h>

#include "droid.h"

class CRailstar : public CDroid
{
  public:
	CRailstar(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

  private:
	bool FindTarget() override;
	bool ResolveTarget(vec2 *pDirection);
	vec2 MuzzlePos() const;
	void BeginCharge();
	void CancelCharge();
	void ShowTelegraph(const vec2 &Direction);
	void FireRail(const vec2 &Direction);

	bool m_Charging;
	int m_ChargeTicksRemaining;
	int m_CooldownTicksRemaining;
	int m_TelegraphTick;
	float m_HoverPhase;
};

#endif
