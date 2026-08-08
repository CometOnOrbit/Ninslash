#ifndef GAME_CLIENT_LOCAL_GAME_MODES_H
#define GAME_CLIENT_LOCAL_GAME_MODES_H

#include <base/system.h>

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
	LOCAL_MODE_ROAM,
	LOCAL_MODE_NODES,
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
	LOCAL_RULE_ROAM_CHECKPOINTS,
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
	bool m_HasBots;
	bool m_HasRoguelite;
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
static const char *s_apLocalNodesMaps[] = {"Nodes Small", "Nodes Medium"};
static const char *s_apLocalNodesMapCommands[] = {"generate_ctf_small1", "generate_ctf_medium1"};

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
						 Rule,																						   \
						 HasBots,																					   \
						 HasRoguelite)                                                                                 \
	{                                                                                                                  \
		Name, Description, RecommendedPlayers, Duration, Difficulty, Mechanics, Config, GameType, VoteImage, Pve,      \
			MapGen, Selectable, Maps, Commands, (int)(sizeof(Maps) / sizeof(Maps[0])), Rule, HasBots, HasRoguelite       \
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
					 LOCAL_RULE_FIXED, false, false),
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
					 LOCAL_RULE_INVASION, false, true),
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
					 LOCAL_RULE_HORDE, false, true),
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
					 LOCAL_RULE_EXTRACTION, false, true),
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
					 LOCAL_RULE_DM_SCORE, true, false),
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
					 LOCAL_RULE_TDM_SCORE, true, false),
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
					 LOCAL_RULE_CTF_SCORE, true, false),
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
					 LOCAL_RULE_FIXED, false, true),
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
					 LOCAL_RULE_REACTOR_SCORE, true, false),
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
					 LOCAL_RULE_BALL_SCORE, true, false),
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
					 LOCAL_RULE_FIXED, true, false),
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
					 LOCAL_RULE_DM_SCORE, true, false),
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
					 LOCAL_RULE_CTF_SCORE, true, false),
	LOCAL_MODE_ENTRY("Roam Race",
					 "Race through a generated modular course with a selectable number of checkpoints.",
					 "1-8",
					 "10 min",
					 "Normal",
					 "Race  ·  Checkpoints  ·  Modular course",
					 "cfg/roam_mapgen.cfg",
					 "roam",
					 "invasion1",
					 false,
					 true,
					 true,
					 s_apLocalMaps,
					 s_apLocalMapCommands,
					 LOCAL_RULE_ROAM_CHECKPOINTS, false, false),
	LOCAL_MODE_ENTRY("Nodes",
					 "Capture territory through technology, powered structures and team respawn networks.",
					 "2-12",
					 "20-40 min",
					 "Hard",
					 "Teams  ·  Building  ·  Tech levels",
					 "cfg/nodes.cfg",
					 "nodes",
					 "nodes1",
					 false,
					 true,
					 true,
					 s_apLocalNodesMaps,
					 s_apLocalNodesMapCommands,
					 LOCAL_RULE_FIXED, true, false),
};

static const int s_aAllLocalModes[] = {
	LOCAL_MODE_INVASION, LOCAL_MODE_HORDE, LOCAL_MODE_EXTRACTION, LOCAL_MODE_REACTOR_DEFENSE,
	LOCAL_MODE_DM, LOCAL_MODE_TDM, LOCAL_MODE_CTF,
	LOCAL_MODE_REACTOR_ASSAULT, LOCAL_MODE_BALL,
	LOCAL_MODE_BATTLE_ROYALE, LOCAL_MODE_GRENADE_DM, LOCAL_MODE_INSTAGIB_CTF, LOCAL_MODE_ROAM,
	LOCAL_MODE_NODES
};

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
		   Mode == LOCAL_MODE_BALL || Mode == LOCAL_MODE_INSTAGIB_CTF || Mode == LOCAL_MODE_NODES;
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
	if(Rule == LOCAL_RULE_ROAM_CHECKPOINTS)
		return "Checkpoints";
	return "Score limit";
}

// Game-vote categories shared between the in-game mode vote overlay and the
// room-creation mode picker. Category assignment follows the local mode
// definitions instead of guessing from thumbnail file names.
enum EGameVoteCategory
{
	GAMEVOTE_CATEGORY_PVE = 0,
	GAMEVOTE_CATEGORY_TEAM,
	GAMEVOTE_CATEGORY_SOLO,
	GAMEVOTE_CATEGORY_ARCADE,
	NUM_GAMEVOTE_CATEGORIES,
};

// Category a local game mode belongs to in the mode vote overlay.
inline int LocalGameModeVoteCategory(int Mode)
{
	if(Mode < 0 || Mode >= LOCAL_MODE_COUNT)
		return GAMEVOTE_CATEGORY_ARCADE;
	if(s_aLocalGameModes[Mode].m_Pve)
		return GAMEVOTE_CATEGORY_PVE;
	if(LocalGameModeUsesTeamPopulation(Mode))
		return GAMEVOTE_CATEGORY_TEAM;
	if(Mode == LOCAL_MODE_ROAM)
		return GAMEVOTE_CATEGORY_ARCADE;
	return GAMEVOTE_CATEGORY_SOLO;
}

// Resolves a server game-vote thumbnail name to the local mode it belongs to.
// Extra map variants shipped as separate .vot files map onto their base mode.
inline int LocalGameModeFromImage(const char *pImage)
{
	static const struct
	{
		const char *m_pImage;
		int m_Mode;
	} s_aImageAliases[] = {
		{"invasion2", LOCAL_MODE_INVASION},
		{"invasion3", LOCAL_MODE_INVASION},
		{"invasion4", LOCAL_MODE_INVASION},
		{"invasion5", LOCAL_MODE_INVASION},
		{"invasion-endless", LOCAL_MODE_INVASION},
		{"ball2", LOCAL_MODE_BALL},
	};
	for(unsigned i = 0; i < sizeof(s_aImageAliases) / sizeof(s_aImageAliases[0]); i++)
		if(str_comp(pImage, s_aImageAliases[i].m_pImage) == 0)
			return s_aImageAliases[i].m_Mode;
	for(int i = 1; i < LOCAL_MODE_COUNT; i++)
		if(str_comp(pImage, s_aLocalGameModes[i].m_pGameVoteImage) == 0)
			return i;
	return -1;
}

// Display order key: index inside s_aAllLocalModes so the vote overlay lists
// modes in the same order as the room-creation mode picker. Modes that are not
// part of the picker list sort after every listed mode.
inline int LocalGameModeSortKey(int Mode)
{
	const int Count = (int)(sizeof(s_aAllLocalModes) / sizeof(s_aAllLocalModes[0]));
	for(int i = 0; i < Count; i++)
		if(s_aAllLocalModes[i] == Mode)
			return i;
	return Count;
}

#endif
