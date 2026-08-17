#include "bunny1_ai.h"

CAIbunny1::CAIbunny1(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pCharacter, Level, ProfileId)
{
}

void CAIbunny1::DoBehavior()
{
	RunProfileBehavior();
}
