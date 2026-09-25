#ifndef GAME_SERVER_PVE_BOTS_H
#define GAME_SERVER_PVE_BOTS_H

#include <base/system.h>
#include <base/math.h>
#include <base/deterministic_random.h>
#include <game/server/ai/inv/invasion_ai.h>

inline CAI *CreatePveBotAI(CGameContext *pGameServer, CCharacter *pCharacter, int Level)
{
	Level = max(1, Level);
	const int Roll = irandom(5);
	if(Level >= 8 && frandom() < 0.35f)
		return new CInvasionAI(pGameServer, pCharacter, Level, INVASION_SKIN_ELITE_ALIEN_ALPHA);
	if(Level >= 7 && frandom() < 0.3f)
		return new CInvasionAI(pGameServer, pCharacter, Level, INVASION_SKIN_CYBORG_GUNNER);

	switch(Roll)
	{
		case 0:
			return new CInvasionAI(pGameServer, pCharacter, Level, INVASION_SKIN_ROBO1);
		case 1:
			return new CInvasionAI(pGameServer, pCharacter, Level, INVASION_SKIN_BUNNY1);
		case 2:
			return new CInvasionAI(pGameServer, pCharacter, Level, INVASION_SKIN_PYRO1);
		case 3:
			return new CInvasionAI(pGameServer, pCharacter, Level, INVASION_SKIN_ALIEN1);
		default:
			return new CInvasionAI(pGameServer, pCharacter, Level, INVASION_SKIN_ALIEN1);
	}
}

inline void TriggerAllBotAI(CGameContext *pGameServer, int TriggerLevel)
{
	pGameServer->TriggerBotAI(TriggerLevel);
}

#endif
