#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "character.h"
#include "droid_bastionstrider.h"

static const CBossBodyShape s_StriderShape = {STRIDER_BODY_HALF_W, STRIDER_BODY_TOP, STRIDER_BODY_BOTTOM, STRIDER_MAX_CROUCH};

CBastionStrider::CBastionStrider(CGameWorld *pGameWorld, vec2 Pos) :
	CBossV5(pGameWorld, Pos, DROIDTYPE_BOSSBULKHEAD, STRIDER_HEALTH, s_aStriderPartShare, s_aStriderHit, NUM_STRIDER_HIT,
		STRIDER_ACT_DEATH)
{
	m_ProximityRadius = 80.0f;
	m_Move = 0;
	m_ChargeCooldown = 30;
	m_MortarCooldown = 0;
	m_StompCooldown = 0;
	m_LeapCooldown = 40;
	m_VentUntil = 0;
	m_Airborne = false;
	m_Dashing = false;
	m_LaunchTick = 0;
	m_PreVel = vec2(0, 0);
	m_Ground = Pos.y + STRIDER_HOVER;
	m_Body.Init(s_StriderShape, STRIDER_STEP_UP);
}

void CBastionStrider::TickTimers()
{
	if(m_ChargeCooldown > 0)
		m_ChargeCooldown--;
	if(m_MortarCooldown > 0)
		m_MortarCooldown--;
	if(m_StompCooldown > 0)
		m_StompCooldown--;
	if(m_LeapCooldown > 0)
		m_LeapCooldown--;
}

bool CBastionStrider::Exposed()
{
	return Server()->Tick() < m_VentUntil || m_Act == STRIDER_ACT_OVERHEAT || m_Act == STRIDER_ACT_STAGGER;
}

int CBastionStrider::StatusFlags()
{
	int Flags = CBossV5::StatusFlags();
	if(m_Act == STRIDER_ACT_CHARGE)
		Flags |= BOSSV5_FLAG_THRUST;
	if(m_Airborne || m_Body.m_AirTicks > 3)
		Flags |= BOSSV5_FLAG_AIR;
	return Flags;
}

void CBastionStrider::OnActStart(int Act)
{
	m_Dashing = false;
	if(Act != STRIDER_ACT_LEAP)
		m_Airborne = false;
}

void CBastionStrider::OnActFinished(int Act)
{
	static const int s_aReload[3] = {6, 4, 2};
	static const int s_aCharge[3] = {90, 70, 50};
	static const int s_aMortar[3] = {110, 85, 65};
	static const int s_aStomp[3] = {70, 55, 40};
	static const int s_aLeap[3] = {100, 80, 60};
	m_ReloadTimer = s_aReload[m_Phase];
	if(Act == STRIDER_ACT_CHARGE)
		m_ChargeCooldown = s_aCharge[m_Phase];
	if(Act == STRIDER_ACT_MORTAR)
		m_MortarCooldown = s_aMortar[m_Phase];
	if(Act == STRIDER_ACT_STOMP)
		m_StompCooldown = s_aStomp[m_Phase];
	if(Act == STRIDER_ACT_LEAP)
		m_LeapCooldown = s_aLeap[m_Phase];
	if(Act == STRIDER_ACT_TURN)
		m_ReloadTimer = 0; // a turn is already a wait; swing straight into the next move
	m_Airborne = false;
	m_Dashing = false;
}

void CBastionStrider::OnPartBroken(int Part)
{
	if(Part == STRIDER_PART_SHIELD)
		GameServer()->CreateSound(m_Pos, SOUND_METAL_HIT);
}

int CBastionStrider::IncomingDamage(int Dmg, int Part, vec2 Pos, int *pPartDmg)
{
	const bool FromBehind = (Pos.x - m_Pos.x) * m_Dir < -20.0f;
	int ShieldDmg = 0;
	const int Out = StriderIncomingDamage(Dmg, Part, FromBehind, Exposed(), &ShieldDmg);
	if(ShieldDmg > 0)
	{
		// The shield takes the full hit; only the bleed reaches the hull.
		*pPartDmg = ShieldDmg;
		if(frandom() < 0.3f)
			GameServer()->CreateSound(Pos, SOUND_SHIELD_HIT);
	}
	return Out;
}

