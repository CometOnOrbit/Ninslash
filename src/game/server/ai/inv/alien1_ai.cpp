#include "alien1_ai.h"

CAIalien1::CAIalien1(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pCharacter, Level, ProfileId)
{
}

void CAIalien1::DoBehavior()
{
	RunProfileBehavior();
}
