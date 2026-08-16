#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "droid_kamikazestar.h"

static constexpr float KAMIKAZE_STAR_TARGET_RANGE = 850.0f;
static constexpr float KAMIKAZE_STAR_DETONATION_RANGE = 70.0f;
static constexpr float KAMIKAZE_STAR_MIN_AIM_LENGTH = 0.001f;
static constexpr float KAMIKAZE_STAR_DIVE_ACCELERATION = 0.9f;
static constexpr float KAMIKAZE_STAR_MAX_SPEED = 26.0f;
static constexpr int KAMIKAZE_STAR_DIVE_TIMEOUT_SECONDS = 4;

CKamikazeStar::CKamikazeStar(CGameWorld *pGameWorld, vec2 Pos) : CDroid(pGameWorld, Pos, DROIDTYPE_KAMIKAZESTAR)
{
	Reset();
	GameWorld()->InsertEntity(this);
}

void CKamikazeStar::Reset()
{
	CDroid::Reset();
	m_Center = vec2(0, 0);
	m_Health = 120;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_ProximityRadius = KamikazeStarPhysSize;
	m_Anim = DROIDANIM_IDLE;
	m_DamageTakenTick = 0;
	m_Diving = false;
	m_Detonated = false;
	m_DiveStartTick = 0;
}

bool CKamikazeStar::ResolveTarget(vec2 *pTargetPos)
{
	if(m_TargetIndex < 0 || m_TargetIndex >= MAX_CLIENTS)
		return false;

	CPlayer *pPlayer = GameServer()->m_apPlayers[m_TargetIndex];
	if(!pPlayer)
		return false;

	CCharacter *pCharacter = pPlayer->GetCharacter();
	if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
		return false;

	if(GameServer()->m_pController->IsCoop() && pCharacter->m_IsBot)
		return false;

	const vec2 TargetPos = pCharacter->m_Pos + vec2(0, -24);
	const vec2 Aim = TargetPos - (m_Pos + m_Center);
	if(length(Aim) <= KAMIKAZE_STAR_MIN_AIM_LENGTH || length(Aim) > KAMIKAZE_STAR_TARGET_RANGE)
		return false;

	if(GameServer()->Collision()->FastIntersectLine(m_Pos + m_Center, TargetPos))
		return false;

	m_Dir = Aim.x < 0.0f ? -1 : 1;
	m_Target = -Aim;
	*pTargetPos = TargetPos;
	return true;
}

bool CKamikazeStar::FindTarget()
{
	m_TargetIndex = -1;
	float ClosestDistance = KAMIKAZE_STAR_TARGET_RANGE;
	const vec2 Origin = m_Pos + m_Center;

	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer)
			continue;

		CCharacter *pCharacter = pPlayer->GetCharacter();
		if(!pCharacter || !pCharacter->IsAlive() || pCharacter->Invisible())
			continue;

		if(GameServer()->m_pController->IsCoop() && pCharacter->m_IsBot)
			continue;

		const vec2 TargetPos = pCharacter->m_Pos + vec2(0, -24);
		const float Distance = length(TargetPos - Origin);
		if(Distance <= KAMIKAZE_STAR_MIN_AIM_LENGTH || Distance >= ClosestDistance ||
		   GameServer()->Collision()->FastIntersectLine(Origin, TargetPos))
			continue;

		ClosestDistance = Distance;
		m_TargetIndex = i;
	}

	return m_TargetIndex >= 0;
}

void CKamikazeStar::Detonate()
{
	if(m_Detonated)
		return;

	m_Detonated = true;
	m_Diving = false;
	m_Health = 0;
	m_Status = DROIDSTATUS_TERMINATED;
	m_DeathTick = Server()->Tick();
	GameServer()->CreateExplosion(m_Pos + m_Center, CAttackSource::Droid(TEAM_NEUTRAL, m_Type, true));

	if(frandom() * 10 < 4)
		GameServer()->m_pController->DropPickup(m_Pos, POWERUP_AMMO, m_Vel, 0);
	else if(frandom() * 10 < 4)
		GameServer()->m_pController->DropPickup(m_Pos, POWERUP_HEALTH, m_Vel, 0);
	else if(frandom() * 10 < 4)
		GameServer()->m_pController->DropPickup(m_Pos, POWERUP_ARMOR, m_Vel, 0);
	else
		GameServer()->m_pController->DropPickup(m_Pos, POWERUP_KIT, m_Vel, 0);

	GameServer()->m_World.DestroyEntity(this);
}

