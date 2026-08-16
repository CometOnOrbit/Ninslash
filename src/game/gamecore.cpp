

#include "gamecore.h"

const char *CTuningParams::m_apNames[] = {
#define MACRO_TUNING_PARAM(Name, ScriptName, Value) #ScriptName,
#include "tuning.h"
#undef MACRO_TUNING_PARAM
};

bool CTuningParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuningParams::Get(int Index, float *pValue)
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuningParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Set(i, Value);
	return false;
}

bool CTuningParams::Get(const char *pName, float *pValue)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Get(i, pValue);

	return false;
}

float VelocityRamp(float Value, float Start, float Range, float Curvature)
{
	if(Value < Start)
		return 1.0f;
	return 1.0f / powf(Curvature, (Value - Start) / Range);
}

CCharacterCore::CCharacterCore()
{
	Init(0, 0);
	Reset();
}

void CCharacterCore::Init(CWorldCore *pWorld, CCollision *pCollision)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;
	m_MoveSpeedMultiplier = 1.0f;
}

void CCharacterCore::Reset()
{
	m_DamageTick = 0;
	m_Pos = vec2(0, 0);
	m_Vel = vec2(0, 0);
	m_JumpTimer = 0;
	m_Jumped = 0;
	m_CoyoteTime = 0;
	m_JumpBufferTime = 0;
	m_PrevJumpInput = 0;
	m_LandingVelocity = 0.0f;
	m_Sliding = 0;
	m_Jetpack = 0;
	m_JetpackPower = 200;
	m_Wallrun = 0;
	m_Roll = 0;
	m_Slide = 0;
	m_Status = 0;
	m_Status |= 1 << STATUS_SPAWNING;
	m_TriggeredEvents = 0;
	m_Health = 100;
	m_MoveSpeedMultiplier = 1.0f;

	m_HookPos = vec2(0, 0);
	m_HookDir = vec2(0, 0);
	m_HookTick = 0;
	m_HookState = HOOK_IDLE;
	m_HookedPlayer = -1;

	m_Action = 0;
	m_ActionState = 0;
	m_PlayerCollision = false;
	m_MonsterDamage = false;
	m_FluidDamage = false;

	m_DashTimer = 0;
	m_DashAngle = 0;

	m_ClientID = -1;
	m_KickDamage = -1;

	m_HandJetpack = false;
	m_OnWall = false;

	m_Direction = 0;
	m_Down = 0;
	m_Charge = 0;
	m_ChargeLevel = 0;
	m_Angle = 0;
	m_Anim = 0;
	m_LockDirection = 0;
}

bool CCharacterCore::IsGrounded()
{
	float PhysSize = 28.0f;

	if(m_Sliding)
		return true;

	const bool Down = PlatformState();
	for(int i = -PhysSize / 2; i <= PhysSize / 2; i++)
	{
		if(m_pCollision->CheckPoint(m_Pos.x + i, m_Pos.y + PhysSize / 2 + 5, false, Down))
		{
			return true;
		}
	}

	return false;
}

bool CCharacterCore::PlatformState()
{
	float PhysSize = 28.0f;

	if(m_pCollision->IsPlatform(m_Pos.x - PhysSize / 2, m_Pos.y + PhysSize / 2 + 32) ||
	   m_pCollision->IsPlatform(m_Pos.x + PhysSize / 2, m_Pos.y + PhysSize / 2 + 32))
		return m_Slide == 0 && m_Roll == 0 && absolute(m_Vel.x) < 5.0f && m_Down;

	return true;
}

int CCharacterCore::IsOnForceTile()
{
	if(m_Sliding)
		return true;

	const float HalfSize = 14.0f;
	const float SampleY = m_Pos.y + HalfSize + 5.0f;
	const float LeftX = m_Pos.x - HalfSize;
	const float RightX = m_Pos.x + HalfSize;
	return m_pCollision->IsForceTile(LeftX, RightX, SampleY);
}

bool CCharacterCore::IsInFluid()
{
	return m_pCollision->IsInFluid(m_Pos.x, m_Pos.y);
}

int CCharacterCore::SlopeState()
{
	float PhysSize = 28.0f;

	int tmp = 0;
	int height_left = 0;
	for(int x = -1; x <= -1; x++)
		for(int y = 0; y <= 4; y++)
			if((tmp = m_pCollision->CheckPoint(m_Pos.x - PhysSize / 2 + x, m_Pos.y + PhysSize / 2 + y)) > height_left)
			{
				height_left = tmp;
			}

	int height_right = 0;
	for(int x = 1; x <= 1; x++)
		for(int y = 0; y <= 4; y++)
			if((tmp = m_pCollision->CheckPoint(m_Pos.x + PhysSize / 2 + x, m_Pos.y + PhysSize / 2 + y)) > height_right)
			{
				height_right = tmp;
			}

	if(height_left == CCollision::SS_COL_RL)
	{
		// std::cerr << "RET 1" << std::endl;
		return 1;
	}
	else if(height_right == CCollision::SS_COL_RR)
	{
		// std::cerr << "RET -1" << std::endl;
		return -1;
	}
	// std::cerr << "RET 0" << std::endl;
	return 0;
}

