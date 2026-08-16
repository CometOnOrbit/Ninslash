#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "droid_mendercrawler.h"

namespace
{
constexpr int HealAmount = 30;
constexpr float HealRadius = 300.0f;
constexpr float PlayerAvoidRadius = 400.0f;
constexpr int HealIntervalSeconds = 2;
constexpr int MaxNearbyDroids = 256;
} // namespace

CMenderCrawler::CMenderCrawler(CGameWorld *pGameWorld, vec2 Pos) : CDroid(pGameWorld, Pos, DROIDTYPE_MENDERCRAWLER)
{
	m_StartPos = Pos;
	Reset();
	GameWorld()->InsertEntity(this);
}

void CMenderCrawler::Reset()
{
	m_Center = vec2(0, 0);
	m_Health = 350;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_Pos = m_StartPos;
	m_Status = DROIDSTATUS_IDLE;
	m_Dir = 1;
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
	m_ProximityRadius = MenderCrawlerPhysSize;
	m_FireDelay = 0;
	m_FireCount = 0;
	m_AttackTimer = 0;
	m_DamageTakenTick = 0;
	m_pHealTarget = nullptr;
	m_HealTick = Server()->Tick() + max(1, Server()->TickSpeed() * HealIntervalSeconds);
	m_Move = 1;
	m_JumpTick = 0;
	m_JumpForce = 0.0f;
	m_Anim = DROIDANIM_IDLE;
}

void CMenderCrawler::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	if(m_Health <= 0 || Dmg <= 0)
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
}

bool CMenderCrawler::IsValidHealTarget(const CDroid *pDroid) const
{
	return pDroid && pDroid != this && pDroid->m_Health > 0 && pDroid->m_MaxHealth > 0 &&
		   pDroid->m_Health < pDroid->m_MaxHealth;
}

void CMenderCrawler::FindHealTarget()
{
	m_pHealTarget = nullptr;
	CEntity *apEntities[MaxNearbyDroids];
	const int NumDroids =
		GameWorld()->FindEntities(m_Pos, 0.0f, apEntities, MaxNearbyDroids, CGameWorld::ENTTYPE_DROID);
	float ClosestDistanceSquared = 0.0f;

	for(int i = 0; i < NumDroids; i++)
	{
		CDroid *pDroid = static_cast<CDroid *>(apEntities[i]);
		if(!IsValidHealTarget(pDroid))
			continue;

		const vec2 Delta = pDroid->m_Pos + pDroid->m_Center - (m_Pos + m_Center);
		const float DistanceSquared = dot(Delta, Delta);
		if(!m_pHealTarget || DistanceSquared < ClosestDistanceSquared ||
		   (DistanceSquared == ClosestDistanceSquared &&
			(pDroid->m_Pos.x < m_pHealTarget->m_Pos.x ||
			 (pDroid->m_Pos.x == m_pHealTarget->m_Pos.x && pDroid->m_Pos.y < m_pHealTarget->m_Pos.y))))
		{
			m_pHealTarget = pDroid;
			ClosestDistanceSquared = DistanceSquared;
		}
	}
}

bool CMenderCrawler::EvadePlayers()
{
	CCharacter *pClosestCharacter = nullptr;
	float ClosestDistanceSquared = PlayerAvoidRadius * PlayerAvoidRadius;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pCharacter = GameServer()->GetPlayerChar(i);
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
			continue;

		const vec2 Delta = pCharacter->m_Pos - m_Pos;
		const float DistanceSquared = dot(Delta, Delta);
		if(DistanceSquared >= ClosestDistanceSquared)
			continue;

		pClosestCharacter = pCharacter;
		ClosestDistanceSquared = DistanceSquared;
	}

	if(!pClosestCharacter)
		return false;

	m_Target = m_Pos - pClosestCharacter->m_Pos;
	if(m_Target.x > 0.0f)
		m_Move = 1;
	else if(m_Target.x < 0.0f)
		m_Move = -1;
	return true;
}

