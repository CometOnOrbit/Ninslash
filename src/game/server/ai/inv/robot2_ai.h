#ifndef GAME_SERVER_AI_ROBOT2_AI_H
#define GAME_SERVER_AI_ROBOT2_AI_H

#include "invasion_ai.h"

class CAIrobot2 : public CInvasionAI
{
  public:
	CAIrobot2(CGameContext *pGameServer, CCharacter *pCharacter, int Level = 0,
		EInvasionSkinId ProfileId = INVASION_SKIN_CYBORG_GUNNER);

	void DoBehavior() override;
};

#endif
