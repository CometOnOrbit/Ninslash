#ifndef GAME_SERVER_ENTITIES_ACTORS_DROID_BULWARK_H
#define GAME_SERVER_ENTITIES_ACTORS_DROID_BULWARK_H
#include "droid_specialist.h"
class CBulwark : public CSpecialistDroid { public: CBulwark(CGameWorld *pWorld, vec2 Pos); protected: void AbilityTick() override; };
#endif
