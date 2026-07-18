#ifndef GAME_SERVER_ENTITIES_COMBAT_DEATHRAY_H
#define GAME_SERVER_ENTITIES_COMBAT_DEATHRAY_H

#include <game/server/core/entity.h>
#include <game/server/entities/structures/building.h>


class CDeathray : public CBuilding
{
public:
	CDeathray(CGameWorld *pGameWorld, vec2 Pos);

	virtual void Tick();
	
	int m_Height;
	int m_AttackTick;
	bool m_Loading;
private:
};

#endif
