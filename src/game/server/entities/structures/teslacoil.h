#ifndef GAME_SERVER_ENTITIES_STRUCTURES_TESLACOIL_H
#define GAME_SERVER_ENTITIES_STRUCTURES_TESLACOIL_H

#include <game/server/core/entity.h>
#include <game/server/entities/structures/building.h>


class CTeslacoil : public CBuilding
{
public:
	CTeslacoil(CGameWorld *pGameWorld, vec2 Pos, int Team, int OwnerPlayer = -1);

	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	int m_OwnerPlayer;
	int m_AttackTick;
	int m_FlipY;
	
private:
	void Fire();
};

#endif