void CCharacterCore::Tick(bool UseInput)
{
	m_PlayerCollision = false;
	m_MonsterDamage = false;
	m_KickDamage = -1;
	m_FluidDamage = false;
	m_HandJetpack = false;

	int s = m_Status;
	if(s & (1 << STATUS_DEATHRAY))
	{
		m_Vel = vec2(0, 0);
		return;
	}

	if(Status(STATUS_SPAWNING))
		return;

	float PhysSize = 28.0f;
	m_TriggeredEvents = 0;

	// get ground state

	bool Grounded = IsGrounded();
	bool InFluid = IsInFluid();
	int slope = SlopeState();

	/*
	bool Grounded = false;
	if(m_pCollision->CheckPoint(m_Pos.x+PhysSize/2, m_Pos.y+PhysSize/2+5))
		Grounded = true;
	if(m_pCollision->CheckPoint(m_Pos.x-PhysSize/2, m_Pos.y+PhysSize/2+5))
		Grounded = true;
	*/

	vec2 TargetDirection(0, 0);
	if(UseInput)
		TargetDirection = normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY));

	float ControlSpeed = m_pWorld->m_Tuning.m_ControlSpeed;

	float MaxSpeed = (Grounded ? m_pWorld->m_Tuning.m_GroundControlSpeed : m_pWorld->m_Tuning.m_AirControlSpeed) *
					 ControlSpeed * max(0.1f, m_MoveSpeedMultiplier);
	float Accel = (Grounded ? m_pWorld->m_Tuning.m_GroundControlAccel : m_pWorld->m_Tuning.m_AirControlAccel);
	float Friction = Grounded ? m_pWorld->m_Tuning.m_GroundFriction : m_pWorld->m_Tuning.m_AirFriction;
	float SlideFriction = m_pWorld->m_Tuning.m_SlideFriction;

	float SlideSlopeAcceleration = m_pWorld->m_Tuning.m_SlideSlopeAcceleration;
	float SlopeDeceleration = m_pWorld->m_Tuning.m_SlopeDeceleration;
	float SlopeAscendingControlSpeed = m_pWorld->m_Tuning.m_SlopeAscendingControlSpeed * invsqrt2;
	float SlopeDescendingControlSpeed = m_pWorld->m_Tuning.m_SlopeDescendingControlSpeed * invsqrt2;
	float SlideControlSpeed = m_pWorld->m_Tuning.m_SlideControlSpeed * invsqrt2;

	float SlideActivationSpeed = m_pWorld->m_Tuning.m_SlideActivationSpeed;

	float JumpPower = m_pWorld->m_Tuning.m_JumpPower;
	float WallrunPower = m_pWorld->m_Tuning.m_WallrunImpulse;

	float HandJetpackControlSpeed = 11.5f * ControlSpeed;
	float HandJetpackImpulse = 1.10f;
	float JetpackControlSpeed = m_pWorld->m_Tuning.m_JetpackControlSpeed * ControlSpeed;
	float JetpackControlAccel = m_pWorld->m_Tuning.m_JetpackControlAccel;

	float DashPower = float(m_pWorld->m_Tuning.m_DashPower);
	const int CoyoteTicks = max(0, round_to_int(m_pWorld->m_Tuning.m_JumpCoyoteTicks));
	const int JumpBufferTicks = max(0, round_to_int(m_pWorld->m_Tuning.m_JumpBufferTicks));
	const int WallJumpDelay = max(1, round_to_int(m_pWorld->m_Tuning.m_WallJumpDelayTicks));
	const int WallJumpLockTicks = max(0, round_to_int(m_pWorld->m_Tuning.m_WallJumpDirectionLockTicks));
	const float WallJumpHorizontalImpulse = m_pWorld->m_Tuning.m_WallJumpHorizontalImpulse;
	m_LandingVelocity = 0.0f;
	if(Grounded && m_Vel.y >= 0.0f)
		m_CoyoteTime = CoyoteTicks;
	const bool JumpPressed = UseInput && m_Input.m_Jump && !m_PrevJumpInput;
	bool BufferedJumpThisTick = false;

	m_OnWall = false;

	// rage
	if(m_Status & (1 << STATUS_DASH))
	{
		Friction /= 1.4f;
		MaxSpeed *= 1.4f;
		Accel *= 1.4f;
		JumpPower *= 1.1f;
		WallrunPower *= 1.3f;
		HandJetpackControlSpeed *= 1.3f;
	}

	if(m_Status & (1 << STATUS_SLOWMOVING))
	{
		MaxSpeed *= 0.8f;
		Accel *= 0.8f;
		JumpPower *= 0.8f;
		WallrunPower *= 0.8f;
		HandJetpackControlSpeed *= 0.8f;
		JetpackControlSpeed *= 0.8f;
		SlideControlSpeed *= 0.8f;
		JetpackControlAccel *= 0.8f;
		HandJetpackImpulse *= 0.8f;
	}

	int Mask = m_Status >> STATUS_MASK1;

	if(Mask == 2)
	{
		Friction /= 1.1f;
		MaxSpeed *= 1.15f;
		Accel *= 1.1f;
		JumpPower *= 1.15f;
		WallrunPower *= 1.15f;
		HandJetpackControlSpeed *= 1.15f;
		HandJetpackImpulse *= 1.1f;
		JetpackControlSpeed *= 1.15f;
		SlideControlSpeed *= 1.15f;
		JetpackControlAccel *= 1.1f;
		DashPower *= 1.15f;
	}

	if(m_Slide > 0)
		HandJetpackControlSpeed *= 1.2f;

	// gravity & jump physics
	if(m_Action == COREACTION_JUMP)
	{
		// sharper jump upwards
		if(m_ActionState++ > 2)
			m_Vel.y += m_pWorld->m_Tuning.m_Gravity;
	}
	else if(m_Action == COREACTION_SLIDEKICK)
	{
		if(m_ActionState < 8)
			m_Vel.y += m_pWorld->m_Tuning.m_Gravity * 2;
	}
	else if(m_Action == COREACTION_JUMPPAD)
	{
		// sharper jump upwards
		if(m_ActionState++ > 8)
			m_Vel.y += m_pWorld->m_Tuning.m_Gravity;
		else if(m_ActionState < 6)
			m_Vel.y -= 2.0f;
	}
	else if(m_Action == COREACTION_WALLJUMP)
	{
		int Dir = m_ActionState > 0 ? 1 : -1;
		int State = abs(m_ActionState);

		if(State < WallJumpDelay)
		{
			m_Vel.y = 0.0f;
		}
		else if(State == WallJumpDelay)
		{
			m_Vel.y = -JumpPower;
			m_Vel.x = WallJumpHorizontalImpulse * Dir + m_Input.m_Direction * 3.0f;
			m_LockDirection = WallJumpLockTicks * Dir;
			m_CoyoteTime = 0;
			m_JumpBufferTime = 0;
			m_TriggeredEvents |= COREEVENT_WALL_JUMP;
		}
		else if(State < WallJumpDelay + 4)
		{
			// no gravity
		}
		else
		{
			m_Vel.y += m_pWorld->m_Tuning.m_Gravity;
		}

		m_ActionState += Dir;
	}
	else
	{
		m_Vel.y += m_pWorld->m_Tuning.m_Gravity;
	}

	m_Anim = 0;

	bool LoadJetpack = false;

	// fill jetpack
	if(Grounded)
	{
		LoadJetpack = true;

		if(m_Wallrun < -10 || m_Wallrun > 10)
			m_Wallrun = 0;
	}

	int ForceTileStatus = IsOnForceTile() * 4;

	// handle input
	if(UseInput)
	{
		m_Direction = m_Input.m_Direction;
		m_Down = m_Input.m_Down;
		if(JumpPressed && !Grounded && !InFluid && m_Vel.y > 0.0f)
		{
			m_JumpBufferTime = JumpBufferTicks;
			BufferedJumpThisTick = true;
		}

		m_Charge = m_Input.m_Charge;

		if(m_ChargeLevel < 0)
			m_ChargeLevel++;

		/*
		if (m_ChargeLevel < 0)
			m_ChargeLevel++;
		else
		{
			if (m_Charge)
				m_ChargeLevel = min(m_ChargeLevel+2, 100);
			else if (m_ChargeLevel > 80)
				m_ChargeLevel = -m_ChargeLevel/2;
			else
				m_ChargeLevel = max(m_ChargeLevel-2, 0);

		}
		*/

		// sliding
		Slide(Grounded, ForceTileStatus / 4);

		// go down faster while holding down (default key: s)
		// if (!Grounded && m_Down && m_Vel.y < MaxSpeed*1.0f)
		//	m_Vel.y += 0.4f;

		if(m_LockDirection > 0)
		{
			m_LockDirection--;
			m_Direction = 1;
		}
		if(m_LockDirection < 0)
		{
			m_LockDirection++;
			m_Direction = -1;
		}

		// wall climbing
		if(!Grounded && m_Jetpack == 0)
		{
			// falling down
			if(m_Vel.y > -2.5f || m_Direction == 0)
			{
				if(m_Wallrun > 10 && m_Wallrun < 25)
					m_Wallrun++;
				else if(m_Wallrun < -10 && m_Wallrun > -25)
					m_Wallrun--;
				else
					m_Wallrun = 0;

				if(m_Direction < 0 && m_pCollision->CheckPoint(m_Pos.x - PhysSize, m_Pos.y + PhysSize / 2) &&
				   m_pCollision->CheckPoint(m_Pos.x - PhysSize, m_Pos.y - PhysSize / 2))
				{
					if((m_Input.m_Hook && m_JetpackPower > 0 && TargetDirection.y > 0) || m_Input.m_Down)
						m_Vel.y *= 0.97f;
					else if(m_Vel.y > 0.0f)
						m_Vel.y *= 0.8f;
					else
						m_Vel.y *= 0.9f;

					m_Anim = 1;

					LoadJetpack = true;
					m_OnWall = true;
				}

				if(m_Direction > 0 && m_pCollision->CheckPoint(m_Pos.x + PhysSize, m_Pos.y + PhysSize / 2) &&
				   m_pCollision->CheckPoint(m_Pos.x + PhysSize, m_Pos.y - PhysSize / 2))
				{
					if((m_Input.m_Hook && m_JetpackPower > 0 && TargetDirection.y > 0) || m_Input.m_Down)
						m_Vel.y *= 0.97f;
					else if(m_Vel.y > 0.0f)
						m_Vel.y *= 0.8f;
					else
						m_Vel.y *= 0.9f;

					m_Anim = -1;

					LoadJetpack = true;
					m_OnWall = true;
				}

				// wall jump / walljump
				if(m_Input.m_Jump && !(m_Jumped & 1))
				{
					if(m_pCollision->CheckPoint(m_Pos.x - (PhysSize + 6), m_Pos.y + PhysSize / 2) &&
					   m_pCollision->CheckPoint(m_Pos.x - (PhysSize + 6), m_Pos.y - PhysSize / 2))
					{
						m_Jumped |= 1;
						m_Wallrun = 11;
						m_CoyoteTime = 0;
						m_JumpBufferTime = 0;
						SetAction(COREACTION_WALLJUMP, 1);
					}

					if(m_pCollision->CheckPoint(m_Pos.x + (PhysSize + 6), m_Pos.y + PhysSize / 2) &&
					   m_pCollision->CheckPoint(m_Pos.x + (PhysSize + 6), m_Pos.y - PhysSize / 2))
					{
						m_Jumped |= 1;
						m_Wallrun = -11;
						m_CoyoteTime = 0;
						m_JumpBufferTime = 0;
						SetAction(COREACTION_WALLJUMP, -1);
					}
				}
			}

			// going up
			else
			{
				int WalljumpEndFrame = 17;
				// wallrun
				if(m_pCollision->CheckPoint(m_Pos.x - (PhysSize + 6), m_Pos.y + PhysSize / 2) &&
				   m_pCollision->CheckPoint(m_Pos.x - (PhysSize + 6), m_Pos.y - PhysSize / 2))
				{
					// run up
					if(++m_Wallrun > 5 && m_Wallrun < 11)
					{
						m_Wallrun = 1;
						m_Vel.y = -WallrunPower;
					}
					else if(m_Wallrun > WalljumpEndFrame || m_Wallrun < 0)
						m_Wallrun = 3;

					LoadJetpack = true;
					m_OnWall = true;

					// wall jump
					if(m_Input.m_Jump && !(m_Jumped & 1))
					{
						// m_TriggeredEvents |= COREEVENT_AIR_JUMP;
						m_Vel.y = -(JumpPower + 3.0f);
						m_Vel.x = WallJumpHorizontalImpulse + m_Input.m_Direction * 3.0f;
						m_Jumped |= 1;
						m_LockDirection = WallJumpLockTicks;
						m_Wallrun = 11;
						m_CoyoteTime = 0;
						m_JumpBufferTime = 0;
						m_TriggeredEvents |= COREEVENT_WALL_JUMP;
					}
				}
				else if(m_pCollision->CheckPoint(m_Pos.x + (PhysSize + 6), m_Pos.y + PhysSize / 2) &&
						m_pCollision->CheckPoint(m_Pos.x + (PhysSize + 6), m_Pos.y - PhysSize / 2))
				{
					// run up
					if(--m_Wallrun < -5 && m_Wallrun > -11)
					{
						m_Wallrun = -1;
						m_Vel.y = -WallrunPower;
					}
					else if(m_Wallrun < -WalljumpEndFrame || m_Wallrun > 0)
						m_Wallrun = -3;

					LoadJetpack = true;
					m_OnWall = true;

					// wall jump
					if(m_Input.m_Jump && !(m_Jumped & 1))
					{
						// m_TriggeredEvents |= COREEVENT_AIR_JUMP;
						m_Vel.y = -(JumpPower + 3.0f);
						m_Vel.x = -WallJumpHorizontalImpulse + m_Input.m_Direction * 3.0f;
						m_Jumped |= 1;
						m_LockDirection = -WallJumpLockTicks;
						m_Wallrun = -11;
						m_CoyoteTime = 0;
						m_JumpBufferTime = 0;
						m_TriggeredEvents |= COREEVENT_WALL_JUMP;
					}
				}
				else
				{
					if(m_Wallrun > 10 && m_Wallrun < 25)
						m_Wallrun++;
					else if(m_Wallrun < -10 && m_Wallrun > -25)
						m_Wallrun--;
					else
						m_Wallrun = 0;
				}
			}
		}

		// setup angle
		float a = 0;
		if(m_Input.m_TargetX == 0)
			a = atanf((float)m_Input.m_TargetY);
		else
			a = atanf((float)m_Input.m_TargetY / (float)m_Input.m_TargetX);

		if(m_Input.m_TargetX < 0)
			a = a + pi;

		m_Angle = (int)(a * 256.0f);

		// Ground and coyote jumps take priority over jetpack activation.
		const bool WantsGroundJump = JumpPressed || m_JumpBufferTime > 0;
		const bool CanGroundJump = Grounded || InFluid || m_CoyoteTime > 0;
		bool GroundJumped = false;
		if(m_Roll)
			m_Jetpack = 0;
		if(WantsGroundJump && CanGroundJump && !(m_Jumped & 1) && !m_Roll &&
		   !m_pCollision->CheckPoint(m_Pos.x, m_Pos.y - 64, false, true))
		{
			m_JumpTimer = m_Slide > 0 ? -6 : 6;
			m_TriggeredEvents |= COREEVENT_GROUND_JUMP;
			m_Action = COREACTION_JUMP;
			m_ActionState = 0;
			m_Vel.y = slope == 0 ? -JumpPower : -JumpPower * invsqrt2;
			m_Jumped |= 1;
			m_CoyoteTime = 0;
			m_JumpBufferTime = 0;
			GroundJumped = true;

			if(m_Slide > 0 && m_Slide < 12)
			{
				m_Action = COREACTION_SLIDEKICK;
				m_ActionState = 0;
				m_TriggeredEvents |= COREEVENT_SLIDEKICK;
			}
		}

		if(m_Input.m_Jump)
		{
			if(!GroundJumped && !m_Roll && m_JumpBufferTime == 0 && m_Jetpack == 1 && m_JetpackPower > 0 &&
			   (m_Wallrun == 0 || abs(m_Wallrun) > 15) && !(m_Action == COREACTION_SLIDEKICK && m_ActionState < 6))
			{
				m_Wallrun = 0;
				if(m_Direction == 1)
				{
					if(m_Vel.x < JetpackControlSpeed)
						m_Vel.x += 1.0f;
					if(m_Vel.y > -JetpackControlSpeed * 0.85f)
						m_Vel.y -= m_pWorld->m_Tuning.m_Gravity * JetpackControlAccel * (m_Vel.y > 0.0f ? 1.8f : 0.8f);
				}
				else if(m_Direction == -1)
				{
					if(m_Vel.x > -JetpackControlSpeed)
						m_Vel.x -= 1.0f;
					if(m_Vel.y > -JetpackControlSpeed * 0.85f)
						m_Vel.y -= m_pWorld->m_Tuning.m_Gravity * JetpackControlAccel * (m_Vel.y > 0.0f ? 1.8f : 0.8f);
				}
				else if(m_Vel.y > -JetpackControlSpeed)
					m_Vel.y -= m_pWorld->m_Tuning.m_Gravity * JetpackControlAccel * 1.4f;
				m_JetpackPower -= 1;
			}

			if(!GroundJumped && !CanGroundJump && m_JumpBufferTime == 0)
			{
				if(!m_Roll && !(m_Jumped & 1) && m_JetpackPower > 0)
					m_Jetpack = 1;

				if(!(m_Jumped & 2) && !m_Roll && m_Slide > 0 && m_Slide < 12)
				{
					m_Jumped |= 3;
					m_Action = COREACTION_SLIDEKICK;
					m_ActionState = 0;
					m_TriggeredEvents |= COREEVENT_SLIDEKICK;
					m_Vel.y = -JumpPower * 0.7f;
				}
			}
		}
		else
		{
			if(Grounded || m_OnWall)
				m_Jumped = 0;

			m_Jumped &= ~1;
			m_Jetpack = 0;
		}

		// press down on wall
		if(!m_Input.m_Hook && m_Input.m_Down && m_Vel.y < 0.0f && m_Wallrun != 0 && m_Wallrun <= 10 && m_Wallrun >= -10)
		{
			m_Wallrun = 0;
			m_Vel.y = 0.0f;
		}

		// hand turbo boost
		/*
		if(m_Input.m_Hook && m_JetpackPower > 0 && !InFluid)
		{
			if ((TargetDirection.x > 0 && m_Vel.x < HandJetpackControlSpeed + ForceTileStatus) || (TargetDirection.x < 0
		&& m_Vel.x > -HandJetpackControlSpeed + ForceTileStatus)) m_Vel.x += TargetDirection.x*HandJetpackImpulse;

			if ((TargetDirection.y > 0 && m_Vel.y < HandJetpackControlSpeed) || (TargetDirection.y < 0 && m_Vel.y >
		-HandJetpackControlSpeed)) m_Vel.y += TargetDirection.y*HandJetpackImpulse;

			if (TargetDirection.y > 0 && m_Vel.y < 0.0f && m_Wallrun != 0 && m_Wallrun <= 10 && m_Wallrun >= -10)
			{
				m_Wallrun = 0;
				m_Vel.y = 0.0f;
			}

			m_JetpackPower -= 1;
			m_HandJetpack = true;
		}
		*/

		// handle hook
		if(m_Input.m_Hook)
		{
			if(m_HookState == HOOK_IDLE)
			{
				m_HookState = HOOK_FLYING;
				m_HookPos = m_Pos + TargetDirection * PhysSize * 1.5f;
				m_HookDir = TargetDirection;
				m_HookedPlayer = -1;
				m_HookTick = 0;
				m_TriggeredEvents |= COREEVENT_HOOK_LAUNCH;
			}
		}
		else
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_IDLE;
			m_HookPos = m_Pos;
		}
		m_PrevJumpInput = m_Input.m_Jump != 0;
	}
	if(!Grounded && m_CoyoteTime > 0)
		m_CoyoteTime--;
	if(m_JumpBufferTime > 0 && !BufferedJumpThisTick)
		m_JumpBufferTime--;

	m_Sliding = false;

	if(slope != 0)
		m_Sliding = true;

	if((m_Vel.x > SlideActivationSpeed && slope == 1) || (m_Vel.x < -SlideActivationSpeed && slope == -1))
	{
		m_Sliding = true;
	}

	if(slope != 0 && m_Direction == slope && !m_Sliding)
		MaxSpeed = SlopeDescendingControlSpeed;
	else if(slope != 0 && m_Direction == -slope && !m_Sliding)
	{
		MaxSpeed = SlopeAscendingControlSpeed;
		float diff = SlopeDeceleration * fabs(m_Vel.x - MaxSpeed);

		if(m_Vel.x > MaxSpeed)
			m_Vel.x -= diff;
		if(m_Vel.x < -MaxSpeed)
			m_Vel.x += diff;
	}

	if(m_Action == COREACTION_HANG)
	{
		Accel *= 0.3f;
		MaxSpeed *= 0.3f;
	}

	// add the speed modification according to players wanted direction
	if(m_Slide == 0)
	{
		const bool Reversing = m_Direction != 0 && m_Vel.x * m_Direction < 0.0f;
		const float DirectionAccel = Accel * (Reversing ? (Grounded ? m_pWorld->m_Tuning.m_GroundReverseAccel
																	: m_pWorld->m_Tuning.m_AirReverseAccel)
														: 1.0f);
		if(m_Direction < 0) // && (!m_Sliding || !Grounded))
		{
			if(slope > 0)
				m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -DirectionAccel * 0.7f);
			else
				m_Vel.x =
					SaturatedAdd(-MaxSpeed + ForceTileStatus, MaxSpeed + ForceTileStatus, m_Vel.x, -DirectionAccel);
		}
		if(m_Direction > 0) // && (!m_Sliding || !Grounded))
		{
			if(slope < 0)
				m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, DirectionAccel * 0.7f);
			else
				m_Vel.x =
					SaturatedAdd(-MaxSpeed + ForceTileStatus, MaxSpeed + ForceTileStatus, m_Vel.x, DirectionAccel);
		}
	}

	if(m_Sliding && slope != 0)
	{
		m_Vel.x = SaturatedAdd(-SlideControlSpeed, SlideControlSpeed, m_Vel.x, slope * SlideSlopeAcceleration);
	}
	else if(m_Sliding && Grounded)
	{
		m_Vel.x *= SlideFriction;
	}
	else if(m_Direction == 0 && m_Slide == 0)
	{
		if(slope != 0 && !m_Jumped)
		{
			m_Vel.x *= Friction; // /invsqrt2;
			m_Vel.y *= Friction; // /invsqrt2;
		}
		else
		{
			m_Vel.x = (m_Vel.x + ForceTileStatus) * Friction;
		}
	}

	// handle jumping
	// 1 bit = to keep track if a jump has been made on this input
	// 2 bit = to keep track if a air-jump has been made
	// if(Grounded)
	//	m_Jumped &= ~2;

	if(LoadJetpack)
	{
		m_JetpackPower += 3;

		if(m_JetpackPower > 200)
			m_JetpackPower = 200;
	}

	// limit falling speed
	if(m_Vel.y > MaxSpeed * 2.5f)
		m_Vel.y = MaxSpeed * 2.5f;

	if(m_Roll > 0)
		m_Roll++;

	if(m_Roll > 0)
	{
		const int RollDuration = max(1, round_to_int(m_pWorld->m_Tuning.m_RollDurationTicks));
		if(m_Vel.x < -2.0f)
		{
			if(m_Roll < RollDuration)
				m_Anim = -2;
			else
			{
				m_Roll = 0;
				m_Slide = 5;
				m_Anim = -3;
			}
		}
		else if(m_Vel.x > 2.0f)
		{
			if(m_Roll < RollDuration)
				m_Anim = 2;
			else
			{
				m_Roll = 0;
				m_Slide = 5;
				m_Anim = 3;
			}
		}
		else
		{
			m_Roll = 0;
		}
	}

	// roll dash
	if(m_Roll > 4 && !m_DashTimer && JumpPressed)
	{
		const int DashDirection = m_Input.m_Direction != 0 ? m_Input.m_Direction : (m_Vel.x < 0.0f ? -1 : 1);
		m_DashTimer = 4;
		m_DashAngle = DashDirection < 0 ? round_to_int(pi * 256.0f) : 0;
		m_Jetpack = 0;
		m_JumpBufferTime = 0;

		m_JetpackPower = min(m_JetpackPower + 25, 200);
	}

	if(m_DashTimer > 0)
	{
		m_DashTimer--;

		const float DashDirection = cosf(m_DashAngle / 256.0f) < 0.0f ? -1.0f : 1.0f;
		vec2 d = vec2(DashDirection, 0.0f) * max(length(m_Vel) * 1.05f, DashPower);
		m_Vel += (d - m_Vel) / 3.0f;
	}

	if(m_JumpTimer > 0)
	{
		m_JumpTimer--;
		m_Anim = 5;
	}
	else if(m_JumpTimer < 0)
	{
		m_JumpTimer++;
		m_Anim = -5;
	}

	if(m_Action == COREACTION_JUMPPAD && m_ActionState < 28)
		m_Anim = 6;

	m_BallHitVel = vec2(0, 0);

	// do hook
	if(m_HookState == HOOK_IDLE)
	{
		m_HookedPlayer = -1;
		m_HookState = HOOK_IDLE;
		m_HookPos = m_Pos;
	}
	else if(m_HookState >= HOOK_RETRACT_START && m_HookState < HOOK_RETRACT_END)
	{
		m_HookState++;
	}
	else if(m_HookState == HOOK_RETRACT_END)
	{
		m_HookState = HOOK_RETRACTED;
		m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
		m_HookState = HOOK_RETRACTED;
	}
	else if(m_HookState == HOOK_FLYING)
	{
		vec2 NewPos = m_HookPos + m_HookDir * m_pWorld->m_Tuning.m_HookFireSpeed;
		if(distance(m_Pos, NewPos) > m_pWorld->m_Tuning.m_HookLength)
		{
			m_HookState = HOOK_RETRACT_START;
			NewPos = m_Pos + normalize(NewPos - m_Pos) * m_pWorld->m_Tuning.m_HookLength;
		}

		// make sure that the hook doesn't go though the ground
		bool GoingToHitGround = false;
		bool GoingToRetract = false;
		int Hit = m_pCollision->IntersectLine(m_HookPos, NewPos, &NewPos, 0);
		if(Hit)
			GoingToHitGround = true;

		// Check against other players first
		if(m_pWorld)
		{
			if(m_pWorld->m_Tuning.m_PlayerHooking)
			{
				float Distance = 0.0f;
				for(int i = 0; i < MAX_CHARACTERS; i++)
				{
					CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
					if(!pCharCore || pCharCore == this)
						continue;

					vec2 ClosestPoint = closest_point_on_line(m_HookPos, NewPos, pCharCore->m_Pos);
					if(distance(pCharCore->m_Pos, ClosestPoint) < PhysSize + 2.0f)
					{
						if(m_HookedPlayer == -1 || distance(m_HookPos, pCharCore->m_Pos) < Distance)
						{
							m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_PLAYER | COREEVENT_HOOK_HIT;
							m_HookState = HOOK_GRABBED;
							m_HookedPlayer = i;
							Distance = distance(m_HookPos, pCharCore->m_Pos);
						}
					}
				}
			}

			// Check for ball
			CBallCore *pBallCore = m_pWorld->m_pBall;

			if(pBallCore)
			{
				vec2 ClosestPoint = closest_point_on_line(m_HookPos, NewPos, pBallCore->m_Pos);
				if(distance(pBallCore->m_Pos, ClosestPoint) < m_pWorld->m_Tuning.m_BallSize - 20.0f)
				{
					m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_PLAYER | COREEVENT_HOOK_HIT;
					m_HookState = HOOK_GRABBEDBALL;
					pBallCore->PlayerHit();
				}
			}

			// check for droids
			if(m_pWorld->m_DroidCounter)
			{
				for(int i = 0; i < m_pWorld->m_DroidCounter; i++)
				{
					if(m_pWorld->m_aDroidRadius[i] <= 0)
						continue;

					vec2 DroidPos = m_pWorld->m_aDroidPos[i];
					int DroidRadius = m_pWorld->m_aDroidRadius[i];

					vec2 ClosestPoint = closest_point_on_line(m_HookPos, NewPos, DroidPos);
					if(distance(DroidPos, ClosestPoint) < DroidRadius)
					{
						m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_PLAYER | COREEVENT_HOOK_HIT;
						m_HookState = HOOK_GRABBEDDROID;
						m_HookedPlayer = m_pWorld->m_aDroidID[i];
						m_HookPos = ClosestPoint;
						break;
					}
				}
			}
		}

		if(m_HookState == HOOK_FLYING)
		{
			// check against ground
			if(GoingToHitGround)
			{
				m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_GROUND | COREEVENT_HOOK_HIT;
				m_HookState = HOOK_GRABBED;
			}
			else if(GoingToRetract)
			{
				m_TriggeredEvents |= COREEVENT_HOOK_HIT_NOHOOK;
				m_HookState = HOOK_RETRACT_START;
			}

			m_HookPos = NewPos;
		}
	}

	if(m_HookState == HOOK_GRABBEDDROID)
	{
		const vec2 HookDelta = m_HookPos - m_Pos;
		const float HookDistance = length(HookDelta);
		if(HookDistance > PhysSize * m_pWorld->m_Tuning.m_HookDragMinDistFactor)
		{
			const float HookForce = (HookDistance - 64.0f) * 0.001f;
			const vec2 HookDirection = HookDelta / HookDistance;
			vec2 HookVel = HookDirection * m_pWorld->m_Tuning.m_HookDragAccel * (1.0f + HookForce);
			vec2 HookImpactVel = -HookDirection * m_pWorld->m_Tuning.m_HookDragAccel * (1.0f + HookForce);
			// the hook as more power to drag you up then down.
			// this makes it easier to get on top of an platform
			if(HookVel.y > 0)
				HookVel.y *= m_pWorld->m_Tuning.m_HookDownFactor;

			if(HookImpactVel.y > 0)
				HookImpactVel.y *= 0.5f;

			HookImpactVel.x *= 0.8f;

			// the hook will boost it's power if the player wants to move
			// in that direction. otherwise it will dampen everything abit
			if((HookVel.x < 0 && m_Direction < 0) || (HookVel.x > 0 && m_Direction > 0))
				HookVel.x *= m_pWorld->m_Tuning.m_HookMoveAlongFactor;
			else
				HookVel.x *= m_pWorld->m_Tuning.m_HookMoveAgainstFactor;

			vec2 NewVel = m_Vel + HookVel;
			m_pWorld->AddDroidHookImpact(m_HookedPlayer, HookImpactVel);

			// check if we are under the legal limit for the hook
			const float NewSpeedSquared = dot(NewVel, NewVel);
			const float DragSpeedSquared = m_pWorld->m_Tuning.m_HookDragSpeed * m_pWorld->m_Tuning.m_HookDragSpeed;
			if(NewSpeedSquared < DragSpeedSquared || NewSpeedSquared < dot(m_Vel, m_Vel))
				m_Vel = NewVel; // no problem. apply
		}

		m_HookPos += m_pWorld->FindDroidVel(m_HookedPlayer);

		// release hook
		m_HookTick++;
		const int HookTargetHoldTicks =
			max(1, round_to_int(SERVER_TICK_SPEED * (float)m_pWorld->m_Tuning.m_HookTargetHoldSeconds));
		if(m_HookTick >= HookTargetHoldTicks)
		{
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}
	}

	if(m_HookState == HOOK_GRABBED || m_HookState == HOOK_GRABBEDBALL)
	{
		if(m_HookState == HOOK_GRABBEDBALL)
		{
			CBallCore *pBallCore = m_pWorld->m_pBall;

			if(pBallCore)
				m_HookPos = pBallCore->m_Pos;
			else
			{
				// release hook
				m_HookState = HOOK_RETRACTED;
				m_HookPos = m_Pos;
			}
		}
		else
		{
			if(m_HookedPlayer != -1)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[m_HookedPlayer];
				if(pCharCore)
					m_HookPos = pCharCore->m_Pos;
				else
				{
					// release hook
					m_HookedPlayer = -1;
					m_HookState = HOOK_RETRACTED;
					m_HookPos = m_Pos;
				}
			}
		}

		// don't do this hook rutine when we are hook to a player or ball
		if(m_HookState == HOOK_GRABBED && m_HookedPlayer == -1)
		{
			const vec2 HookDelta = m_HookPos - m_Pos;
			const float HookDistance = length(HookDelta);
			if(HookDistance > PhysSize * m_pWorld->m_Tuning.m_HookDragMinDistFactor)
			{
				const float HookForce = (HookDistance - 64.0f) * 0.001f;
				vec2 HookVel = HookDelta / HookDistance * m_pWorld->m_Tuning.m_HookDragAccel * (1.0f + HookForce);

				// the hook as more power to drag you up then down.
				// this makes it easier to get on top of an platform
				if(HookVel.y > 0)
					HookVel.y *= m_pWorld->m_Tuning.m_HookDownFactor;

				// the hook will boost it's power if the player wants to move
				// in that direction. otherwise it will dampen everything abit
				if((HookVel.x < 0 && m_Direction < 0) || (HookVel.x > 0 && m_Direction > 0))
					HookVel.x *= m_pWorld->m_Tuning.m_HookMoveAlongFactor;
				else
					HookVel.x *= m_pWorld->m_Tuning.m_HookMoveAgainstFactor;

				vec2 NewVel = m_Vel + HookVel;

				// check if we are under the legal limit for the hook
				const float NewSpeedSquared = dot(NewVel, NewVel);
				const float DragSpeedSquared = m_pWorld->m_Tuning.m_HookDragSpeed * m_pWorld->m_Tuning.m_HookDragSpeed;
				if(NewSpeedSquared < DragSpeedSquared || NewSpeedSquared < dot(m_Vel, m_Vel))
					m_Vel = NewVel; // no problem. apply
			}
		}

		// Terrain hooks persist until release; dynamic targets share one hold limit.
		m_HookTick++;
		const int HookTargetHoldTicks =
			max(1, round_to_int(SERVER_TICK_SPEED * (float)m_pWorld->m_Tuning.m_HookTargetHoldSeconds));
		if(m_HookedPlayer != -1 && (m_HookTick >= HookTargetHoldTicks || !m_pWorld->m_apCharacters[m_HookedPlayer]))
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}
		if(m_HookState == HOOK_GRABBEDBALL && m_HookTick >= HookTargetHoldTicks)
		{
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}
	}

	if(m_pWorld)
	{
		// ball collision
		CBallCore *pBallCore = m_pWorld->m_pBall;

		if(pBallCore)
		{
			float BallSize = m_pWorld->m_Tuning.m_BallSize;
			vec2 BPos = pBallCore->m_Pos;
			float OffsetY = -26;

			if(m_Action == COREACTION_SLIDEKICK && m_ActionState > 2 && m_ActionState < 10)
				OffsetY = -14;

			if(m_Slide != 0 || m_Roll != 0)
				OffsetY = -6;

			if(m_DashTimer > 0)
				OffsetY = -12;

			vec2 Pos = m_Pos + vec2(0, OffsetY);

			const vec2 BallDelta = Pos - BPos;
			const float Distance = length(BallDelta);
			const vec2 Dir = Distance > 0.0f ? BallDelta / Distance : vec2(0, 0);

			if(Distance <= BallSize * 0.80f + 16)
			{
				float a = ((BallSize * 0.80f + 16) - Distance);
				float Velocity = 0.5f;

				m_BallHitVel = m_Vel;

				// make sure that we don't add excess force by checking the
				// direction against the current velocity. if not zero.
				const float SpeedSquared = dot(m_Vel, m_Vel);
				if(SpeedSquared > 0.00000001f)
					Velocity = 1 - (dot(m_Vel, Dir) / sqrtf(SpeedSquared) + 1) / 2;

				m_Vel += Dir * a * (Velocity * 0.75f);
				m_Vel *= 0.85f;
			}

			// handle hook ball influence
			if(m_HookState == HOOK_GRABBEDBALL)
			{
				if(Distance > PhysSize * m_pWorld->m_Tuning.m_HookDragMinDistFactor)
				{
					float Accel = m_pWorld->m_Tuning.m_HookDragAccel * (Distance / m_pWorld->m_Tuning.m_HookLength);
					float DragSpeed = m_pWorld->m_Tuning.m_HookDragSpeed * 1.25f;

					// add force to the hooked player
					pBallCore->m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, pBallCore->m_Vel.x, Accel * Dir.x * 1.5f);
					pBallCore->m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, pBallCore->m_Vel.y, Accel * Dir.y * 1.5f);

					// add a little bit force to the guy who has the grip
					float PullX = -Accel * Dir.x * 0.3f;
					float PullY = -Accel * Dir.y * 0.3f;
					PullX *= (PullX < 0.0f && m_Direction < 0) || (PullX > 0.0f && m_Direction > 0)
								 ? m_pWorld->m_Tuning.m_HookMoveAlongFactor
								 : m_pWorld->m_Tuning.m_HookMoveAgainstFactor;
					if(PullY > 0.0f)
						PullY *= m_pWorld->m_Tuning.m_HookDownFactor;
					m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.x, PullX);
					m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.y, PullY);
				}
			}
		}

		for(int i = 0; i < MAX_CHARACTERS; i++)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
			if(!pCharCore)
				continue;

			// player *p = (player*)ent;
			if(pCharCore == this || pCharCore->Status(STATUS_SPAWNING)) // || !(p->flags&FLAG_ALIVE)
				continue;												// make sure that we don't nudge our self

			// handle player <-> player collision
			const vec2 PlayerDelta = m_Pos - pCharCore->m_Pos;
			const float DistanceSquared = dot(PlayerDelta, PlayerDelta);
			float Distance = -1.0f;
			vec2 Dir(0, 0);

			if(m_pWorld->m_Tuning.m_PlayerCollision && m_Roll == 0)
			{
				if(m_Slide == 0 && pCharCore->m_Slide == 0 && pCharCore->m_Roll == 0 &&
				   DistanceSquared < (PhysSize * 1.35f) * (PhysSize * 1.35f) && DistanceSquared > 12.0f * 12.0f)
				{
					Distance = sqrtf(DistanceSquared);
					Dir = PlayerDelta / Distance;
					m_PlayerCollision = true;

					float a = (PhysSize * 1.45f - Distance);
					float Velocity = 0.5f;

					// make sure that we don't add excess force by checking the
					// direction against the current velocity. if not zero.
					const float SpeedSquared = dot(m_Vel, m_Vel);
					if(SpeedSquared > 0.00000001f)
						Velocity = 1 - (dot(m_Vel, Dir) / sqrtf(SpeedSquared) + 1) / 2;

					m_Vel += Dir * a * (Velocity * 0.75f);
					m_Vel *= 0.85f;
				}

				if(absolute(m_Vel.x) < 1.0f && absolute(m_Vel.y) < 1.0f && DistanceSquared < PhysSize * PhysSize &&
				   m_Pos.y <= pCharCore->m_Pos.y)
				{
					if(!m_pCollision->CheckPoint(m_Pos.x - 28.0f * 0.5f, m_Pos.y - 64.0f * 0.5f) &&
					   !m_pCollision->CheckPoint(m_Pos.x + 28.0f * 0.5f, m_Pos.y - 64.0f * 0.5f))
					{
						m_Vel.y -= 1.0f;
						m_Pos.y -= 1.0f;
					}
				}
			}

			// handle hook influence
			if(m_HookedPlayer == i && m_pWorld->m_Tuning.m_PlayerHooking)
			{
				const float MinimumHookDistance = PhysSize * m_pWorld->m_Tuning.m_HookDragMinDistFactor;
				if(DistanceSquared > MinimumHookDistance * MinimumHookDistance)
				{
					if(Distance < 0.0f)
					{
						Distance = sqrtf(DistanceSquared);
						Dir = PlayerDelta / Distance;
					}
					float Accel = m_pWorld->m_Tuning.m_HookDragAccel * (Distance / m_pWorld->m_Tuning.m_HookLength);
					float DragSpeed = m_pWorld->m_Tuning.m_HookDragSpeed;

					// add force to the hooked player
					pCharCore->m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.x, Accel * Dir.x * 1.5f);
					pCharCore->m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.y, Accel * Dir.y * 1.5f);

					// add a little bit force to the guy who has the grip
					float PullX = -Accel * Dir.x * 0.25f;
					float PullY = -Accel * Dir.y * 0.25f;
					PullX *= (PullX < 0.0f && m_Direction < 0) || (PullX > 0.0f && m_Direction > 0)
								 ? m_pWorld->m_Tuning.m_HookMoveAlongFactor
								 : m_pWorld->m_Tuning.m_HookMoveAgainstFactor;
					if(PullY > 0.0f)
						PullY *= m_pWorld->m_Tuning.m_HookDownFactor;
					m_Vel.x = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.x, PullX);
					m_Vel.y = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.y, PullY);
				}
			}
		}

		// jumppads
		if(m_Action != COREACTION_JUMPPAD || (m_Action == COREACTION_JUMPPAD && m_ActionState > 12))
		{
			for(int i = 0; i < MAX_DROIDS; i++)
			{
				vec4 ImpactPos = m_pWorld->m_aImpactPos[i];

				if(ImpactPos.x == 0)
					continue;

				// if (abs(m_Pos.x - ImpactPos.x) < 64 && abs(m_Pos.y - (ImpactPos.y - 16)) < 16)
				if(m_Pos.x > ImpactPos.x && m_Pos.x < ImpactPos.z && m_Pos.y > ImpactPos.y && m_Pos.y < ImpactPos.w)
					Jumppad();
			}
		}
	}

	// fix to slope bug (standing near wall)
	if(IsGrounded() && !m_Sliding && absolute(m_Vel.y) < 1.5f && absolute(m_Vel.x) < 0.2f)
	{
		if(!m_pCollision->IsTileSolid(m_Pos.x - PhysSize, m_Pos.y + PhysSize * 0.7) ||
		   !m_pCollision->IsTileSolid(m_Pos.x + PhysSize, m_Pos.y + PhysSize * 0.7))
		{
			if(!m_pCollision->IsTileSolid(m_Pos.x - PhysSize * 0.2f, m_Pos.y + 25) ||
			   !m_pCollision->IsTileSolid(m_Pos.x + PhysSize * 0.2f, m_Pos.y + 25))
			{
				if(m_pCollision->IsTileSolid(m_Pos.x - PhysSize * 1.2f, m_Pos.y))
					m_Pos.x += 2.0f;

				if(m_pCollision->IsTileSolid(m_Pos.x + PhysSize * 1.2f, m_Pos.y))
					m_Pos.x -= 2.0f;
			}
		}
	}

	if(m_Action == COREACTION_SLIDEKICK)
	{
		if(m_ActionState == 1 && absolute(m_Vel.x) < 22.0f)
			m_Vel.x *= 1.05f;

		if(m_ActionState < 0)
		{
			if(m_ActionState-- < -11)
				m_Action = COREACTION_IDLE;
		}
		else
		{
			if(m_ActionState++ > 11)
				m_Action = COREACTION_IDLE;
		}

		if(m_Vel.x > 0)
			m_Anim = 7;
		else
			m_Anim = -7;
	}

	// hang on to somethings
	if(m_Down || m_HandJetpack || m_Jetpack || m_HookState != HOOK_IDLE)
	{
		if(m_Action == COREACTION_HANG)
			m_Action = COREACTION_IDLE;
	}
	else
	{
		if(m_pCollision->IsHangTile(m_Pos + vec2(0, -32))) // -32
		{
			if(m_Vel.y >= 0.0f)
			{
				m_Vel.x *= 0.97f;
				m_Vel.y = 0.0f;
				m_Action = COREACTION_HANG;
				m_ActionState = 0;
				m_Pos.y -= (int(m_Pos.y) % 32 - 26) / 2.0f; // 26

				if(!LoadJetpack)
				{
					m_JetpackPower += 3;

					if(m_JetpackPower > 200)
						m_JetpackPower = 200;
				}
			}
		}
		else if(m_Action == COREACTION_HANG)
			m_Action = COREACTION_IDLE;
	}

	// clamp the velocity to something sane
	if(length(m_Vel) > 6000)
		m_Vel = normalize(m_Vel) * 6000;

	// electric damage effect
	s = m_Status;
	if(s & (1 << STATUS_ELECTRIC))
		m_Vel.x *= 0.85f;

	// fluid collision
	if(InFluid)
	{
		m_Vel *= 0.85f;
		m_Jetpack = false;
		;
		m_HandJetpack = 0;
		m_FluidDamage = true;
	}
}

