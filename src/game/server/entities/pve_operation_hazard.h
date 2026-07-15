#ifndef GAME_SERVER_ENTITIES_PVE_OPERATION_HAZARD_H
#define GAME_SERVER_ENTITIES_PVE_OPERATION_HAZARD_H

#include <game/server/entity.h>

class CPveOperationHazard : public CEntity
{
public:
	enum EKind
	{
		BOMBARDMENT,
		ROTATING_EMP,
	};

private:
	EKind m_Kind;
	vec2 m_Anchor;
	int m_EndTick;
	int m_NextActionTick;
	int m_Phase;

public:
	CPveOperationHazard(CGameWorld *pWorld, vec2 Pos, EKind Kind, int DurationTicks);
	void Reset() override;
	void Tick() override;
	void TickPaused() override;
};

#endif
