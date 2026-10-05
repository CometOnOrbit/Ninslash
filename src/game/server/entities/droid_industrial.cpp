#include "droid_industrial.h"
#include "character.h"
#include "droid_skittermatriarch.h"
#include "droid_bastionstrider.h"
#include "droid_stormseraph.h"
#include "droid_siegemonolith.h"
#include <game/server/bosspool.h>
#include <engine/shared/config.h>
#include <game/industrial_boss_sockets.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/pve_director.h>

CIndustrialBoss::CIndustrialBoss(CGameWorld *pWorld, vec2 Pos, int Kind)
	: CDroid(pWorld, Pos, DROIDTYPE_BOSSRAIL + clamp(Kind, 0, 3)), m_Kind(clamp(Kind, 0, 3))
{
	for(auto &S : m_aShots)
		S.m_ID = -1;
	Reset();
	GameWorld()->InsertEntity(this);
}
CIndustrialBoss::~CIndustrialBoss()
{
	ClearShots();
}
void CIndustrialBoss::RemoveShot(int i)
{
	if(m_aShots[i].m_ID >= 0)
		Server()->SnapFreeID(m_aShots[i].m_ID);
	m_aShots[i].m_ID = -1;
}
void CIndustrialBoss::ClearShots()
{
	for(int i = 0; i < MAX_SHOTS; i++)
		RemoveShot(i);
}
void CIndustrialBoss::Reset()
{
	CDroid::Reset();
	ClearShots();
	const auto &P = IndustrialProfile(m_Kind);
	m_Health = P.m_Health;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + .5f);
	m_MaxHealth = max(1, m_Health);
	m_Center = vec2(0, 0);
	m_Dir = 1;
	m_PendingDir = m_Dir;
	m_Vel = vec2(0, 0);
	m_Phase = 0;
	m_Sequence = 0;
	m_JumpCooldown = 0;
	m_Released = false;
	m_DamageTakenTick = 0;
	m_SnapTick = 0;
	m_TargetIndex = -1;
	m_ProximityRadius = length(P.m_Box) * .5f;
	for(int i = 0; i < 4; i++)
		m_aParts[i] = m_aPartMax[i] = max(1, m_MaxHealth * (i == 1 ? 22 : 16) / 100);
	m_NextAttack = Server()->Tick() + 80;
	m_Aim = vec2(1, 0);
	StartAct(IB_ARRIVAL);
}
void CIndustrialBoss::StartAct(int Act)
{
	m_Act = Act;
	m_ActTick = m_AttackTick = Server()->Tick();
	m_Anim = IndustrialAct(Act).m_Anim;
	m_Events = 0;
	if(g_Config.m_Debug)
		dbg_msg("industrial",
				"type=%d act=%s hp=%d pos=(%.0f,%.0f)",
				m_Type,
				IndustrialAct(Act).m_pClip,
				m_Health,
				m_Pos.x,
				m_Pos.y);
	for(bool &Hit : m_aHit)
		Hit = false;
	if(Act == IB_DEATH)
	{
		m_Status = DROIDSTATUS_TERMINATED;
		ClearShots();
	}
	if(Act == IB_DASH || Act == IB_MELEE || Act == IB_SPECIAL)
		GameServer()->CreateSound(m_Pos, SOUND_BOUNCER_FIRE);
}
vec2 CIndustrialBoss::Socket(int Which, int Tick)
{
	if(Tick < 0)
		Tick = Server()->Tick() - m_ActTick;
	const auto &P = IndustrialProfile(m_Kind);
	const vec2 V = IndustrialSocket(m_Kind, m_Act, Tick, Which);
	return m_Pos + vec2(0, P.m_Box.y * .5f) + vec2(V.x * m_Dir, -V.y) * P.m_Scale;
}
bool CIndustrialBoss::Exposed()
{
	return IndustrialExposed(m_Act, Server()->Tick() - m_ActTick);
}
bool CIndustrialBoss::Target()
{
	float Best = 1600;
	m_TargetIndex = -1;
	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *p = GameServer()->GetPlayerChar(i);
		if(!p || !p->IsAlive() || p->m_IsBot || p->Invisible())
			continue;
		const float D = distance(p->m_Pos, m_Pos);
		if(D < Best)
		{
			Best = D;
			m_TargetIndex = i;
			m_Target = p->m_Pos + vec2(0, -20) - m_Pos;
		}
	}
	if(m_TargetIndex < 0)
		return false;
	if(fabsf(m_Target.x) > 32)
	{
		const int DesiredDir = m_Target.x > 0 ? 1 : -1;
		if(DesiredDir != m_Dir && (m_Act == IB_IDLE || m_Act == IB_MOVE))
		{
			m_PendingDir = DesiredDir;
			StartAct(IB_TURN);
		}
	}
	m_Aim = length(m_Target) > 1 ? normalize(m_Target) : vec2((float)m_Dir, 0);
	return true;
}
void CIndustrialBoss::Think()
{
	if(!Target())
	{
		if(m_Act != IB_IDLE)
			StartAct(IB_IDLE);
		return;
	}
	// Target() may have started the dedicated server-authoritative turn. Do not
	// start an attack until the complete 0.32 s performance has finished.
	if(m_Act == IB_TURN)
		return;
	if(!m_Phase && m_Health <= m_MaxHealth / 2)
	{
		m_Phase = 1;
		StartAct(IB_ENRAGE);
		return;
	}
	if(Server()->Tick() < m_NextAttack)
	{
		if(m_Act != IB_MOVE)
			StartAct(IB_MOVE);
		return;
	}
	const auto &P = IndustrialProfile(m_Kind);
	const bool Ground = GameServer()->Collision()->TestBox(m_Pos + vec2(0, 3), P.m_Box);
	const bool Blocked = GameServer()->Collision()->TestBox(m_Pos + vec2(m_Dir * 30.f, 0), P.m_Box);
	if(Ground && !m_JumpCooldown && (Blocked || m_Target.y < -90))
	{
		m_JumpCooldown = 100;
		StartAct(IB_LEAP);
		return;
	}
	const bool Sight = !GameServer()->Collision()->FastIntersectLine(m_Pos, m_Pos + m_Target);
	if(!Sight)
	{
		if(m_Act != IB_MOVE)
			StartAct(IB_MOVE);
		return;
	}
	++m_Sequence;
	if(m_Kind >= INDUSTRIAL_ARC && m_aParts[3] > 0 && m_Sequence % 3 == 0)
		StartAct(IB_SPECIAL);
	else if(length(m_Target) < 190 && m_aParts[1] > 0 && m_Sequence % 3 != 0 &&
			(m_Kind != INDUSTRIAL_RAIL || fabsf(m_Target.x) > 95))
		StartAct(IB_MELEE);
	else if(fabsf(m_Target.y) < 140 &&
			(fabsf(m_Target.x) < 100 || (fabsf(m_Target.x) > 220 && (m_Sequence % 3 != 0 || m_aParts[3] <= 0))))
		StartAct(IB_DASH);
	else if(m_aParts[3] > 0)
		StartAct(IB_RANGED);
	else if(Ground && !m_JumpCooldown)
	{
		m_JumpCooldown = 100;
		StartAct(IB_LEAP);
	}
	else
		StartAct(IB_DASH);
}
void CIndustrialBoss::HurtSegment(vec2 From, vec2 To, float Radius, int Damage, bool Once)
{
	for(int i = 0; i < MAX_CHARACTERS; i++)
	{
		CCharacter *p = GameServer()->GetPlayerChar(i);
		if(!p || !p->IsAlive() || p->m_IsBot || (Once && m_aHit[i]))
			continue;
		const vec2 Center = p->m_Pos + vec2(0, -16);
		const vec2 Near = distance(From, To) > .001f ? closest_point_on_line(From, To, Center) : From;
		if(distance(Near, Center) > Radius + 20 || GameServer()->Collision()->FastIntersectLine(Near, Center))
			continue;
		// A swept blade can extend beyond a wall even when the body cannot.
		if(Once && GameServer()->Collision()->FastIntersectLine(m_Pos, Near))
			continue;
		if(Once)
			m_aHit[i] = true;
		p->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), Damage, vec2(m_Dir * 7.f, -4), p->m_Pos);
	}
}
void CIndustrialBoss::AddShot(vec2 Pos, vec2 Vel, int Kind, int Delay)
{
	if(GameServer()->Collision()->CheckPoint(Pos.x, Pos.y) ||
	   ((Kind == 2 || Kind == 9) && GameServer()->Collision()->FastIntersectLine(m_Pos, Pos)))
		return;
	for(auto &S : m_aShots)
		if(S.m_ID < 0)
		{
			const int ID = Server()->SnapNewID();
			if(ID < 0)
				return;
			S.m_ID = ID;
			S.m_Pos = Pos;
			S.m_Vel = Vel;
			S.m_Kind = Kind;
			S.m_Age = 0;
			S.m_Delay = Delay;
			S.m_Health = 32;
			for(bool &Hit : S.m_aHit)
				Hit = false;
			return;
		}
}
void CIndustrialBoss::MarkPattern()
{
	const vec2 Aim = m_Pos + m_Target;
	// Spatially separated, staggered circles rather than sealing every exit.
	const int Count = m_aParts[3] > 0 ? 3 : 1;
	for(int i = 0; i < Count; i++)
	{
		const float X = Aim.x + (i - 1) * 200.f;
		vec2 P(X, Aim.y - 90), Floor;
		if(GameServer()->Collision()->IntersectLine(P, P + vec2(0, 320), 0, &Floor, false, true))
			P = Floor - vec2(0, 22);
		if(P.x < 32 || P.x > GameServer()->Collision()->GetWidth() * 32 - 32)
			continue;
		AddShot(P, vec2(0, 0), m_Kind == INDUSTRIAL_ARC ? 3 : 5, 50 + i * 18);
		if(m_Kind == INDUSTRIAL_VAULT && m_Phase && i != 1)
		{
			const vec2 Lane = P + vec2(i == 0 ? -100.f : 100.f, -55.f);
			if(!GameServer()->Collision()->TestBox(Lane, vec2(24, 150)))
				AddShot(Lane, vec2(0, 0), 7, 110);
		}
	}
}
void CIndustrialBoss::TickShots()
{
	for(int i = 0; i < MAX_SHOTS; i++)
	{
		CShot &S = m_aShots[i];
		if(S.m_ID < 0)
			continue;
		++S.m_Age;
		if(S.m_Kind == 7 || S.m_Kind == 8)
		{
			if(S.m_Age >= S.m_Delay)
				S.m_Kind = 8;
			if(S.m_Age > S.m_Delay + 65)
			{
				RemoveShot(i);
				continue;
			}
			if(S.m_Kind == 8)
				for(int j = 0; j < MAX_CHARACTERS; j++)
				{
					CCharacter *p = GameServer()->GetPlayerChar(j);
					if(!p || !p->IsAlive() || p->m_IsBot || S.m_aHit[j])
						continue;
					const vec2 Center = p->m_Pos + vec2(0, -16);
					const vec2 Near = closest_point_on_line(S.m_Pos - vec2(0, 75), S.m_Pos + vec2(0, 75), Center);
					if(distance(Near, Center) < 28 && !GameServer()->Collision()->FastIntersectLine(Near, Center))
					{
						S.m_aHit[j] = true;
						p->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), 18, vec2(0, -4), p->m_Pos);
					}
				}
			continue;
		}
		if(S.m_Kind == 2 || S.m_Kind == 9)
		{
			const vec2 Next = S.m_Pos + S.m_Vel;
			if(S.m_Age > 110 || GameServer()->Collision()->FastIntersectLine(S.m_Pos, Next))
			{
				RemoveShot(i);
				continue;
			}
			bool Hit = false;
			for(int j = 0; j < MAX_CHARACTERS; j++)
			{
				CCharacter *p = GameServer()->GetPlayerChar(j);
				if(!p || !p->IsAlive() || p->m_IsBot)
					continue;
				const vec2 Near = closest_point_on_line(S.m_Pos, Next, p->m_Pos + vec2(0, -16));
				if(distance(Near, p->m_Pos + vec2(0, -16)) < 26 &&
				   !GameServer()->Collision()->FastIntersectLine(Near, p->m_Pos + vec2(0, -16)))
				{
					p->TakeDamage(CAttackSource::Droid(NEUTRAL_BASE, m_Type), 12, normalize(S.m_Vel) * 3, p->m_Pos);
					Hit = true;
					break;
				}
			}
			if(Hit)
				RemoveShot(i);
			else
				S.m_Pos = Next;
		}
		else if(S.m_Age == S.m_Delay)
		{
			S.m_Kind = S.m_Kind == 3 ? 4 : 6;
			HurtSegment(S.m_Pos, S.m_Pos, S.m_Kind == 4 ? 76.f : 88.f, S.m_Kind == 4 ? 22 : 30, false);
			GameServer()->CreateEffect(FX_SHIELDHIT, S.m_Pos);
			GameServer()->CreateSound(S.m_Pos, SOUND_HAMMER_HIT);
		}
		else if(S.m_Age > S.m_Delay + 9)
			RemoveShot(i);
	}
}
void CIndustrialBoss::TickAttack()
{
	const int T = Server()->Tick() - m_ActTick;
	if(m_Act == IB_TURN && IndustrialTurnFacingSwitched(T) && !(m_Events & 1))
	{
		m_Events |= 1;
		m_Dir = m_PendingDir;
		m_Aim = length(m_Target) > 1 ? normalize(m_Target) : vec2((float)m_Dir, 0);
	}
	if(m_Act == IB_LEAP && T >= 16 && !(m_Events & 1))
	{
		m_Events |= 1;
		m_Vel = vec2(clamp(m_Target.x / 36.f, -14.f, 14.f), -15.f);
		GameServer()->CreateSound(m_Pos, SOUND_DASH);
	}
	if(m_Act == IB_RANGED)
	{
		for(int n = 0; n < 2; n++)
			if(T >= (n ? 66 : 48) && !(m_Events & (1 << n)))
			{
				m_Events |= 1 << n;
				if(n && m_aParts[3] <= 0)
					continue;
				const vec2 Origin = Socket(1);
				const int Count = m_Kind == INDUSTRIAL_BULKHEAD ? 3 : 5;
				for(int j = 0; j < Count; j++)
				{
					const float Angle = (j - (Count - 1) * .5f) * .16f;
					const vec2 V(m_Aim.x * cosf(Angle) - m_Aim.y * sinf(Angle),
								 m_Aim.x * sinf(Angle) + m_Aim.y * cosf(Angle));
					AddShot(Origin, V * (m_Kind == INDUSTRIAL_ARC ? 13.f : 17.f), m_Kind == INDUSTRIAL_ARC ? 9 : 2);
				}
				GameServer()->CreateSound(Origin, SOUND_BOUNCER_FIRE);
			}
	}
	if(m_Act == IB_SPECIAL && T >= 26 && !(m_Events & 1))
	{
		m_Events |= 1;
		MarkPattern();
	}
}
void CIndustrialBoss::MoveBody()
{
	const auto &P = IndustrialProfile(m_Kind);
	CCollision *C = GameServer()->Collision();
	const vec2 Before = m_Pos;
	const int T = Server()->Tick() - m_ActTick;
	const bool Active = IndustrialActive(m_Act, T) && m_Act == IB_DASH;
	const bool Ground = C->TestBox(m_Pos + vec2(0, 3), P.m_Box);
	if(Active)
		m_Vel.x = m_Dir * P.m_Dash * (m_aParts[2] > 0 ? 1.f : .65f) * (m_Phase ? 1.12f : 1.f);
	else if(m_Act == IB_MOVE && m_TargetIndex >= 0)
	{
		const float Want = fabsf(m_Target.x) > 110 ? m_Dir * P.m_Run * (m_Phase ? 1.15f : 1.f) : 0;
		m_Vel.x += (Want - m_Vel.x) * .22f;
		if(Ground && C->TestBox(m_Pos + vec2(m_Dir * 22.f, 0), P.m_Box) && !m_JumpCooldown)
		{
			m_JumpCooldown = 100;
			StartAct(IB_LEAP);
		}
	}
	else if(m_Act != IB_LEAP)
		m_Vel.x *= .8f;
	m_Vel.y = min(m_Vel.y + .65f, 18.f);
	C->MoveBox(&m_Pos, &m_Vel, P.m_Box, 0, false);
	if(Active)
	{
		HurtSegment(Before, m_Pos, 46, 22, true);
		if(fabsf(m_Pos.x - Before.x) < fabsf(P.m_Dash) * .35f)
		{
			m_Vel.x = 0;
			StartAct(IB_STAGGER);
			m_NextAttack = Server()->Tick() + 65;
		}
	}
	if(m_Act == IB_LEAP && (m_Events & 1) && !(m_Events & 2) && T > 22 && C->TestBox(m_Pos + vec2(0, 3), P.m_Box))
	{
		m_Events |= 2;
		// Landing is mobility, not an untelegraphed extra hit.
		GameServer()->CreateBuildingHit(m_Pos + vec2(0, P.m_Box.y * .5f));
	}
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, (int)(P.m_Box.x * .45f));
}
void CIndustrialBoss::FinishDeath()
{
	if(m_Released)
		return;
	m_Released = true;
	if(g_Config.m_Debug)
		dbg_msg("industrial", "type=%d death finalized once", m_Type);
	GameServer()->CreateExplosion(m_Pos, CAttackSource::Droid(NEUTRAL_BASE, m_Type, true), 0.f);
	for(int i = 0; i < 4; i++)
		GameServer()->m_pController->DropPickup(
			m_Pos, i % 2 ? POWERUP_AMMO : POWERUP_ARMOR, vec2((i - 1.5f) * 3, -10), 0);
	GameServer()->m_pController->DropWeapon(
		m_Pos, vec2(0, -12), GameServer()->NewWeapon(CWeaponCatalog::Static(SW_UPGRADE)));
	GameServer()->m_World.DestroyEntity(this);
}
void CIndustrialBoss::Tick()
{
	if(m_Released)
		return;
	// Recover an out-of-map fall without killing the encounter or awarding loot.
	if(m_Health > 0 && (m_Pos.y > GameServer()->Collision()->GetHeight() * 32.f + 160.f || m_Pos.x < -160.f ||
						m_Pos.x > GameServer()->Collision()->GetWidth() * 32.f + 160.f))
	{
		m_Pos = m_StartPos;
		m_Vel = vec2(0, 0);
		ClearShots();
		StartAct(IB_ARRIVAL);
	}
	if(m_Health <= 0)
	{
		if(Server()->Tick() - m_ActTick >= WardenTicks(IndustrialAct(IB_DEATH).m_Duration))
			FinishDeath();
		return;
	}
	if(m_JumpCooldown > 0)
		--m_JumpCooldown;
	TickShots();
	if(m_Act == IB_IDLE || m_Act == IB_MOVE)
		Think();
	else
		TickAttack();
	const int T = Server()->Tick() - m_ActTick;
	const vec2 PreviousCutter = Socket(0, max(0, T - 1));
	MoveBody();
	if(m_Act == IB_MELEE && IndustrialActive(m_Act, Server()->Tick() - m_ActTick))
	{
		if(m_Kind == INDUSTRIAL_ARC)
			HurtSegment(Socket(0),
						m_Pos + vec2(m_Dir * 80.f, IndustrialProfile(m_Kind).m_Box.y * .5f - 20),
						18,
						m_aParts[1] > 0 ? 24 : 12,
						true);
		else
			HurtSegment(
				PreviousCutter, Socket(0), m_Kind == INDUSTRIAL_RAIL ? 30.f : 38.f, m_aParts[1] > 0 ? 26 : 13, true);
	}
	if(m_Act != IB_IDLE && m_Act != IB_MOVE &&
	   Server()->Tick() - m_ActTick >= WardenTicks(IndustrialAct(m_Act).m_Duration))
	{
		m_NextAttack = max(m_NextAttack, Server()->Tick() + (m_Phase ? 10 : 22));
		StartAct(IB_MOVE);
	}
	if(m_Status == DROIDSTATUS_HURT && Server()->Tick() - m_DamageTakenTick > 12)
		m_Status = DROIDSTATUS_IDLE;
}
void CIndustrialBoss::TickPaused()
{
	++m_ActTick;
	++m_AttackTick;
	++m_NextAttack;
	if(m_DeathTick)
		++m_DeathTick;
	if(m_DamageTakenTick)
		++m_DamageTakenTick;
}
bool CIndustrialBoss::HitSegment(vec2 From, vec2 To, float Radius, vec2 *pAt)
{
	if(m_Health <= 0)
		return false;
	float Best = 1e20f;
	vec2 At;
	auto Circle = [&](vec2 Center, float R)
	{
		const vec2 P = distance(From, To) > .001f ? closest_point_on_line(From, To, Center) : From;
		const float Along = distance(From, P);
		if(distance(P, Center) <= R + Radius && Along < Best)
		{
			Best = Along;
			At = P;
		}
	};
	Circle(m_Pos, IndustrialProfile(m_Kind).m_Box.x * .46f);
	Circle(Socket(0), 32);
	Circle(Socket(2), 27);
	for(const auto &S : m_aShots)
		if(S.m_ID >= 0 && (S.m_Kind == 3 || S.m_Kind == 5))
			Circle(S.m_Pos, 18);
	if(Best == 1e20f)
		return false;
	*pAt = At;
	return true;
}
void CIndustrialBoss::TakeDamage(vec2 Force, int Damage, const CAttackSource &Source, vec2 Pos)
{
	if(m_Health <= 0 || Damage <= 0 || Source.m_Kind == EAttackSourceKind::Droid || IgnoresMapObject(Source))
		return;
	const bool HasPos = Pos.x != 0 || Pos.y != 0;
	if(HasPos)
		for(int i = 0; i < MAX_SHOTS; i++)
		{
			CShot &S = m_aShots[i];
			if(S.m_ID >= 0 && (S.m_Kind == 3 || S.m_Kind == 5) && distance(Pos, S.m_Pos) < 26)
			{
				S.m_Health -= Damage;
				if(S.m_Health <= 0)
				{
					GameServer()->CreateEffect(FX_SHIELDHIT, S.m_Pos);
					RemoveShot(i);
					if(m_Kind == INDUSTRIAL_ARC)
						StartAct(IB_STAGGER);
				}
				return;
			}
		}
	int Part = 0;
	if(HasPos)
	{
		if(distance(Pos, Socket(0)) < 46)
			Part = 1;
		else if(Pos.y > m_Pos.y + 24)
			Part = 2;
		else if(distance(Pos, Socket(1)) < 40)
			Part = 3;
	}
	if(GameServer()->m_pPveDirector)
		Damage = GameServer()->m_pPveDirector->ModifyDroidDamage(Source, Damage, true, this);
	if(Damage <= 0)
		return;
	if(Exposed())
		Damage = max(1, (int)(Damage * 1.4f));
	else if(m_Kind == INDUSTRIAL_BULKHEAD && m_aParts[1] > 0 && HasPos && (Pos.x - m_Pos.x) * m_Dir > 0)
		Damage = max(1, Damage / 2);
	// Administrative one-hit-kill must also bypass an intact front shield.
	if(g_Config.m_SvOneHitKill)
		Damage = m_MaxHealth;
	if(Part && m_aParts[Part] > 0)
	{
		m_aParts[Part] = max(0, m_aParts[Part] - Damage);
		if(m_aParts[Part] == 0)
		{
			GameServer()->CreateEffect(FX_SHIELDHIT, Pos);
			StartAct(IB_STAGGER);
		}
	}
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	const vec2 At = PresentDamage(Combat, Force, Damage, Pos);
	CommitDamage(At, Damage, Source);
	m_DamageTakenTick = Server()->Tick();
	m_Status = DROIDSTATUS_HURT;
	if(m_Health <= 0)
	{
		m_DeathTick = Server()->Tick();
		StartAct(IB_DEATH);
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnDroidKilled(this, Source);
	}
}
void CIndustrialBoss::Snap(int Client)
{
	if(m_Released)
		return;
	CDroid::Snap(Client);
	CPlayer *p = Client >= 0 ? GameServer()->m_apPlayers[Client] : 0;
	if(p && distance(p->m_ViewPos, m_Pos) > 2200)
		return;
	auto *B = static_cast<CNetObj_BossStatus *>(
		Server()->SnapNewItem(NETOBJTYPE_BOSSSTATUS, m_ID, sizeof(CNetObj_BossStatus)));
	if(B)
	{
		B->m_Health = max(0, m_Health);
		B->m_MaxHealth = m_MaxHealth;
		B->m_Phase = m_Phase;
		B->m_Part0 = clamp(m_Health * 100 / m_MaxHealth, 0, 100);
		B->m_Part1 = m_aParts[1] * 100 / m_aPartMax[1];
		B->m_Part2 = m_aParts[2] * 100 / m_aPartMax[2];
		B->m_Part3 = m_aParts[3] * 100 / m_aPartMax[3];
		B->m_ArmOut = Exposed() ? 1 : 0;
		B->m_ArmX = (int)Socket(2).x;
		B->m_ArmY = (int)Socket(2).y;
		if(m_Kind == INDUSTRIAL_ARC && m_Act == IB_MELEE && Server()->Tick() - m_ActTick < 56)
		{
			B->m_ArmOut = 1;
			B->m_ArmX = (int)(m_Pos.x + m_Dir * 80.f);
			B->m_ArmY = (int)(m_Pos.y + IndustrialProfile(m_Kind).m_Box.y * .5f - 20);
		}
	}
	for(const auto &S : m_aShots)
	{
		if(S.m_ID < 0 || NetworkClipped(Client, S.m_Pos))
			continue;
		auto *P = static_cast<CNetObj_BossShot *>(
			Server()->SnapNewItem(NETOBJTYPE_BOSSSHOT, S.m_ID, sizeof(CNetObj_BossShot)));
		if(!P)
			continue;
		P->m_X = (int)S.m_Pos.x;
		P->m_Y = (int)S.m_Pos.y;
		P->m_VelX = (int)(S.m_Vel.x * 100);
		P->m_VelY = (int)(S.m_Vel.y * 100);
		P->m_Kind = S.m_Kind;
	}
}

