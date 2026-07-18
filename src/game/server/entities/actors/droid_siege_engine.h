#ifndef GAME_SERVER_ENTITIES_ACTORS_DROID_SIEGE_ENGINE_H
#define GAME_SERVER_ENTITIES_ACTORS_DROID_SIEGE_ENGINE_H
#include "droid_specialist.h"
class CSiegeEngine : public CSpecialistDroid
{
public:
	CSiegeEngine(CGameWorld *pWorld, vec2 Pos);
protected:
	void AbilityTick() override;
	void MovementTick(class CCharacter *pTarget) override;
	void OnHealthThreshold(int Threshold) override;
	void OnSpecialistDeath() override;
private:
	int m_SkillCycle;
	int m_ChargeEndTick;
	bool m_ChargeHit;
	CDroid *m_apGuards[2];
};
#endif