void CCharacterCore::Jumppad()
{
	m_Vel.y = -9.0f;
	m_Action = COREACTION_JUMPPAD;
	m_ActionState = 0;
}

void CCharacterCore::Roll()
{
	float PhysSize = 28.0f;

	if(m_Roll > 0)
		return;

	const int Direction =
		m_Input.m_Direction != 0 ? m_Input.m_Direction : (m_Vel.x < 0.0f ? -1 : (m_Vel.x > 0.0f ? 1 : 0));
	if(Direction == 0 || absolute(m_Vel.x) <= 2.5f ||
	   m_pCollision->CheckPoint(m_Pos.x + Direction * (PhysSize + 32), m_Pos.y + PhysSize / 2))
		return;

	m_Vel.x = Direction * absolute(m_Vel.x);
	m_Roll = 1;
	m_LockDirection = Direction * 8;
	m_TriggeredEvents |= COREEVENT_ROLL_START;
}

void CCharacterCore::Slide(bool Grounded, int ForceTile)
{
	float PhysSize = 28.0f;

	// start sliding
	if(m_Input.m_Down && m_Slide == 0 && m_Roll == 0)
	{
		const int Direction =
			m_Input.m_Direction != 0 ? m_Input.m_Direction : (m_Vel.x < 0.0f ? -1 : (m_Vel.x > 0.0f ? 1 : 0));
		const bool HasSlideSpeed = Direction != 0 && absolute(m_Vel.x) >= m_pWorld->m_Tuning.m_SlideActivationSpeed;
		const bool EnteringTunnel =
			Direction != 0 &&
			!m_pCollision->CheckPoint(m_Pos.x + Direction * (PhysSize + 32), m_Pos.y + PhysSize / 2) &&
			m_pCollision->CheckPoint(m_Pos.x + Direction * (PhysSize + 32), m_Pos.y - 64);
		if(HasSlideSpeed || EnteringTunnel)
		{
			m_Slide = 1;
			m_Vel.x = Direction * max(absolute(m_Vel.x), (float)m_pWorld->m_Tuning.m_SlideMinimumSpeed);
			m_TriggeredEvents |= COREEVENT_SLIDE_START;
		}
	}

	if(!m_Input.m_Down && m_Slide > 0)
		m_Slide = -4;

	if(m_Wallrun != 0)
		m_Slide = 0;

	if(m_Slide != 0)
	{
		m_Slide++;

		if(m_Slide > 0)
		{
			// if (IsGrounded())
			{
				if(Grounded && (!m_Input.m_Hook || ((ForceTile < 0 && m_Vel.x > 0) || (ForceTile > 0 && m_Vel.x < 0))))
					m_Vel.x *= 0.98f;

				if(m_Vel.x < -3.5f) // && !m_pCollision->CheckPoint(m_Pos.x-(PhysSize+32), m_Pos.y+PhysSize/2))
				{
					m_LockDirection = -2;
					m_Anim = -3;
				}
				else if(m_Vel.x > 3.5f) // && !m_pCollision->CheckPoint(m_Pos.x+(PhysSize+32), m_Pos.y+PhysSize/2))
				{
					m_LockDirection = 2;
					m_Anim = 3;
				}
				else
				{
					m_Slide = -4;
				}
			}
			// else
			//	m_Slide = -4;
		}

		// stand up animation after slide
		if(m_Slide < 0)
		{
			if(m_Vel.x < 0.0f)
			{
				m_LockDirection = -2;
				m_Anim = -4;
			}
			if(m_Vel.x > 0.0f)
			{
				m_LockDirection = 2;
				m_Anim = 4;
			}
		}
	}

	// force slide when in tunnel
	if((m_Slide != 0 || m_Roll != 0) && ((m_pCollision->CheckPoint(m_Pos.x + PhysSize, m_Pos.y - 64) &&
										  !m_pCollision->CheckPoint(m_Pos.x + PhysSize, m_Pos.y + PhysSize / 2)) ||
										 (m_pCollision->CheckPoint(m_Pos.x - PhysSize, m_Pos.y - 64) &&
										  !m_pCollision->CheckPoint(m_Pos.x - PhysSize, m_Pos.y + PhysSize / 2))))
	{
		if(absolute(m_Vel.x) < 5.0f)
			m_Vel.x /= 0.98f;

		if(m_Vel.x < 0)
		{
			m_LockDirection = -2;
			m_Anim = -3;
		}
		else
		{
			m_LockDirection = 2;
			m_Anim = 3;
		}
		m_Slide = 5;
	}
}

