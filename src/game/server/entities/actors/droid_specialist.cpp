#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/core/gamecontext.h>
#include <game/server/pve/pve_director.h>
#include <game/server/entities/actors/character.h>
#include <game/server/entities/combat/projectile.h>
#include "droid_specialist.h"

CSpecialistDroid::CSpecialistDroid(CGameWorld *pWorld, vec2 Pos, int Type, int Health, bool Boss) :
	CDroid(pWorld, Pos, Type), m_BaseHealth(Health), m_IsBoss(Boss)
{
	m_StartPos = Pos;
	Reset();
	GameWorld()->InsertEntity(this);
}

void CSpecialistDroid::Reset()
{
	CDroid::Reset();
	m_Center = vec2(0, -30);
	m_Health = m_BaseHealth;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_Status = DROIDSTATUS_IDLE;
	m_AbilityTick = Server()->Tick() + Server()->TickSpeed();
	m_ThresholdMask = 0;
	m_PlacementResolved = false;
	m_MovementGoal = m_Pos;
	m_MovementGoalEndTick = 0;
	m_NextHopTick = Server()->Tick();
}

CCharacter *CSpecialistDroid::TargetCharacter()
{
	return m_TargetIndex >= 0 && m_TargetIndex < MAX_CLIENTS ? GameServer()->GetPlayerChar(m_TargetIndex) : 0;
}

bool CSpecialistDroid::AcquireTarget(float Range, bool RequireSight)
{
	CCharacter *pBest = 0;
	float BestDistanceSquared = Range * Range;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if(!pChr || !pChr->IsAlive() || pChr->Invisible() || (GameServer()->m_pController->IsCoop() && pChr->m_IsBot))
			continue;
		const vec2 Delta = m_Pos - pChr->m_Pos;
		const float DistanceSquared = dot(Delta, Delta);
		if(DistanceSquared >= BestDistanceSquared || (RequireSight && GameServer()->Collision()->FastIntersectLine(m_Pos + m_Center, pChr->m_Pos - vec2(0, 24))))
			continue;
		BestDistanceSquared = DistanceSquared;
		pBest = pChr;
		m_TargetIndex = i;
	}
	if(!pBest)
		m_TargetIndex = -1;
	return pBest != 0;
}

void CSpecialistDroid::FireProjectile(int Damage, float Spread)
{
	CCharacter *pChr = TargetCharacter();
	if(!pChr)
		return;
	vec2 Dir = normalize(pChr->m_Pos - vec2(0, 24) - (m_Pos + m_Center));
	Dir = normalize(Dir + vec2((frandom() - frandom()) * Spread, (frandom() - frandom()) * Spread));
	const CAttackSource Source = CAttackSource::Droid(NEUTRAL_BASE, m_Type);
	CWeaponCombatProfile Combat;
	if(!CWeaponCatalog::TryResolveAttack(Source, &Combat))
		return;
	new CProjectile(&GameServer()->m_World, Source,
		m_Pos + m_Center + Dir * 24.0f, Dir, vec2(0, 0),
		(int)(Server()->TickSpeed() * Combat.m_ProjectileLife), Damage,
		Combat.m_ProjectileKnockback, -1);
	m_AttackTick = Server()->Tick();
}

int CSpecialistDroid::CountDroids(int Type, float Radius)
{
	CDroid *apDroids[256];
	int Num = GameServer()->m_World.FindEntities(m_Pos, Radius, (CEntity **)apDroids, 256, CGameWorld::ENTTYPE_DROID);
	int Count = 0;
	for(int i = 0; i < Num; i++)
		if(apDroids[i] && apDroids[i] != this && apDroids[i]->m_Health > 0 && apDroids[i]->m_Type == Type)
			Count++;
	return Count;
}

bool CSpecialistDroid::ConsumeThreshold(int Threshold, int Bit)
{
	if((m_ThresholdMask & Bit) || m_Health * 100 > m_MaxHealth * Threshold)
		return false;
	m_ThresholdMask |= Bit;
	return true;
}

void CSpecialistDroid::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	if(m_Type == DROIDTYPE_BULWARK && TargetCharacter())
	{
		const vec2 Incoming = Pos - (m_Pos + m_Center);
		if(Incoming.x * m_Dir < 0.0f)
			Dmg = max(1, Dmg / 3);
	}
	CDroid::TakeDamage(Force, Dmg, Source, Pos);
	if(m_Health <= 0)
	{
		m_Status = DROIDSTATUS_TERMINATED;
		OnSpecialistDeath();
		return;
	}
	if(m_Type == DROIDTYPE_SIEGE_ENGINE)
	{
		if(ConsumeThreshold(70, 1)) OnHealthThreshold(70);
		if(ConsumeThreshold(35, 2)) OnHealthThreshold(35);
	}
	else if(m_Type == DROIDTYPE_OVERSEER_CORE)
	{
		if(ConsumeThreshold(75, 1)) OnHealthThreshold(75);
		if(ConsumeThreshold(40, 2)) OnHealthThreshold(40);
	}
}