bool CBastionStrider::FloorAhead(float Dist)
{
	(void)Dist;
	return m_Body.FloorAhead(GameServer()->Collision(), m_Pos, m_Dir, 120.0f);
}

bool CBastionStrider::WallAhead(int Dir)
{
	return BodyWallAhead(Dir, 40.0f);
}

// ---------------------------------------------------------------- shots

void CBastionStrider::FireShell(int Shot)
{
	static const float s_aSpread[5] = {0.0f, -110.0f, 110.0f, -220.0f, 220.0f};
	CCharacter *pTarget = TargetChr();
	vec2 Aim = pTarget ? pTarget->m_Pos + pTarget->GetVel() * 18.0f : m_Pos + m_Target;
	Aim.x += s_aSpread[Shot % 5];
	const vec2 From = LocalToWorld(s_StriderMortar.m_X + 14.0f, s_StriderMortar.m_Y - 40.0f);
	const float Dist = distance(Aim, From);
	const float T = BossV5Clamp(Dist / 11.0f, 34.0f, 60.0f);
	float Vx, Vy;
	BossV5Ballistic(Aim.x - From.x, Aim.y - From.y, T, STRIDER_SHELL_GRAVITY, 24.0f, &Vx, &Vy);
	if(AddShot(STRIDER_SHOT_SHELL, From, vec2(Vx, Vy), 0, 1) >= 0)
		GameServer()->CreateSound(From, SOUND_BAZOOKA_FIRE);
	m_Aim = Aim;
}

void CBastionStrider::ExplodeShell(int i, vec2 At)
{
	GameServer()->CreateExplosion(At, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	GameServer()->CreateSound(At, SOUND_GRENADE_EXPLODE);
	HurtPlayers(At, STRIDER_SHELL_RADIUS, STRIDER_SHELL_DAMAGE, 9.0f, 0.4f);
	RemoveShot(i);
}

void CBastionStrider::OnShotDestroyed(int i)
{
	// A shot-down shell bursts harmlessly in the air.
	GameServer()->CreateExplosion(m_aShots[i].m_Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	RemoveShot(i);
}

void CBastionStrider::TickShot(int i)
{
	CCollision *pCollision = GameServer()->Collision();
	CShot &s = m_aShots[i];
	if(s.m_Kind == STRIDER_SHOT_SHELL)
	{
		s.m_Vel.y += STRIDER_SHELL_GRAVITY;
		const vec2 Next = s.m_Pos + s.m_Vel;
		CCharacter *pHit = GameServer()->m_World.ClosestCharacter(Next, 28.0f, 0);
		vec2 At;
		if(pHit && pHit->IsAlive() && !pHit->m_IsBot)
			ExplodeShell(i, Next);
		else if(pCollision->IntersectLine(s.m_Pos, Next, &At, 0, false, true))
			ExplodeShell(i, At);
		else if(s.m_Life > 260)
			RemoveShot(i);
		else
			s.m_Pos = Next;
		return;
	}
	if(s.m_Kind == STRIDER_SHOT_WAVE)
	{
		// The wave hugs the floor: re-find it each step so it climbs ramps and drops down steps.
		const vec2 Next = s.m_Pos + vec2(s.m_Vel.x, 0.0f);
		vec2 Floor;
		if(s.m_Life > STRIDER_WAVE_LIFE || !pCollision->IntersectLine(Next + vec2(0, -60.0f), Next + vec2(0, 90.0f), &Floor, 0, false, true) ||
			pCollision->CheckPoint(Floor + vec2(0, -6.0f)))
		{
			RemoveShot(i);
			return;
		}
		s.m_Pos = Floor;
		for(int c = 0; c < MAX_CHARACTERS; c++)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(c);
			if(!pChr || !pChr->IsAlive() || pChr->m_IsBot || m_aHitCooldown[c] > Server()->Tick())
				continue;
			const vec2 d = pChr->m_Pos - s.m_Pos;
			if(fabsf(d.x) > 34.0f || d.y < -STRIDER_WAVE_HEIGHT - 28.0f || d.y > 12.0f)
				continue;
			m_aHitCooldown[c] = Server()->Tick() + 25;
			pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(STRIDER_WAVE_DAMAGE), vec2(s.m_Vel.x > 0 ? 6.0f : -6.0f, -11.0f), pChr->m_Pos);
		}
		if(s.m_Life % 6 == 0)
			GameServer()->CreateBuildingHit(s.m_Pos + vec2(0, -6.0f));
	}
}

