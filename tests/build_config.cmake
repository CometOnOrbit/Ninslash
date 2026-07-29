if(NOT DEFINED NINSLASH_SOURCE_DIR)
	message(FATAL_ERROR "NINSLASH_SOURCE_DIR is required")
endif()

file(READ "${NINSLASH_SOURCE_DIR}/cfg/invasion_root.cfg" INVASION_CONFIG)
string(FIND "${INVASION_CONFIG}" "exec cfg/default.cfg" DEFAULT_CONFIG_POS)
string(FIND "${INVASION_CONFIG}" "sv_enablebuilding 1" BUILDING_ENABLED_POS)

if(DEFAULT_CONFIG_POS EQUAL -1)
	message(FATAL_ERROR "Invasion config no longer loads cfg/default.cfg")
endif()
if(BUILDING_ENABLED_POS EQUAL -1 OR BUILDING_ENABLED_POS LESS DEFAULT_CONFIG_POS)
	message(FATAL_ERROR "Invasion must enable building after loading cfg/default.cfg")
endif()
