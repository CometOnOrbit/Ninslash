#ifndef GAME_SERVER_ENTITIES_DROID_CRAWLER_H
#define GAME_SERVER_ENTITIES_DROID_CRAWLER_H

#include <game/server/entity.h>
#include "droid.h"

const int CrawlerPhysSize = 40;

class CCrawler : public CDroid
{
public:
	CCrawler(CGameWorld *pGameWorld, vec2 Pos);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

private:
	bool FindTarget();
	bool Target();
	void Fire();
	
	void Move();
	void MoveDead();
	
	int m_Move;
	
	vec2 m_MoveTarget;
	float m_AngleTimer;
	
	int m_AttackCount;
	
	int m_JumpTick;
	float m_JumpForce;
};

#endif
