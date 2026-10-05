#include <base/math.h>
#include <engine/graphics.h>
#include <game/collision.h>
#include <game/client/render.h>
#include <game/foundry_warden.h> // WardenSolveLeg
#include <game/bastion_strider.h>
#include <game/storm_seraph.h>
#include <game/siege_monolith.h>

#include "boss_v5_rig.h"

static float Approach(float Cur, float Want, float Rate, float Dt)
{
	const float k = 1.0f - expf(-Rate * Dt);
	return Cur + (Want - Cur) * k;
}

static vec2 Approach(vec2 Cur, vec2 Want, float Rate, float Dt)
{
	const float k = 1.0f - expf(-Rate * Dt);
	return Cur + (Want - Cur) * k;
}

static vec2 Rotate(vec2 v, float a)
{
	const float c = cosf(a), s = sinf(a);
	return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static vec4 Shade(vec4 c, float k) { return vec4(c.r * k, c.g * k, c.b * k, c.a); }

// ================================================================ Crawler-style footing

bool BossV5Foothold(CCollision *pCollision, vec2 Hip, vec2 Want, float Reach, vec2 *pOut)
{
	// Columns from the rest spot in toward the hip, then slightly past the rest spot.
	static const float s_aSlide[6] = {0.0f, 0.25f, 0.5f, 0.75f, -0.18f, 0.92f};
	const float Top = min(Hip.y, Want.y) - 12.0f;
	for(int k = 0; k < 6; k++)
	{
		const float x = mix(Want.x, Hip.x, s_aSlide[k]);
		const vec2 From(x, Top);
		if(pCollision->CheckPoint(From.x, From.y))
			continue; // inside a wall at hip height: no foot goes there
		if(pCollision->IntersectLine(vec2(Hip.x, Top), From, 0, 0))
			continue; // a wall between the hip and this column
		vec2 At;
		if(!pCollision->IntersectLine(From, vec2(x, Hip.y + Reach), &At, 0, false, true))
			continue;
		if(distance(At, Hip) > Reach)
			continue;
		*pOut = At + vec2(0, -2.0f);
		return true;
	}
	// Anything the leg points at: a step face, a wall, a ledge lip.
	vec2 Dir = Want - Hip;
	const float Len = length(Dir);
	Dir = Len > 0.01f ? Dir / Len : vec2(0, 1);
	vec2 At;
	if(pCollision->IntersectLine(Hip, Hip + Dir * Reach, &At, 0, false, true))
	{
		*pOut = At - Dir * 2.0f;
		return true;
	}
	return false;
}

bool BossV5PressFoot(CCollision *pCollision, vec2 Hip, float Reach, vec2 *pFoot)
{
	const vec2 Foot = *pFoot;
	// Still standing on something (floor below, or gripping a wall beside it).
	if(pCollision->CheckPoint(Foot.x, Foot.y + 6.0f) || pCollision->CheckPoint(Foot.x - 6.0f, Foot.y) ||
		pCollision->CheckPoint(Foot.x + 6.0f, Foot.y))
		return true;
	// Floor fell away: slide straight down onto the new surface, like a collision-pushed point.
	vec2 At;
	if(pCollision->IntersectLine(Foot, Foot + vec2(0, 120.0f), &At, 0, false, true) && distance(At, Hip) <= Reach)
	{
		*pFoot = At + vec2(0, -2.0f);
		return true;
	}
	return false;
}

float BossV5LegSag(const vec2 *pHip, const vec2 *pFoot, const bool *pUse, int Num, float Reach)
{
	float Sag = 0.0f;
	for(int i = 0; i < Num; i++)
	{
		if(!pUse[i])
			continue;
		const vec2 d = pFoot[i] - pHip[i];
		if(d.y <= 0.0f || length(d) <= Reach)
			continue;
		// Lowering the hip by s gives |(dx, dy - s)| = Reach.
		const float Need = d.y - sqrtf(max(0.0f, Reach * Reach - d.x * d.x));
		Sag = max(Sag, Need);
	}
	return Sag;
}

// ---------------------------------------------------------------- shared drawing

void BossV5DrawBone(CRenderTools *pRender, int Atlas, const char *pName, vec2 A, vec2 B, float AW, float AH, float J0x, float J0y,
	float J1x, float J1y, bool MirrorX, bool MirrorY, vec4 Color)
{
	if(MirrorX)
	{
		J0x = AW - J0x;
		J1x = AW - J1x;
	}
	if(MirrorY)
	{
		J0y = AH - J0y;
		J1y = AH - J1y;
	}
	const vec2 J0(J0x, J0y), Dj = vec2(J1x, J1y) - J0;
	const vec2 D = B - A;
	const float Lj = length(Dj), Ld = length(D);
	if(Lj < 1.0f || Ld < 0.5f)
		return;
	const float Scale = Ld / Lj;
	const float Rot = atan2f(D.y, D.x) - atan2f(Dj.y, Dj.x);
	const vec2 Center = A + Rotate((vec2(AW * 0.5f, AH * 0.5f) - J0) * Scale, Rot);
	pRender->RenderAtlasSpriteEx(Atlas, pName, Center, vec2(AW * Scale, AH * Scale), Rot, MirrorX, MirrorY, Color);
}

void BossV5DrawPivot(CRenderTools *pRender, int Atlas, const char *pName, vec2 Pivot, float W, float H, float Px, float Py, float Angle,
	int Dir, vec4 Color)
{
	vec2 Off((0.5f - Px) * W, (0.5f - Py) * H);
	float World = Angle;
	if(Dir < 0)
	{
		Off.x = -Off.x;
		World = -Angle;
	}
	pRender->RenderAtlasSpriteEx(Atlas, pName, Pivot + Rotate(Off, World), vec2(W, H), World, Dir < 0, false, Color);
}

void BossV5DrawBolt(IGraphics *pGraphics, vec2 From, vec2 To, float Width, vec4 Color, int Seed, float Jitter)
{
	const vec2 D = To - From;
	const float Len = length(D);
	if(Len < 1.0f)
		return;
	const vec2 Dir = D / Len;
	const vec2 N(-Dir.y, Dir.x);
	const int Segs = clamp((int)(Len / 22.0f), 1, 48);
	vec2 aP[49];
	unsigned s = (unsigned)Seed * 2654435761u + 12345u;
	for(int i = 0; i <= Segs; i++)
	{
		s = s * 1103515245u + 12345u;
		const float r = ((s >> 8) & 0xffff) / 65535.0f - 0.5f;
		const float Edge = (i == 0 || i == Segs) ? 0.0f : 1.0f;
		aP[i] = From + D * (i / (float)Segs) + N * r * Jitter * Edge;
	}
	pGraphics->TextureClear();
	pGraphics->BlendAdditive();
	pGraphics->QuadsBegin();
	for(int Pass = 0; Pass < 2; Pass++)
	{
		const float w = Pass == 0 ? Width * 2.4f : Width * 0.6f;
		if(Pass == 0)
			pGraphics->SetColor(Color.r * 0.5f, Color.g * 0.5f, Color.b * 0.5f, Color.a * 0.45f);
		else
			pGraphics->SetColor(0.6f + Color.r * 0.4f, 0.6f + Color.g * 0.4f, 0.6f + Color.b * 0.4f, Color.a);
		for(int i = 0; i < Segs; i++)
		{
			const vec2 a = aP[i], b = aP[i + 1];
			vec2 d = b - a;
			const float l = length(d);
			if(l < 0.01f)
				continue;
			const vec2 n = vec2(-d.y, d.x) / l * (w * 0.5f);
			IGraphics::CFreeformItem Q(a.x + n.x, a.y + n.y, a.x - n.x, a.y - n.y, b.x + n.x, b.y + n.y, b.x - n.x, b.y - n.y);
			pGraphics->QuadsDrawFreeform(&Q, 1);
		}
	}
	pGraphics->QuadsEnd();
	pGraphics->BlendNormal();
}

// ---------------------------------------------------------------- base

void CBossV5RigBase::ResetBase()
{
	m_ItemID = -1;
	m_LastTick = 0;
	m_Landed = 0;
	m_Init = false;
	m_Pos = m_Body = vec2(0, 0);
	m_Dir = 1;
	m_Turn = 1.0f;
	m_Tilt = 0.0f;
	m_Time = 0.0f;
	m_Shake = 0.0f;
}

vec2 CBossV5RigBase::ToWorld(float x, float y) const
{
	return m_Body + Rotate(vec2(x * m_Turn, y), m_Tilt);
}

// ================================================================ Bastion Strider

void CStriderRig::Reset()
{
	ResetBase();
	m_Pitch = m_Crouch = m_Bob = 0.0f;
	m_Settle = 0.0f;
	m_MortarAngle = m_MortarKick = 0.0f;
	m_ShieldPush = 0.0f;
	m_Airborne = false;
	for(int i = 0; i < 4; i++)
	{
		m_aFoot[i] = m_aFrom[i] = m_aTo[i] = vec2(0, 0);
		m_aStep[i] = -1.0f;
		m_aPlanted[i] = false;
	}
}

vec2 CStriderRig::ThrusterPos() const { return ToWorld(s_StriderThruster.m_X - 34.0f, s_StriderThruster.m_Y); }
vec2 CStriderRig::MortarPos() const { return ToWorld(s_StriderMortar.m_X + 30.0f, s_StriderMortar.m_Y - 40.0f); }
vec2 CStriderRig::ShieldPos() const { return ToWorld(s_StriderShield.m_X + m_ShieldPush, s_StriderShield.m_Y); }

bool CStriderRig::FindFoothold(CCollision *pCollision, int Leg, vec2 *pOut) const
{
	// Aim the foot into the ground below its rest spot and take whatever real surface the leg can
	// reach (Crawler rule: a foot always ends on a surface, never in the air).
	const vec2 Hip = ToWorld(s_aStriderHipX[Leg], s_aStriderHipY[Leg]);
	const vec2 Want = m_Pos + vec2(s_aStriderFootX[Leg] * m_Dir, STRIDER_HOVER + 20.0f);
	return BossV5Foothold(pCollision, Hip, Want, StriderLegReach(), pOut);
}

void CStriderRig::Update(CCollision *pCollision, const CBossV5RigInput &In, float Dt)
{
	if(Dt <= 0.0f)
		return;
	Dt = min(Dt, 0.05f);
	m_Time += Dt;
	m_Pos = In.m_Pos;
	if(!m_Init)
	{
		m_Dir = In.m_Dir;
		m_Turn = (float)m_Dir;
		m_Body = m_Pos;
		for(int i = 0; i < 4; i++)
		{
			vec2 F;
			m_aFoot[i] = FindFoothold(pCollision, i, &F) ? F : m_Pos + vec2(s_aStriderFootX[i] * m_Dir, STRIDER_HOVER);
			m_aPlanted[i] = true;
		}
		m_Init = true;
	}
	m_Dir = In.m_Dir;
	m_Turn = Approach(m_Turn, (float)m_Dir, In.m_Act == STRIDER_ACT_TURN ? 9.0f : 16.0f, Dt);
	if(fabsf(m_Turn) < 0.08f)
		m_Turn = m_Turn < 0.0f ? -0.08f : 0.08f;

	const int Act = In.m_Act;
	const float t = In.m_Time;
	const float Speed = length(In.m_Vel);

	vec2 GroundAt;
	const bool Ground = pCollision->IntersectLine(m_Pos, m_Pos + vec2(0, STRIDER_HOVER + 60.0f), &GroundAt, 0, false, true);
	const bool WasAir = m_Airborne;
	m_Airborne = !Ground && Act != STRIDER_ACT_DEATH && fabsf(In.m_Vel.y) > 1.0f;
	if(WasAir && !m_Airborne)
	{
		m_Landed += 4;
		m_Shake = 1.0f;
		for(int i = 0; i < 4; i++)
		{
			vec2 F;
			if(FindFoothold(pCollision, i, &F))
				m_aFoot[i] = F;
			m_aPlanted[i] = true;
			m_aStep[i] = -1.0f;
		}
	}

	// ---- act poses (pitch: nose-up positive; crouch pushes the body down) ----
	float WantPitch = 0.0f, WantCrouch = 0.0f, WantPush = 0.0f, WantMortar = -0.15f;
	const float Shot = 1.0f / BOSSV5_TICK_SPEED;
	switch(Act)
	{
	case STRIDER_ACT_BASH:
		if(t < StriderBashTick() * Shot)
		{
			WantPitch = 0.12f;
			WantPush = -14.0f;
			WantCrouch = 8.0f;
		}
		else
		{
			WantPitch = -0.12f;
			WantPush = 34.0f;
		}
		break;
	case STRIDER_ACT_CHARGE:
		WantCrouch = 14.0f;
		WantPitch = -0.1f;
		WantPush = 10.0f;
		break;
	case STRIDER_ACT_STOMP:
		if(t < StriderStompTick() * Shot)
		{
			WantPitch = 0.22f;
			WantCrouch = -18.0f;
		}
		else
		{
			WantCrouch = 22.0f;
			WantPitch = -0.05f;
			if(t < StriderStompTick() * Shot + 0.2f)
				m_Shake = max(m_Shake, 0.8f);
		}
		break;
	case STRIDER_ACT_MORTAR:
	{
		WantCrouch = 10.0f;
		WantPitch = 0.05f;
		// Elevate toward the aim point.
		const vec2 d = In.m_Aim - m_Pos;
		WantMortar = clamp(atan2f(d.y, fabsf(d.x) + 1.0f) * 0.4f, -0.5f, 0.2f) - 0.2f;
		for(int s = 0; s < StriderShellCount(In.m_Phase); s++)
		{
			const float Fire = StriderShellTick(s) * Shot;
			if(t >= Fire && t < Fire + 0.05f)
				m_MortarKick = 1.0f;
		}
		break;
	}
	case STRIDER_ACT_LEAP:
		if(!m_Airborne && t < StriderLeapWindup(In.m_Phase) * Shot)
		{
			WantCrouch = 30.0f;
			WantPitch = 0.1f;
		}
		else if(m_Airborne)
			WantPitch = clamp(-In.m_Vel.y * 0.02f, -0.3f, 0.3f);
		break;
	case STRIDER_ACT_ROAR:
		WantPitch = t < 1.1f ? 0.25f : 0.0f;
		WantPush = 12.0f;
		if(t > 0.45f && t < 1.0f)
			m_Shake = max(m_Shake, 0.6f);
		break;
	case STRIDER_ACT_STAGGER:
		WantCrouch = 24.0f;
		WantPitch = -0.12f;
		m_Shake = max(m_Shake, 0.35f);
		break;
	case STRIDER_ACT_OVERHEAT:
		WantCrouch = 30.0f;
		WantPitch = -0.16f;
		WantPush = -10.0f;
		m_Shake = max(m_Shake, 0.25f);
		break;
	case STRIDER_ACT_DEATH:
		WantCrouch = 40.0f + min(t, 1.0f) * 20.0f;
		WantPitch = -0.18f;
		break;
	default:
		break;
	}
	m_Pitch = Approach(m_Pitch, WantPitch, 9.0f, Dt);
	m_Crouch = Approach(m_Crouch, WantCrouch, 10.0f, Dt);
	m_ShieldPush = Approach(m_ShieldPush, WantPush, 16.0f, Dt);
	m_MortarAngle = Approach(m_MortarAngle, WantMortar, 6.0f, Dt);
	m_MortarKick = max(0.0f, m_MortarKick - Dt * 6.0f);
	m_Shake = max(0.0f, m_Shake - Dt * 2.5f);

	const bool Moving = fabsf(In.m_Vel.x) > 1.0f;
	m_Bob = Approach(m_Bob, Moving ? sinf(m_Time * 11.0f) * 4.0f : sinf(m_Time * 2.0f) * 2.0f, 10.0f, Dt);
	vec2 ShakeOff(0, 0);
	if(m_Shake > 0.0f)
		ShakeOff = vec2(sinf(m_Time * 71.0f), cosf(m_Time * 57.0f)) * 5.0f * m_Shake;
	// Sit down between the feet: on slopes/steps the server holds the body high enough for its box,
	// the art settles toward the average foothold and far enough that every planted leg reaches
	// its foot, but never so far that the hull sinks into the floor under its middle.
	float WantSettle = 0.0f;
	if(!m_Airborne && Act != STRIDER_ACT_DEATH)
	{
		float Sum = 0.0f;
		vec2 aHip[4];
		bool aUse[4];
		for(int i = 0; i < 4; i++)
		{
			Sum += m_aFoot[i].y;
			aHip[i] = m_Pos + (ToWorld(s_aStriderHipX[i], s_aStriderHipY[i]) - m_Body); // hip at the unsettled body
			aUse[i] = m_aPlanted[i];
		}
		WantSettle = max(Sum * 0.25f - STRIDER_HOVER - m_Pos.y, BossV5LegSag(aHip, m_aFoot, aUse, 4, StriderLegReach()));
		const float Floor = Ground ? GroundAt.y : m_Pos.y + STRIDER_HOVER;
		WantSettle = clamp(WantSettle, -12.0f, max(0.0f, Floor - STRIDER_HULL_BOTTOM - 4.0f - m_Pos.y - m_Crouch));
	}
	m_Settle = Approach(m_Settle, WantSettle, 8.0f, Dt);
	m_Body = m_Pos + vec2(0, m_Bob + m_Crouch + m_Settle) + ShakeOff;

	// ---- legs ----
	auto Pose = [&](int i, vec2 Local, float Rate)
	{
		m_aFoot[i] = Approach(m_aFoot[i], ToWorld(Local.x, Local.y), Rate, Dt);
		m_aPlanted[i] = false;
		m_aStep[i] = -1.0f;
	};
	if(m_Airborne)
	{
		const float Brace = In.m_Vel.y > 0.0f ? 1.0f : 0.0f;
		for(int i = 0; i < 4; i++)
		{
			const float Fx = s_aStriderFootX[i];
			Pose(i, vec2(Fx * (0.75f + 0.25f * Brace), 70.0f + Brace * 40.0f), 12.0f);
		}
	}
	else if(Act == STRIDER_ACT_DEATH)
	{
		for(int i = 0; i < 4; i++)
			Pose(i, vec2(s_aStriderFootX[i] * 1.25f, 70.0f), 4.0f);
	}
	else
	{
		bool aGroup[2] = {false, false};
		static const int s_aGait[4] = {0, 1, 1, 0};
		for(int i = 0; i < 4; i++)
			if(m_aStep[i] >= 0.0f)
				aGroup[s_aGait[i]] = true;
		const float StepTime = clamp(0.3f - Speed * 0.015f, 0.14f, 0.3f);
		const float Reach = StriderLegReach();
		for(int i = 0; i < 4; i++)
		{
			// Stomp: the near front leg rears up and slams.
			if(Act == STRIDER_ACT_STOMP && i == 3 && t < StriderStompTick() * Shot)
			{
				Pose(i, vec2(s_aStriderFootX[i] + 30.0f, -20.0f), 10.0f);
				continue;
			}
			vec2 Rest;
			if(!FindFoothold(pCollision, i, &Rest))
			{
				Pose(i, vec2(s_aStriderFootX[i] * 0.8f, STRIDER_HOVER + 8.0f * sinf(m_Time * 7.0f + i)), 8.0f);
				continue;
			}
			const vec2 Hip = ToWorld(s_aStriderHipX[i], s_aStriderHipY[i]);
			// A planted foot whose floor vanished slides down onto the new surface or re-steps.
			if(m_aPlanted[i] && m_aStep[i] < 0.0f && !BossV5PressFoot(pCollision, Hip, Reach, &m_aFoot[i]))
			{
				m_aFrom[i] = m_aFoot[i];
				m_aTo[i] = Rest;
				m_aStep[i] = 0.0f;
				aGroup[s_aGait[i]] = true;
				continue;
			}
			if(m_aStep[i] >= 0.0f)
			{
				m_aStep[i] += Dt / StepTime;
				// Lead the step with the body's motion, but land on real ground.
				vec2 Lead;
				m_aTo[i] = BossV5Foothold(pCollision, Hip, Rest + vec2(In.m_Vel.x * 4.0f, 30.0f), Reach, &Lead) ? Lead : Rest;
				if(m_aStep[i] >= 1.0f)
				{
					m_aStep[i] = -1.0f;
					m_aFoot[i] = m_aTo[i];
					m_aPlanted[i] = true;
					m_Landed++;
					m_Shake = max(m_Shake, 0.12f);
				}
				else
				{
					const float k = m_aStep[i];
					const float e = k * k * (3.0f - 2.0f * k);
					m_aFoot[i] = mix(m_aFrom[i], m_aTo[i], e) + vec2(0, -sinf(k * pi) * STRIDER_STEP_LIFT);
				}
				continue;
			}
			if(!m_aPlanted[i])
			{
				m_aFrom[i] = m_aFoot[i];
				m_aTo[i] = Rest;
				m_aStep[i] = 0.0f;
				continue;
			}
			const float Off = distance(m_aFoot[i], Rest);
			const bool Over = distance(m_aFoot[i], Hip) > Reach + 10.0f;
			const int g = s_aGait[i];
			if(Over || (Off > STRIDER_STEP_DIST && !aGroup[1 - g]))
			{
				m_aFrom[i] = m_aFoot[i];
				m_aTo[i] = Rest;
				m_aStep[i] = 0.0f;
				aGroup[g] = true;
			}
		}
	}

	// ---- body tilt follows the slope under the feet ----
	float WantTilt = 0.0f;
	if(!m_Airborne && Act != STRIDER_ACT_DEATH)
	{
		const vec2 Rear = (m_aFoot[0] + m_aFoot[2]) * 0.5f, Front = (m_aFoot[1] + m_aFoot[3]) * 0.5f;
		const vec2 d = Front - Rear;
		WantTilt = clamp(atan2f(d.y * m_Dir, fabsf(d.x) + 1.0f) * m_Dir, -0.62f, 0.62f);
	}
	m_Tilt = Approach(m_Tilt, WantTilt + m_Pitch * -(float)m_Dir, 7.0f, Dt);
}

void CStriderRig::Render(CRenderTools *pRender, int Atlas, const CBossV5RigInput &In)
{
	if(!m_Init)
		return;
	const bool Flip = m_Turn < 0.0f;
	const float Sx = fabsf(m_Turn);
	const float Hurt = In.m_Hurt;
	const bool Overheat = In.m_Act == STRIDER_ACT_OVERHEAT || (In.m_Flags & BOSSV5_FLAG_EXPOSED);
	const vec4 Lit(1.0f, 1.0f - Hurt * 0.35f, 1.0f - Hurt * 0.35f, 1.0f);
	const vec4 Far(0.52f, 0.52f - Hurt * 0.2f, 0.56f - Hurt * 0.2f, 1.0f);

	auto Part = [&](const char *pName, float x, float y, float w, float h, float Rot, vec4 Color)
	{
		pRender->RenderAtlasSpriteEx(Atlas, pName, ToWorld(x, y), vec2(w * Sx, h), m_Tilt + Rot * (Flip ? -1.0f : 1.0f), Flip, false, Color);
	};

	auto Leg = [&](int i, vec4 Color)
	{
		const bool Front = i == 1 || i == 3;
		const float Bend = Front ? (float)m_Dir : -(float)m_Dir;
		const vec2 Hip = ToWorld(s_aStriderHipX[i], s_aStriderHipY[i]);
		const vec2 Ankle = m_aFoot[i] + vec2(0, -STRIDER_ANKLE);
		float HipA, KneeA;
		WardenSolveLeg(Hip.x, Hip.y, Ankle.x, Ankle.y, STRIDER_THIGH, STRIDER_SHIN, Bend, &HipA, &KneeA);
		const vec2 Knee = Hip + vec2(cosf(HipA), sinf(HipA)) * STRIDER_THIGH;
		// Out of reach (mid-settle, mid-step): the shin piston extends up to 15% so the foot stays on
		// the ground instead of hanging; only beyond that does the foot lift.
		vec2 AnkleReal = Ankle;
		if(distance(Knee, Ankle) > STRIDER_SHIN * 1.15f)
			AnkleReal = Knee + normalize(Ankle - Knee) * STRIDER_SHIN * 1.15f;
		vec4 c = Color;
		if(In.m_aBroken[STRIDER_PART_LEGS])
			c = vec4(c.r * 0.72f, c.g * 0.62f, c.b * 0.55f, 1.0f);
		const bool Mirror = Bend < 0.0f;
		pRender->RenderAtlasSpriteEx(Atlas, "foot", AnkleReal + vec2(8.0f * (Front ? 1.0f : -1.0f) * m_Dir, 14.0f),
			vec2(STRIDER_FOOT_W, STRIDER_FOOT_H), 0.0f, m_Dir < 0, false, c);
		BossV5DrawBone(pRender, Atlas, "shin", Knee, AnkleReal, STRIDER_SHIN_ART_W, STRIDER_SHIN_ART_H, STRIDER_SHIN_J0X, STRIDER_SHIN_J0Y,
			STRIDER_SHIN_J1X, STRIDER_SHIN_J1Y, Mirror, false, c);
		BossV5DrawBone(pRender, Atlas, "thigh", Hip, Knee, STRIDER_THIGH_ART_W, STRIDER_THIGH_ART_H, STRIDER_THIGH_J0X, STRIDER_THIGH_J0Y,
			STRIDER_THIGH_J1X, STRIDER_THIGH_J1Y, Mirror, false, c);
	};

	Leg(0, Far);
	Leg(1, Far);
	{
		const CStriderSprite &s = s_StriderThruster;
		const bool Hot = In.m_Flags & BOSSV5_FLAG_THRUST;
		Part(s.m_pName, s.m_X, s.m_Y, s.m_W, s.m_H, 0.0f, Hot ? vec4(1.3f, 1.1f, 0.9f, 1.0f) : Lit);
	}
	{
		const CStriderSprite &s = s_StriderHull;
		const vec4 HullColor = Overheat ? vec4(1.0f + 0.25f * sinf(m_Time * 16.0f), 0.85f, 0.7f, 1.0f) : Lit;
		Part(s.m_pName, s.m_X, s.m_Y, s.m_W, s.m_H, 0.0f, HullColor);
	}
	if(!In.m_aBroken[STRIDER_PART_MORTAR])
	{
		const CStriderSprite &s = s_StriderMortar;
		const float Kick = m_MortarKick * m_MortarKick;
		const vec2 Pivot = ToWorld(s.m_X - Kick * 8.0f, s.m_Y + s.m_H * 0.38f + Kick * 5.0f);
		BossV5DrawPivot(pRender, Atlas, s.m_pName, Pivot, s.m_W * Sx, s.m_H, 0.45f, 0.88f, m_MortarAngle + m_Tilt * m_Dir - Kick * 0.15f,
			Flip ? -1 : 1, Lit);
	}
	else
		Part("mortar", s_StriderMortar.m_X - 4.0f, s_StriderMortar.m_Y + 26.0f, s_StriderMortar.m_W * 0.8f, s_StriderMortar.m_H * 0.55f, 0.5f,
			vec4(0.35f, 0.32f, 0.3f, 1.0f));
	Leg(2, Lit);
	Leg(3, Lit);
	if(!In.m_aBroken[STRIDER_PART_SHIELD])
	{
		const CStriderSprite &s = s_StriderShield;
		const vec4 c = (In.m_Flags & BOSSV5_FLAG_THRUST) ? vec4(1.25f, 1.1f, 0.9f, 1.0f) : Lit;
		Part(s.m_pName, s.m_X + m_ShieldPush, s.m_Y, s.m_W, s.m_H, 0.0f, c);
	}
	else
	{
		const CStriderSprite &s = s_StriderShieldBroken;
		Part(s.m_pName, s.m_X + m_ShieldPush * 0.5f, s.m_Y, s.m_W, s.m_H, 0.08f, Lit);
	}
}

// ================================================================ Storm Seraph

void CSeraphRig::Reset()
{
	ResetBase();
	m_Flap = 0.0f;
	m_FlapSpeed = 5.0f;
	m_Fold = m_Spread = 0.0f;
	m_Bank = m_Spin = 0.0f;
	m_HaloSpin = 0.0f;
	m_Droop = 0.0f;
}

vec2 CSeraphRig::CorePos() const { return ToWorld(s_SeraphCore.m_X, s_SeraphCore.m_Y); }
vec2 CSeraphRig::HaloPos() const { return ToWorld(s_SeraphHalo.m_X, s_SeraphHalo.m_Y); }
vec2 CSeraphRig::ThrusterPos() const { return ToWorld(s_SeraphThruster.m_X, s_SeraphThruster.m_Y + 30.0f); }

void CSeraphRig::Update(CCollision *pCollision, const CBossV5RigInput &In, float Dt)
{
	if(Dt <= 0.0f)
		return;
	Dt = min(Dt, 0.05f);
	m_Time += Dt;
	m_Pos = In.m_Pos;
	if(!m_Init)
	{
		m_Dir = In.m_Dir;
		m_Turn = (float)m_Dir;
		m_Body = m_Pos;
		m_Init = true;
	}
	m_Dir = In.m_Dir;
	m_Turn = Approach(m_Turn, (float)m_Dir, 14.0f, Dt);
	if(fabsf(m_Turn) < 0.08f)
		m_Turn = m_Turn < 0.0f ? -0.08f : 0.08f;

	const int Act = In.m_Act;
	const float t = In.m_Time;
	const bool Grounded = In.m_Flags & BOSSV5_FLAG_AIR;
	const bool Thrust = In.m_Flags & BOSSV5_FLAG_THRUST;
	float WantFlap = 6.0f, WantFold = 0.0f, WantSpread = 0.0f, WantDroop = 0.0f;
	float WantTilt = clamp(In.m_Vel.x * 0.025f, -0.35f, 0.35f);
	switch(Act)
	{
	case SERAPH_ACT_FLY:
		WantFlap = 9.0f;
		break;
	case SERAPH_ACT_DIVE:
		if(Thrust)
		{
			WantFold = 1.0f;
			WantFlap = 0.0f;
			const float a = atan2f(In.m_Vel.y, fabsf(In.m_Vel.x) + 0.01f);
			WantTilt = clamp(a * 0.7f, -0.9f, 0.9f) * m_Dir;
		}
		else
		{
			WantSpread = 0.8f; // rear back
			WantFlap = 12.0f;
			WantTilt = -0.25f * m_Dir;
		}
		break;
	case SERAPH_ACT_LATTICE:
	case SERAPH_ACT_ORBS:
		WantSpread = 0.6f;
		WantFlap = 7.0f;
		break;
	case SERAPH_ACT_ZAP:
		WantSpread = 0.4f;
		WantTilt = 0.15f * m_Dir;
		break;
	case SERAPH_ACT_PLUNGE:
		if(Thrust)
		{
			WantFold = 1.0f;
			WantFlap = 0.0f;
			WantTilt = 0.2f * m_Dir;
		}
		else
		{
			WantSpread = 1.0f;
			WantFlap = 14.0f;
		}
		break;
	case SERAPH_ACT_ROAR:
		WantSpread = 1.0f;
		WantFlap = 4.0f;
		if(t > 0.4f && t < 1.0f)
			m_Shake = max(m_Shake, 0.6f);
		break;
	case SERAPH_ACT_STAGGER:
		WantFold = 0.4f;
		m_Shake = max(m_Shake, 0.4f);
		break;
	case SERAPH_ACT_DEATH:
		WantFold = 0.6f;
		WantFlap = 0.0f;
		m_Spin += Dt * (2.0f + t * 3.0f) * m_Dir;
		break;
	default:
		break;
	}
	if(Grounded)
	{
		WantDroop = 1.0f;
		WantFold = 0.7f;
		WantFlap = 1.5f;
		WantTilt = -0.15f * m_Dir;
	}
	if(In.m_aBroken[SERAPH_PART_WINGS])
		WantFlap *= 1.4f; // ragged, frantic flapping
	m_FlapSpeed = Approach(m_FlapSpeed, WantFlap, 6.0f, Dt);
	m_Flap += m_FlapSpeed * Dt;
	m_Fold = Approach(m_Fold, WantFold, 10.0f, Dt);
	m_Spread = Approach(m_Spread, WantSpread, 6.0f, Dt);
	m_Droop = Approach(m_Droop, WantDroop, 5.0f, Dt);
	m_Tilt = Approach(m_Tilt, WantTilt + (Act == SERAPH_ACT_DEATH ? m_Spin : 0.0f), 8.0f, Dt);
	m_HaloSpin += Dt * (Act == SERAPH_ACT_ORBS || Act == SERAPH_ACT_ROAR ? 9.0f : 2.5f);
	m_Shake = max(0.0f, m_Shake - Dt * 2.5f);

	vec2 ShakeOff(0, 0);
	if(m_Shake > 0.0f)
		ShakeOff = vec2(sinf(m_Time * 71.0f), cosf(m_Time * 57.0f)) * 5.0f * m_Shake;
	const float Bob = (1.0f - m_Droop) * sinf(m_Flap) * 6.0f;
	m_Body = m_Pos + vec2(0, Bob + m_Droop * 10.0f) + ShakeOff;
	(void)pCollision;
}

void CSeraphRig::Render(CRenderTools *pRender, int Atlas, const CBossV5RigInput &In)
{
	if(!m_Init)
		return;
	const bool Flip = m_Turn < 0.0f;
	const int D = Flip ? -1 : 1;
	const float Sx = fabsf(m_Turn);
	const float Hurt = In.m_Hurt;
	const bool Exposed = In.m_Flags & BOSSV5_FLAG_EXPOSED;
	const vec4 Lit(1.0f, 1.0f - Hurt * 0.35f, 1.0f - Hurt * 0.35f, 1.0f);
	const float FacingTilt = m_Tilt * D;

	auto Part = [&](const char *pName, float x, float y, float w, float h, float Rot, vec4 Color)
	{
		pRender->RenderAtlasSpriteEx(Atlas, pName, ToWorld(x, y), vec2(w * Sx, h), m_Tilt + Rot * (Flip ? -1.0f : 1.0f), Flip, false, Color);
	};
	// Wing beat: down-stroke fast, up-stroke slow. Rest pose is raised (tip up = positive in
	// facing space); spread lifts it further, folded sweeps it back flat along the body.
	auto WingAngle = [&](float Rest, float Phase, float Amp)
	{
		const float s = sinf(m_Flap + Phase);
		const float Beat = (s > 0.0f ? s : s * 0.7f) * Amp * (1.0f - m_Fold);
		return FacingTilt + Rest - Beat + m_Spread * 0.4f - m_Fold * (Rest + 0.25f);
	};
	const bool Ragged = In.m_aBroken[SERAPH_PART_WINGS];
	const vec4 WingColor = Ragged ? vec4(0.6f, 0.55f, 0.55f, 1.0f) : Lit;
	const float WScale = Ragged ? 0.8f : 1.0f;
	auto Wing = [&](const char *pName, float x, float y, float W, float H, float Rx, float Ry, float Angle, vec4 Color)
	{
		BossV5DrawPivot(pRender, Atlas, pName, ToWorld(x, y), W * WScale * Sx, H * WScale, Rx, Ry, Angle, D, Color);
	};

	// Far wings and the near lower wing sit behind the body.
	Wing("wing", SERAPH_WING_X + 20.0f, SERAPH_WING_Y - 16.0f, SERAPH_WING_W * 0.9f, SERAPH_WING_H * 0.9f, SERAPH_WING_RX, SERAPH_WING_RY,
		WingAngle(0.55f, 0.35f, 0.5f), Shade(WingColor, 0.55f));
	Wing("wing2", SERAPH_WING2_X + 16.0f, SERAPH_WING2_Y - 6.0f, SERAPH_WING2_W * 0.9f, SERAPH_WING2_H * 0.9f, SERAPH_WING2_RX,
		SERAPH_WING2_RY, WingAngle(-0.05f, 0.9f, 0.4f), Shade(WingColor, 0.55f));
	Wing("wing2", SERAPH_WING2_X, SERAPH_WING2_Y, SERAPH_WING2_W, SERAPH_WING2_H, SERAPH_WING2_RX, SERAPH_WING2_RY,
		WingAngle(-0.25f, 0.55f, 0.45f), WingColor);

	{
		const CSeraphSprite &s = s_SeraphThruster;
		const float Flicker = 1.0f + 0.12f * sinf(m_Time * 40.0f);
		Part(s.m_pName, s.m_X, s.m_Y + (Flicker - 1.0f) * 20.0f, s.m_W, s.m_H * Flicker, 0.0f, Lit);
	}
	Part(s_SeraphBody.m_pName, s_SeraphBody.m_X, s_SeraphBody.m_Y, s_SeraphBody.m_W, s_SeraphBody.m_H, 0.0f, Lit);
	{
		const CSeraphSprite &s = s_SeraphCore;
		const float Pulse = Exposed ? 1.0f + 0.15f * sinf(m_Time * 14.0f) : 1.0f + 0.04f * sinf(m_Time * 4.0f);
		const vec4 Glow = Exposed ? vec4(1.3f, 1.4f, 1.6f, 1.0f) : Lit;
		Part(s.m_pName, s.m_X, s.m_Y, s.m_W * Pulse, s.m_H * Pulse, m_HaloSpin * 0.3f, Glow);
	}
	if(!In.m_aBroken[SERAPH_PART_HALO])
	{
		const CSeraphSprite &s = s_SeraphHalo;
		const float Squash = 0.75f + 0.25f * cosf(m_HaloSpin);
		Part(s.m_pName, s.m_X, s.m_Y + sinf(m_Time * 3.0f) * 3.0f, s.m_W * Squash, s.m_H, 0.0f, Lit);
	}

	// Near wing over the body.
	Wing("wing", SERAPH_WING_X, SERAPH_WING_Y, SERAPH_WING_W * 0.85f, SERAPH_WING_H * 0.85f, SERAPH_WING_RX, SERAPH_WING_RY,
		WingAngle(0.3f, 0.0f, 0.55f), WingColor);
}

// ================================================================ Siege Monolith

void CMonolithRig::Reset()
{
	ResetBase();
	m_TurretAngle = 0.0f;
	m_Recoil = 0.0f;
	m_Retract = 0.0f;
	m_VentOpen = 0.0f;
	m_Sway = 0.0f;
	m_Settle = 0.0f;
	for(int i = 0; i < 3; i++)
	{
		m_aFoot[i] = vec2(0, 0);
		m_aFootOk[i] = false;
	}
}

vec2 CMonolithRig::Muzzle() const
{
	const vec2 Pivot = ToWorld(MONOLITH_TURRET_X, MONOLITH_TURRET_Y);
	const float a = m_TurretAngle;
	return Pivot + vec2(cosf(a) * m_Dir, sinf(a)) * (MONOLITH_MUZZLE - m_Recoil * 10.0f);
}

vec2 CMonolithRig::ThrusterPos() const { return ToWorld(0.0f, s_MonolithThruster.m_Y + 45.0f); }

vec2 CMonolithRig::VentPos(int i) const { return ToWorld(s_aMonolithVent[i].m_X, s_aMonolithVent[i].m_Y); }

void CMonolithRig::Update(CCollision *pCollision, const CBossV5RigInput &In, float Dt)
{
	if(Dt <= 0.0f)
		return;
	Dt = min(Dt, 0.05f);
	m_Time += Dt;
	m_Pos = In.m_Pos;
	const bool Air = In.m_Flags & BOSSV5_FLAG_AIR;
	if(!m_Init)
	{
		m_Dir = In.m_Dir;
		m_Turn = (float)m_Dir;
		m_Body = m_Pos;
		m_Retract = Air ? 1.0f : 0.0f;
		m_Init = true;
	}
	m_Dir = In.m_Dir;
	m_Turn = Approach(m_Turn, (float)m_Dir, 10.0f, Dt);
	if(fabsf(m_Turn) < 0.08f)
		m_Turn = m_Turn < 0.0f ? -0.08f : 0.08f;

	const int Act = In.m_Act;
	const float t = In.m_Time;
	const bool WasDown = m_Retract < 0.5f;
	m_Retract = Approach(m_Retract, Air || (Act == MONOLITH_ACT_SLAM && In.m_Vel.y > 8.0f) ? 1.0f : 0.0f, 5.0f, Dt);
	if(!WasDown && m_Retract < 0.5f)
	{
		m_Landed += 3;
		m_Shake = 0.8f;
	}
	m_VentOpen = Approach(m_VentOpen, (In.m_Flags & BOSSV5_FLAG_EXPOSED) ? 1.0f : 0.0f, 9.0f, Dt);
	if(Act == MONOLITH_ACT_ROAR && t > 0.4f && t < 1.0f)
		m_Shake = max(m_Shake, 0.6f);
	if(Act == MONOLITH_ACT_STAGGER)
		m_Shake = max(m_Shake, 0.35f);
	if(Act == MONOLITH_ACT_SLAM && In.m_Vel.y > 8.0f)
		m_Shake = max(m_Shake, 0.2f);
	m_Shake = max(0.0f, m_Shake - Dt * 2.5f);

	// Turret: follows the aim point the server sends (beam end while sweeping).
	{
		const vec2 Pivot = ToWorld(MONOLITH_TURRET_X, MONOLITH_TURRET_Y);
		const vec2 d = In.m_Aim - Pivot;
		const float Want = clamp(atan2f(d.y, d.x * m_Dir), -1.45f, 1.45f);
		m_TurretAngle = Approach(m_TurretAngle, Want, Act == MONOLITH_ACT_SWEEP ? 30.0f : 10.0f, Dt);
	}
	if(Act == MONOLITH_ACT_MORTAR)
		for(int s = 0; s < MonolithShellCount(In.m_Phase); s++)
		{
			const float Fire = MonolithShellTick(s) / (float)BOSSV5_TICK_SPEED;
			if(t >= Fire && t < Fire + 0.05f)
				m_Recoil = 1.0f;
		}
	m_Recoil = max(0.0f, m_Recoil - Dt * 5.0f);

	float WantTilt = clamp(In.m_Vel.x * (Air ? 0.02f : 0.01f), -0.25f, 0.25f);
	// Standing: lean with the ground under the outer struts (front strut 1 vs rear strut 2).
	if(!Air && m_Retract < 0.5f && m_aFootOk[1] && m_aFootOk[2])
	{
		const vec2 d = m_aFoot[1] - m_aFoot[2];
		WantTilt += clamp(atan2f(d.y * m_Dir, fabsf(d.x) + 1.0f) * m_Dir, -0.4f, 0.4f) * 0.8f;
	}
	if(Act == MONOLITH_ACT_DEATH)
		WantTilt = 0.35f * m_Dir * min(t, 1.0f);
	m_Tilt = Approach(m_Tilt, WantTilt, 4.0f, Dt);
	m_Sway = sinf(m_Time * (Air ? 2.2f : 1.4f)) * (Air ? 6.0f : 2.0f);
	vec2 ShakeOff(0, 0);
	if(m_Shake > 0.0f)
		ShakeOff = vec2(sinf(m_Time * 71.0f), cosf(m_Time * 57.0f)) * 5.0f * m_Shake;
	// Settle onto the struts: low enough that every planted strut reaches its foot, never so low
	// that the hull bottom touches the floor under its middle.
	float WantSettle = 0.0f;
	if(!Air && m_Retract < 0.5f && Act != MONOLITH_ACT_DEATH)
	{
		vec2 aHip[3];
		for(int i = 0; i < 3; i++)
			aHip[i] = m_Pos + (ToWorld(s_aMonolithStrutX[i], s_aMonolithStrutY[i]) - m_Body);
		WantSettle = BossV5LegSag(aHip, m_aFoot, m_aFootOk, 3, MONOLITH_STRUT_REACH);
		vec2 Under;
		const float Floor = pCollision->IntersectLine(m_Pos, m_Pos + vec2(0, MONOLITH_HOVER + 60.0f), &Under, 0, false, true) ?
			Under.y : m_Pos.y + MONOLITH_HOVER;
		WantSettle = clamp(WantSettle, 0.0f, max(0.0f, Floor - s_MonolithHull.m_H * 0.5f - 4.0f - m_Pos.y));
	}
	m_Settle = Approach(m_Settle, WantSettle, 8.0f, Dt);
	m_Body = m_Pos + vec2(0, m_Sway + m_Settle) + ShakeOff;

	// Struts: Crawler rule, a planted strut always ends on real ground it can reach.
	for(int i = 0; i < 3; i++)
	{
		const vec2 Hip = ToWorld(s_aMonolithStrutX[i], s_aMonolithStrutY[i]);
		const vec2 Tucked = ToWorld(s_aMonolithStrutX[i] + s_aMonolithFootX[i] * 0.25f, s_aMonolithStrutY[i] + 60.0f);
		vec2 Want = Tucked;
		vec2 At;
		m_aFootOk[i] = BossV5Foothold(pCollision, Hip, m_Pos + vec2(s_aMonolithFootX[i] * m_Turn, MONOLITH_HOVER + 20.0f),
			MONOLITH_STRUT_REACH, &At);
		if(m_aFootOk[i])
			Want = mix(At, Tucked, m_Retract);
		if(!m_Init || length(m_aFoot[i]) < 1.0f)
			m_aFoot[i] = Want;
		// Planted struts snap down onto the floor fast (no hovering lag), lift a little slower.
		m_aFoot[i] = Approach(m_aFoot[i], Want, Want.y > m_aFoot[i].y ? 30.0f : 16.0f, Dt);
	}
}

void CMonolithRig::Render(CRenderTools *pRender, int Atlas, const CBossV5RigInput &In)
{
	if(!m_Init)
		return;
	const bool Flip = m_Turn < 0.0f;
	const int D = Flip ? -1 : 1;
	const float Sx = fabsf(m_Turn);
	const float Hurt = In.m_Hurt;
	const vec4 Lit(1.0f, 1.0f - Hurt * 0.35f, 1.0f - Hurt * 0.35f, 1.0f);
	const vec4 Far = Shade(Lit, 0.55f);

	auto Part = [&](const char *pName, float x, float y, float w, float h, float Rot, vec4 Color)
	{
		pRender->RenderAtlasSpriteEx(Atlas, pName, ToWorld(x, y), vec2(w * Sx, h), m_Tilt + Rot * (Flip ? -1.0f : 1.0f), Flip, false, Color);
	};
	auto Strut = [&](int i, vec4 Color)
	{
		const vec2 Hip = ToWorld(s_aMonolithStrutX[i], s_aMonolithStrutY[i]);
		const bool Mirror = (s_aMonolithFootX[i] - s_aMonolithStrutX[i]) * D < 0.0f; // art leans outward-right
		BossV5DrawBone(pRender, Atlas, "strut", Hip, m_aFoot[i] - vec2(0.0f, 16.0f), MONOLITH_STRUT_ART_W, MONOLITH_STRUT_ART_H, MONOLITH_STRUT_J0X,
			MONOLITH_STRUT_J0Y, MONOLITH_STRUT_J1X, MONOLITH_STRUT_J1Y, Mirror, false, Color);
	};

	Strut(0, Far);
	{
		const CMonolithSprite &s = s_MonolithThruster;
		const bool Dead = In.m_aBroken[MONOLITH_PART_THRUSTERS];
		const vec4 c = Dead ? vec4(0.4f, 0.37f, 0.35f, 1.0f) : ((In.m_Flags & (BOSSV5_FLAG_AIR | BOSSV5_FLAG_THRUST)) ? vec4(1.2f, 1.1f, 1.0f, 1.0f) : Lit);
		Part(s.m_pName, s.m_X, s.m_Y, s.m_W, s.m_H, 0.0f, c);
	}
	if(!In.m_aBroken[MONOLITH_PART_TURRET])
		Part(s_MonolithLauncher.m_pName, s_MonolithLauncher.m_X, s_MonolithLauncher.m_Y, s_MonolithLauncher.m_W, s_MonolithLauncher.m_H, 0.0f, Far);
	Part(s_MonolithHull.m_pName, s_MonolithHull.m_X, s_MonolithHull.m_Y, s_MonolithHull.m_W, s_MonolithHull.m_H, 0.0f, Lit);
	if(!In.m_aBroken[MONOLITH_PART_TURRET])
		Part(s_MonolithLauncher.m_pName, s_MonolithLauncher.m_X + 6.0f, s_MonolithLauncher.m_Y + 2.0f, s_MonolithLauncher.m_W,
			s_MonolithLauncher.m_H, 0.0f, Lit);
	for(int v = 0; v < 2; v++)
	{
		const CMonolithSprite &s = s_aMonolithVent[v];
		if(In.m_aBroken[MONOLITH_PART_VENTS])
			Part("vent_closed", s.m_X, s.m_Y, MONOLITH_VENT_CLOSED_W, MONOLITH_VENT_CLOSED_H, 0.0f, vec4(0.35f, 0.3f, 0.28f, 1.0f));
		else if(m_VentOpen > 0.5f)
		{
			const float Glow = 1.1f + 0.3f * sinf(m_Time * 18.0f);
			Part("vent", s.m_X, s.m_Y, s.m_W * m_VentOpen, s.m_H, 0.0f, vec4(Glow, Glow * 0.9f, Glow * 0.8f, 1.0f));
		}
		else
			Part("vent_closed", s.m_X, s.m_Y, MONOLITH_VENT_CLOSED_W, MONOLITH_VENT_CLOSED_H * (1.0f - m_VentOpen), 0.0f, Lit);
	}
	if(!In.m_aBroken[MONOLITH_PART_TURRET])
	{
		const vec2 Pivot = ToWorld(MONOLITH_TURRET_X - m_Recoil * 10.0f * cosf(m_TurretAngle), MONOLITH_TURRET_Y);
		const vec4 c = (In.m_Flags & BOSSV5_FLAG_BEAM) ? vec4(1.25f, 1.0f, 1.0f, 1.0f) : Lit;
		BossV5DrawPivot(pRender, Atlas, "turret", Pivot, MONOLITH_TURRET_W, MONOLITH_TURRET_H, MONOLITH_TURRET_PX, MONOLITH_TURRET_PY,
			m_TurretAngle, D, c);
	}
	else
		BossV5DrawPivot(pRender, Atlas, "turret", ToWorld(MONOLITH_TURRET_X, MONOLITH_TURRET_Y + 8.0f), MONOLITH_TURRET_W * 0.55f,
			MONOLITH_TURRET_H, MONOLITH_TURRET_PX, MONOLITH_TURRET_PY, 0.5f, D, vec4(0.35f, 0.32f, 0.3f, 1.0f));
	Strut(1, Lit);
	Strut(2, Lit);
}
