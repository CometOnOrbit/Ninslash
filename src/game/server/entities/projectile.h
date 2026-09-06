#ifndef GAME_SERVER_ENTITIES_PROJECTILE_H
#define GAME_SERVER_ENTITIES_PROJECTILE_H

#include <game/weapons/weapon_catalog.h>

enum ProjectileExtraInfo
{
	PROJ_BIGEXPLOSION,
	PROJ_SUPEREXPLOSION,
	PROJ_SLEEPEFFECT
};

enum ExplosionType
{
	NO_EXPLOSION,
	EXPLOSION_EXPLOSION,
	EXPLOSION_GREEN,
	EXPLOSION_FLAME,
	EXPLOSION_ELECTRIC,
};

class CProjectile : public CEntity
{
  public:
	CProjectile(CGameWorld *pGameWorld,
				const CAttackSource &Source,
				vec2 Pos,
				vec2 Dir,
				vec2 Vel,
				int Span,
				int Damage,
				float Force,
				int SoundImpact,
				float ExplosionDamageScale = 1.0f,
				int Penetration = 0);

	vec2 GetPos(float Time);
	void FillInfo(CNetObj_Projectile *pProj);

	bool Bounce(vec2 Pos, int Collision);
	int BounceTick;

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	class CBuilding *m_OwnerBuilding;

  private:
	vec2 m_Direction;
	vec2 m_Vel2;
	int m_LifeSpan;
	int m_Owner;
	CAttackSource m_Source;
	int m_Damage;
	float m_ExplosionDamageScale;
	int m_SoundImpact;
	float m_Force;
	int m_StartTick;
	bool m_Explosive;
	int m_Bounces;

	//
	float m_Speed;
	float m_Curvature;
	int m_ProjectilePosType;
	float m_ProjectileSize;
	uint32_t m_BehaviorFlags;
	int m_WeaponLevel;
	int m_WeaponMaxLevel;
	int m_RemainingPenetrations;
	bool m_InfinitePenetration;
	class CCharacter *m_pPenetratedCharacter;
	class CDroid *m_pPenetratedDroid;

	bool m_SkipCollision;

	void UpdateStats();

	int m_ElectroTimer;
};

#endif
