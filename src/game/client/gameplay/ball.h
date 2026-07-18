#ifndef GAME_CLIENT_GAMEPLAY_BALL_H
#define GAME_CLIENT_GAMEPLAY_BALL_H
#include <game/client/core/component.h>

class CBalls : public CComponent
{
	void RenderBall(
		const CNetObj_Ball *pPrevBall,
		const CNetObj_Ball *pBall);
	
public:
	virtual void OnRender();
};

#endif
