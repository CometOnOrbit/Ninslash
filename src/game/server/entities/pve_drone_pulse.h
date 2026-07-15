#ifndef GAME_SERVER_ENTITIES_PVE_DRONE_PULSE_H
#define GAME_SERVER_ENTITIES_PVE_DRONE_PULSE_H

#include <game/server/entity.h>

class CPveDronePulse : public CEntity
{
public:
	CPveDronePulse(CGameWorld *pGameWorld, vec2 From, vec2 To, int Owner, int Weapon);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

private:
	vec2 m_From;
	vec2 m_To;
	int m_Owner;
	int m_Weapon;
	int m_StartTick;
	int m_EndTick;
};

#endif
