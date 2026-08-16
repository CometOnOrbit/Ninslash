#ifndef GAME_SERVER_AI_ROBOT1_AI_H
#define GAME_SERVER_AI_ROBOT1_AI_H

#include "invasion_ai.h"

class CAIrobot1 : public CInvasionAI
{
  public:
	CAIrobot1(CGameContext *pGameServer, CCharacter *pCharacter, int Level,
		EInvasionSkinId ProfileId = INVASION_SKIN_ROBO1);

	void DoBehavior() override;
};

#endif
