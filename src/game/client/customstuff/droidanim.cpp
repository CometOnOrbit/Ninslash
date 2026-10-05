#include <game/client/gameclient.h>
#include <game/client/droid_visual.h>
#include <game/foundry_warden.h>
#include <game/abyss_angler.h>
#include "droidanim.h"

CDroidAnim::CDroidAnim(CGameClient *pClient)
{
	m_pClient = pClient;
	Reset();
}

CDroidAnim::~CDroidAnim()
{
}

void CDroidAnim::Reset()
{
	m_Status = 0;
	m_Type = 0;

	for(int i = 0; i < NUM_DROID_VALUE; i++)
		m_aValue[i] = 0.0f;

	for(int i = 0; i < NUM_DROID_VECTOR_VALUE; i++)
		m_aVectorValue[i] = vec2(0, 0);

	m_Dir = 1;
	m_Pos = vec2(0, 0);
	m_Vel = vec2(0, 0);

	m_aLegTargetPos[0] = vec2(-50, 30);
	m_aLegTargetPos[1] = vec2(-30, 40);
	m_aLegTargetPos[2] = vec2(30, 40);
	m_aLegTargetPos[3] = vec2(50, 30);

	for(int i = 0; i < 4; i++)
	{
		m_aLegAngle[i] = 0.0f;
		m_aLegTargetAngle[i] = 0.0f;
		m_aLegVel[i] = vec2(0, 0);
		m_aLegPos[i] = vec2(0, 0);
		m_aLegFrom[i] = vec2(0, 0);
		m_aLegStep[i] = 0.0f;
	}
	m_BodyOffset = vec2(0, 0);
	m_BodyOffsetVel = vec2(0, 0);
	m_BodyTilt = 0.0f;
	m_Shake = 0.0f;
	m_JetPuffs = 0;
	m_PrevVelX = 0.0f;
	m_ClipTime = 0.0f;

	m_Anim = DROIDANIM_IDLE;
	m_Angle = 0;
	m_DisplayAngle = 0;
	m_TargetDisplayAngle = 0;
	m_LocomotionTime = 0.0f;
	m_SmoothedAimAngle = 0.0f;
	m_RenderInitialized = false;
}

void CDroidAnim::OnWardenClip(int Act, float Time)
{
	if(Act == WARDEN_ACT_SLAM && m_ClipTime < 1.12f && Time >= 1.12f)
	{
		m_Shake = 1.0f;
		m_BodyOffsetVel.y += 7.0f;
	}
	else if(Act == WARDEN_ACT_CLAMP && m_ClipTime < 0.775f && Time >= 0.775f)
		m_Shake = max(m_Shake, 0.45f);
	else if(Act == WARDEN_ACT_ENRAGE && Time > 0.5f && Time < 1.8f)
		m_Shake = max(m_Shake, 0.35f);
	else if(Act == WARDEN_ACT_STAGGER && Time < m_ClipTime)
	{
		m_Shake = max(m_Shake, 0.6f);
		m_BodyOffsetVel.x -= m_Dir * 3.0f;
	}
	m_ClipTime = Time;
}

