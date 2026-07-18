#ifndef GAME_SERVER_ENTITIES_DROID_OVERSEER_CORE_H
#define GAME_SERVER_ENTITIES_DROID_OVERSEER_CORE_H
#include "droid_specialist.h"
class COverseerCore : public CSpecialistDroid
{
public:
	COverseerCore(CGameWorld *pWorld, vec2 Pos);
protected:
	void AbilityTick() override;
	void MovementTick(class CCharacter *pTarget) override;
	vec2 CollisionSize() const override { return vec2(116.0f, 116.0f); }
	void TakeDamage(vec2 Force, int Dmg, const CAttackSource &Source, vec2 Pos) override;
	void OnHealthThreshold(int Threshold) override;
	void OnSpecialistDeath() override;
private:
	void SpawnPhase(int Threshold);
	int m_EmpTick;
	int m_Burst;
	float m_OrbitAngle;
	CDroid *m_apAssemblers[2];
};
#endif
