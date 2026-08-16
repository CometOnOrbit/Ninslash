#ifndef GAME_SERVER_AI_PYRO2_AI_H
#define GAME_SERVER_AI_PYRO2_AI_H

#include "invasion_ai.h"

class CAIpyro2 : public CInvasionAI
{
  public:
	CAIpyro2(CGameContext *pGameServer, CCharacter *pCharacter, int Level = 0,
		EInvasionSkinId ProfileId = INVASION_SKIN_ELITE_PYRO_SIEGE);

	void DoBehavior() override;
};

#endif
