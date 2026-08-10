#include "pyro2_ai.h"

CAIpyro2::CAIpyro2(CGameContext *pGameServer, CPlayer *pPlayer, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pPlayer, Level, ProfileId)
{
}

void CAIpyro2::DoBehavior()
{
	RunProfileBehavior();
}
