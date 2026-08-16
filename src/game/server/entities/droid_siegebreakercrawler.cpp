#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "droid_siegebreakercrawler.h"

namespace
{
constexpr float CollisionSize = 72.0f;
constexpr float LandingDamageScale = 0.55f;
} // namespace

CSiegeBreakerCrawler::CSiegeBreakerCrawler(CGameWorld *pGameWorld, vec2 Pos)
	: CDroid(pGameWorld, Pos, DROIDTYPE_SIEGEBREAKERCRAWLER)
{
	m_StartPos = Pos;

	Reset();
	GameWorld()->InsertEntity(this);
}

void CSiegeBreakerCrawler::Reset()
{
	m_Center = vec2(0, 0);
	m_Health = 1100;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_Pos = m_StartPos;
	m_Status = DROIDSTATUS_IDLE;
	m_Dir = -1;
	m_DeathTick = 0;
	SetState(0);
	m_TargetIndex = -1;
	m_ReloadTimer = 0;
	m_AttackTick = 0;
	m_TargetTimer = 0;
	m_Target = vec2(0, 0);
	m_NewTarget = vec2(0, 0);
	m_Vel = vec2(0, 0);
	m_FlyTargetTick = 0;
	m_Mode = 0;
	m_ProximityRadius = SiegeBreakerCrawlerPhysSize;
	m_FireDelay = 0;
	m_FireCount = 0;
	m_AttackTimer = 0;
	m_DamageTakenTick = 0;
	m_Move = 0;
	m_AttackCount = 0;
	m_JumpTick = 0;
	m_JumpForce = 0.0f;
	m_LandingImpactArmed = false;
	m_JumpWasAirborne = false;
}

void CSiegeBreakerCrawler::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	if(m_Health <= 0 || !Dmg)
		return;

	if(g_Config.m_SvOneHitKill)
		Dmg = 1000;
	if(GameServer()->m_pPveDirector)
		Dmg = GameServer()->m_pPveDirector->ModifyDroidDamage(Source, Dmg, false, this);

	vec2 DmgPos = m_Pos + m_Center;
	if(Combat.m_ElectroAmount > 0.0f)
		m_Status = DROIDSTATUS_ELECTRIC;
	else if(Combat.m_FlameAmount > 0.0f)
		m_Status = DROIDSTATUS_HURT;
	else
	{
		if(Pos.x != 0 && Pos.y != 0)
			DmgPos = Pos;

		GameServer()->CreateBuildingHit(DmgPos);
		m_Status = DROIDSTATUS_HURT;
	}

	GameServer()->CreateDamageInd(DmgPos, GetAngle(-Force), -Dmg, -1);

	m_Vel += Force * 0.30f;
	if(length(m_Vel) > 8.0f)
		m_Vel = normalize(m_Vel) * 8.0f;

	const int HealthBefore = m_Health;
	m_Health -= Dmg;
	GameServer()->CreateHitConfirm(DmgPos, Source, min(Dmg, HealthBefore), HIT_TARGET_METAL, m_Health <= 0);

	if(m_Health <= 0)
	{
		if(GameServer()->m_pPveDirector)
			GameServer()->m_pPveDirector->OnDroidKilled(this, Source);
		CCharacter *pChr = GameServer()->GetPlayerChar(From);
		if(pChr)
			pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
	}

	m_DamageTakenTick = Server()->Tick();
}

bool CSiegeBreakerCrawler::IsGrounded()
{
	const float HalfWidth = CollisionSize * 0.5f;
	const float GroundY = m_Pos.y + CollisionSize * 0.5f + 5.0f;
	return GameServer()->Collision()->CheckPoint(m_Pos.x - HalfWidth, GroundY) ||
		   GameServer()->Collision()->CheckPoint(m_Pos.x + HalfWidth, GroundY);
}

void CSiegeBreakerCrawler::CreateLandingImpact()
{
	const vec2 ImpactPos = m_Pos + m_Center;
	GameServer()->CreateBuildingHit(ImpactPos);
	GameServer()->CreateBuildingHit(ImpactPos + vec2(-SiegeBreakerCrawlerPhysSize, 0));
	GameServer()->CreateBuildingHit(ImpactPos + vec2(SiegeBreakerCrawlerPhysSize, 0));
	GameServer()->CreateSound(ImpactPos, SOUND_BODY_LAND);
	GameServer()->CreateExplosion(ImpactPos, CAttackSource::Droid(TEAM_NEUTRAL, m_Type, true), LandingDamageScale);
}