void CCharacterCore::Move()
{
	int s = m_Status;
	if(s & (1 << STATUS_DEATHRAY))
		return;

	if(Status(STATUS_SPAWNING))
		return;

	float RampValue = VelocityRamp(length(m_Vel) * 50,
								   m_pWorld->m_Tuning.m_VelrampStart,
								   m_pWorld->m_Tuning.m_VelrampRange,
								   m_pWorld->m_Tuning.m_VelrampCurvature);

	m_Vel.x = m_Vel.x * RampValue;

	float VelY = m_Vel.y;

	bool Down = PlatformState();

	if(VelY < 0.0f)
		Down = true;

	vec2 NewPos = m_Pos;

	if((m_Slide == 0 && m_Roll == 0) || m_Wallrun != 0)
	{
		NewPos.y -= 18;
		m_pCollision->MoveBox(&NewPos, &m_Vel, vec2(28.0f, 64.0f), 0, !m_Sliding, Down);
		NewPos.y += 18;

		int TopLeft = m_pCollision->CheckPoint(m_Pos.x - 28.0f * 0.5f, m_Pos.y - (64.0f) * 0.5f - 18, false, true);
		int TopRight = m_pCollision->CheckPoint(m_Pos.x + 28.0f * 0.5f, m_Pos.y - (64.0f) * 0.5f - 18, false, true);

		// unstuck jumpkick
		if(TopLeft && !TopRight)
			NewPos.x += 1;
		else if(TopRight && !TopLeft)
			NewPos.x -= 1;
	}
	else
	{
		NewPos.y -= 10;
		m_pCollision->MoveBox(&NewPos, &m_Vel, vec2(28.0f, 48.0f), 0, !m_Sliding, Down);
		NewPos.y += 10;
	}

	const float LandingEventSpeed = max(1.0f, (float)m_pWorld->m_Tuning.m_Gravity * 1.5f);
	const bool FallingStopped = VelY > LandingEventSpeed && absolute(m_Vel.y) < 2.0f;
	bool GroundContact = false;
	if(FallingStopped)
	{
		// MoveBox can stop vertical movement when a box corner brushes a wall.
		// Probe inside the feet so only an actual floor contact counts as landing.
		const float FeetY = NewPos.y + 15.0f;
		for(int x = -12; x <= 12 && !GroundContact; x++)
			GroundContact = m_pCollision->CheckPoint(NewPos.x + x, FeetY, false, Down);
	}

	if(GroundContact)
	{
		m_LandingVelocity = VelY;
		m_TriggeredEvents |= COREEVENT_LAND;
	}

	if(GroundContact && VelY > m_pWorld->m_Tuning.m_RollLandingSpeed && m_Input.m_Down)
		Roll();

	m_Vel.x = m_Vel.x * (1.0f / RampValue);

	/*
	if (m_Action == COREACTION_SLIDEKICK)
	{

		float Distance = distance(m_Pos, NewPos);
		int End = Distance+1;
		vec2 LastPos = m_Pos;
		for(int i = 0; i < End; i++)
		{
			float a = i/Distance;
			vec2 Pos = mix(m_Pos, NewPos, a);
			for(int p = 0; p < MAX_CLIENTS; p++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[p];
				if(!pCharCore || pCharCore == this || pCharCore->m_Roll != 0 || pCharCore->Status(STATUS_SPAWNING))
					continue;
				float D = distance(Pos, pCharCore->m_Pos+vec2(0, -32));
				if(D < 40.0f)
				{
					m_PlayerCollision = true;
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(distance(NewPos, pCharCore->m_Pos) > D)
						m_Pos = NewPos;

					pCharCore->m_Vel += m_Vel;
					//m_Vel.x *= 0.9f;

					return;
				}
			}
			LastPos = Pos;
		}
	}
	*/

	// ball collision
	if(true)
	{
		// ball
		CBallCore *pBallCore = m_pWorld->m_pBall;

		if(pBallCore)
		{
			float BallSize = m_pWorld->m_Tuning.m_BallSize;

			vec2 BPos = pBallCore->m_Pos;
			float OffsetY = -26;

			if(m_Action == COREACTION_SLIDEKICK && m_ActionState > 2 && m_ActionState < 10)
				OffsetY = -14;

			if(m_Slide != 0 || m_Roll != 0)
				OffsetY = -6;

			if(m_DashTimer > 0)
				OffsetY = -12;

			const float MovementDistance = distance(m_Pos, NewPos);
			const int End = max(1, (int)MovementDistance + 1);
			const float CollisionRadius = BallSize * 0.80f + 16.0f;
			const float CollisionRadiusSquared = CollisionRadius * CollisionRadius;
			vec2 LastPos = m_Pos;

			for(int i = 0; i < End; i++)
			{
				const float a = MovementDistance > 0.0f ? i / MovementDistance : 0.0f;
				vec2 Pos = mix(m_Pos, NewPos, a) + vec2(0, OffsetY);
				const vec2 BallDelta = Pos - BPos;
				const float BallDistanceSquared = dot(BallDelta, BallDelta);

				if(BallDistanceSquared <= CollisionRadiusSquared)
				{
					const float D = sqrtf(BallDistanceSquared);
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(dot(NewPos - BPos, NewPos - BPos) > BallDistanceSquared)
						m_Pos = NewPos;

					float theta = atan2((Pos.y - BPos.y), (Pos.x - BPos.x));
					float overlap = CollisionRadius - D;

					vec2 BVel = -vec2(cos(theta), sin(theta)) * overlap;
					m_pCollision->MoveBox(&pBallCore->m_Pos, &BVel, vec2(BallSize, BallSize), 0.0f);

					pBallCore->PlayerHit();

					BPos = pBallCore->m_Pos;

					float theta1 = GetAngle(m_BallHitVel);
					float theta2 = GetAngle(pBallCore->m_Vel);
					float phi = atan2(BPos.y - Pos.y, BPos.x - Pos.x);
					float m1 = 1.0f;
					float m2 = 0.5f;

					float v1 = length(m_BallHitVel);
					float v2 = length(pBallCore->m_Vel);

					float dx2F =
						(v2 * cos(theta2 - phi) * (m2 - m1) + 2 * m1 * v1 * cos(theta1 - phi)) / (m1 + m2) * cos(phi) +
						v2 * sin(theta2 - phi) * cos(phi + pi / 2);
					float dy2F =
						(v2 * cos(theta2 - phi) * (m2 - m1) + 2 * m1 * v1 * cos(theta1 - phi)) / (m1 + m2) * sin(phi) +
						v2 * sin(theta2 - phi) * sin(phi + pi / 2);

					// if (pBallCore->m_Status & (1<<BALLSTATUS_SUPER))
					//	m_Vel -= vec2(dx1F, dy1F)*0.6f;

					if(m_DashTimer > 0)
					{
						pBallCore->m_Vel = vec2(dx2F, dy2F) * 1.3f;

						if(dot(pBallCore->m_Vel, pBallCore->m_Vel) > 15.0f * 15.0f)
							pBallCore->m_Status |= 1 << BALLSTATUS_SUPER;
					}
					else
						// pBallCore->m_Vel = vec2(dx2F, dy2F)*0.8f + m_pCollision->Reflect(pBallCore->m_Vel,
						// normalize(Pos - BPos))*0.25f + m_pCollision->Reflect(pBallCore->m_Vel,
						// normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY)))*0.25f;
						pBallCore->m_Vel = vec2(dx2F, dy2F) * 0.8f +
										   m_pCollision->Reflect(pBallCore->m_Vel, normalize(Pos - BPos)) * 0.25f;

					// pBallCore->m_Vel = ;

					// m_Vel -= vec2(dx1F, dy1F)*0.1f;
					break;
				}

				LastPos = Pos;
			}
		}
	}

	// check player collision
	if((m_Action == COREACTION_SLIDEKICK && m_ActionState > 2 && m_ActionState < 10) ||
	   (m_pWorld && m_pWorld->m_Tuning.m_PlayerCollision && m_Roll == 0 && m_Slide == 0))
	{
		const float MovementDistance = distance(m_Pos, NewPos);
		const int End = max(1, (int)MovementDistance + 1);
		vec2 LastPos = m_Pos;
		for(int i = 0; i < End; i++)
		{
			const float a = MovementDistance > 0.0f ? i / MovementDistance : 0.0f;
			vec2 Pos = mix(m_Pos, NewPos, a);

			for(int p = 0; p < MAX_CHARACTERS; p++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[p];
				if(!pCharCore || pCharCore == this || pCharCore->m_Roll != 0 || pCharCore->Status(STATUS_SPAWNING))
					continue;
				const vec2 PlayerDelta = Pos - pCharCore->m_Pos;
				const float PlayerDistanceSquared = dot(PlayerDelta, PlayerDelta);
				float KickDistanceSquared = 9000.0f * 9000.0f;

				vec2 Off = vec2(20.0, 0.0f);
				if(m_Vel.x < 0)
					Off.x *= -1;

				if(m_Action == COREACTION_SLIDEKICK)
				{
					const vec2 KickDelta = Pos + Off - (pCharCore->m_Pos + vec2(0, -32));
					KickDistanceSquared = dot(KickDelta, KickDelta);
				}

				if(KickDistanceSquared < 40.0f * 40.0f ||
				   (PlayerDistanceSquared < 32.0f * 32.0f && PlayerDistanceSquared > 12.0f * 12.0f))
				{
					m_PlayerCollision = true;
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(dot(NewPos - pCharCore->m_Pos, NewPos - pCharCore->m_Pos) > PlayerDistanceSquared)
						m_Pos = NewPos;

					if(m_Action == COREACTION_SLIDEKICK)
					{
						if(m_ActionState > 0)
						{
							m_ActionState *= -1;
							pCharCore->m_KickDamage = m_ClientID;

							// both players hitting
							if(pCharCore->m_Action == COREACTION_SLIDEKICK && m_ActionState > 2 && m_ActionState < 10)
							{
								m_KickDamage = pCharCore->m_ClientID;
								pCharCore->m_ActionState *= -1;

								vec2 CCv = vec2(m_Vel.x * 1.5f, -absolute(m_Vel.x * 0.5f));

								m_Vel = vec2(pCharCore->m_Vel.x * 1.5f, -absolute(pCharCore->m_Vel.x * 0.5f));
								pCharCore->m_Vel = CCv;
							}
							// just this hitting
							else
							{
								pCharCore->m_Vel = vec2(m_Vel.x * 1.5f, -absolute(m_Vel.x * 0.5f));
								m_Vel *= 0.3f;
							}
						}
						else
							m_Vel *= 0.3f;
					}

					return;
				}
			}
			LastPos = Pos;
		}
	}

	m_Pos = NewPos;
}

