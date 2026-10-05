#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/skitter_matriarch.h>
#include <game/boss_v5.h> // shared depth/phase scaling and pressure tuning
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_director.h>
#include "character.h"
#include "droid_crawler.h"
#include "droid_skittermatriarch.h"

static const CBossBodyShape s_MatriarchShape = {MATRIARCH_BODY_HALF_W, MATRIARCH_BODY_TOP, MATRIARCH_HOVER, MATRIARCH_MAX_CROUCH};
static const vec2 s_EggBox(26.0f, 26.0f);

CSkitterMatriarch::CSkitterMatriarch(CGameWorld *pGameWorld, vec2 Pos) : CDroid(pGameWorld, Pos, DROIDTYPE_BOSSRAIL)
{
	m_ProximityRadius = 70.0f;
	m_StartPos = Pos;
	for(int i = 0; i < MATRIARCH_MAX_SHOTS; i++)
		m_aShots[i].m_ID = -1;
	Reset();
	GameWorld()->InsertEntity(this);
}

CSkitterMatriarch::~CSkitterMatriarch()
{
	ClearShots();
}

void CSkitterMatriarch::Reset()
{
	m_Center = vec2(0, -10);
	m_Health = MATRIARCH_HEALTH;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_Health = (int)(m_Health * BossV5DepthHealth(g_Config.m_SvMapGenLevel) + 0.5f);
	m_MaxHealth = m_Health;
	m_DamageScale = BossV5DepthDamage(g_Config.m_SvMapGenLevel);
	m_Pressure = 0.0f;
	for(int i = 0; i < NUM_MATRIARCH_PARTS; i++)
	{
		m_aPartMax[i] = (int)(m_MaxHealth * s_aMatriarchPartShare[i]);
		m_aPartHealth[i] = m_aPartMax[i];
	}
	m_Pos = m_StartPos;
	m_Status = DROIDSTATUS_IDLE;
	m_Dir = -1;
	m_DeathTick = 0;
	SetState(0);
	m_TargetIndex = -1;
	m_ReloadTimer = Server()->TickSpeed();
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
	m_Act = MATRIARCH_ACT_IDLE;
	m_ActTick = Server()->Tick();
	m_Events = 0;
	m_Phase = 0;
	m_Stagger = 0;
	m_ActsDone = 0;
	m_PounceCooldown = 20;
	m_SpitCooldown = 0;
	m_BroodCooldown = 220;
	m_ThrashCooldown = 0;
	m_Move = 0;
	m_MoveTimer = 0;
	m_StuckTicks = 0;
	m_LastX = m_Pos.x;
	m_Supported = false;
	m_Climbing = false;
	m_PreVel = vec2(0, 0);
	m_Ground = m_Pos.y + MATRIARCH_HOVER;
	m_BlockedTime = 0;
	m_Body.Init(s_MatriarchShape, MATRIARCH_STEP_UP);
	m_Airborne = false;
	m_Chained = false;
	m_ContactHit = false;
	m_LaunchTick = 0;
	m_LandTick = 0;
	m_ExposeUntil = 0;
	m_Aim = m_Pos;
	ClearShots();
	m_Anim = DROIDANIM_IDLE;
}

void CSkitterMatriarch::StartAct(int Act)
{
	m_Act = Act;
	m_ActTick = Server()->Tick();
	m_AttackTick = m_ActTick;
	m_Events = 0;
	m_Anim = MatriarchAct(Act).m_Anim;
	if(Act == MATRIARCH_ACT_DEATH)
		m_Status = DROIDSTATUS_TERMINATED;
}

void CSkitterMatriarch::SetLocomotion(int Act)
{
	if(m_Act != Act)
		StartAct(Act);
}

bool CSkitterMatriarch::AcquireTarget(bool NeedSight)
{
	m_TargetIndex = -1;
	CCharacter *pClosest = 0;
	float Closest = 0.0f;
	const float RangeX = NeedSight ? 1100.0f : 1400.0f;
	const float RangeY = NeedSight ? 600.0f : 760.0f;
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
	if(!m_Airborne)
		m_Dir = m_Target.x >= 0.0f ? 1 : -1;
	return true;
}

int CSkitterMatriarch::ScaleDamage(int Dmg)
{
	return max(1, (int)(Dmg * m_DamageScale * BossV5PhaseDamage(m_Phase) + 0.5f));
}

int CSkitterMatriarch::HurtPlayers(vec2 Pos, float Radius, int Dmg, float Knock, float Lift)
{
	Dmg = ScaleDamage(Dmg);
	CCharacter *apEnts[MAX_CHARACTERS];
	const int Num = GameServer()->m_World.FindEntities(Pos, Radius, (CEntity **)apEnts, MAX_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);
	int Hits = 0;
	for(int i = 0; i < Num; i++)
	{
		CCharacter *pChr = apEnts[i];
		if(!pChr || !pChr->IsAlive() || pChr->m_IsBot)
			continue;
		vec2 Diff = pChr->m_Pos - Pos;
		const float Len = length(Diff);
		vec2 Force = vec2((float)m_Dir, 0.0f);
		if(Len > 0.01f)
			Force = Diff / Len;
		Force = vec2(Force.x * Knock, Force.y * Knock * 0.5f - Knock * Lift);
		pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), Dmg, Force, pChr->m_Pos);
		Hits++;
	}
	return Hits;
}

