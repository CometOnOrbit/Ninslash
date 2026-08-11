#include <game/questinfo.h>

#include <assert.h>

int main()
{
	assert(InvasionMapBandIndex(1) == 0);
	assert(InvasionMapBandIndex(10) == 0);
	assert(InvasionMapBandIndex(11) == 1);
	assert(InvasionMapBandIndex(40) == 3);
	assert(InvasionMapBandIndex(41) == 4);
	assert(InvasionMapBandIndex(70) == 6);
	assert(InvasionMapBandIndex(71) == 7);
	assert(InvasionMapsListPickIndex(1, 8) == 0);
	assert(InvasionMapsListPickIndex(11, 8) == 1);
	assert(InvasionMapsListPickIndex(71, 8) == 7);
	assert(InvasionMapsListPickIndex(81, 8) == 0);
	assert(InvasionMapsListPickIndex(5, 0) == 0);
	return 0;
}
