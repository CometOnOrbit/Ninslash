#ifndef GAME_WEAPON_CATALOG_H
#define GAME_WEAPON_CATALOG_H

#include <cstdint>

#include <base/vmath.h>

#include <game/weapons.h>

enum class WeaponDefinitionId : uint16_t
{
	Invalid = 0,
	StaticFirst = 1,
	StaticLast = StaticFirst + NUM_STATIC_WEAPONS - 1,
	ModularFirst = 100,
	ModularLast = ModularFirst + 6 * 9 - 1,
};

struct CWeaponSpec
{
	WeaponDefinitionId m_DefinitionId = WeaponDefinitionId::Invalid;
	uint8_t m_Level = 0;

	constexpr bool IsValid() const { return m_DefinitionId != WeaponDefinitionId::Invalid; }
	constexpr bool operator==(const CWeaponSpec &Other) const
	{
		return m_DefinitionId == Other.m_DefinitionId && m_Level == Other.m_Level;
	}
};

enum class EWeaponDefinitionKind : uint8_t
{
	Static,
	Modular,
};

struct CWeaponDefinition
{
	WeaponDefinitionId m_Id;
	EWeaponDefinitionKind m_Kind;
	uint8_t m_StaticType;
	uint8_t m_Part1;
	uint8_t m_Part2;
	uint8_t m_MaxLevel;
};

struct CWeaponCombatProfile
{
	int m_FiringType;
	float m_FireRate;
	bool m_FullAuto;
	int m_MaxAmmo;
	bool m_UsesAmmo;
	int m_ShotSpread;
	float m_ProjectileSpread;
	float m_ProjectileSpeed;
	float m_ProjectileCurvature;
	float m_ProjectileLife;
	float m_ProjectileDamage;
	float m_ProjectileKnockback;
	float m_ExplosionSize;
	float m_ExplosionDamage;
	float m_MeleeHitRadius;
	float m_WeaponKnockback;
	int m_BurstCount;
	float m_BurstReload;
	int m_AiAttackRange;
	bool m_ValidForTurret;
};

struct CWeaponVisualProfile
{
	int m_RenderType;
	ivec2 m_VisualSize;
	ivec2 m_VisualSize2;
	vec2 m_RenderOffset;
	vec2 m_MuzzleOffset;
	vec2 m_ProjectileOffset;
	vec2 m_HandOffset;
	vec2 m_ColorSwap;
	float m_RenderRecoil;
	float m_ProjectileSize;
	float m_ProjectileSprite;
	int m_ProjectileTraceType;
	float m_TraceThreshold;
	int m_ExplosionSprite;
	int m_ExplosionSound;
	int m_FireSound;
	int m_FireSound2;
	int m_MuzzleType;
	int m_MuzzleAmount;
};

struct CResolvedWeaponProfile
{
	CWeaponDefinition m_Definition;
	CWeaponSpec m_Spec;
	CWeaponCombatProfile m_Combat;
	CWeaponVisualProfile m_Visual;
};

class CWeaponCatalog
{
public:
	static bool TryGetDefinition(WeaponDefinitionId Id, CWeaponDefinition *pDefinition);
	static bool IsValidSpec(const CWeaponSpec &Spec);
	static bool TryResolve(const CWeaponSpec &Spec, CResolvedWeaponProfile *pProfile);
	static bool Validate();

	// Temporary migration boundary. Bit-packed values must not escape through new APIs.
	static bool TryFromLegacy(int LegacyWeapon, CWeaponSpec *pSpec);
	static int ToLegacy(const CWeaponSpec &Spec);
};

enum class EAttackSourceKind : uint8_t
{
	PlayerWeapon,
	Droid,
	Building,
	World,
	DeathEffect,
};

struct CAttackSource
{
	EAttackSourceKind m_Kind = EAttackSourceKind::World;
	int m_Owner = -1;
	int m_Type = 0;
	CWeaponSpec m_Weapon;

	static CAttackSource PlayerWeapon(int Owner, CWeaponSpec Weapon);
	static CAttackSource Droid(int Owner, int DroidType, bool OnDeath = false);
	static CAttackSource Building(int Owner, int BuildingType);
	static CAttackSource World();
};

#endif
