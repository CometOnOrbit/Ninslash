#ifndef GAME_SERVER_AI_DM_AI_H
#define GAME_SERVER_AI_DM_AI_H
#include <game/server/ai/ai.h>
#include <game/server/core/gamecontext.h>

class CAIdm : public CAI
{
public:
	CAIdm(CGameContext *pGameServer, CPlayer *pPlayer);

	virtual void DoBehavior();
	void OnCharacterSpawn(class CCharacter *pChr);
	void ReceiveDamage(int CID, int Dmg);

private:
	int m_SkipMoveUpdate;
};

#endif
