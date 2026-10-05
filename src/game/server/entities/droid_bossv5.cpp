#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_director.h>
#include "character.h"
#include "droid_bossv5.h"

CBossV5::CBossV5(CGameWorld *pGameWorld, vec2 Pos, int Type, int BaseHealth, const float *pPartShare,
	const CBossV5Hit *pHits, int NumHits, int DeathAct) :
	CDroid(pGameWorld, Pos, Type),
	m_pHits(pHits), m_NumHits(NumHits), m_DeathAct(DeathAct)
{
	m_ProximityRadius = 70.0f;
	m_StartPos = Pos;
	m_Pos = Pos;
	for(int i = 0; i < BOSSV5_MAX_SHOTS; i++)
		m_aShots[i].m_ID = -1;
	for(int i = 0; i < MAX_CHARACTERS; i++)
		m_aHitCooldown[i] = 0;

	m_Center = vec2(0, -10);
	m_Health = (int)(BaseHealth * BossV5DepthHealth(g_Config.m_SvMapGenLevel) + 0.5f);
	m_DamageScale = BossV5DepthDamage(g_Config.m_SvMapGenLevel);
	m_Pressure = 0.0f;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	for(int i = 0; i < BOSSV5_NUM_PARTS; i++)
	{
		m_aPartMax[i] = (int)(m_MaxHealth * pPartShare[i]);
		m_aPartHealth[i] = m_aPartMax[i];
	}
	m_Status = DROIDSTATUS_IDLE;
	m_Dir = -1;
	m_DeathTick = 0;
	SetState(0);
	m_TargetIndex = -1;
	m_ReloadTimer = 50;
	m_AttackTick = Server()->Tick();
	m_TargetTimer = 0;
	m_Target = vec2(0, 0);
	m_NewTarget = vec2(0, 0);
	m_Vel = vec2(0, 0);
	m_FlyTargetTick = 0;
	m_Mode = 0;
	m_FireDelay = 0;
	m_FireCount = 0;
	m_AttackTimer = 0;
	m_DamageTakenTick = 0;
	m_Act = 0;
	m_ActTick = Server()->Tick();
	m_Events = 0;
	m_Phase = 0;
	m_Stagger = 0;
	m_ActsDone = 0;
	m_Aim = Pos;
	m_Ground = Pos.y + 100.0f;
	m_Supported = false;
	m_StuckTicks = 0;
	m_BlockedX = false;
	m_BlockedTime = 0;
	m_LastX = Pos.x;
	m_Anim = DROIDANIM_IDLE;
	GameWorld()->InsertEntity(this);
}

CBossV5::~CBossV5()
{
	ClearShots();
}

int CBossV5::ScaleDamage(int Dmg)
{
	return max(1, (int)(Dmg * m_DamageScale * BossV5PhaseDamage(m_Phase) + 0.5f));
}

int CBossV5::Elapsed()
{
	return Server()->Tick() - m_ActTick;
}

void CBossV5::StartAct(int Act)
{
	m_Act = Act;
	m_ActTick = Server()->Tick();
	m_AttackTick = m_ActTick;
	m_Events = 0;
	m_Anim = ActInfo(Act).m_Anim;
	if(Act == m_DeathAct)
		m_Status = DROIDSTATUS_TERMINATED;
	OnActStart(Act);
}

void CBossV5::SetLocomotion(int Act)
{
	if(m_Act != Act)
		StartAct(Act);
}

void CBossV5::FinishAct()
{
	const int Act = m_Act;
	if(!IsLocomotion(Act))
		m_ActsDone++;
	StartAct(0);
	OnActFinished(Act);
}

