#include "alien2_ai.h"

CAIalien2::CAIalien2(CGameContext *pGameServer, CPlayer *pPlayer, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pPlayer, Level, ProfileId)
{
}

void CAIalien2::DoBehavior()
{
	RunProfileBehavior();
}
