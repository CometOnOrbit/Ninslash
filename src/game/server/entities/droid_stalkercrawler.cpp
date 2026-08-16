#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "droid_stalkercrawler.h"

namespace
{
constexpr float s_EngagementRange = 400.0f;

constexpr int StealthStatus(bool Attacking, bool Detected, bool OutsideEngagement)
{
	return !Attacking && !Detected && OutsideEngagement ? DROIDSTATUS_STEALTH : DROIDSTATUS_IDLE;
}

static_assert(StealthStatus(false, false, true) == DROIDSTATUS_STEALTH);
static_assert(StealthStatus(true, false, true) == DROIDSTATUS_IDLE);
static_assert(StealthStatus(false, true, true) == DROIDSTATUS_IDLE);
static_assert(StealthStatus(false, false, false) == DROIDSTATUS_IDLE);
} // namespace

CStalkerCrawler::CStalkerCrawler(CGameWorld *pGameWorld, vec2 Pos) : CDroid(pGameWorld, Pos, DROIDTYPE_STALKERCRAWLER)
{
	m_StartPos = Pos;
	Reset();
	GameWorld()->InsertEntity(this);
}

void CStalkerCrawler::Reset()
{
	m_Center = vec2(0, 0);
	m_Health = 300;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_Pos = m_StartPos;
	m_Status = DROIDSTATUS_STEALTH;
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
	m_ProximityRadius = StalkerCrawlerPhysSize;
	m_FireDelay = 0;
	m_FireCount = 0;
	m_AttackTimer = 0;
	m_DamageTakenTick = 0;
	m_Move = 0;
	m_AttackCount = 0;
	m_JumpTick = 0;
	m_JumpForce = 0.0f;
	m_RevealUntilTick = 0;
}

void CStalkerCrawler::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
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
	m_Vel += Force * 0.75f;
	if(length(m_Vel) > 20.0f)
		m_Vel = normalize(m_Vel) * 20.0f;

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
	m_RevealUntilTick = Server()->Tick() + Server()->TickSpeed();
}

void CStalkerCrawler::Tick()
{
	if(m_SnapTick && m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 5.0f)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
		{
			GameServer()->m_World.DestroyEntity(this);
			return;
		}
	}

	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID) * 0.5f;
	m_Vel.y += 0.8f;
	m_Vel *= 0.99f;

	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(60.0f, 60.0f), 0, false);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 30);

	const int OffY = m_JumpTick ? 50 : 80;
	vec2 To = m_Pos + vec2(0, OffY);

	if(m_Health <= 0)
	{
		if(Server()->Tick() > m_DamageTakenTick + 30 || m_Vel.y > 12.0f)
		{
			if(Server()->Tick() > m_DamageTakenTick + 90 || abs(m_Vel.y) < 0.2f)
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

		m_Vel.x *= 0.8f;
		if(m_Anim == DROIDANIM_ATTACK)
		{
			if(abs(m_Vel.x) < 15.0f)
				m_Vel.x += VelX * 1.8f;
		}
		else if(abs(m_Vel.x) < 8.0f)
			m_Vel.x += VelX * 0.9f;

		if(m_Status != DROIDSTATUS_ELECTRIC && !m_JumpTick &&
		   (frandom() < 0.01f || (abs(m_Vel.x) < 0.15f && frandom() < 0.4f) ||
			(abs(m_Target.x) > 300 && frandom() < 0.05f)))
			m_JumpTick = Server()->Tick() + Server()->TickSpeed() * 0.25f;

		if(m_JumpTick && m_JumpTick < Server()->Tick())
		{
			if(abs(m_Target.x) > 300)
				m_JumpForce = -5.0f;
			else
				m_JumpForce = -7.0f - frandom() * 3.0f;
		}

		m_Vel.y += m_JumpForce;
		m_Vel.x -= m_JumpForce * VelX * 0.25f;

		if(m_JumpForce < -0.1f)
			m_Anim = DROIDANIM_JUMPATTACK;
		else if(abs(m_Target.x) > 20 && abs(m_Target.x) < s_EngagementRange)
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

	bool HasTarget = Target();
	if(!HasTarget)
	{
		HasTarget = FindTarget() && Target();
		if(!HasTarget)
			m_Anim = DROIDANIM_IDLE;
	}

	if(m_Anim == DROIDANIM_JUMPATTACK || m_Anim == DROIDANIM_ATTACK)
	{
		if(m_AttackCount++ > 3)
		{
			m_AttackCount = 0;
			const vec2 ProjPos = To + vec2(m_Move * 54.0f, -20.0f);
			GameServer()->CreateProjectile(
				CAttackSource::Droid(NEUTRAL_BASE, m_Type), 0, ProjPos, normalize(m_Pos - ProjPos), m_Pos);
		}
	}
	else
		m_AttackCount = 0;

	UpdateStealthStatus(HasTarget);
}

bool CStalkerCrawler::Target()
{
	if(m_TargetIndex < 0 || m_TargetIndex >= MAX_CLIENTS)
	{
		m_Target = vec2(0, 0);
		return false;
	}

	CCharacter *pCharacter = GameServer()->GetPlayerChar(m_TargetIndex);
	if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
	{
		m_TargetIndex = -1;
		m_Target = vec2(0, 0);
		return false;
	}

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

	m_TargetIndex = -1;
	m_Target = vec2(0, 0);
	return false;
}

bool CStalkerCrawler::FindTarget()
{
	m_TargetIndex = -1;
	CCharacter *pClosestCharacter = nullptr;
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

	return pClosestCharacter != nullptr;
}

void CStalkerCrawler::UpdateStealthStatus(bool HasTarget)
{
	if(m_Status == DROIDSTATUS_HURT || m_Status == DROIDSTATUS_ELECTRIC)
	{
		if(Server()->Tick() <= m_DamageTakenTick + 15)
			return;
	}

	const bool Attacking = m_Anim == DROIDANIM_ATTACK || m_Anim == DROIDANIM_JUMPATTACK;
	const bool Detected = Server()->Tick() < m_RevealUntilTick;
	const bool OutsideEngagement = !HasTarget || length(m_Target) > s_EngagementRange;
	m_Status = StealthStatus(Attacking, Detected, OutsideEngagement);
}

void CStalkerCrawler::TickPaused()
{
}
