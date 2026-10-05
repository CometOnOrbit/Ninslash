#include <base/math.h>
#include <engine/graphics.h>
#include <game/collision.h>
#include <game/client/render.h>
#include <game/foundry_warden.h> // WardenSolveLegUp
#include <game/skitter_matriarch.h>

#include "boss_v5_rig.h" // BossV5Foothold / BossV5PressFoot / BossV5LegSag (Crawler-style footing)
#include "matriarch_rig.h"

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

CMatriarchRig::CMatriarchRig()
{
	Reset();
}

void CMatriarchRig::Reset()
{
	m_Init = false;
	m_ItemID = -1;
	m_LastTick = 0;
	m_Landed = 0;
	m_Pos = m_Body = vec2(0, 0);
	m_Dir = 1;
	m_Turn = 1.0f;
	m_Tilt = m_Pitch = m_Crouch = m_Bob = m_Shake = 0.0f;
	m_Settle = 0.0f;
	m_SacAngle = m_SacVel = m_SacSwell = 0.0f;
	m_Mandible = 0.0f;
	m_Time = 0.0f;
	m_Airborne = false;
	for(int i = 0; i < 6; i++)
	{
		m_aFoot[i] = m_aFrom[i] = m_aTo[i] = vec2(0, 0);
		m_aStep[i] = -1.0f;
		m_aPlanted[i] = false;
	}
}

// Local layout point (facing right) to world, through turn squash, pitch/tilt and bob.
vec2 CMatriarchRig::ToWorld(float x, float y) const
{
	const float a = m_Tilt + m_Pitch * -(float)m_Dir;
	const float lx = x * m_Turn;
	const float c = cosf(a), s = sinf(a);
	return m_Body + vec2(lx * c - y * s, lx * s + y * c);
}

vec2 CMatriarchRig::HeadPos() const { return ToWorld(s_MatriarchHead.m_X + 30.0f, s_MatriarchHead.m_Y - 10.0f); }
vec2 CMatriarchRig::SacPos() const { return ToWorld(s_MatriarchSac.m_X, s_MatriarchSac.m_Y); }
vec2 CMatriarchRig::MouthPos() const { return ToWorld(MATRIARCH_MANDIBLE_X, MATRIARCH_MANDIBLE_Y + 10.0f); }

// Crawler rule: real ground within the leg's reach (rest spot, then slid toward the hip, then any
// surface the leg points at); after that a wall beside the hip, then the ceiling (climbing).
bool CMatriarchRig::FindFoothold(CCollision *pCollision, int Leg, vec2 *pOut) const
{
	const float Fx = s_aMatriarchFootX[Leg] * (float)m_Dir;
	vec2 At;
	const vec2 Hip = ToWorld(s_aMatriarchHipX[Leg], s_aMatriarchHipY[Leg]);
	const float Reach = MATRIARCH_THIGH + MATRIARCH_SHIN - 20.0f;
	if(BossV5Foothold(pCollision, Hip, m_Pos + vec2(Fx, MATRIARCH_HOVER + 20.0f), Reach, pOut))
		return true;
	const float Side = s_aMatriarchFootX[Leg] >= 0.0f ? (float)m_Dir : -(float)m_Dir;
	const float Dy = (Leg % 3 - 1) * 60.0f;
	if(pCollision->IntersectLine(Hip, Hip + vec2(Side * Reach, Dy + 40.0f), &At, 0))
	{
		*pOut = At - vec2(Side * 3.0f, 0);
		return true;
	}
	if(pCollision->IntersectLine(Hip, Hip + vec2(Fx * 0.4f, -Reach), &At, 0))
	{
		*pOut = At + vec2(0, 3.0f);
		return true;
	}
	return false;
}

void CMatriarchRig::PoseFoot(int Leg, vec2 Local, float Rate, float Dt)
{
	m_aFoot[Leg] = Approach(m_aFoot[Leg], ToWorld(Local.x, Local.y), Rate, Dt);
	m_aPlanted[Leg] = false;
	m_aStep[Leg] = -1.0f;
}

