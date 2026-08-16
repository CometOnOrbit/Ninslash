#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>

#include "droid_railstar.h"
#include "laser.h"
#include "staticlaser.h"

static constexpr float RAILSTAR_RANGE = 1200.0f;
static constexpr float RAILSTAR_MIN_AIM_LENGTH = 0.001f;
static constexpr int RAILSTAR_DAMAGE = 26;
static constexpr int RAILSTAR_CHARGE_MS = 900;
static constexpr int RAILSTAR_RELOAD_MS = 2500;
static constexpr int RAILSTAR_TELEGRAPH_INTERVAL = 2;

CRailstar::CRailstar(CGameWorld *pGameWorld, vec2 Pos) : CDroid(pGameWorld, Pos, DROIDTYPE_RAILSTAR)
{
	m_StartPos = Pos;
	Reset();
	GameWorld()->InsertEntity(this);
}

void CRailstar::Reset()
{
	m_Center = vec2(0, 0);
	m_Health = 280;
	if(GameServer()->m_pPveDirector)
		m_Health = (int)(m_Health * GameServer()->m_pPveDirector->EnemyHealthMultiplier() + 0.5f);
	m_MaxHealth = m_Health;
	m_Pos = m_StartPos;
	m_Status = DROIDSTATUS_IDLE;
	m_Dir = -1;
	m_DeathTick = 0;
	SetState(IDLE);
	m_TargetIndex = -1;
	m_ReloadTimer = 0;
	m_AttackTick = 0;
	m_TargetTimer = 0;
	m_Target = vec2(0, 0);
	m_NewTarget = vec2(0, 0);
	m_Vel = vec2(0, 0);
	m_FlyTargetTick = 0;
	m_Mode = 0;
	m_ProximityRadius = DroidPhysSize;
	m_FireDelay = 0;
	m_FireCount = 0;
	m_AttackTimer = 0;
	m_DamageTakenTick = 0;
	m_Anim = DROIDANIM_IDLE;
	m_Charging = false;
	m_ChargeTicksRemaining = 0;
	m_CooldownTicksRemaining = 0;
	m_TelegraphTick = 0;
	m_HoverPhase = (m_ID % 32) * (pi / 16.0f);
}

void CRailstar::TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos)
{
	if(m_Health <= 0 || Dmg <= 0)
		return;

	CDroid::TakeDamage(Force, Dmg, Source, Pos);
	if(m_Health <= 0)
		CancelCharge();
}

vec2 CRailstar::MuzzlePos() const
{
	return m_Pos + vec2(m_Dir * 16.0f, m_Center.y - 20.0f);
}

bool CRailstar::ResolveTarget(vec2 *pDirection)
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

	const vec2 TargetPos = pCharacter->m_Pos + vec2(0, -24) + pCharacter->GetCore().m_Vel * 2.0f;
	m_Dir = TargetPos.x < m_Pos.x ? -1 : 1;
	const vec2 Muzzle = MuzzlePos();
	const vec2 Aim = TargetPos - Muzzle;
	if(length(Aim) > RAILSTAR_RANGE || length(Aim) <= RAILSTAR_MIN_AIM_LENGTH)
		return false;

	if(GameServer()->Collision()->FastIntersectLine(Muzzle, TargetPos))
		return false;

	*pDirection = normalize(Aim);
	m_NewTarget = -Aim;
	m_Target = m_NewTarget;
	return true;
}

bool CRailstar::FindTarget()
{
	m_TargetIndex = -1;
	float ClosestDistance = RAILSTAR_RANGE;
	const vec2 Muzzle = MuzzlePos();

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
		const float Distance = distance(TargetPos, Muzzle);
		if(Distance >= ClosestDistance || GameServer()->Collision()->FastIntersectLine(Muzzle, TargetPos))
			continue;

		ClosestDistance = Distance;
		m_TargetIndex = i;
	}

	return m_TargetIndex >= 0;
}

void CRailstar::BeginCharge()
{
	m_Charging = true;
	m_ChargeTicksRemaining = max(1, RAILSTAR_CHARGE_MS * Server()->TickSpeed() / 1000);
	m_TelegraphTick = 0;
	m_AttackTick = Server()->Tick();
	m_Anim = DROIDANIM_ATTACK;
	GameServer()->CreateEffect(FX_LAZERLOAD, MuzzlePos());
}

void CRailstar::CancelCharge()
{
	m_Charging = false;
	m_ChargeTicksRemaining = 0;
	m_TelegraphTick = 0;
	m_AttackTick = 0;
	m_Anim = DROIDANIM_IDLE;
}

