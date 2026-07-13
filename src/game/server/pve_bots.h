#ifndef GAME_SERVER_PVE_BOTS_H
#define GAME_SERVER_PVE_BOTS_H

#include <base/system.h>
#include <game/server/ai/inv/alien1_ai.h>
#include <game/server/ai/inv/alien2_ai.h>
#include <game/server/ai/inv/robot1_ai.h>
#include <game/server/ai/inv/robot2_ai.h>
#include <game/server/ai/inv/bunny1_ai.h>
#include <game/server/ai/inv/pyro1_ai.h>

inline CAI *CreatePveBotAI(CGameContext *pGameServer, CPlayer *pPlayer, int Level)
{
	Level = max(1, Level);
	const int Roll = rand() % 5;
	if(Level >= 8 && frandom() < 0.35f)
		return new CAIalien2(pGameServer, pPlayer);
	if(Level >= 7 && frandom() < 0.3f)
		return new CAIrobot2(pGameServer, pPlayer);

	switch(Roll)
	{
	case 0: return new CAIrobot1(pGameServer, pPlayer, Level);
	case 1: return new CAIbunny1(pGameServer, pPlayer, Level);
	case 2: return new CAIpyro1(pGameServer, pPlayer, Level);
	case 3: return new CAIalien1(pGameServer, pPlayer, Level);
	default: return new CAIalien1(pGameServer, pPlayer, Level);
	}
}

inline void TriggerAllBotAI(CGameContext *pGameServer, int TriggerLevel)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = pGameServer->m_apPlayers[i];
		if(pPlayer && pPlayer->m_pAI)
			pPlayer->m_pAI->Trigger(TriggerLevel);
	}
}

#endif
