#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "character.h"
#include "droid_siegemonolith.h"
#include "droid_star.h"

// Collision body == drawn silhouette: ground form spans the planted struts, air form the hull
// with the struts tucked. Both reach from the launcher/turret top to the soles (strut feet).
static const CBossBodyShape s_MonolithGroundShape = {MONOLITH_BODY_HALF_W, MONOLITH_BODY_TOP, MONOLITH_HOVER, MONOLITH_MAX_CROUCH};
static const CBossBodyShape s_MonolithAirShape = {MONOLITH_AIR_HALF_W, MONOLITH_BODY_TOP, MONOLITH_AIR_BOTTOM, 0.0f};

CSiegeMonolith::CSiegeMonolith(CGameWorld *pGameWorld, vec2 Pos) :
	CBossV5(pGameWorld, Pos, DROIDTYPE_BOSSVAULT, MONOLITH_HEALTH, s_aMonolithPartShare, s_aMonolithHit, NUM_MONOLITH_HIT, MONOLITH_ACT_DEATH)
{
	m_ProximityRadius = 80.0f;
	m_Air = false;
	m_Landed = false;
	m_Falling = false;
	m_FormActs = 0;
	m_Move = 0;
	m_SweepCooldown = 40;
	m_MortarCooldown = 0;
	m_BombCooldown = 0;
	m_DroneCooldown = 200;
	m_SlamCooldown = 60;
	m_FormCooldown = 220;
	m_SweepFrom = 0.0f;
	m_SweepTo = 0.0f;
	m_TurretAngle = 0.0f;
	m_BombDir = 1;
	m_FlyTo = Pos;
	m_Body.Init(s_MonolithGroundShape, MONOLITH_STEP_UP);
}

bool CSiegeMonolith::Exposed()
{
	if(m_Act == MONOLITH_ACT_TRANSFORM || m_Act == MONOLITH_ACT_STAGGER)
		return true;
	return m_Act == MONOLITH_ACT_SLAM && m_Landed; // vents dump heat after the crash
}

bool CSiegeMonolith::HitCircleActive(int Part)
{
	if(Part == MONOLITH_PART_VENTS)
		return PartAlive(Part) && Exposed(); // closed vents are just hull
	return Part == 0 || PartAlive(Part);
}

int CSiegeMonolith::StatusFlags()
{
	int Flags = CBossV5::StatusFlags();
	if(m_Air)
		Flags |= BOSSV5_FLAG_AIR;
	if(m_Act == MONOLITH_ACT_SWEEP && Elapsed() >= MonolithSweepStart() && Elapsed() < MonolithSweepEnd(m_Phase))
		Flags |= BOSSV5_FLAG_BEAM;
	if(m_Falling || (m_Air && m_Act == MONOLITH_ACT_BOMB))
		Flags |= BOSSV5_FLAG_THRUST;
	return Flags;
}

int CSiegeMonolith::IncomingDamage(int Dmg, int Part, vec2 Pos, int *pPartDmg)
{
	if(Part == MONOLITH_PART_CORE && !Exposed() && !PartAlive(MONOLITH_PART_VENTS))
		return max(1, Dmg); // with the vents wrecked the armour no longer holds
	return MonolithIncomingDamage(Dmg, Part, Exposed());
}

