#include <base/math.h>
#include <game/collision.h>

#include "boss_body.h"

CBossBody::CBossBody()
{
	CBossBodyShape s = {32.0f, -32.0f, 32.0f, 0.0f};
	Init(s, 0.0f);
}

void CBossBody::Init(const CBossBodyShape &Shape, float StepUp)
{
	m_Shape = Shape;
	m_StepUp = StepUp;
	m_Crouch = 0.0f;
	m_Grounded = false;
	m_OnPlatform = false;
	m_DropTicks = 0;
	m_AirTicks = 0;
}

vec2 CBossBody::Center(vec2 Pos, float Crouch) const
{
	// Crouching lowers the body origin while the soles stay put: the box keeps its top at
	// Pos + Top and ends at the soles, Pos + Bottom - Crouch.
	return vec2(Pos.x, Pos.y + (m_Shape.m_Top + m_Shape.m_Bottom - Crouch) * 0.5f);
}

vec2 CBossBody::Size(float Crouch) const
{
	return vec2(m_Shape.m_HalfW * 2.0f, m_Shape.m_Bottom - Crouch - m_Shape.m_Top);
}

bool CBossBody::Fits(CCollision *pCollision, vec2 Pos, float Crouch, bool Down) const
{
	return pCollision->TestBox(Center(Pos, Crouch), Size(Crouch), Down) == 0;
}

void CBossBody::Unstick(CCollision *pCollision, vec2 *pPos)
{
	if(Fits(pCollision, *pPos, true))
		return;
	// Crouch first (cheap and keeps the feet where they are).
	for(float c = m_Crouch + 4.0f; c <= m_Shape.m_MaxCrouch; c += 4.0f)
	{
		const vec2 p = *pPos + vec2(0, c - m_Crouch);
		if(Fits(pCollision, p, c, true))
		{
			*pPos = p;
			m_Crouch = c;
			return;
		}
	}
	for(int d = 4; d <= 256; d += 4)
	{
		const vec2 aTry[5] = {vec2(0, -d), vec2(d, 0), vec2(-d, 0), vec2(d, -d), vec2(-d, -d)};
		for(int k = 0; k < 5; k++)
			if(Fits(pCollision, *pPos + aTry[k], true))
			{
				*pPos += aTry[k];
				return;
			}
	}
}

bool CBossBody::SetShape(CCollision *pCollision, vec2 *pPos, const CBossBodyShape &Shape)
{
	const CBossBodyShape Old = m_Shape;
	const float OldCrouch = m_Crouch;
	// Keep the soles where they are when the shape changes.
	const float Feet = this->Feet(*pPos);
	m_Shape = Shape;
	m_Crouch = 0.0f;
	vec2 p(pPos->x, Feet - Shape.m_Bottom);
	for(int up = 0; up <= 96; up += 8)
		if(Fits(pCollision, p - vec2(0, up), true))
		{
			*pPos = p - vec2(0, up);
			return true;
		}
	m_Shape = Old;
	m_Crouch = OldCrouch;
	return false;
}

