#ifndef GAME_CLIENT_ROOM_CREATION_H
#define GAME_CLIENT_ROOM_CREATION_H

enum ERoomVisibility
{
	ROOM_VISIBILITY_SOLO = 0,
	ROOM_VISIBILITY_FRIENDS,
	ROOM_VISIBILITY_LAN,
	ROOM_VISIBILITY_PUBLIC,
};

enum ERoomHostKind
{
	ROOM_HOST_LOCAL,
	ROOM_HOST_STEAM_RELAY,
};

enum EInGameLeaveAction
{
	INGAME_LEAVE_DISCONNECT_TO_MENU,
	INGAME_LEAVE_OPEN_TUTORIAL_EXIT,
};

enum ERoomPrimaryActionState
{
	ROOM_PRIMARY_CREATE,
	ROOM_PRIMARY_RESTART_LOCAL,
	ROOM_PRIMARY_STARTING_LOCAL,
	ROOM_PRIMARY_STOPPING_LOCAL,
	ROOM_PRIMARY_CREATING_STEAM,
	ROOM_PRIMARY_RESTART_STEAM,
};

struct CRoomModeDefaults
{
	int m_Players;
	int m_Difficulty;
	int m_Bots;
	int m_Rule;
};

inline CRoomModeDefaults RoomModeDefaults(int Mode)
{
	static const CRoomModeDefaults s_aDefaults[] = {
		{1, 1, 0, 0}, // Tutorial, unavailable in normal room creation.
		{4, 5, 0, 0},
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
	};
	if(Mode < 1 || Mode > 12)
		Mode = 1;
	return s_aDefaults[Mode];
}

inline bool RoomVisibilityRequiresSteam(int Visibility)
{
	return Visibility == ROOM_VISIBILITY_FRIENDS || Visibility == ROOM_VISIBILITY_PUBLIC;
}

inline ERoomHostKind RoomHostKind(int Visibility)
{
	return RoomVisibilityRequiresSteam(Visibility) ? ROOM_HOST_STEAM_RELAY : ROOM_HOST_LOCAL;
}

inline int RoomSlotsForVisibility(int Visibility, int MultiplayerSlots)
{
	if(Visibility == ROOM_VISIBILITY_SOLO)
		return 1;
	return MultiplayerSlots < 2 ? 2 : MultiplayerSlots > 16 ? 16 : MultiplayerSlots;
}

inline const char *InGameRoomActionLabel(bool ManagedLocalGameActive, bool SteamHostedGameActive)
{
	return ManagedLocalGameActive || SteamHostedGameActive ? "Change mode" : "Create room";
}

inline EInGameLeaveAction InGameLeaveAction(bool TutorialActive)
{
	return TutorialActive ? INGAME_LEAVE_OPEN_TUTORIAL_EXIT : INGAME_LEAVE_DISCONNECT_TO_MENU;
}

inline const char *RoomPrimaryActionLabel(int State)
{
	if(State == ROOM_PRIMARY_RESTART_LOCAL || State == ROOM_PRIMARY_RESTART_STEAM)
		return "Restart with changes";
	if(State == ROOM_PRIMARY_STARTING_LOCAL)
		return "Starting local game";
	if(State == ROOM_PRIMARY_STOPPING_LOCAL)
		return "Stopping local game";
	if(State == ROOM_PRIMARY_CREATING_STEAM)
		return "Creating room";
	return "Create and join";
}

inline bool RoomPrimaryActionEnabled(int State)
{
	return State != ROOM_PRIMARY_CREATING_STEAM && State != ROOM_PRIMARY_STARTING_LOCAL && State != ROOM_PRIMARY_STOPPING_LOCAL;
}

#endif
