#ifndef GAME_SERVER_ENTITIES_BOSS_BODY_H
#define GAME_SERVER_ENTITIES_BOSS_BODY_H

#include <base/vmath.h>

class CCollision;

// Full-silhouette body for the walking v5 bosses (Matriarch, Strider, Monolith) and the Seraph.
//
// The collision box IS the drawn silhouette: from the top of the art to the soles of the feet,
// symmetric left/right so turning around never pushes the art into a wall. The boss stands on
// that box under gravity like a Crawler (no hover spring), so:
//  - nothing it draws can be inside a wall, a ceiling or the floor;
//  - it cannot hang in the air on a probe ray: if the box is not touching ground, it falls.
//
// Shape is given relative to the body origin (m_Pos, the art origin): HalfW, Top (negative, above),
// Bottom (positive, the soles). Crouch lowers the body between its legs (feet stay put) and shrinks
// the box from the top, up to MaxCrouch px, so it can squeeze under low ceilings.
struct CBossBodyShape
{
	float m_HalfW;
	float m_Top;
	float m_Bottom;
	float m_MaxCrouch;
};

class CBossBody
{
  public:
	CBossBody();
	void Init(const CBossBodyShape &Shape, float StepUp);
	// Switch shape (Monolith air/ground form). Returns false (and keeps the old one) if it does not fit.
	bool SetShape(CCollision *pCollision, vec2 *pPos, const CBossBodyShape &Shape);

	vec2 Center(vec2 Pos, float Crouch) const;
	vec2 Size(float Crouch) const;
	vec2 Center(vec2 Pos) const { return Center(Pos, m_Crouch); }
	vec2 Size() const { return Size(m_Crouch); }
	float Feet(vec2 Pos) const { return Pos.y + m_Shape.m_Bottom - m_Crouch; } // world y of the soles
	float HalfW() const { return m_Shape.m_HalfW; }

	// Box free at Pos? Down = one-way platforms do not block the bottom edge.
	bool Fits(CCollision *pCollision, vec2 Pos, float Crouch, bool Down) const;
	bool Fits(CCollision *pCollision, vec2 Pos, bool Down = false) const { return Fits(pCollision, Pos, m_Crouch, Down); }

	// Push the body out of solid ground (spawn, shape change, map edits). Tries up first, then sideways.
	void Unstick(CCollision *pCollision, vec2 *pPos);

	enum
	{
		MOVE_BLOCKED_X = 1,
		MOVE_HIT_CEILING = 2,
		MOVE_LANDED = 4,
		MOVE_TIPPING = 8,
	};
	// Moves the body by Vel with full-box collision. Walk = step up slopes/steps and stick to the
	// ground going downhill (off while leaping/flying). Updates m_Grounded/m_OnPlatform/m_Crouch.
	int Move(CCollision *pCollision, vec2 *pPos, vec2 *pVel, bool Walk);

	// Drop through the one-way platform under the feet for a moment.
	void DropThrough(int Ticks = 12) { m_DropTicks = Ticks; }
	bool Dropping() const { return m_DropTicks > 0; }

	// Distance from the soles down to whatever the box would land on (box sweep), up to MaxDist.
	float FloorGap(CCollision *pCollision, vec2 Pos, float MaxDist) const;
	// Obstacle in front: height (px above the soles) of the lowest spot ahead the body can stand on,
	// within MaxUp. Returns -1 if the way is open at floor level, -2 if it is a wall too tall to jump.
	float LedgeAhead(CCollision *pCollision, vec2 Pos, int Dir, float MaxUp) const;
	// Ground continues ahead (within MaxDrop below the soles) for a body-width step?
	bool FloorAhead(CCollision *pCollision, vec2 Pos, int Dir, float MaxDrop) const;
	// Room above to stand up fully?
	bool CanStand(CCollision *pCollision, vec2 Pos) const;

	CBossBodyShape m_Shape;
	float m_StepUp;
	float m_Crouch;
	bool m_Grounded;
	bool m_OnPlatform; // standing only on one-way platforms (can drop through)
	int m_DropTicks;
	int m_AirTicks; // ticks since the box last touched ground
};

#endif