void CBastionStrider::SnapShot(const CShot &s, CNetObj_BossShot *pShot)
{
	CBossV5::SnapShot(s, pShot);
	if(s.m_Kind == STRIDER_SHOT_WAVE)
		pShot->m_VelY = STRIDER_WAVE_LIFE - s.m_Life;
}

void CBastionStrider::Slam(vec2 At, bool Waves)
{
	HurtPlayers(At, 130.0f, STRIDER_WAVE_DAMAGE, 12.0f, 0.7f);
	GameServer()->CreateBuildingHit(At);
	GameServer()->CreateSound(At, SOUND_BODY_LAND);
	if(!Waves)
		return;
	const float Speed = StriderWaveSpeed(m_Phase);
	for(int Side = -1; Side <= 1; Side += 2)
	{
		vec2 Floor;
		const vec2 From = vec2(At.x + Side * 110.0f, At.y);
		if(GameServer()->Collision()->IntersectLine(From + vec2(0, -60.0f), From + vec2(0, 90.0f), &Floor, 0, false, true))
			AddShot(STRIDER_SHOT_WAVE, Floor, vec2(Side * Speed, 0.0f), Side);
	}
}

// ---------------------------------------------------------------- acts

void CBastionStrider::TickAct(int Elapsed)
{
	switch(m_Act)
	{
	case STRIDER_ACT_TURN:
		if(!(m_Events & 1) && Elapsed >= BossV5Ticks(StriderTurnTime(m_Phase) * 0.5f))
		{
			m_Events |= 1;
			m_Dir = -m_Dir;
		}
		if(Elapsed >= BossV5Ticks(StriderTurnTime(m_Phase)))
			FinishAct();
		break;
	case STRIDER_ACT_BASH:
	{
		const int Count = StriderBashCount(m_Phase);
		for(int b = 0; b < Count; b++)
		{
			const int At = StriderBashTick() + b * StriderBashGap();
			if((m_Events & (1 << b)) || Elapsed < At)
				continue;
			m_Events |= 1 << b;
			const vec2 From = LocalToWorld(80.0f, 0.0f);
			const vec2 To = LocalToWorld(80.0f + STRIDER_BASH_REACH, 10.0f);
			HurtSegment(From, To, 52.0f, STRIDER_BASH_DAMAGE, 16.0f);
			m_Vel.x += m_Dir * 7.0f;
			GameServer()->CreateSound(To, SOUND_KICKHIT);
			m_Aim = To;
		}
		break;
	}
	case STRIDER_ACT_CHARGE:
	{
		const int Windup = StriderChargeWindup(m_Phase);
		if(Elapsed < Windup)
		{
			// Tracks while revving, then commits.
			if(AcquireTarget(false, false) && m_Target.x * m_Dir < -60.0f && Elapsed < Windup / 2)
				m_Dir = -m_Dir;
			m_Aim = m_Pos + m_Target;
			if(Elapsed == 1)
				GameServer()->CreateSound(m_Pos, SOUND_CHARGE_UP);
			break;
		}
		if(!m_Dashing && !(m_Events & 1))
		{
			m_Events |= 1;
			m_Dashing = true;
			for(int c = 0; c < MAX_CHARACTERS; c++)
				m_aHitCooldown[c] = 0;
			GameServer()->CreateSound(m_Pos, SOUND_DASH);
		}
		if(!m_Dashing)
			break;
		const float Speed = StriderChargeSpeed(m_Phase) * (PartAlive(STRIDER_PART_LEGS) ? 1.0f : 0.75f);
		m_Vel.x = m_Dir * Speed;
		// Ram everything in front of the shield.
		const vec2 Front = LocalToWorld(130.0f, 0.0f);
		for(int c = 0; c < MAX_CHARACTERS; c++)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(c);
			if(!pChr || !pChr->IsAlive() || pChr->m_IsBot || m_aHitCooldown[c] > Server()->Tick())
				continue;
			if(distance(pChr->m_Pos, Front) > 90.0f && distance(pChr->m_Pos, m_Pos) > 100.0f)
				continue;
			m_aHitCooldown[c] = Server()->Tick() + 50;
			pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(STRIDER_CHARGE_DAMAGE), vec2(m_Dir * 20.0f, -10.0f), pChr->m_Pos);
			GameServer()->CreateSound(pChr->m_Pos, SOUND_HAMMER_HIT);
		}
		if(m_BlockedX || WallAhead(m_Dir))
		{
			// Slammed into a wall: shockwave, then it is stuck venting.
			m_Dashing = false;
			m_Vel.x = -m_Dir * 4.0f;
			Slam(LocalToWorld(120.0f, STRIDER_HOVER - 10.0f), m_Phase >= 1);
			StartAct(STRIDER_ACT_OVERHEAT);
			return;
		}
		if(Elapsed >= Windup + StriderChargeTicks() || !FloorAhead(170.0f))
		{
			m_Dashing = false;
			m_Vel.x *= 0.3f;
			m_VentUntil = Server()->Tick() + STRIDER_VENT_TICKS / 2;
			FinishAct();
		}
		break;
	}
	case STRIDER_ACT_STOMP:
		if(!(m_Events & 1) && Elapsed >= StriderStompTick())
		{
			m_Events |= 1;
			Slam(vec2(m_Pos.x, m_Ground < 1e8f ? m_Ground : m_Pos.y + STRIDER_HOVER), true);
		}
		if(m_Phase >= 2 && !(m_Events & 2) && Elapsed >= StriderStompTick() + BossV5Ticks(0.38f))
		{
			m_Events |= 2;
			Slam(vec2(m_Pos.x, m_Ground < 1e8f ? m_Ground : m_Pos.y + STRIDER_HOVER), true);
		}
		break;
	case STRIDER_ACT_MORTAR:
	{
		if(!PartAlive(STRIDER_PART_MORTAR))
		{
			FinishAct();
			return;
		}
		AcquireTarget(false, false);
		const int Count = StriderShellCount(m_Phase);
		for(int Shot = 0; Shot < Count; Shot++)
		{
			if((m_Events & (1 << Shot)) || Elapsed < StriderShellTick(Shot))
				continue;
			m_Events |= 1 << Shot;
			FireShell(Shot);
		}
		break;
	}
	case STRIDER_ACT_LEAP:
	{
		if(!(m_Events & 1))
		{
			AcquireTarget(false);
			m_Aim = m_Pos + m_Target;
			if(Elapsed < StriderLeapWindup(m_Phase))
				break;
			m_Events |= 1;
			CCharacter *pTarget = TargetChr();
			vec2 Aim = pTarget ? pTarget->m_Pos + pTarget->GetVel() * 10.0f : m_Pos + m_Target;
			Aim.y -= STRIDER_HOVER - 40.0f;
			float Dist = distance(Aim, m_Pos);
			if(Dist > 760.0f)
			{
				Aim = m_Pos + (Aim - m_Pos) * (760.0f / Dist);
				Dist = 760.0f;
			}
			float Vx, Vy;
			BossV5Ballistic(Aim.x - m_Pos.x, Aim.y - m_Pos.y, StriderLeapFlight(Dist), STRIDER_GRAVITY, 28.0f, &Vx, &Vy);
			m_Vel = vec2(Vx, min(Vy, -9.0f));
			m_Airborne = true;
			m_LaunchTick = Server()->Tick();
			m_Aim = Aim;
			GameServer()->CreateSound(m_Pos, SOUND_WALKER_TAKEOFF);
			break;
		}
		if(m_Airborne)
		{
			const int Flight = Server()->Tick() - m_LaunchTick;
			if((Flight > 4 && m_Body.m_Grounded && m_Vel.y >= 0.0f) || Flight > 110)
			{
				m_Airborne = false;
				m_Events |= 2;
				m_Vel.x *= 0.2f;
				HurtPlayers(m_Pos + vec2(0, 40.0f), STRIDER_LEAP_RADIUS, STRIDER_LEAP_DAMAGE, 14.0f, 0.6f);
				Slam(vec2(m_Pos.x, m_Ground < 1e8f ? m_Ground : m_Pos.y + STRIDER_HOVER), m_Phase >= 1);
				// Hard landing leaves the vents open for a beat.
				m_VentUntil = Server()->Tick() + STRIDER_VENT_TICKS / 2;
				m_ActTick = Server()->Tick() - BossV5Ticks(StriderAct(STRIDER_ACT_LEAP).m_Duration) + BossV5Ticks(0.6f);
			}
		}
		break;
	}
	case STRIDER_ACT_ROAR:
		if(!(m_Events & 1) && Elapsed >= StriderRoarTick())
		{
			m_Events |= 1;
			HurtPlayers(m_Pos, STRIDER_ROAR_RADIUS, STRIDER_ROAR_DAMAGE, 16.0f, 0.5f);
			Slam(vec2(m_Pos.x, m_Ground < 1e8f ? m_Ground : m_Pos.y + STRIDER_HOVER), m_Phase >= 2);
			GameServer()->CreateSound(m_Pos, SOUND_CHARGE_FULL);
		}
		break;
	default:
		break;
	}
}

