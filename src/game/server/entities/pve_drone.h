#ifndef GAME_SERVER_ENTITIES_PVE_DRONE_H
#define GAME_SERVER_ENTITIES_PVE_DRONE_H

#include <game/server/entity.h>

class CPveDrone : public CEntity
{
public:
	CPveDrone(CGameWorld *pGameWorld, int Owner);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

	int Owner() const { return m_Owner; }

private:
	int m_Owner;
	int m_StartTick;
};

#endif