void CMatriarchRig::Update(CCollision *pCollision, const CInput &In, float Dt)
{
	if(Dt <= 0.0f)
		return;
	if(Dt > 0.05f)
		Dt = 0.05f;
	m_Time += Dt;
	m_Pos = In.m_Pos;
	if(!m_Init)
	{
		m_Dir = In.m_Dir;
		m_Turn = (float)m_Dir;
		m_Body = m_Pos;
		for(int i = 0; i < 6; i++)
		{
			vec2 F;
			m_aFoot[i] = FindFoothold(pCollision, i, &F) ? F : m_Pos + vec2(s_aMatriarchFootX[i] * m_Dir, MATRIARCH_HOVER);
			m_aPlanted[i] = true;
			m_aStep[i] = -1.0f;
		}
		m_Init = true;
	}
	m_Dir = In.m_Dir;
	m_Turn = Approach(m_Turn, (float)m_Dir, 22.0f, Dt);
	if(fabsf(m_Turn) < 0.08f)
		m_Turn = m_Turn < 0.0f ? -0.08f : 0.08f;

	const int Act = In.m_Act;
	const float t = In.m_Time;
	const float Speed = length(In.m_Vel);

	// Airborne when nothing is under the body within leg reach.
	vec2 GroundAt;
	const bool Ground = pCollision->IntersectLine(m_Pos, m_Pos + vec2(0, MATRIARCH_HOVER + 70.0f), &GroundAt, 0, false, true);
	const bool WasAir = m_Airborne;
	m_Airborne = !Ground && Act != MATRIARCH_ACT_CLING && Act != MATRIARCH_ACT_DEATH && fabsf(In.m_Vel.y) > 1.0f;
	if(Act == MATRIARCH_ACT_POUNCE && t > 0.3f && In.m_Vel.y < -2.0f)
		m_Airborne = true;
	if(WasAir && !m_Airborne)
	{
		m_Landed += 6;
		m_Shake = 1.0f;
		for(int i = 0; i < 6; i++)
		{
			vec2 F;
			if(FindFoothold(pCollision, i, &F))
				m_aFoot[i] = F;
			m_aPlanted[i] = true;
			m_aStep[i] = -1.0f;
		}
	}

	// ---- act poses (pitch is nose-up positive, crouch pushes the body down) ----
	float WantPitch = 0.0f, WantCrouch = 0.0f, WantMandible = 0.15f, WantSwell = 0.0f;
	switch(Act)
	{
	case MATRIARCH_ACT_POUNCE:
		if(!m_Airborne && t < 0.45f)
		{
			WantCrouch = 26.0f;
			WantPitch = 0.12f;
			WantMandible = 0.5f;
		}
		else if(m_Airborne)
		{
			WantPitch = clamp(-In.m_Vel.y * 0.025f, -0.35f, 0.45f);
			WantMandible = 0.8f;
		}
		else
			WantCrouch = 16.0f;
		break;
	case MATRIARCH_ACT_STAB:
		WantPitch = t < 0.3f ? 0.2f : -0.06f;
		WantMandible = 0.7f;
		break;
	case MATRIARCH_ACT_SPIT:
		WantPitch = 0.3f;
		WantMandible = 0.55f + 0.35f * sinf(t * 24.0f);
		break;
	case MATRIARCH_ACT_BROOD:
		WantPitch = -0.12f;
		WantSwell = t > 0.2f && t < 1.3f ? 1.0f : 0.0f;
		break;
	case MATRIARCH_ACT_ROAR:
		WantPitch = t < 1.2f ? 0.38f : 0.0f;
		WantMandible = 1.0f;
		if(t > 0.45f && t < 1.1f)
			m_Shake = max(m_Shake, 0.6f);
		break;
	case MATRIARCH_ACT_STAGGER:
		WantCrouch = 22.0f;
		WantPitch = -0.1f;
		m_Shake = max(m_Shake, 0.35f);
		break;
	case MATRIARCH_ACT_THRASH:
		WantPitch = t < 0.42f ? 0.55f : 0.0f;
		WantCrouch = t < 0.42f ? -18.0f : 10.0f;
		WantMandible = 0.9f;
		break;
	case MATRIARCH_ACT_DEATH:
		WantCrouch = 40.0f + min(t, 1.0f) * 20.0f;
		WantPitch = -0.15f;
		WantMandible = 0.9f;
		break;
	default:
		break;
	}
	m_Pitch = Approach(m_Pitch, WantPitch, 10.0f, Dt);
	m_Crouch = Approach(m_Crouch, WantCrouch, 12.0f, Dt);
	m_Mandible = Approach(m_Mandible, WantMandible, 14.0f, Dt);
	m_SacSwell = Approach(m_SacSwell, WantSwell, 6.0f, Dt);
	m_Shake = max(0.0f, m_Shake - Dt * 2.5f);

	// ---- body ride: bob on the gait, shake, crouch ----
	const bool Moving = fabsf(In.m_Vel.x) > 1.5f;
	m_Bob = Approach(m_Bob, Moving ? sinf(m_Time * 18.0f) * 3.0f : sinf(m_Time * 2.4f) * 2.5f, 12.0f, Dt);
	vec2 ShakeOff(0, 0);
	if(m_Shake > 0.0f)
		ShakeOff = vec2(sinf(m_Time * 71.0f), cosf(m_Time * 57.0f)) * 5.0f * m_Shake;
	// Settle between the feet: low enough that every planted leg reaches its foot (no dangling leg on
	// a slope or step), never so low that the body box sinks into the floor under its middle.
	float WantSettle = 0.0f;
	if(!m_Airborne && Act != MATRIARCH_ACT_CLING && Act != MATRIARCH_ACT_DEATH)
	{
		vec2 aHip[6];
		for(int i = 0; i < 6; i++)
			aHip[i] = m_Pos + (ToWorld(s_aMatriarchHipX[i], s_aMatriarchHipY[i]) - m_Body);
		WantSettle = BossV5LegSag(aHip, m_aFoot, m_aPlanted, 6, MATRIARCH_THIGH + MATRIARCH_SHIN - 8.0f);
		const float Floor = Ground ? GroundAt.y : m_Pos.y + MATRIARCH_HOVER;
		WantSettle = clamp(WantSettle, 0.0f, max(0.0f, Floor - 40.0f - m_Pos.y - m_Crouch));
	}
	m_Settle = Approach(m_Settle, WantSettle, 8.0f, Dt);
	m_Body = m_Pos + vec2(0, m_Bob + m_Crouch + m_Settle) + ShakeOff;

	// ---- legs ----
	if(m_Airborne && Act != MATRIARCH_ACT_DEATH)
	{
		// Leap: front legs reach forward, rear legs trail. Rising = spread, falling = brace.
		const float Brace = In.m_Vel.y > 0.0f ? 1.0f : 0.0f;
		for(int i = 0; i < 6; i++)
		{
			const float Fx = s_aMatriarchFootX[i];
			const vec2 Local = Fx > 0.0f ? vec2(Fx + 50.0f, 40.0f + Brace * 60.0f) : vec2(Fx - 40.0f, 30.0f + Brace * 50.0f);
			PoseFoot(i, Local, 16.0f, Dt);
		}
	}
	else if(Act == MATRIARCH_ACT_DEATH)
	{
		for(int i = 0; i < 6; i++)
		{
			const float k = min(t / 1.2f, 1.0f);
			const vec2 Local(s_aMatriarchHipX[i] * 1.3f + (i % 3 - 1) * 20.0f * k, 60.0f - 30.0f * k);
			PoseFoot(i, Local, 5.0f, Dt);
		}
	}
	else
	{
		// Act overrides for single legs: stab legs and thrash flails.
		bool aOverride[6] = {false, false, false, false, false, false};
		if(Act == MATRIARCH_ACT_STAB)
		{
			const int Count = MatriarchStabCount(In.m_Phase);
			for(int s = 0; s < Count; s++)
			{
				const float Hit = MatriarchStabTick(s) / (float)MATRIARCH_TICK_SPEED;
				const int Leg = (s % 2) ? 2 : 5;
				if(t > Hit - 0.2f && t < Hit + 0.14f)
				{
					aOverride[Leg] = true;
					if(t < Hit - 0.04f)
						PoseFoot(Leg, vec2(150.0f, -110.0f), 24.0f, Dt); // cock back
					else
						m_aFoot[Leg] = Approach(m_aFoot[Leg], In.m_Aim, 40.0f, Dt);
					m_aPlanted[Leg] = false;
				}
			}
			if(t < MatriarchStabTick(0) / (float)MATRIARCH_TICK_SPEED - 0.2f)
			{
				aOverride[5] = aOverride[2] = true;
				PoseFoot(5, vec2(170.0f, -90.0f), 10.0f, Dt);
				PoseFoot(2, vec2(190.0f, -70.0f), 10.0f, Dt);
			}
		}
		else if(Act == MATRIARCH_ACT_THRASH)
		{
			for(int i = 0; i < 6; i++)
			{
				aOverride[i] = i == 2 || i == 5 || i == 1 || i == 4;
				if(!aOverride[i])
					continue;
				const float Ang = (t < 0.42f ? -1.9f : 0.4f) + (i % 3) * 0.5f + sinf(t * 30.0f + i) * 0.2f;
				PoseFoot(i, vec2(cosf(Ang) * 210.0f * (s_aMatriarchFootX[i] > 0 ? 1.0f : -1.0f), sinf(Ang) * 170.0f), 18.0f, Dt);
			}
		}
		else if(Act == MATRIARCH_ACT_ROAR && t < 1.2f)
		{
			aOverride[2] = aOverride[5] = true;
			PoseFoot(2, vec2(200.0f, -150.0f + sinf(t * 20.0f) * 15.0f), 10.0f, Dt);
			PoseFoot(5, vec2(170.0f, -170.0f + cosf(t * 20.0f) * 15.0f), 10.0f, Dt);
		}

		// Tripod gait. A group may lift only while the other group is planted.
		bool aGroupStepping[2] = {false, false};
		for(int i = 0; i < 6; i++)
			if(m_aStep[i] >= 0.0f)
				aGroupStepping[s_aMatriarchGait[i]] = true;
		const float StepTime = clamp(0.2f - Speed * 0.008f, 0.1f, 0.2f);
		const float Reach = MATRIARCH_THIGH + MATRIARCH_SHIN - 8.0f;
		for(int i = 0; i < 6; i++)
		{
			if(aOverride[i])
				continue;
			vec2 Rest;
			const bool Has = FindFoothold(pCollision, i, &Rest);
			const vec2 Hip = ToWorld(s_aMatriarchHipX[i], s_aMatriarchHipY[i]);
			if(!Has)
			{
				// Nothing to stand on (cliff edge): dangle and paw.
				PoseFoot(i, vec2(s_aMatriarchFootX[i] * 0.7f, MATRIARCH_HOVER + 10.0f * sinf(m_Time * 9.0f + i)), 10.0f, Dt);
				continue;
			}
			// A planted foot whose floor vanished slides down onto the new surface or re-steps.
			if(m_aPlanted[i] && m_aStep[i] < 0.0f && !BossV5PressFoot(pCollision, Hip, Reach, &m_aFoot[i]))
			{
				m_aFrom[i] = m_aFoot[i];
				m_aTo[i] = Rest;
				m_aStep[i] = 0.0f;
				aGroupStepping[s_aMatriarchGait[i]] = true;
				continue;
			}
			if(m_aStep[i] >= 0.0f)
			{
				m_aStep[i] += Dt / StepTime;
				// Keep tracking a moving target, but land on real ground.
				vec2 Lead;
				m_aTo[i] = BossV5Foothold(pCollision, Hip, Rest + vec2(In.m_Vel.x * 3.0f, 30.0f), Reach, &Lead) ? Lead : Rest;
				if(m_aStep[i] >= 1.0f)
				{
					m_aStep[i] = -1.0f;
					m_aFoot[i] = m_aTo[i];
					m_aPlanted[i] = true;
					if(Speed > 6.0f && (i == 3 || i == 5))
						m_Landed++;
				}
				else
				{
					const float k = m_aStep[i];
					const float e = k * k * (3.0f - 2.0f * k);
					m_aFoot[i] = mix(m_aFrom[i], m_aTo[i], e) + vec2(0, -sinf(k * pi) * MATRIARCH_STEP_LIFT);
				}
				continue;
			}
			if(!m_aPlanted[i])
			{
				// Coming out of a pose: step straight to the foothold.
				m_aFrom[i] = m_aFoot[i];
				m_aTo[i] = Rest;
				m_aStep[i] = 0.0f;
				continue;
			}
			const float Off = distance(m_aFoot[i], Rest);
			const bool Overstretched = distance(m_aFoot[i], Hip) > Reach;
			const int Group = s_aMatriarchGait[i];
			if(Overstretched || (Off > MATRIARCH_STEP_DIST && !aGroupStepping[1 - Group]))
			{
				m_aFrom[i] = m_aFoot[i];
				m_aTo[i] = Rest;
				m_aStep[i] = 0.0f;
				aGroupStepping[Group] = true;
			}
		}
	}

	// ---- body tilt from planted feet (rear vs front), steeper when a wall is being climbed ----
	{
		vec2 Rear(0, 0), Front(0, 0);
		int nr = 0, nf = 0;
		const int aRear[2] = {0, 3}, aFront[2] = {2, 5};
		for(int k = 0; k < 2; k++)
		{
			if(m_aPlanted[aRear[k]])
			{
				Rear += m_aFoot[aRear[k]];
				nr++;
			}
			if(m_aPlanted[aFront[k]])
			{
				Front += m_aFoot[aFront[k]];
				nf++;
			}
		}
		float WantTilt = 0.0f;
		if(nr && nf && !m_Airborne && Act != MATRIARCH_ACT_DEATH)
		{
			const vec2 d = Front / (float)nf - Rear / (float)nr;
			WantTilt = atan2f(d.y * m_Dir, d.x * m_Dir);
			const bool Climb = Act == MATRIARCH_ACT_CLING || fabsf(In.m_Vel.y) > 4.0f;
			const float Lim = Climb ? 0.95f : 0.5f;
			WantTilt = clamp(WantTilt, -Lim, Lim);
		}
		if(Act == MATRIARCH_ACT_CLING)
			WantTilt = -1.2f * m_Dir;
		m_Tilt = Approach(m_Tilt, WantTilt, 7.0f, Dt);
	}

	// Sac pendulum swings against the body's motion.
	const float Accel = -In.m_Vel.x * 0.01f;
	m_SacVel += (-m_SacAngle * 30.0f + Accel * 60.0f) * Dt - m_SacVel * 4.0f * Dt;
	m_SacAngle = clamp(m_SacAngle + m_SacVel * Dt * 6.0f, -0.6f, 0.6f);
}

