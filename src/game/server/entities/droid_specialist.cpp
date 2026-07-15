#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>
#include "character.h"
#include "projectile.h"
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
}

CCharacter *CSpecialistDroid::TargetCharacter()
{
	return m_TargetIndex >= 0 && m_TargetIndex < MAX_CLIENTS ? GameServer()->GetPlayerChar(m_TargetIndex) : 0;
}

bool CSpecialistDroid::AcquireTarget(float Range, bool RequireSight)
{
	CCharacter *pBest = 0;
	float Best = Range;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if(!pChr || !pChr->IsAlive() || pChr->Invisible() || (GameServer()->m_pController->IsCoop() && pChr->m_IsBot))
			continue;
		const float Dist = distance(m_Pos, pChr->m_Pos);
		if(Dist >= Best || (RequireSight && GameServer()->Collision()->FastIntersectLine(m_Pos + m_Center, pChr->m_Pos - vec2(0, 24))))
			continue;
		Best = Dist;
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
	const int Weapon = GetDroidWeapon(m_Type);
	new CProjectile(&GameServer()->m_World, Weapon, NEUTRAL_BASE,
		m_Pos + m_Center + Dir * 24.0f, Dir, vec2(0, 0),
		(int)(Server()->TickSpeed() * GetProjectileLife(Weapon)), Damage,
		0, GetProjectileKnockback(Weapon), -1);
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

void CSpecialistDroid::TakeDamage(vec2 Force, int Dmg, int From, vec2 Pos, int Weapon)
{
	if(m_Type == DROIDTYPE_BULWARK && TargetCharacter())
	{
		const vec2 Incoming = Pos - (m_Pos + m_Center);
		if(Incoming.x * m_Dir < 0.0f)
			Dmg = max(1, Dmg / 3);
	}
	CDroid::TakeDamage(Force, Dmg, From, Pos, Weapon);
	if(m_Health <= 0)
		OnSpecialistDeath();
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

void CSpecialistDroid::Tick()
{
	if(m_Health <= 0)
	{
		GameServer()->m_World.DestroyEntity(this);
		return;
	}
	AcquireTarget(m_IsBoss ? 1200.0f : 900.0f);
	CCharacter *pTarget = TargetCharacter();
	if(pTarget)
	{
		m_Dir = pTarget->m_Pos.x < m_Pos.x ? -1 : 1;
		m_Target = pTarget->m_Pos - (m_Pos + m_Center);
	}
	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID) * 0.25f;
	m_Vel.y += 0.8f;
	if(pTarget && distance(m_Pos, pTarget->m_Pos) > 320.0f)
		m_Vel.x += m_Dir * (m_IsBoss ? 0.30f : 0.45f) * (GameServer()->m_pPveDirector ? GameServer()->m_pPveDirector->EnemySpeedMultiplier() : 1.0f);
	m_Vel.x *= 0.88f;
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, m_IsBoss ? vec2(88, 88) : vec2(56, 64), 0, false);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, m_IsBoss ? 44 : 30);
	if(Server()->Tick() >= m_AbilityTick)
		AbilityTick();
	if(Server()->Tick() > m_DamageTakenTick + 15)
		m_Status = DROIDSTATUS_IDLE;
}

void CSpecialistDroid::TickPaused()
{
	m_AbilityTick++;
}
