#include "pvp_balance.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>

namespace
{
// Keep this table intentionally neutral until a balance pass supplies
// measured values. The table is the single mode-level extension point; Lua
// weapon profiles provide the per-weapon multipliers.
const CPvpModeBalance gs_aPvpModeBalances[] = {
	{"Deathmatch", -1, -1, -1, -1},
	{"Team deathmatch", -1, -1, -1, -1},
	{"Capture the flag", -1, -1, -1, -1},
	{"Reactor Assault", -1, -1, -1, -1},
	{"Ball", -1, -1, -1, -1},
	{"Battle Royale", -1, -1, -1, -1},
	{"Grenade DM", -1, -1, -1, -1},
	{"Instagib CTF", -1, -1, -1, -1},
};
}

const CPvpModeBalance *GetPvpModeBalance(const char *pProfile)
{
	if(!pProfile || !pProfile[0])
		return 0;
	for(const CPvpModeBalance &Balance : gs_aPvpModeBalances)
		if(str_comp_nocase(Balance.m_pName, pProfile) == 0)
			return &Balance;
	return 0;
}

void ApplyPvpModeBalance(CGameContext *pGameServer)
{
	if(!pGameServer || !pGameServer->m_pController || pGameServer->m_pController->IsCoop())
		return;

	const char *pProfile = g_Config.m_SvPvpProfile[0] ? g_Config.m_SvPvpProfile : pGameServer->m_pController->GameType();
	const CPvpModeBalance *pBalance = GetPvpModeBalance(pProfile);
	if(!pBalance)
		return;
	if(pBalance->m_RespawnDelay >= 0)
		g_Config.m_SvRespawnDelay = pBalance->m_RespawnDelay;
	if(pBalance->m_ScoreLimit >= 0)
		g_Config.m_SvScorelimit = pBalance->m_ScoreLimit;
	if(pBalance->m_BotLevel >= 0)
		g_Config.m_SvBotLevel = pBalance->m_BotLevel;
	if(pBalance->m_RandomWeapons >= 0)
		g_Config.m_SvRandomWeapons = pBalance->m_RandomWeapons;
}
