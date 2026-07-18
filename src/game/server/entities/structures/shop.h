#ifndef GAME_SERVER_ENTITIES_STRUCTURES_SHOP_H
#define GAME_SERVER_ENTITIES_STRUCTURES_SHOP_H

#include <game/server/core/entity.h>
#include <game/server/entities/structures/building.h>


class CShop : public CBuilding
{
public:
	CShop(CGameWorld *pGameWorld, vec2 Pos);

	virtual void Reset();
	virtual void SurvivalReset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	
	virtual CWeaponSpec GetItem(int Slot);
	virtual void ClearItem(int Slot);
	
private:
	CWeaponSpec m_aItem[5];
	bool m_Autofill;
	
	void FillSlots();
};

#endif
