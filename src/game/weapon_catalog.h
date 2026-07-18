#ifndef GAME_WEAPON_CATALOG_H
#define GAME_WEAPON_CATALOG_H

#include <cstdint>
#include <limits>

#include <base/vmath.h>

#include <game/weapons.h>

constexpr int WEAPON_MODULAR_PART1_COUNT = PART1_SPIN;
constexpr int WEAPON_MODULAR_PART2_COUNT = PART2_MELEE5;
constexpr int WEAPON_RANGED_PART1_COUNT = PART1_BASE5 - PART1_BASE1 + 1;
constexpr int WEAPON_RANGED_PART2_COUNT = PART2_CAPACITOR - PART2_BARREL1 + 1;
constexpr int WEAPON_MELEE_PART1_COUNT = PART1_SPIN - PART1_MELEE + 1;
constexpr int WEAPON_MELEE_PART2_COUNT = PART2_MELEE5 - PART2_MELEE1 + 1;
constexpr int WEAPON_DEFINITION_MODULAR_BASE = 100;
constexpr int WEAPON_SPEC_MAX_LEVEL = 15;
constexpr int WEAPON_SPEC_LEVEL_COUNT = WEAPON_SPEC_MAX_LEVEL + 1;
constexpr int WEAPON_DEFINITION_COUNT = NUM_STATIC_WEAPONS + WEAPON_RANGED_PART1_COUNT * WEAPON_RANGED_PART2_COUNT + WEAPON_MELEE_PART1_COUNT * WEAPON_MELEE_PART2_COUNT;
constexpr int WEAPON_PROFILE_COUNT = WEAPON_DEFINITION_COUNT * WEAPON_SPEC_LEVEL_COUNT;
constexpr int WEAPON_DROID_PROFILE_COUNT = NUM_DROIDTYPES;
constexpr int WEAPON_BUILDING_PROFILE_COUNT = BUILDING_PVE_SHIELD_NODE + 1;
constexpr int WEAPON_CLUSTER_FRAGMENT_LEVEL = WEAPON_SPEC_MAX_LEVEL;
constexpr int WEAPON_UPGRADE_SUPERCHARGE_LEVEL = 4;
constexpr int WEAPON_HIGH_TIER_MIN_MAX_LEVEL = 4;
constexpr int WEAPON_HIGH_TIER_SUPERCHARGE_BONUS = 4;
constexpr int WEAPON_HIGH_TIER_SUPERCHARGE_STEP = 2;
constexpr int WEAPON_LOW_TIER_SUPERCHARGE_BONUS = 2;
constexpr int WEAPON_LOW_TIER_SUPERCHARGE_STEP = 1;
constexpr float WEAPON_INFINITE_PROJECTILE_LIFETIME = std::numeric_limits<float>::infinity();

enum class WeaponDefinitionId : uint16_t
{
	Invalid = 0,
	StaticFirst = 1,
	StaticLast = StaticFirst + NUM_STATIC_WEAPONS - 1,
	ModularFirst = WEAPON_DEFINITION_MODULAR_BASE,
	ModularLast = ModularFirst + WEAPON_MODULAR_PART1_COUNT * WEAPON_MODULAR_PART2_COUNT - 1,
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
	float m_ThrowForce;
	float m_FlameAmount;
	float m_ElectroAmount;
	bool m_ExplosiveProjectile;
	bool m_LaserWeapon;
	int m_CursorWeapon;
	int m_Cost;
	bool m_Aimline;
	int m_ProjectilePosType;
	int m_LaserRange;
	int m_LaserCharge;
	int m_ProjectileBounces;
	bool m_AutoPick;
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
	float m_ScreenshakeAmount;
};

struct CResolvedWeaponProfile
{
	CWeaponDefinition m_Definition;
	CWeaponSpec m_Spec;
	CWeaponCombatProfile m_Combat;
	CWeaponVisualProfile m_Visual;
};

struct CAttackSource;

class CWeaponCatalog
{
public:
	static bool TryGetDefinition(WeaponDefinitionId Id, CWeaponDefinition *pDefinition);
	static CWeaponSpec Static(StaticWeaponType Type, int Level = 0);
	static CWeaponSpec Modular(int Part1, int Part2, int Level = 0);
	static bool IsValidSpec(const CWeaponSpec &Spec);
	static bool TryResolve(const CWeaponSpec &Spec, CResolvedWeaponProfile *pProfile);
	static float CapacitorDamageScale(int Charge);
	static float CapacitorRangeScale(int Charge);
	static bool Validate();
	static bool TryFromProtocol(int DefinitionId, int Level, CWeaponSpec *pSpec);
	static bool TryResolveAttack(const CAttackSource &Source, CWeaponCombatProfile *pCombat, CWeaponVisualProfile *pVisual = nullptr);
	static bool TryAttackSourceFromProtocol(int Kind, int Type, int DefinitionId, int Level, CAttackSource *pSource);
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
	bool m_HitFeedback = true;

	static CAttackSource PlayerWeapon(int Owner, CWeaponSpec Weapon);
	static CAttackSource Droid(int Owner, int DroidType, bool OnDeath = false);
	static CAttackSource Building(int Owner, int BuildingType);
	static CAttackSource World(int Type, int Owner = -1);
};

#endif