int CSkitterMatriarch::HurtSegment(vec2 From, vec2 To, float Radius, int Dmg, float Knock)
{
	Dmg = ScaleDamage(Dmg);
	CCharacter *apEnts[MAX_CHARACTERS];
	const vec2 Mid = (From + To) * 0.5f;
	const int Num = GameServer()->m_World.FindEntities(
		Mid, distance(From, To) * 0.5f + Radius + 30.0f, (CEntity **)apEnts, MAX_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);
	int Hits = 0;
	for(int i = 0; i < Num; i++)
	{
		CCharacter *pChr = apEnts[i];
		if(!pChr || !pChr->IsAlive() || pChr->m_IsBot)
			continue;
		if(distance(closest_point_on_line(From, To, pChr->m_Pos), pChr->m_Pos) > Radius + 14.0f)
			continue;
		const vec2 Push = normalize(To - From);
		pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), Dmg, vec2(Push.x * Knock, Push.y * Knock - 3.0f), pChr->m_Pos);
		Hits++;
	}
	if(Hits)
		GameServer()->CreateSound(To, SOUND_HAMMER_HIT);
	return Hits;
}

bool CSkitterMatriarch::SacExposed()
{
	if(m_Health <= 0)
		return false;
	const int Elapsed = Server()->Tick() - m_ActTick;
	if(Server()->Tick() < m_ExposeUntil || m_Act == MATRIARCH_ACT_STAGGER)
		return true;
	return m_Act == MATRIARCH_ACT_BROOD && Elapsed >= MatriarchTicks(0.45f) && Elapsed < MatriarchTicks(1.4f);
}

vec2 CSkitterMatriarch::Mouth() const
{
	return LocalToWorld(MATRIARCH_MANDIBLE_X, MATRIARCH_MANDIBLE_Y);
}

vec2 CSkitterMatriarch::SacPos() const
{
	return LocalToWorld(s_MatriarchSac.m_X, s_MatriarchSac.m_Y);
}

int CSkitterMatriarch::AliveBrood()
{
	int Count = 0;
	for(int i = 0; i < MATRIARCH_MAX_SHOTS; i++)
		if(m_aShots[i].m_ID >= 0 && m_aShots[i].m_Kind == MATRIARCH_SHOT_EGG)
			Count++;
	CEntity *apEnts[64];
	const int Num = GameServer()->m_World.FindEntities(m_Pos, 1800.0f, apEnts, 64, CGameWorld::ENTTYPE_DROID);
	for(int i = 0; i < Num; i++)
	{
		CDroid *pDroid = static_cast<CDroid *>(apEnts[i]);
		if(pDroid != this && pDroid->m_Type == DROIDTYPE_CRAWLER && pDroid->m_Health > 0)
			Count++;
	}
	return Count;
}

// ---------------------------------------------------------------- shots

int CSkitterMatriarch::AddShot(int Kind, vec2 Pos, vec2 Vel)
{
	int Slot = -1;
	for(int i = 0; i < MATRIARCH_MAX_SHOTS && Slot < 0; i++)
		if(m_aShots[i].m_ID < 0)
			Slot = i;
	// Out of slots: a fresh puddle replaces the oldest one.
	if(Slot < 0 && Kind == MATRIARCH_SHOT_PUDDLE)
	{
		int Oldest = -1;
		for(int i = 0; i < MATRIARCH_MAX_SHOTS; i++)
			if(m_aShots[i].m_Kind == MATRIARCH_SHOT_PUDDLE && (Oldest < 0 || m_aShots[i].m_Life > m_aShots[Oldest].m_Life))
				Oldest = i;
		if(Oldest >= 0)
		{
			RemoveShot(Oldest);
			Slot = Oldest;
		}
	}
	if(Slot < 0)
		return -1;
	CShot &s = m_aShots[Slot];
	s.m_ID = Server()->SnapNewID();
	s.m_Kind = Kind;
	s.m_Pos = Pos;
	s.m_Vel = Vel;
	s.m_Life = 0;
	return Slot;
}

void CSkitterMatriarch::RemoveShot(int i)
{
	if(m_aShots[i].m_ID >= 0)
		Server()->SnapFreeID(m_aShots[i].m_ID);
	m_aShots[i].m_ID = -1;
}

void CSkitterMatriarch::ClearShots()
{
	for(int i = 0; i < MATRIARCH_MAX_SHOTS; i++)
		RemoveShot(i);
}

void CSkitterMatriarch::FireGlob(int Shot)
{
	static const float s_aSpread[5] = {0.0f, -80.0f, 80.0f, -150.0f, 150.0f};
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetIndex);
	vec2 Aim = pTarget ? pTarget->m_Pos + pTarget->GetVel() * 14.0f : m_Pos + m_Target;
	Aim.x += s_aSpread[Shot % 5];
	const vec2 From = Mouth();
	const float Dist = distance(Aim, From);
	float T = Dist / 13.0f;
	T = T < 20.0f ? 20.0f : (T > 46.0f ? 46.0f : T);
	float Vx, Vy;
	MatriarchBallistic(Aim.x - From.x, Aim.y - From.y, T, MATRIARCH_GLOB_GRAVITY, 22.0f, &Vx, &Vy);
	if(AddShot(MATRIARCH_SHOT_GLOB, From, vec2(Vx, Vy)) >= 0)
		GameServer()->CreateSound(From, SOUND_BOUNCER_FIRE);
}