void CBastionStrider::Think()
{
	const bool Seen = AcquireTarget(true, false);
	if(!Seen && !AcquireTarget(false, false))
	{
		m_Move = 0;
		SetLocomotion(STRIDER_ACT_IDLE);
		return;
	}
	const float Fwd = m_Target.x * m_Dir; // distance in front of the shield
	const float Adx = fabsf(m_Target.x);
	const float Dy = m_Target.y;
	const float Dist = length(m_Target);
	const bool Legs = PartAlive(STRIDER_PART_LEGS);
	const bool Mortar = PartAlive(STRIDER_PART_MORTAR);

	// Under fire: answer instead of soaking it (stomp the flankers, otherwise ram or pounce).
	if(Pressured())
	{
		ClearPressure();
		if(!m_StompCooldown && Adx < 300.0f && fabsf(Dy) < 200.0f)
		{
			StartAct(STRIDER_ACT_STOMP);
			return;
		}
		if(Legs && !m_LeapCooldown && m_Supported)
		{
			StartAct(STRIDER_ACT_LEAP);
			return;
		}
		m_ReloadTimer = 0;
	}
	// Walled off for a while (leaps did not get it through): shell the target from here.
	if(StuckLong() && Mortar && !m_MortarCooldown && Dist > 200.0f)
	{
		m_BlockedTime = SERVER_TICK_SPEED * 2;
		StartAct(STRIDER_ACT_MORTAR);
		return;
	}
	// Pinned against a wall it cannot climb: vault it toward the target, or turn back out.
	if(m_StuckTicks > 20)
	{
		m_StuckTicks = 0;
		if(Legs && !m_LeapCooldown && m_Supported)
		{
			StartAct(STRIDER_ACT_LEAP);
			return;
		}
		if(Fwd > 0.0f && Dist > 300.0f && Mortar && !m_MortarCooldown)
		{
			StartAct(STRIDER_ACT_MORTAR);
			return;
		}
	}
	// Behind it: the quick turn is the flank window. Too close behind gets stomped instead.
	if(Fwd < -40.0f)
	{
		if(!m_StompCooldown && Adx < 180.0f && fabsf(Dy) < 170.0f)
			StartAct(STRIDER_ACT_STOMP);
		else
			StartAct(STRIDER_ACT_TURN);
		return;
	}
	// Anti-kite: up on a ledge, or far away.
	if(Legs && !m_LeapCooldown && m_Supported && (Dy < -150.0f || Dist > 620.0f))
	{
		StartAct(STRIDER_ACT_LEAP);
		return;
	}
	if(Seen && !m_ChargeCooldown && Fwd > 320.0f && fabsf(Dy) < 110.0f && FloorAhead(170.0f) && !WallAhead(m_Dir))
	{
		StartAct(STRIDER_ACT_CHARGE);
		return;
	}

	if(Fwd > 250.0f)
		m_Move = m_Dir;
	else if(Fwd < 110.0f)
		m_Move = -m_Dir;
	else
		m_Move = 0;

	if(m_ReloadTimer > 0)
	{
		SetLocomotion(m_Move ? STRIDER_ACT_WALK : STRIDER_ACT_IDLE);
		return;
	}
	if(!Seen)
	{
		if(Mortar && !m_MortarCooldown)
			StartAct(STRIDER_ACT_MORTAR);
		else
			SetLocomotion(m_Move ? STRIDER_ACT_WALK : STRIDER_ACT_IDLE);
		return;
	}
	if(Fwd < 260.0f && fabsf(Dy) < 120.0f)
	{
		if(!m_StompCooldown && frandom() < 0.3f)
			StartAct(STRIDER_ACT_STOMP);
		else
			StartAct(STRIDER_ACT_BASH);
	}
	else if(!m_ChargeCooldown && fabsf(Dy) < 90.0f && FloorAhead(170.0f) && !WallAhead(m_Dir) && frandom() < 0.6f)
		StartAct(STRIDER_ACT_CHARGE);
	else if(Mortar && !m_MortarCooldown)
		StartAct(STRIDER_ACT_MORTAR);
	else if(!m_StompCooldown && Adx < 520.0f && fabsf(Dy) < 80.0f)
		StartAct(STRIDER_ACT_STOMP);
	else
		SetLocomotion(m_Move ? STRIDER_ACT_WALK : STRIDER_ACT_IDLE);
}

