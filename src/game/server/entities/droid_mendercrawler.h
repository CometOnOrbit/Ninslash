#ifndef GAME_SERVER_ENTITIES_DROID_MENDERCRAWLER_H
#define GAME_SERVER_ENTITIES_DROID_MENDERCRAWLER_H

#include "droid.h"

const int MenderCrawlerPhysSize = 40;

class CMenderCrawler : public CDroid
{
  public:
	CMenderCrawler(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

  private:
	bool IsValidHealTarget(const CDroid *pDroid) const;
	void FindHealTarget();
	bool EvadePlayers();
	void HealNearbyDroids();

	CDroid *m_pHealTarget;
	int m_HealTick;
	int m_Move;
	int m_JumpTick;
	float m_JumpForce;
};

#endif
