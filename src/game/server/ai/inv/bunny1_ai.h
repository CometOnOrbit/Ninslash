#ifndef GAME_SERVER_AI_BUNNY1_AI_H
#define GAME_SERVER_AI_BUNNY1_AI_H

#include "invasion_ai.h"

class CAIbunny1 : public CInvasionAI
{
  public:
	CAIbunny1(CGameContext *pGameServer, CCharacter *pCharacter, int Level,
		EInvasionSkinId ProfileId = INVASION_SKIN_BUNNY1);

	void DoBehavior() override;
};

#endif