void CCharacterCore::Write(CNetObj_CharacterCore *pObjCore)
{
	if(!pObjCore)
		return;
	const bool CollisionReady = m_pCollision && m_pCollision->m_pTiles && m_pCollision->m_pLayers;

	pObjCore->m_X = round_to_int(m_Pos.x);
	pObjCore->m_Y = round_to_int(m_Pos.y);

	pObjCore->m_HookState = m_HookState;
	pObjCore->m_HookTick = m_HookTick;
	pObjCore->m_HookX = round_to_int(m_HookPos.x);
	pObjCore->m_HookY = round_to_int(m_HookPos.y);
	pObjCore->m_HookDx = round_to_int(m_HookDir.x * 256.0f);
	pObjCore->m_HookDy = round_to_int(m_HookDir.y * 256.0f);
	pObjCore->m_HookedPlayer = m_HookedPlayer;

	pObjCore->m_Health = m_Health;
	pObjCore->m_VelX = round_to_int(m_Vel.x * 256.0f);
	pObjCore->m_VelY = round_to_int(m_Vel.y * 256.0f);
	pObjCore->m_MoveSpeedMultiplier = clamp(round_to_int(m_MoveSpeedMultiplier * 100.0f), 10, 300);
	pObjCore->m_DamageTick = m_DamageTick;
	pObjCore->m_Jumped = m_Jumped;
	pObjCore->m_CoyoteTime = m_CoyoteTime;
	pObjCore->m_JumpBufferTime = m_JumpBufferTime;
	pObjCore->m_PrevJumpInput = m_PrevJumpInput;
	pObjCore->m_JumpTimer = m_JumpTimer;
	pObjCore->m_Direction = m_Direction;
	pObjCore->m_Down = m_Down;
	pObjCore->m_Charge = m_Charge;
	pObjCore->m_ChargeLevel = m_ChargeLevel;
	pObjCore->m_Sliding = m_Sliding;
	pObjCore->m_Grounded = CollisionReady ? IsGrounded() : 0;
	pObjCore->m_Angle = m_Angle;
	pObjCore->m_Anim = m_Anim;
	pObjCore->m_Jetpack = m_Jetpack;
	pObjCore->m_HandJetpack = m_HandJetpack;
	pObjCore->m_JetpackPower = m_JetpackPower;
	pObjCore->m_Wallrun = m_Wallrun;
	pObjCore->m_Roll = m_Roll;
	pObjCore->m_Slide = m_Slide;
	pObjCore->m_Status = m_Status;
	pObjCore->m_LockDirection = m_LockDirection;
	pObjCore->m_Slope = CollisionReady ? SlopeState() : 0;
	pObjCore->m_Action = m_Action;
	pObjCore->m_ActionState = m_ActionState;

	pObjCore->m_Movement1 = m_DashTimer | m_DashAngle << 6;
}

