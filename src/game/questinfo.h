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
	QUEST_HOLD_ZONE = 12,
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

// Server game-vote config names. The vote only runs outside Invasion rooms
// (Invasion keeps its campaign going instead of voting), so the mode vote must
// offer only the base Invasion tier: voting into Invasion always starts at
// floor 1 instead of inheriting another mode's sv_mapgen_level (which other
// modes use for difficulty / map generation).
inline bool IsInvasionVoteConfig(const char *pConfig)
{
	return str_comp_num(pConfig, "cfg/invasion", 12) == 0;
}

inline bool IsBaseInvasionVoteConfig(const char *pConfig)
{
	return str_comp(pConfig, "cfg/invasion1") == 0;
}

inline int InvasionMapBandIndex(int Level)
{
	if(Level < 1)
		Level = 1;
	return (Level - 1) / 10;
}

inline int InvasionMapsListPickIndex(int Level, int Count)
{
	if(Count <= 0)
		return 0;
	return InvasionMapBandIndex(Level) % Count;
}

// Mapgen layouts that need player spawn + enemy spawn (Invasion-style).
inline bool IsCoopMapGenGametype(const char *pType)
{
	return str_comp(pType, "coop") == 0 || str_comp(pType, "tutorial") == 0 || str_comp(pType, "horde") == 0 ||
		   str_comp(pType, "extract") == 0;
}

inline bool IsTutorialGametype(const char *pType)
{
	return str_comp(pType, "tutorial") == 0;
}

enum EFieldOrder
{
	FIELD_ORDER_STANDARD = 0, // default, always offered
	FIELD_ORDER_FIREPOWER,	  // player damage +10%
	FIELD_ORDER_BLITZ,		  // player speed +8%, enemy speed -8%
	FIELD_ORDER_SALVAGE,	  // drop rate +50%, elite chance +20%
	FIELD_ORDER_STEALTH,	  // wave size -30%, defend time +50%
	FIELD_ORDER_FURY,		  // weapon cooldown -15%, max health -15%
	FIELD_ORDER_ARMORY,		  // 3 upgrade drops at floor start
	FIELD_ORDER_BULWARK,	  // build cost -40%, building damage taken -25%
	NUM_FIELD_ORDERS,
};

enum EFieldOrderEffect
{
	FIELD_EFFECT_NONE = 0,
	FIELD_EFFECT_DAMAGE,
	FIELD_EFFECT_SPEED,
	FIELD_EFFECT_SALVAGE,
	FIELD_EFFECT_STEALTH,
	FIELD_EFFECT_FURY,
	FIELD_EFFECT_ARMORY,
	FIELD_EFFECT_BULWARK,
	NUM_FIELD_EFFECTS,
};

const char *GetFieldOrderDisplayName(int FieldOrder);
const char *GetFieldOrderEffectText(int FieldOrder);
int FieldOrderEffect(int FieldOrder);
int InvasionFieldOrderCandidates(int Level, int *pPackages, int MaxPackages);

const char *GetQuestDisplayName(int Quest);
const char *GetQuestStartMessage(int Quest, int WaveType = WAVE_NONE);
const char *GetQuestCompletedMessage(int Quest, int WaveType = WAVE_NONE);
const char *GetThemeDisplayName(int Theme);
const char *GetWaveDisplayName(int WaveType);

#endif