void CMenderCrawler::HealNearbyDroids()
{
	if(Server()->Tick() < m_HealTick)
		return;

	m_HealTick = Server()->Tick() + max(1, Server()->TickSpeed() * HealIntervalSeconds);
	CEntity *apEntities[MaxNearbyDroids];
	const int NumDroids =
		GameWorld()->FindEntities(m_Pos, HealRadius, apEntities, MaxNearbyDroids, CGameWorld::ENTTYPE_DROID);
	bool Healed = false;

	for(int i = 0; i < NumDroids; i++)
	{
		CDroid *pDroid = static_cast<CDroid *>(apEntities[i]);
		if(!IsValidHealTarget(pDroid))
			continue;
		const vec2 Delta = pDroid->m_Pos + pDroid->m_Center - (m_Pos + m_Center);
		if(dot(Delta, Delta) > HealRadius * HealRadius)
			continue;

		const int RestoredHealth = min(HealAmount, pDroid->m_MaxHealth - pDroid->m_Health);
		pDroid->m_Health += RestoredHealth;
		GameServer()->CreateRepairInd(pDroid->m_Pos + pDroid->m_Center);
		Healed = true;
	}

	if(Healed)
		GameServer()->CreateSound(m_Pos + m_Center, SOUND_PICKUP_HEALTH);
}

void CMenderCrawler::Tick()
{
	if(m_SnapTick && m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 5.0f)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
		{
			GameWorld()->DestroyEntity(this);
			return;
		}
	}

	m_Vel += GameWorld()->m_Core.FindDroidHookImpactVel(m_ID) * 0.5f;
	m_Vel.y += 0.8f;
	m_Vel *= 0.99f;
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(60.0f, 60.0f), 0, false);
	GameWorld()->m_Core.AddDroid(m_ID, m_Pos, m_Vel, 30);

	if(m_Health <= 0)
	{
		if(Server()->Tick() > m_DamageTakenTick + 30 || m_Vel.y > 12.0f)
		{
			if(Server()->Tick() > m_DamageTakenTick + 90 || abs(m_Vel.y) < 0.2f)
			{
				GameServer()->CreateExplosion(m_Pos + m_Center, CAttackSource::Droid(TEAM_NEUTRAL, m_Type, true));
				m_DeathTick = Server()->Tick();
				GameWorld()->DestroyEntity(this);
				return;
			}
		}

		m_Status = DROIDSTATUS_TERMINATED;
		return;
	}

	FindHealTarget();

	const bool Evading = EvadePlayers();
	if(!Evading)
	{
		if(m_pHealTarget)
		{
			m_Target = m_pHealTarget->m_Pos + m_pHealTarget->m_Center - (m_Pos + m_Center);
			if(m_Target.x > 0.0f)
				m_Move = 1;
			else if(m_Target.x < 0.0f)
				m_Move = -1;
		}
		else
			m_Target = vec2(m_Move * HealRadius, 0.0f);
	}

	const int OffY = m_JumpTick ? 50 : 80;
	vec2 To = m_Pos + vec2(0, OffY);
	if(GameServer()->Collision()->IntersectLine(m_Pos, To, nullptr, &To, false, true))
	{
		if(abs(m_Vel.x) < 0.15f)
			m_Move *= -1;

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
		if(abs(m_Vel.x) < 8.0f)
			m_Vel.x += VelX * 0.9f;

		if(m_Status != DROIDSTATUS_ELECTRIC && !m_JumpTick && (abs(m_Vel.x) < 0.15f || abs(m_Target.x) > HealRadius))
			m_JumpTick = Server()->Tick() + Server()->TickSpeed() / 4;

		if(m_JumpTick && m_JumpTick < Server()->Tick())
			m_JumpForce = abs(m_Target.x) > HealRadius ? -5.0f : -7.0f;

		m_Vel.y += m_JumpForce;
		m_Vel.x -= m_JumpForce * VelX * 0.25f;
	}
	else if(m_JumpTick && m_JumpTick < Server()->Tick())
		m_JumpTick = 0;

	m_Vel.x -= m_JumpForce * m_Move * 0.1f;
	m_JumpForce *= 0.9f;
	m_Dir = m_Move;
	m_Anim = m_JumpForce < -0.1f ? DROIDANIM_JUMPATTACK : DROIDANIM_MOVE;

	HealNearbyDroids();
	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;
}

void CMenderCrawler::TickPaused()
{
}