void CSiegeBreakerCrawler::Tick()
{
	if(m_SnapTick && m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 5.0f)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
		{
			GameServer()->m_World.DestroyEntity(this);
			return;
		}
	}

	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID) * 0.2f;
	m_Vel.y += 0.8f;
	m_Vel *= 0.99f;

	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(CollisionSize, CollisionSize), 0, false);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, SiegeBreakerCrawlerPhysSize);

	const bool Grounded = IsGrounded();
	if(m_LandingImpactArmed && !Grounded)
		m_JumpWasAirborne = true;
	else if(m_Health > 0 && m_LandingImpactArmed && m_JumpWasAirborne)
	{
		CreateLandingImpact();
		m_LandingImpactArmed = false;
		m_JumpWasAirborne = false;
	}

	const int OffY = m_JumpTick ? 60 : 90;
	vec2 To = m_Pos + vec2(0, OffY);

	if(m_Health <= 0)
	{
		if(Server()->Tick() > m_DamageTakenTick + 40 || m_Vel.y > 10.0f)
		{
			if(Server()->Tick() > m_DamageTakenTick + 110 || abs(m_Vel.y) < 0.2f)
			{
				GameServer()->CreateExplosion(m_Pos + m_Center, CAttackSource::Droid(TEAM_NEUTRAL, m_Type, true));
				m_DeathTick = Server()->Tick();

				for(int i = 0; i < 3; i++)
				{
					if(frandom() < 0.3f)
						GameServer()->m_pController->DropPickup(
							m_Pos, POWERUP_AMMO, vec2(frandom() * 6.0f - frandom() * 6.0f, -frandom() * 14.0f), 0);
					else if(frandom() < 0.3f)
						GameServer()->m_pController->DropPickup(
							m_Pos, POWERUP_ARMOR, vec2(frandom() * 6.0f - frandom() * 6.0f, -frandom() * 14.0f), 0);
					else
						GameServer()->m_pController->DropPickup(
							m_Pos, POWERUP_KIT, vec2(frandom() * 6.0f - frandom() * 6.0f, -frandom() * 14.0f), 0);
				}

				if(frandom() < 0.25f)
					GameServer()->m_pController->DropWeapon(
						m_Pos,
						vec2(frandom() * 6.0f - frandom() * 6.0f, -frandom() * 14.0f),
						GameServer()->NewWeapon(CWeaponCatalog::Static(SW_UPGRADE)));
				else if(frandom() < 0.15f)
					GameServer()->m_pController->DropWeapon(
						m_Pos,
						vec2(frandom() * 6.0f - frandom() * 6.0f, -frandom() * 14.0f),
						GameServer()->NewWeapon(CWeaponCatalog::Static(SW_RESPAWNER)));

				GameServer()->m_World.DestroyEntity(this);
				return;
			}
		}

		m_Status = DROIDSTATUS_TERMINATED;
		return;
	}

	if(GameServer()->Collision()->IntersectLine(m_Pos, To, 0x0, &To, false, true))
	{
		if(abs(m_Vel.x) < 1.0f && frandom() < 0.05f)
			m_Move = frandom() < 0.5f ? -1 : 1;

		float VelX = m_Move;
		if(m_Status == DROIDSTATUS_ELECTRIC)
		{
			VelX *= 0.5f;
			m_Vel.x *= 0.85f;
		}

		const float VelY = m_Pos.y - (To.y - OffY) * 0.0002f;
		if(VelY > 0.0f && !m_JumpTick)
		{
			m_Vel.y -= min(1.4f, VelY);
			m_Vel.y *= 0.99f;
		}

		m_Vel.x *= 0.82f;
		if(m_Anim == DROIDANIM_ATTACK)
		{
			if(abs(m_Vel.x) < 10.0f)
				m_Vel.x += VelX * 1.2f;
		}
		else if(abs(m_Vel.x) < 6.0f)
			m_Vel.x += VelX * 0.65f;

		if(m_Status != DROIDSTATUS_ELECTRIC && !m_JumpTick &&
		   (frandom() < 0.005f || (abs(m_Vel.x) < 0.15f && frandom() < 0.2f) ||
			(abs(m_Target.x) > 300 && frandom() < 0.025f)))
		{
			m_JumpTick = Server()->Tick() + Server()->TickSpeed() * 0.25f;
		}

		if(m_JumpTick && m_JumpTick < Server()->Tick())
		{
			if(!m_LandingImpactArmed)
			{
				m_LandingImpactArmed = true;
				m_JumpWasAirborne = false;
			}

			if(abs(m_Target.x) > 300)
				m_JumpForce = -3.0f;
			else
				m_JumpForce = -4.2f - frandom() * 1.8f;
		}

		m_Vel.y += m_JumpForce;
		m_Vel.x -= m_JumpForce * VelX * 0.20f;

		if(m_JumpForce < -0.1f)
			m_Anim = DROIDANIM_JUMPATTACK;
		else if(abs(m_Target.x) > 20 && abs(m_Target.x) < 400)
			m_Anim = DROIDANIM_ATTACK;
		else
			m_Anim = DROIDANIM_IDLE;
	}
	else if(m_JumpTick && m_JumpTick < Server()->Tick())
		m_JumpTick = 0;
	else if(frandom() < 0.02f)
		m_Vel.x += (frandom() - frandom()) * 2.0f;

	m_Vel.x -= m_JumpForce * m_Move * 0.1f;
	m_JumpForce *= 0.9f;
	m_Dir = m_Move;

	if(!Target())
	{
		FindTarget();
		m_Anim = DROIDANIM_IDLE;
	}

	if(m_Anim == DROIDANIM_JUMPATTACK || m_Anim == DROIDANIM_ATTACK)
	{
		if(m_AttackCount++ > 3)
		{
			m_AttackCount = 0;
			const vec2 ProjPos = To + vec2(m_Move * 62.0f, -24.0f);
			GameServer()->CreateProjectile(
				CAttackSource::Droid(NEUTRAL_BASE, m_Type), 0, ProjPos, normalize(m_Pos - ProjPos), m_Pos);
		}
	}

	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;
}

