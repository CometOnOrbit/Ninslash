#include "alien1_ai.h"

CAIalien1::CAIalien1(CGameContext *pGameServer, CPlayer *pPlayer, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pPlayer, Level, ProfileId)
{
}

void CAIalien1::DoBehavior()
{
	RunProfileBehavior();
}
