#include <game/server/core/gamecontext.h>

#include <game/server/entities/structures/electrowall.h>
#include <game/server/entities/combat/laser.h>
#include <game/server/entities/combat/weapon.h>
#include "weapon_behavior.h"

namespace
{
constexpr int WEAPON_CHARGE_MAX = 100;
constexpr int WEAPON_CHARGE_STEP = 1;
constexpr int THROWN_WEAPON_CHARGE_STEP = 3;
}

namespace
{
vec2 RandomDirection()
{
	return normalize(vec2(frandom() - 0.5f, frandom() - 0.5f));
}

}

bool CWeaponBehaviorExecutor::CreateElectroWall(CWeapon &Weapon)
{
	float BestDistance = 9000.0f;
	vec2 Point1;
	vec2 Point2;
	bool Found = false;
	for(int i = 0; i < 10; ++i)
	{
		const float Angle = float(i) / 10.0f * pi + Weapon.m_Angle;
		vec2 To1 = Weapon.m_Pos + vec2(cos(Angle), sin(Angle)) * 900.0f;
		vec2 To2 = Weapon.m_Pos - vec2(cos(Angle), sin(Angle)) * 900.0f;
		if(Weapon.GameServer()->Collision()->IntersectLine(Weapon.m_Pos, To1, nullptr, &To1) &&
			Weapon.GameServer()->Collision()->IntersectLine(Weapon.m_Pos, To2, nullptr, &To2) &&
			distance(To1, To2) < BestDistance)
		{
			BestDistance = distance(To1, To2);
			Point1 = To1;
			Point2 = To2;
			Found = true;
		}
	}
	if(Found)
		new CElectroWall(Weapon.GameWorld(), Point1, Point2);
	return Found;
}

bool CWeaponBehaviorExecutor::Fire(CWeapon &Weapon, float *pKnockback)
{
	Weapon.m_Disabled = false;
	if(!Weapon.m_CanFire || Weapon.m_ReloadTimer > 0)
		return false;
	const int FiringType = Weapon.m_WeaponProfile.m_Combat.m_FiringType;
	if(FiringType == WFT_NONE)
		return false;
	if(FiringType == WFT_ACTIVATE)
		return Weapon.Activate();
	if(Weapon.m_IsTurret)
		Weapon.m_Ammo = Weapon.m_MaxAmmo;
	if(Weapon.m_UseAmmo && Weapon.m_MaxAmmo > 0 && Weapon.m_Ammo <= 0)
	{
		if(Weapon.m_LastNoAmmoSound + Weapon.Server()->TickSpeed() <= Weapon.Server()->Tick())
		{
			Weapon.GameServer()->CreateSound(Weapon.m_Pos, SOUND_WEAPON_NOAMMO);
			Weapon.m_LastNoAmmoSound = Weapon.Server()->Tick();
		}
		return false;
	}

	Weapon.UpdateStats();
	if(Weapon.m_BurstMax > 1 && ++Weapon.m_BurstCount >= Weapon.m_BurstMax)
		Weapon.m_BurstCount = 0;
	if(Weapon.m_BurstCount > 0)
	{
		Weapon.m_ReloadTimer = Weapon.m_FireRate * Weapon.m_WeaponProfile.m_Combat.m_BurstReload * Weapon.Server()->TickSpeed() / 1000;
		Weapon.m_BurstReloadTimer = Weapon.m_FireRate * Weapon.Server()->TickSpeed() / 1000;
	}
	else
	{
		Weapon.m_ReloadTimer = Weapon.m_FireRate * Weapon.Server()->TickSpeed() / 1000;
		Weapon.m_BurstReloadTimer = 0;
	}
	if(Weapon.m_IsTurret)
		Weapon.m_ReloadTimer *= 1.5f;

	if(FiringType == WFT_PROJECTILE || FiringType == WFT_MELEE)
		Weapon.CreateProjectile();
	else if(FiringType == WFT_HOLD)
	{
		const bool Flamer = Weapon.m_WeaponProfile.m_Definition.m_Kind == EWeaponDefinitionKind::Static && Weapon.m_WeaponProfile.m_Definition.m_StaticType == SW_FLAMER;
		Weapon.m_TriggerTick = Weapon.Server()->Tick() + Weapon.m_FireRate * (Flamer ? 2 : 1) * Weapon.Server()->TickSpeed() / 1000;
		if(Weapon.m_FireSound >= 0)
			Weapon.GameServer()->CreateSound(Weapon.m_Pos, Weapon.m_FireSound);
	}
	else
		return false;

	if(pKnockback)
		*pKnockback = Weapon.m_KnockBack;
	if(Weapon.m_Ammo > 0 && !Weapon.m_InfiniteAmmo)
		Weapon.m_Ammo--;
	return true;
}

