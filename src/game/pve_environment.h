#ifndef GAME_PVE_ENVIRONMENT_H
#define GAME_PVE_ENVIRONMENT_H

// Server-owned PvE environment themes. Values are protocol-stable: append new
// themes, never reorder existing ones.
enum EPveEnvironmentBiome
{
	PVE_BIOME_NONE = 0,
	PVE_BIOME_BLUE_PLANET,
};

enum EPveEnvironmentPhase
{
	PVE_ENV_PHASE_CALM = 0,
	PVE_ENV_PHASE_WARNING,
	PVE_ENV_PHASE_DARK,
	PVE_ENV_PHASE_RECOVERY,
};

enum EPveEnvironmentBossPhase
{
	PVE_ENV_BOSS_PHASE_NONE = 0,
	PVE_ENV_BOSS_PHASE_ONE,
	PVE_ENV_BOSS_PHASE_TWO,
	PVE_ENV_BOSS_PHASE_THREE,
};

#endif