void CCharacterCore::Read(const CNetObj_CharacterCore *pObjCore)
{
	m_Pos.x = pObjCore->m_X;
	m_Pos.y = pObjCore->m_Y;

	m_HookState = pObjCore->m_HookState;
	m_HookTick = pObjCore->m_HookTick;
	m_HookPos.x = pObjCore->m_HookX;
	m_HookPos.y = pObjCore->m_HookY;
	m_HookDir.x = pObjCore->m_HookDx / 256.0f;
	m_HookDir.y = pObjCore->m_HookDy / 256.0f;
	m_HookedPlayer = pObjCore->m_HookedPlayer;

	m_Health = pObjCore->m_Health;
	m_Vel.x = pObjCore->m_VelX / 256.0f;
	m_Vel.y = pObjCore->m_VelY / 256.0f;
	m_MoveSpeedMultiplier = pObjCore->m_MoveSpeedMultiplier / 100.0f;
	m_DamageTick = pObjCore->m_DamageTick;
	m_Jumped = pObjCore->m_Jumped;
	m_CoyoteTime = pObjCore->m_CoyoteTime;
	m_JumpBufferTime = pObjCore->m_JumpBufferTime;
	m_PrevJumpInput = pObjCore->m_PrevJumpInput;
	m_JumpTimer = pObjCore->m_JumpTimer;
	m_Direction = pObjCore->m_Direction;
	m_Down = pObjCore->m_Down;
	m_Charge = pObjCore->m_Charge;
	m_ChargeLevel = pObjCore->m_ChargeLevel;
	m_Sliding = pObjCore->m_Sliding;
	m_Angle = pObjCore->m_Angle;
	m_Anim = pObjCore->m_Anim;
	m_Jetpack = pObjCore->m_Jetpack;
	m_HandJetpack = pObjCore->m_HandJetpack;
	m_JetpackPower = pObjCore->m_JetpackPower;
	m_Wallrun = pObjCore->m_Wallrun;
	m_Roll = pObjCore->m_Roll;
	m_Slide = pObjCore->m_Slide;
	m_Status = pObjCore->m_Status;
	m_LockDirection = pObjCore->m_LockDirection;
	m_Action = pObjCore->m_Action;
	m_ActionState = pObjCore->m_ActionState;

	m_DashTimer = pObjCore->m_Movement1 & (63 << 0);
	// m_DashAngle = (pObjCore->m_Movement1&(255<<6))>>6;
	m_DashAngle = pObjCore->m_Movement1 >> 6;
}