bool CWeaponBehaviorExecutor::Activate(CWeapon &Weapon)
{
	if(Weapon.m_DestructionTick)
		return false;
	const CWeaponDefinition &Definition = Weapon.m_WeaponProfile.m_Definition;
	if(Definition.m_Kind != EWeaponDefinitionKind::Static)
		return false;
	if(Definition.m_StaticType == SW_INVIS || Definition.m_StaticType == SW_SHIELD || Definition.m_StaticType == SW_RESPAWNER)
	{
		if(Weapon.GameServer()->m_pController->TriggerWeapon(&Weapon))
		{
			Weapon.m_DestructionTick = 1;
			return true;
		}
		return false;
	}
	if(Definition.m_StaticType != SW_BOMB)
		return false;
	if(Weapon.GameServer()->m_pController->InBombArea(Weapon.m_Pos))
	{
		const float ReloadTicks = Weapon.m_FireRate * Weapon.Server()->TickSpeed() / 1000;
		Weapon.m_ReloadTimer = ReloadTicks * (0.5f + frandom() * 0.5f);
		Weapon.GameServer()->CreateSound(Weapon.m_Pos, SOUND_BOMB_BEEP);
		Weapon.m_BombResetTick = Weapon.Server()->Tick() + Weapon.Server()->TickSpeed();
		if(Weapon.m_Owner >= 0 && (Weapon.m_BombCounter == 0 || Weapon.m_BombCounter % 3 == 0))
			Weapon.GameServer()->SendBroadcastFormat(Weapon.m_Owner, false, "Arming bomb... %d", Weapon.m_Owner, 4 - Weapon.m_BombCounter / 3);
		if(Weapon.m_BombCounter++ > 12 && Weapon.GameServer()->m_pController->TriggerWeapon(&Weapon))
		{
			Weapon.m_DestructionTick = Weapon.Server()->Tick() + 20.0f * Weapon.Server()->TickSpeed();
			Weapon.m_AttackTick = Weapon.Server()->Tick();
			Weapon.m_BombCounter = 0;
			Weapon.m_BombDisarmCounter = 0;
			Weapon.GameServer()->m_pController->TriggerBomb();
			return true;
		}
	}
	else
	{
		Weapon.m_ReloadTimer = Weapon.m_FireRate * Weapon.Server()->TickSpeed() / 1000 * 3.0f;
		Weapon.GameServer()->CreateSound(Weapon.m_Pos, SOUND_BOMB_DENIED);
		Weapon.m_BombCounter = 0;
		Weapon.m_BombResetTick = 0;
	}
	return false;
}

bool CWeaponBehaviorExecutor::Charge(CWeapon &Weapon)
{
	if(!Weapon.m_CanFire || Weapon.m_ReloadTimer > 0)
		return false;
	const CWeaponDefinition &Definition = Weapon.m_WeaponProfile.m_Definition;
	if(!Weapon.m_DestructionTick && Definition.m_Kind == EWeaponDefinitionKind::Static)
	{
		switch(Definition.m_StaticType)
		{
		case SW_BALL:
			Weapon.m_AttackTick = Weapon.Server()->Tick();
			break;
		case SW_GRENADE1:
			Weapon.m_AttackTick = Weapon.Server()->Tick();
			Weapon.m_DestructionTick = Weapon.Server()->Tick() + 2.0f * Weapon.Server()->TickSpeed();
			break;
		case SW_GRENADE2:
		case SW_GRENADE3:
		case SW_ELECTROWALL:
			Weapon.m_AttackTick = Weapon.Server()->Tick();
			Weapon.m_TriggerTick = Weapon.Server()->Tick() + 2.0f * Weapon.Server()->TickSpeed();
			Weapon.m_DestructionTick = Weapon.Server()->Tick() + 4.0f * Weapon.Server()->TickSpeed();
			break;
		case SW_AREASHIELD:
			Weapon.m_AttackTick = Weapon.Server()->Tick();
			Weapon.m_TriggerTick = Weapon.Server()->Tick() + 2.0f * Weapon.Server()->TickSpeed();
			Weapon.m_DestructionTick = Weapon.Server()->Tick() + 10.0f * Weapon.Server()->TickSpeed();
			break;
		default: break;
		}
	}
	const int ChargeStep = Weapon.m_WeaponProfile.m_Combat.m_FiringType == WFT_THROW ? THROWN_WEAPON_CHARGE_STEP : WEAPON_CHARGE_STEP;
	Weapon.m_Charge = min(Weapon.m_Charge + ChargeStep, WEAPON_CHARGE_MAX);
	return true;
}