void CSpecialistDroid::AbilityTick() {}

void CSpecialistDroid::SetMovementGoal(vec2 Pos, int DurationTicks)
{
	m_MovementGoal = Pos;
	m_MovementGoalEndTick = Server()->Tick() + max(1, DurationTicks);
}

vec2 CSpecialistDroid::CollisionSize() const
{
	switch(m_Type)
	{
	case DROIDTYPE_BULWARK: return vec2(94.0f, 92.0f);
	case DROIDTYPE_ASSEMBLER: return vec2(82.0f, 88.0f);
	case DROIDTYPE_SABOTEUR: return vec2(104.0f, 74.0f);
	case DROIDTYPE_RAILGUNNER: return vec2(78.0f, 96.0f);
	case DROIDTYPE_SIEGE_ENGINE: return vec2(142.0f, 92.0f);
	default: return vec2(72.0f, 72.0f);
	}
}

void CSpecialistDroid::MovementTick(CCharacter *pTarget)
{
	const vec2 Size = CollisionSize();
	// Visual hitboxes are large for Spine bodies. Movement uses a tighter box so
	// ramp tiles are not treated as full-height walls.
	const vec2 MoveSize(Size.x * 0.70f, Size.y * 0.58f);
	const float SpeedScale = GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->EnemySpeedMultiplier() : 1.0f;
	const bool Grounded = GameServer()->Collision()->TestBox(m_Pos + vec2(0, 4), MoveSize);
	const int FootTile = GameServer()->Collision()->GetCollisionAt(m_Pos.x, m_Pos.y + MoveSize.y * 0.5f + 6.0f);
	const bool OnRamp = FootTile == CCollision::COLFLAG_RAMP_LEFT || FootTile == CCollision::COLFLAG_RAMP_RIGHT;
	m_Vel.y += Grounded && OnRamp ? 0.35f : 0.72f;
	m_Vel.x *= Grounded ? (OnRamp ? 0.94f : 0.88f) : 0.975f;
	if(Grounded && m_Vel.y > 0.0f)
		m_Vel.y = 0.0f;
	const bool TacticalGoal = Server()->Tick() < m_MovementGoalEndTick;
	const vec2 Goal = TacticalGoal ? m_MovementGoal : (pTarget ? pTarget->m_Pos : m_Pos);
	if(TacticalGoal || pTarget)
	{
		m_Dir = Goal.x < m_Pos.x ? -1 : 1;
		float PreferredRange = m_IsBoss ? 300.0f : 220.0f;
		float Drive = m_IsBoss ? 1.05f : 1.45f;
		float MaxSpeed = m_IsBoss ? 13.0f : 16.0f;
		if(m_Type == DROIDTYPE_RAILGUNNER)
		{
			PreferredRange = 560.0f;
			Drive = 1.30f;
			MaxSpeed = 15.0f;
		}
		else if(m_Type == DROIDTYPE_ASSEMBLER)
		{
			PreferredRange = 300.0f;
			Drive = 1.35f;
		}
		else if(m_Type == DROIDTYPE_BULWARK)
		{
			PreferredRange = TacticalGoal ? 165.0f : 190.0f;
			Drive = 1.20f;
			MaxSpeed = 14.0f;
		}
		else if(m_Type == DROIDTYPE_SABOTEUR)
		{
			Drive = 1.70f;
			MaxSpeed = 17.0f;
		}
		const float Dist = distance(m_Pos, Goal);
		if(Dist > PreferredRange)
			m_Vel.x += m_Dir * Drive * SpeedScale;
		else if(!TacticalGoal && m_Type == DROIDTYPE_RAILGUNNER && Dist < 340.0f)
			m_Vel.x -= m_Dir * Drive * 0.72f * SpeedScale;

		const float AheadX = m_Pos.x + m_Dir * (MoveSize.x * 0.55f + 10.0f);
		const int AheadFoot = GameServer()->Collision()->GetCollisionAt(AheadX, m_Pos.y + MoveSize.y * 0.5f + 4.0f);
		const int AheadChest = GameServer()->Collision()->GetCollisionAt(AheadX, m_Pos.y - MoveSize.y * 0.05f);
		const bool RampAhead = AheadFoot == CCollision::COLFLAG_RAMP_LEFT || AheadFoot == CCollision::COLFLAG_RAMP_RIGHT;
		const bool WallAhead = AheadChest == CCollision::COLFLAG_SOLID;
		vec2 FloorHit;
		const bool FloorAhead = GameServer()->Collision()->IntersectLine(vec2(AheadX, m_Pos.y), vec2(AheadX, m_Pos.y + MoveSize.y + 80.0f), 0, &FloorHit, false, true);
		const bool TargetAbove = Goal.y < m_Pos.y - MoveSize.y * 0.45f;
		const bool LongPursuit = !TacticalGoal && pTarget && abs(Goal.x - m_Pos.x) > 340.0f && frandom() < 0.012f;
		const bool Stalled = abs(m_Vel.x) < 0.65f && abs(Goal.x - m_Pos.x) > 100.0f && frandom() < 0.075f;

		// Climb ramps instead of hopping against them.
		if(Grounded && (OnRamp || RampAhead) && Dist > PreferredRange * 0.35f && absolute(m_Vel.x) > 0.4f)
			m_Vel.y = min(m_Vel.y, -0.85f * SpeedScale);

		if(Grounded && Server()->Tick() >= m_NextHopTick &&
			(WallAhead || TargetAbove || (!FloorAhead && !RampAhead && abs(Goal.x - m_Pos.x) > PreferredRange) || LongPursuit || (Stalled && !OnRamp && !RampAhead)))
		{
			const float Hop = m_IsBoss ? 8.5f : (m_Type == DROIDTYPE_SABOTEUR ? 11.2f : 10.4f);
			m_Vel.y = min(m_Vel.y, -Hop);
			m_Vel.x += m_Dir * (m_IsBoss ? 1.4f : 2.2f) * SpeedScale;
			m_NextHopTick = Server()->Tick() + Server()->TickSpeed() * 2 / 5;
		}
		m_Vel.x = clamp(m_Vel.x, -MaxSpeed * SpeedScale, MaxSpeed * SpeedScale);
	}
	m_Vel.y = clamp(m_Vel.y, -13.0f, 13.0f);
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, MoveSize, 0, false);
	m_Anim = absolute(m_Vel.x) > 0.15f ? 1 : 0;
}

