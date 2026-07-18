#include <game/server/gamecontext.h>
#include <game/server/pve_director.h>
#include <game/weapons.h>
#include <base/system.h>
#include "laser.h"
#include "electrowall.h"
#include "weapon.h"
#include "weapon_behavior.h"

CWeapon::CWeapon(CGameWorld *pGameWorld, const CWeaponSpec &Spec)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_WEAPON)
{
	m_ProximityRadius = ms_PhysSize;
	dbg_assert(CWeaponCatalog::IsValidSpec(Spec), "invalid player weapon spec");
	m_WeaponSpec = Spec;
	m_PowerLevel = Spec.m_Level;
	
	CWeaponDefinition Definition;
	CWeaponCatalog::TryGetDefinition(Spec.m_DefinitionId, &Definition);
	if (Definition.m_Kind == EWeaponDefinitionKind::Static && Definition.m_StaticType == SW_UPGRADE && m_PowerLevel < WEAPON_UPGRADE_SUPERCHARGE_LEVEL)
	{
		m_PowerLevel = 0;
		m_WeaponSpec.m_Level = 0;
	}
	
	Reset();
	
	GameWorld()->InsertEntity(this);
}

void CWeapon::Reset()
{
	m_SkipPickTick = 0;
	m_InfiniteAmmo = false;
	CWeaponDefinition Definition;
	CWeaponCatalog::TryGetDefinition(m_WeaponSpec.m_DefinitionId, &Definition);
	m_MaxLevel = Definition.m_MaxLevel;
	m_Disabled = false;
	m_IsTurret = false;
	m_ChargeSoundTimer = 0;
	m_TriggerTick = 0;
	m_ReloadTimer = 0;
	m_BurstReloadTimer = 0;
	m_RogueliteCooldownCarry = 0.0f;
	m_Pos = vec2(0, 0);
	m_Direction = vec2(0, 0);
	m_Owner = TEAM_NEUTRAL;
	m_ChargeLocked = false;
	m_Charge = 0;
	m_LastNoAmmoSound = -1;
	m_Vel = vec2(0, 0);
	m_Released = false;
	m_DestructionTick = 0;
	m_AttackTick = 0;
	m_AngleForce = 0.0f;
	m_Angle = 0.0f;
	m_TriggerCount = 0;
	m_BurstCount = 0;
	m_BurstMax = 0;
	m_Stuck = false;
	m_BombCounter = 0;
	m_BombDisarmCounter = 0;
	m_BombResetTick = 0;
	
	UpdateStats();
	m_Ammo = m_MaxAmmo;
}
	
void CWeapon::SetCharge(int Charge)
{
	if (!m_ChargeLocked)
		m_Charge = Charge;
}

void CWeapon::SetOwner(int CID)
{
	m_Owner = CID;
	m_Disabled = false;
}

void CWeapon::OnOwnerDeath(bool IsActive)
{
	m_InfiniteAmmo = false;
	m_Owner = TEAM_NEUTRAL;
	Deactivate();
	
	if (IsActive)
	{
		if (Drop())
		{
			m_TriggerTick = 0;
			m_TriggerCount = 0;
		}
	}
	else
	{
		GameServer()->m_World.DestroyEntity(this);
	}
}

void CWeapon::Deactivate()
{
	if (m_WeaponProfile.m_Combat.m_FiringType == WFT_CHARGE)
		m_Charge = 0;
}

void CWeapon::Clear()
{
	GameServer()->m_World.DestroyEntity(this);
}	

void CWeapon::SetPos(vec2 Pos, vec2 Vel, vec2 Direction, float Radius)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_Direction = Direction;
	m_ProximityRadius = Radius;
	m_Disabled = false;
}

void CWeapon::OnPlayerPick()
{
	m_AttackTick = 0;
}


void CWeapon::SetTurret(bool TurretBit)
{
	m_IsTurret = TurretBit;
}

void CWeapon::UpdateStats()
{
	m_CanFire = true;
	m_WeaponSpec.m_Level = m_PowerLevel;
	dbg_assert(CWeaponCatalog::TryResolve(m_WeaponSpec, &m_WeaponProfile), "failed to resolve player weapon");
	m_MaxLevel = m_WeaponProfile.m_Definition.m_MaxLevel;
	const CWeaponCombatProfile &Combat = m_WeaponProfile.m_Combat;
	m_FireRate = Combat.m_FireRate;
	m_KnockBack = Combat.m_WeaponKnockback;
	m_FireSound = m_WeaponProfile.m_Visual.m_FireSound;
	m_FireSound2 = m_WeaponProfile.m_Visual.m_FireSound2;
	m_FullAuto = Combat.m_FullAuto;
	m_MaxAmmo = Combat.m_MaxAmmo;
	m_UseAmmo = Combat.m_UsesAmmo;
	m_BurstMax = Combat.m_BurstCount;
}