bool CBossV5::AcquireTarget(bool NeedSight, bool Face)
{
	m_TargetIndex = -1;
	CCharacter *pClosest = 0;
	float Closest = 0.0f;
	const float RangeX = NeedSight ? 1150.0f : 1450.0f;
	const float RangeY = NeedSight ? 650.0f : 800.0f;
	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *pCharacter = GameServer()->GetPlayerChar(i);
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible() || pCharacter->m_IsBot)
			continue;
		if(fabsf(m_Pos.x - pCharacter->m_Pos.x) >= RangeX || fabsf(m_Pos.y - pCharacter->m_Pos.y) >= RangeY)
			continue;
		if(NeedSight && GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), m_Pos + vec2(0, -20)))
			continue;
		const float Distance = distance(pCharacter->m_Pos, m_Pos);
		if(!pClosest || Distance < Closest)
		{
			pClosest = pCharacter;
			Closest = Distance;
			m_TargetIndex = i;
		}
	}
	if(!pClosest)
		return false;
	m_Target = pClosest->m_Pos - m_Pos;
	if(Face)
		m_Dir = m_Target.x >= 0.0f ? 1 : -1;
	return true;
}

CCharacter *CBossV5::TargetChr()
{
	if(m_TargetIndex < 0)
		return 0;
	CCharacter *pChr = GameServer()->GetPlayerChar(m_TargetIndex);
	return pChr && pChr->IsAlive() ? pChr : 0;
}

int CBossV5::HurtPlayers(vec2 Pos, float Radius, int Dmg, float Knock, float Lift)
{
	int Hits = 0;
	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if(!pChr || !pChr->IsAlive() || pChr->m_IsBot)
			continue;
		vec2 Diff = pChr->m_Pos - Pos;
		const float Len = length(Diff);
		if(Len > Radius + 14.0f)
			continue;
		vec2 Force = vec2((float)m_Dir, 0.0f);
		if(Len > 0.01f)
			Force = Diff / Len;
		Force = vec2(Force.x * Knock, Force.y * Knock * 0.5f - Knock * Lift);
		pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(Dmg), Force, pChr->m_Pos);
		Hits++;
	}
	return Hits;
}

int CBossV5::HurtSegment(vec2 From, vec2 To, float Radius, int Dmg, float Knock, int *pCooldowns, int Cooldown)
{
	int Hits = 0;
	const vec2 Dir = distance(From, To) > 0.01f ? normalize(To - From) : vec2((float)m_Dir, 0.0f);
	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if(!pChr || !pChr->IsAlive() || pChr->m_IsBot)
			continue;
		if(distance(closest_point_on_line(From, To, pChr->m_Pos), pChr->m_Pos) > Radius + 14.0f)
			continue;
		if(pCooldowns)
		{
			if(pCooldowns[i] > Server()->Tick())
				continue;
			pCooldowns[i] = Server()->Tick() + Cooldown;
		}
		// Push away from the line, not along it, so beams and lattices shove players out.
		vec2 Side = pChr->m_Pos - closest_point_on_line(From, To, pChr->m_Pos);
		Side = length(Side) > 0.01f ? normalize(Side) : vec2(-Dir.y, Dir.x);
		const vec2 Push = normalize(Dir * 0.5f + Side);
		pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(Dmg), vec2(Push.x * Knock, Push.y * Knock - 3.0f), pChr->m_Pos);
		Hits++;
	}
	return Hits;
}

// ---------------------------------------------------------------- shots

int CBossV5::AddShot(int Kind, vec2 Pos, vec2 Vel, int Data, int Hp)
{
	for(int i = 0; i < BOSSV5_MAX_SHOTS; i++)
	{
		if(m_aShots[i].m_ID >= 0)
			continue;
		CShot &s = m_aShots[i];
		s.m_ID = Server()->SnapNewID();
		s.m_Kind = Kind;
		s.m_Pos = Pos;
		s.m_Vel = Vel;
		s.m_Life = 0;
		s.m_Data = Data;
		s.m_Hp = Hp;
		return i;
	}
	return -1;
}

void CBossV5::RemoveShot(int i)
{
	if(m_aShots[i].m_ID >= 0)
		Server()->SnapFreeID(m_aShots[i].m_ID);
	m_aShots[i].m_ID = -1;
}