bool CWeaponBehaviorExecutor::ReleaseCharge(CWeapon &Weapon, float *pKnockback)
{
	(void)pKnockback;
	if(!Weapon.m_CanFire)
	{
		Weapon.m_Charge = 0;
		return false;
	}
	if(Weapon.m_Charge <= 0)
	{
		Weapon.m_Charge = 0;
		return false;
	}
	if(Weapon.m_WeaponProfile.m_Combat.m_FiringType == WFT_CHARGE)
	{
		const CWeaponDefinition &Definition = Weapon.m_WeaponProfile.m_Definition;
		if(Definition.m_Kind == EWeaponDefinitionKind::Modular && Definition.m_Part1 == PART1_BASE1)
		{
			Weapon.m_TriggerCount = Weapon.m_WeaponSpec.m_Level;
			if(Weapon.m_TriggerCount)
				Weapon.m_TriggerTick = Weapon.Server()->Tick() + Weapon.m_FireRate * 0.5f * Weapon.Server()->TickSpeed() / 1000;
		}
		Weapon.CreateProjectile();
		Weapon.m_Charge = 0;
	}
	Weapon.m_ReloadTimer = Weapon.m_FireRate * Weapon.Server()->TickSpeed() / 1000;
	Weapon.m_SkipPickTick = Weapon.Server()->Tick() + Weapon.Server()->TickSpeed() * 0.1f;
	return true;
}

bool CWeaponBehaviorExecutor::Throw(CWeapon &Weapon)
{
	if(Weapon.m_Released)
		return false;

	vec2 Velocity;
	Velocity.x = sin(Weapon.m_Direction.x) * (Weapon.m_Direction.x > 0.0f ? 1 : -1) * Weapon.m_Vel.x;
	Velocity.y = sin(Weapon.m_Direction.y) * (Weapon.m_Direction.y > 0.0f ? 1 : -1) * Weapon.m_Vel.y;
	Weapon.m_Vel = Velocity + Weapon.m_Direction * Weapon.m_Charge * 0.24f * Weapon.m_WeaponProfile.m_Combat.m_ThrowForce;
	Weapon.m_Angle = 0.0f;
	Weapon.m_AngleForce = Weapon.m_Vel.x * 0.3f;

	if(Weapon.m_WeaponProfile.m_Definition.m_Kind == EWeaponDefinitionKind::Static && Weapon.m_WeaponProfile.m_Definition.m_StaticType == SW_SHURIKEN)
	{
		Weapon.m_AngleForce = Weapon.m_Charge * 0.1f * (Weapon.m_Direction.x < 0 ? -1.0f : 1.0f);
		Weapon.m_AttackTick = Weapon.Server()->Tick();
	}

	Weapon.m_Charge = 0;
	Weapon.m_Released = true;
	return true;
}

void CWeaponBehaviorExecutor::CreateProjectile(CWeapon &Weapon)
{
	const vec2 Offset = Weapon.m_WeaponProfile.m_Visual.m_ProjectileOffset;
	vec2 StartPos = Weapon.m_Pos + Weapon.m_Direction * Offset.x + vec2(0, Offset.y);
	const int FiringType = Weapon.m_WeaponProfile.m_Combat.m_FiringType;
	if(FiringType == WFT_PROJECTILE || FiringType == WFT_HOLD)
		Weapon.GameServer()->Collision()->IntersectLine(Weapon.m_Pos, StartPos, nullptr, &StartPos);

	Weapon.GameServer()->CreateProjectile(CAttackSource::PlayerWeapon(Weapon.m_Owner, Weapon.m_WeaponSpec), Weapon.m_Charge, StartPos, Weapon.m_Direction, Weapon.m_Pos + vec2(0, 20));
	if(Weapon.m_FireSound >= 0 && FiringType != WFT_HOLD)
	{
		Weapon.GameServer()->CreateSound(Weapon.m_Pos, Weapon.m_FireSound);
		if(Weapon.m_FireSound2 >= 0)
			Weapon.GameServer()->CreateSound(Weapon.m_Pos, Weapon.m_FireSound2);
	}
}

