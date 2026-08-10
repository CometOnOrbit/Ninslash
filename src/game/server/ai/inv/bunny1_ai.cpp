#include "bunny1_ai.h"

CAIbunny1::CAIbunny1(CGameContext *pGameServer, CPlayer *pPlayer, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pPlayer, Level, ProfileId)
{
}

void CAIbunny1::DoBehavior()
{
	RunProfileBehavior();
}
