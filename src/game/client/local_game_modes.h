#ifndef GAME_CLIENT_LOCAL_GAME_MODES_H
#define GAME_CLIENT_LOCAL_GAME_MODES_H

#include <game/client/room_creation.h>

enum ELocalGameMode
{
	LOCAL_MODE_TUTORIAL = 0,
	LOCAL_MODE_INVASION,
	LOCAL_MODE_HORDE,
	LOCAL_MODE_EXTRACTION,
	LOCAL_MODE_DM,
	LOCAL_MODE_TDM,
	LOCAL_MODE_CTF,
	// Keep the original IDs above stable for saved client configurations.
	LOCAL_MODE_REACTOR_DEFENSE,
	LOCAL_MODE_REACTOR_ASSAULT,
	LOCAL_MODE_BALL,
	LOCAL_MODE_BATTLE_ROYALE,
	LOCAL_MODE_GRENADE_DM,
	LOCAL_MODE_INSTAGIB_CTF,
	LOCAL_MODE_COUNT,
};

enum ELocalGameRule
{
	LOCAL_RULE_FIXED = 0,
	LOCAL_RULE_INVASION,
	LOCAL_RULE_HORDE,
	LOCAL_RULE_EXTRACTION,
	LOCAL_RULE_DM_SCORE,
	LOCAL_RULE_TDM_SCORE,
	LOCAL_RULE_CTF_SCORE,
	LOCAL_RULE_REACTOR_SCORE,
	LOCAL_RULE_BALL_SCORE,
};

struct CLocalGameMode
{
	const char *m_pName;
	const char *m_pDescription;
	const char *m_pRecommendedPlayers;
	const char *m_pDuration;
	const char *m_pRecommendedDifficulty;
	const char *m_pMechanics;
	const char *m_pConfig;
	const char *m_pGameType;
	const char *m_pGameVoteImage;
	bool m_Pve;
	bool m_MapGen;
	bool m_SelectableMap;
	const char *const *m_ppMapNames;
	const char *const *m_ppMapCommands;
	int m_MapCount;
	int m_Rule;
};

static const char *s_apLocalMaps[] = {
	"City I", "City II", "Space", "Large I", "Large II", "Large III", "Blue planet", "Foundry"};
static const char *s_apLocalMapCommands[] = {"generate_city1",
											 "generate_city2",
											 "generate_space1",
											 "generate_large1",
											 "generate_large2",
											 "generate_large3",
											 "generate_blueplanet1",
											 "generate_foundry1"};
static const char *s_apLocalCtfMaps[] = {"Compact", "Standard"};
static const char *s_apLocalCtfMapCommands[] = {"generate_ctf_small1", "generate_ctf_medium1"};
static const char *s_apLocalBallMaps[] = {"Arena I", "Arena II"};
static const char *s_apLocalBallMapCommands[] = {"ball_small1", "ball_small2"};
static const char *s_apLocalReactorDefenseMaps[] = {"Reactor Defense"};
static const char *s_apLocalReactorDefenseMapCommands[] = {"reactor_pve1"};
static const char *s_apLocalReactorAssaultMaps[] = {"Reactor Assault"};
static const char *s_apLocalReactorAssaultMapCommands[] = {"reactor1"};

#define LOCAL_MODE_ENTRY(Name,                                                                                         \
						 Description,                                                                                  \
						 RecommendedPlayers,                                                                           \
						 Duration,                                                                                     \
						 Difficulty,                                                                                   \
						 Mechanics,                                                                                    \
						 Config,                                                                                       \
						 GameType,                                                                                     \
						 VoteImage,                                                                                    \
						 Pve,                                                                                          \
						 MapGen,                                                                                       \
						 Selectable,                                                                                   \
						 Maps,                                                                                         \
						 Commands,                                                                                     \
						 Rule)                                                                                         \
	{                                                                                                                  \
		Name, Description, RecommendedPlayers, Duration, Difficulty, Mechanics, Config, GameType, VoteImage, Pve,      \
			MapGen, Selectable, Maps, Commands, (int)(sizeof(Maps) / sizeof(Maps[0])), Rule                            \
	}