void CSpecialistDroid::Tick()
{
	if(m_Health <= 0)
	{
		// Crawler-style death: retain impact velocity, fall against map geometry,
		// settle, then remove the wreck. The client clamps its final collapse pose.
		m_Status = DROIDSTATUS_TERMINATED;
		m_Vel.y = min(12.0f, m_Vel.y + 0.8f);
		m_Vel.x *= 0.96f;
		const vec2 Size = CollisionSize();
		GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, Size, 0, false);
		GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, max(Size.x, Size.y) * 0.5f);
		const int DeathAge = Server()->Tick() - m_DeathTick;
		const bool Settled = GameServer()->Collision()->TestBox(m_Pos + vec2(0, 3), Size) && abs(m_Vel.y) < 0.25f;
		if((DeathAge >= Server()->TickSpeed() * 2 / 5 && Settled) || DeathAge >= Server()->TickSpeed() * 3 / 2)
			GameServer()->m_World.DestroyEntity(this);
		return;
	}
	if(!m_PlacementResolved)
	{
		m_PlacementResolved = true;
		const vec2 Size = CollisionSize();
		if(GameServer()->Collision()->TestBox(m_Pos, Size))
		{
			bool Found = false;
			// Prefer lifting out of a platform, then try symmetric side offsets.
			// This reconciles spawn anchors with each unit's real body.
			for(int Up = 16; Up <= 192 && !Found; Up += 16)
				for(int Side : {0, -32, 32, -64, 64})
				{
					const vec2 Candidate = m_Pos + vec2(Side, -Up);
					if(!GameServer()->Collision()->TestBox(Candidate, Size))
					{
						m_Pos = Candidate;
						Found = true;
						break;
					}
				}
			if(!Found)
				dbg_msg("pve-droid", "type=%d could not resolve embedded spawn at %.0f,%.0f", m_Type, m_Pos.x, m_Pos.y);
		}
	}
	// Navigation may pursue a player through nearby geometry; each weapon still
	// performs its own line-of-sight acquisition before firing.
	AcquireTarget(m_IsBoss ? 1400.0f : 1100.0f, false);
	CCharacter *pTarget = TargetCharacter();
	if(pTarget)
	{
		m_Dir = pTarget->m_Pos.x < m_Pos.x ? -1 : 1;
		m_Target = pTarget->m_Pos - (m_Pos + m_Center);
	}
	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID) * 0.25f;
	MovementTick(pTarget);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, max(CollisionSize().x, CollisionSize().y) * 0.5f);
	if(Server()->Tick() >= m_AbilityTick)
		AbilityTick();
	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;
}

void CSpecialistDroid::TickPaused()
{
	m_AbilityTick++;
}
