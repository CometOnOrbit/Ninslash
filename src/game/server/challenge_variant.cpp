#include <game/challenge_variant.h>

#include <engine/shared/config.h>

#include <game/server/gamecontext.h>

int ApplyChallengeVariants(CGameContext *pGameServer)
{
	const int Mask = g_Config.m_SvChallengeVariants;
	if(Mask <= 0)
		return 0;
	int Applied = 0;

	if(ChallengeVariantEnabled(Mask, CHALLENGE_LOW_GRAVITY))
	{
		CTuningParams *pTuning = pGameServer->Tuning();
		pTuning->m_Gravity = (pTuning->m_Gravity.Get() / 100.0f) * 0.6f;
		Applied++;
	}
	if(ChallengeVariantEnabled(Mask, CHALLENGE_NO_BUILD))
	{
		g_Config.m_SvEnableBuilding = 0;
		Applied++;
	}
	// CHALLENGE_DOUBLE_ENEMIES / CHALLENGE_GLASS_CANNON / CHALLENGE_ONLY_MELEE
	// require generation / logic hooks; see docs/playability_design.md §2.1.

	return Applied;
}
