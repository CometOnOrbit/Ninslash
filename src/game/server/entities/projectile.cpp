#include <generated/protocol.h>
#include <game/collision.h>
#include <game/server/gamecontext.h>
#include <game/weapons.h>
#include "projectile.h"
#include "ball.h"
#include "building.h"
#include "droid.h"
#include "electro.h"

inline vec2 RandomDir()
{
	return normalize(vec2(frandom() - 0.5f, frandom() - 0.5f));
}

namespace
{
constexpr float PROJECTILE_DIRECTION_NETWORK_SCALE = 100.0f;
constexpr float PROJECTILE_VELOCITY_NETWORK_SCALE = 10.0f;
} // namespace

CProjectile::CProjectile(CGameWorld *pGameWorld,
						 const CAttackSource &Source,
						 vec2 Pos,
						 vec2 Dir,
						 vec2 Vel,
						 int Span,
						 int Damage,
						 float Force,
						 int SoundImpact,
						 float ExplosionDamageScale,
						 int Penetration)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE)
{
	m_Source = Source;
	m_Pos = Pos;
	m_Direction = Dir;
	m_LifeSpan = Span;
	m_Owner = Source.m_Owner;
	m_Force = Force;
	m_Damage = Damage;
	m_ExplosionDamageScale = ExplosionDamageScale;
	m_SoundImpact = SoundImpact;
	m_StartTick = Server()->Tick();
	m_Bounces = 0;
	m_InfinitePenetration = (Penetration == WEAPON_INFINITE_PENETRATION);
	m_RemainingPenetrations = max(0, Penetration);
	m_pPenetratedCharacter = 0;
	m_pPenetratedDroid = 0;
	m_Vel2 = Vel * 30.0f;

	m_ElectroTimer = 0;

	m_OwnerBuilding = 0;
	BounceTick = 0;
	m_SkipCollision = false;

	UpdateStats();

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CProjectile::UpdateStats()
{
	CWeaponCombatProfile Combat{};
	CWeaponVisualProfile Visual{};
	CWeaponCatalog::TryResolveAttack(m_Source,
		&Combat,
		&Visual,
		GameServer()->m_pController && !GameServer()->m_pController->IsCoop());
	m_Speed = Combat.m_ProjectileSpeed;
	m_Curvature = Combat.m_ProjectileCurvature;
	m_ProjectilePosType = Combat.m_ProjectilePosType;
	m_Explosive = Combat.m_ExplosiveProjectile;
	m_ProjectileSize = Visual.m_ProjectileSize;
	m_BehaviorFlags = 0;
	m_WeaponLevel = 0;
	m_WeaponMaxLevel = 0;
	if(m_Source.m_Kind == EAttackSourceKind::PlayerWeapon)
	{
		CWeaponDefinition Definition;
		if(CWeaponCatalog::TryGetDefinition(m_Source.m_Weapon.m_DefinitionId, &Definition))
		{
			m_BehaviorFlags = Definition.m_BehaviorFlags;
			m_WeaponLevel = m_Source.m_Weapon.m_Level;
			m_WeaponMaxLevel = Definition.m_MaxLevel;
		}
	}
	m_Bounces = Combat.m_ProjectileBounces;
}

vec2 CProjectile::GetPos(float Time)
{
	if(m_ProjectilePosType == WEAPON_PROJECTILE_PATH_LOG)
		return CalcLogPos(m_Pos, m_Direction, m_Vel2, m_Curvature, m_Speed, Time);

	if(m_ProjectilePosType == WEAPON_PROJECTILE_PATH_ROCKET)
		return CalcRocketPos(m_Pos, m_Direction, m_Vel2, m_Curvature, m_Speed, Time);

	return CalcPos(m_Pos, m_Direction, m_Vel2, m_Curvature, m_Speed, Time);
}

// todo: fix broken bouncing
bool CProjectile::Bounce(vec2 Pos, int Collision)
{
	if(m_Bounces-- > 0)
	{
		BounceTick = Server()->Tick();

		m_Direction = GameServer()->Collision()->WallReflect(Pos, m_Direction, Collision);
		m_Vel2 = GameServer()->Collision()->WallReflect(Pos, m_Vel2, Collision);

		if(m_BehaviorFlags & WEAPON_BEHAVIOR_CLUSTER)
			GameServer()->CreateSound(Pos, SOUND_SFX_BOUNCE1);
		else
			GameServer()->CreateSound(Pos, SOUND_BOUNCER_BOUNCE);

		return true;
	}

	return false;
}

void CProjectile::Tick()
{
	float Pt = (Server()->Tick() - m_StartTick - 1) / (float)Server()->TickSpeed();
	float Ct = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();
	vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	int Collide = 0;
	CCharacter *OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	CCharacter *TargetChr = 0;
	CCharacter *ReflectChr = 0;

	if(m_SkipCollision)
		m_SkipCollision = false;
	else
		Collide = GameServer()->Collision()->IntersectLine(PrevPos, CurPos, &CurPos, 0);

	float r = 6.0f * m_ProjectileSize;

	// Reflection and ordinary character hits share the same candidate list.
	// Resolve both in one pass; reflection keeps its original priority.
	TargetChr = GameServer()->m_World.IntersectCharacter(
		PrevPos, CurPos, r, CurPos, OwnerChar, false, &ReflectChr, r * 0.8f, m_pPenetratedCharacter);

	int Team = m_Owner;

	if(OwnerChar && GameServer()->m_pController->IsTeamplay())
		Team = OwnerChar->GetPlayer()->GetTeam();

	CBuilding *TargetBuilding = 0;

	TargetBuilding = GameServer()->m_World.IntersectBuilding(PrevPos, CurPos, r, CurPos, Team);

	CBall *Ball = 0;
	Ball = GameServer()->m_World.IntersectBall(PrevPos, CurPos, r, CurPos);

	bool Shielded = GameServer()->m_World.IsShielded(PrevPos, CurPos, r, Team);

	if(m_OwnerBuilding == TargetBuilding)
		TargetBuilding = 0;

	CDroid *TargetMonster = 0;

	TargetMonster = GameServer()->m_World.IntersectWalker(PrevPos, CurPos, r, CurPos, m_pPenetratedDroid);

	if(m_Owner == NEUTRAL_BASE)
		TargetMonster = 0;

	m_LifeSpan--;

	if(Collide && Bounce(CurPos, Collide))
	{
		m_StartTick = Server()->Tick();
		m_SkipCollision = true;
		m_Pos = CurPos;
		Collide = false;
	}

	if(ReflectChr)
	{
		m_StartTick = Server()->Tick() - 1;
		m_Pos = CurPos;

		// m_Direction.y *= -1;
		// m_Direction.x *= -1;

		// vec2 d = (ReflectChr->m_Pos+vec2(0, -24))-PrevPos;
		// d += vec2(frandom()-frandom(), frandom()-frandom()) * length(d) * 0.4f;
		// m_Direction = -normalize(d);

		vec2 d = (ReflectChr->m_Pos + vec2(0, -24)) - PrevPos;
		m_Direction = GameServer()->Collision()->Reflect(m_Direction, normalize(d));
		m_Vel2 = GameServer()->Collision()->Reflect(m_Vel2, normalize(d));
		GameServer()->CreateBuildingHit(CurPos);
	}

	if(Collide)
	{
		if(GameServer()->Collision()->CheckBlocks(CurPos))
			GameServer()->DamageBlocks(CurPos, m_Damage, 1);
		else if(GameServer()->Collision()->CheckBlocks(CurPos + vec2(-4, -4)))
			GameServer()->DamageBlocks(CurPos + vec2(-4, -4), m_Damage, 1);
		else if(GameServer()->Collision()->CheckBlocks(CurPos + vec2(4, -4)))
			GameServer()->DamageBlocks(CurPos + vec2(4, -4), m_Damage, 1);
		else if(GameServer()->Collision()->CheckBlocks(CurPos + vec2(-4, 4)))
			GameServer()->DamageBlocks(CurPos + vec2(-4, 4), m_Damage, 1);
		else if(GameServer()->Collision()->CheckBlocks(CurPos + vec2(4, 4)))
			GameServer()->DamageBlocks(CurPos + vec2(4, 4), m_Damage, 1);
	}

	const bool Clipped = GameLayerClipped(CurPos);
	if(Ball || TargetMonster || TargetBuilding || TargetChr || Collide || m_LifeSpan < 0 || Shielded || Clipped)
	{
		const bool PenetratesTarget = (m_InfinitePenetration || m_RemainingPenetrations > 0) &&
									  (TargetChr || TargetMonster) && !Ball && !TargetBuilding && !Collide &&
									  m_LifeSpan >= 0 && !Shielded && !Clipped;

		if(TargetChr)
		{
			vec2 Force = m_Direction * max(0.001f, m_Force);
			TargetChr->TakeDamage(m_Source, m_Damage, Force, CurPos);

			GameServer()->CreateEffect(FX_BLOOD2, (CurPos + TargetChr->m_Pos) / 2.0f + vec2(0, -4));
		}

		if(Shielded)
			GameServer()->CreateEffect(FX_SHIELDHIT, CurPos);

		if(TargetBuilding)
		{
			vec2 Force = m_Direction * max(0.001f, m_Force);

			if(TargetBuilding->m_Type == BUILDING_GENERATOR)
			{
				TargetBuilding->m_DamagePos = CurPos;

				if(distance(TargetBuilding->m_Pos, CurPos) > TargetBuilding->m_ProximityRadius)
				{
					GameServer()->CreateEffect(FX_SHIELDHIT, CurPos);
					TargetBuilding->TakeDamage(m_Damage / 3, m_Source, Force);
				}
				else
				{
					GameServer()->CreateBuildingHit(CurPos);
					TargetBuilding->TakeDamage(m_Damage, m_Source, Force);
				}
			}
			else
			{
				GameServer()->CreateBuildingHit(CurPos);
				TargetBuilding->TakeDamage(m_Damage, m_Source, Force);
			}
		}

		if(TargetMonster)
		{
			TargetMonster->TakeDamage(m_Direction * max(0.001f, m_Force), m_Damage, m_Source, CurPos);
		}

		if(Ball)
		{
			vec2 Force = m_Direction * max(0.001f, m_Force);
			Ball->AddForce(Force);
			GameServer()->m_pController->m_LastBallToucher = m_Owner;
		}

		if(PenetratesTarget)
		{
			if(!m_InfinitePenetration)
				--m_RemainingPenetrations;
			if(TargetChr)
				m_pPenetratedCharacter = TargetChr;
			if(TargetMonster)
				m_pPenetratedDroid = TargetMonster;
			if(m_Explosive)
				GameServer()->CreateExplosion(CurPos, m_Source, m_ExplosionDamageScale);
		}

		// cluster grenades
		if(!PenetratesTarget && (m_BehaviorFlags & WEAPON_BEHAVIOR_CLUSTER) &&
		   m_WeaponLevel < WEAPON_CLUSTER_FRAGMENT_LEVEL)
		{
			const float LevelCharge = m_WeaponLevel / float(max(1, m_WeaponMaxLevel));
			CWeaponSpec Fragment = m_Source.m_Weapon;
			Fragment.m_Level = WEAPON_CLUSTER_FRAGMENT_LEVEL;
			for(int i = 0; i < 1 + LevelCharge * 2.0f; i++)
			{
				GameServer()->CreateProjectile(
					CAttackSource::PlayerWeapon(m_Owner, Fragment), 0, PrevPos, normalize(RandomDir()), PrevPos);
			}
		}

		if(!PenetratesTarget)
		{
			if(m_LifeSpan < 0)
				GameServer()->CreateExplosion(PrevPos, m_Source, m_ExplosionDamageScale);
			else if(m_Explosive)
				GameServer()->CreateExplosion(CurPos, m_Source, m_ExplosionDamageScale);

			GameServer()->m_World.DestroyEntity(this);
		}
	}

	// fluid kills the projectile
	if(GameServer()->Collision()->IsInFluid(PrevPos.x, PrevPos.y))
		GameServer()->m_World.DestroyEntity(this);
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

void CProjectile::FillInfo(CNetObj_Projectile *pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x * PROJECTILE_DIRECTION_NETWORK_SCALE);
	pProj->m_VelY = (int)(m_Direction.y * PROJECTILE_DIRECTION_NETWORK_SCALE);
	pProj->m_Vel2X = (int)(m_Vel2.x * PROJECTILE_VELOCITY_NETWORK_SCALE);
	pProj->m_Vel2Y = (int)(m_Vel2.y * PROJECTILE_VELOCITY_NETWORK_SCALE);
	pProj->m_StartTick = m_StartTick;
	pProj->m_SourceKind = static_cast<int>(m_Source.m_Kind);
	pProj->m_SourceType = m_Source.m_Type;
	pProj->m_WeaponDefinitionId = static_cast<int>(m_Source.m_Weapon.m_DefinitionId);
	pProj->m_WeaponLevel = m_Source.m_Weapon.m_Level;
}

void CProjectile::Snap(int SnappingClient)
{
	float Ct = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();

	if(NetworkClipped(SnappingClient, GetPos(Ct)))
		return;

	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(
		Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_ID, sizeof(CNetObj_Projectile)));
	if(pProj)
		FillInfo(pProj);
}
