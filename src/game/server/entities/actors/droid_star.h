#ifndef GAME_SERVER_ENTITIES_ACTORS_DROID_STAR_H
#define GAME_SERVER_ENTITIES_ACTORS_DROID_STAR_H

#include <game/server/core/entity.h>
#include <game/server/entities/actors/droid.h>

const int StarPhysSize = 60;

class CStar : public CDroid
{
public:
	CStar(CGameWorld *pGameWorld, vec2 Pos);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();

	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;
	
private:
	bool FindTarget();
	bool Target();
	void Fire();
	
	vec2 m_MoveTarget;
	float m_AngleTimer;
};

#endif
