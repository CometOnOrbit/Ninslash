#ifndef GAME_DETERMINISTIC_RANDOM_H
#define GAME_DETERMINISTIC_RANDOM_H

// Deterministic, cross-platform PRNG (splitmix64) for content generation.
// Independent of the global rand() state, so a challenge run with the same
// seed reproduces the same content stream (enemy line-ups, tasks, events).
// Micro-behaviour (AI movement, bullet spread) intentionally keeps using the
// global rand(); only content selection goes through this stream.
//
// See docs/playability_design.md §3.2.
class CDeterministicRandom
{
	unsigned long long m_State;

	static unsigned long long SplitMix64(unsigned long long &x)
	{
		unsigned long long z = (x += 0x9E3779B97F4A7C15ull);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}

  public:
	CDeterministicRandom(unsigned long long Seed)
	{
		m_State = Seed ? Seed : 0x9E3779B97F4A7C15ull;
		SplitMix64(m_State); // warm up
	}

	// [0, Max)
	int NextInt(int Max)
	{
		if(Max <= 0)
			return 0;
		return (int)(SplitMix64(m_State) % (unsigned long long)Max);
	}

	// [Min, Max]
	int Range(int Min, int Max)
	{
		return Min + NextInt(Max - Min + 1);
	}

	// [0, 1)
	float NextFloat()
	{
		return (SplitMix64(m_State) >> 40) / (float)(1ull << 24);
	}
};

// Derives an independent stream per content domain from the base seed, so
// different content categories do not interfere with each other.
inline unsigned long long DeterministicSeed(unsigned long long Base, const char *pDomain)
{
	unsigned long long h = Base ? Base : 1;
	for(; *pDomain; pDomain++)
		h = h * 131 + (unsigned char)*pDomain;
	return h;
}

#endif
