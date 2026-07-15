#ifndef GAME_SERVER_ENTITIES_DROID_ASSEMBLER_H
#define GAME_SERVER_ENTITIES_DROID_ASSEMBLER_H
#include "droid_specialist.h"
class CAssembler : public CSpecialistDroid { public: CAssembler(CGameWorld *pWorld, vec2 Pos); protected: void AbilityTick() override; };
#endif
