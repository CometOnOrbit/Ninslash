#include <game/pve/questinfo.h>
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
	assert(InvasionThemeFinalQuest(INVASION_THEME_BOSS_ASSAULT) == QUEST_KILL_BOSS);
	assert(InvasionThemeFinalQuest(INVASION_THEME_PURGE) == QUEST_KILLREMAININGENEMIES);
	assert(InvasionThemeFinalQuest(INVASION_THEME_STANDARD_WAVE) == QUEST_SURVIVEWAVE);
	assert(InvasionThemeFinalQuest(INVASION_THEME_DUAL_SWITCHES) == QUEST_ACTIVATE_SWITCHES);
	assert(InvasionThemeFinalQuest(INVASION_THEME_REACTOR_DEFEND) == QUEST_DEFEND);
	assert(InvasionThemeFinalQuest(INVASION_THEME_TIMED_SURVIVE) == QUEST_SURVIVEWAVETIME);
	assert(InvasionThemeFinalQuest(INVASION_THEME_TRAP_RUN) == QUEST_SURVIVEWAVETIME);
	assert(InvasionThemeFinalQuest(INVASION_THEME_ELITE_WAVE) == QUEST_SURVIVEWAVE);
	assert(InvasionThemeFinalQuest(INVASION_THEME_Z_SECTOR) == QUEST_SURVIVEWAVE);
	assert(InvasionThemeFinalQuest(INVASION_THEME_ACID_ESCAPE) == QUEST_REACHDOOR);
	assert(!InvasionThemeAllowsPushForward(INVASION_THEME_STANDARD_WAVE, false));
	assert(InvasionThemeAllowsPushForward(INVASION_THEME_STANDARD_WAVE, true));
	assert(InvasionThemeAllowsPushForward(INVASION_THEME_TRAP_RUN, false));
	assert(PveEnvironmentUsesPhaseCycle(PVE_BIOME_BLUE_PLANET));
	assert(!PveEnvironmentUsesPhaseCycle(PVE_BIOME_CITY_LOCKDOWN));
	assert(!PveEnvironmentUsesPhaseCycle(PVE_BIOME_CITY_BLACKOUT));
	assert(!PveEnvironmentUsesPhaseCycle(PVE_BIOME_COLLAPSE_RETREAT));
	assert(PveBlackoutBrightness(11) > PveBlackoutBrightness(20));
	assert(PveBlackoutBrightness(20) > PveBlackoutBrightness(40));
	assert(PveBlackoutBrightness(80) == 48);
	return 0;
}