void CKamikazeStar::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	if(m_Detonated || m_Health <= 0 || Dmg <= 0)
		return;

	const int From = Source.m_Owner;
	CWeaponCombatProfile Combat{};
	CWeaponCatalog::TryResolveAttack(Source, &Combat);
	if(g_Config.m_SvOneHitKill)
		Dmg = 1000;
	if(GameServer()->m_pPveDirector)
		Dmg = GameServer()->m_pPveDirector->ModifyDroidDamage(Source, Dmg, false, this);

	vec2 DamagePos = m_Pos + m_Center;
	if(Combat.m_ElectroAmount > 0.0f)
		m_Status = DROIDSTATUS_ELECTRIC;
	else if(Combat.m_FlameAmount > 0.0f)
		m_Status = DROIDSTATUS_HURT;
	else
	{
		if(Pos.x != 0 && Pos.y != 0)
			DamagePos = Pos;
		GameServer()->CreateBuildingHit(DamagePos);
		m_Status = DROIDSTATUS_HURT;
	}

	GameServer()->CreateDamageInd(DamagePos, GetAngle(-Force), -Dmg, -1);
	m_Vel += Force * 0.75f;
	if(length(m_Vel) > KAMIKAZE_STAR_MAX_SPEED)
		m_Vel = normalize(m_Vel) * KAMIKAZE_STAR_MAX_SPEED;

	const int HealthBefore = m_Health;
	m_Health -= Dmg;
	GameServer()->CreateHitConfirm(DamagePos, Source, min(Dmg, HealthBefore), HIT_TARGET_METAL, m_Health <= 0);
	if(m_Health > 0)
	{
		m_DamageTakenTick = Server()->Tick();
		return;
	}

	if(GameServer()->m_pPveDirector)
		GameServer()->m_pPveDirector->OnDroidKilled(this, Source);
	CCharacter *pCharacter = GameServer()->GetPlayerChar(From);
	if(pCharacter)
		pCharacter->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());

	Detonate();
}

void CKamikazeStar::Tick()
{
	if(m_Detonated)
		return;

	if(m_SnapTick && m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 5)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
		{
			GameServer()->m_World.DestroyEntity(this);
			return;
		}
	}

	if(m_Health <= 0)
	{
		Detonate();
		return;
	}

	if(GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
	{
		TakeDamage(vec2(0, -0.5f), 2, CAttackSource::World(DAMAGETYPE_FLUID), vec2(0, 0));
		if(m_Detonated)
			return;
	}

	vec2 TargetPos;
	bool HasTarget = ResolveTarget(&TargetPos);
	if(!HasTarget)
	{
		m_TargetIndex = -1;
		m_Diving = false;
		m_Anim = DROIDANIM_IDLE;
		if(FindTarget())
			HasTarget = ResolveTarget(&TargetPos);
	}

	if(HasTarget)
	{
		if(!m_Diving)
		{
			m_Diving = true;
			m_DiveStartTick = Server()->Tick();
		}

		const int TimeoutTicks = max(1, KAMIKAZE_STAR_DIVE_TIMEOUT_SECONDS * Server()->TickSpeed());
		if(Server()->Tick() - m_DiveStartTick >= TimeoutTicks)
		{
			m_TargetIndex = -1;
			m_Diving = false;
			m_Anim = DROIDANIM_IDLE;
			return;
		}

		m_Anim = DROIDANIM_ATTACK;
		const vec2 Dive = TargetPos - (m_Pos + m_Center);
		if(length(Dive) < KAMIKAZE_STAR_DETONATION_RANGE)
		{
			Detonate();
			return;
		}
		m_Vel += normalize(Dive) * KAMIKAZE_STAR_DIVE_ACCELERATION *
				 (GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->EnemySpeedMultiplier() : 1.0f);
	}

	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID) * 0.25f;
	m_Vel *= 0.98f;
	if(length(m_Vel) > KAMIKAZE_STAR_MAX_SPEED)
		m_Vel = normalize(m_Vel) * KAMIKAZE_STAR_MAX_SPEED;
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(96.0f, 128.0f), 0, false, true);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 40);

	if(Server()->Tick() > m_DamageTakenTick + 15 && m_Status != DROIDSTATUS_TERMINATED)
		m_Status = DROIDSTATUS_IDLE;
}

void CKamikazeStar::TickPaused()
{
	if(m_Diving)
		++m_DiveStartTick;
}
