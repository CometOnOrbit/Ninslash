#ifndef GAME_SERVER_ENTITIES_DROID_SABOTEUR_H
#define GAME_SERVER_ENTITIES_DROID_SABOTEUR_H
#include "droid_specialist.h"
class CSaboteur : public CSpecialistDroid { public: CSaboteur(CGameWorld *pWorld, vec2 Pos); protected: void AbilityTick() override; private: CEntity *m_pEmpTarget; int m_ChargeStart; int m_NextSlowFieldTick; };
#endif
