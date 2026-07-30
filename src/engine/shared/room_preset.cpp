#include "room_preset.h"
#include "content_manifest.h"

#include <base/system.h>
#include <engine/external/json-parser/json.h>

namespace
{
bool Fail(char *pError, int ErrorSize, const char *pText)
{
	if(pError && ErrorSize > 0)
		str_copy(pError, pText, ErrorSize);
	return false;
}
bool Integer(const json_value &Value, int Min, int Max)
{
	return Value.type == json_integer && Value.u.integer >= Min && Value.u.integer <= Max;
}
bool Boolean(const json_value &Value)
{
	return Value.type == json_boolean;
}
bool Forbidden(const json_value &Root)
{
	const char *apKeys[] = {"password", "port", "server_name", "visibility", "player_progress"};
	for(unsigned i = 0; i < sizeof(apKeys) / sizeof(apKeys[0]); i++)
		if(Root[apKeys[i]].type != json_none)
			return true;
	return false;
}
} // namespace

bool RoomPresetParse(
	const char *pJson, int JsonLength, bool Challenge, CRoomPreset *pPreset, char *pError, int ErrorSize)
{
	if(!pJson || JsonLength <= 0 || JsonLength > 64 * 1024 || !pPreset)
		return Fail(pError, ErrorSize, "invalid room preset");
	json_settings Settings;
	mem_zero(&Settings, sizeof(Settings));
	char aJsonError[128];
	json_value *pRoot = json_parse_ex(&Settings, pJson, JsonLength, aJsonError);
	if(!pRoot || pRoot->type != json_object || Forbidden(*pRoot))
	{
		if(pRoot)
			json_value_free(pRoot);
		return Fail(pError, ErrorSize, "room preset contains invalid or private fields");
	}
	const json_value &GameType = (*pRoot)["gametype"], &Map = (*pRoot)["map"], &MaxPlayers = (*pRoot)["max_players"],
					 &Difficulty = (*pRoot)["difficulty"], &Bots = (*pRoot)["bots"], &BotLevel = (*pRoot)["bot_level"],
					 &ModeRule = (*pRoot)["mode_rule"], &MapGen = (*pRoot)["mapgen"],
					 &MapGenLevel = (*pRoot)["mapgen_level"], &RandomSeed = (*pRoot)["random_seed"],
					 &Seed = (*pRoot)["seed"], &Roguelite = (*pRoot)["roguelite"], &Contracts = (*pRoot)["contracts"],
					 &InvasionStart = (*pRoot)["invasion_start"], &InvasionFloor = (*pRoot)["invasion_floor"];
	const bool Valid = GameType.type == json_string && ((const char *)GameType)[0] && Map.type == json_string &&
					   ContentManifestIsSafeRelativePath((const char *)Map) && Integer(MaxPlayers, 1, 64) &&
					   Integer(Difficulty, 1, 50) && Integer(Bots, 0, 64) && Integer(BotLevel, 1, 30) &&
					   Integer(ModeRule, 0, 10000) && Boolean(MapGen) && Integer(MapGenLevel, 0, 9999) &&
					   Boolean(RandomSeed) && Integer(Seed, 0, 2147483647) && Boolean(Roguelite) &&
					   Boolean(Contracts) && Integer(InvasionStart, 0, 2) && Integer(InvasionFloor, 1, 9999);
	if(!Valid || Bots.u.integer >= MaxPlayers.u.integer || (Challenge && RandomSeed.u.boolean))
	{
		json_value_free(pRoot);
		return Fail(pError,
					ErrorSize,
					Challenge ? "challenge requires a fixed seed and locked valid rules" : "invalid room preset field");
	}
	mem_zero(pPreset, sizeof(*pPreset));
	str_copy(pPreset->m_aGameType, (const char *)GameType, sizeof(pPreset->m_aGameType));
	str_copy(pPreset->m_aMapLocator, (const char *)Map, sizeof(pPreset->m_aMapLocator));
	pPreset->m_MaxPlayers = (int)MaxPlayers.u.integer;
	pPreset->m_Difficulty = (int)Difficulty.u.integer;
	pPreset->m_Bots = (int)Bots.u.integer;
	pPreset->m_BotLevel = (int)BotLevel.u.integer;
	pPreset->m_ModeRule = (int)ModeRule.u.integer;
	pPreset->m_MapGen = MapGen.u.boolean;
	pPreset->m_MapGenLevel = (int)MapGenLevel.u.integer;
	pPreset->m_RandomSeed = RandomSeed.u.boolean;
	pPreset->m_Seed = (int)Seed.u.integer;
	pPreset->m_Roguelite = Roguelite.u.boolean;
	pPreset->m_Contracts = Contracts.u.boolean;
	pPreset->m_InvasionStart = (int)InvasionStart.u.integer;
	pPreset->m_InvasionFloor = (int)InvasionFloor.u.integer;
	json_value_free(pRoot);
	return true;
}

unsigned RoomPresetDifferenceMask(const CRoomPreset &A, const CRoomPreset &B)
{
	unsigned Result = 0;
	if(str_comp(A.m_aGameType, B.m_aGameType))
		Result |= ROOM_PRESET_DIFF_MODE;
	if(str_comp(A.m_aMapLocator, B.m_aMapLocator) || A.m_MapGen != B.m_MapGen || A.m_MapGenLevel != B.m_MapGenLevel)
		Result |= ROOM_PRESET_DIFF_MAP;
	if(A.m_MaxPlayers != B.m_MaxPlayers)
		Result |= ROOM_PRESET_DIFF_CAPACITY;
	if(A.m_Difficulty != B.m_Difficulty)
		Result |= ROOM_PRESET_DIFF_DIFFICULTY;
	if(A.m_Bots != B.m_Bots || A.m_BotLevel != B.m_BotLevel)
		Result |= ROOM_PRESET_DIFF_BOTS;
	if(A.m_ModeRule != B.m_ModeRule)
		Result |= ROOM_PRESET_DIFF_RULES;
	if(A.m_RandomSeed != B.m_RandomSeed || A.m_Seed != B.m_Seed)
		Result |= ROOM_PRESET_DIFF_SEED;
	if(A.m_Roguelite != B.m_Roguelite || A.m_Contracts != B.m_Contracts || A.m_InvasionStart != B.m_InvasionStart ||
	   A.m_InvasionFloor != B.m_InvasionFloor)
		Result |= ROOM_PRESET_DIFF_PVE;
	return Result;
}