int CBossBody::Move(CCollision *pCollision, vec2 *pPos, vec2 *pVel, bool Walk)
{
	int Result = 0;
	vec2 Pos = *pPos;
	vec2 Vel = *pVel;
	const bool WasGrounded = m_Grounded;
	bool Down = m_DropTicks > 0 || Vel.y < 0.0f;
	// Overlapping a one-way platform (just dropped / jumped through it): let it pass.
	if(!Down && !Fits(pCollision, Pos, false) && Fits(pCollision, Pos, true))
		Down = true;
	if(!Fits(pCollision, Pos, Down))
		Unstick(pCollision, &Pos);

	const float Sub = 6.0f;

	// ---- horizontal: slide, step up slopes/steps, crouch under low ceilings ----
	if(fabsf(Vel.x) > 0.0001f)
	{
		const int n = (int)ceilf(fabsf(Vel.x) / Sub);
		const float Step = Vel.x / n;
		for(int i = 0; i < n; i++)
		{
			if(Fits(pCollision, Pos + vec2(Step, 0), Down))
			{
				Pos.x += Step;
				continue;
			}
			bool Ok = false;
			if(Walk && m_StepUp > 0.0f && (WasGrounded || m_AirTicks < 6))
				for(float Lift = 2.0f; Lift <= m_StepUp; Lift += 2.0f)
					if(Fits(pCollision, Pos + vec2(Step, -Lift), Down))
					{
						Pos += vec2(Step, -Lift);
						Ok = true;
						break;
					}
			if(!Ok && m_Crouch < m_Shape.m_MaxCrouch)
				for(float c = m_Crouch + 4.0f; c <= m_Shape.m_MaxCrouch; c += 4.0f)
				{
					// Lower the body (soles fixed) and try the step again, also with a small lift.
					const vec2 Low = Pos + vec2(0, c - m_Crouch);
					if(Fits(pCollision, Low + vec2(Step, 0), c, Down))
					{
						Pos = Low + vec2(Step, 0);
						m_Crouch = c;
						Ok = true;
						break;
					}
				}
			if(!Ok)
			{
				Result |= MOVE_BLOCKED_X;
				Vel.x = 0.0f;
				break;
			}
		}
	}

	// ---- vertical ----
	m_Grounded = false;
	if(fabsf(Vel.y) > 0.0001f)
	{
		const int n = (int)ceilf(fabsf(Vel.y) / Sub);
		const float Step = Vel.y / n;
		for(int i = 0; i < n; i++)
		{
			const bool D = m_DropTicks > 0 || Step < 0.0f || Down;
			if(Fits(pCollision, Pos + vec2(0, Step), D))
			{
				Pos.y += Step;
				continue;
			}
			// Find the exact contact pixel.
			const float Dir = Step > 0.0f ? 1.0f : -1.0f;
			for(int k = 0; k < 8 && Fits(pCollision, Pos + vec2(0, Dir), D); k++)
				Pos.y += Dir;
			if(Step > 0.0f)
			{
				m_Grounded = true;
				if(!WasGrounded)
					Result |= MOVE_LANDED;
			}
			else
				Result |= MOVE_HIT_CEILING;
			Vel.y = 0.0f;
			break;
		}
	}

	const bool DownNow = m_DropTicks > 0;
	// ---- walking downhill / off a small step: stay on the ground instead of launching ----
	if(!m_Grounded && Walk && WasGrounded && Vel.y >= 0.0f && !DownNow)
	{
		for(float d = 2.0f; d <= m_StepUp + 6.0f; d += 2.0f)
			if(!Fits(pCollision, Pos + vec2(0, d), false))
			{
				Pos.y += d - 2.0f;
				for(int k = 0; k < 2 && Fits(pCollision, Pos + vec2(0, 1), false); k++)
					Pos.y += 1.0f;
				m_Grounded = true;
				Vel.y = 0.0f;
				break;
			}
	}
	if(!m_Grounded && Vel.y >= 0.0f && !DownNow && !Fits(pCollision, Pos + vec2(0, 1), false))
		m_Grounded = true;

	// ---- balance: perched on a corner over a pit -> tip off ----
	// A long box on a 45-degree ramp also rests on one corner, so only tip when the ground under the
	// middle is deeper than a 45-degree slope from the contact point would put it (a real gap).
	if(m_Grounded && Walk)
	{
		const float Feet = Pos.y + m_Shape.m_Bottom - m_Crouch;
		const float Step = m_Shape.m_HalfW * 0.25f;
		float aDepth[9];
		int Nearest = 99;
		for(int k = -4; k <= 4; k++)
		{
			const float x = Pos.x + k * Step;
			vec2 At;
			aDepth[k + 4] = pCollision->IntersectLine(vec2(x, Feet - 2.0f), vec2(x, Feet + 400.0f), &At, 0, false, !m_DropTicks) ?
				At.y - Feet : 1e9f;
			if(aDepth[k + 4] <= 8.0f && abs(k) < abs(Nearest))
				Nearest = k;
		}
		if(Nearest != 99 && abs(Nearest) >= 3 && aDepth[4] > abs(Nearest) * Step + 24.0f)
		{
			const float Push = Nearest < 0 ? 4.0f : -4.0f; // slide toward the open side
			if(Fits(pCollision, Pos + vec2(Push, 0), false))
			{
				Pos.x += Push;
				Vel.x += Push * 0.25f;
				Result |= MOVE_TIPPING;
			}
		}
	}

	// ---- stand back up when there is room ----
	if(m_Crouch > 0.0f)
	{
		const float c = max(0.0f, m_Crouch - 3.0f);
		const vec2 High = Pos - vec2(0, m_Crouch - c);
		if(Fits(pCollision, High, c, true))
		{
			Pos = High;
			m_Crouch = c;
		}
	}

	m_OnPlatform = m_Grounded && Fits(pCollision, Pos + vec2(0, 2), true);
	if(m_DropTicks > 0)
		m_DropTicks--;
	m_AirTicks = m_Grounded ? 0 : m_AirTicks + 1;

	*pPos = Pos;
	*pVel = Vel;
	return Result;
}

float CBossBody::FloorGap(CCollision *pCollision, vec2 Pos, float MaxDist) const
{
	for(float d = 0.0f; d <= MaxDist; d += 8.0f)
		if(!Fits(pCollision, Pos + vec2(0, d + 1.0f), false))
			return d;
	return MaxDist + 1.0f;
}

float CBossBody::LedgeAhead(CCollision *pCollision, vec2 Pos, int Dir, float MaxUp) const
{
	const vec2 Ahead = Pos + vec2(Dir * 24.0f, 0);
	if(Fits(pCollision, Ahead, false))
		return -1.0f;
	for(float Up = 8.0f; Up <= MaxUp; Up += 8.0f)
		if(Fits(pCollision, Ahead - vec2(0, Up), false) && Fits(pCollision, Pos - vec2(0, Up), true))
			return Up;
	return -2.0f;
}

bool CBossBody::FloorAhead(CCollision *pCollision, vec2 Pos, int Dir, float MaxDrop) const
{
	const float Feet = this->Feet(Pos);
	// Sample under the leading half of the box one body-width ahead.
	for(int k = 0; k <= 2; k++)
	{
		const float x = Pos.x + Dir * (m_Shape.m_HalfW * (0.5f + 0.5f * k) + 24.0f);
		if(pCollision->IntersectLine(vec2(x, Feet - 8.0f), vec2(x, Feet + MaxDrop), 0, 0, false, true))
			return true;
	}
	return false;
}

bool CBossBody::CanStand(CCollision *pCollision, vec2 Pos) const
{
	return Fits(pCollision, Pos - vec2(0, m_Crouch), 0.0f, true);
}
