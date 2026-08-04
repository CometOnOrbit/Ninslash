#ifndef GAME_SERVER_PVP_BALANCE_H
#define GAME_SERVER_PVP_BALANCE_H

class CGameContext;

// Optional per-mode overrides. A negative value leaves the controller's
// existing configuration unchanged, which keeps the initial data table
// backwards compatible while allowing later balance passes without changing
// gamemode code.
struct CPvpModeBalance
{
	const char *m_pName;
	int m_RespawnDelay;
	int m_ScoreLimit;
	int m_BotLevel;
	int m_RandomWeapons;
};

const CPvpModeBalance *GetPvpModeBalance(const char *pProfile);
void ApplyPvpModeBalance(CGameContext *pGameServer);

#endif