void CSkitterMatriarch::TickShots()
{
	CCollision *pCollision = GameServer()->Collision();
	for(int i = 0; i < MATRIARCH_MAX_SHOTS; i++)
	{
		CShot &s = m_aShots[i];
		if(s.m_ID < 0)
			continue;
		s.m_Life++;
		if(s.m_Kind == MATRIARCH_SHOT_GLOB)
		{
			s.m_Vel.y += MATRIARCH_GLOB_GRAVITY;
			const vec2 Next = s.m_Pos + s.m_Vel;
			CCharacter *pHit = GameServer()->m_World.ClosestCharacter(Next, 30.0f, 0);
			if(pHit && pHit->IsAlive() && !pHit->m_IsBot)
			{
				pHit->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(MATRIARCH_GLOB_DAMAGE), s.m_Vel * 0.3f, pHit->m_Pos);
				pHit->Slow(0.5f);
				GameServer()->CreateSound(Next, SOUND_GREEN_EXPLOSION);
				RemoveShot(i);
				continue;
			}
			vec2 At, Before;
			if(pCollision->IntersectLine(s.m_Pos, Next, &At, &Before, false, true))
			{
				GameServer()->CreateSound(At, SOUND_GREEN_EXPLOSION);
				HurtPlayers(At, 44.0f, MATRIARCH_GLOB_DAMAGE / 2, 3.0f, 0.1f);
				RemoveShot(i);
				// Pool on the floor below the splash point.
				vec2 Floor;
				if(pCollision->IntersectLine(Before, Before + vec2(0, 80.0f), &Floor, 0, false, true))
					AddShot(MATRIARCH_SHOT_PUDDLE, Floor - vec2(0, 2.0f), vec2(0, 0));
				continue;
			}
			s.m_Pos = Next;
			if(s.m_Life > 220)
				RemoveShot(i);
		}
		else if(s.m_Kind == MATRIARCH_SHOT_PUDDLE)
		{
			if(s.m_Life >= MATRIARCH_PUDDLE_LIFE)
			{
				RemoveShot(i);
				continue;
			}
			if(s.m_Life % 12)
				continue;
			for(int c = 0; c < MAX_CHARACTERS; c++)
			{
				CCharacter *pChr = GameServer()->GetPlayerChar(c);
				if(!pChr || !pChr->IsAlive() || pChr->m_IsBot)
					continue;
				const vec2 d = pChr->m_Pos - s.m_Pos;
				if(fabsf(d.x) > MATRIARCH_PUDDLE_RADIUS || d.y < -60.0f || d.y > 16.0f)
					continue;
				pChr->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(MATRIARCH_PUDDLE_DAMAGE), vec2(0, 0), pChr->m_Pos);
				pChr->Slow(0.3f);
			}
		}
		else if(s.m_Kind == MATRIARCH_SHOT_EGG)
		{
			s.m_Vel.y += 0.6f;
			if(pCollision->TestBox(s.m_Pos + vec2(0, 4.0f), s_EggBox))
				s.m_Vel.x *= 0.85f;
			pCollision->MoveBox(&s.m_Pos, &s.m_Vel, s_EggBox, 0.3f, false);
			if(s.m_Life < MATRIARCH_EGG_HATCH)
				continue;
			vec2 P = s.m_Pos + vec2(0, -18.0f);
			for(int n = 0; n < 5 && pCollision->TestBox(P, vec2(60.0f, 60.0f)); n++)
				P.y -= 14.0f;
			RemoveShot(i);
			if(!pCollision->TestBox(P, vec2(60.0f, 60.0f)))
			{
				new CCrawler(GameWorld(), P);
				GameServer()->CreateSound(P, SOUND_SPAWN);
			}
		}
	}
}

// ---------------------------------------------------------------- acts

void CSkitterMatriarch::LaunchPounce(bool Counter)
{
	m_Events |= 1;
	CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetIndex);
	if(pTarget && !pTarget->IsAlive())
		pTarget = 0;
	const vec2 Body = m_Pos;
	vec2 Aim = pTarget ? pTarget->m_Pos : m_Pos + m_Target;
	float Dist = distance(Aim, Body);
	const float T0 = MatriarchPounceFlight(Dist);
	if(pTarget)
		Aim += pTarget->GetVel() * T0 * 0.45f;
	Aim.y -= 18.0f;
	const bool Legs = PartAlive(MATRIARCH_PART_LEGS);
	const float MaxReach = Legs ? 720.0f : 380.0f;
	Dist = distance(Aim, Body);
	if(Dist > MaxReach)
	{
		Aim = Body + (Aim - Body) * (MaxReach / Dist);
		Dist = MaxReach;
	}
	float Vx, Vy;
	MatriarchBallistic(Aim.x - Body.x, Aim.y - Body.y, MatriarchPounceFlight(Dist), MATRIARCH_GRAVITY, Legs ? 26.0f : 18.0f, &Vx, &Vy);
	if(Counter)
	{
		Vx *= 1.1f;
		Vy *= 1.05f;
	}
	m_Vel = vec2(Vx, Vy);
	m_Aim = Aim;
	m_Airborne = true;
	m_ContactHit = false;
	m_LaunchTick = Server()->Tick();
	m_Dir = Vx >= 0.0f ? 1 : -1;
	GameServer()->CreateSound(Body, SOUND_WALKER_TAKEOFF);
}

void CSkitterMatriarch::LandPounce()
{
	m_Airborne = false;
	m_LandTick = Server()->Tick();
	m_ExposeUntil = m_LandTick + MATRIARCH_EXPOSE_TICKS;
	m_Vel.x *= 0.3f;
	HurtPlayers(m_Pos + vec2(0, 30.0f), MATRIARCH_LAND_RADIUS, MATRIARCH_LAND_DAMAGE, 10.0f, 0.6f);
	GameServer()->CreateBuildingHit(vec2(m_Pos.x, m_Body.Feet(m_Pos) - 10.0f));
	GameServer()->CreateSound(m_Pos, SOUND_BODY_LAND);
}

