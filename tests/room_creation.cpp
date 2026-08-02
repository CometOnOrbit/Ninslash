#include <game/client/room_creation.h>
#include <game/client/local_game_modes.h>

#include <base/system.h>

#include <assert.h>
#include <stdio.h>

int main()
{
	const CRoomModeDefaults Expected[] = {{4, 5, 0, 0},
										  {6, 5, 0, 20},
										  {4, 5, 0, 4},
										  {5, 3, 4, 20},
										  {8, 8, 4, 50},
										  {8, 8, 4, 500},
										  {4, 14, 0, 0},
										  {8, 14, 6, 400},
										  {8, 8, 4, 5},
										  {9, 8, 8, 25},
										  {5, 3, 4, 15},
										  {8, 3, 4, 500},
										  {8, 8, 4, 24}};
	for(int Mode = 1; Mode < LOCAL_MODE_COUNT; Mode++)
	{
		const CRoomModeDefaults Actual = RoomModeDefaults(Mode);
		assert(Actual.m_Players == Expected[Mode - 1].m_Players);
		assert(Actual.m_Difficulty == Expected[Mode - 1].m_Difficulty);
		assert(Actual.m_Bots == Expected[Mode - 1].m_Bots);
		assert(Actual.m_Rule == Expected[Mode - 1].m_Rule);
	}
	assert(LOCAL_MODE_INVASION == 1 && LOCAL_MODE_CTF == 6);
	assert(LOCAL_MODE_REACTOR_DEFENSE == 7 && LOCAL_MODE_INSTAGIB_CTF == 12);
	assert(!LocalGameMode(LOCAL_MODE_ROAM).m_Pve);
	assert(RoomModeDefaults(LOCAL_MODE_ROAM).m_Bots == 4);
	assert(RoomModeDefaults(LOCAL_MODE_ROAM).m_Rule == 24);
	assert(LocalGameMode(LOCAL_MODE_ROAM).m_Rule == LOCAL_RULE_ROAM_CHECKPOINTS);
	assert((int)(sizeof(s_aAllLocalModes) / sizeof(s_aAllLocalModes[0])) == 13);
	assert(str_comp(LocalGameMode(LOCAL_MODE_REACTOR_DEFENSE).m_pGameVoteImage, "reactor_def1") == 0);
	assert(!LocalGameModeUsesTeamPopulation(LOCAL_MODE_DM));
	assert(!LocalGameModeUsesTeamPopulation(LOCAL_MODE_BATTLE_ROYALE));
	assert(LocalGameMode(LOCAL_MODE_BATTLE_ROYALE).m_MapGen);
	assert(LocalGameModeUsesTeamPopulation(LOCAL_MODE_TDM));
	assert(LocalGameModeUsesTeamPopulation(LOCAL_MODE_CTF));
	assert(LocalGameModeUsesTeamPopulation(LOCAL_MODE_REACTOR_ASSAULT));
	assert(LocalGameModeUsesTeamPopulation(LOCAL_MODE_BALL));
	assert(LocalGameModeUsesTeamPopulation(LOCAL_MODE_INSTAGIB_CTF));
	assert(str_comp(LocalGamePopulationLabel(LOCAL_MODE_DM), "Target active players") == 0);
	assert(str_comp(LocalGamePopulationLabel(LOCAL_MODE_TDM), "Target players per team") == 0);
	assert(str_comp(LocalGameRuleLabel(LOCAL_RULE_CTF_SCORE), "Score limit") == 0);
	assert(str_comp(LocalGameRuleLabel(LOCAL_RULE_BALL_SCORE), "Goal target") == 0);
	assert(str_comp(LocalGameRuleLabel(LOCAL_RULE_HORDE), "Target waves") == 0);
	assert(str_comp(LocalGameRuleLabel(LOCAL_RULE_EXTRACTION), "Mission time") == 0);
	assert(str_comp(LocalGameRuleLabel(LOCAL_RULE_ROAM_CHECKPOINTS), "Checkpoints") == 0);
	for(int Mode = 1; Mode < LOCAL_MODE_COUNT; Mode++)
	{
		const CLocalGameMode &Spec = LocalGameMode(Mode);
		assert(Spec.m_pConfig[0] && Spec.m_pGameType[0] && Spec.m_pGameVoteImage[0]);
		FILE *pConfig = fopen(Spec.m_pConfig, "rb");
		assert(pConfig);
		fclose(pConfig);
		char aImagePath[128];
		snprintf(aImagePath, sizeof(aImagePath), "data/gamevotes/%s.png", Spec.m_pGameVoteImage);
		FILE *pImage = fopen(aImagePath, "rb");
		assert(pImage);
		fclose(pImage);
		assert(Spec.m_MapCount > 0 && Spec.m_ppMapNames && Spec.m_ppMapCommands);
		assert(Spec.m_MapGen ==
			   (Mode != LOCAL_MODE_REACTOR_DEFENSE && Mode != LOCAL_MODE_REACTOR_ASSAULT && Mode != LOCAL_MODE_BALL));
	}

	assert(RoomHostKind(ROOM_VISIBILITY_SOLO) == ROOM_HOST_LOCAL);
	assert(RoomHostKind(ROOM_VISIBILITY_FRIENDS) == ROOM_HOST_STEAM_RELAY);
	assert(RoomHostKind(ROOM_VISIBILITY_LAN) == ROOM_HOST_LOCAL);
	assert(RoomHostKind(ROOM_VISIBILITY_PUBLIC) == ROOM_HOST_STEAM_RELAY);
	assert(RoomSlotsForVisibility(ROOM_VISIBILITY_SOLO, 8) == 1);
	assert(RoomSlotsForVisibility(ROOM_VISIBILITY_FRIENDS, 8) == 8);
	assert(RoomSlotsForVisibility(ROOM_VISIBILITY_LAN, 1) == 2);
	assert(RoomSlotsForVisibility(ROOM_VISIBILITY_PUBLIC, 20) == 16);
	assert(str_comp(InGameRoomActionLabel(false, false), "Create room") == 0);
	assert(str_comp(InGameRoomActionLabel(true, false), "Change mode") == 0);
	assert(str_comp(InGameRoomActionLabel(false, true), "Change mode") == 0);
	assert(InGameLeaveAction(false) == INGAME_LEAVE_DISCONNECT_TO_MENU);
	assert(InGameLeaveAction(true) == INGAME_LEAVE_OPEN_TUTORIAL_EXIT);
	assert(str_comp(RoomPrimaryActionLabel(ROOM_PRIMARY_CREATE), "Create and join") == 0);
	assert(str_comp(RoomPrimaryActionLabel(ROOM_PRIMARY_RESTART_LOCAL), "Restart with changes") == 0);
	assert(str_comp(RoomPrimaryActionLabel(ROOM_PRIMARY_RESTART_STEAM), "Restart with changes") == 0);
	assert(str_comp(RoomPrimaryActionLabel(ROOM_PRIMARY_STARTING_LOCAL), "Starting local game") == 0);
	assert(str_comp(RoomPrimaryActionLabel(ROOM_PRIMARY_STOPPING_LOCAL), "Stopping local game") == 0);
	assert(str_comp(RoomPrimaryActionLabel(ROOM_PRIMARY_CREATING_STEAM), "Creating room") == 0);
	assert(RoomPrimaryActionEnabled(ROOM_PRIMARY_CREATE));
	assert(RoomPrimaryActionEnabled(ROOM_PRIMARY_RESTART_STEAM));
	assert(!RoomPrimaryActionEnabled(ROOM_PRIMARY_STARTING_LOCAL));
	assert(!RoomPrimaryActionEnabled(ROOM_PRIMARY_STOPPING_LOCAL));
	assert(!RoomPrimaryActionEnabled(ROOM_PRIMARY_CREATING_STEAM));

	const CRoomConfigureLayout Narrow = RoomConfigureLayout(620.0f, 1.0f, true, 5, 3, true);
	assert(Narrow.m_SingleColumn);
	assert(Narrow.m_ContentHeight == 50.0f + Narrow.m_MainSettingsHeight + 8.0f + Narrow.m_IdentityHeight);
	assert(Narrow.m_ContentHeight > 400.0f); // Requires scrolling in the 800x600 Play view.
	const CRoomConfigureLayout NarrowScaled = RoomConfigureLayout(620.0f, 1.5f, true, 5, 3, true);
	assert(NarrowScaled.m_SingleColumn);
	assert(NarrowScaled.m_ContentHeight > 260.0f);
	assert(NarrowScaled.m_ContentHeight < Narrow.m_ContentHeight);
	const CRoomConfigureLayout Wide = RoomConfigureLayout(700.0f, 1.0f, false, 5, 3, true);
	assert(!Wide.m_SingleColumn);
	assert(Wide.m_ContentHeight ==
		   50.0f +
			   (Wide.m_MainSettingsHeight > Wide.m_IdentityHeight ? Wide.m_MainSettingsHeight : Wide.m_IdentityHeight));
	assert(Wide.m_MainSettingsHeight > Narrow.m_MainSettingsHeight); // Offline hint has reserved space.
	return 0;
}
