#ifndef GAME_SERVER_ENTITIES_DROID_SIEGE_ENGINE_H
#define GAME_SERVER_ENTITIES_DROID_SIEGE_ENGINE_H
#include "droid_specialist.h"
class CSiegeEngine : public CSpecialistDroid { public: CSiegeEngine(CGameWorld *pWorld, vec2 Pos); protected: void AbilityTick() override; void OnHealthThreshold(int Threshold) override; private: int m_SkillCycle; };
#endif
