#ifndef GAME_NODES_VARIABLES_H
#define GAME_NODES_VARIABLES_H
#undef GAME_NODES_VARIABLES_H

MACRO_CONFIG_INT(SvNodesStartBuildpoints, sv_nodes_start_buildpoints, 1000, 0, 1000000,
	CFGFLAG_SAVE | CFGFLAG_SERVER, "Nodes build points on game start")
MACRO_CONFIG_INT(SvNodesBuildDelay, sv_nodes_build_delay, 10, 0, 60,
	CFGFLAG_SAVE | CFGFLAG_SERVER, "Seconds before a Nodes player can build again")
MACRO_CONFIG_INT(SvNodesSpawnDelay, sv_nodes_spawn_delay, 3, 0, 60,
	CFGFLAG_SAVE | CFGFLAG_SERVER, "Seconds between Nodes spawn queue entries")
MACRO_CONFIG_INT(SvNodesEnemyBuildDistance, sv_nodes_enemy_build_distance, 400, 0, 2000,
	CFGFLAG_SAVE | CFGFLAG_SERVER, "Minimum distance from enemy Nodes buildings")

#endif
