#ifndef GAME_SERVER_ENTITIES_STRUCTURES_POWERUPPER_H
#define GAME_SERVER_ENTITIES_STRUCTURES_POWERUPPER_H

#include <game/server/core/entity.h>
#include <game/server/entities/structures/building.h>


class CPowerupper : public CBuilding
{
public:
	CPowerupper(CGameWorld *pGameWorld, vec2 Pos);

	virtual void Reset();
	virtual void SurvivalReset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	
	int m_Item;
	int m_ItemTakenTick;
private:
};

#endif
