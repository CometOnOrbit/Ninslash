#ifndef GAME_SERVER_ENTITIES_DROID_TEMPESTSTAR_H
#define GAME_SERVER_ENTITIES_DROID_TEMPESTSTAR_H

#include "droid_star.h"

const int TempestStarPhysSize = 66;

class CTempestStar : public CStar
{
  public:
	CTempestStar(CGameWorld *pGameWorld, vec2 Pos);

	void Reset() override;

  private:
	void Fire() override;
};

#endif
