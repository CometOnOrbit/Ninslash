#ifndef GAME_SERVER_ENTITIES_ACTORS_DROID_WALKER_H
#define GAME_SERVER_ENTITIES_ACTORS_DROID_WALKER_H

#include <game/server/core/entity.h>
#include <game/server/entities/actors/droid.h>

const int WalkerPhysSize = 60;

class CWalker : public CDroid
{
public:
	CWalker(CGameWorld *pGameWorld, vec2 Pos);

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
