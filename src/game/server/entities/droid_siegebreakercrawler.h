#ifndef GAME_SERVER_ENTITIES_DROID_SIEGEBREAKERCRAWLER_H
#define GAME_SERVER_ENTITIES_DROID_SIEGEBREAKERCRAWLER_H

#include <game/server/entity.h>

#include "droid.h"

const int SiegeBreakerCrawlerPhysSize = 56;

class CSiegeBreakerCrawler : public CDroid
{
  public:
	CSiegeBreakerCrawler(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

  private:
	bool FindTarget();
	bool Target();
	bool IsGrounded();
	void CreateLandingImpact();

	int m_Move;
	int m_AttackCount;
	int m_JumpTick;
	float m_JumpForce;
	bool m_LandingImpactArmed;
	bool m_JumpWasAirborne;
};

#endif
