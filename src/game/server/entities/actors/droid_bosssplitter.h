#ifndef GAME_SERVER_ENTITIES_ACTORS_DROID_BOSSSPLITTER_H
#define GAME_SERVER_ENTITIES_ACTORS_DROID_BOSSSPLITTER_H

#include <game/server/core/entity.h>
#include <game/server/entities/actors/droid.h>

const int BossSplitterPhysSize = 70;

class CBossSplitter : public CDroid
{
public:
	CBossSplitter(CGameWorld *pGameWorld, vec2 Pos);

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