// ---------------------------------------------------------------- body

void CBastionStrider::MoveBody()
{
	// Full-silhouette body under gravity (see CBossBody): the box is the whole drawn walker, from the
	// mortar to the soles, so no part of it can sink into walls, ceilings or floors, and it can only
	// stand where its box rests on real ground.
	m_PreVel = m_Vel;
	if(m_Health <= 0)
	{
		m_Vel.x *= 0.9f;
		WalkBody(0.0f, 0.0f, STRIDER_GRAVITY, false, 0.0f);
		return;
	}
	if(m_Airborne)
	{
		// Leap: ballistic, no steering. The leap act lands itself; anything else ends on touchdown.
		WalkBody(0.0f, 0.0f, STRIDER_GRAVITY, false, 0.0f);
		const int Flight = Server()->Tick() - m_LaunchTick;
		if(m_Act != STRIDER_ACT_LEAP && ((Flight > 4 && m_Body.m_Grounded) || Flight > 120))
			m_Airborne = false;
		return;
	}

	const bool Walking = m_Act == STRIDER_ACT_WALK && m_Move != 0;
	float Want = 0.0f, Accel = 0.8f;
	if(Walking)
		Want = m_Move * StriderSpeed(m_Phase, PartAlive(STRIDER_PART_LEGS));
	else if(m_Dashing)
	{
		Want = m_Vel.x; // the charge drives the velocity itself
		Accel = 100.0f;
	}
	else
	{
		Want = m_Vel.x * 0.82f;
		Accel = 100.0f;
	}
	// Keeps stepping up slopes and hops onto ledges up to STRIDER_MAX_HOP; a taller wall counts
	// m_StuckTicks for Think (leap over it or turn back).
	WalkBody(Want, Accel, STRIDER_GRAVITY, true, (Walking || m_Dashing) ? STRIDER_MAX_HOP : 0.0f);
}

void CBastionStrider::ExtraDebug(char *pBuffer, int Size)
{
	str_format(pBuffer, Size, "dir=%d air=%d dash=%d supported=%d", m_Dir, m_Airborne, m_Dashing, m_Supported);
}
