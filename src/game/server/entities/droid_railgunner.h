#ifndef GAME_SERVER_ENTITIES_DROID_RAILGUNNER_H
#define GAME_SERVER_ENTITIES_DROID_RAILGUNNER_H
#include "droid_specialist.h"
class CRailgunner : public CSpecialistDroid { public: CRailgunner(CGameWorld *pWorld, vec2 Pos); protected: void AbilityTick() override; private: int m_ChargeStart; vec2 m_AimDir; void FireRail(); };
#endif