static const CLocalGameMode s_aLocalGameModes[LOCAL_MODE_COUNT] = {
	LOCAL_MODE_ENTRY("Tutorial",
					 "Six guided solo chapters for controls, combat, objectives, forging, builds and rooms.",
					 "1",
					 "45 min",
					 "Guided",
					 "Objectives  ·  Forge  ·  Builds",
					 "cfg/tutorial.cfg",
					 "tutorial",
					 "invasion1",
					 true,
					 true,
					 false,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_FIXED),
	LOCAL_MODE_ENTRY("Invasion",
					 "Explore generated floors, complete objectives and keep your build between maps.",
					 "1-4",
					 "30-60 min",
					 "Normal",
					 "Objectives  ·  Progression  ·  Builds",
					 "cfg/invasion_root.cfg",
					 "coop",
					 "invasion1",
					 true,
					 true,
					 false,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_INVASION),
	LOCAL_MODE_ENTRY("Horde",
					 "Defend, build and survive increasingly dangerous enemy waves.",
					 "1-6",
					 "20-40 min",
					 "Normal",
					 "Waves  ·  Building  ·  Survival",
					 "cfg/horde_root.cfg",
					 "horde",
					 "invasion7",
					 true,
					 true,
					 true,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_HORDE),
	LOCAL_MODE_ENTRY("Extraction",
					 "Finish the mission and reach the extraction zone before time runs out.",
					 "1-4",
					 "10-20 min",
					 "Normal",
					 "Timed  ·  Objectives  ·  Extraction",
					 "cfg/extract_root.cfg",
					 "extract",
					 "invasion6",
					 true,
					 true,
					 true,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_EXTRACTION),
	LOCAL_MODE_ENTRY("Deathmatch",
					 "Fight every player and reach the score limit first.",
					 "2-8",
					 "10 min",
					 "Easy",
					 "Free-for-all  ·  Fast respawn",
					 "cfg/pvp_practice.cfg",
					 "dm",
					 "dm1",
					 false,
					 true,
					 true,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_DM_SCORE),
	LOCAL_MODE_ENTRY("Team deathmatch",
					 "Coordinate with your team to win the score race.",
					 "4-10",
					 "15 min",
					 "Normal",
					 "Teams  ·  Building  ·  Score race",
					 "cfg/tdm_root.cfg",
					 "tdm",
					 "tdm1",
					 false,
					 true,
					 true,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_TDM_SCORE),
	LOCAL_MODE_ENTRY("Capture the flag",
					 "Steal the enemy flag while defending your own base.",
					 "4-10",
					 "15-25 min",
					 "Normal",
					 "Flags  ·  Teams  ·  Base defense",
					 "cfg/ctf_root.cfg",
					 "ctf",
					 "ctf1",
					 false,
					 true,
					 true,
					 s_apLocalCtfMaps,
					 s_apLocalCtfMapCommands,
					 LOCAL_RULE_CTF_SCORE),
	LOCAL_MODE_ENTRY("Reactor Defense",
					 "Fortify the reactor and survive waves of hostile machines.",
					 "1-4",
					 "20-40 min",
					 "Hard",
					 "Reactor  ·  Building  ·  Boss waves",
					 "cfg/reactor_def1.cfg",
					 "base",
					 "reactor_def1",
					 true,
					 false,
					 true,
					 s_apLocalReactorDefenseMaps,
					 s_apLocalReactorDefenseMapCommands,
					 LOCAL_RULE_FIXED),
	LOCAL_MODE_ENTRY("Reactor Assault",
					 "Plant or disarm the bomb in round-based reactor combat.",
					 "4-10",
					 "15-25 min",
					 "Hard",
					 "Teams  ·  Bomb  ·  Reactor",
					 "cfg/reactor1.cfg",
					 "def",
					 "reactor1",
					 false,
					 false,
					 true,
					 s_apLocalReactorAssaultMaps,
					 s_apLocalReactorAssaultMapCommands,
					 LOCAL_RULE_REACTOR_SCORE),
	LOCAL_MODE_ENTRY("Ball",
					 "Carry the ball into the enemy goal while defending your own.",
					 "4-10",
					 "10-20 min",
					 "Normal",
					 "Teams  ·  Ball  ·  Goals",
					 "cfg/ball_root.cfg",
					 "ball",
					 "ball1",
					 false,
					 false,
					 true,
					 s_apLocalBallMaps,
					 s_apLocalBallMapCommands,
					 LOCAL_RULE_BALL_SCORE),
	LOCAL_MODE_ENTRY("Battle Royale",
					 "Outlast every opponent in a single-life survival match.",
					 "2-9",
					 "10-20 min",
					 "Normal",
					 "Last survivor  ·  Closing hazard",
					 "cfg/br_root.cfg",
					 "dm",
					 "br1",
					 false,
					 true,
					 true,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_FIXED),
	LOCAL_MODE_ENTRY("Grenade DM",
					 "Fight a fast free-for-all with unlimited grenades.",
					 "2-8",
					 "5-10 min",
					 "Easy",
					 "Grenades  ·  Free-for-all",
					 "cfg/grenade_dm1.cfg",
					 "dm",
					 "grenade1",
					 false,
					 true,
					 true,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_DM_SCORE),
	LOCAL_MODE_ENTRY("Instagib CTF",
					 "Capture the flag with lethal precision weapons and instant kills.",
					 "4-10",
					 "10-20 min",
					 "Hard",
					 "Instagib  ·  Flags  ·  Teams",
					 "cfg/ictf_small1.cfg",
					 "ctf",
					 "ictf1",
					 false,
					 true,
					 true,
					 s_apLocalCtfMaps,
					 s_apLocalCtfMapCommands,
					 LOCAL_RULE_CTF_SCORE),
};

