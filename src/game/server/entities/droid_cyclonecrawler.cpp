#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "droid_cyclonecrawler.h"

namespace
{
constexpr float CollisionSize = 60.0f;
constexpr float EngagementRange = 600.0f;
constexpr float LandingDamageScale = 0.35f;
constexpr int BarrageRocketCount = 5;
const float BarrageHalfAngle = 20.0f * pi / 180.0f;
} // namespace

CCycloneCrawler::CCycloneCrawler(CGameWorld *pGameWorld, vec2 Pos) : CCrawler(pGameWorld, Pos)
{
	m_Type = DROIDTYPE_CYCLONECRAWLER;
	Reset();
}

void CCycloneCrawler::Reset()
{
	CCrawler::Reset();
	m_Type = DROIDTYPE_CYCLONECRAWLER;
	m_Health = 450;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_ProximityRadius = CycloneCrawlerPhysSize;
	m_Anim = DROIDANIM_IDLE;
	m_CycloneState = CYCLONE_READY;
	m_CycloneCooldownTick = Server()->Tick();
	m_HoverStartTick = 0;
	m_RocketsFired = 0;
	m_AngleTimer = 0.0f;
	m_WasAirborne = false;
}

bool CCycloneCrawler::IsGrounded()
{
	const float HalfWidth = CollisionSize * 0.5f;
	const float GroundY = m_Pos.y + CollisionSize * 0.5f + 5.0f;
	return GameServer()->Collision()->CheckPoint(m_Pos.x - HalfWidth, GroundY) ||
		   GameServer()->Collision()->CheckPoint(m_Pos.x + HalfWidth, GroundY);
}

bool CCycloneCrawler::UpdateTarget()
{
	if(m_TargetIndex < 0 || m_TargetIndex >= MAX_CLIENTS)
	{
		m_Target = vec2(0, 0);
		return false;
	}

	CCharacter *pCharacter = GameServer()->GetPlayerChar(m_TargetIndex);
	if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible() ||
	   (GameServer()->m_pController->IsCoop() && pCharacter->m_IsBot) || distance(pCharacter->m_Pos, m_Pos) >= 700.0f ||
	   GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), m_Pos))
	{
		m_TargetIndex = -1;
		m_Target = vec2(0, 0);
		return false;
	}

	m_Target = pCharacter->m_Pos - m_Pos;
	return true;
}

bool CCycloneCrawler::FindTarget()
{
	m_TargetIndex = -1;
	float ClosestDistance = EngagementRange;

	for(int ClientID = 0; ClientID < MAX_CLIENTS; ++ClientID)
	{
		CCharacter *pCharacter = GameServer()->GetPlayerChar(ClientID);
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
			continue;
		if(GameServer()->m_pController->IsCoop() && pCharacter->m_IsBot)
			continue;
		if(abs(m_Pos.y - pCharacter->m_Pos.y) >= 300.0f ||
		   GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), m_Pos))
			continue;

		const float Distance = distance(pCharacter->m_Pos, m_Pos);
		if(Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			m_TargetIndex = ClientID;
		}
	}

	return m_TargetIndex >= 0;
}

void CCycloneCrawler::FireBarrageRocket()
{
	const float Spread = BarrageRocketCount > 1
							 ? BarrageHalfAngle * 2.0f * m_RocketsFired / (BarrageRocketCount - 1) - BarrageHalfAngle
							 : 0.0f;
	const vec2 Direction = vec2(sinf(Spread), cosf(Spread));
	const vec2 ProjectilePos = m_Pos + vec2(0, 24);
	GameServer()->CreateProjectile(CAttackSource::Droid(NEUTRAL_BASE, m_Type), 0, ProjectilePos, Direction, m_Pos);
	GameServer()->CreateSound(ProjectilePos, SOUND_GRENADE_FIRE);
	m_AttackTick = Server()->Tick();
	++m_RocketsFired;
}

void CCycloneCrawler::CreateLandingImpact()
{
	const vec2 ImpactPos = m_Pos + vec2(0, CollisionSize * 0.5f);
	GameServer()->CreateBuildingHit(ImpactPos);
	GameServer()->CreateSound(ImpactPos, SOUND_BODY_LAND);
	GameServer()->CreateExplosion(ImpactPos, CAttackSource::Droid(TEAM_NEUTRAL, m_Type, true), LandingDamageScale);
}