void CSkitterMatriarch::TickPounce(int Elapsed)
{
	if(!(m_Events & 1))
	{
		if(Elapsed < MatriarchPounceWindup(m_Phase, m_Chained))
		{
			AcquireTarget(false);
			m_Aim = m_Pos + m_Target;
			return;
		}
		LaunchPounce(false);
		return;
	}
	if(m_Airborne)
	{
		const int Flight = Server()->Tick() - m_LaunchTick;
		if(!m_ContactHit)
		{
			CCharacter *pHit = GameServer()->m_World.ClosestCharacter(m_Pos, 100.0f, 0);
			if(pHit && pHit->IsAlive() && !pHit->m_IsBot)
			{
				m_ContactHit = true;
				pHit->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), ScaleDamage(MATRIARCH_POUNCE_DAMAGE),
					vec2(m_Dir * 14.0f, -9.0f), pHit->m_Pos);
				GameServer()->CreateSound(pHit->m_Pos, SOUND_HAMMER_HIT);
			}
		}
		// Slammed into a wall mid-leap: from phase 3 she sticks to it and kicks off back at the player.
		const bool WallHit = fabsf(m_PreVel.x) > 5.0f && fabsf(m_Vel.x) < 1.0f &&
				     !m_Body.Fits(GameServer()->Collision(), m_Pos + vec2(m_PreVel.x > 0 ? 8.0f : -8.0f, 0));
		if(WallHit && m_Phase >= 2 && Flight > 3)
		{
			m_Airborne = false;
			m_Vel = vec2(0, 0);
			StartAct(MATRIARCH_ACT_CLING);
			GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND);
			return;
		}
		const bool Down = m_Vel.y >= 0.0f || (m_PreVel.y > 2.0f && fabsf(m_Vel.y) < 0.5f);
		if(Flight > 4 && Down && m_Body.m_Grounded)
			LandPounce();
		else if(Flight > 100)
			LandPounce();
		return;
	}
	// Landed: the sac hangs open. From phase 2, a far target gets a second leap straight away.
	const int Since = Server()->Tick() - m_LandTick;
	if(Since == 8 && m_Phase >= 1 && !m_Chained && PartAlive(MATRIARCH_PART_LEGS) && AcquireTarget(true) &&
		length(m_Target) > 240.0f)
	{
		m_Chained = true;
		StartAct(MATRIARCH_ACT_POUNCE);
		return;
	}
	if(Since >= MATRIARCH_EXPOSE_TICKS)
		FinishAct();
}

void CSkitterMatriarch::TickAct()
{
	const int Elapsed = Server()->Tick() - m_ActTick;
	switch(m_Act)
	{
	case MATRIARCH_ACT_POUNCE:
		TickPounce(Elapsed);
		break;
	case MATRIARCH_ACT_STAB:
	{
		const int Count = MatriarchStabCount(m_Phase);
		if(Elapsed < MatriarchStabTick(0))
		{
			AcquireTarget(false);
			m_Aim = m_Pos + m_Target;
		}
		for(int Stab = 0; Stab < Count; Stab++)
		{
			if((m_Events & (1 << Stab)) || Elapsed < MatriarchStabTick(Stab))
				continue;
			m_Events |= 1 << Stab;
			CCharacter *pTarget = GameServer()->GetPlayerChar(m_TargetIndex);
			const vec2 Base = LocalToWorld(120.0f, 18.0f);
			vec2 Want = pTarget && pTarget->IsAlive() ? pTarget->m_Pos - Base : vec2((float)m_Dir, 0.2f);
			if(Want.x * m_Dir < 20.0f)
				Want.x = 20.0f * m_Dir;
			Want = normalize(Want);
			// Keep the jab in a forward cone; straight up or down is the thrash's job.
			if(fabsf(Want.y) > 0.8f)
				Want = normalize(vec2(Want.x < 0 ? -0.6f : 0.6f, Want.y < 0 ? -0.8f : 0.8f));
			const vec2 Tip = Base + Want * MATRIARCH_STAB_REACH;
			m_Aim = Tip;
			const int Dmg = PartAlive(MATRIARCH_PART_FANGS) ? MATRIARCH_STAB_DAMAGE : MATRIARCH_STAB_DAMAGE_BROKEN;
			HurtSegment(Base, Tip, MATRIARCH_STAB_RADIUS, Dmg, 7.0f);
			m_Vel.x += m_Dir * 6.0f;
			GameServer()->CreateSound(Base, SOUND_HAMMER_FIRE);
		}
		break;
	}
	case MATRIARCH_ACT_SPIT:
	{
		if(!PartAlive(MATRIARCH_PART_SAC))
		{
			FinishAct();
			return;
		}
		AcquireTarget(false);
		m_Aim = m_Pos + m_Target;
		const int Count = MatriarchSpitCount(m_Phase);
		for(int Shot = 0; Shot < Count; Shot++)
		{
			if((m_Events & (1 << Shot)) || Elapsed < MatriarchSpitTick(Shot))
				continue;
			m_Events |= 1 << Shot;
			FireGlob(Shot);
		}
		break;
	}
	case MATRIARCH_ACT_BROOD:
		if(!(m_Events & 1) && Elapsed >= MatriarchBroodTick())
		{
			m_Events |= 1;
			const int Want = m_Phase >= 2 ? 3 : 2;
			const int Room = MATRIARCH_MAX_BROOD - AliveBrood();
			for(int i = 0; i < Want && i < Room; i++)
				AddShot(MATRIARCH_SHOT_EGG, SacPos(), vec2(-m_Dir * (2.0f + i * 2.5f) + (frandom() - 0.5f) * 2.0f, -7.0f - frandom() * 3.0f));
			GameServer()->CreateSound(SacPos(), SOUND_GREEN_EXPLOSION);
		}
		break;
	case MATRIARCH_ACT_ROAR:
		if(!(m_Events & 1) && Elapsed >= MatriarchRoarTick())
		{
			m_Events |= 1;
			HurtPlayers(m_Pos, MATRIARCH_ROAR_RADIUS, MATRIARCH_ROAR_DAMAGE, 16.0f, 0.5f);
			GameServer()->CreateSound(m_Pos, SOUND_CHARGE_FULL);
		}
		break;
	case MATRIARCH_ACT_THRASH:
		if(!(m_Events & 1) && Elapsed >= MatriarchThrashTick())
		{
			m_Events |= 1;
			HurtPlayers(m_Pos + vec2(0, -10.0f), MATRIARCH_THRASH_RADIUS, MATRIARCH_THRASH_DAMAGE, 14.0f, 0.8f);
			m_Vel.y = -6.0f;
			GameServer()->CreateSound(m_Pos, SOUND_KICKHIT);
		}
		break;
	case MATRIARCH_ACT_CLING:
		AcquireTarget(false);
		if(Elapsed >= 10)
		{
			m_Chained = true;
			StartAct(MATRIARCH_ACT_POUNCE);
			LaunchPounce(true);
			return;
		}
		break;
	default:
		break;
	}
}

