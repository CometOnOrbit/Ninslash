#include <cassert>

// Hold zone used to activate whenever pos != 0 after snap-to-floor landed in
// an acid pit. Activation now requires a dry standable point.
static bool HoldZoneActivate(bool Standable)
{
	return Standable;
}

// GenerateAcid fills [x,z) x [y,w). Switches inside that rect must not be
// overwritten, and switch placement must skip these tiles.
static bool AcidPitContains(int PitX, int PitY, int PitZ, int PitW, int X, int Y)
{
	return PitX != 0 && X >= PitX && X < PitZ && Y >= PitY && Y < PitW;
}

int main()
{
	assert(!HoldZoneActivate(false));
	assert(HoldZoneActivate(true));

	assert(AcidPitContains(10, 20, 16, 24, 10, 20));
	assert(AcidPitContains(10, 20, 16, 24, 15, 23));
	assert(!AcidPitContains(10, 20, 16, 24, 16, 20));
	assert(!AcidPitContains(10, 20, 16, 24, 12, 24));
	assert(!AcidPitContains(0, 20, 16, 24, 12, 22));
	return 0;
}
