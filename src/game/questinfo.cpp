#include "questinfo.h"
#include <base/deterministic_random.h>
#include <base/math.h>

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
			return "Activate switches to extract";
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

const char *GetFieldOrderDisplayName(int FieldOrder)
{
	switch(FieldOrder)
	{
		case FIELD_ORDER_FIREPOWER:
			return "Firepower";
		case FIELD_ORDER_BLITZ:
			return "Blitz";
		case FIELD_ORDER_SALVAGE:
			return "Salvage";
		case FIELD_ORDER_STEALTH:
			return "Stealth";
		case FIELD_ORDER_FURY:
			return "Fury";
		case FIELD_ORDER_ARMORY:
			return "Armory";
		case FIELD_ORDER_BULWARK:
			return "Bulwark";
		case FIELD_ORDER_STANDARD:
		default:
			return "Standard";
	}
}

const char *GetFieldOrderEffectText(int FieldOrder)
{
	switch(FieldOrder)
	{
		case FIELD_ORDER_FIREPOWER:
			return "Team damage +10%";
		case FIELD_ORDER_BLITZ:
			return "Team speed +8%, enemy speed -8%";
		case FIELD_ORDER_SALVAGE:
			return "Drops +50%, elite chance +20%";
		case FIELD_ORDER_STEALTH:
			return "Wave size -30%, defend time +50%";
		case FIELD_ORDER_FURY:
			return "Fire rate +15%, max health -15%";
		case FIELD_ORDER_ARMORY:
			return "Start with 3 upgrade drops";
		case FIELD_ORDER_BULWARK:
			return "Build cost -40%, buildings take -25% damage";
		case FIELD_ORDER_STANDARD:
		default:
			return "No modifiers";
	}
}

int FieldOrderEffect(int FieldOrder)
{
	switch(FieldOrder)
	{
		case FIELD_ORDER_FIREPOWER:
			return FIELD_EFFECT_DAMAGE;
		case FIELD_ORDER_BLITZ:
			return FIELD_EFFECT_SPEED;
		case FIELD_ORDER_SALVAGE:
			return FIELD_EFFECT_SALVAGE;
		case FIELD_ORDER_STEALTH:
			return FIELD_EFFECT_STEALTH;
		case FIELD_ORDER_FURY:
			return FIELD_EFFECT_FURY;
		case FIELD_ORDER_ARMORY:
			return FIELD_EFFECT_ARMORY;
		case FIELD_ORDER_BULWARK:
			return FIELD_EFFECT_BULWARK;
		case FIELD_ORDER_STANDARD:
		default:
			return FIELD_EFFECT_NONE;
	}
}

int InvasionFieldOrderCandidates(int Level, int *pPackages, int MaxPackages)
{
	if(!pPackages || MaxPackages <= 0)
		return 0;
	pPackages[0] = FIELD_ORDER_STANDARD;
	int Count = 1;
	const int Theme = InvasionThemeFromLevel(max(1, Level));
	static const int s_aPool[] = {
		FIELD_ORDER_FIREPOWER,
		FIELD_ORDER_BLITZ,
		FIELD_ORDER_SALVAGE,
		FIELD_ORDER_STEALTH,
		FIELD_ORDER_FURY,
		FIELD_ORDER_ARMORY,
		FIELD_ORDER_BULWARK,
	};
	const int PoolSize = (int)(sizeof(s_aPool) / sizeof(s_aPool[0]));
	int aShuffled[PoolSize];
	for(int i = 0; i < PoolSize; i++)
		aShuffled[i] = s_aPool[i];
	for(int i = PoolSize - 1; i > 0; i--)
	{
		const int J = irandom(i + 1);
		const int Tmp = aShuffled[i];
		aShuffled[i] = aShuffled[J];
		aShuffled[J] = Tmp;
	}
	const int MaxAlternatives = min(2, MaxPackages - 1);
	for(int i = 0; i < PoolSize && Count - 1 < MaxAlternatives; i++)
	{
		const int Package = aShuffled[i];
		if(Theme == INVASION_THEME_ACID_ESCAPE && Package != FIELD_ORDER_STEALTH)
			continue;
		if(Theme == INVASION_THEME_REACTOR_DEFEND && Package == FIELD_ORDER_BLITZ)
			continue;
		if(Theme == INVASION_THEME_TRAP_RUN && (Package == FIELD_ORDER_BLITZ || Package == FIELD_ORDER_FURY))
			continue;
		pPackages[Count++] = Package;
	}
	return Count;
}
