#ifndef GAME_SERVER_AI_ALIEN1_AI_H
#define GAME_SERVER_AI_ALIEN1_AI_H

#include "invasion_ai.h"

class CAIalien1 : public CInvasionAI
{
  public:
	CAIalien1(CGameContext *pGameServer, CPlayer *pPlayer, int Level,
		EInvasionSkinId ProfileId = INVASION_SKIN_ALIEN1);

	void DoBehavior() override;
};

#endif
