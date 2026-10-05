#ifndef GAME_CLIENT_COMPONENTS_MATRIARCH_RIG_H
#define GAME_CLIENT_COMPONENTS_MATRIARCH_RIG_H

#include <base/vmath.h>

// Client-side procedural rig for the Skitter Matriarch: six IK legs on a tripod gait with
// raycast footholds (floor, wall, ceiling), body tilt from the feet, act poses.
class CMatriarchRig
{
  public:
	struct CInput
	{
		vec2 m_Pos;		// interpolated body center
		vec2 m_Vel;		// px per tick
		int m_Dir;		// +1 faces right
		int m_Act;		// MATRIARCH_ACT_*
		float m_Time;	// seconds since the act started
		bool m_Exposed; // sac open (BossStatus.ArmOut)
		vec2 m_Aim;		// BossStatus.ArmX/ArmY
		int m_Phase;
		bool m_FangsBroken, m_LegsBroken, m_SacBroken;
		float m_Hurt; // 0..1 flash
	};

	CMatriarchRig();
	void Reset();
	// Collision is the client CCollision; Dt in seconds.
	void Update(class CCollision *pCollision, const CInput &In, float Dt);
	void Render(class CRenderTools *pRender, class IGraphics *pGraphics, int Atlas, const CInput &In);

	vec2 HeadPos() const;
	vec2 SacPos() const;
	vec2 MouthPos() const;
	vec2 FootPos(int i) const { return m_aFoot[i]; }
	bool Initialized() const { return m_Init; }
	int m_ItemID;
	int m_LastTick;
	int m_Landed; // footfall dust owed to the renderer

  private:
	vec2 ToWorld(float x, float y) const;
	bool FindFoothold(class CCollision *pCollision, int Leg, vec2 *pOut) const;
	void PoseFoot(int Leg, vec2 Local, float Rate, float Dt);

	bool m_Init;
	vec2 m_Pos;
	vec2 m_Body; // m_Pos plus bob/crouch offset
	int m_Dir;
	float m_Turn; // -1..1, eases through 0 when the facing flips
	float m_Settle;
	float m_Tilt;
	float m_Pitch;
	float m_Crouch;
	float m_Bob;
	float m_Shake;
	float m_SacAngle, m_SacVel;
	float m_SacSwell;
	float m_Mandible;
	float m_Time;
	vec2 m_aFoot[6];
	vec2 m_aFrom[6];
	vec2 m_aTo[6];
	float m_aStep[6]; // <0 planted, 0..1 stepping
	bool m_aPlanted[6];
	bool m_Airborne;
};

#endif
