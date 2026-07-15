#ifndef GAME_SERVER_ENTITIES_DROID_OVERSEER_CORE_H
#define GAME_SERVER_ENTITIES_DROID_OVERSEER_CORE_H
#include "droid_specialist.h"
class COverseerCore : public CSpecialistDroid { public: COverseerCore(CGameWorld *pWorld, vec2 Pos); protected: void AbilityTick() override; void OnHealthThreshold(int Threshold) override; private: void SpawnPhase(int Threshold); int m_EmpTick; };
#endif
