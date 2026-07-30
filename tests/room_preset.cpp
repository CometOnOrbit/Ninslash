#include <engine/shared/room_preset.h>
#include <assert.h>
#include <string.h>

int main()
{
	const char *pJson =
		"{\"gametype\":\"horde\",\"map\":\"generate_city1\",\"max_players\":8,\"difficulty\":4,\"bots\":3,\"bot_"
		"level\":4,\"mode_rule\":20,\"mapgen\":true,\"mapgen_level\":4,\"random_seed\":false,\"seed\":123,"
		"\"roguelite\":true,\"contracts\":true,\"invasion_start\":1,\"invasion_floor\":1}";
	CRoomPreset A, B;
	char aError[128];
	assert(RoomPresetParse(pJson, strlen(pJson), false, &A, aError, sizeof(aError)));
	assert(RoomPresetParse(pJson, strlen(pJson), true, &B, aError, sizeof(aError)));
	assert(RoomPresetDifferenceMask(A, B) == 0);
	B.m_Seed++;
	assert(RoomPresetDifferenceMask(A, B) == ROOM_PRESET_DIFF_SEED);
	const char *pPrivate = "{\"password\":\"secret\"}";
	assert(!RoomPresetParse(pPrivate, strlen(pPrivate), false, &A, aError, sizeof(aError)));
	const char *pRandomChallenge =
		"{\"gametype\":\"horde\",\"map\":\"generate_city1\",\"max_players\":8,\"difficulty\":4,\"bots\":3,\"bot_"
		"level\":4,\"mode_rule\":20,\"mapgen\":true,\"mapgen_level\":4,\"random_seed\":true,\"seed\":123,\"roguelite\":"
		"true,\"contracts\":true,\"invasion_start\":1,\"invasion_floor\":1}";
	assert(!RoomPresetParse(pRandomChallenge, strlen(pRandomChallenge), true, &A, aError, sizeof(aError)));
	return 0;
}
