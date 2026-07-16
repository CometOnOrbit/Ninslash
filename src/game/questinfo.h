#ifndef GAME_QUESTINFO_H
#define GAME_QUESTINFO_H

#include <cstring>
#include <base/system.h>
#include <generated/protocol.h>

enum Quests
{
	QUEST_NONE,
	QUEST_KILLREMAININGENEMIES,
	QUEST_REACHDOOR,
	QUEST_SURVIVEWAVE,
	QUEST_SURVIVEWAVETIME,
	QUEST_FIND_SWITCH,
	QUEST_KILL_BOSS,
	QUEST_DEFEND,
	QUEST_ACTIVATE_SWITCHES,
	QUEST_HORDE,
	QUEST_EXTRACT,
	QUEST_ROUTE,
};

enum WaveTypes
{
	WAVE_NONE,
	WAVE_ALIENS,
	WAVE_ROBOTS,
	WAVE_SKELETONS,
	WAVE_FURRIES,
	WAVE_CYBORGS,
	NUM_WAVES,
};

// Invasion coop floor themes (Level % INVASION_THEME_CYCLE).
enum InvasionThemes
{
	INVASION_THEME_BOSS_ASSAULT = 0,
	INVASION_THEME_PURGE,
	INVASION_THEME_STANDARD_WAVE,
	INVASION_THEME_DUAL_SWITCHES,
	INVASION_THEME_REACTOR_DEFEND,
	INVASION_THEME_TIMED_SURVIVE,
	INVASION_THEME_TRAP_RUN,
	INVASION_THEME_ELITE_WAVE,
	INVASION_THEME_Z_SECTOR,
	INVASION_THEME_ACID_ESCAPE,
	NUM_INVASION_THEMES,
};

static const int INVASION_THEME_CYCLE = NUM_INVASION_THEMES;

inline int InvasionThemeFromLevel(int Level)
{
	return Level % INVASION_THEME_CYCLE;
}

// Mapgen layouts that need player spawn + enemy spawn (Invasion-style).
inline bool IsCoopMapGenGametype(const char *pType)
{
	return str_comp(pType, "coop") == 0
		|| str_comp(pType, "horde") == 0
		|| str_comp(pType, "extract") == 0;
}


const char *GetQuestDisplayName(int Quest);
const char *GetQuestStartMessage(int Quest, int WaveType = WAVE_NONE);
const char *GetQuestCompletedMessage(int Quest, int WaveType = WAVE_NONE);
const char *GetThemeDisplayName(int Theme);
const char *GetWaveDisplayName(int WaveType);

#endif