void CRailstar::ShowTelegraph(const vec2 &Direction)
{
	if(m_TelegraphTick-- > 0)
		return;

	m_TelegraphTick = RAILSTAR_TELEGRAPH_INTERVAL;
	const vec2 From = MuzzlePos();
	vec2 To = From + Direction * RAILSTAR_RANGE;
	vec2 CollisionPos;
	GameServer()->Collision()->IntersectLine(From, To, &CollisionPos, &To);
	new CStaticlaser(GameWorld(), From, To, RAILSTAR_TELEGRAPH_INTERVAL + 1);
}

void CRailstar::FireRail(const vec2 &Direction)
{
	const vec2 Muzzle = MuzzlePos();
	new CLaser(GameWorld(),
			   Muzzle,
			   Direction,
			   RAILSTAR_RANGE,
			   CAttackSource::Droid(NEUTRAL_BASE, m_Type),
			   RAILSTAR_DAMAGE,
			   0,
			   WEAPON_INFINITE_PENETRATION);
	GameServer()->CreateSound(Muzzle, SOUND_LASER_FIRE);
	m_Vel -= Direction * 5.0f;
	m_AttackTick = Server()->Tick();
	m_CooldownTicksRemaining = max(1, RAILSTAR_RELOAD_MS * Server()->TickSpeed() / 1000);
	m_Charging = false;
	m_ChargeTicksRemaining = 0;
	m_TelegraphTick = 0;
	m_Anim = DROIDANIM_IDLE;
}

void CRailstar::Tick()
{
	if(TickControlled())
		return;

	if(m_SnapTick && m_SnapTick < Server()->Tick() - Server()->TickSpeed() * 5.0f)
	{
		if(GameServer()->StoreEntity(m_ObjType, m_Type, 0, m_Pos.x, m_Pos.y))
		{
			CancelCharge();
			GameServer()->m_World.DestroyEntity(this);
			return;
		}
	}

	if(m_Health <= 0)
	{
		CancelCharge();
		m_Status = DROIDSTATUS_TERMINATED;
		m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID);
		m_Vel.y += 0.8f;
		m_Vel *= 0.98f;
		GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(80.0f, 100.0f), 0, false, true);
		GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 40);
		if(Server()->Tick() > m_DeathTick + Server()->TickSpeed())
			GameServer()->m_World.DestroyEntity(this);
		return;
	}

	if(GameServer()->Collision()->IsInFluid(m_Pos.x, m_Pos.y))
	{
		TakeDamage(vec2(0, -0.5f), 2, CAttackSource::World(DAMAGETYPE_FLUID), vec2(0, 0));
		if(m_Health <= 0)
			return;
	}

	vec2 Direction;
	bool HasTarget = ResolveTarget(&Direction);
	if(!HasTarget && !m_Charging && FindTarget())
		HasTarget = ResolveTarget(&Direction);

	if(m_Charging)
	{
		if(!HasTarget)
		{
			m_TargetIndex = -1;
			CancelCharge();
		}
		else
		{
			ShowTelegraph(Direction);
			if(--m_ChargeTicksRemaining <= 0)
				FireRail(Direction);
		}
	}
	else
	{
		if(m_CooldownTicksRemaining > 0)
			--m_CooldownTicksRemaining;
		else if(HasTarget)
			BeginCharge();
	}

	const float Hover = sin(Server()->Tick() * 0.035f + m_HoverPhase);
	if(HasTarget)
	{
		const float TargetDistance = length(m_NewTarget);
		const vec2 TowardTarget = -normalize(m_NewTarget);
		if(TargetDistance > 760.0f)
			m_Vel += TowardTarget * 0.25f;
		else if(TargetDistance < 520.0f)
			m_Vel -= TowardTarget * 0.3f;
	}
	m_Vel.y += Hover * 0.04f;
	m_Vel += GameServer()->m_World.m_Core.FindDroidHookImpactVel(m_ID) * 0.25f;
	m_Vel *= 0.97f;
	if(length(m_Vel) > 12.0f)
		m_Vel = normalize(m_Vel) * 12.0f;
	GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(96.0f, 128.0f), 0, false, true);
	GameServer()->m_World.m_Core.AddDroid(m_ID, m_Pos, m_Vel, 40);

	if(Server()->Tick() > m_DamageTakenTick + 15 && m_Status != DROIDSTATUS_TERMINATED)
		m_Status = DROIDSTATUS_IDLE;
}

void CRailstar::TickPaused()
{
}
