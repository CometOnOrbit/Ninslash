#ifndef GAME_SERVER_AI_ALIEN2_AI_H
#define GAME_SERVER_AI_ALIEN2_AI_H

#include "invasion_ai.h"

class CAIalien2 : public CInvasionAI
{
  public:
	CAIalien2(CGameContext *pGameServer, CCharacter *pCharacter, int Level = 0,
		EInvasionSkinId ProfileId = INVASION_SKIN_ELITE_ALIEN_ALPHA);

	void DoBehavior() override;
};

#endif