void CCycloneCrawler::Tick()
{
	if(m_Health <= 0)
	{
		CCrawler::Tick();
		return;
	}

	if(m_SnapTick && m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 5.0f)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
		{
			GameServer()->m_World.DestroyEntity(this);
			return;
		}
	}

	bool HasTarget = UpdateTarget();
	if(!HasTarget && FindTarget())
		HasTarget = UpdateTarget();

	const bool GroundedBeforeMove = IsGrounded();
	if(m_CycloneState == CYCLONE_DESCENDING && GroundedBeforeMove && m_WasAirborne)
	{
		CreateLandingImpact();
		m_CycloneState = CYCLONE_READY;
		m_WasAirborne = false;
		m_Anim = DROIDANIM_IDLE;
	}

	if(m_CycloneState == CYCLONE_READY && GroundedBeforeMove && HasTarget && length(m_Target) <= EngagementRange &&
	   Server()->Tick() >= m_CycloneCooldownTick && m_Status != DROIDSTATUS_ELECTRIC)
	{
		m_CycloneState = CYCLONE_ASCENDING;
		m_CycloneCooldownTick = Server()->Tick() + max(1, Server()->TickSpeed() * 4);
		m_RocketsFired = 0;
		m_WasAirborne = false;
		m_AngleTimer = 0.0f;
		m_Vel.y = -13.0f;
		m_Vel.x += m_Target.x < 0.0f ? -2.0f : 2.0f;
		m_Anim = DROIDANIM_JUMPATTACK;
	}

	if((m_CycloneState == CYCLONE_ASCENDING || m_CycloneState == CYCLONE_HOVERING) && !HasTarget)
		m_CycloneState = CYCLONE_DESCENDING;

	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID) * 0.5f;
	if(m_CycloneState == CYCLONE_HOVERING)
	{
		m_Vel.x *= 0.92f;
		m_Vel.y = 0.0f;
	}
	else
	{
		m_Vel.y += 0.8f;
		m_Vel *= 0.99f;
	}

	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(CollisionSize, CollisionSize), 0, false);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, CycloneCrawlerPhysSize);

	const bool GroundedAfterMove = IsGrounded();
	if(!GroundedAfterMove && m_CycloneState != CYCLONE_READY)
		m_WasAirborne = true;

	if(m_CycloneState == CYCLONE_ASCENDING && m_WasAirborne && m_Vel.y >= -1.5f)
	{
		m_CycloneState = CYCLONE_HOVERING;
		m_HoverStartTick = Server()->Tick();
		m_Vel = vec2(m_Vel.x * 0.5f, 0.0f);
	}

	if(m_CycloneState == CYCLONE_HOVERING)
	{
		const int HoverTicks = max(1, Server()->TickSpeed() * 1200 / 1000);
		const int RocketInterval = max(1, HoverTicks / BarrageRocketCount);
		m_AngleTimer += 12.0f * pi / max(1, Server()->TickSpeed());
		m_Anim = DROIDANIM_JUMPATTACK;

		if(HasTarget && m_RocketsFired < BarrageRocketCount &&
		   Server()->Tick() >= m_HoverStartTick + m_RocketsFired * RocketInterval)
			FireBarrageRocket();

		if(Server()->Tick() >= m_HoverStartTick + HoverTicks)
			m_CycloneState = CYCLONE_DESCENDING;
	}

	if(m_CycloneState == CYCLONE_DESCENDING)
	{
		m_Anim = DROIDANIM_JUMPATTACK;
		if(GroundedAfterMove && m_WasAirborne)
		{
			CreateLandingImpact();
			m_CycloneState = CYCLONE_READY;
			m_WasAirborne = false;
			m_Anim = DROIDANIM_IDLE;
		}
	}
	else if(m_CycloneState == CYCLONE_READY)
	{
		m_Vel.x *= 0.82f;
		if(HasTarget && abs(m_Target.x) > 20.0f)
		{
			m_Dir = m_Target.x < 0.0f ? -1 : 1;
			if(abs(m_Vel.x) < 7.0f)
				m_Vel.x += m_Dir * 0.8f;
			m_Anim = DROIDANIM_MOVE;
		}
		else
			m_Anim = DROIDANIM_IDLE;
	}

	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;
}

void CCycloneCrawler::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	m_SnapTick = Server()->Tick();
	CNetObj_Droid *pDroid =
		static_cast<CNetObj_Droid *>(Server()->SnapNewItem(NETOBJTYPE_DROID, m_ID, sizeof(CNetObj_Droid)));
	if(!pDroid)
		return;

	pDroid->m_X = (int)m_Pos.x;
	pDroid->m_Y = (int)m_Pos.y;
	pDroid->m_Type = m_Type;
	pDroid->m_Status = m_Status;
	pDroid->m_AttackTick = m_Health <= 0 ? m_DeathTick : m_AttackTick;
	pDroid->m_Anim = m_Anim;
	pDroid->m_Dir = m_Dir;
	pDroid->m_Angle = m_CycloneState == CYCLONE_HOVERING
						  ? (int)(m_AngleTimer * 180.0f / pi)
						  : (int)(GetAngle(vec2(abs(m_Target.x), m_Target.y)) * 180.0f / pi);
}
