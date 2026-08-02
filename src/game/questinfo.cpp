#include "questinfo.h"

const char *GetQuestDisplayName(int Quest)
{
	switch(Quest)
	{
		case QUEST_KILLREMAININGENEMIES:
			return "Terminate the enemies";
		case QUEST_REACHDOOR:
			return "Reach the door";
		case QUEST_SURVIVEWAVE:
			return "Survive the wave of enemies";
		case QUEST_SURVIVEWAVETIME:
			return "Hold out against the wave";
		case QUEST_FIND_SWITCH:
			return "Find the switch";
		case QUEST_KILL_BOSS:
			return "Destroy the boss";
		case QUEST_DEFEND:
			return "Defend the reactor";
		case QUEST_ACTIVATE_SWITCHES:
			return "Activate the switches";
		case QUEST_HORDE:
			return "Survive the horde";
		case QUEST_EXTRACT:
			return "Recover valuables and extract";
		case QUEST_DESTROY_TURRETS:
			return "Destroy the turrets";
		case QUEST_HOLD_ZONE:
			return "Hold the signal point";
		default:
			return "";
	}
}

const char *GetQuestStartMessage(int Quest, int WaveType)
{
	if(Quest == QUEST_SURVIVEWAVE || Quest == QUEST_SURVIVEWAVETIME)
	{
		switch(WaveType)
		{
			case WAVE_ALIENS:
				return "Wave of aliens incoming";
			case WAVE_ROBOTS:
				return "Wave of robots incoming";
			case WAVE_SKELETONS:
				return "Wave of skeletons incoming";
			case WAVE_FURRIES:
				return "Wave of furries incoming";
			case WAVE_CYBORGS:
				return "Wave of cyborgs incoming";
			default:
				return "Wave incoming";
		}
	}

	if(Quest == QUEST_KILLREMAININGENEMIES)
	{
		switch(WaveType)
		{
			case WAVE_ALIENS:
				return "Terminate the aliens";
			case WAVE_ROBOTS:
				return "Terminate the robots";
			case WAVE_SKELETONS:
				return "Terminate the skeletons";
			case WAVE_FURRIES:
				return "Terminate the furries";
			case WAVE_CYBORGS:
				return "Terminate the cyborgs";
			default:
				return "Terminate the enemies";
		}
	}

	switch(Quest)
	{
		case QUEST_KILLREMAININGENEMIES:
			return "Terminate the enemies";
		case QUEST_REACHDOOR:
			return "Seek the door";
		case QUEST_SURVIVEWAVE:
			return "Wave incoming";
		case QUEST_SURVIVEWAVETIME:
			return "Hold the line";
		case QUEST_FIND_SWITCH:
			return "Find and activate the switch";
		case QUEST_KILL_BOSS:
			return "Destroy the boss";
		case QUEST_DEFEND:
			return "Defend the reactor";
		case QUEST_ACTIVATE_SWITCHES:
			return "Activate all switches";
		case QUEST_HORDE:
			return "Horde incoming";
		case QUEST_EXTRACT:
			return "Find and activate the switches";
		case QUEST_DESTROY_TURRETS:
			return "Destroy the enemy turrets";
		case QUEST_HOLD_ZONE:
			return "Hold the marked signal point";
		default:
			return "";
	}
}

const char *GetQuestCompletedMessage(int Quest, int WaveType)
{
	if(Quest == QUEST_SURVIVEWAVE || Quest == QUEST_SURVIVEWAVETIME)
	{
		switch(WaveType)
		{
			case WAVE_ALIENS:
				return "Alien wave cleared";
			case WAVE_ROBOTS:
				return "Robot wave cleared";
			case WAVE_FURRIES:
				return "Furry wave cleared";
			case WAVE_SKELETONS:
				return "Skeleton wave cleared";
			case WAVE_CYBORGS:
				return "Cyborg wave cleared";
			default:
				return "Wave cleared";
		}
	}

	if(Quest == QUEST_KILLREMAININGENEMIES)
	{
		switch(WaveType)
		{
			case WAVE_ALIENS:
				return "Aliens terminated";
			case WAVE_ROBOTS:
				return "Robots terminated";
			case WAVE_SKELETONS:
				return "Skeletons terminated";
			case WAVE_FURRIES:
				return "Furries terminated";
			case WAVE_CYBORGS:
				return "Cyborgs terminated";
			default:
				return "Enemies terminated";
		}
	}

	switch(Quest)
	{
		case QUEST_FIND_SWITCH:
			return "Switch activated";
		case QUEST_ACTIVATE_SWITCHES:
			return "All switches activated";
		case QUEST_KILL_BOSS:
			return "Boss destroyed";
		case QUEST_DEFEND:
			return "Reactor secured";
		case QUEST_REACHDOOR:
			return "";
		case QUEST_HORDE:
			return "Wave cleared";
		case QUEST_EXTRACT:
			return "Extraction ready";
		case QUEST_DESTROY_TURRETS:
			return "Turrets destroyed";
		case QUEST_HOLD_ZONE:
			return "Signal secured";
		default:
			return "";
	}
}

const char *GetThemeDisplayName(int Theme)
{
	switch(Theme)
	{
		case INVASION_THEME_BOSS_ASSAULT:
			return "Boss assault";
		case INVASION_THEME_PURGE:
			return "Purge";
		case INVASION_THEME_STANDARD_WAVE:
			return "Standard wave";
		case INVASION_THEME_DUAL_SWITCHES:
			return "Dual switches";
		case INVASION_THEME_REACTOR_DEFEND:
			return "Reactor defense";
		case INVASION_THEME_TIMED_SURVIVE:
			return "Timed survive";
		case INVASION_THEME_TRAP_RUN:
			return "Trap run";
		case INVASION_THEME_ELITE_WAVE:
			return "Elite wave";
		case INVASION_THEME_Z_SECTOR:
			return "Z-sector wave";
		case INVASION_THEME_ACID_ESCAPE:
			return "Acid escape";
		case INVASION_THEME_TURRET_SWEEP:
			return "Turret sweep";
		case INVASION_THEME_SIGNAL_HOLD:
			return "Signal hold";
		default:
			return "Invasion";
	}
}

const char *GetWaveDisplayName(int WaveType)
{
	switch(WaveType)
	{
		case WAVE_ALIENS:
			return "Aliens";
		case WAVE_ROBOTS:
			return "Robots";
		case WAVE_SKELETONS:
			return "Skeletons";
		case WAVE_FURRIES:
			return "Furries";
		case WAVE_CYBORGS:
			return "Cyborgs";
		default:
			return "";
	}
}
