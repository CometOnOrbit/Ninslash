#ifndef GAME_CHALLENGE_VARIANT_H
#define GAME_CHALLENGE_VARIANT_H

// Challenge modifiers (docs/playability_design.md §2). Each variant occupies
// one bit of sv_challenge_variants; bit i enables EChallengeVariant value i.
// Official variants are defined here (data-driven JSON/Lua extension is a
// later step); effect layers: parameter (tuning/sv_ override), generation
// (enemy/drop/event hooks), logic (code hooks).
enum EChallengeVariant
{
	CHALLENGE_NONE = 0,
	CHALLENGE_LOW_GRAVITY, // 1 << 1: 40% gravity
	CHALLENGE_NO_BUILD, // 1 << 2: building disabled
	CHALLENGE_DOUBLE_ENEMIES, // 1 << 3: doubled enemy pressure (generation layer, pending)
	CHALLENGE_GLASS_CANNON, // 1 << 4: damage tuning (pending)
	CHALLENGE_ONLY_MELEE, // 1 << 5: firearms disabled; non-firearm items allowed
	CHALLENGE_DARK, // 1 << 6: dark vision (server-authoritative client render layer)
	NUM_CHALLENGE_VARIANTS,
};

// Applies enabled variants (bitmask) to world tuning / server config. Called
// by controllers during construction, after their baseline config is set.
// Returns the number of variants applied.
int ApplyChallengeVariants(class CGameContext *pGameServer);

inline bool ChallengeVariantEnabled(int Mask, int Variant)
{
	return (Mask & (1 << Variant)) != 0;
}

#endif
