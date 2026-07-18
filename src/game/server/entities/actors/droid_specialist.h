#ifndef GAME_SERVER_ENTITIES_ACTORS_DROID_SPECIALIST_H
#define GAME_SERVER_ENTITIES_ACTORS_DROID_SPECIALIST_H

#include <game/server/entities/actors/droid.h>

class CSpecialistDroid : public CDroid
{
public:
	CSpecialistDroid(CGameWorld *pWorld, vec2 Pos, int Type, int Health, bool Boss);
	void Reset() override;
	void Tick() override;
	void TickPaused() override;
	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;

protected:
	virtual void AbilityTick();
	virtual void MovementTick(class CCharacter *pTarget);
	virtual vec2 CollisionSize() const;
	virtual void OnHealthThreshold(int Threshold) {}
	virtual void OnSpecialistDeath() {}
	bool AcquireTarget(float Range, bool RequireSight = true);
	class CCharacter *TargetCharacter();
	void FireProjectile(int Damage, float Spread = 0.0f);
	int CountDroids(int Type, float Radius = 0.0f);
	bool ConsumeThreshold(int Threshold, int Bit);
	void SetMovementGoal(vec2 Pos, int DurationTicks);

	int m_BaseHealth;
	bool m_IsBoss;
	int m_AbilityTick;
	int m_ThresholdMask;
	bool m_PlacementResolved;
	vec2 m_MovementGoal;
	int m_MovementGoalEndTick;
	int m_NextHopTick;
};

#endif
