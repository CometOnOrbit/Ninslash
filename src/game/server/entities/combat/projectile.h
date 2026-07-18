#ifndef GAME_SERVER_ENTITIES_COMBAT_PROJECTILE_H
#define GAME_SERVER_ENTITIES_COMBAT_PROJECTILE_H

#include <game/shared/weapons/weapon_catalog.h>

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
	CProjectile(CGameWorld *pGameWorld, const CAttackSource &Source, vec2 Pos, vec2 Dir, vec2 Vel, int Span,
		int Damage, float Force, int SoundImpact);

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
	int m_SoundImpact;
	float m_Force;
	int m_StartTick;
	bool m_Explosive;
	int m_Bounces;
	
	//
	int m_Part1;
	int m_Part2;
	float m_Speed;
	float m_Curvature;
	int m_ProjectilePosType;
	float m_ProjectileSize;
	int m_StaticType;
	int m_WeaponLevel;
	int m_WeaponMaxLevel;
	
	bool m_SkipCollision;
	
	void UpdateStats();
	
	int m_ElectroTimer;
};

#endif
