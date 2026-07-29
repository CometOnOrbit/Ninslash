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

file(READ "${NINSLASH_SOURCE_DIR}/bam.lua" BAM_CONFIG)
string(FIND "${BAM_CONFIG}" "engine, zlib, pnglite, json_parser" BAM_TOOL_JSON_LINK_POS)
if(BAM_TOOL_JSON_LINK_POS EQUAL -1)
	message(FATAL_ERROR "Bam tools that link engine must also link json_parser")
endif()
string(REGEX MATCHALL "engine, zlib, json_parser\\)" BAM_SERVICE_JSON_LINKS "${BAM_CONFIG}")
list(LENGTH BAM_SERVICE_JSON_LINKS BAM_SERVICE_JSON_LINK_COUNT)
if(BAM_SERVICE_JSON_LINK_COUNT LESS 2)
	message(FATAL_ERROR "Bam version and master servers must also link json_parser")
endif()

file(READ "${NINSLASH_SOURCE_DIR}/src/base/system.c" BASE_SYSTEM_SOURCE)
string(FIND "${BASE_SYSTEM_SOURCE}" "module_offset=%p" WINDOWS_U64_FORMAT_POS)
if(WINDOWS_U64_FORMAT_POS EQUAL -1)
	message(FATAL_ERROR "Windows crash logging must use a compiler-compatible pointer format")
endif()

file(READ "${NINSLASH_SOURCE_DIR}/.github/workflows/build.yaml" BUILD_WORKFLOW)
foreach(STEAM_BETA_WORKFLOW_REQUIREMENT
	"steam-beta:"
	"github.ref == 'refs/heads/dev'"
	"environment: steam-beta"
	"--set-live beta"
	"STEAMCMD_AUTH_B64"
	"steam-macos-depots:"
	"--platform macos"
	"1812704"
	"5016794"
)
	string(FIND "${BUILD_WORKFLOW}" "${STEAM_BETA_WORKFLOW_REQUIREMENT}" STEAM_BETA_WORKFLOW_POS)
	if(STEAM_BETA_WORKFLOW_POS EQUAL -1)
		message(FATAL_ERROR "Steam beta workflow is missing: ${STEAM_BETA_WORKFLOW_REQUIREMENT}")
	endif()
endforeach()

file(READ "${NINSLASH_SOURCE_DIR}/CMakeLists.txt" ROOT_CMAKE)
file(READ "${NINSLASH_SOURCE_DIR}/packaging/steam/app_build.vdf.in" STEAM_CLIENT_BUILD)
file(READ "${NINSLASH_SOURCE_DIR}/packaging/steam/tool_build.vdf.in" STEAM_SERVER_BUILD)
foreach(MACOS_DEPOT_REQUIREMENT
	"STEAM_MACOS_CLIENT_DEPOT_ID \"1812704\""
	"STEAM_MACOS_SERVER_DEPOT_ID \"5016794\""
	"@MACOS_CLIENT_DEPOT_ID@"
	"@MACOS_SERVER_DEPOT_ID@"
)
	string(FIND "${ROOT_CMAKE}${STEAM_CLIENT_BUILD}${STEAM_SERVER_BUILD}" "${MACOS_DEPOT_REQUIREMENT}" MACOS_DEPOT_REQUIREMENT_POS)
	if(MACOS_DEPOT_REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "macOS Steam depot configuration is missing: ${MACOS_DEPOT_REQUIREMENT}")
	endif()
endforeach()
