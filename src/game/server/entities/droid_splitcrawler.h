#ifndef GAME_SERVER_ENTITIES_DROID_SPLITCRAWLER_H
#define GAME_SERVER_ENTITIES_DROID_SPLITCRAWLER_H

#include "droid.h"

const int SplitCrawlerPhysSize = 36;

class CSplitCrawler : public CDroid
{
  public:
	CSplitCrawler(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

  private:
	bool FindTarget() override;
	bool Target() override;

	int m_Move;
	int m_AttackCount;
	int m_JumpTick;
	float m_JumpForce;
	bool m_DeathHandled;
};

#endif
