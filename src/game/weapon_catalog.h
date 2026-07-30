#ifndef GAME_WEAPON_CATALOG_H
#define GAME_WEAPON_CATALOG_H

#include <cstdint>
#include <limits>

#include <base/vmath.h>

#include <game/weapons.h>

constexpr int WEAPON_MODULAR_PART1_COUNT = NUM_PART1;
constexpr int WEAPON_MODULAR_PART2_COUNT = NUM_PART2;
constexpr int WEAPON_RANGED_PART1_COUNT = PART1_BASE6 - PART1_BASE1 + 1;
constexpr int WEAPON_RANGED_PART2_COUNT = PART2_RAIL - PART2_BARREL1 + 1;
constexpr int WEAPON_MELEE_PART1_COUNT = PART1_SPIN - PART1_MELEE + 1;
constexpr int WEAPON_MELEE_PART2_COUNT = PART2_MELEE6 - PART2_MELEE1 + 1;
constexpr int WEAPON_SPEC_MAX_LEVEL = 15;
constexpr int WEAPON_SPEC_LEVEL_COUNT = WEAPON_SPEC_MAX_LEVEL + 1;
constexpr int WEAPON_DEFINITION_COUNT = NUM_STATIC_WEAPONS + WEAPON_RANGED_PART1_COUNT * WEAPON_RANGED_PART2_COUNT +
										WEAPON_MELEE_PART1_COUNT * WEAPON_MELEE_PART2_COUNT;
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
constexpr int WEAPON_INFINITE_PENETRATION = -1;
constexpr float WEAPON_INFINITE_PROJECTILE_LIFETIME = std::numeric_limits<float>::infinity();