static const int s_aLocalPveModes[] = {
	LOCAL_MODE_INVASION, LOCAL_MODE_HORDE, LOCAL_MODE_EXTRACTION, LOCAL_MODE_REACTOR_DEFENSE};
static const int s_aLocalPvpModes[] = {LOCAL_MODE_DM,
									   LOCAL_MODE_TDM,
									   LOCAL_MODE_CTF,
									   LOCAL_MODE_REACTOR_ASSAULT,
									   LOCAL_MODE_BALL,
									   LOCAL_MODE_BATTLE_ROYALE,
									   LOCAL_MODE_GRENADE_DM,
									   LOCAL_MODE_INSTAGIB_CTF};

#undef LOCAL_MODE_ENTRY

inline const CLocalGameMode &LocalGameMode(int Mode)
{
	if(Mode < 0 || Mode >= LOCAL_MODE_COUNT)
		Mode = LOCAL_MODE_INVASION;
	return s_aLocalGameModes[Mode];
}

inline int LocalGameModeCount()
{
	return LOCAL_MODE_COUNT;
}

inline bool LocalGameModeUsesTeamPopulation(int Mode)
{
	return Mode == LOCAL_MODE_TDM || Mode == LOCAL_MODE_CTF || Mode == LOCAL_MODE_REACTOR_ASSAULT ||
		   Mode == LOCAL_MODE_BALL || Mode == LOCAL_MODE_INSTAGIB_CTF;
}

inline const char *LocalGamePopulationLabel(int Mode)
{
	return LocalGameModeUsesTeamPopulation(Mode) ? "Target players per team" : "Target active players";
}

inline const char *LocalGameRuleLabel(int Rule)
{
	if(Rule == LOCAL_RULE_HORDE)
		return "Target waves";
	if(Rule == LOCAL_RULE_EXTRACTION)
		return "Mission time";
	if(Rule == LOCAL_RULE_BALL_SCORE)
		return "Goal target";
	return "Score limit";
}

#endif