void CBossV5::ClearShots()
{
	for(int i = 0; i < BOSSV5_MAX_SHOTS; i++)
		RemoveShot(i);
}

int CBossV5::CountShots(int Kind)
{
	int n = 0;
	for(int i = 0; i < BOSSV5_MAX_SHOTS; i++)
		if(m_aShots[i].m_ID >= 0 && m_aShots[i].m_Kind == Kind)
			n++;
	return n;
}

void CBossV5::SnapShot(const CShot &s, CNetObj_BossShot *pShot)
{
	pShot->m_X = (int)s.m_Pos.x;
	pShot->m_Y = (int)s.m_Pos.y;
	pShot->m_VelX = (int)(s.m_Vel.x * 100.0f);
	pShot->m_VelY = (int)(s.m_Vel.y * 100.0f);
	pShot->m_Kind = s.m_Kind;
}

// ---------------------------------------------------------------- body helpers

// Ground under the front, middle and rear of the body; the highest one wins, so a ramp or
// step ahead lifts the body before the box can touch it.
bool CBossV5::ProbeGround(float Hover, float Spread, float Extra)
{
	CCollision *pCollision = GameServer()->Collision();
	m_Ground = 1e9f;
	for(int i = -1; i <= 1; i++)
	{
		const vec2 From = m_Pos + vec2(i * Spread, 0.0f);
		vec2 At;
		if(pCollision->IntersectLine(From, From + vec2(0, Hover + Extra), &At, 0, false, true))
			m_Ground = min(m_Ground, At.y);
	}
	m_Supported = m_Ground < m_Pos.y + Hover + 40.0f;
	return m_Supported;
}

void CBossV5::SyncGround()
{
	m_Supported = m_Body.m_Grounded;
	const float Feet = m_Body.Feet(m_Pos);
	if(m_Supported)
	{
		m_Ground = Feet;
		return;
	}
	vec2 At;
	m_Ground = GameServer()->Collision()->IntersectLine(vec2(m_Pos.x, Feet - 4.0f), vec2(m_Pos.x, Feet + 800.0f), &At, 0, false, true) ?
		At.y :
		1e9f;
}

bool CBossV5::BodyWallAhead(int Dir, float Dist)
{
	CCollision *pCollision = GameServer()->Collision();
	const vec2 Ahead = m_Pos + vec2(Dir * Dist, 0.0f);
	return !m_Body.Fits(pCollision, Ahead, true) && !m_Body.Fits(pCollision, Ahead - vec2(0, m_Body.m_StepUp), true);
}

int CBossV5::WalkBody(float WantVx, float Accel, float Gravity, bool Walk, float MaxJump)
{
	CCollision *pCollision = GameServer()->Collision();
	const int Dir = WantVx > 0.5f ? 1 : (WantVx < -0.5f ? -1 : 0);
	if(Walk && Dir && m_Body.m_Grounded && m_Health > 0)
	{
		// Never stride off a drop unless the target is down there.
		if(m_Target.y < 96.0f && !m_Body.FloorAhead(pCollision, m_Pos, Dir, 220.0f))
			WantVx = 0.0f;
	}
	// Standing on a one-way platform with the target below: drop through it (like a player pressing down).
	if(m_Health > 0 && m_Body.m_OnPlatform && !m_Body.Dropping() && m_Target.y > 110.0f && fabsf(m_Target.x) < 700.0f)
		m_Body.DropThrough();
	if(Walk)
		m_Vel.x += clamp(WantVx - m_Vel.x, -Accel, Accel);
	m_Vel.y = min(m_Vel.y + Gravity, 24.0f);
	int Result = m_Body.Move(pCollision, &m_Pos, &m_Vel, Walk);
	m_BlockedX = Result & CBossBody::MOVE_BLOCKED_X;
	if(m_BlockedX && Walk && Dir && m_Body.m_Grounded && MaxJump > 0.0f)
	{
		// Taller than a step: hop onto it when it is within jumping height.
		const float Up = m_Body.LedgeAhead(pCollision, m_Pos, Dir, MaxJump);
		if(Up > 0.0f)
		{
			m_Vel = vec2(Dir * 4.5f, -sqrtf(2.0f * Gravity * (Up + 24.0f)));
			m_Body.m_Grounded = false;
			Result |= WALK_HOPPED;
		}
		else
			m_StuckTicks++;
	}
	else if(fabsf(m_Vel.x) > 1.0f || !Dir)
		m_StuckTicks = 0;
	if(Walk && Dir && m_Health > 0 && (m_BlockedX || fabsf(m_Vel.x) < 1.0f) && !(Result & WALK_HOPPED))
		m_BlockedTime++;
	else if(fabsf(m_Vel.x) > 2.0f && m_BlockedTime > 0)
		m_BlockedTime = max(0, m_BlockedTime - 3);
	SyncGround();
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 60);
	return Result;
}

