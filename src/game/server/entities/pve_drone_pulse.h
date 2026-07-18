#ifndef GAME_SERVER_ENTITIES_PVE_DRONE_PULSE_H
#define GAME_SERVER_ENTITIES_PVE_DRONE_PULSE_H

#include <game/server/entity.h>
#include <game/weapon_catalog.h>

class CPveDronePulse : public CEntity
{
public:
	CPveDronePulse(CGameWorld *pGameWorld, vec2 From, vec2 To, const CAttackSource &Source);

	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void Snap(int SnappingClient) override;

private:
	vec2 m_From;
	vec2 m_To;
	CAttackSource m_Source;
	int m_StartTick;
	int m_EndTick;
};

#endif
