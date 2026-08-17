#include "alien2_ai.h"

CAIalien2::CAIalien2(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pCharacter, Level, ProfileId)
{
}

void CAIalien2::DoBehavior()
{
	RunProfileBehavior();
}
