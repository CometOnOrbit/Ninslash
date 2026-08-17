#include "robot2_ai.h"

CAIrobot2::CAIrobot2(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pCharacter, Level, ProfileId)
{
}

void CAIrobot2::DoBehavior()
{
	RunProfileBehavior();
}