bool CSiegeBreakerCrawler::Target()
{
	if(m_TargetIndex < 0 || m_TargetIndex >= MAX_CLIENTS)
		return false;

	CCharacter *pCharacter = GameServer()->GetPlayerChar(m_TargetIndex);
	if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
		return false;

	if(m_Move == -1 && pCharacter->m_Pos.x > m_Pos.x && frandom() < 0.15f)
		m_Move = 1;
	if(m_Move == 1 && pCharacter->m_Pos.x < m_Pos.x && frandom() < 0.15f)
		m_Move = -1;

	const int Distance = distance(pCharacter->m_Pos, m_Pos);
	if(Distance < 700 && !GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), m_Pos))
	{
		m_Target = pCharacter->m_Pos - m_Pos;
		return true;
	}

	m_Target = vec2(0, 0);
	return false;
}

bool CSiegeBreakerCrawler::FindTarget()
{
	m_TargetIndex = -1;
	CCharacter *pClosestCharacter = 0;
	int ClosestDistance = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pCharacter = GameServer()->GetPlayerChar(i);
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
			continue;
		if(GameServer()->m_pController->IsCoop() && pCharacter->m_IsBot)
			continue;

		if(abs(m_Pos.x - pCharacter->m_Pos.x) >= 600 || abs(m_Pos.y - pCharacter->m_Pos.y) >= 220)
			continue;
		if(GameServer()->Collision()->FastIntersectLine(pCharacter->m_Pos + vec2(0, -24), m_Pos))
			continue;

		const int Distance = distance(pCharacter->m_Pos, m_Pos);
		if(!pClosestCharacter || Distance < ClosestDistance)
		{
			pClosestCharacter = pCharacter;
			ClosestDistance = Distance;
			m_TargetIndex = i;
		}
	}

	return pClosestCharacter != 0;
}

void CSiegeBreakerCrawler::TickPaused()
{
}
