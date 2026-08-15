#include <base/deterministic_random.h>
#include <base/math.h>

#include <cassert>
#include <cmath>

int main()
{
	assert(irandom(0) == 0);
	assert(irandom(1) == 0);

	seed_random(4245);
	const int aExpected[] = {15, 60, 99, 52, 22, 57, 42, 12};
	for(int i = 0; i < 8; i++)
		assert(irandom(100) == aExpected[i]);

	seed_random(4245);
	int aFirst[16];
	for(int i = 0; i < 16; i++)
		aFirst[i] = irandom(97);
	seed_random(4245);
	for(int i = 0; i < 16; i++)
		assert(irandom(97) == aFirst[i]);

	seed_random(1);
	assert(fabsf(frandom() - 0.74578172f) < 0.000001f);
	assert(frandom() >= 0.0f && frandom() < 1.0f);

	assert(DeterministicSeed(4245, "mapgen") == 21458095281904257ull);
	seed_random(DeterministicSeed(4245, "mapgen") + 4);
	const int A = irandom(1000);
	seed_random(DeterministicSeed(4245, "mapgen") + 4);
	assert(irandom(1000) == A);
	return 0;
}
