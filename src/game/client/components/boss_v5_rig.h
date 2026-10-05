#ifndef GAME_CLIENT_COMPONENTS_BOSS_V5_RIG_H
#define GAME_CLIENT_COMPONENTS_BOSS_V5_RIG_H

#include <base/vmath.h>

// Client-side procedural rigs for the v5 bosses (Bastion Strider, Storm Seraph, Siege Monolith).
// The painted parts are drawn one by one; legs/struts are two-joint bones fitted between
// measured joint pixels on the art (BossV5DrawBone), so the art never stretches off its joints.

class CCollision;
class CRenderTools;

struct CBossV5RigInput
{
	vec2 m_Pos;	  // interpolated body center
	vec2 m_Vel;	  // px per tick
	int m_Dir;	  // +1 faces right
	int m_Act;	  // per-boss act
	float m_Time; // seconds since the act started
	int m_Flags;  // BOSSV5_FLAG_* (droid snapshot angle)
	vec2 m_Aim;	  // BossStatus.ArmX/ArmY
	int m_Phase;
	bool m_aBroken[4]; // part 1..3 broken (index 0 unused)
	float m_Hurt;	   // 0..1 flash
};

// Draws an atlas sprite so that its art joints (J0, J1 in source pixels of an AW x AH image)
// land on world points A and B. Mirror flips the art horizontally (joints are mirrored too).
void BossV5DrawBone(CRenderTools *pRender, int Atlas, const char *pName, vec2 A, vec2 B, float AW, float AH, float J0x, float J0y,
	float J1x, float J1y, bool MirrorX, bool MirrorY, vec4 Color);

// Draws a sprite rotated about a pivot given as a fraction of the sprite (facing right art).
void BossV5DrawPivot(CRenderTools *pRender, int Atlas, const char *pName, vec2 Pivot, float W, float H, float Px, float Py, float Angle,
	int Dir, vec4 Color);

// Crawler-style footing shared by every walking v5 boss (Matriarch, Strider, Monolith).
// The Crawler never shows a floating leg because its feet are points pushed INTO the ground
// and stopped by collision: whatever surface is there, the foot ends on it. These helpers give
// the IK legs the same guarantee:
//  - BossV5Foothold finds real ground within the leg's reach. It prefers the rest spot, then
//    spots slid in toward the hip, then a bit past it, then any surface the leg points at
//    (step faces, walls). It fails only when there is truly nothing within reach.
//  - BossV5PressFoot keeps a planted foot on the ground: if the floor under it vanished (the body
//    walked over a ledge or onto a slope), the foot slides down onto the new surface. It returns
//    false when nothing is below within reach, so the caller must re-step.
bool BossV5Foothold(CCollision *pCollision, vec2 Hip, vec2 Want, float Reach, vec2 *pOut);
bool BossV5PressFoot(CCollision *pCollision, vec2 Hip, float Reach, vec2 *pFoot);
// How far (px, >= 0) the body has to come down so that every foot is within reach of its hip.
float BossV5LegSag(const vec2 *pHip, const vec2 *pFoot, const bool *pUse, int Num, float Reach);

// Jagged lightning / beam quads (no texture).
void BossV5DrawBolt(class IGraphics *pGraphics, vec2 From, vec2 To, float Width, vec4 Color, int Seed, float Jitter);

class CBossV5RigBase
{
  public:
	CBossV5RigBase() { ResetBase(); }
	virtual ~CBossV5RigBase() {}
	void ResetBase();
	int m_ItemID;
	int m_LastTick;
	int m_Landed; // footfall dust owed to the renderer
	bool m_Init;

  protected:
	vec2 ToWorld(float x, float y) const;
	vec2 m_Pos;
	vec2 m_Body;
	int m_Dir;
	float m_Turn; // -1..1, eases through 0 when the facing flips
	float m_Tilt;
	float m_Time;
	float m_Shake;
};

class CStriderRig : public CBossV5RigBase
{
  public:
	CStriderRig() { Reset(); }
	void Reset();
	void Update(CCollision *pCollision, const CBossV5RigInput &In, float Dt);
	void Render(CRenderTools *pRender, int Atlas, const CBossV5RigInput &In);
	vec2 FootPos(int i) const { return m_aFoot[i]; }
	vec2 ThrusterPos() const;
	vec2 CorePos() const { return ToWorld(10.0f, -10.0f); }
	vec2 MortarPos() const;
	vec2 ShieldPos() const;

  private:
	bool FindFoothold(CCollision *pCollision, int Leg, vec2 *pOut) const;
	float m_Pitch, m_Crouch, m_Bob;
	float m_Settle; // body sinks toward the feet when the server keeps it high for clearance
	float m_MortarAngle, m_MortarKick;
	float m_ShieldPush;
	vec2 m_aFoot[4], m_aFrom[4], m_aTo[4];
	float m_aStep[4];
	bool m_aPlanted[4];
	bool m_Airborne;
};

class CSeraphRig : public CBossV5RigBase
{
  public:
	CSeraphRig() { Reset(); }
	void Reset();
	void Update(CCollision *pCollision, const CBossV5RigInput &In, float Dt);
	void Render(CRenderTools *pRender, int Atlas, const CBossV5RigInput &In);
	vec2 CorePos() const;
	vec2 HaloPos() const;
	vec2 ThrusterPos() const;

  private:
	float m_Flap, m_FlapSpeed, m_Fold, m_Spread;
	float m_Bank, m_Spin;
	float m_HaloSpin;
	float m_Droop;
};

class CMonolithRig : public CBossV5RigBase
{
  public:
	CMonolithRig() { Reset(); }
	void Reset();
	void Update(CCollision *pCollision, const CBossV5RigInput &In, float Dt);
	void Render(CRenderTools *pRender, int Atlas, const CBossV5RigInput &In);
	vec2 Muzzle() const;
	vec2 ThrusterPos() const;
	vec2 VentPos(int i) const;
	float TurretAngle() const { return m_TurretAngle; } // facing space

  private:
	float m_TurretAngle;
	float m_Recoil;
	float m_Retract; // 0 struts down .. 1 tucked (air form)
	float m_VentOpen;
	float m_Sway;
	vec2 m_aFoot[3];
	bool m_aFootOk[3];
	float m_Settle;
};

#endif