// Runs at the fixed CCustomStuff rate (120 Hz), not per frame.
void CDroidAnim::TickWarden()
{
	const float Scale = DroidVisual(m_Type).m_Scale;
	const float Sign = m_Dir == 1 ? -1.0f : 1.0f;
	const vec2 Ground = m_Pos + vec2(0.0f, 64.0f);
	const float Lead = clamp(m_Vel.x * 2.0f, -14.0f, 14.0f);
	const float StepDist = WARDEN_STEP_DIST * Scale;
	const float Rate = 0.045f + min(fabsf(m_Vel.x), 12.0f) * 0.006f;

	// Charge: the legs fold up under the hips and fire as thrusters.
	if(m_Anim == DROIDANIM_CHARGE)
	{
		for(int i = 0; i < 4; i++)
		{
			const vec2 Tuck = Ground + vec2(s_aWardenHipX[i] * 1.3f * Sign, s_aWardenHipY[i] + 80.0f) * Scale;
			m_aLegStep[i] = 0.0f;
			m_aLegPos[i] += (Tuck - m_aLegPos[i]) * 0.25f;
		}
		const float Speed = length(m_Vel);
		m_JetPuffs = min(m_JetPuffs + 1, 8);
		m_BodyTilt += (clamp(m_Vel.x * 0.015f, -0.35f, 0.35f) - m_BodyTilt) * 0.1f;
		m_BodyOffset *= 0.9f;
		m_BodyOffsetVel = vec2(0.0f, 0.0f);
		m_Shake = max(m_Shake * 0.9f, Speed > 6.0f ? 0.25f : 0.1f);
		return;
	}

	vec2 aRest[4];
	bool aGrounded[4];
	for(int i = 0; i < 4; i++)
	{
		const float x = Ground.x + s_aWardenFootX[i] * Scale * Sign + Lead;
		vec2 Hit;
		aGrounded[i] = Collision()->IntersectLine(vec2(x, Ground.y - 56.0f), vec2(x, Ground.y + 72.0f), 0, &Hit, false, true) != 0;
		aRest[i] = aGrounded[i] ? Hit : vec2(x, Ground.y + 18.0f);
	}

	int aStepping[2] = {0, 0};
	for(int i = 0; i < 4; i++)
		if(m_aLegStep[i] > 0.0f)
			aStepping[i & 1]++;

	int Landed = 0;
	for(int i = 0; i < 4; i++)
	{
		const int Pair = i & 1;
		if(m_aLegStep[i] <= 0.0f)
		{
			if((m_aLegPos[i].x == 0.0f && m_aLegPos[i].y == 0.0f) || distance(m_aLegPos[i], aRest[i]) > 200.0f)
				m_aLegPos[i] = aRest[i];
			else if(!aGrounded[i])
				m_aLegPos[i] += (aRest[i] - m_aLegPos[i]) * 0.2f;
			else if(distance(m_aLegPos[i], aRest[i]) > StepDist && !aStepping[Pair ^ 1])
			{
				m_aLegFrom[i] = m_aLegPos[i];
				m_aLegStep[i] = 0.0001f;
				aStepping[Pair]++;
			}
		}
		if(m_aLegStep[i] > 0.0f)
		{
			m_aLegStep[i] += Rate;
			const float t = min(m_aLegStep[i], 1.0f);
			const float Ease = t * t * (3.0f - 2.0f * t);
			m_aLegTargetPos[i] = aRest[i];
			m_aLegPos[i] = mix(m_aLegFrom[i], aRest[i], Ease) - vec2(0.0f, sinf(t * pi) * WARDEN_STEP_LIFT * Scale);
			if(m_aLegStep[i] >= 1.0f)
			{
				m_aLegStep[i] = 0.0f;
				m_aLegPos[i] = aRest[i];
				if(aGrounded[i])
					Landed++;
			}
		}
	}

	// Body rides the feet: tilt follows the ground line, dips on each footfall, leans into speed.
	vec2 Left(0, 0), Right(0, 0);
	float Lifted = 0.0f, FootY = 0.0f;
	for(int i = 0; i < 4; i++)
	{
		FootY += m_aLegPos[i].y;
		if(m_aLegStep[i] > 0.0f)
			Lifted += sinf(min(m_aLegStep[i], 1.0f) * pi);
		if((i < 2) == (Sign > 0.0f))
			Left += m_aLegPos[i] * 0.5f;
		else
			Right += m_aLegPos[i] * 0.5f;
	}
	FootY *= 0.25f;
	const float Accel = m_Vel.x - m_PrevVelX;
	m_PrevVelX = m_Vel.x;

	const float Slope = Right.x - Left.x > 1.0f ? atan2f(Right.y - Left.y, Right.x - Left.x) : 0.0f;
	const float TargetTilt = clamp(Slope * 0.7f, -0.35f, 0.35f) + clamp(m_Vel.x * 0.012f, -0.1f, 0.1f);
	m_BodyTilt += (TargetTilt - m_BodyTilt) * 0.08f;

	const vec2 TargetOffset(0.0f, clamp(FootY - Ground.y, -40.0f, 40.0f) - Lifted * 2.5f * Scale);
	m_BodyOffsetVel += (TargetOffset - m_BodyOffset) * 0.06f;
	m_BodyOffsetVel.x -= Accel * 0.6f;
	m_BodyOffsetVel.y += Landed * 1.4f * Scale;
	m_BodyOffsetVel *= 0.84f;
	m_BodyOffset += m_BodyOffsetVel;
	m_BodyOffset.x = clamp(m_BodyOffset.x, -18.0f, 18.0f);
	m_BodyOffset.y = clamp(m_BodyOffset.y, -40.0f, 40.0f);

	m_Shake = max(0.0f, m_Shake * 0.9f - 0.002f) + Landed * 0.06f;
}

