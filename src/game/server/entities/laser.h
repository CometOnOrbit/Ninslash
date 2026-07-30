

#ifndef GAME_SERVER_ENTITIES_LASER_H
#define GAME_SERVER_ENTITIES_LASER_H

#include <game/server/entity.h>
#include <game/weapon_catalog.h>

class CLaser : public CEntity
{
  public:
	// CLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, float StartEnergy, int Owner, int Damage, int
	// PowerLevel, class CBuilding *OwnerBuilding = 0);
	CLaser(CGameWorld *pGameWorld,
		   vec2 Pos,
		   vec2 Direction,
		   float StartEnergy,
		   const CAttackSource &Source,
		   int Damage,
		   int Charge,
		   int Penetration = 0);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);

	class CBuilding *m_OwnerBuilding;

  protected:
	bool HitScythe(vec2 From, vec2 To);
	bool HitCharacter(vec2 From, vec2 To);
	bool HitMonster(vec2 From, vec2 To);
	bool HitBuilding(vec2 From, vec2 To);
	bool HitPenetratingTargets(vec2 From, vec2 To);
	void DamageBuilding(class CBuilding *pBuilding, vec2 At);
	void DoBounce();

  private:
	vec2 m_From;
	vec2 m_Dir;
	float m_Energy;
	int m_Bounces;
	int m_EvalTick;
	int m_Owner;
	int m_Damage;
	int m_Charge;
	int m_RemainingPenetrations;
	bool m_InfinitePenetration;
	CAttackSource m_Source;

	int m_IgnoreScythe;
};

#endif