void CMatriarchRig::Render(CRenderTools *pRender, IGraphics *pGraphics, int Atlas, const CInput &In)
{
	if(!m_Init)
		return;
	const float BodyRot = m_Tilt + m_Pitch * -(float)m_Dir;
	const bool Flip = m_Turn < 0.0f;
	const float Sx = fabsf(m_Turn);
	const float Hurt = In.m_Hurt;
	const vec4 Lit(1.0f, 1.0f - Hurt * 0.35f, 1.0f - Hurt * 0.35f, 1.0f);
	const vec4 Far(0.5f, 0.52f - Hurt * 0.2f, 0.58f - Hurt * 0.2f, 1.0f);

	auto Part = [&](const char *pName, float x, float y, float w, float h, float Rot, vec4 Color)
	{
		pRender->RenderAtlasSpriteEx(Atlas, pName, ToWorld(x, y), vec2(w * Sx, h), BodyRot + Rot * (Flip ? -1.0f : 1.0f), Flip,
			false, Color);
	};

	// Sprite hung from a pivot at its top edge (mandibles).
	auto Hung = [&](const char *pName, float px, float py, float w, float h, float Rot, vec4 Color)
	{
		Part(pName, px - sinf(Rot) * h * 0.5f, py + cosf(Rot) * h * 0.5f, w, h, Rot, Color);
	};

	auto Leg = [&](int i, vec4 Color)
	{
		const vec2 Hip = ToWorld(s_aMatriarchHipX[i], s_aMatriarchHipY[i]);
		const vec2 Foot = m_aFoot[i];
		float HipA, KneeA;
		WardenSolveLegUp(Hip.x, Hip.y, Foot.x, Foot.y, MATRIARCH_THIGH, MATRIARCH_SHIN, &HipA, &KneeA);
		const vec2 Knee = Hip + vec2(cosf(HipA), sinf(HipA)) * MATRIARCH_THIGH;
		vec2 ShinDir = Foot - Knee;
		const float ShinLen = length(ShinDir);
		ShinDir = ShinLen > 0.01f ? ShinDir / ShinLen : vec2(0, 1);
		const float ShinA = atan2f(ShinDir.y, ShinDir.x);
		// Thigh art: joint at the left end, 0.865 of the width between joints.
		const float TW = MATRIARCH_THIGH / 0.865f, TH = TW * 76.0f / 300.0f;
		const vec2 TC = Hip + vec2(cosf(HipA), sinf(HipA)) * (TW * 0.5f - TW * 0.078f);
		// Shin art: knee at the right end, blade tip at the left, 0.94 of the width.
		const float SW = MATRIARCH_SHIN / 0.94f, SH = SW * 109.0f / 380.0f;
		const vec2 SC = Knee + ShinDir * (SW * 0.5f - SW * 0.055f);
		const bool LegsBroken = In.m_LegsBroken;
		vec4 c = Color;
		if(LegsBroken)
			c = vec4(c.r * 0.7f, c.g * 0.62f, c.b * 0.55f, 1.0f);
		pRender->RenderAtlasSpriteEx(Atlas, "shin", SC, vec2(SW, SH), ShinA, true, cosf(ShinA) < 0.0f, c);
		pRender->RenderAtlasSpriteEx(Atlas, "thigh", TC, vec2(TW, TH), HipA, false, cosf(HipA) < 0.0f, c);
		pRender->RenderAtlasSpriteEx(Atlas, "hip", Hip, vec2(30.0f, 30.0f), BodyRot, false, false, c);
	};

	// Far side legs.
	for(int i = 0; i < 3; i++)
		Leg(i, Far);

	// Sac swings on its stalk and swells while laying; it glows when open.
	{
		const CMatriarchSprite &s = s_MatriarchSac;
		const float Pulse = In.m_Exposed ? 0.12f * sinf(m_Time * 14.0f) : 0.0f;
		const float Grow = 1.0f + m_SacSwell * (0.22f + 0.06f * sinf(m_Time * 20.0f)) + Pulse;
		if(In.m_SacBroken)
			Part("sac_broken", s.m_X + 6.0f, s.m_Y - 4.0f, 74.0f, 46.0f, s.m_Rot + 0.3f, Lit);
		else
		{
			const vec4 Glow = In.m_Exposed ? vec4(1.25f, 1.4f, 1.0f, 1.0f) : Lit;
			Part(s.m_pName, s.m_X, s.m_Y, s.m_W * Grow, s.m_H * Grow, s.m_Rot + m_SacAngle, Glow);
		}
	}
	Part(s_MatriarchAbdomen.m_pName, s_MatriarchAbdomen.m_X, s_MatriarchAbdomen.m_Y, s_MatriarchAbdomen.m_W,
		s_MatriarchAbdomen.m_H, 0.0f, Lit);
	Part(s_MatriarchThorax.m_pName, s_MatriarchThorax.m_X, s_MatriarchThorax.m_Y, s_MatriarchThorax.m_W,
		s_MatriarchThorax.m_H, 0.0f, Lit);

	// Mandibles pivot at the top; broken fangs droop and darken.
	const float Open = In.m_FangsBroken ? 0.1f : m_Mandible;
	const vec4 FangColor = In.m_FangsBroken ? vec4(0.55f, 0.5f, 0.45f, 1.0f) : Lit;
	const float FangLen = In.m_FangsBroken ? 0.7f : 1.0f;
	Hung("mandible", MATRIARCH_MANDIBLE_X - 8.0f, MATRIARCH_MANDIBLE_Y - 22.0f, MATRIARCH_MANDIBLE_W * 0.9f,
		MATRIARCH_MANDIBLE_H * 0.9f * FangLen, 0.25f + Open * 0.5f, vec4(FangColor.r * 0.6f, FangColor.g * 0.6f, FangColor.b * 0.6f, 1.0f));
	Part(s_MatriarchHead.m_pName, s_MatriarchHead.m_X, s_MatriarchHead.m_Y, s_MatriarchHead.m_W, s_MatriarchHead.m_H, 0.0f, Lit);

	// Near side legs over the body.
	for(int i = 3; i < 6; i++)
		Leg(i, Lit);
	Hung("mandible", MATRIARCH_MANDIBLE_X + 4.0f, MATRIARCH_MANDIBLE_Y - 20.0f, MATRIARCH_MANDIBLE_W,
		MATRIARCH_MANDIBLE_H * FangLen, -0.1f - Open * 0.45f, FangColor);
	(void)pGraphics;
}
