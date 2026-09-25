#include <game/pve/invasion_rules.h>
#include <game/pve/pve_environment.h>

#include <assert.h>

int main()
{
	assert(InvasionMapBandIndex(1) == 0);
	assert(InvasionMapBandIndex(10) == 0);
	assert(InvasionMapBandIndex(11) == 1);
	assert(InvasionMapBandIndex(40) == 3);
	assert(InvasionMapBandIndex(41) == 4);
	assert(InvasionMapBandIndex(70) == 6);
	assert(InvasionMapBandIndex(71) == 7);
	assert(InvasionMapsListPickIndex(1, 8) == 0);
	assert(InvasionMapsListPickIndex(11, 8) == 1);
	assert(InvasionMapsListPickIndex(71, 8) == 7);
	assert(InvasionMapsListPickIndex(81, 8) == 0);
	assert(InvasionMapsListPickIndex(5, 0) == 0);
	assert(InvasionObjectiveQuest(INVASION_THEME_BOSS_ASSAULT, 1, 1, 10) == QUEST_KILL_BOSS);
	assert(InvasionObjectiveQuest(INVASION_THEME_BOSS_ASSAULT, 0, 1, 10) == QUEST_SURVIVEWAVE);
	assert(InvasionObjectiveQuest(INVASION_THEME_PURGE, 1, 1, 10) == QUEST_KILLREMAININGENEMIES);
	assert(InvasionObjectiveQuest(INVASION_THEME_STANDARD_WAVE, 1, 1, 12) == QUEST_SURVIVEWAVE);
	assert(InvasionObjectiveQuest(INVASION_THEME_DUAL_SWITCHES, 1, 1, 13) == QUEST_ACTIVATE_SWITCHES);
	assert(InvasionObjectiveQuest(INVASION_THEME_REACTOR_DEFEND, 1, 1, 14) == QUEST_DEFEND);
	assert(InvasionObjectiveQuest(INVASION_THEME_TIMED_SURVIVE, 0, 1, 15) == QUEST_SURVIVEWAVETIME);
	assert(InvasionObjectiveQuest(INVASION_THEME_TRAP_RUN, 1, 1, 16) == QUEST_SURVIVEWAVETIME);
	assert(InvasionObjectiveQuest(INVASION_THEME_ELITE_WAVE, 1, 1, 17) == QUEST_SURVIVEWAVE);
	assert(InvasionObjectiveQuest(INVASION_THEME_Z_SECTOR, 1, 1, 18) == QUEST_SURVIVEWAVE);
	assert(InvasionObjectiveQuest(INVASION_THEME_ACID_ESCAPE, 0, 1, 19) == QUEST_REACHDOOR);
	assert(InvasionObjectiveQuest(INVASION_THEME_TRAP_RUN, 1, 1, 30) == QUEST_KILL_BOSS);
	assert(InvasionWaveType(WAVE_ALIENS, INVASION_THEME_ELITE_WAVE, PVE_BIOME_NONE, 10, PVE_ENV_PHASE_CALM, 1) ==
		   WAVE_ALIENS);
	assert(InvasionWaveType(WAVE_NONE, INVASION_THEME_ELITE_WAVE, PVE_BIOME_NONE, 10, PVE_ENV_PHASE_CALM, 1) ==
		   WAVE_CYBORGS);
	assert(InvasionWaveType(WAVE_NONE, INVASION_THEME_PURGE, PVE_BIOME_NONE, 10, PVE_ENV_PHASE_CALM, 1) ==
		   InvasionWaveType(WAVE_NONE, INVASION_THEME_PURGE, PVE_BIOME_NONE, 10, PVE_ENV_PHASE_DARK, 1));
	assert(InvasionWaveType(WAVE_NONE, INVASION_THEME_PURGE, PVE_BIOME_BLUE_PLANET, 30, PVE_ENV_PHASE_DARK, 1) ==
		   WAVE_ALIENS);
	assert(InvasionWaveType(WAVE_NONE, INVASION_THEME_PURGE, PVE_BIOME_BLUE_PLANET, 30, PVE_ENV_PHASE_CALM, 7) ==
		   InvasionWaveType(WAVE_NONE, INVASION_THEME_PURGE, PVE_BIOME_BLUE_PLANET, 30, PVE_ENV_PHASE_RECOVERY, 7));
	const int WaveBase = InvasionEnemyBudget(INV_BUDGET_WAVE, 10, 1, INVASION_THEME_PURGE, 1.0f);
	const int WaveScaled = InvasionEnemyBudget(INV_BUDGET_WAVE, 10, 1, INVASION_THEME_PURGE, 1.35f);
	assert(WaveBase > 0);
	assert(WaveScaled == (int)(WaveBase * 1.35f + 0.5f));
	assert(InvasionEnemyBudget(INV_BUDGET_TIMED, 10, 1, INVASION_THEME_TIMED_SURVIVE, 1.35f) == 9999);
	assert(PveClampCheckpoint(25, 5) == 1);
	assert(PveClampCheckpoint(25, 11) == 11);
	assert(PveClampCheckpoint(25, 21) == 21);
	assert(PveClampCheckpoint(25, 31) == 1);
	assert(!InvasionThemeAllowsPushForward(INVASION_THEME_STANDARD_WAVE, false));
	assert(InvasionThemeAllowsPushForward(INVASION_THEME_STANDARD_WAVE, true));
	assert(InvasionThemeAllowsPushForward(INVASION_THEME_TRAP_RUN, false));
	assert(PveEnvironmentUsesPhaseCycle(PVE_BIOME_BLUE_PLANET));
	assert(!PveEnvironmentUsesPhaseCycle(PVE_BIOME_NONE));
	assert(PveSanitizeBiome(1) == PVE_BIOME_BLUE_PLANET);
	assert(PveSanitizeBiome(2) == PVE_BIOME_NONE);
	assert(PveSanitizeBiome(7) == PVE_BIOME_NONE);
	assert(InvasionElevatorStop(10));
	assert(InvasionElevatorStop(20));
	assert(!InvasionElevatorStop(0));
	assert(!InvasionElevatorStop(5));
	assert(!InvasionElevatorStop(15));
	return 0;
}