float CBossV5::ProbeRide(float Hover, float Spread, float Far, float BoxHalfH, float Extra)
{
	CCollision *pCollision = GameServer()->Collision();
	const float aDx[5] = {-Far, -Spread, 0.0f, Spread, Far};
	float aY[5];
	bool aHas[5];
	float High = 1e9f, Sum = 0.0f;
	int Num = 0;
	for(int i = 0; i < 5; i++)
	{
		const vec2 From = m_Pos + vec2(aDx[i], -20.0f);
		vec2 At;
		aHas[i] = false;
		if(pCollision->CheckPoint(From.x, From.y))
		{
			aY[i] = From.y;
			aHas[i] = true;
		}
		else if(pCollision->IntersectLine(From, From + vec2(0, Hover + Extra), &At, 0, false, true))
		{
			aY[i] = At.y;
			aHas[i] = true;
		}
		if(!aHas[i])
			continue;
		if(i >= 1 && i <= 3)
			High = min(High, aY[i]);
		if(i == 0 || i == 2 || i == 4)
		{
			Sum += aY[i];
			Num++;
		}
	}
	if(High >= 1e9f)
		for(int i = 0; i < 5; i += 4)
			if(aHas[i])
				High = min(High, aY[i]);
	if(High >= 1e9f || !Num)
	{
		m_Ground = 1e9f;
		m_Supported = false;
		return m_Pos.y;
	}
	m_Ground = aHas[2] ? aY[2] : High;
	m_Supported = High < m_Pos.y + Hover + 40.0f;
	return min(Sum / Num - Hover, High - BoxHalfH - 12.0f);
}

void CBossV5::HoverSpringTo(float TargetY)
{
	const float Err = TargetY - m_Pos.y;
	m_Vel.y += clamp(Err * 0.12f, -3.0f, 3.0f);
	m_Vel.y *= 0.8f;
}

void CBossV5::HoverSpring(float Hover)
{
	const float Err = (m_Ground - Hover) - m_Pos.y;
	m_Vel.y += clamp(Err * 0.12f, -3.0f, 3.0f);
	m_Vel.y *= 0.8f;
}

void CBossV5::DropLoot()
{
	for(int i = 0; i < 4; i++)
	{
		const int Kind = frandom() < 0.34f ? POWERUP_AMMO : (frandom() < 0.5f ? POWERUP_ARMOR : POWERUP_KIT);
		GameServer()->m_pController->DropPickup(m_Pos, Kind, vec2(frandom() * 6.0f - frandom() * 6.0f, -frandom() * 14.0f), 0);
	}
	GameServer()->m_pController->DropWeapon(
		m_Pos, vec2(frandom() * 8.0f - frandom() * 8.0f, -frandom() * 14.0f), GameServer()->NewWeapon(CWeaponCatalog::Static(SW_UPGRADE)));
	GameServer()->m_pController->DropWeapon(
		m_Pos, vec2(frandom() * 8.0f - frandom() * 8.0f, -frandom() * 14.0f), GameServer()->NewWeapon(CWeaponCatalog::Static(SW_UPGRADE)));
	GameServer()->m_pController->DropWeapon(
		m_Pos, vec2(frandom() * 8.0f - frandom() * 8.0f, -frandom() * 14.0f), GameServer()->NewWeapon(CWeaponCatalog::Static(SW_RESPAWNER)));
}