void CSkitterMatriarch::FinishAct()
{
	static const int s_aReload[3] = {5, 3, 1};
	static const int s_aPounceCooldown[3] = {60, 45, 32};
	static const int s_aSpitCooldown[3] = {80, 60, 45};
	static const int s_aBroodCooldown[3] = {420, 330, 300};
	static const int s_aThrashCooldown[3] = {70, 55, 40};
	const int Act = m_Act;
	if(Act == MATRIARCH_ACT_STAB || Act == MATRIARCH_ACT_POUNCE || Act == MATRIARCH_ACT_SPIT || Act == MATRIARCH_ACT_BROOD ||
		Act == MATRIARCH_ACT_THRASH)
		m_ActsDone++;
	if(Act == MATRIARCH_ACT_POUNCE)
		m_PounceCooldown = s_aPounceCooldown[m_Phase];
	if(Act == MATRIARCH_ACT_SPIT)
		m_SpitCooldown = s_aSpitCooldown[m_Phase];
	if(Act == MATRIARCH_ACT_BROOD)
		m_BroodCooldown = s_aBroodCooldown[m_Phase];
	if(Act == MATRIARCH_ACT_THRASH)
		m_ThrashCooldown = s_aThrashCooldown[m_Phase];
	m_ReloadTimer = s_aReload[m_Phase];
	m_Chained = false;
	m_Airborne = false;
	StartAct(MATRIARCH_ACT_IDLE);

	// From phase 2 a stab string that pushed the player out of reach rolls into a pounce.
	if(Act == MATRIARCH_ACT_STAB && !m_PounceCooldown && AcquireTarget(true) &&
		fabsf(m_Target.x) > 200.0f && fabsf(m_Target.x) < 560.0f)
		StartAct(MATRIARCH_ACT_POUNCE);
}

