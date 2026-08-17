#include "bunny2_ai.h"

CAIbunny2::CAIbunny2(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pCharacter, Level, ProfileId)
{
}

void CAIbunny2::DoBehavior()
{
	RunProfileBehavior();
}
