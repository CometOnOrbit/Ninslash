#ifndef GAME_SERVER_AI_BUNNY2_AI_H
#define GAME_SERVER_AI_BUNNY2_AI_H

#include "invasion_ai.h"

class CAIbunny2 : public CInvasionAI
{
  public:
	CAIbunny2(CGameContext *pGameServer, CPlayer *pPlayer, int Level = 0,
		EInvasionSkinId ProfileId = INVASION_SKIN_ELITE_BUNNY_ASSASSIN);

	void DoBehavior() override;
};

#endif
