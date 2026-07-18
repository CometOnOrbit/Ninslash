#ifndef GAME_SERVER_AI_INV_PYRO2_AI_H
#define GAME_SERVER_AI_INV_PYRO2_AI_H
#include <game/server/ai/ai.h>
#include <game/server/core/gamecontext.h>

class CAIpyro2 : public CAI
{
public:
	CAIpyro2(CGameContext *pGameServer, CPlayer *pPlayer);

	virtual void DoBehavior();
	void OnCharacterSpawn(class CCharacter *pChr);
	void ReceiveDamage(int CID, int Dmg);

private:
	int m_SkipMoveUpdate;
	bool m_Triggered;
	vec2 m_StartPos;
	
	int m_ShockTimer;
};

#endif
