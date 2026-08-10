#include "bunny2_ai.h"

CAIbunny2::CAIbunny2(CGameContext *pGameServer, CPlayer *pPlayer, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pPlayer, Level, ProfileId)
{
}

void CAIbunny2::DoBehavior()
{
	RunProfileBehavior();
}