void CWeapon::SurvivalReset()
{
	if (m_Released || m_DestructionTick)
	{
		GameServer()->m_World.DestroyEntity(this);
	}
}


bool CWeapon::Activate()
{
	return CWeaponBehaviorExecutor::Activate(*this);
}



bool CWeapon::Fire(float *pKnockback)
{
	return CWeaponBehaviorExecutor::Fire(*this, pKnockback);
}


int CWeapon::Reflect()
{
	if (m_TriggerTick && m_TriggerTick > Server()->Tick() && IsModular() && Part1() == PART1_SPIN)
		return 80;
	
	return 0;
}

int CWeapon::GetCharge()
{
	return m_Charge;
}


bool CWeapon::Charge()
{
	return CWeaponBehaviorExecutor::Charge(*this);
}


bool CWeapon::ReleaseCharge(float *pKnockback)
{
	return CWeaponBehaviorExecutor::ReleaseCharge(*this, pKnockback);
}

bool CWeapon::Throw()
{
	return CWeaponBehaviorExecutor::Throw(*this);
}



void CWeapon::CreateProjectile()
{
	CWeaponBehaviorExecutor::CreateProjectile(*this);
}


void CWeapon::ReduceAmmo(int Amount)
{
	m_Ammo = max(0, m_Ammo - Amount);
}


void CWeapon::IncreaseAmmo(int Amount)
{
	m_Ammo = min(m_MaxAmmo, m_Ammo + Amount);
}
	
	
bool CWeapon::CanSwitch()
{
	if (m_Charge > 0 || m_ReloadTimer > 0)
		return false;
		
	return true;
}

bool CWeapon::Drop()
{
	if (m_Charge > 0 && m_WeaponProfile.m_Combat.m_FiringType == WFT_THROW)
		return false;
	
	m_Owner = TEAM_NEUTRAL;
	
	m_TriggerTick = 0;
	m_TriggerCount = 0;
			
	return true;
}

bool CWeapon::AddClip()
{
	if (!m_UseAmmo || !m_MaxAmmo)
		return false;
	
	if (m_Ammo < m_MaxAmmo)
	{
		m_Ammo = min(m_Ammo+m_MaxAmmo/3, m_MaxAmmo);
		if(m_Ammo == m_MaxAmmo && GameServer()->m_pPveDirector && m_Owner >= 0)
			GameServer()->m_pPveDirector->OnFullReload(m_Owner);
		return true;
	}
	
	return false;
}

bool CWeapon::Overcharge()
{
	if (StaticType() == SW_UPGRADE)
	{
		if (m_PowerLevel < WEAPON_UPGRADE_SUPERCHARGE_LEVEL)
		{
			m_PowerLevel = WEAPON_UPGRADE_SUPERCHARGE_LEVEL;
			UpdateStats();
			return true;
		}
	}
	else if (m_MaxLevel > 0 && m_PowerLevel <= m_MaxLevel)
	{
		if (m_MaxLevel >= WEAPON_HIGH_TIER_MIN_MAX_LEVEL && m_PowerLevel == m_MaxLevel)
			m_PowerLevel++;
		
		m_PowerLevel++;
		UpdateStats();
		return true;
	}
	
	return false;
}

bool CWeapon::Supercharge()
{
	if (StaticType() == SW_UPGRADE)
		return false;
	
	if (m_MaxLevel > 0 && m_PowerLevel > m_MaxLevel)
	{
		if (m_MaxLevel >= WEAPON_HIGH_TIER_MIN_MAX_LEVEL)
		{
			if (m_PowerLevel <= m_MaxLevel + WEAPON_HIGH_TIER_SUPERCHARGE_BONUS)
			{
				m_PowerLevel += WEAPON_HIGH_TIER_SUPERCHARGE_STEP;
				UpdateStats();
				return true;
			}
		}
		else
		{
			if (m_PowerLevel <= m_MaxLevel + WEAPON_LOW_TIER_SUPERCHARGE_BONUS)
			{
				m_PowerLevel += WEAPON_LOW_TIER_SUPERCHARGE_STEP;
				UpdateStats();
				return true;
			}
		}
		
	}
	
	return false;
}


bool CWeapon::Upgrade()
{
	if (StaticType() == SW_UPGRADE)
		return false;
	
	
	if (m_PowerLevel < m_MaxLevel)
	{
		m_PowerLevel++;
		UpdateStats();
		return true;
	}
	
	return false;
}

