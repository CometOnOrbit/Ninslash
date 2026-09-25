#ifndef GAME_PVE_INVASION_RULES_H
#define GAME_PVE_INVASION_RULES_H

#include <base/deterministic_random.h>
#include <base/math.h>

#include <game/pve/pve_environment.h>
#include <game/pve/questinfo.h>

// Checkpoint floors are 1, 11, 21, ... up to the band the player has cleared.
// Shop stop after floors 10, 20, 30, ... The next combat floor still starts with its perk vote.
inline bool InvasionElevatorStop(int CompletedLevel)
{
	return CompletedLevel > 0 && CompletedLevel % 10 == 0;
}

inline int PveCheckpointMax(int Highest)
{
	if(Highest < 10)
		return 1;
	return (Highest / 10) * 10 + 1;
}

inline int PveClampCheckpoint(int Highest, int Preferred)
{
	const int MaxCheckpoint = PveCheckpointMax(Highest);
	if(Preferred < 1 || Preferred > MaxCheckpoint || (Preferred - 1) % 10 != 0)
		return 1;
	return Preferred;
}

inline int InvasionEffectiveLevel(int Level)
{
	Level = max(0, Level);
	return min(Level, 20) + max(0, Level - 20) / 2;
}

inline int InvasionWaveCap(int Level, int Players)
{
	const int EffectiveLevel = InvasionEffectiveLevel(Level);
	Players = max(1, Players);
	return min(Level > 20 ? 24 : 28, 10 + EffectiveLevel / 2 + Players);
}

inline int InvasionConcurrentCap(int Level, int Players, int WaveSizeNerf)
{
	return max(8, InvasionWaveCap(Level, Players) - WaveSizeNerf * 3);
}

enum
{
	INV_BUDGET_OPENING = 0,
	INV_BUDGET_WAVE,
	INV_BUDGET_PUSH,
	INV_BUDGET_PURGE,
	INV_BUDGET_BOSS_MINIONS,
	INV_BUDGET_ACID,
	INV_BUDGET_TIMED,
};

// CountMultiplier is applied once. Timed waves use 9999 as a timer sentinel.
inline int InvasionEnemyBudget(int Kind, int Level, int Players, int Theme, float CountMultiplier)
{
	if(Kind == INV_BUDGET_TIMED)
		return 9999;
	if(CountMultiplier < 0.0f)
		CountMultiplier = 0.0f;
	Players = max(1, Players);
	Level = max(0, Level);
	const int Effective = InvasionEffectiveLevel(Level);
	int Count = 8;
	switch(Kind)
	{
		case INV_BUDGET_OPENING:
			Count = min(Level > 20 ? 16 : 18, max(7, 6 + Effective));
			break;
		case INV_BUDGET_WAVE:
			Count = (int)(min(8 + Effective * 2, Level > 20 ? 42 : 50) * (1.0f + (Players - 1) * 0.2f) + 0.5f);
			if(Theme == INVASION_THEME_Z_SECTOR)
				Count = (int)(Count * 1.25f + 0.5f);
			break;
		case INV_BUDGET_PUSH:
			Count = min(InvasionWaveCap(Level, Players), max(6, 8 + Effective / 2));
			break;
		case INV_BUDGET_PURGE:
			Count = min(16, 8 + Level);
			break;
		case INV_BUDGET_BOSS_MINIONS:
			Count = min(Level > 20 ? 12 : 16, 6 + (Level > 20 ? Effective / 4 : Level / 3));
			break;
		case INV_BUDGET_ACID:
			Count = min(10, 4 + Level / 4);
			break;
		default:
			break;
	}
	return max(1, (int)(Count * CountMultiplier + 0.5f));
}

// Forced wave, then theme, then biome. Blue Planet calm/recovery is the only roll.
inline int InvasionWaveType(int Forced, int Theme, int Biome, int Level, int EnvironmentPhase, unsigned long long Seed)
{
	if(Forced > WAVE_NONE && Forced < NUM_WAVES)
		return Forced;
	if(Theme == INVASION_THEME_ELITE_WAVE)
		return WAVE_CYBORGS;
	if(Theme == INVASION_THEME_Z_SECTOR)
		return WAVE_ALIENS;
	if(Biome == PVE_BIOME_BLUE_PLANET)
	{
		if(EnvironmentPhase == PVE_ENV_PHASE_DARK)
			return WAVE_ALIENS;
		if(EnvironmentPhase == PVE_ENV_PHASE_WARNING)
			return WAVE_ROBOTS;
	}
	const int WaveUnlocked = min(NUM_WAVES - 1, max(2, Level / 5 + 1));
	CDeterministicRandom WaveRng(DeterministicSeed(Seed ? Seed : 1, "invasion_wave"));
	return WaveRng.NextInt(WaveUnlocked) + 1;
}

// Quest the theme wants before map fallbacks (missing switches, missing reactor, push route).
inline int InvasionObjectiveQuest(int Theme, int Done, int LastSlot, int Level)
{
	if(Level == 30 && Done >= LastSlot)
		return QUEST_KILL_BOSS;
	switch(Theme)
	{
		case INVASION_THEME_BOSS_ASSAULT:
			return Done >= LastSlot ? QUEST_KILL_BOSS : QUEST_SURVIVEWAVE;
		case INVASION_THEME_PURGE:
			return Done >= LastSlot ? QUEST_KILLREMAININGENEMIES : QUEST_SURVIVEWAVE;
		case INVASION_THEME_DUAL_SWITCHES:
			return Done >= LastSlot ? QUEST_ACTIVATE_SWITCHES : QUEST_SURVIVEWAVE;
		case INVASION_THEME_REACTOR_DEFEND:
			return Done >= LastSlot ? QUEST_DEFEND : QUEST_SURVIVEWAVE;
		case INVASION_THEME_TIMED_SURVIVE:
		case INVASION_THEME_TRAP_RUN:
			return QUEST_SURVIVEWAVETIME;
		case INVASION_THEME_ACID_ESCAPE:
			return QUEST_REACHDOOR;
		default:
			return QUEST_SURVIVEWAVE;
	}
}

#endif