void CSkitterMatriarch::BreakPart(int Part)
{
	m_aPartHealth[Part] = 0;
	m_Stagger = 1;
	float x = 0.0f, y = 0.0f;
	for(int i = 0; i < NUM_MATRIARCH_HIT; i++)
		if(s_aMatriarchHit[i].m_Part == Part)
		{
			x = s_aMatriarchHit[i].m_X;
			y = s_aMatriarchHit[i].m_Y;
		}
	GameServer()->CreateExplosion(LocalToWorld(x, y), CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
}

void CSkitterMatriarch::Think()
{
	const int Phase = MatriarchPhase(m_Health, m_MaxHealth);
	if(Phase > m_Phase)
	{
		m_Phase = Phase;
		m_Stagger = 0;
		StartAct(MATRIARCH_ACT_ROAR);
		return;
	}
	if(m_Stagger)
	{
		m_Stagger = 0;
		StartAct(MATRIARCH_ACT_STAGGER);
		return;
	}

	if(m_ReloadTimer > 0)
		m_ReloadTimer--;
	const bool Seen = AcquireTarget(true);
	if(!Seen && !AcquireTarget(false))
	{
		m_Move = 0;
		SetLocomotion(MATRIARCH_ACT_IDLE);
		return;
	}
	const float Dx = fabsf(m_Target.x);
	const float Dy = m_Target.y;
	const float Dist = length(m_Target);
	const bool Sac = PartAlive(MATRIARCH_PART_SAC);
	const bool CanPounce = !m_PounceCooldown && m_Supported;

	// Standing on her back or tucked underneath is never safe.
	if(!m_ThrashCooldown && ((Dx < 150.0f && Dy < -50.0f && Dy > -220.0f) || (Dx < 70.0f && Dy > 0.0f && Dy < 90.0f)))
	{
		StartAct(MATRIARCH_ACT_THRASH);
		return;
	}
	// Under fire: answer it instead of soaking it (thrash the huggers, leap at the shooters).
	if(m_Pressure > m_MaxHealth * BOSSV5_PRESSURE_TRIGGER)
	{
		m_Pressure = 0.0f;
		if(!m_ThrashCooldown && Dist < 200.0f)
		{
			StartAct(MATRIARCH_ACT_THRASH);
			return;
		}
		if(CanPounce)
		{
			StartAct(MATRIARCH_ACT_POUNCE);
			return;
		}
		m_ReloadTimer = 0;
	}
	// Anti-kite: far away or up on a ledge means a leap, even mid-reload.
	if(Seen && CanPounce && (Dist > MATRIARCH_POUNCE_RANGE || Dy < -150.0f))
	{
		StartAct(MATRIARCH_ACT_POUNCE);
		return;
	}

	// Locomotion between acts: hold a jab range, with nervous back-and-forth skitters.
	if(--m_MoveTimer <= 0)
		m_MoveTimer = 24 + rand() % 36;
	const int Toward = m_Target.x >= 0.0f ? 1 : -1;
	if(Dx > 240.0f)
		m_Move = Toward;
	else if(Dx < 110.0f)
		m_Move = -Toward;
	else
		m_Move = (m_MoveTimer / 12) % 2 ? Toward : -Toward;

	if(m_ReloadTimer > 0)
	{
		SetLocomotion(MATRIARCH_ACT_SKITTER);
		return;
	}
	// Walled off for 4 s (climbing/hopping did not get her through): glob the target from here.
	if(m_BlockedTime > SERVER_TICK_SPEED * 4 && Sac && !m_SpitCooldown)
	{
		m_BlockedTime = SERVER_TICK_SPEED * 2;
		StartAct(MATRIARCH_ACT_SPIT);
		return;
	}
	if(!Seen)
	{
		if(Sac && !m_SpitCooldown)
			StartAct(MATRIARCH_ACT_SPIT);
		else
			SetLocomotion(MATRIARCH_ACT_SKITTER);
		return;
	}
	if(m_Phase >= 1 && Sac && !m_BroodCooldown && AliveBrood() < MATRIARCH_MAX_BROOD && (m_ActsDone % 4) == 3)
		StartAct(MATRIARCH_ACT_BROOD);
	else if(Dx < 250.0f && fabsf(Dy) < 150.0f)
		StartAct(MATRIARCH_ACT_STAB);
	else if(CanPounce && Dist < 600.0f && frandom() < 0.7f)
		StartAct(MATRIARCH_ACT_POUNCE);
	else if(Sac && !m_SpitCooldown)
		StartAct(MATRIARCH_ACT_SPIT);
	else if(CanPounce)
		StartAct(MATRIARCH_ACT_POUNCE);
	else
		SetLocomotion(MATRIARCH_ACT_SKITTER);
}

// ---------------------------------------------------------------- body

void CSkitterMatriarch::SyncGround()
{
	// Floor under the body for the slams/landing effects: the soles when standing, else straight down.
	m_Supported = m_Body.m_Grounded;
	if(m_Body.m_Grounded)
	{
		m_Ground = m_Body.Feet(m_Pos);
		return;
	}
	vec2 At;
	const vec2 From(m_Pos.x, m_Body.Feet(m_Pos) - 4.0f);
	if(GameServer()->Collision()->IntersectLine(From, From + vec2(0, 400.0f), &At, 0, false, true))
		m_Ground = At.y;
	else
		m_Ground = 1e9f;
}

void CSkitterMatriarch::MoveBody()
{
	// Full-silhouette body (carapace, sac and every leg tip) under gravity, like a crawler: the box
	// can only rest on real ground, steps over bumps, crouches under low ceilings, drops through
	// one-way platforms toward the target and climbs walls it cannot step over.
	CCollision *pCollision = GameServer()->Collision();
	m_PreVel = m_Vel;
	m_Climbing = false;

	if(m_Act == MATRIARCH_ACT_CLING && m_Health > 0)
	{
		m_Vel = vec2(0, 0);
		SyncGround();
		GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 60);
		return;
	}
	const bool Alive = m_Health > 0;
	if(m_Airborne || !Alive)
	{
		if(!Alive)
			m_Vel.x *= 0.9f;
		m_Vel.y = min(m_Vel.y + MATRIARCH_GRAVITY, 24.0f);
		m_Body.Move(pCollision, &m_Pos, &m_Vel, false);
		SyncGround();
		GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 60);
		return;
	}

	const bool Legs = PartAlive(MATRIARCH_PART_LEGS);
	float Speed = MatriarchSpeed(m_Phase, Legs);
	bool Moving = m_Move != 0 && (m_Act == MATRIARCH_ACT_SKITTER || m_Act == MATRIARCH_ACT_SPIT);
	if(m_Act == MATRIARCH_ACT_SPIT)
		Speed *= 0.3f;

	// Don't walk off a drop unless the target is down there.
	if(Moving && m_Body.m_Grounded && m_Target.y < 96.0f && !m_Body.FloorAhead(pCollision, m_Pos, m_Move, 300.0f))
		Moving = false;
	// On a one-way platform with the target below: drop through it.
	if(m_Body.m_OnPlatform && !m_Body.Dropping() && m_Target.y > 110.0f && fabsf(m_Target.x) < 700.0f)
		m_Body.DropThrough();

	if(Moving)
		m_Vel.x += clamp(m_Move * Speed - m_Vel.x, -1.4f, 1.4f);
	else
		m_Vel.x *= 0.82f;
	m_Vel.y = min(m_Vel.y + MATRIARCH_GRAVITY, 20.0f);
	const int Result = m_Body.Move(pCollision, &m_Pos, &m_Vel, true);
	const bool Blocked = (Result & CBossBody::MOVE_BLOCKED_X) != 0;
	if(Moving && Blocked && !(Result & CBossBody::MOVE_HIT_CEILING))
	{
		// Wall taller than a step: climb it like the crawlers do (the box slides up the face).
		m_Climbing = true;
		m_Vel.y = min(m_Vel.y, -7.0f);
		m_Vel.x = m_Move * 2.0f;
	}

	// Pinned while trying to move: hop out and turn round; walled off for long: spit from here.
	if(Moving && fabsf(m_Pos.x - m_LastX) < 0.6f && !m_Climbing)
	{
		m_BlockedTime++;
		if(++m_StuckTicks > 50)
		{
			m_StuckTicks = 0;
			if(m_Body.m_Grounded)
				m_Vel = vec2(m_Dir * 8.0f, -13.0f);
			m_Move = -m_Move;
		}
	}
	else
	{
		m_StuckTicks = 0;
		if(fabsf(m_Vel.x) > 2.0f && m_BlockedTime > 0)
			m_BlockedTime = max(0, m_BlockedTime - 3);
	}
	m_LastX = m_Pos.x;
	SyncGround();
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 60);
}

