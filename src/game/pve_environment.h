#ifndef GAME_PVE_ENVIRONMENT_H
#define GAME_PVE_ENVIRONMENT_H

// Server-owned PvE environment themes. Values are protocol-stable: append new
// themes, never reorder existing ones.
enum EPveEnvironmentBiome
{
	PVE_BIOME_NONE = 0,
	PVE_BIOME_BLUE_PLANET,
	PVE_BIOME_CITY_LOCKDOWN,
	PVE_BIOME_CITY_BLACKOUT,
	PVE_BIOME_COLLAPSE_RETREAT,
	PVE_BIOME_VERTICAL_RUINS,
	PVE_BIOME_STORM_FRONT,
	PVE_BIOME_ORBITAL,
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

inline bool PveEnvironmentUsesPhaseCycle(int Biome)
{
	return Biome == PVE_BIOME_BLUE_PLANET;
}

inline int PveBlackoutBrightness(int Level)
{
	const int Depth = Level > 10 ? Level - 10 : 0;
	const int Brightness = 160 - Depth * 6;
	return Brightness < 48 ? 48 : Brightness;
}

#endif