void CSiegeMonolith::OnPartBroken(int Part)
{
	GameServer()->CreateExplosion(m_Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	if(Part == MONOLITH_PART_THRUSTERS && m_Air)
	{
		// Thrusters gone mid-air: it drops like a rock and lies open.
		m_Air = false;
		m_Falling = true;
		m_Stagger = 1;
	}
}

void CSiegeMonolith::TickTimers()
{
	int *apTimer[] = {&m_SweepCooldown, &m_MortarCooldown, &m_BombCooldown, &m_DroneCooldown, &m_SlamCooldown, &m_FormCooldown};
	for(int *p : apTimer)
		if(*p > 0)
			(*p)--;
	if(m_Act != MONOLITH_ACT_SWEEP && m_Health > 0)
	{
		// The turret tracks the target between attacks (clamped so it never points backwards).
		const float Want = clamp(AimAngle(m_Pos + m_Target), -1.3f, 1.3f);
		m_TurretAngle += clamp(Want - m_TurretAngle, -0.06f, 0.06f);
	}
}

vec2 CSiegeMonolith::StatusAim()
{
	if(m_Act == MONOLITH_ACT_SWEEP)
		return m_Aim; // beam end (along the turret)
	const vec2 Pivot = LocalToWorld(MONOLITH_TURRET_X, MONOLITH_TURRET_Y);
	return Pivot + vec2(cosf(m_TurretAngle) * m_Dir, sinf(m_TurretAngle)) * 300.0f;
}

void CSiegeMonolith::OnActStart(int Act)
{
	m_Landed = false;
	if(Act == MONOLITH_ACT_SLAM)
		m_Falling = false;
	if(Act == MONOLITH_ACT_BOMB)
		m_BombDir = m_Target.x >= 0.0f ? 1 : -1;
}

void CSiegeMonolith::OnActFinished(int Act)
{
	static const int s_aReload[3] = {8, 5, 2};
	m_ReloadTimer = s_aReload[m_Phase];
	const float Speed = m_Phase >= 2 ? 0.7f : (m_Phase >= 1 ? 0.85f : 1.0f);
	if(Act == MONOLITH_ACT_SWEEP)
		m_SweepCooldown = (int)(150 * Speed);
	if(Act == MONOLITH_ACT_MORTAR)
		m_MortarCooldown = (int)(100 * Speed);
	if(Act == MONOLITH_ACT_BOMB)
		m_BombCooldown = (int)(130 * Speed);
	if(Act == MONOLITH_ACT_DRONES)
		m_DroneCooldown = (int)(420 * Speed);
	if(Act == MONOLITH_ACT_SLAM)
		m_SlamCooldown = (int)(110 * Speed);
	if(Act == MONOLITH_ACT_TRANSFORM)
	{
		m_FormCooldown = (int)(150 * Speed);
		m_FormActs = 0;
	}
	else if(Act != MONOLITH_ACT_ROAR && Act != MONOLITH_ACT_STAGGER)
		m_FormActs++;
}

// ---------------------------------------------------------------- shots

int CSiegeMonolith::AliveDrones()
{
	CEntity *apEnts[64];
	const int Num = GameServer()->m_World.FindEntities(m_Pos, 1800.0f, apEnts, 64, CGameWorld::ENTTYPE_DROID);
	int Count = 0;
	for(int i = 0; i < Num; i++)
	{
		CDroid *pDroid = static_cast<CDroid *>(apEnts[i]);
		if(pDroid != this && pDroid->m_Type == DROIDTYPE_STAR && pDroid->m_Health > 0)
			Count++;
	}
	return Count;
}

float CSiegeMonolith::AimAngle(vec2 To)
{
	const vec2 Pivot = LocalToWorld(MONOLITH_TURRET_X, MONOLITH_TURRET_Y);
	const vec2 d = To - Pivot;
	return atan2f(d.y, d.x * m_Dir);
}

vec2 CSiegeMonolith::Muzzle()
{
	const vec2 l = MonolithMuzzleLocal(m_TurretAngle);
	return LocalToWorld(l.x, l.y);
}

void CSiegeMonolith::FireShell(int Shot)
{
	CCharacter *pTarget = TargetChr();
	vec2 Aim = pTarget ? pTarget->m_Pos + pTarget->GetVel() * 18.0f : m_Pos + m_Target;
	// Spread the volley so standing still and running both get punished.
	Aim.x += (Shot - (MonolithShellCount(m_Phase) - 1) * 0.5f) * 90.0f;
	const vec2 From = LocalToWorld(s_MonolithLauncher.m_X, s_MonolithLauncher.m_Y);
	const float T = clamp(distance(From, Aim) / 14.0f, 30.0f, 75.0f);
	float Vx, Vy;
	BossV5Ballistic(Aim.x - From.x, Aim.y - From.y, T, MONOLITH_SHELL_GRAVITY, 22.0f, &Vx, &Vy);
	if(AddShot(MONOLITH_SHOT_SHELL, From, vec2(Vx, Vy), 0, 1) >= 0)
		GameServer()->CreateSound(From, SOUND_BAZOOKA_FIRE);
}

void CSiegeMonolith::Blast(vec2 At)
{
	AddShot(MONOLITH_SHOT_BLAST, At, vec2(0, 0));
}

void CSiegeMonolith::Explode(int i, vec2 At, float Radius, int Dmg)
{
	GameServer()->CreateExplosion(At, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	GameServer()->CreateSound(At, SOUND_GRENADE_EXPLODE);
	HurtPlayers(At, Radius, Dmg, 9.0f, 0.4f);
	RemoveShot(i);
	Blast(At);
}

void CSiegeMonolith::OnShotDestroyed(int i)
{
	GameServer()->CreateExplosion(m_aShots[i].m_Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	RemoveShot(i);
}

void CSiegeMonolith::SnapShot(const CShot &s, CNetObj_BossShot *pShot)
{
	CBossV5::SnapShot(s, pShot);
	if(s.m_Kind == MONOLITH_SHOT_BLAST)
		pShot->m_VelY = s.m_Life;
}

void CSiegeMonolith::TickShot(int i)
{
	CCollision *pCollision = GameServer()->Collision();
	CShot &s = m_aShots[i];
	if(s.m_Kind == MONOLITH_SHOT_BLAST)
	{
		if(s.m_Life > 20)
			RemoveShot(i);
		return;
	}
	const bool Bomb = s.m_Kind == MONOLITH_SHOT_BOMB;
	s.m_Vel.y += Bomb ? MONOLITH_BOMB_GRAVITY : MONOLITH_SHELL_GRAVITY;
	const vec2 Next = s.m_Pos + s.m_Vel;
	const float Radius = Bomb ? MONOLITH_BOMB_RADIUS : MONOLITH_SHELL_RADIUS;
	const int Dmg = Bomb ? MONOLITH_BOMB_DAMAGE : MONOLITH_SHELL_DAMAGE;
	CCharacter *pHit = GameServer()->m_World.ClosestCharacter(Next, 28.0f, 0);
	vec2 At;
	if(pHit && pHit->IsAlive() && !pHit->m_IsBot)
		Explode(i, Next, Radius, Dmg);
	else if(pCollision->IntersectLine(s.m_Pos, Next, &At, 0, false, true))
		Explode(i, At, Radius, Dmg);
	else if(s.m_Life > 260)
		RemoveShot(i);
	else
		s.m_Pos = Next;
}

// ---------------------------------------------------------------- acts

void CSiegeMonolith::SlamImpact()
{
	m_Landed = true;
	m_Falling = false;
	m_Air = false;
	m_Vel = vec2(0, 0);
	const vec2 Floor(m_Pos.x, m_Ground < 1e8f ? m_Ground : m_Pos.y + MONOLITH_HOVER);
	// Directly underneath gets crushed, the rest gets the pound.
	HurtPlayers(Floor + vec2(0, -40.0f), 90.0f, MONOLITH_CRUSH_DAMAGE - MONOLITH_POUND_DAMAGE, 6.0f, 0.2f);
	HurtPlayers(Floor + vec2(0, -30.0f), MONOLITH_POUND_RADIUS, MONOLITH_POUND_DAMAGE, 15.0f, 0.75f);
	GameServer()->CreateExplosion(Floor + vec2(-60.0f, -20.0f), CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	GameServer()->CreateExplosion(Floor + vec2(60.0f, -20.0f), CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
	GameServer()->CreateBuildingHit(Floor);
	GameServer()->CreateSound(m_Pos, SOUND_BODY_LAND);
	Blast(Floor);
	m_ActTick = Server()->Tick() - BossV5Ticks(MonolithAct(MONOLITH_ACT_SLAM).m_Duration) + BossV5Ticks(1.1f); // 1.1 s recovery
}

void CSiegeMonolith::TickAct(int Elapsed)
{
	CCharacter *pTarget = TargetChr();
	switch(m_Act)
	{
	case MONOLITH_ACT_SWEEP:
	{
		if(Elapsed < MonolithSweepStart())
		{
			// Telegraph: the laser sight sits on the target, then the beam sweeps through it.
			if(AcquireTarget(false))
				pTarget = TargetChr();
			const vec2 To = pTarget ? pTarget->m_Pos : m_Pos + m_Target;
			const float Center = AimAngle(To);
			const float Half = MonolithSweepArc() * 0.5f;
			// Sweep from above down onto the floor the target is standing on.
			m_SweepFrom = Center - Half;
			m_SweepTo = Center + Half;
			m_TurretAngle = m_SweepFrom;
			if(Elapsed == 1)
				GameServer()->CreateSound(m_Pos, SOUND_CHARGE_UP);
		}
		else
		{
			float t = clamp((Elapsed - MonolithSweepStart()) / (float)max(1, MonolithSweepEnd(m_Phase) - MonolithSweepStart()), 0.0f, 1.0f);
			if(m_Phase >= 2)
				t = t < 0.5f ? t * 2.0f : 2.0f - t * 2.0f; // there and back
			m_TurretAngle = m_SweepFrom + (m_SweepTo - m_SweepFrom) * t;
			if(Elapsed == MonolithSweepStart())
				GameServer()->CreateSound(m_Pos, SOUND_DEATHRAY);
		}
		const vec2 From = Muzzle();
		const vec2 Dir(cosf(m_TurretAngle) * m_Dir, sinf(m_TurretAngle));
		vec2 To = From + Dir * MONOLITH_BEAM_RANGE;
		vec2 Wall;
		if(GameServer()->Collision()->IntersectLine(From, To, &Wall, 0))
			To = Wall;
		m_Aim = To;
		if(Elapsed >= MonolithSweepStart() && Elapsed < MonolithSweepEnd(m_Phase))
		{
			HurtSegment(From, To, MONOLITH_BEAM_RADIUS, MONOLITH_BEAM_DAMAGE, 4.0f, m_aHitCooldown, 10);
			if((Elapsed % 6) == 0)
				GameServer()->CreateBuildingHit(To);
		}
		if(Elapsed >= MonolithSweepEnd(m_Phase))
			FinishAct();
		break;
	}
	case MONOLITH_ACT_MORTAR:
		AcquireTarget(false);
		for(int Shot = 0; Shot < MonolithShellCount(m_Phase); Shot++)
		{
			if((m_Events & (1 << Shot)) || Elapsed < MonolithShellTick(Shot))
				continue;
			m_Events |= 1 << Shot;
			FireShell(Shot);
		}
		break;
	case MONOLITH_ACT_TRANSFORM:
		if(!(m_Events & 1))
		{
			m_Events |= 1;
			GameServer()->CreateSound(m_Pos, SOUND_WALKER_TAKEOFF);
		}
		if(!(m_Events & 2) && Elapsed >= BossV5Ticks(0.8f))
		{
			m_Events |= 2;
			const bool WantAir = !m_Air && PartAlive(MONOLITH_PART_THRUSTERS);
			// Struts fold in (slimmer box) or fold out (wider box: only where there is room for them).
			if(m_Body.SetShape(GameServer()->Collision(), &m_Pos, WantAir ? s_MonolithAirShape : s_MonolithGroundShape))
			{
				m_Air = WantAir;
				if(m_Air)
					m_Vel.y = -9.0f; // blast off instead of drifting up
				else
					m_Falling = true; // settles down onto the struts
			}
			GameServer()->CreateSound(m_Pos, SOUND_JETPACK);
		}
		break;
	case MONOLITH_ACT_BOMB:
	{
		// Carpet run: cross over the target and keep going, a bomb every few ticks.
		const float Base = pTarget ? pTarget->m_Pos.y : m_Pos.y + m_Target.y;
		m_FlyTo = vec2(m_Pos.x + m_BombDir * 200.0f, Base - MONOLITH_AIR_UP);
		if(Elapsed > BossV5Ticks(0.4f) && Elapsed < BossV5Ticks(0.4f) + MonolithBombTicks() && (Elapsed % MonolithBombEvery(m_Phase)) == 0)
		{
			AddShot(MONOLITH_SHOT_BOMB, m_Pos + vec2(0, 110.0f), vec2(m_Vel.x * 0.5f, 2.0f));
			if((Elapsed % (MonolithBombEvery(m_Phase) * 3)) == 0)
				GameServer()->CreateSound(m_Pos, SOUND_BOMB_BEEP);
		}
		if(WallAhead(m_BombDir))
			m_BombDir = -m_BombDir;
		break;
	}
	case MONOLITH_ACT_DRONES:
		if(!(m_Events & 1) && Elapsed >= MonolithDroneTick())
		{
			m_Events |= 1;
			const int Room = MONOLITH_MAX_DRONES - AliveDrones();
			for(int n = 0; n < min(Room, MonolithDroneCount(m_Phase)); n++)
			{
				vec2 P = LocalToWorld(s_MonolithLauncher.m_X, s_MonolithLauncher.m_Y - 40.0f) + vec2((n - 1) * 60.0f, -n * 20.0f);
				for(int k = 0; k < 5 && GameServer()->Collision()->TestBox(P, vec2(40.0f, 40.0f)); k++)
					P.y += 20.0f;
				if(GameServer()->Collision()->TestBox(P, vec2(40.0f, 40.0f)))
					continue;
				new CStar(GameWorld(), P);
				GameServer()->CreateSound(P, SOUND_SPAWN);
			}
		}
		break;
	case MONOLITH_ACT_SLAM:
	{
		if(m_Landed)
			break; // recovery, vents open
		const int Rise = MonolithSlamRise() + (m_Air ? BossV5Ticks(0.35f) : 0);
		if(Elapsed < Rise)
		{
			if(pTarget)
				m_FlyTo = vec2(pTarget->m_Pos.x, (m_Air ? pTarget->m_Pos.y - MONOLITH_AIR_UP - 60.0f : m_Pos.y - 10.0f));
			if(!m_Air)
				m_Vel.y = -7.0f; // hop
			if(Elapsed == 1)
				GameServer()->CreateSound(m_Pos, SOUND_CHARGE_UP);
			break;
		}
		if(!m_Falling)
		{
			m_Falling = true;
			for(int c = 0; c < MAX_CHARACTERS; c++)
				m_aHitCooldown[c] = 0;
			GameServer()->CreateSound(m_Pos, SOUND_DASH);
		}
		m_Vel = vec2(0, 26.0f);
		HurtPlayers(m_Pos + vec2(0, 130.0f), 70.0f, MONOLITH_CRUSH_DAMAGE / 2, 10.0f, 0.0f);
		if(m_Body.m_Grounded || Elapsed > Rise + 70)
			SlamImpact();
		break;
	}
	case MONOLITH_ACT_ROAR:
		if(!(m_Events & 1) && Elapsed >= MonolithRoarTick())
		{
			m_Events |= 1;
			HurtPlayers(m_Pos, MONOLITH_ROAR_RADIUS, MONOLITH_ROAR_DAMAGE, 16.0f, 0.5f);
			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE);
		}
		break;
	default:
		break;
	}
}

void CSiegeMonolith::Think()
{
	const bool Seen = AcquireTarget(true);
	if(!Seen && !AcquireTarget(false))
	{
		m_Move = 0;
		SetLocomotion(MONOLITH_ACT_IDLE);
		return;
	}
	const float Dist = length(m_Target);
	const float AbsX = fabsf(m_Target.x);
	const bool Thrusters = PartAlive(MONOLITH_PART_THRUSTERS);
	const bool Turret = PartAlive(MONOLITH_PART_TURRET);

	// Ground form never stands still: it presses in when far, backs off huggers, and otherwise
	// strafes back and forth inside its firing band so it is never a free target.
	if(!m_Air)
	{
		if(AbsX > 480.0f)
			m_Move = m_Target.x > 0 ? 1 : -1;
		else if(AbsX < 240.0f)
			m_Move = m_Target.x > 0 ? -1 : 1;
		else
		{
			if(m_Move == 0 || (Server()->Tick() % 70) == 0)
				m_Move = frandom() < 0.5f ? -1 : 1;
		}
	}
	else
		m_Move = 0;

	// Under fire: answer instead of soaking it.
	if(Pressured() && m_Act != MONOLITH_ACT_SLAM)
	{
		ClearPressure();
		if(!m_SlamCooldown && AbsX < 420.0f)
		{
			StartAct(MONOLITH_ACT_SLAM);
			return;
		}
		if(!m_Air && Thrusters && !m_FormCooldown)
		{
			StartAct(MONOLITH_ACT_TRANSFORM);
			return;
		}
		m_ReloadTimer = 0;
	}
	// Walled in on foot and cannot fly out: shell from here.
	if(!m_Air && StuckLong() && !m_MortarCooldown)
	{
		m_BlockedTime = SERVER_TICK_SPEED * 2;
		StartAct(MONOLITH_ACT_MORTAR);
		return;
	}

	if(m_ReloadTimer > 0)
	{
		SetLocomotion(MONOLITH_ACT_MOVE);
		return;
	}
	if(!m_Air)
	{
		if(AbsX < 260.0f && fabsf(m_Target.y) < 220.0f && !m_SlamCooldown)
			StartAct(MONOLITH_ACT_SLAM); // anti-hug pound
		else if(Thrusters && !m_FormCooldown && (Dist > 700.0f || m_Target.y < -220.0f || !Seen || m_FormActs >= 2))
			StartAct(MONOLITH_ACT_TRANSFORM); // anti-kite / anti-camp: take off
		else if(Turret && Seen && !m_SweepCooldown)
			StartAct(MONOLITH_ACT_SWEEP);
		else if(m_Phase >= 1 && !m_DroneCooldown && AliveDrones() < MONOLITH_MAX_DRONES)
			StartAct(MONOLITH_ACT_DRONES);
		else if(!m_MortarCooldown)
			StartAct(MONOLITH_ACT_MORTAR);
		else
			SetLocomotion(MONOLITH_ACT_MOVE);
		return;
	}
	if(AbsX < 170.0f && m_Target.y > 0.0f && !m_SlamCooldown)
		StartAct(MONOLITH_ACT_SLAM);
	else if(!m_BombCooldown)
		StartAct(MONOLITH_ACT_BOMB);
	else if(!m_DroneCooldown && AliveDrones() < MONOLITH_MAX_DRONES)
		StartAct(MONOLITH_ACT_DRONES);
	else if(Turret && Seen && !m_SweepCooldown)
		StartAct(MONOLITH_ACT_SWEEP);
	else if(m_FormActs >= 4 && !m_FormCooldown)
		StartAct(m_SlamCooldown ? MONOLITH_ACT_TRANSFORM : MONOLITH_ACT_SLAM);
	else
		SetLocomotion(MONOLITH_ACT_MOVE);
}

// ---------------------------------------------------------------- body

bool CSiegeMonolith::WallAhead(int Dir)
{
	return BodyWallAhead(Dir, 70.0f);
}

void CSiegeMonolith::MoveBody()
{
	CCollision *pCollision = GameServer()->Collision();
	// Out of the air form some other way (thrusters shot off mid-flight): fold the struts out.
	if(!m_Air && m_Body.m_Shape.m_HalfW != s_MonolithGroundShape.m_HalfW)
		m_Body.SetShape(pCollision, &m_Pos, s_MonolithGroundShape);

	if(m_Health <= 0)
	{
		m_Vel.x *= 0.9f;
		WalkBody(0.0f, 0.0f, MONOLITH_GRAVITY, false, 0.0f);
		return;
	}
	if(fabsf(m_Target.x) > 30.0f && m_Act != MONOLITH_ACT_BOMB && m_Act != MONOLITH_ACT_SWEEP)
		m_Dir = m_Target.x > 0 ? 1 : -1;
	if(m_Act == MONOLITH_ACT_BOMB)
		m_Dir = m_BombDir;

	const bool Slamming = m_Act == MONOLITH_ACT_SLAM && m_Falling && !m_Landed;
	if(Slamming)
	{
		// Velocity set by the act; straight down until the box hits something.
		WalkBody(0.0f, 0.0f, 0.0f, false, 0.0f);
		return;
	}
	if(m_Air)
	{
		vec2 Want = m_FlyTo;
		float MaxSpeed = MonolithSpeed(m_Phase, true);
		if(m_Act != MONOLITH_ACT_BOMB && m_Act != MONOLITH_ACT_SLAM)
		{
			CCharacter *pTarget = TargetChr();
			const vec2 Base = pTarget ? pTarget->m_Pos : m_Pos + m_Target;
			// Weave from side to side over the target instead of parking.
			const float Side = sinf(Server()->Tick() * 0.025f);
			Want = Base + vec2(Side * 300.0f, -MONOLITH_AIR_UP + sinf(Server()->Tick() * 0.05f) * 30.0f);
			vec2 At;
			if(pCollision->IntersectLine(Base + vec2(0, -30.0f), Want, &At, 0))
				Want = Base + vec2(0, -30.0f) + (At - Base - vec2(0, -30.0f)) * 0.7f;
			if(m_Act == MONOLITH_ACT_SWEEP || m_Act == MONOLITH_ACT_DRONES || m_Act == MONOLITH_ACT_TRANSFORM || m_Act == MONOLITH_ACT_ROAR)
				MaxSpeed *= 0.55f;
		}
		else if(m_Act == MONOLITH_ACT_BOMB)
			MaxSpeed *= 1.15f;
		vec2 Acc = (Want - m_Pos) * 0.04f;
		if(length(Acc) > 1.1f)
			Acc = normalize(Acc) * 1.1f;
		m_Vel += Acc;
		m_Vel *= 0.94f;
		if(length(m_Vel) > MaxSpeed)
			m_Vel = normalize(m_Vel) * MaxSpeed;
		// Flying: full-box collision, no gravity, slides along walls and ceilings.
		WalkBody(0.0f, 0.0f, 0.0f, false, 0.0f);
		return;
	}

	// Ground form: it keeps gliding while it shells, launches drones or sweeps (slower while the beam is live).
	const bool Moving = m_Move != 0 && (m_Act == MONOLITH_ACT_MOVE || m_Act == MONOLITH_ACT_MORTAR || m_Act == MONOLITH_ACT_DRONES ||
		(m_Act == MONOLITH_ACT_SWEEP && m_Phase >= 1));
	float Want = m_Vel.x * 0.82f, Accel = 100.0f;
	if(Moving)
	{
		const float Mult = m_Act == MONOLITH_ACT_MOVE ? 1.0f : (m_Act == MONOLITH_ACT_SWEEP ? 0.35f : 0.6f);
		Want = m_Move * MonolithSpeed(m_Phase, false) * Mult;
		Accel = 0.8f;
		// Don't back off a ledge: strafe the other way (unless the target is down there).
		if(m_Body.m_Grounded && m_Target.y < 96.0f && !m_Body.FloorAhead(pCollision, m_Pos, m_Move, 200.0f))
		{
			m_Move = -m_Move;
			Want = 0.0f;
		}
	}
	if(m_Act == MONOLITH_ACT_SLAM && !m_Landed)
	{
		Want = clamp((m_FlyTo.x - m_Pos.x) * 0.05f, -8.0f, 8.0f); // hop toward the hugger
		Accel = 0.7f;
	}
	WalkBody(Want, Accel, MONOLITH_GRAVITY, true, Moving ? MONOLITH_MAX_HOP : 0.0f);
	if(m_Body.m_Grounded)
		m_Falling = false;
	// A wall too tall to hop: fly over it.
	if(Moving && m_StuckTicks > 20 && PartAlive(MONOLITH_PART_THRUSTERS) && !m_FormCooldown)
	{
		m_StuckTicks = 0;
		StartAct(MONOLITH_ACT_TRANSFORM);
	}
}

void CSiegeMonolith::ExtraDebug(char *pBuffer, int Size)
{
	str_format(pBuffer, Size, "air=%d falling=%d landed=%d formacts=%d drones=%d turret=%.2f", m_Air, m_Falling, m_Landed, m_FormActs,
		AliveDrones(), m_TurretAngle);
}