enum class WeaponDefinitionId : uint16_t
{
	Invalid = 0,
	StaticFirst = 1,
	StaticLast = StaticFirst + NUM_STATIC_WEAPONS - 1,
	// Official numeric IDs are assigned by immutable Lua declaration order.
	ModularFirst = StaticLast + 1,
	ModularLast = ModularFirst + WEAPON_RANGED_PART1_COUNT * WEAPON_RANGED_PART2_COUNT +
				  WEAPON_MELEE_PART1_COUNT * WEAPON_MELEE_PART2_COUNT - 1,
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

enum EWeaponBehaviorFlag : uint32_t
{
	WEAPON_BEHAVIOR_NONE = 0,
	WEAPON_BEHAVIOR_TOOL = 1u << 0,
	WEAPON_BEHAVIOR_CLAW = 1u << 1,
	WEAPON_BEHAVIOR_CHAINSAW = 1u << 2,
	WEAPON_BEHAVIOR_FLAMER = 1u << 3,
	WEAPON_BEHAVIOR_GRENADE_TIMED = 1u << 4,
	WEAPON_BEHAVIOR_GRENADE_LASER = 1u << 5,
	WEAPON_BEHAVIOR_GRENADE_DROP = 1u << 6,
	WEAPON_BEHAVIOR_UPGRADE = 1u << 7,
	WEAPON_BEHAVIOR_CONTROLLER_ACTIVATE = 1u << 8,
	WEAPON_BEHAVIOR_ELECTROWALL = 1u << 9,
	WEAPON_BEHAVIOR_AREA_SHIELD = 1u << 10,
	WEAPON_BEHAVIOR_SHURIKEN = 1u << 11,
	WEAPON_BEHAVIOR_BOMB = 1u << 12,
	WEAPON_BEHAVIOR_BALL = 1u << 13,
	WEAPON_BEHAVIOR_CLUSTER = 1u << 14,
	WEAPON_BEHAVIOR_BAZOOKA = 1u << 15,
	WEAPON_BEHAVIOR_ELECTRIC_GUN = 1u << 16,
	WEAPON_BEHAVIOR_COMPACT_GUN_HANDS = 1u << 17,
	WEAPON_BEHAVIOR_CHARGED_BURST = 1u << 18,
	WEAPON_BEHAVIOR_SPIN_REFLECT = 1u << 19,
	WEAPON_BEHAVIOR_IMPACT_SPARK = 1u << 20,
	WEAPON_BEHAVIOR_EXPLOSION_SMOKE = 1u << 21,
	WEAPON_BEHAVIOR_GREEN_EXPLOSION = 1u << 22,
	WEAPON_BEHAVIOR_ACTIVATE_INVIS = 1u << 23,
	WEAPON_BEHAVIOR_ACTIVATE_SHIELD = 1u << 24,
	WEAPON_BEHAVIOR_ACTIVATE_RESPAWNER = 1u << 25,
	WEAPON_BEHAVIOR_MELEE = 1u << 26,
	WEAPON_BEHAVIOR_CHARGED_BLADE = 1u << 27,
	WEAPON_BEHAVIOR_CAPACITOR = 1u << 28,
	WEAPON_BEHAVIOR_RAIL = 1u << 29,
	WEAPON_BEHAVIOR_HAMMER_IMPACT = 1u << 30,
};

enum EWeaponImpactEffect
{
	WEAPON_IMPACT_EFFECT_NONE = 0,
	WEAPON_IMPACT_EFFECT_BALLISTIC,
	WEAPON_IMPACT_EFFECT_LAUNCHER,
	WEAPON_IMPACT_EFFECT_GREEN,
	WEAPON_IMPACT_EFFECT_ELECTRIC,
	WEAPON_IMPACT_EFFECT_SPRITE,
	WEAPON_IMPACT_EFFECT_ELECTRIC_AREA,
	WEAPON_IMPACT_EFFECT_SPARKS,
	WEAPON_IMPACT_EFFECT_SPRITE_ELECTRIC,
	NUM_WEAPON_IMPACT_EFFECTS,
};

struct CWeaponDefinition
{
	WeaponDefinitionId m_Id;
	EWeaponDefinitionKind m_Kind;
	uint8_t m_StaticType;
	uint8_t m_Part1;
	uint8_t m_Part2;
	uint8_t m_MaxLevel;
	uint32_t m_BehaviorFlags;
	bool m_Custom;
	// Stable registry handles. Zero means the definition has no declared module.
	uint16_t m_FrameModule;
	uint16_t m_PartModule;
	uint64_t m_TagMask;
	char m_aStableId[384];
	char m_aPackageId[32];
	char m_aComposeId[64];
	char m_aHeldImage[256];
	char m_aProjectileImage[256];
	char m_aMuzzleImage[256];
	char m_aFireSound[256];
	char m_aFireSound2[256];
	char m_aExplosionSound[256];
	char m_aNameKey[128];
	char m_aDescriptionKey[256];
};

inline bool WeaponHasBehavior(const CWeaponDefinition &Definition, EWeaponBehaviorFlag Flag)
{
	return (Definition.m_BehaviorFlags & static_cast<uint32_t>(Flag)) != 0;
}

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
	float m_ChargeDamageMin;
	float m_ChargeDamageMax;
	float m_ChargeRangeMin;
	float m_ChargeRangeMax;
	float m_ChargePowerMin;
	float m_ChargePowerMax;
	int m_ProjectilePenetration;
	int m_ChargePenetrationMax;
	bool m_ChargeControlsLaser;
	bool m_DirectMelee;
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
	int m_ImpactEffect;
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
	// Loads official embedded Lua on first use. External definitions are appended
	// in deterministic package/declaration order and never run during gameplay.
	static bool Initialize(char *pError = 0, int ErrorSize = 0);
	static const char *OfficialContentHash();
	static bool LoadLuaDefinitions(const char *pPackageId,
								   int Capabilities,
								   const char *const *ppDependencies,
								   int DependencyCount,
								   const char *pSource,
								   int SourceSize,
								   char *pError,
								   int ErrorSize);
	static bool
	LoadLuaDefinitions(const char *pPackageId, const char *pSource, int SourceSize, char *pError, int ErrorSize);
	static bool FinalizeLuaDefinitions(char *pError, int ErrorSize);
	static void ResetCustomDefinitions();
	static void BeginCustomDefinitionReload();
	static void CommitCustomDefinitionReload();
	static void RollbackCustomDefinitionReload();
	static bool TryFromStableId(const char *pStableId, int Level, CWeaponSpec *pSpec);
	static const char *StableId(const CWeaponSpec &Spec);
	static bool IsCustom(const CWeaponSpec &Spec);
	static int DefinitionCount();
	static bool TryGetDefinitionByIndex(int Index, CWeaponDefinition *pDefinition);
	static bool TryGetDefinition(WeaponDefinitionId Id, CWeaponDefinition *pDefinition);
	static CWeaponSpec Static(StaticWeaponType Type, int Level = 0);
	static CWeaponSpec Modular(int Part1, int Part2, int Level = 0);
	static const char *Part1NameKey(int Part1);
	static const char *Part2NameKey(int Part2);
	static bool IsValidSpec(const CWeaponSpec &Spec);
	static bool TryResolve(const CWeaponSpec &Spec, CResolvedWeaponProfile *pProfile);
	static bool Validate();
	static bool TryFromProtocol(int DefinitionId, int Level, CWeaponSpec *pSpec);
	static bool
	TryResolveAttack(const CAttackSource &Source, CWeaponCombatProfile *pCombat, CWeaponVisualProfile *pVisual = 0);
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
