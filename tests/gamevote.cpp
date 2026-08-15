#include <game/client/room_creation.h>
#include <game/client/local_game_modes.h>
#include <game/questinfo.h>

#include <base/system.h>

#include <assert.h>
#include <stdio.h>

static void AssertImageMode(const char *pImage, int Expected)
{
	assert(LocalGameModeFromImage(pImage) == Expected);
}

int main()
{
	// Category assignment follows the local mode definitions.
	assert(LocalGameModeVoteCategory(LOCAL_MODE_INVASION) == GAMEVOTE_CATEGORY_PVE);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_HORDE) == GAMEVOTE_CATEGORY_PVE);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_EXTRACTION) == GAMEVOTE_CATEGORY_PVE);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_REACTOR_DEFENSE) == GAMEVOTE_CATEGORY_PVE);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_TDM) == GAMEVOTE_CATEGORY_PVP);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_CTF) == GAMEVOTE_CATEGORY_PVP);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_INSTAGIB_CTF) == GAMEVOTE_CATEGORY_PVP);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_REACTOR_ASSAULT) == GAMEVOTE_CATEGORY_PVP);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_BALL) == GAMEVOTE_CATEGORY_ARCADE);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_DM) == GAMEVOTE_CATEGORY_PVP);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_GRENADE_DM) == GAMEVOTE_CATEGORY_PVP);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_BATTLE_ROYALE) == GAMEVOTE_CATEGORY_PVP);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_ROAM) == GAMEVOTE_CATEGORY_ARCADE);
	assert(LocalGameModeVoteCategory(-1) == GAMEVOTE_CATEGORY_ARCADE);
	assert(LocalGameModeVoteCategory(LOCAL_MODE_COUNT) == GAMEVOTE_CATEGORY_ARCADE);

	// Server .vot thumbnail names resolve to the local mode.
	AssertImageMode("invasion1", LOCAL_MODE_INVASION);
	AssertImageMode("invasion7", LOCAL_MODE_HORDE);
	AssertImageMode("invasion6", LOCAL_MODE_EXTRACTION);
	AssertImageMode("reactor_def1", LOCAL_MODE_REACTOR_DEFENSE);
	AssertImageMode("dm1", LOCAL_MODE_DM);
	AssertImageMode("tdm1", LOCAL_MODE_TDM);
	AssertImageMode("ctf1", LOCAL_MODE_CTF);
	AssertImageMode("ictf1", LOCAL_MODE_INSTAGIB_CTF);
	AssertImageMode("grenade1", LOCAL_MODE_GRENADE_DM);
	AssertImageMode("br1", LOCAL_MODE_BATTLE_ROYALE);
	AssertImageMode("ball1", LOCAL_MODE_BALL);
	AssertImageMode("reactor1", LOCAL_MODE_REACTOR_ASSAULT);
	// Map variants shipped as separate .vot files map onto their base mode.
	AssertImageMode("invasion2", LOCAL_MODE_INVASION);
	AssertImageMode("invasion3", LOCAL_MODE_INVASION);
	AssertImageMode("invasion4", LOCAL_MODE_INVASION);
	AssertImageMode("invasion5", LOCAL_MODE_INVASION);
	AssertImageMode("invasion-endless", LOCAL_MODE_INVASION);
	AssertImageMode("ball2", LOCAL_MODE_BALL);
	// Unknown thumbnails resolve to no mode.
	assert(LocalGameModeFromImage("nonexistent") == -1);

	// Display order matches the room-creation mode picker (s_aAllLocalModes).
	// The count is intentionally coupled to s_aAllLocalModes: it must change
	// when modes are added/removed from the picker.
	const int AllCount = (int)(sizeof(s_aAllLocalModes) / sizeof(s_aAllLocalModes[0]));
	assert(AllCount == 14);
	for(int i = 0; i < AllCount; i++)
		assert(LocalGameModeSortKey(s_aAllLocalModes[i]) == i);
	// Modes outside the picker list sort after every listed mode.
	assert(LocalGameModeSortKey(LOCAL_MODE_TUTORIAL) == AllCount);
	assert(LocalGameModeSortKey(-1) == AllCount);
	assert(LocalGameModeSortKey(LOCAL_MODE_COUNT) == AllCount);

	// Every local mode thumbnail ships in data/gamevotes/ and maps back to its
	// own mode, except Roam Race and Expedition Invasion which share invasion1
	// with Invasion (the client disambiguates by vote name).
	for(int Mode = 1; Mode < LOCAL_MODE_COUNT; Mode++)
	{
		const CLocalGameMode &Spec = LocalGameMode(Mode);
		char aImagePath[128];
		snprintf(aImagePath, sizeof(aImagePath), "data/gamevotes/%s.png", Spec.m_pGameVoteImage);
		FILE *pImage = fopen(aImagePath, "rb");
		assert(pImage);
		fclose(pImage);
		const int Mapped = LocalGameModeFromImage(Spec.m_pGameVoteImage);
		if(Mode == LOCAL_MODE_ROAM || Mode == LOCAL_MODE_EXPEDITION)
			assert(Mapped == LOCAL_MODE_INVASION);
		else
			assert(Mapped == Mode);
	}

	// The base Invasion vote tier keeps a real level gate (min-level >= 1):
	// running Invasion at sv_mapgen_level 0 is unsafe, so the server filters
	// votes with an effective level of at least 1 instead of loosening the
	// vote data. This assert prevents the gate from being lowered to 0.
	{
		FILE *pVote = fopen("data/server/gamevotes/invasion1.vot", "rb");
		assert(pVote);
		char aLine[128];
		int MinLevel = -1;
		while(fgets(aLine, sizeof(aLine), pVote))
			if(sscanf(aLine, "min-level: %d", &MinLevel) == 1)
				break;
		fclose(pVote);
		assert(MinLevel >= 1);
	}

	// Vote config classification: only the base Invasion tier is offered in
	// the mode vote (voting into Invasion always starts at floor 1); the
	// higher tiers and non-Invasion configs must not be treated as it.
	assert(IsInvasionVoteConfig("cfg/invasion1"));
	assert(IsInvasionVoteConfig("cfg/invasion2"));
	assert(IsInvasionVoteConfig("cfg/invasion-endless"));
	assert(IsBaseInvasionVoteConfig("cfg/invasion1"));
	assert(!IsBaseInvasionVoteConfig("cfg/invasion2"));
	assert(!IsBaseInvasionVoteConfig("cfg/invasion-endless"));
	assert(!IsInvasionVoteConfig("cfg/horde_blueplanet"));
	assert(!IsInvasionVoteConfig("cfg/extract1"));
	assert(!IsInvasionVoteConfig("cfg/tdm_medium1"));
	assert(!IsInvasionVoteConfig(""));

	return 0;
}
