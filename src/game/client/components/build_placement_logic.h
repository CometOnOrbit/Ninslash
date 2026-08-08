#ifndef GAME_CLIENT_COMPONENTS_BUILD_PLACEMENT_LOGIC_H
#define GAME_CLIENT_COMPONENTS_BUILD_PLACEMENT_LOGIC_H

#include <base/math.h>
#include <base/vmath.h>

namespace BuildPlacementLogic
{
enum EState
{
	STATE_IDLE,
	STATE_WHEEL,
	STATE_PLACEMENT,
};

enum EInvalidReason
{
	INVALID_NONE,
	INVALID_NO_KITS,
	INVALID_OCCUPIED,
	INVALID_NO_SUPPORT,
	INVALID_NO_CLEARANCE,
	INVALID_TOO_CLOSE,
	INVALID_NO_WALL,
	INVALID_NO_CEILING,
	INVALID_FORCE_TILE,
};

struct CBuildPlacementResult
{
	vec2 m_Position = vec2(0, 0);
	vec2 m_PreviewPosition = vec2(0, 0);
	vec2 m_AnchorPosition = vec2(0, 0);
	bool m_HasAnchor = false;
	bool m_FlipHorizontal = false;
	bool m_FlipVertical = false;
	int m_Price = 0;
	bool m_Valid = false;
	EInvalidReason m_Reason = INVALID_OCCUPIED;
	float m_MinimumDistance = 48.0f;
};

inline int WheelSector(vec2 Direction, float DeadZone, int NumSectors = 9)
{
	if(NumSectors <= 0 || length(Direction) < DeadZone)
		return -1;
	float Angle = atan2f(Direction.y, Direction.x) + pi / 2.0f + pi / NumSectors;
	while(Angle < 0.0f)
		Angle += 2.0f * pi;
	while(Angle >= 2.0f * pi)
		Angle -= 2.0f * pi;
	return clamp((int)(Angle / (2.0f * pi) * NumSectors), 0, NumSectors - 1);
}

class CStateMachine
{
	EState m_State = STATE_IDLE;
	EState m_StateBeforeWheel = STATE_IDLE;
	int m_Selected = -1;

  public:
	EState State() const { return m_State; }
	int Selected() const { return m_Selected; }

	void OpenWheel()
	{
		if(m_State == STATE_WHEEL)
			return;
		m_StateBeforeWheel = m_State;
		m_State = STATE_WHEEL;
	}

	bool ReleaseWheel(int Hovered, bool Affordable)
	{
		if(m_State != STATE_WHEEL)
			return false;
		if(Hovered < 0)
		{
			m_State = m_StateBeforeWheel;
			return false;
		}
		if(!Affordable)
		{
			m_State = m_StateBeforeWheel;
			return false;
		}
		m_Selected = Hovered;
		m_State = STATE_PLACEMENT;
		return true;
	}

	void Cancel()
	{
		m_State = STATE_IDLE;
		m_StateBeforeWheel = STATE_IDLE;
		m_Selected = -1;
	}
};

class CPlacementTrigger
{
	bool m_Down = false;
	bool m_HasGrid = false;
	int m_GridX = 0;
	int m_GridY = 0;

  public:
	void SetDown(bool Down)
	{
		m_Down = Down;
		if(!Down)
			m_HasGrid = false;
	}

	bool ShouldSend(bool Continuous, bool PressEdge, int GridX, int GridY)
	{
		if(!Continuous)
			return PressEdge;
		if(!m_Down)
			return false;
		if(m_HasGrid && m_GridX == GridX && m_GridY == GridY)
			return false;
		m_HasGrid = true;
		m_GridX = GridX;
		m_GridY = GridY;
		return true;
	}
};
} // namespace BuildPlacementLogic

#endif
