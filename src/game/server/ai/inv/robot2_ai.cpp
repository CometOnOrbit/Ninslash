#include "robot2_ai.h"

CAIrobot2::CAIrobot2(CGameContext *pGameServer, CPlayer *pPlayer, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pPlayer, Level, ProfileId)
{
}

void CAIrobot2::DoBehavior()
{
	RunProfileBehavior();
}