void CCharacterCore::Quantize()
{
	CNetObj_CharacterCore Core;
	Write(&Core);
	Read(&Core);
}

/*
	- - - - - BALL - - - - -
*/

CBallCore::CBallCore()
{
	Init(0, 0);
	Reset();
}

void CBallCore::Init(CWorldCore *pWorld, CCollision *pCollision)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;
}

void CBallCore::Reset()
{
	m_Pos = vec2(0, 0);
	m_Vel = vec2(0, 0);

	m_Angle = 0.0f;
	m_AngleForce = 0.0f;
	m_Status = 0;
	m_Status |= 1 << BALLSTATUS_STATIONARY;
	m_ForceCoreSend = false;
	m_TriggeredEvents = 0;
}

void CBallCore::PlayerHit()
{
	m_ForceCoreSend = true;
	if(m_Status & (1 << BALLSTATUS_STATIONARY))
		m_Status ^= 1 << BALLSTATUS_STATIONARY;
}

const float CBallCore::BallSize()
{
	return m_pWorld->m_Tuning.m_BallSize;
}

bool CBallCore::PlatformState()
{
	float PhysSize = BallSize();

	if(m_pCollision->IsPlatform(m_Pos.x - PhysSize / 2, m_Pos.y + PhysSize / 2 + 24) ||
	   m_pCollision->IsPlatform(m_Pos.x + PhysSize / 2, m_Pos.y + PhysSize / 2 + 24))
		return m_Vel.y < 0.0f;

	return true;
}