// ---------------------------------------------------------------- tick

void CBossV5::Tick()
{
	if(DespawnIfUnsnapped())
		return;

	if(m_Health > 0 && GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
		TakeDamage(vec2(0, -0.5f), 12, CAttackSource::World(DAMAGETYPE_FLUID), vec2(0, 0));

	for(int i = 0; i < BOSSV5_MAX_SHOTS; i++)
		if(m_aShots[i].m_ID >= 0)
		{
			m_aShots[i].m_Life++;
			TickShot(i);
		}

	if(m_Health <= 0)
	{
		if(!m_DeathTick)
			m_DeathTick = Server()->Tick();
		m_Status = DROIDSTATUS_TERMINATED;
		m_Act = m_DeathAct;
		m_Anim = ActInfo(m_DeathAct).m_Anim;
		const int Since = Server()->Tick() - m_DeathTick;
		OnDeathTick(Since);
		MoveBody();
		if(Since < BossV5Ticks(ActInfo(m_DeathAct).m_Duration))
			return;
		GameServer()->CreateExplosion(m_Pos + m_Center, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true));
		GameServer()->CreateExplosion(m_Pos + vec2(-40.0f * m_Dir, 30.0f), CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
		DropLoot();
		GameServer()->m_World.DestroyEntity(this);
		return;
	}

	const int Phase = BossV5Phase(m_Health, m_MaxHealth);
	if(Phase > m_Phase)
	{
		m_Phase = Phase;
		m_Stagger = 0;
		StartAct(PhaseAct());
	}
	else if(m_Stagger && IsLocomotion(m_Act))
	{
		m_Stagger = 0;
		StartAct(StaggerAct());
	}

	if(m_ReloadTimer > 0)
		m_ReloadTimer--;
	m_Pressure *= BOSSV5_PRESSURE_DECAY;
	TickTimers();

	if(!IsLocomotion(m_Act))
	{
		const int Elapsed = Server()->Tick() - m_ActTick;
		TickAct(Elapsed);
		// TickAct may have switched acts; only time out the act that is still running.
		// (open-ended acts finish themselves; the duration is their safety cap).
		if(!IsLocomotion(m_Act) && Server()->Tick() - m_ActTick >= BossV5Ticks(ActInfo(m_Act).m_Duration))
			FinishAct();
	}
	else
		Think();

	MoveBody();

	if(Server()->Tick() > m_DamageTakenTick + 15 && m_Status == DROIDSTATUS_HURT)
		m_Status = DROIDSTATUS_IDLE;
}

// ---------------------------------------------------------------- snap

int CBossV5::StatusFlags()
{
	return Exposed() ? BOSSV5_FLAG_EXPOSED : 0;
}

void CBossV5::Snap(int SnappingClient)
{
	CPlayer *pPlayer = SnappingClient >= 0 ? GameServer()->m_apPlayers[SnappingClient] : 0;
	const bool Far = pPlayer && distance(pPlayer->m_ViewPos, m_Pos) > 2200.0f;

	for(int i = 0; i < BOSSV5_MAX_SHOTS; i++)
	{
		const CShot &s = m_aShots[i];
		if(s.m_ID < 0 || NetworkClipped(SnappingClient, s.m_Pos))
			continue;
		CNetObj_BossShot *pShot = static_cast<CNetObj_BossShot *>(Server()->SnapNewItem(NETOBJTYPE_BOSSSHOT, s.m_ID, sizeof(CNetObj_BossShot)));
		if(pShot)
			SnapShot(s, pShot);
	}
	if(Far)
		return;

	// The droid item is written here instead of CDroid::Snap so m_Angle can carry the boss flags.
	m_SnapTick = Server()->Tick();
	CNetObj_Droid *pP = static_cast<CNetObj_Droid *>(Server()->SnapNewItem(NETOBJTYPE_DROID, m_ID, sizeof(CNetObj_Droid)));
	if(pP)
	{
		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
		pP->m_Type = m_Type;
		pP->m_Status = m_Status;
		pP->m_AttackTick = m_Health <= 0 ? m_DeathTick : m_AttackTick;
		pP->m_Anim = m_Anim;
		pP->m_Dir = m_Dir;
		pP->m_Angle = m_Health > 0 ? StatusFlags() : 0;
	}

	CNetObj_BossStatus *pStatus = static_cast<CNetObj_BossStatus *>(Server()->SnapNewItem(NETOBJTYPE_BOSSSTATUS, m_ID, sizeof(CNetObj_BossStatus)));
	if(!pStatus)
		return;
	pStatus->m_Health = max(0, m_Health);
	pStatus->m_MaxHealth = m_MaxHealth;
	pStatus->m_Phase = m_Phase;
	pStatus->m_Part0 = m_MaxHealth > 0 ? clamp(m_Health * 100 / m_MaxHealth, 0, 100) : 0;
	int *apPart[3] = {&pStatus->m_Part1, &pStatus->m_Part2, &pStatus->m_Part3};
	for(int p = 1; p < BOSSV5_NUM_PARTS; p++)
		*apPart[p - 1] = m_aPartMax[p] > 0 ? clamp(m_aPartHealth[p] * 100 / m_aPartMax[p], 0, 100) : 0;
	pStatus->m_ArmOut = m_Health > 0 && Exposed() ? 1 : 0;
	const vec2 Aim = StatusAim();
	pStatus->m_ArmX = (int)Aim.x;
	pStatus->m_ArmY = (int)Aim.y;
}

// ---------------------------------------------------------------- damage

static void BossV5HitCircle(vec2 Pos0, vec2 Pos1, vec2 Center, float Range, float *pBest, vec2 *pAt)
{
	const vec2 P = closest_point_on_line(Pos0, Pos1, Center);
	if(distance(P, Center) >= Range)
		return;
	const float Along = distance(Pos0, P);
	if(Along < *pBest)
	{
		*pBest = Along;
		*pAt = P;
	}
}

bool CBossV5::HitSegment(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pAt)
{
	float Best = 1e9f;
	if(m_Health > 0 || Server()->Tick() < m_DeathTick + 20)
		for(int i = 0; i < m_NumHits; i++)
		{
			const CBossV5Hit &c = m_pHits[i];
			if(!HitCircleActive(c.m_Part))
				continue;
			BossV5HitCircle(Pos0, Pos1, LocalToWorld(c.m_X, c.m_Y), c.m_R + Radius, &Best, pAt);
		}
	for(int i = 0; i < BOSSV5_MAX_SHOTS; i++)
		if(m_aShots[i].m_ID >= 0 && ShotHittable(m_aShots[i]))
			BossV5HitCircle(Pos0, Pos1, m_aShots[i].m_Pos, 20.0f + Radius, &Best, pAt);
	return Best < 1e9f;
}

void CBossV5::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	if(m_Health <= 0 || !Dmg || Source.m_Kind == EAttackSourceKind::Droid || IgnoresMapObject(Source))
		return;

	const bool HasPos = Pos.x != 0.0f || Pos.y != 0.0f;
	int Part = 0;
	if(HasPos)
	{
		// Projectiles with hit points can be shot down first.
		for(int i = 0; i < BOSSV5_MAX_SHOTS; i++)
		{
			CShot &s = m_aShots[i];
			if(s.m_ID < 0 || !ShotHittable(s) || distance(Pos, s.m_Pos) > 30.0f)
				continue;
			s.m_Hp -= max(1, Dmg);
			if(s.m_Hp <= 0)
				OnShotDestroyed(i);
			return;
		}
		Part = BossV5PartAt(m_pHits, m_NumHits, (Pos.x - m_Pos.x) * m_Dir, Pos.y - m_Pos.y);
		if(!HitCircleActive(Part))
			Part = 0;
	}
	if(Invulnerable())
		return;

	if(g_Config.m_SvOneHitKill)
		Dmg = 1000;
	if(GameServer()->m_pPveDirector)
		Dmg = GameServer()->m_pPveDirector->ModifyDroidDamage(Source, Dmg, true, this);
	int PartDmg = -1;
	Dmg = IncomingDamage(Dmg, Part, HasPos ? Pos : m_Pos, &PartDmg);
	if(PartDmg < 0)
		PartDmg = Dmg;

	if(Part != 0 && m_aPartHealth[Part] > 0 && PartDmg > 0)
	{
		m_aPartHealth[Part] -= PartDmg;
		if(m_aPartHealth[Part] <= 0)
		{
			m_aPartHealth[Part] = 0;
			m_Stagger = 1;
			float x = 0.0f, y = 0.0f;
			for(int i = 0; i < m_NumHits; i++)
				if(m_pHits[i].m_Part == Part)
				{
					x = m_pHits[i].m_X;
					y = m_pHits[i].m_Y;
				}
			GameServer()->CreateExplosion(LocalToWorld(x, y), CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
			OnPartBroken(Part);
		}
	}
	const bool Fluid = Source.m_Kind == EAttackSourceKind::World && Source.m_Type == DAMAGETYPE_FLUID;
	if(Dmg > 0 && !Fluid && !Exposed())
		Dmg = max(1, (int)(Dmg * BossV5Armor(m_Phase) + 0.5f));
	if(Dmg <= 0)
		return;
	if(!Fluid)
		m_Pressure += Dmg;

	vec2 DmgPos = PresentDamage(Combat, Force, Dmg, Pos);
	CommitDamage(DmgPos, Dmg, Source);
	m_DamageTakenTick = Server()->Tick();

	if(m_Health <= 0)
	{
		m_DeathTick = Server()->Tick();
		StartAct(m_DeathAct);
		m_AttackTick = m_DeathTick;
		m_Status = DROIDSTATUS_TERMINATED;
		OnDeathStart();
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnDroidKilled(this, Source);
		CCharacter *pChr = GameServer()->GetPlayerChar(From);
		if(pChr)
			pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		return;
	}
	// No random flinching: v5 bosses only stagger when a part breaks (they soak and counter instead).
}

// ---------------------------------------------------------------- debug

void CBossV5::DebugAct(int Act)
{
	if(m_Health <= 0 || Act < 0 || Act >= m_DeathAct)
		return;
	AcquireTarget(false);
	StartAct(Act);
}

void CBossV5::DebugState(char *pBuffer, int Size)
{
	char aExtra[128];
	ExtraDebug(aExtra, sizeof(aExtra));
	str_format(pBuffer, Size, "%s act=%s phase=%d hp=%d/%d p1=%d p2=%d p3=%d pos=%.0f,%.0f vel=%.1f,%.1f ground=%.0f exposed=%d %s",
		m_Type == DROIDTYPE_BOSSBULKHEAD ? "strider" : (m_Type == DROIDTYPE_BOSSARC ? "seraph" : "monolith"), ActInfo(m_Act).m_pName,
		m_Phase, m_Health, m_MaxHealth, m_aPartHealth[1], m_aPartHealth[2], m_aPartHealth[3], m_Pos.x, m_Pos.y, m_Vel.x, m_Vel.y,
		m_Ground < 1e8f ? m_Ground - m_Pos.y : -1.0f, Exposed() ? 1 : 0, aExtra);
}
