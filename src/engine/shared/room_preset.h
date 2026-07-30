#ifndef ENGINE_SHARED_ROOM_PRESET_H
#define ENGINE_SHARED_ROOM_PRESET_H

struct CRoomPreset
{
	char m_aGameType[32];
	char m_aMapLocator[256];
	int m_MaxPlayers;
	int m_Difficulty;
	int m_Bots;
	int m_BotLevel;
	int m_ModeRule;
	int m_MapGenLevel;
	int m_Seed;
	int m_InvasionStart;
	int m_InvasionFloor;
	bool m_MapGen;
	bool m_RandomSeed;
	bool m_Roguelite;
	bool m_Contracts;
};

bool RoomPresetParse(
	const char *pJson, int JsonLength, bool Challenge, CRoomPreset *pPreset, char *pError, int ErrorSize);
unsigned RoomPresetDifferenceMask(const CRoomPreset &A, const CRoomPreset &B);

enum ERoomPresetDifference
{
	ROOM_PRESET_DIFF_MODE = 1 << 0,
	ROOM_PRESET_DIFF_MAP = 1 << 1,
	ROOM_PRESET_DIFF_CAPACITY = 1 << 2,
	ROOM_PRESET_DIFF_DIFFICULTY = 1 << 3,
	ROOM_PRESET_DIFF_BOTS = 1 << 4,
	ROOM_PRESET_DIFF_RULES = 1 << 5,
	ROOM_PRESET_DIFF_SEED = 1 << 6,
	ROOM_PRESET_DIFF_PVE = 1 << 7,
};

#endif
