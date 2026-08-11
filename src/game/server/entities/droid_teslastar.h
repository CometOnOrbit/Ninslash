#ifndef GAME_SERVER_ENTITIES_DROID_TESLASTAR_H
#define GAME_SERVER_ENTITIES_DROID_TESLASTAR_H

#include "droid_star.h"

const int TeslaStarPhysSize = 64;

class CCharacter;

class CTeslaStar : public CStar
{
  public:
	CTeslaStar(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;
	void TickPaused() override;

  private:
	void Fire() override;
	CCharacter *ValidTarget(int ClientID);
	int FindNextTarget(const bool *pHit, const vec2 &From);

	int m_NextCastTick;
};

#endif
