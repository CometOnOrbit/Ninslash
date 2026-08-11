#ifndef GAME_SERVER_ENTITIES_DROID_STALKERCRAWLER_H
#define GAME_SERVER_ENTITIES_DROID_STALKERCRAWLER_H

#include "droid.h"

const int StalkerCrawlerPhysSize = 36;

class CStalkerCrawler : public CDroid
{
  public:
	CStalkerCrawler(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

  private:
	bool FindTarget() override;
	bool Target() override;
	void UpdateStealthStatus(bool HasTarget);

	int m_Move;
	int m_AttackCount;
	int m_JumpTick;
	float m_JumpForce;
	int m_RevealUntilTick;
};

#endif
