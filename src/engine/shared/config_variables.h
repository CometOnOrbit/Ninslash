

#ifndef ENGINE_SHARED_CONFIG_VARIABLES_H
#define ENGINE_SHARED_CONFIG_VARIABLES_H
#undef ENGINE_SHARED_CONFIG_VARIABLES_H // this file will be included several times

// TODO: remove this
#include "././game/variables.h" 


MACRO_CONFIG_STR(PlayerName, player_name, 16, "bloodless", CFGFLAG_SAVE|CFGFLAG_CLIENT, "Name of the player")
MACRO_CONFIG_STR(PlayerClan, player_clan, 12, "", CFGFLAG_SAVE|CFGFLAG_CLIENT, "Clan of the player")
MACRO_CONFIG_INT(PlayerCountry, player_country, -1, -1, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Country of the player")
MACRO_CONFIG_STR(Password, password, 32, "", CFGFLAG_CLIENT|CFGFLAG_SERVER, "Password to the server")
MACRO_CONFIG_STR(Logfile, logfile, 128, "", CFGFLAG_SAVE|CFGFLAG_CLIENT|CFGFLAG_SERVER, "Filename to log all output to")
MACRO_CONFIG_INT(ConsoleOutputLevel, console_output_level, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SERVER, "Adjusts the amount of information in the console")

MACRO_CONFIG_INT(ClCpuThrottle, cl_cpu_throttle, 0, 0, 100, CFGFLAG_SAVE|CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(ClEditor, cl_editor, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(ClLoadCountryFlags, cl_load_country_flags, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Load and show country flags")

MACRO_CONFIG_INT(ClAutoDemoRecord, cl_auto_demo_record, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Automatically record demos")
MACRO_CONFIG_INT(ClAutoDemoMax, cl_auto_demo_max, 10, 0, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Maximum number of automatically recorded demos (0 = no limit)")
MACRO_CONFIG_INT(ClAutoScreenshot, cl_auto_screenshot, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Automatically take game over screenshot")
MACRO_CONFIG_INT(ClAutoScreenshotMax, cl_auto_screenshot_max, 10, 0, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Maximum number of automatically created screenshots (0 = no limit)")

MACRO_CONFIG_INT(ClEventthread, cl_eventthread, 0, 0, 1, CFGFLAG_CLIENT, "Enables the usage of a thread to pump the events")

MACRO_CONFIG_INT(InpGrab, inp_grab, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Use forceful input grabbing method")

MACRO_CONFIG_STR(BrFilterString, br_filter_string, 25, "", CFGFLAG_SAVE|CFGFLAG_CLIENT, "Server browser filtering string")
MACRO_CONFIG_INT(BrFilterFull, br_filter_full, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out full server in browser")
MACRO_CONFIG_INT(BrFilterEmpty, br_filter_empty, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out empty server in browser")
MACRO_CONFIG_INT(BrFilterSpectators, br_filter_spectators, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out spectators from player numbers")
MACRO_CONFIG_INT(BrFilterFriends, br_filter_friends, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out servers with no friends")
MACRO_CONFIG_INT(BrFilterCountry, br_filter_country, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out servers with non-matching player country")
MACRO_CONFIG_INT(BrFilterCountryIndex, br_filter_country_index, -1, -1, 999, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Player country to filter by in the server browser")
MACRO_CONFIG_INT(BrFilterPw, br_filter_pw, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out password protected servers in browser")
MACRO_CONFIG_INT(BrFilterPing, br_filter_ping, 999, 0, 999, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Ping to filter by in the server browser")
MACRO_CONFIG_STR(BrFilterGametype, br_filter_gametype, 128, "", CFGFLAG_SAVE|CFGFLAG_CLIENT, "Game types to filter")
MACRO_CONFIG_INT(BrFilterGametypeStrict, br_filter_gametype_strict, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Strict gametype filter")
MACRO_CONFIG_STR(BrFilterServerAddress, br_filter_serveraddress, 128, "", CFGFLAG_SAVE|CFGFLAG_CLIENT, "Server address to filter")
MACRO_CONFIG_INT(BrFilterPure, br_filter_pure, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out non-standard servers in browser")
MACRO_CONFIG_INT(BrFilterPureMap, br_filter_pure_map, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out non-standard maps in browser")
MACRO_CONFIG_INT(BrFilterCompatversion, br_filter_compatversion, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Filter out non-compatible servers in browser")

MACRO_CONFIG_INT(BrSort, br_sort, 0, 0, 256, CFGFLAG_SAVE|CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(BrSortOrder, br_sort_order, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(BrMaxRequests, br_max_requests, 25, 0, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Number of requests to use when refreshing server browser")

MACRO_CONFIG_INT(SndBufferSize, snd_buffer_size, 512, 128, 32768, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Sound buffer size")
MACRO_CONFIG_INT(SndRate, snd_rate, 48000, 0, 0, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Sound mixing rate")
MACRO_CONFIG_INT(SndEnable, snd_enable, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Sound enable")
MACRO_CONFIG_INT(SndMusic, snd_enable_music, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Play background music")
MACRO_CONFIG_INT(SndVolume, snd_volume, 100, 0, 100, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Sound volume")

MACRO_CONFIG_INT(SndEnvironmental, snd_environmental, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Environmental sounds")

MACRO_CONFIG_INT(GfxScreen, gfx_screen, 0, 0, 0, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Screen index")

MACRO_CONFIG_INT(GamepadID, gamepadid, -1, 9999, -1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Gamepad to use")



MACRO_CONFIG_INT(GoreBlood, gore_blood, 10, 0, 100, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Blood amount")
MACRO_CONFIG_INT(GoreWallSplatter, gore_wallsplatter, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Wall splatter enable")

MACRO_CONFIG_INT(SndNonactiveMute, snd_nonactive_mute, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "")

MACRO_CONFIG_INT(GfxScreenWidth, gfx_screen_width, 0, 0, 0, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Screen resolution width")
MACRO_CONFIG_INT(GfxScreenHeight, gfx_screen_height, 0, 0, 0, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Screen resolution height")
MACRO_CONFIG_INT(GfxBorderless, gfx_borderless, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Borderless window (not to be used with fullscreen)")
MACRO_CONFIG_INT(GfxFullscreen, gfx_fullscreen, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Fullscreen")
MACRO_CONFIG_INT(GfxAlphabits, gfx_alphabits, 0, 0, 0, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Alpha bits for framebuffer (fullscreen only)")
MACRO_CONFIG_INT(GfxColorDepth, gfx_color_depth, 24, 16, 24, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Colors bits for framebuffer (fullscreen only)")
MACRO_CONFIG_INT(GfxClear, gfx_clear, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Clear screen before rendering")
MACRO_CONFIG_INT(GfxVsync, gfx_vsync, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Vertical sync")
MACRO_CONFIG_INT(GfxDisplayAllModes, gfx_display_all_modes, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(GfxTextureCompression, gfx_texture_compression, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Use texture compression")
MACRO_CONFIG_INT(GfxHighDetail, gfx_high_detail, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "High detail")
MACRO_CONFIG_INT(GfxTextureQuality, gfx_texture_quality, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(GfxFsaaSamples, gfx_fsaa_samples, 0, 0, 16, CFGFLAG_SAVE|CFGFLAG_CLIENT, "FSAA Samples")
MACRO_CONFIG_INT(GfxFinish, gfx_finish, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(GfxAsyncRender, gfx_asyncrender, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Do rendering async from the the update")

MACRO_CONFIG_INT(GfxThreaded, gfx_threaded, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Use the threaded graphics backend")

MACRO_CONFIG_INT(GfxShaders, gfx_shaders, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Use shaders")
MACRO_CONFIG_INT(GfxMultiBuffering, gfx_multibuffering, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Use multiple screen buffers")

MACRO_CONFIG_INT(InpMousesens, inp_mousesens, 100, 5, 100000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Mouse sensitivity")
MACRO_CONFIG_INT(InpHWCursor, inp_hw_cursor, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Use a hardware cursor")

MACRO_CONFIG_STR(SvName, sv_name, 128, "unnamed server", CFGFLAG_SERVER, "Server name")
MACRO_CONFIG_STR(Bindaddr, bindaddr, 128, "", CFGFLAG_CLIENT|CFGFLAG_SERVER|CFGFLAG_MASTER, "Address to bind the client/server to")
MACRO_CONFIG_INT(SvPort, sv_port, 8303, 0, 0, CFGFLAG_SERVER, "Port to use for the server")
MACRO_CONFIG_INT(SvExternalPort, sv_external_port, 0, 0, 0, CFGFLAG_SERVER, "External port to report to the master servers")
MACRO_CONFIG_STR(SvMap, sv_map, 128, "generate_city1", CFGFLAG_SERVER, "Map to use on the server")
MACRO_CONFIG_STR(SvInvMap, sv_dont_use_j92tka9j, 128, "", CFGFLAG_SERVER, "Latest invasion map")
MACRO_CONFIG_INT(SvMaxClients, sv_max_clients, 64, 1, MAX_CLIENTS, CFGFLAG_SERVER, "Maximum number of clients that are allowed on a server")
MACRO_CONFIG_INT(SvMaxClientsPerIP, sv_max_clients_per_ip, 64, 1, MAX_CLIENTS, CFGFLAG_SERVER, "Maximum number of clients with the same IP that can connect to the server")
MACRO_CONFIG_INT(SvHighBandwidth, sv_high_bandwidth, 0, 0, 1, CFGFLAG_SERVER, "Use high bandwidth mode. Doubles the bandwidth required for the server. LAN use only")
MACRO_CONFIG_INT(SvRegister, sv_register, 1, 0, 1, CFGFLAG_SERVER, "Register server with master server for public listing")
MACRO_CONFIG_STR(SvRconPassword, sv_rcon_password, 32, "", CFGFLAG_SERVER, "Remote console password (full access)")
MACRO_CONFIG_STR(SvRconModPassword, sv_rcon_mod_password, 32, "", CFGFLAG_SERVER, "Remote console password for moderators (limited access)")
MACRO_CONFIG_INT(SvRconMaxTries, sv_rcon_max_tries, 3, 0, 100, CFGFLAG_SERVER, "Maximum number of tries for remote console authentication")
MACRO_CONFIG_INT(SvRconBantime, sv_rcon_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time a client gets banned if remote console authentication fails. 0 makes it just use kick")
MACRO_CONFIG_INT(SvAutoDemoRecord, sv_auto_demo_record, 0, 0, 1, CFGFLAG_SERVER, "Automatically record demos")
MACRO_CONFIG_INT(SvAutoDemoMax, sv_auto_demo_max, 10, 0, 1000, CFGFLAG_SERVER, "Maximum number of automatically recorded demos (0 = no limit)")

MACRO_CONFIG_STR(EcBindaddr, ec_bindaddr, 128, "localhost", CFGFLAG_ECON, "Address to bind the external console to. Anything but 'localhost' is dangerous")
MACRO_CONFIG_INT(EcPort, ec_port, 0, 0, 0, CFGFLAG_ECON, "Port to use for the external console")
MACRO_CONFIG_STR(EcPassword, ec_password, 32, "", CFGFLAG_ECON, "External console password")
MACRO_CONFIG_INT(EcBantime, ec_bantime, 0, 0, 1440, CFGFLAG_ECON, "The time a client gets banned if econ authentication fails. 0 just closes the connection")
MACRO_CONFIG_INT(EcAuthTimeout, ec_auth_timeout, 30, 1, 120, CFGFLAG_ECON, "Time in seconds before the the econ authentification times out")
MACRO_CONFIG_INT(EcOutputLevel, ec_output_level, 1, 0, 2, CFGFLAG_ECON, "Adjusts the amount of information in the external console")

MACRO_CONFIG_INT(Debug, debug, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SERVER, "Debug mode")
MACRO_CONFIG_INT(DbgStress, dbg_stress, 0, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SERVER, "Stress systems")
MACRO_CONFIG_INT(DbgStressNetwork, dbg_stress_network, 0, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SERVER, "Stress network")
MACRO_CONFIG_INT(DbgPref, dbg_pref, 0, 0, 1, CFGFLAG_SERVER, "Performance outputs")
MACRO_CONFIG_INT(DbgGraphs, dbg_graphs, 0, 0, 1, CFGFLAG_CLIENT, "Performance graphs")
MACRO_CONFIG_INT(DbgHitch, dbg_hitch, 0, 0, 0, CFGFLAG_SERVER, "Hitch warnings")
MACRO_CONFIG_STR(DbgStressServer, dbg_stress_server, 32, "localhost", CFGFLAG_CLIENT, "Server to stress")
MACRO_CONFIG_INT(DbgResizable, dbg_resizable, 0, 0, 0, CFGFLAG_CLIENT, "Enables window resizing")

// MapGen
MACRO_CONFIG_INT(SvMapGen, sv_mapgen, 1, 0, 1, CFGFLAG_SERVER, "Map Generation Status")
MACRO_CONFIG_INT(SvMapGenLevel, sv_mapgen_level, 1, 1, 9999, CFGFLAG_SERVER, "Map Difficulty")
MACRO_CONFIG_INT(SvMapGenSeed, sv_mapgen_seed, 0, 0, 32767, CFGFLAG_SERVER, "Map generation seed")
MACRO_CONFIG_INT(SvMapGenRandSeed, sv_mapgen_random_seed, 1, 0, 1, CFGFLAG_SERVER, "Random map generation seed")

// Invasion
MACRO_CONFIG_INT(SvInvFails, sv_inv_fails,  0, 0, 9, CFGFLAG_SERVER, "Invasion level fails")

// Co-op PvE Roguelite Director
MACRO_CONFIG_INT(SvPveRoguelite, sv_pve_roguelite, 1, 0, 1, CFGFLAG_SERVER, "Enable the shared PvE Roguelite Director")
MACRO_CONFIG_INT(SvPveContracts, sv_pve_contracts, 1, 0, 1, CFGFLAG_SERVER, "Enable PvE team contracts")
MACRO_CONFIG_INT(SvPveOperations, sv_pve_operations, 0, 0, 1, CFGFLAG_SERVER, "Enable PvE operation votes") // To be continue...
MACRO_CONFIG_INT(SvPveChoiceTime, sv_pve_choice_time, 10, 3, 60, CFGFLAG_SERVER, "Seconds allowed for an individual perk choice")
MACRO_CONFIG_INT(SvPveContractVoteTime, sv_pve_contract_vote_time, 8, 3, 60, CFGFLAG_SERVER, "Seconds allowed for a team contract vote")
MACRO_CONFIG_INT(SvPveOperationVoteTime, sv_pve_operation_vote_time, 10, 3, 60, CFGFLAG_SERVER, "Seconds allowed for a team operation vote")

// Client-owned permanent PvE progress. Current-run combat state is never stored here.
MACRO_CONFIG_INT(ClPveProgressVersion, cl_pve_progress_version, 2, 0, 999, CFGFLAG_SAVE|CFGFLAG_CLIENT, "PvE progression format version")
MACRO_CONFIG_INT(ClPveResearchPoints, cl_pve_research_points, 0, 0, 999, CFGFLAG_SAVE|CFGFLAG_CLIENT, "(EDIT = CHEAT)Unspent PvE research points")
MACRO_CONFIG_STR(ClPveResearchMask, cl_pve_research_mask, 33, "00000000000000000000000000000000", CFGFLAG_SAVE|CFGFLAG_CLIENT, "(EDIT = CHEAT)128-bit hexadecimal PvE research unlock mask")
MACRO_CONFIG_INT(ClPveHighestInvasion, cl_pve_highest_invasion, 0, 0, 9999, CFGFLAG_SAVE|CFGFLAG_CLIENT, "(EDIT = CHEAT)Highest completed Invasion floor")
MACRO_CONFIG_INT(ClPvePreferredCheckpoint, cl_pve_preferred_checkpoint, 1, 1, 9999, CFGFLAG_SAVE|CFGFLAG_CLIENT, "(EDIT = CHEAT)Preferred unlocked Invasion checkpoint")
MACRO_CONFIG_INT(ClPveDroneTutorialSeen, cl_pve_drone_tutorial_seen, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Whether the drone command wheel tutorial has been shown")

// One-click local game hosting. The password is intentionally session-only.
MACRO_CONFIG_INT(ClLocalServerMode, cl_local_server_mode, 0, 0, 5, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Selected local game mode")
MACRO_CONFIG_INT(ClLocalServerMap, cl_local_server_map, 0, 0, 6, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Selected local game map preset")
MACRO_CONFIG_INT(ClLocalServerDifficulty, cl_local_server_difficulty, 1, 1, 50, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Local game map or AI difficulty")
MACRO_CONFIG_INT(ClLocalServerBots, cl_local_server_bots, 5, 0, 16, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Number of AI players in local competitive games")
MACRO_CONFIG_INT(ClLocalServerMaxClients, cl_local_server_max_clients, 8, 1, 16, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Maximum total player slots in a local game")
MACRO_CONFIG_INT(ClLocalServerPort, cl_local_server_port, 8303, 1024, 65535, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Port used by the local game server")
MACRO_CONFIG_INT(ClLocalServerLan, cl_local_server_lan, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Allow other players on the LAN to join the local game")
MACRO_CONFIG_STR(ClLocalServerName, cl_local_server_name, 64, "Local Game", CFGFLAG_SAVE|CFGFLAG_CLIENT, "Local game server name")
MACRO_CONFIG_STR(ClLocalServerPassword, cl_local_server_password, 32, "", CFGFLAG_CLIENT, "Session-only local game password")
MACRO_CONFIG_INT(ClLocalServerRandomSeed, cl_local_server_random_seed, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Generate a new local game map seed on every launch")
MACRO_CONFIG_INT(ClLocalServerSeed, cl_local_server_seed, 0, 0, 32767, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Fixed local game map seed")
MACRO_CONFIG_INT(ClLocalServerRoguelite, cl_local_server_roguelite, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Enable the PvE Roguelite Director in local games")
MACRO_CONFIG_INT(ClLocalServerContracts, cl_local_server_contracts, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Enable PvE contracts in local games")
MACRO_CONFIG_INT(ClLocalServerHordeWaves, cl_local_server_horde_waves, 0, 0, 100, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Horde target wave count, or zero for endless")
MACRO_CONFIG_INT(ClLocalServerExtractionTime, cl_local_server_extraction_time, 4, 2, 15, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Extraction mission time limit in minutes")
MACRO_CONFIG_INT(ClLocalServerDmScore, cl_local_server_dm_score, 20, 1, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Deathmatch score limit")
MACRO_CONFIG_INT(ClLocalServerTdmScore, cl_local_server_tdm_score, 50, 1, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Team deathmatch score limit")
MACRO_CONFIG_INT(ClLocalServerCtfScore, cl_local_server_ctf_score, 500, 1, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Capture the flag score limit")
MACRO_CONFIG_INT(ClLocalServerAdvanced, cl_local_server_advanced, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show advanced local game rules")

// ===== AntiPing / Prediction System =====
MACRO_CONFIG_INT(ClAntiPing, cl_antiping, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Enable antiping, more aggressive prediction")
MACRO_CONFIG_INT(ClAntiPingPlayers, cl_antiping_players, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Predict other player's movement more aggressively (only if cl_antiping is 1)")
MACRO_CONFIG_INT(ClAntiPingGrenade, cl_antiping_grenade, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Predict grenades (only if cl_antiping is 1)")
MACRO_CONFIG_INT(ClAntiPingWeapons, cl_antiping_weapons, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Predict weapon projectiles (only if cl_antiping is 1)")
MACRO_CONFIG_INT(ClAntiPingSmooth, cl_antiping_smooth, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Make the prediction of other player's movement smoother")
MACRO_CONFIG_INT(ClAntiPingGunfire, cl_antiping_gunfire, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Predict gunfire and show predicted weapon physics")
MACRO_CONFIG_INT(ClPredictionMargin, cl_prediction_margin, 10, 1, 300, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Prediction margin in ms (adds latency, can reduce lag from ping jumps)")
MACRO_CONFIG_INT(ClShowpred, cl_showpred, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show ingame prediction time in milliseconds")

// ===== HUD Modularity =====
MACRO_CONFIG_INT(ClShowhudHealthAmmo, cl_showhud_healthammo, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show ingame HUD (Health + Ammo)")
MACRO_CONFIG_INT(ClShowhudScore, cl_showhud_score, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show ingame HUD (Score)")
MACRO_CONFIG_INT(ClShowhudTimer, cl_showhud_timer, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show ingame HUD (Timer)")
MACRO_CONFIG_INT(ClShowhudSpectatorCount, cl_showhud_spectator_count, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show ingame HUD (Spectator Count)")
MACRO_CONFIG_INT(ClShowhudPlayerPosition, cl_showhud_player_position, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show ingame HUD (Player Position)")
MACRO_CONFIG_INT(ClShowhudPlayerSpeed, cl_showhud_player_speed, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show ingame HUD (Player Speed)")
MACRO_CONFIG_INT(ClShowhudPlayerAngle, cl_showhud_player_angle, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show ingame HUD (Player Aim Angle)")
MACRO_CONFIG_INT(ClPveObjectiveDisplay, cl_pve_objective_display, 2, 0, 2, CFGFLAG_SAVE|CFGFLAG_CLIENT, "PvE objective display: 0 scoreboard, 1 always, 2 updates")

// ===== Chat Filtering =====
MACRO_CONFIG_INT(ClShowChat, cl_showchat, 1, 0, 2, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show chat (2 to always show large chat area)")
MACRO_CONFIG_INT(ClShowChatTeamMembersOnly, cl_show_chat_team_members_only, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show only chat messages from team members")
MACRO_CONFIG_INT(ClShowChatSystem, cl_show_chat_system, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show chat messages from the server")
MACRO_CONFIG_INT(ClShowKillMessages, cl_showkillmessages, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show kill messages")
MACRO_CONFIG_INT(ClShowsocial, cl_showsocial, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show social data like names, clans, chat")
MACRO_CONFIG_INT(ClFilterchat, cl_filterchat, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show chat messages from: 0=all, 1=friends only, 2=no one")
MACRO_CONFIG_INT(ClHideSelfScore, cl_hide_self_score, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Hide player's score in scoreboard")
MACRO_CONFIG_INT(ClScoreboardUserId, cl_scoreboard_userid, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show client IDs in scoreboard")
MACRO_CONFIG_INT(ClDisableWhisper, cl_disable_whisper, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Disable whisper feature")

// ===== Nameplate Customization =====
MACRO_CONFIG_INT(ClNamePlatesClan, cl_nameplates_clan, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show clan names in name plates")
MACRO_CONFIG_INT(ClNamePlatesClanSize, cl_nameplates_clan_size, 30, 0, 100, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Size of the clan name in name plates")
MACRO_CONFIG_INT(ClNamePlatesIds, cl_nameplates_ids, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show client IDs in name plates")
MACRO_CONFIG_INT(ClNamePlatesIdsSize, cl_nameplates_ids_size, 50, 0, 100, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Size of the client IDs in name plates")
MACRO_CONFIG_INT(ClNamePlatesFriendMark, cl_nameplates_friendmark, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show friend mark (heart) in name plates")
MACRO_CONFIG_INT(ClNamePlatesOwn, cl_nameplates_own, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Show own name plate (useful for demo recording)")

// ===== Smooth Spectating =====
MACRO_CONFIG_INT(ClSmoothSpectatingTime, cl_smooth_spectating_time, 300, 0, 5000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Time of smooth camera switch animation when spectating in ms (0 for off)")

// ===== Dynamic Camera (additional settings) =====
MACRO_CONFIG_INT(ClDyncamSmoothness, cl_dyncam_smoothness, 0, 0, 100, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Transition amount of the camera movement, 0=instant, 100=slow and smooth")
MACRO_CONFIG_INT(ClDyncamStabilizing, cl_dyncam_stabilizing, 0, 0, 100, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Amount of camera slowdown during fast cursor movement")

// ===== Smooth Spectating =====
MACRO_CONFIG_INT(EdAutosaveInterval, ed_autosave_interval, 10, 0, 240, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Interval in minutes at which the editor map is auto-saved (0 for off)")
MACRO_CONFIG_INT(EdAutosaveMax, ed_autosave_max, 10, 0, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Maximum number of autosaves per map (0 = no limit)")

// ===== Desktop Notifications =====
MACRO_CONFIG_INT(ClShowNotifications, cl_shownotifications, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Make the client notify when someone highlights you")

// ===== Streamer Mode =====
MACRO_CONFIG_INT(ClStreamerMode, cl_streamer_mode, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Censor sensitive information such as passwords")
#endif