void CBallCore::Tick()
{
	m_TriggeredEvents = 0;

	if(m_Status & (1 << BALLSTATUS_STATIONARY))
		return;

	if(m_Status & (1 << BALLSTATUS_SUPER) && length(m_Vel) < 15.0f)
		m_Status ^= 1 << BALLSTATUS_SUPER;

	if(m_Status & (1 << BALLSTATUS_SUPER))
		m_Vel.y += 0.45f;
	else
		m_Vel.y += 0.5f;
}

void CBallCore::Move()
{
	if(m_Status & (1 << BALLSTATUS_STATIONARY))
		return;

	// limit speed
	float Limit = 30.0f;
	float Elastic = 0.7f;

	bool Down = PlatformState();

	if(m_Status & (1 << BALLSTATUS_SUPER))
	{
		Limit = 40.0f;
		Elastic = 0.85f;
	}

	if(length(m_Vel) > Limit)
		m_Vel = normalize(m_Vel) * Limit;

	float BallSize = m_pWorld->m_Tuning.m_BallSize;

	bool Grounded = false;
	if(m_pCollision->CheckPoint(m_Pos.x + BallSize / 2, m_Pos.y + BallSize / 2 + 5, false, Down))
		Grounded = true;
	else if(m_pCollision->CheckPoint(m_Pos.x - BallSize / 2, m_Pos.y + BallSize / 2 + 5, false, Down))
		Grounded = true;

	const int OnForceTile =
		m_pCollision->IsForceTile(m_Pos.x - BallSize / 2, m_Pos.x + BallSize / 2, m_Pos.y + BallSize / 2 + 5);

	if(Grounded)
	{
		if(OnForceTile)
			m_Vel.x = (m_Vel.x + OnForceTile * 0.4f) * 0.925f;

		// m_Vel.x *= 0.8f;
		m_AngleForce += (m_Vel.x - OnForceTile * 0.55f * 8.0f - m_AngleForce) / 2.0f;
		m_Vel.x *= 0.99f;
		m_Vel.y *= 0.99f;
	}
	else
	{
		// m_Vel.x *= 0.99f;
		// m_Vel.y *= 0.99f;
		m_AngleForce *= 0.99f;
	}

	vec2 NewPos = m_Pos;
	vec2 OldVel = m_Vel;

	m_pCollision->MoveBox(&NewPos, &m_Vel, vec2(BallSize, BallSize), Elastic, false, Down);

	if((((OldVel.x < 0 && m_Vel.x > 0) || (OldVel.x > 0 && m_Vel.x < 0)) && absolute(m_Vel.x) > 3.0f) ||
	   (((OldVel.y < 0 && m_Vel.y > 0) || (OldVel.y > 0 && m_Vel.y < 0)) && absolute(m_Vel.y) > 3.0f))
		m_TriggeredEvents |= COREEVENT_BALL_BOUNCE;

	m_Angle += clamp(m_AngleForce * 0.04f, -0.3f, 0.3f);

	m_Pos = NewPos;
}

void CBallCore::Write(CNetObj_BallCore *pObjCore)
{
	if(!pObjCore)
		return;

	pObjCore->m_X = round_to_int(m_Pos.x);
	pObjCore->m_Y = round_to_int(m_Pos.y);

	pObjCore->m_VelX = round_to_int(m_Vel.x * 256.0f);
	pObjCore->m_VelY = round_to_int(m_Vel.y * 256.0f);
	pObjCore->m_Angle = round_to_int(m_Angle * 256.0f);
	pObjCore->m_AngleForce = round_to_int(m_AngleForce * 256.0f);

	pObjCore->m_Status = m_Status;
}

void CBallCore::Read(const CNetObj_BallCore *pObjCore)
{
	m_Pos.x = pObjCore->m_X;
	m_Pos.y = pObjCore->m_Y;

	m_Vel.x = pObjCore->m_VelX / 256.0f;
	m_Vel.y = pObjCore->m_VelY / 256.0f;
	m_Angle = pObjCore->m_Angle / 256.0f;
	m_AngleForce = pObjCore->m_AngleForce / 256.0f;

	m_Status = pObjCore->m_Status;
}

void CBallCore::Quantize()
{
	CNetObj_BallCore Core;
	Write(&Core);
	Read(&Core);
}
