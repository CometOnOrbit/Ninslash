#include "pyro2_ai.h"

CAIpyro2::CAIpyro2(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pCharacter, Level, ProfileId)
{
}

void CAIpyro2::DoBehavior()
{
	RunProfileBehavior();
}