void CWeapon::Tick()
{
	if (m_Disabled)
		return;
	
	// pick me up!
	if ((m_Stuck && m_Owner < 0) || (StaticType() == SW_BALL && m_Released))
	{
		CCharacter *pChr = GameServer()->m_World.ClosestCharacter(m_Pos, 18.0f, 0);
		if(pChr && pChr->IsAlive() && (pChr->GetPlayer()->GetCID() != m_Owner || m_SkipPickTick < Server()->Tick()))
		{
			if (pChr->PickWeapon(this))
			{
				Reset();
				// pickup sound
				
			}
		}
	}
	
	 // shuriken flying hit
	if (!m_Stuck && StaticType() == SW_SHURIKEN)
	{
		if (length(m_Vel) > 20.0f)
			CreateProjectile();
	}
	
	if (m_BurstCount > 0)
		m_FullAuto = true;
	
	if(m_ReloadTimer > 0)
		m_ReloadTimer--;
	if(m_ReloadTimer > 0 && GameServer()->m_pPveDirector)
	{
		const float Reduction = GameServer()->m_pPveDirector->CooldownReduction(m_Owner, m_WeaponSpec);
		if(Reduction > 0.0f)
		{
			m_RogueliteCooldownCarry += Reduction / (1.0f - Reduction);
			const int ExtraTicks = (int)m_RogueliteCooldownCarry;
			if(ExtraTicks > 0)
			{
				m_ReloadTimer = max(0, m_ReloadTimer - ExtraTicks);
				m_RogueliteCooldownCarry -= ExtraTicks;
			}
		}
	}
	
	if(m_BurstReloadTimer > 0)
	{
		if (--m_BurstReloadTimer <= 0)
			m_BurstCount = 0;
	}
	
	if (m_Released)
		Move();
	
	// bomb
	if (m_BombResetTick && m_BombResetTick < Server()->Tick())
	{
		m_BombResetTick = 0;
		m_BombCounter = 0;
	}
	
	
	if (m_WeaponProfile.m_Combat.m_FiringType == WFT_HOLD)
	{
		if (m_TriggerTick && m_TriggerTick > Server()->Tick())
			Trigger();
	}
	else if (m_TriggerTick && m_TriggerTick <= Server()->Tick())
		Trigger();
	
	
	// bomb
	if (StaticType() == SW_BOMB)
	{
		// bomb sound
		if (m_DestructionTick && m_ChargeSoundTimer-- <= 0)
		{
			float d = min(26.0f, (Server()->Tick() - m_AttackTick) * 0.0275f);
			m_ChargeSoundTimer = 30 - d;
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_CHARGE1_1+min(8, int(d/3)));
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_CHARGE1_1+min(8, int(d/3)));
		}
		
		// disarm
		if (m_DestructionTick)
		{
			GameServer()->m_pController->m_BombPos = m_Pos;
			GameServer()->m_pController->m_BombStatus = BOMB_ARMED;
			
			// disarm success
			if (m_BombDisarmCounter >= 18)
			{
				if (m_ChargeSoundTimer < 99)
				{
					m_ChargeSoundTimer = 999;
					GameServer()->m_pController->DisarmBomb();
				}
				m_DestructionTick = Server()->Tick() + 20.0f * Server()->TickSpeed();
				m_AttackTick = Server()->Tick();
				GameServer()->m_pController->m_BombStatus = BOMB_DISARMED;
			}
			else
			{
				bool Found = false;
				
				CCharacter *pChr = GameServer()->m_World.ClosestCharacter(m_Pos, 32.0f, 0);
				if (pChr && pChr->IsAlive() && pChr->GetPlayer()->GetTeam() == TEAM_BLUE)
				{
					Found = true;
					
					if (m_BombCounter-- < 0)
					{
						if (m_Owner >= 0 && (m_BombDisarmCounter == 0 || m_BombDisarmCounter%2 == 0))
							GameServer()->SendBroadcastFormat(pChr->GetPlayer()->GetCID(), false, "Disarming bomb... %d", 8-m_BombDisarmCounter/2);
						
						m_BombCounter = 10+frandom()*10;
						m_BombDisarmCounter++;
						GameServer()->CreateSound(m_Pos, SOUND_BOMB_BEEP);
					}
				}
				
				if (!Found && m_BombDisarmCounter > 0)
				{
					m_BombCounter = 0;
					m_BombDisarmCounter = 0;
					GameServer()->CreateSound(m_Pos, SOUND_BOMB_DENIED);
				}
			}
		}
		else
		{
			if (m_Released)
			{
				GameServer()->m_pController->m_BombStatus = BOMB_IDLE;
				GameServer()->m_pController->m_BombPos = m_Pos;
			}
		}
	}
	
	// charge sound
	else if (m_DestructionTick && m_ChargeSoundTimer-- <= 0)
	{
		float d = min(14.0f, (Server()->Tick() - m_AttackTick) * 0.1f);
		m_ChargeSoundTimer = 18 - d;
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_CHARGE1_1+min(8, int(d)));
	}
	
	
	if (m_DestructionTick && m_DestructionTick <= Server()->Tick())
		SelfDestruct();
}

