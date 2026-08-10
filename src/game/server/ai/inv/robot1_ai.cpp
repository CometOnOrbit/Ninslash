#include "robot1_ai.h"

CAIrobot1::CAIrobot1(CGameContext *pGameServer, CPlayer *pPlayer, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pPlayer, Level, ProfileId)
{
}

void CAIrobot1::DoBehavior()
{
	RunProfileBehavior();
}
