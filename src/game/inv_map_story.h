#ifndef GAME_INV_MAP_STORY_H
#define GAME_INV_MAP_STORY_H

// Procedural coop / INV map layout + first featured quest strand (computed before map gen).
enum
{
	INV_MAP_STORY_NONE = 0,
	INV_MAP_STORY_PREP_SIEGE, // Deploy shield gen, next defend strand decided at runtime
	INV_MAP_STORY_DEF_REACTOR,
	INV_MAP_STORY_DEF_GENERATORS,
	INV_MAP_STORY_PURGE_ESCAPE, // Vertical climb / exit high + purge switch elsewhere
	INV_MAP_STORY_PREP_THEN_DEFEND_SHIELDS, // tutorial: players build gens first — no prefab generators on map
};

inline int InvPickMapStory(unsigned Level, unsigned Seed)
{
	if(Level < 5)
		return INV_MAP_STORY_NONE;

	unsigned Mix = Level * 73856093u ^ Seed * 19349663u ^ 4042322167u;
	Mix ^= Mix >> 15;
	Mix *= 2654435761u;
	Mix ^= Mix >> 13;
	int Chance = (int)(Mix % 100);

	// Mirrors invasion.cpp mission weights (sans “already have gens” tweak).
	if(Level >= 7 && Chance >= 42 && Chance < 54)
		return INV_MAP_STORY_PURGE_ESCAPE;
	if(Level >= 5 && Chance < 14)
		return INV_MAP_STORY_PREP_SIEGE;
	if(Level >= 6 && Chance >= 14 && Chance < 32)
		return INV_MAP_STORY_DEF_REACTOR;
	if(Level >= 6 && Chance >= 32 && Chance < 42)
		return INV_MAP_STORY_DEF_GENERATORS;
	return INV_MAP_STORY_NONE;
}

// Coop levels 1–4: fixed tutorial progression (story drives map gimmicks + first ConsumeFeatured strand).
// 1 = wipe enemies only, 2 = defend reactor, 3 = shield gens, 4 = purge/acid climb & escape.
inline int InvCoopStoryForLevel(unsigned Level, unsigned Seed)
{
	switch(Level)
	{
	case 1: return INV_MAP_STORY_NONE;
	case 2: return INV_MAP_STORY_DEF_REACTOR;
	case 3: return INV_MAP_STORY_PREP_THEN_DEFEND_SHIELDS;
	case 4: return INV_MAP_STORY_PURGE_ESCAPE;
	default: return InvPickMapStory(Level, Seed);
	}
}

inline int InvPrepFollowDice(unsigned Level, unsigned Seed)
{
	unsigned Mix = Level * 83492791u ^ Seed * 97266353u ^ 2166136261u;
	Mix ^= Mix >> 17;
	Mix *= 374761393u;
	return (Mix % 100);
}

inline bool InvMapStoryUsesPurgingAscent(int StoryOrLevelNine, int Level)
{
	return StoryOrLevelNine == INV_MAP_STORY_PURGE_ESCAPE || Level % 10 == 9;
}

#endif