void CWeapon::Trigger()
{
	CWeaponBehaviorExecutor::Trigger(*this);
}


void CWeapon::SelfDestruct()
{
	CWeaponBehaviorExecutor::SelfDestruct(*this);
}


void CWeapon::Move()
{
	if (m_Stuck)
		return;
	
	m_Vel.y += 0.5f;
	
	//m_Vel.y = min(m_Vel.y, 25.0f);
	
	bool Down = m_Vel.y < 0.0f;
	
	bool Grounded = false;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x+12, m_Pos.y+12+5, false, Down))
		Grounded = true;
	if(GameServer()->Collision()->CheckPoint(m_Pos.x-12, m_Pos.y+12+5, false, Down))
		Grounded = true;
		
	const int OnForceTile = GameServer()->Collision()->IsForceTile(m_Pos.x-12, m_Pos.x+12, m_Pos.y+12+5);
		
	if (Grounded)
	{
		m_Vel.x = (m_Vel.x + OnForceTile*0.7f) * 0.925f;
		//m_Vel.x *= 0.8f;
		m_AngleForce += (m_Vel.x - m_AngleForce) / 2.0f;
	}
	else
	{
		m_Vel.x *= 0.99f;
		m_Vel.y *= 0.99f;
	}
	
	vec2 OldVel = m_Vel;
	
	
	if (StaticType() == SW_BALL)
		GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(18.0f, 18.0f), 0.9f);
	else
		GameServer()->Collision()->MoveBox(&m_Pos, &m_Vel, vec2(18.0f, 18.0f), 0.5f);
	
	
	if ((((OldVel.x < 0 && m_Vel.x > 0) || (OldVel.x > 0 && m_Vel.x < 0)) && abs(m_Vel.x) > 3.0f) ||
		(((OldVel.y < 0 && m_Vel.y > 0) || (OldVel.y > 0 && m_Vel.y < 0)) && abs(m_Vel.y) > 3.0f))
		GameServer()->CreateSound(m_Pos, SOUND_SFX_BOUNCE1);
		
	if (StaticType() == SW_SHURIKEN)
	{
		if ((((OldVel.x < 0 && m_Vel.x > 0) || (OldVel.x > 0 && m_Vel.x < 0)) && abs(m_Vel.x) > 5.0f) ||
			(((OldVel.y < 0 && m_Vel.y > 0) || (OldVel.y > 0 && m_Vel.y < 0)) && abs(m_Vel.y) > 5.0f))
		{
			m_Pos += OldVel*0.3f;
			GameServer()->CreateExplosion(m_Pos, CAttackSource::PlayerWeapon(m_Owner, m_WeaponSpec));
			GameServer()->CreateSound(m_Pos, SOUND_SFX_BOUNCE1);
			m_Stuck = true;
			m_Owner = -1;
			
			// todo: correct sound & effect
		}
		
		if (abs(m_Vel.x) < 0.1f && abs(m_Vel.y) < 1.0f && GameServer()->Collision()->IsTileSolid(m_Pos.x, m_Pos.y+10.0f, Down))
		{
			m_Stuck = true;
			m_Owner = -1;
		}
	}
	
	if (StaticType() != SW_SHURIKEN)
	{
		m_AngleForce *= 0.98f;
	}
	
	m_Angle += clamp(m_AngleForce*0.04f, -0.6f, 0.6f);
}
	
void CWeapon::TickPaused()
{
	
}

void CWeapon::Snap(int SnappingClient)
{
	if (m_Disabled)
		return;
	
	if(!m_Released || NetworkClipped(SnappingClient))
		return;
	
	CNetObj_Weapon *pW = static_cast<CNetObj_Weapon *>(Server()->SnapNewItem(NETOBJTYPE_WEAPON, m_ID, sizeof(CNetObj_Weapon)));
	if(!pW)
		return;

	pW->m_X = (int)m_Pos.x;
	pW->m_Y = (int)m_Pos.y;
	pW->m_Angle = (int)(m_Angle*256.0f);
	pW->m_WeaponDefinitionId = static_cast<int>(m_WeaponSpec.m_DefinitionId);
	pW->m_WeaponLevel = m_WeaponSpec.m_Level;
	pW->m_AttackTick = m_AttackTick;
}