void CSkitterMatriarch::TickDeath()
{
	m_Status = DROIDSTATUS_TERMINATED;
	m_Act = MATRIARCH_ACT_DEATH;
	m_Anim = MatriarchAct(MATRIARCH_ACT_DEATH).m_Anim;
	m_Airborne = false;
	MoveBody();
	if(Server()->Tick() < m_DeathTick + MatriarchTicks(MatriarchAct(MATRIARCH_ACT_DEATH).m_Duration))
		return;

	GameServer()->CreateExplosion(m_Pos + m_Center, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true));
	GameServer()->CreateExplosion(SacPos(), CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
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
	GameServer()->m_World.DestroyEntity(this);
}

void CSkitterMatriarch::Tick()
{
	if(DespawnIfUnsnapped())
		return;

	if(m_Health > 0 && GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
		TakeDamage(vec2(0, -0.5f), 12, CAttackSource::World(DAMAGETYPE_FLUID), vec2(0, 0));

	TickShots();
	if(m_Health <= 0)
	{
		if(!m_DeathTick)
			m_DeathTick = Server()->Tick();
		TickDeath();
		return;
	}

	m_Pressure *= BOSSV5_PRESSURE_DECAY;
	if(m_PounceCooldown > 0)
		m_PounceCooldown--;
	if(m_SpitCooldown > 0)
		m_SpitCooldown--;
	if(m_BroodCooldown > 0)
		m_BroodCooldown--;
	if(m_ThrashCooldown > 0)
		m_ThrashCooldown--;

	if(m_Act != MATRIARCH_ACT_IDLE && m_Act != MATRIARCH_ACT_SKITTER)
	{
		TickAct();
		if(m_Act != MATRIARCH_ACT_IDLE && m_Act != MATRIARCH_ACT_POUNCE && m_Act != MATRIARCH_ACT_CLING &&
			Server()->Tick() - m_ActTick >= MatriarchTicks(MatriarchAct(m_Act).m_Duration))
			FinishAct();
		else if(m_Act == MATRIARCH_ACT_POUNCE && Server()->Tick() - m_ActTick >= MatriarchTicks(MatriarchAct(m_Act).m_Duration))
			FinishAct();
	}
	else
		Think();

	MoveBody();

	if(Server()->Tick() > m_DamageTakenTick + 15 && m_Status == DROIDSTATUS_HURT)
		m_Status = DROIDSTATUS_IDLE;
}

void CSkitterMatriarch::TickPaused()
{
}

void CSkitterMatriarch::Snap(int SnappingClient)
{
	CDroid::Snap(SnappingClient);

	for(int i = 0; i < MATRIARCH_MAX_SHOTS; i++)
	{
		const CShot &s = m_aShots[i];
		if(s.m_ID < 0 || NetworkClipped(SnappingClient, s.m_Pos))
			continue;
		CNetObj_BossShot *pShot = static_cast<CNetObj_BossShot *>(
			Server()->SnapNewItem(NETOBJTYPE_BOSSSHOT, s.m_ID, sizeof(CNetObj_BossShot)));
		if(!pShot)
			continue;
		pShot->m_X = (int)s.m_Pos.x;
		pShot->m_Y = (int)s.m_Pos.y;
		// Puddles carry their remaining life in VelX so the client can fade them out.
		pShot->m_VelX = s.m_Kind == MATRIARCH_SHOT_PUDDLE ? MATRIARCH_PUDDLE_LIFE - s.m_Life : (int)(s.m_Vel.x * 100.0f);
		pShot->m_VelY = s.m_Kind == MATRIARCH_SHOT_EGG ? s.m_Life : (int)(s.m_Vel.y * 100.0f);
		pShot->m_Kind = s.m_Kind;
	}

	CPlayer *pPlayer = SnappingClient >= 0 ? GameServer()->m_apPlayers[SnappingClient] : 0;
	if(pPlayer && distance(pPlayer->m_ViewPos, m_Pos) > 2200.0f)
		return;
	CNetObj_BossStatus *pStatus = static_cast<CNetObj_BossStatus *>(
		Server()->SnapNewItem(NETOBJTYPE_BOSSSTATUS, m_ID, sizeof(CNetObj_BossStatus)));
	if(!pStatus)
		return;
	pStatus->m_Health = max(0, m_Health);
	pStatus->m_MaxHealth = m_MaxHealth;
	pStatus->m_Phase = m_Phase;
	pStatus->m_Part0 = m_MaxHealth > 0 ? clamp(m_Health * 100 / m_MaxHealth, 0, 100) : 0;
	pStatus->m_Part1 = m_aPartMax[MATRIARCH_PART_FANGS] > 0 ? m_aPartHealth[MATRIARCH_PART_FANGS] * 100 / m_aPartMax[MATRIARCH_PART_FANGS] : 0;
	pStatus->m_Part2 = m_aPartMax[MATRIARCH_PART_LEGS] > 0 ? m_aPartHealth[MATRIARCH_PART_LEGS] * 100 / m_aPartMax[MATRIARCH_PART_LEGS] : 0;
	pStatus->m_Part3 = m_aPartMax[MATRIARCH_PART_SAC] > 0 ? m_aPartHealth[MATRIARCH_PART_SAC] * 100 / m_aPartMax[MATRIARCH_PART_SAC] : 0;
	pStatus->m_ArmOut = SacExposed() ? 1 : 0;
	pStatus->m_ArmX = (int)m_Aim.x;
	pStatus->m_ArmY = (int)m_Aim.y;
}

static void MatriarchHitCircle(vec2 Pos0, vec2 Pos1, vec2 Center, float Range, float *pBest, vec2 *pAt)
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

bool CSkitterMatriarch::HitSegment(vec2 Pos0, vec2 Pos1, float Radius, vec2 *pAt)
{
	float Best = 1e9f;
	for(int i = 0; i < NUM_MATRIARCH_HIT; i++)
	{
		const CMatriarchHitCircle &c = s_aMatriarchHit[i];
		if(c.m_Part == MATRIARCH_PART_SAC && !PartAlive(MATRIARCH_PART_SAC))
			continue;
		MatriarchHitCircle(Pos0, Pos1, LocalToWorld(c.m_X, c.m_Y), c.m_R + Radius, &Best, pAt);
	}
	for(int i = 0; i < MATRIARCH_MAX_SHOTS; i++)
		if(m_aShots[i].m_ID >= 0 && (m_aShots[i].m_Kind == MATRIARCH_SHOT_GLOB || m_aShots[i].m_Kind == MATRIARCH_SHOT_EGG))
			MatriarchHitCircle(Pos0, Pos1, m_aShots[i].m_Pos, (m_aShots[i].m_Kind == MATRIARCH_SHOT_EGG ? 20.0f : 14.0f) + Radius, &Best, pAt);
	return Best < 1e9f;
}

void CSkitterMatriarch::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	if(m_Health <= 0 || !Dmg || Source.m_Kind == EAttackSourceKind::Droid || IgnoresMapObject(Source))
		return;

	const bool HasPos = Pos.x != 0.0f || Pos.y != 0.0f;
	int Part = MATRIARCH_PART_CORE;
	if(HasPos)
	{
		// Eggs and acid globs can be shot out of the air.
		for(int i = 0; i < MATRIARCH_MAX_SHOTS; i++)
		{
			CShot &s = m_aShots[i];
			if(s.m_ID < 0 || (s.m_Kind != MATRIARCH_SHOT_GLOB && s.m_Kind != MATRIARCH_SHOT_EGG) || distance(Pos, s.m_Pos) > 32.0f)
				continue;
			GameServer()->CreateSound(s.m_Pos, SOUND_GREEN_EXPLOSION);
			if(s.m_Kind == MATRIARCH_SHOT_EGG)
				GameServer()->CreateExplosion(s.m_Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.0f);
			RemoveShot(i);
			return;
		}
		Part = MatriarchPartAt((Pos.x - m_Pos.x) * m_Dir, Pos.y - m_Pos.y);
		if(Part == MATRIARCH_PART_SAC && !PartAlive(MATRIARCH_PART_SAC))
			Part = MATRIARCH_PART_CORE;
	}
	if(m_Act == MATRIARCH_ACT_ROAR)
		return;

	if(g_Config.m_SvOneHitKill)
		Dmg = 1000;
	if(GameServer()->m_pPveDirector)
		Dmg = GameServer()->m_pPveDirector->ModifyDroidDamage(Source, Dmg, true, this);
	Dmg = MatriarchIncomingDamage(Dmg, Part, SacExposed());

	if(Part != MATRIARCH_PART_CORE && m_aPartHealth[Part] > 0)
	{
		m_aPartHealth[Part] -= Dmg;
		if(m_aPartHealth[Part] <= 0)
			BreakPart(Part);
	}

	vec2 DmgPos = PresentDamage(Combat, Force, Dmg, Pos);
	if(!m_Airborne && m_Act != MATRIARCH_ACT_CLING)
		m_Vel += Force * 0.2f;
	if(length(m_Vel) > 26.0f)
		m_Vel = normalize(m_Vel) * 26.0f;
	CommitDamage(DmgPos, Dmg, Source);
	m_DamageTakenTick = Server()->Tick();

	if(m_Health <= 0)
	{
		m_DeathTick = Server()->Tick();
		m_Act = MATRIARCH_ACT_DEATH;
		m_Anim = MatriarchAct(MATRIARCH_ACT_DEATH).m_Anim;
		m_AttackTick = m_DeathTick;
		m_Status = DROIDSTATUS_TERMINATED;
		m_Airborne = false;
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnDroidKilled(this, Source);
		CCharacter *pChr = GameServer()->GetPlayerChar(From);
		if(pChr)
			pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		return;
	}

	const bool Fluid = Source.m_Kind == EAttackSourceKind::World && Source.m_Type == DAMAGETYPE_FLUID;
	if(!Fluid)
		m_Pressure += Dmg; // no random flinch any more: sustained fire makes her counter-attack
}

void CSkitterMatriarch::DebugAct(int Act)
{
	if(m_Health <= 0 || Act < 0 || Act >= MATRIARCH_ACT_DEATH)
		return;
	AcquireTarget(false);
	m_Airborne = false;
	m_Chained = false;
	StartAct(Act);
}

void CSkitterMatriarch::DebugState(char *pBuffer, int Size)
{
	str_format(pBuffer, Size,
		"matriarch act=%s phase=%d hp=%d/%d fangs=%d legs=%d sac=%d pos=%.0f,%.0f vel=%.1f,%.1f ground=%.0f supported=%d air=%d exposed=%d brood=%d",
		MatriarchAct(m_Act).m_pName, m_Phase, m_Health, m_MaxHealth, m_aPartHealth[MATRIARCH_PART_FANGS],
		m_aPartHealth[MATRIARCH_PART_LEGS], m_aPartHealth[MATRIARCH_PART_SAC], m_Pos.x, m_Pos.y, m_Vel.x, m_Vel.y,
		m_Ground < 1e8f ? m_Ground - m_Pos.y : -1.0f, m_Supported, m_Airborne, SacExposed(), AliveBrood());
}
