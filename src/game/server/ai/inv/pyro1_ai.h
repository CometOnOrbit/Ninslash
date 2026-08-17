#ifndef GAME_SERVER_AI_PYRO1_AI_H
#define GAME_SERVER_AI_PYRO1_AI_H

#include "invasion_ai.h"

class CAIpyro1 : public CInvasionAI
{
  public:
	CAIpyro1(CGameContext *pGameServer, CCharacter *pCharacter, int Level,
		EInvasionSkinId ProfileId = INVASION_SKIN_PYRO1);

	void DoBehavior() override;
};

#endif