void CDroidAnim::TickAngler()
{
	const float Target = clamp(m_Vel.x * 0.03f + m_Vel.y * 0.012f, -0.5f, 0.5f);
	m_BodyTilt += (Target - m_BodyTilt) * 0.1f;
	m_Shake = max(0.0f, m_Shake * 0.88f - 0.002f);
}

void CDroidAnim::OnAnglerClip(int Act, float Time)
{
	if(Act == ANGLER_ACT_BITE && m_ClipTime < 0.62f && Time >= 0.62f)
		m_Shake = max(m_Shake, 0.55f);
	else if(Act == ANGLER_ACT_DIVE && m_ClipTime < 1.1f && Time >= 1.1f)
		m_Shake = max(m_Shake, 0.7f);
	else if(Act == ANGLER_ACT_ROAR && Time > 0.4f && Time < 1.6f)
		m_Shake = max(m_Shake, 0.25f);
	else if(Act == ANGLER_ACT_STAGGER && Time < m_ClipTime)
		m_Shake = max(m_Shake, 0.5f);
	m_ClipTime = Time;
}

void CDroidAnim::Tick()
{
    if(DroidVisual(m_Type).m_Draw == DROID_DRAW_BOSSV5 || DroidVisual(m_Type).m_Draw == DROID_DRAW_MATRIARCH) return;
	if(!m_pClient || !m_Type)
		return;
	if(DroidVisual(m_Type).m_Draw == DROID_DRAW_WARDEN)
	{
		if(m_Status != DROIDSTATUS_TERMINATED)
			TickWarden();
		return;
	}
	if(DroidVisual(m_Type).m_Draw == DROID_DRAW_ANGLER)
	{
		if(m_Status != DROIDSTATUS_TERMINATED)
			TickAngler();
		return;
	}

	/*
	m_aLegTargetPos[0] = m_Pos + vec2(-50, 120);
	m_aLegTargetPos[1] = m_Pos + vec2(-30, 140);
	m_aLegTargetPos[2] = m_Pos + vec2(30, 140);
	m_aLegTargetPos[3] = m_Pos + vec2(50, 120);
	*/

	const CDroidVisual &Visual = DroidVisual(m_Type);
	float Scale = Visual.m_Draw == DROID_DRAW_CRAWLER ? Visual.m_Scale : 1.0f;

	const float la = m_Angle * 1.4f;
	const float la2 = m_Angle * 1.4f + pi;
	const float la3 = m_Angle * 2.7f;
	const float la4 = m_Angle * 2.7f + pi;

	const float OffX1 = -cos(la) * 30;
	const float OffY1 = sin(la) * 50;
	const float OffX2 = -cos(la2) * 30;
	const float OffY2 = sin(la2) * 50;

	if(m_Anim == DROIDANIM_IDLE || m_Anim == DROIDANIM_MOVE)
	{

		m_aLegTargetPos[0] = m_Pos + vec2(-50 + OffX1, 64 + OffY1) * Scale;
		m_aLegTargetPos[1] = m_Pos + vec2(-30 + OffX2, 64 + OffY2) * Scale;
		m_aLegTargetPos[2] = m_Pos + vec2(30 + OffX2, 64 + OffY2) * Scale;
		m_aLegTargetPos[3] = m_Pos + vec2(50 + OffX1, 64 + OffY1) * Scale;
	}
	else if(m_Anim == DROIDANIM_ATTACK)
	{
		if(m_Dir == 1)
		{
			if(m_Vel.x > 0)
			{
				const float OffX3 = -cos(la3) * 64;
				const float OffY3 = sin(la3) * 50;
				const float OffX4 = -cos(la4) * 64;
				const float OffY4 = sin(la4) * 50;

				m_aLegTargetPos[0] = m_Pos + vec2(-50 + OffX3, 64 + OffY3) * Scale;
				m_aLegTargetPos[1] = m_Pos + vec2(-30 + OffX4, 64 + OffY4) * Scale;
				m_aLegTargetPos[2] = m_Pos + vec2(20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[3] = m_Pos + vec2(40 + OffX1, 64 + OffY1) * Scale;
			}
			else
			{
				m_aLegTargetPos[0] = m_Pos + vec2(-40 + OffX1, 64 + OffY1) * Scale;
				m_aLegTargetPos[1] = m_Pos + vec2(-20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[2] = m_Pos + vec2(20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[3] = m_Pos + vec2(40 + OffX1, 64 + OffY1) * Scale;
			}
		}
		else
		{
			if(m_Vel.x < 0)
			{
				const float OffX3 = sin(la3) * 64;
				const float OffY3 = cos(la3) * 50;
				const float OffX4 = sin(la4) * 64;
				const float OffY4 = cos(la4) * 50;

				m_aLegTargetPos[0] = m_Pos + vec2(-40 + OffX1, 64 + OffY1) * Scale;
				m_aLegTargetPos[1] = m_Pos + vec2(-20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[2] = m_Pos + vec2(30 + OffX3, 64 + OffY3) * Scale;
				m_aLegTargetPos[3] = m_Pos + vec2(50 + OffX4, 64 + OffY4) * Scale;
			}
			else
			{
				m_aLegTargetPos[0] = m_Pos + vec2(-40 + OffX1, 64 + OffY1) * Scale;
				m_aLegTargetPos[1] = m_Pos + vec2(-20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[2] = m_Pos + vec2(20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[3] = m_Pos + vec2(40 + OffX1, 64 + OffY1) * Scale;
			}
		}
	}
	else if(m_Anim == DROIDANIM_JUMPATTACK)
	{
		if(m_Dir == 1)
		{
			if(m_Vel.x > 0)
			{
				const float OffX3 = -cos(la3) * 60;
				const float OffY3 = sin(la3) * 40;
				const float OffX4 = -cos(la4) * 60;
				const float OffY4 = sin(la4) * 40;

				m_aLegTargetPos[0] = m_Pos + vec2(-50 + OffX3, 64 + OffY3) * Scale;
				m_aLegTargetPos[1] = m_Pos + vec2(-30 + OffX4, 64 + OffY4) * Scale;
				m_aLegTargetPos[2] = m_Pos + vec2(20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[3] = m_Pos + vec2(40 + OffX1, 64 + OffY1) * Scale;
			}
			else
			{
				m_aLegTargetPos[0] = m_Pos + vec2(-40 + OffX1, 64 + OffY1) * Scale;
				m_aLegTargetPos[1] = m_Pos + vec2(-20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[2] = m_Pos + vec2(20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[3] = m_Pos + vec2(40 + OffX1, 64 + OffY1) * Scale;
			}
		}
		else
		{
			if(m_Vel.x < 0)
			{
				const float OffX3 = sin(la3) * 60;
				const float OffY3 = cos(la3) * 40;
				const float OffX4 = sin(la4) * 60;
				const float OffY4 = cos(la4) * 40;

				m_aLegTargetPos[0] = m_Pos + vec2(-40 + OffX1, 64 + OffY1) * Scale;
				m_aLegTargetPos[1] = m_Pos + vec2(-20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[2] = m_Pos + vec2(30 + OffX3, 64 + OffY3) * Scale;
				m_aLegTargetPos[3] = m_Pos + vec2(50 + OffX4, 64 + OffY4) * Scale;
			}
			else
			{
				m_aLegTargetPos[0] = m_Pos + vec2(-40 + OffX1, 64 + OffY1) * Scale;
				m_aLegTargetPos[1] = m_Pos + vec2(-20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[2] = m_Pos + vec2(20 + OffX2, 64 + OffY2) * Scale;
				m_aLegTargetPos[3] = m_Pos + vec2(40 + OffX1, 64 + OffY1) * Scale;
			}
		}
	}

	m_Angle += 0.01f * clamp(m_Vel.x / Scale, -6.0f, 6.0f);

	m_TargetDisplayAngle =
		(((m_aLegPos[2].y + m_aLegPos[3].y) - (m_aLegPos[0].y + m_aLegPos[1].y)) * 0.01f - m_Vel.x * 0.02f) / Scale;
	m_DisplayAngle -= (m_DisplayAngle - m_TargetDisplayAngle) / 6.0f;

	for(int i = 0; i < 4; i++)
	{
		// m_aLegAngle[i] += (i+1)*0.003f;

		m_aLegTargetAngle[i] = m_DisplayAngle * 0.75f;

		if(i == 0 || i == 1)
		{
			if(m_Vel.x > 0 && m_Dir == 1 && m_Anim == DROIDANIM_JUMPATTACK)
				m_aLegTargetAngle[i] += 1.0f - (i == 0 ? sin(la3 - 0.2f) * 1.2f : sin(la4 - 0.2f) * 1.2f);
			else if(m_Vel.x > 0 && m_Dir == 1 && m_Anim == DROIDANIM_ATTACK)
				m_aLegTargetAngle[i] += 1.0f - (i == 0 ? sin(la3 - 0.2f) * 1.2f : sin(la4 - 0.2f) * 1.2f);
		}
		else
		{
			if(m_Vel.x < 0 && m_Dir == -1 && m_Anim == DROIDANIM_JUMPATTACK)
				m_aLegTargetAngle[i] -= 1.0f - (i == 2 ? cos(la3 + 0.2f) * 1.2f : cos(la4 + 0.2f) * 1.2f);
			else if(m_Vel.x < 0 && m_Dir == -1 && m_Anim == DROIDANIM_ATTACK)
				m_aLegTargetAngle[i] -= 1.0f - (i == 2 ? cos(la3 + 0.2f) * 1.2f : cos(la4 + 0.2f) * 1.2f);
		}

		m_aLegAngle[i] -= (m_aLegAngle[i] - m_aLegTargetAngle[i]) / 4.0f;

		/*
		if (distance(m_aLegPos[i], m_aLegTargetPos[i]) > 80)
		{
			//m_aLegPos[i] = m_aLegTargetPos[i];
			m_aLegPos[i] = m_aLegTargetPos[i] + normalize(m_aLegPos[i]-m_aLegTargetPos[i]) * 80.0f;
			//m_aLegVel[i] = vec2(0, 0);
		}
		*/

		if(abs(m_aLegPos[i].x - m_aLegTargetPos[i].x) > 120 * Scale ||
		   abs(m_aLegPos[i].y - m_aLegTargetPos[i].y) > 120 * Scale)
			m_aLegPos[i] = m_aLegTargetPos[i];

		if(m_aLegPos[i].x - m_aLegTargetPos[i].x < -70.0f * Scale)
			m_aLegPos[i].x = m_aLegTargetPos[i].x - 70 * Scale;
		if(m_aLegPos[i].x - m_aLegTargetPos[i].x > 70.0f * Scale)
			m_aLegPos[i].x = m_aLegTargetPos[i].x + 70 * Scale;
		if(m_aLegPos[i].y - m_aLegTargetPos[i].y < -70.0f * Scale)
			m_aLegPos[i].y = m_aLegTargetPos[i].y - 70 * Scale;
		if(m_aLegPos[i].y - m_aLegTargetPos[i].y > 70.0f * Scale)
			m_aLegPos[i].y = m_aLegTargetPos[i].y + 70 * Scale;

		m_aLegVel[i] -= (m_aLegPos[i] - m_aLegTargetPos[i]) / 5.0f;
		// m_aLegVel[i].y += 0.05f;
		// m_aLegVel[i] += vec2(sin(m_Angle+i*0.7f), cos(m_Angle+i*0.7f)) * 6.0f;
		// m_aLegVel[i].y -= m_Vel.y*0.15f;
		m_aLegVel[i] *= 0.9f;

		if(length(m_aLegVel[i]) > 10.0f * Scale)
			m_aLegVel[i] = normalize(m_aLegVel[i]) * 10.0f * Scale;

		// m_aLegPos[i] += m_aLegVel[i];

		// Collision()->MovePoint(&m_aLegPos[i], &m_aLegVel[i], 0.0f, 0);

		vec2 o = vec2(0, 0); // vec2(sin(m_aLegAngle[i]), cos(m_aLegAngle[i])) * 34.0f;

		vec2 p = m_aLegPos[i] + o;

		if(m_aLegVel[i].y < 0)
		{
			float VelY = m_aLegVel[i].y;
			Collision()->MoveBox(&p, &m_aLegVel[i], vec2(10, 10) * Scale, 0.0f, false);

			if(m_aLegVel[i].y != VelY)
			{
				m_aLegVel[i].y = VelY;
				p.y += -10; // VelY*10.0f;
			}
		}
		else
			Collision()->MoveBox(&p, &m_aLegVel[i], vec2(10, 10) * Scale, 0.0f, false);

		// Collision()->IntersectLine(m_Pos, p, 0, &p);

		m_aLegPos[i] = p - o;
	}
}
