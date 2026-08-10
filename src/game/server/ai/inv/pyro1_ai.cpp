#include "pyro1_ai.h"

CAIpyro1::CAIpyro1(CGameContext *pGameServer, CPlayer *pPlayer, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pPlayer, Level, ProfileId)
{
}

void CAIpyro1::DoBehavior()
{
	RunProfileBehavior();
}
