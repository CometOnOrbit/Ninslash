#ifndef GAME_SERVER_ENTITIES_DROID_BOSSWALKER_H
#define GAME_SERVER_ENTITIES_DROID_BOSSWALKER_H

#include <game/server/entity.h>
#include "droid.h"

const int BossWalkerPhysSize = 80;

class CBossWalker : public CDroid
{
public:
	CBossWalker(CGameWorld *pGameWorld, vec2 Pos);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

	enum Mode
	{
		WALKER,
		DRONE,
	};
	
private:

	bool FindTarget();
	bool Target();
	void Fire();
};

#endif
