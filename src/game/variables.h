

#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times

// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict client movements")
MACRO_CONFIG_INT(
	ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Show name plates")
MACRO_CONFIG_INT(ClNameplatesAlways,
				 cl_nameplates_always,
				 1,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Always show name plates disregarding of distance")
MACRO_CONFIG_INT(ClNameplatesTeamcolors,
				 cl_nameplates_teamcolors,
				 1,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Use team colors for name plates")
MACRO_CONFIG_INT(ClNameplatesSize,
				 cl_nameplates_size,
				 40,
				 0,
				 100,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Size of the name plates from 0 to 100%")
MACRO_CONFIG_INT(ClAutoswitchWeapons,
				 cl_autoswitch_weapons,
				 0,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Auto switch weapon on pickup")

MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Show ingame HUD")
MACRO_CONFIG_INT(ClShowChatFriends,
				 cl_show_chat_friends,
				 0,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE,
				 "Show only chat messages from friends")
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame FPS counter")
MACRO_CONFIG_INT(
	ClHitFeedback, cl_hit_feedback, 70, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Hit feedback strength")
MACRO_CONFIG_INT(SndMusicVolume, snd_musicvolume, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music volume")
MACRO_CONFIG_INT(ClMovementFeedback,
				 cl_movement_feedback,
				 60,
				 0,
				 100,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Movement feedback strength")

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(
	ClThreadsoundloading, cl_threadsoundloading, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Load sound files threaded")

MACRO_CONFIG_INT(
	ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Warn about team balance")

MACRO_CONFIG_INT(ClLighting, cl_lighting, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Environmental lighting")

MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 0, 0, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 0, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseMaxDistance, cl_mouse_max_distance, 400, 0, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")

// MACRO_CONFIG_INT(ClFlow, cl_flow, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
// 0 = not started, 1 = in progress, 2 = complete, 3 = skipped.  Kept separate
// from account/progress storage so an offline player can resume the tutorial.
MACRO_CONFIG_INT(ClTutorialState,
				 cl_tutorial_state,
				 0,
				 0,
				 3,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "New player tutorial state")
MACRO_CONFIG_INT(ClTutorialCheckpoint,
				 cl_tutorial_checkpoint,
				 0,
				 0,
				 6,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "New player tutorial checkpoint")
MACRO_CONFIG_INT(ClTutorialActive, cl_tutorial_active, 0, 0, 1, CFGFLAG_CLIENT, "Show the local tutorial HUD")
MACRO_CONFIG_INT(ClTutorialVersion,
				 cl_tutorial_version,
				 0,
				 0,
				 99,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Tutorial content version")
MACRO_CONFIG_INT(ClTutorialChapter,
				 cl_tutorial_chapter,
				 1,
				 1,
				 6,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Tutorial chapter to resume")
MACRO_CONFIG_INT(
	ClTutorialStep, cl_tutorial_step, 0, 0, 9, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Tutorial step to resume")
MACRO_CONFIG_INT(ClTutorialCompletedMask,
				 cl_tutorial_completed_mask,
				 0,
				 0,
				 63,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Completed tutorial chapters")
MACRO_CONFIG_INT(ClTutorialPromptHandled,
				 cl_tutorial_prompt_handled,
				 0,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Initial tutorial prompt has been handled")
MACRO_CONFIG_INT(ClMotdTime,
				 cl_motd_time,
				 10,
				 0,
				 100,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE,
				 "How long to show the server message of the day")

MACRO_CONFIG_STR(ClVersionServer,
				 cl_version_server,
				 100,
				 "version.ninslash.com",
				 CFGFLAG_CLIENT | CFGFLAG_SAVE,
				 "Server to use to check for new versions")

MACRO_CONFIG_STR(ClLanguagefile,
				 cl_languagefile,
				 255,
				 "",
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "What language file to use")
MACRO_CONFIG_INT(ClLanguagecode,
				 cl_languagecode,
				 0,
				 0,
				 999,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Language code sent to the server")

MACRO_CONFIG_INT(PlayerColorBody,
				 player_color_body,
				 0,
				 0,
				 0xFFFFFF,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Player body color")
MACRO_CONFIG_INT(PlayerColorFeet,
				 player_color_feet,
				 0,
				 0,
				 0xFFFFFF,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Player feet color")
MACRO_CONFIG_INT(PlayerColorTopper,
				 player_color_topper,
				 65535,
				 0,
				 0xFFFFFF,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Player topper color")
MACRO_CONFIG_INT(PlayerColorSkin,
				 player_color_skin,
				 65535,
				 0,
				 0xFFFFFF,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Player skin color")
MACRO_CONFIG_STR(
	PlayerTopper, player_topper, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Player hair or hat")
MACRO_CONFIG_STR(PlayerEye, player_eye, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Player eyes")
MACRO_CONFIG_STR(PlayerHead, player_head, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Player head")
MACRO_CONFIG_STR(PlayerBody, player_body, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Player body")
MACRO_CONFIG_STR(PlayerHand, player_hand, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Player hand")
MACRO_CONFIG_STR(PlayerFoot, player_foot, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Player foot")
MACRO_CONFIG_INT(PlayerBloodColor, blood_color, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Blood color")

// MACRO_CONFIG_INT(UiPage, ui_page, 6, 0, 10, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface page")
MACRO_CONFIG_INT(UiPage, ui_page, 1, 0, 25, CFGFLAG_CLIENT, "Interface page")
MACRO_CONFIG_INT(UiToolboxPage, ui_toolbox_page, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toolbox page")
MACRO_CONFIG_STR(
	UiServerAddress, ui_server_address, 64, "localhost:8303", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface server address")
MACRO_CONFIG_STR(ClModHash,
				 cl_mod_hash,
				 65,
				 "",
				 CFGFLAG_CLIENT | CFGFLAG_SAVE,
				 "Installed Workshop mod collection hash sent during connection")
MACRO_CONFIG_STR(ClModIds,
					 cl_mod_ids,
					 1024,
					 "",
					 CFGFLAG_CLIENT | CFGFLAG_SAVE,
					 "Comma-separated enabled Workshop root PublishedFileIDs")
MACRO_CONFIG_STR(ClChallengeScript,
					 cl_challenge_script,
					 256,
					 "",
					 CFGFLAG_CLIENT | CFGFLAG_SAVE,
					 "Local restricted Lua challenge script used for prediction")
MACRO_CONFIG_STR(ClChallengeContentHash,
					 cl_challenge_content_hash,
					 65,
					 "",
					 CFGFLAG_CLIENT | CFGFLAG_SAVE,
					 "Canonical hash of the local challenge script package")
MACRO_CONFIG_INT(ClAutoScreenshotSteam,
				 cl_auto_screenshot_steam,
				 0,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Also add automatic post-match screenshots to the Steam library")
MACRO_CONFIG_INT(ClSteamGyro,
				 cl_steam_gyro,
				 1,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Enable Steam Input gyroscope aiming")
MACRO_CONFIG_INT(ClSteamGyroSensitivity,
				 cl_steam_gyro_sensitivity,
				 100,
				 1,
				 1000,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Steam Input gyroscope sensitivity")
MACRO_CONFIG_INT(ClSteamGyroInvert,
				 cl_steam_gyro_invert,
				 0,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Invert Steam Input gyroscope vertical aim")
MACRO_CONFIG_INT(ClSteamRumble,
				 cl_steam_rumble,
				 1,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Enable Steam Input vibration")
MACRO_CONFIG_INT(ClGamepadMoveDeadzone, cl_gamepad_move_deadzone, 35, 10, 80, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Gamepad movement deadzone percentage")
MACRO_CONFIG_INT(ClGamepadAimDeadzone, cl_gamepad_aim_deadzone, 18, 0, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Gamepad aim inner deadzone percentage")
MACRO_CONFIG_INT(ClGamepadAimCurve, cl_gamepad_aim_curve, 150, 50, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Gamepad aim response curve times 100")
MACRO_CONFIG_INT(ClGamepadAimSensitivity, cl_gamepad_aim_sensitivity, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Gamepad aim sensitivity percentage")
MACRO_CONFIG_INT(ClGamepadInvertY, cl_gamepad_invert_y, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Invert gamepad vertical aim")
MACRO_CONFIG_INT(ClGamepadAimAssist, cl_gamepad_aim_assist, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Gamepad aim assist strength; fresh profiles default to 35")
MACRO_CONFIG_INT(ClInputDebug, cl_input_debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show processed input diagnostics")
MACRO_CONFIG_INT(UiSettingsPage, ui_settings_page, 0, 0, 7, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Last settings category")
MACRO_CONFIG_INT(UiAdvancedSettings,
				 ui_advanced_settings,
				 0,
				 0,
				 1,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE,
				 "Show advanced interface settings")
MACRO_CONFIG_INT(UiInputPage, ui_input_page, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keyboard or controller input settings page")
MACRO_CONFIG_INT(UiScale, ui_scale, 100, 50, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Interface scale")
MACRO_CONFIG_INT(
	UiMousesens, ui_mousesens, 100, 5, 100000, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")

MACRO_CONFIG_INT(
	UiColorHue, ui_color_hue, 150, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Interface color hue")
MACRO_CONFIG_INT(
	UiColorSat, ui_color_sat, 16, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Interface color saturation")
MACRO_CONFIG_INT(
	UiColorLht, ui_color_lht, 188, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Interface color lightness")
MACRO_CONFIG_INT(
	UiColorAlpha, ui_color_alpha, 222, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Interface alpha")
MACRO_CONFIG_INT(UiColorHue2,
				 ui_color_hue2,
				 150,
				 0,
				 255,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Secondary interface color hue")
MACRO_CONFIG_INT(UiColorSat2,
				 ui_color_sat2,
				 10,
				 0,
				 255,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Secondary interface color saturation")
MACRO_CONFIG_INT(UiColorLht2,
				 ui_color_lht2,
				 128,
				 0,
				 255,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Secondary interface color lightness")
MACRO_CONFIG_INT(UiColorAlpha2,
				 ui_color_alpha2,
				 190,
				 0,
				 255,
				 CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD,
				 "Secondary interface alpha")
MACRO_CONFIG_INT(ClMenuAlpha, cl_menu_alpha, 75, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_CLOUD, "Menu opacity")
MACRO_CONFIG_INT(UiWideview, ui_wideview, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Extended menus GUI")
MACRO_CONFIG_INT(UiSidebar, ui_sidebar, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show server browser sidebar")
MACRO_CONFIG_INT(
	ClVideoFps, cl_video_fps, 30, 1, 120, CFGFLAG_CLIENT | CFGFLAG_SAVE, "FPS used when rendering demos to video")

MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable clipping")

// server
MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds to do warmup before round starts")
MACRO_CONFIG_INT(
	SvSteamAuth, sv_steam_auth, 1, 0, 2, CFGFLAG_SERVER, "Steam authentication: 0=off, 1=optional, 2=required")
MACRO_CONFIG_INT(
	SvOfficial, sv_official, 0, 0, 1, CFGFLAG_SERVER, "Official servers require Steam auth and disable Workshop mods")
MACRO_CONFIG_INT(SvRegisterSteam,
				 sv_register_steam,
				 1,
				 0,
				 1,
				 CFGFLAG_SERVER,
				 "Advertise this UDP server in the Steam GameServer list when Steamworks is available")
MACRO_CONFIG_STR(SvModHash, sv_mod_hash, 65, "", 0, "Internal derived Mod collection hash")
MACRO_CONFIG_STR(SvModIds, sv_mod_ids, 1024, "", 0, "Internal derived root Mod PublishedFileIDs")
MACRO_CONFIG_STR(SvChallengeScript,
					 sv_challenge_script,
					 256,
					 "",
					 CFGFLAG_SERVER,
					 "Restricted Lua challenge script path")
MACRO_CONFIG_STR(SvChallengeContentHash,
					 sv_challenge_content_hash,
					 65,
					 "",
					 CFGFLAG_SERVER,
					 "Canonical challenge content-package hash")
MACRO_CONFIG_STR(SvModWhitelist,
				 sv_mod_whitelist,
				 1024,
				 "",
				 CFGFLAG_SERVER,
				 "Comma-separated Workshop PublishedFileIDs allowed by this server; empty allows all")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvTeamdamage, sv_teamdamage, 0, 0, 1, CFGFLAG_SERVER, "Team damage")
MACRO_CONFIG_INT(
	SvRoundsPerMap, sv_rounds_per_map, 1, 1, 100, CFGFLAG_SERVER, "Number of rounds on each map before rotating")
MACRO_CONFIG_INT(SvRoundSwap, sv_round_swap, 1, 0, 1, CFGFLAG_SERVER, "Swap teams between rounds")
MACRO_CONFIG_INT(SvPowerups, sv_powerups, 1, 0, 1, CFGFLAG_SERVER, "Allow powerups like ninja")
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 0, 0, 1000, CFGFLAG_SERVER, "Score limit (0 disables)")
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SERVER, "Time limit in minutes (0 disables)")
MACRO_CONFIG_STR(SvGametype,
				 sv_gametype,
				 32,
				 "dm",
				 CFGFLAG_SERVER,
				 "Game type (dm, tdm, ctf, base, coop, tutorial, horde, extract, ball, roam)")
MACRO_CONFIG_STR(SvPvpProfile,
				 sv_pvp_profile,
				 32,
				 "",
				 CFGFLAG_SERVER,
				 "PvP balance profile (empty selects the controller game type)")
MACRO_CONFIG_INT(SvTournamentMode,
				 sv_tournament_mode,
				 0,
				 0,
				 1,
				 CFGFLAG_SERVER,
				 "Tournament mode. When enabled, players joins the server as spectator")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection")

MACRO_CONFIG_INT(SvRespawnDelay, sv_respawn_delay, 1, 0, 10, CFGFLAG_SERVER, "Time needed to respawn after death")

MACRO_CONFIG_INT(SvSpectatorSlots,
				 sv_spectator_slots,
				 0,
				 0,
				 MAX_CLIENTS,
				 CFGFLAG_SERVER,
				 "Number of slots to reserve for spectators")
MACRO_CONFIG_INT(SvTeambalanceTime,
				 sv_teambalance_time,
				 1,
				 0,
				 1000,
				 CFGFLAG_SERVER,
				 "How many minutes to wait before autobalancing teams")
MACRO_CONFIG_INT(SvInactiveKickTime,
				 sv_inactivekick_time,
				 3,
				 0,
				 1000,
				 CFGFLAG_SERVER,
				 "How many minutes to wait before taking care of inactive players")
MACRO_CONFIG_INT(SvInactiveKick,
				 sv_inactivekick,
				 1,
				 0,
				 2,
				 CFGFLAG_SERVER,
				 "How to deal with inactive players (0=move to spectator, 1=move to free spectator slot/kick, 2=kick)")

MACRO_CONFIG_INT(
	SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(
	SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay,
				 sv_vote_spectate_rejoindelay,
				 3,
				 0,
				 1000,
				 CFGFLAG_SERVER,
				 "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin,
				 sv_vote_kick_min,
				 0,
				 0,
				 MAX_CLIENTS,
				 CFGFLAG_SERVER,
				 "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime,
				 sv_vote_kick_bantime,
				 5,
				 0,
				 1440,
				 CFGFLAG_SERVER,
				 "The time to ban a player if kicked by vote. 0 makes it just use kick")

//

MACRO_CONFIG_INT(SvNull, sv_null, 0, 0, 100, CFGFLAG_SERVER, "does nothing")

MACRO_CONFIG_INT(SvDebugMessages, sv_debugmessages, 0, 0, 1, CFGFLAG_SERVER, "Enable debug messages for crash fixing")

MACRO_CONFIG_INT(SvEnableBuilding, sv_enablebuilding, 0, 0, 1, CFGFLAG_SERVER, "Enable building")
MACRO_CONFIG_INT(SvChallengeVariants, sv_challenge_variants, 0, 0, 255, CFGFLAG_SERVER, "Challenge variant bitmask")
MACRO_CONFIG_INT(ClChallengeVariants, cl_challenge_variants, 0, 0, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Challenge variant bitmask (local games)")
MACRO_CONFIG_INT(SvRandomWeapons, sv_randomweapons, 0, 0, 1, CFGFLAG_SERVER, "Enable random weapons")
MACRO_CONFIG_INT(SvWeaponSpawns, sv_weaponspawns, 1, 0, 1, CFGFLAG_SERVER, "Enable weapon spawning")
MACRO_CONFIG_INT(SvLaserWeapon, sv_laserweapon, 0, 0, 1, CFGFLAG_SERVER, "Give laser weapon on spawn")
MACRO_CONFIG_INT(SvSurvivalMode,
				 sv_survivalmode,
				 0,
				 0,
				 2,
				 CFGFLAG_SERVER,
				 "Survival mode (0=free respawn, 1+=no auto-respawn; coop: revive via Respawn device / next floor, "
				 "wipe when all dead)")
MACRO_CONFIG_INT(SvSurvivalAcid, sv_survivalacid, 1, 0, 1, CFGFLAG_SERVER, "Survival ends with rising acid")
MACRO_CONFIG_INT(SvSurvivalTime, sv_survivaltime, 0, 0, 600, CFGFLAG_SERVER, "Survival round time limit")
MACRO_CONFIG_INT(
	SvSurvivalReward, sv_survivalreward, 5, 0, 1000, CFGFLAG_SERVER, "Survival round winner's reward points")
MACRO_CONFIG_INT(SvAbilities, sv_abilities, 0, 0, 1, CFGFLAG_SERVER, "Enable classes & abilities")
MACRO_CONFIG_INT(SvPickupDrops, sv_pickupdrops, 1, 0, 1, CFGFLAG_SERVER, "Pickup drops")
MACRO_CONFIG_INT(SvHealthPickups, sv_healthpickups, 1, 0, 1, CFGFLAG_SERVER, "Enable hp and armor pickups")
MACRO_CONFIG_INT(SvWeaponDrops, sv_weapondrops, 1, 0, 1, CFGFLAG_SERVER, "Enable weapon drops")
MACRO_CONFIG_INT(SvNumBots,
				 sv_bots,
				 4,
				 0,
				 30,
				 CFGFLAG_SERVER,
				 "AI population target: total active players in free-for-all modes or players per team in team modes; "
				 "0 disables bots")
MACRO_CONFIG_INT(SvNoBotTeam, sv_nobotteam, -1, -1, 9, CFGFLAG_SERVER, "")
MACRO_CONFIG_INT(SvBotLevel, sv_botlevel, 6, 1, 30, CFGFLAG_SERVER, "AI level of bots")
MACRO_CONFIG_INT(SvUnlimitedTurbo, sv_unlimited_turbo, 0, 0, 1, CFGFLAG_SERVER, "Unlimited turbo")
MACRO_CONFIG_INT(SvOneHitKill, sv_one_hit_kill, 0, 0, 1, CFGFLAG_SERVER, "One hit kills")
MACRO_CONFIG_INT(SvSelfKillPenalty, sv_selfkillpenalty, 1, 0, 1, CFGFLAG_SERVER, "Penalty for self kills")

MACRO_CONFIG_INT(SvSpectatorUpdateTime,
				 sv_spectatorupdatetime,
				 5,
				 1,
				 20,
				 CFGFLAG_SERVER,
				 "Time between spectator view changes to spectators")
MACRO_CONFIG_INT(SvSpectateOnlyHumans, sv_spectateonlyhumans, 0, 0, 1, CFGFLAG_SERVER, "Spectate only humans")

MACRO_CONFIG_INT(SvDisablePVP, sv_disablepvp, 0, 0, 1, CFGFLAG_SERVER, "Disable PvP damage")
MACRO_CONFIG_INT(SvEnableVotingMenu, sv_enablevotingmenu, 1, 0, 1, CFGFLAG_SERVER, "Enable shopping in voting menu")

MACRO_CONFIG_INT(SvBotsSkipPickups, sv_botsskippickups, 0, 1, 1, CFGFLAG_SERVER, "Bots skips pickups")

MACRO_CONFIG_INT(SvAutoBalance, sv_autobalance, 0, 1, 1, CFGFLAG_SERVER, "Auto balance")
MACRO_CONFIG_INT(SvNoBotNames, sv_nobotnames, 0, 0, 1, CFGFLAG_SERVER, "Hide bot names")
MACRO_CONFIG_INT(SvNearHumanRespawn, sv_nearhumanrespawn, 0, 0, 1, CFGFLAG_SERVER, "Bots respawn near human players")

MACRO_CONFIG_INT(SvNumRounds, sv_numrounds, 7, 1, 100, CFGFLAG_SERVER, "Number of rounds")

// AI
MACRO_CONFIG_INT(SvBotReactTime, sv_bot_react_time, 6, 0, 20, CFGFLAG_SERVER, "Time bot takes to start shooting")
MACRO_CONFIG_INT(SvGodBots, sv_godbots, 0, 0, 1, CFGFLAG_SERVER, "Hard bots")
MACRO_CONFIG_INT(SvRobots, sv_robots, 0, 0, 1, CFGFLAG_SERVER, "Robot bot skins")

MACRO_CONFIG_INT(SvStartGold, sv_startgold, 0, 0, 999, CFGFLAG_SERVER, "Starting gold")
MACRO_CONFIG_INT(SvForgeMode,
				 sv_forge_mode,
				 1,
				 0,
				 3,
				 CFGFLAG_SERVER,
				 "Forge mode (0=legacy inventory, 1=anywhere, 2=screen only, 3=upgrade drag anywhere + other forge at screen)")
MACRO_CONFIG_INT(SvForgeBaseCost, sv_forge_base_cost, 5, -999, 999, CFGFLAG_SERVER, "Base gold cost for forging")
MACRO_CONFIG_INT(SvForgeLevelCost,
				 sv_forge_level_cost,
				 2,
				 -999,
				 999,
				 CFGFLAG_SERVER,
				 "Gold cost per combined weapon level for forging")
MACRO_CONFIG_INT(SvTutorialMode, sv_tutorial_mode, 0, 0, 1, CFGFLAG_SERVER, "Deterministic local tutorial rules")
MACRO_CONFIG_INT(SvTutorialChapter, sv_tutorial_chapter, 1, 1, 6, CFGFLAG_SERVER, "Tutorial chapter being hosted")
MACRO_CONFIG_INT(SvTutorialStep, sv_tutorial_step, 0, 0, 9, CFGFLAG_SERVER, "Tutorial step being hosted")
MACRO_CONFIG_INT(SvTutorialCompletedMask,
				 sv_tutorial_completed_mask,
				 0,
				 0,
				 63,
				 CFGFLAG_SERVER,
				 "Tutorial chapters already completed")

//
MACRO_CONFIG_INT(SvInfiniteGrenades, sv_infinitegrenades, 0, 0, 1, CFGFLAG_SERVER, "Infinite grenades")

MACRO_CONFIG_INT(SvBroadcastLock, sv_broadcastlock, 3, 0, 5, CFGFLAG_SERVER, "Broadcast lock time (seconds)")

MACRO_CONFIG_INT(
	SvRandomMaps, sv_random_maps, 1, 0, 1, CFGFLAG_SERVER, "Random select map in maps list (1 = on, 0 = off)")

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, 15, CFGFLAG_SERVER, "")
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(ClDebugWeaponWheel,
				 cl_debug_weapon_wheel,
				 0,
				 0,
				 1,
				 CFGFLAG_CLIENT,
				 "Log mouse-wheel weapon switch input and confirmed slots")

MACRO_CONFIG_INT(ClZoom,
					 cl_zoom,
					 10,
					 0,
					 30,
					 CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_CLOUD,
					 "Camera zoom level (10 = default, lower = more zoomed in)")
MACRO_CONFIG_INT(ClSpectatorDirector,
					 cl_spectator_director,
					 0,
					 0,
					 1,
					 CFGFLAG_SAVE | CFGFLAG_CLIENT,
					 "Automatically follow the killer while spectating")
#endif
