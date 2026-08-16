#include "robot1_ai.h"

CAIrobot1::CAIrobot1(CGameContext *pGameServer, CCharacter *pCharacter, int Level, EInvasionSkinId ProfileId) :
	CInvasionAI(pGameServer, pCharacter, Level, ProfileId)
{
}

void CAIrobot1::DoBehavior()
{
	RunProfileBehavior();
}
