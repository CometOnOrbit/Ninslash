#ifndef GAME_SERVER_ENTITIES_PVE_OPERATION_TARGET_H
#define GAME_SERVER_ENTITIES_PVE_OPERATION_TARGET_H

#include <game/server/entities/building.h>

class CPveOperationDirector;
class CRadar;

// A server-owned objective area. It deliberately uses the existing radar
// snapshot, so operation objectives need no new network object or client code.
class CPveOperationTarget : public CBuilding
{
	CPveOperationDirector *m_pDirector;
	CRadar *m_pRadar;
	float m_Radius;
	int m_RequiredTicks;
	int m_ProgressTicks;
	bool m_Complete;
	int m_TargetType;
	int m_CargoType;
	vec2 m_SourcePos;
	vec2 m_DeliveryPos;
	int m_CarrierCID;

public:
	CPveOperationTarget(CGameWorld *pWorld, CPveOperationDirector *pDirector, vec2 Pos, vec2 DeliveryPos, float Radius, int RequiredTicks, int TargetType);
	virtual ~CPveOperationTarget();
	virtual void Reset();
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	void DetachDirector() { m_pDirector = 0; }
	int CarrierCID() const { return m_CarrierCID; }
	vec2 HudTargetPos() const { return m_CarrierCID >= 0 ? m_DeliveryPos : m_Pos; }
};

#endif