void CWeaponBehaviorExecutor::Trigger(CWeapon &Weapon)
{
	if(Weapon.m_WeaponProfile.m_Combat.m_FiringType == WFT_HOLD)
	{
		if(Weapon.m_WeaponProfile.m_Visual.m_RenderType != WRT_SPIN || Weapon.Server()->Tick() % 2 == 0)
			CreateProjectile(Weapon);
		return;
	}

	const CWeaponDefinition &Definition = Weapon.m_WeaponProfile.m_Definition;
	if(Definition.m_Kind != EWeaponDefinitionKind::Static)
		return;
	switch(Definition.m_StaticType)
	{
	case SW_GRENADE2:
		Weapon.m_TriggerTick = Weapon.Server()->Tick() + 0.05f * Weapon.Server()->TickSpeed();
		new CLaser(&Weapon.GameServer()->m_World, Weapon.m_Pos, RandomDirection(), 160.0f, CAttackSource::PlayerWeapon(Weapon.m_Owner, Weapon.m_WeaponSpec), 4, -2);
		new CLaser(&Weapon.GameServer()->m_World, Weapon.m_Pos, RandomDirection(), 160.0f, CAttackSource::PlayerWeapon(Weapon.m_Owner, Weapon.m_WeaponSpec), 4, -2);
		break;
	case SW_GRENADE3:
		Weapon.m_TriggerTick = Weapon.Server()->Tick() + 0.25f * Weapon.Server()->TickSpeed();
		Weapon.GameServer()->CreateEffect(FX_SMALLELECTRIC, Weapon.m_Pos);
		if(frandom() < 0.35f)
			Weapon.GameServer()->m_pController->DropPickup(Weapon.m_Pos + vec2(0, -6), POWERUP_HEALTH, vec2(frandom() - frandom(), frandom() - frandom() * 1.4f) * 14.0f, 0);
		else if(frandom() < 0.4f)
			Weapon.GameServer()->m_pController->DropPickup(Weapon.m_Pos + vec2(0, -6), POWERUP_AMMO, vec2(frandom() - frandom(), frandom() - frandom() * 1.4f) * 14.0f, 0);
		else if(frandom() < 0.6f)
			Weapon.GameServer()->m_pController->DropPickup(Weapon.m_Pos + vec2(0, -6), POWERUP_ARMOR, vec2(frandom() - frandom(), frandom() - frandom() * 1.4f) * 14.0f, 0);
		else
			Weapon.GameServer()->m_pController->DropPickup(Weapon.m_Pos + vec2(0, -6), POWERUP_KIT, vec2(frandom() - frandom(), frandom() - frandom() * 1.4f) * 14.0f, 0);
		break;
	case SW_ELECTROWALL:
		Weapon.m_TriggerTick = Weapon.Server()->Tick() + 0.1f * Weapon.Server()->TickSpeed();
		if(CreateElectroWall(Weapon))
		{
			Weapon.GameServer()->CreateEffect(FX_SMALLELECTRIC, Weapon.m_Pos);
			Weapon.m_DestructionTick = Weapon.Server()->Tick();
		}
		break;
	default:
		break;
	}
}

void CWeaponBehaviorExecutor::SelfDestruct(CWeapon &Weapon)
{
	if(!Weapon.m_Released)
		Weapon.GameServer()->m_pController->ReleaseWeapon(&Weapon);
	const CWeaponDefinition &Definition = Weapon.m_WeaponProfile.m_Definition;
	if(Definition.m_Kind == EWeaponDefinitionKind::Static)
	{
		switch(Definition.m_StaticType)
		{
		case SW_GRENADE1:
		case SW_GRENADE2:
		case SW_GRENADE3:
		case SW_BOMB:
			Weapon.GameServer()->CreateExplosion(Weapon.m_Pos, CAttackSource::PlayerWeapon(Weapon.m_Owner, Weapon.m_WeaponSpec));
			break;
		case SW_ELECTROWALL:
			Weapon.GameServer()->CreateEffect(FX_SMALLELECTRIC, Weapon.m_Pos);
			break;
		default:
			break;
		}
	}
	Weapon.GameServer()->m_World.DestroyEntity(&Weapon);
}
