#ifndef GAME_SERVER_ENTITIES_DROID_KAMIKAZESTAR_H
#define GAME_SERVER_ENTITIES_DROID_KAMIKAZESTAR_H

#include "droid.h"

const int KamikazeStarPhysSize = 48;

class CKamikazeStar : public CDroid
{
  public:
	CKamikazeStar(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

  private:
	bool FindTarget() override;
	bool ResolveTarget(vec2 *pTargetPos);
	void Detonate();

	bool m_Diving;
	bool m_Detonated;
	int m_DiveStartTick;
};

#endif
