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
	int Health() const { return m_Health; }
	int DisabledUntilTick() const { return m_DisabledUntilTick; }
	bool Active();
	bool TakeDamage(int Damage);
	void ApplyEmp(int DurationTicks);
	void SetAction(vec2 Target, int ActionTick);

private:
	int m_Owner;
	int m_StartTick;
	int m_Health;
	int m_DisabledUntilTick;
	vec2 m_Vel;
	vec2 m_Target;
	int m_ActionTick;
};

#endif
