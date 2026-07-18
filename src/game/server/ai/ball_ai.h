#ifndef GAME_SERVER_AI_BALL_AI_H
#define GAME_SERVER_AI_BALL_AI_H
#include <game/server/ai/ai.h>
#include <game/server/core/gamecontext.h>

class CAIball : public CAI
{
public:
	CAIball(CGameContext *pGameServer, CPlayer *pPlayer);

	virtual void DoBehavior();
	void OnCharacterSpawn(class CCharacter *pChr);

private:
	int m_SkipMoveUpdate;
	bool SeekFriend();
	
	int m_Team;
	int m_Role;
	
	vec2 m_aGoalPos[2];
};

#endif
