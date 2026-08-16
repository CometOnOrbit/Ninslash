#include "pyro1_ai.h"

CAIpyro1::CAIpyro1(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pCharacter, Level, ProfileId)
{
}

void CAIpyro1::DoBehavior()
{
	RunProfileBehavior();
}