#if defined(CONF_DEBUG)
// Debug-only authenticated server-console helpers; not present in release builds.
// They exercise the real entity and snapshot path, not a mock implementation.
void CIndustrialBoss::DebugState(char *pBuffer, int Size)
{
	int Shots = 0;
	for(const auto &S : m_aShots)
		if(S.m_ID >= 0)
			++Shots;
	str_format(pBuffer,
			   Size,
			   "type=%d hp=%d/%d act=%s tick=%d shots=%d events=%d phase=%d facing=%d pending=%d parts=%d,%d,%d pos=%.0f,%.0f",
			   m_Type,
			   m_Health,
			   m_MaxHealth,
			   IndustrialAct(m_Act).m_pClip,
			   Server()->Tick() - m_ActTick,
			   Shots,
			   m_Events,
			   m_Phase,
			   m_Dir,
			   m_PendingDir,
			   m_aParts[1],
			   m_aParts[2],
			   m_aParts[3],
			   m_Pos.x,
			   m_Pos.y);
}
void RegisterIndustrialBossDebug(CGameContext *pGame)
{
	pGame->Console()->Register(
		"ib_test_spawn",
		"ii",
		CFGFLAG_SERVER,
		[](IConsole::IResult *pResult, void *pUser)
		{
			auto *G = static_cast<CGameContext *>(pUser);
			if(!g_Config.m_Debug || !G->m_pController || !G->m_pController->IsCoop())
				return;
			const int K = pResult->GetInteger(0), CID = pResult->GetInteger(1);
			if(K < 0 || K >= 4 || CID < 0 || CID >= MAX_CLIENTS)
				return;
			auto *C = G->GetPlayerChar(CID);
			if(!C || !C->IsAlive())
				return;
			vec2 Probes[8];
			for(int i = 0; i < 5; i++)
				Probes[i] = C->m_Pos + vec2((i - 2) * 220.f, 0);
			const vec2 Center(G->Collision()->GetWidth() * 16.f, G->Collision()->GetHeight() * 16.f);
			for(int i = 0; i < 3; i++)
				Probes[5 + i] = Center + vec2((i - 1) * 384.f, 0);
			vec2 Safe;
			if(!FindBossSpawnPosition(&G->m_World, Probes, 8, 0, &Safe))
			{
				G->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ib_test", "no safe arena near client");
				return;
			}
			char Buf[256];
			if(K == 0)
			{
				// Slot 0 (BOSSRAIL) is the Skitter Matriarch now.
				auto *M = new CSkitterMatriarch(&G->m_World, Safe);
				const vec2 PlayerPos = Safe + vec2(320, 40);
				if(!G->Collision()->TestBox(PlayerPos, vec2(28, 56)))
					C->Teleport(PlayerPos);
				M->DebugState(Buf, sizeof(Buf));
				G->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ib_test", Buf);
				return;
			}
			// Slots 1..3 are the v5 bosses: Bastion Strider, Storm Seraph, Siege Monolith.
			CBossV5 *B = 0;
			if(K == 1)
				B = new CBastionStrider(&G->m_World, Safe);
			else if(K == 2)
				B = new CStormSeraph(&G->m_World, Safe);
			else
				B = new CSiegeMonolith(&G->m_World, Safe);
			const vec2 PlayerPos = Safe + vec2(300, 40);
			if(!G->Collision()->TestBox(PlayerPos, vec2(28, 56)))
				C->Teleport(PlayerPos);
			B->DebugState(Buf, sizeof(Buf));
			G->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ib_test", Buf);
		},
		pGame,
		"DEBUG ONLY: spawn industrial kind 0..3 near human client");
	pGame->Console()->Register(
		"ib_test_action",
		"i",
		CFGFLAG_SERVER,
		[](IConsole::IResult *R, void *U)
		{
			auto *G = static_cast<CGameContext *>(U);
			if(!g_Config.m_Debug)
				return;
			for(auto *E = G->m_World.FindFirst(CGameWorld::ENTTYPE_DROID); E; E = E->TypeNext())
			{
				auto *D = static_cast<CDroid *>(E);
				if(D->m_Type == DROIDTYPE_BOSSRAIL)
					static_cast<CSkitterMatriarch *>(D)->DebugAct(R->GetInteger(0));
				else if(CBossV5 *B = dynamic_cast<CBossV5 *>(D))
					B->DebugAct(R->GetInteger(0));
			}
		},
		pGame,
		"DEBUG ONLY: exercise an industrial animation/state 0..11");
	pGame->Console()->Register(
		"ib_test_damage",
		"ii",
		CFGFLAG_SERVER,
		[](IConsole::IResult *R, void *U)
		{
			auto *G = static_cast<CGameContext *>(U);
			if(!g_Config.m_Debug)
				return;
			int CID = R->GetInteger(1);
			if(CID < 0 || CID >= MAX_CLIENTS || !G->GetPlayerChar(CID))
				return;
			for(auto *E = G->m_World.FindFirst(CGameWorld::ENTTYPE_DROID); E; E = E->TypeNext())
			{
				auto *D = static_cast<CDroid *>(E);
				if(IsIndustrialBoss(D->m_Type))
					D->TakeDamage(vec2(0, 0),
								  clamp(R->GetInteger(0), 0, 1000000),
								  CAttackSource::PlayerWeapon(CID, CWeaponCatalog::Static(SW_GUN1)),
								  D->m_Pos);
			}
		},
		pGame,
		"DEBUG ONLY: apply a human weapon hit to industrial bosses");
	pGame->Console()->Register(
		"ib_test_refill",
		"",
		CFGFLAG_SERVER,
		[](IConsole::IResult *, void *U)
		{
			auto *G = static_cast<CGameContext *>(U);
			if(!g_Config.m_Debug)
				return;
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(auto *C = G->GetPlayerChar(i))
					C->RefillHealth();
		},
		pGame,
		"DEBUG ONLY: refill connected human health between test cases");
	pGame->Console()->Register(
		"ib_test_state",
		"",
		CFGFLAG_SERVER,
		[](IConsole::IResult *, void *U)
		{
			auto *G = static_cast<CGameContext *>(U);
			if(!g_Config.m_Debug)
				return;
			int Count = 0;
			for(auto *E = G->m_World.FindFirst(CGameWorld::ENTTYPE_DROID); E; E = E->TypeNext())
			{
				auto *D = static_cast<CDroid *>(E);
				if(!IsIndustrialBoss(D->m_Type))
					continue;
				++Count;
				char Buf[256];
				if(D->m_Type == DROIDTYPE_BOSSRAIL)
					static_cast<CSkitterMatriarch *>(D)->DebugState(Buf, sizeof(Buf));
				else if(CBossV5 *B = dynamic_cast<CBossV5 *>(D))
					B->DebugState(Buf, sizeof(Buf));
				else
					static_cast<CIndustrialBoss *>(D)->DebugState(Buf, sizeof(Buf));
				G->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ib_test", Buf);
			}
			if(!Count)
				G->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ib_test", "industrial count=0");
			for(int i = 0; i < MAX_CLIENTS; i++)
				if(auto *C = G->GetPlayerChar(i))
				{
					char Buf[128];
					str_format(Buf,
							   sizeof(Buf),
							   "client=%d health=%d pos=%.0f,%.0f",
							   i,
							   C->m_HiddenHealth,
							   C->m_Pos.x,
							   C->m_Pos.y);
					G->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "ib_test", Buf);
				}
		},
		pGame,
		"DEBUG ONLY: inspect actual industrial entities and connected players");
}
#endif
