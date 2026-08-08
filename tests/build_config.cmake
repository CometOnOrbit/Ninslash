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
foreach(PVE_BAM_REQUIREMENT
	"pve_cards = EmbedBinary(\"data/pve/pve_cards.json\""
	"pve_contracts = EmbedBinary(\"data/pve/pve_contracts.json\""
	"AddDependency(object, pve_cards)"
	"AddDependency(object, pve_contracts)"
)
	string(FIND "${BAM_CONFIG}" "${PVE_BAM_REQUIREMENT}" PVE_BAM_REQUIREMENT_POS)
	if(PVE_BAM_REQUIREMENT_POS EQUAL -1)
		message(FATAL_ERROR "Bam PvE embed dependency is missing: ${PVE_BAM_REQUIREMENT}")
	endif()
endforeach()

file(READ "${NINSLASH_SOURCE_DIR}/src/base/system.c" BASE_SYSTEM_SOURCE)
string(FIND "${BASE_SYSTEM_SOURCE}" "module_offset=%p" WINDOWS_U64_FORMAT_POS)
if(WINDOWS_U64_FORMAT_POS EQUAL -1)
	message(FATAL_ERROR "Windows crash logging must use a compiler-compatible pointer format")
endif()

file(READ "${NINSLASH_SOURCE_DIR}/src/engine/client/backend_sdl.cpp" GRAPHICS_BACKEND_SOURCE)
foreach(FRAMEBUFFER_GUARD_REQUIREMENT
	"static bool FramebufferFunctionsAvailable()"
	"if(!FramebufferFunctionsAvailable())"
	"framebuffer creation skipped"
)
	string(FIND "${GRAPHICS_BACKEND_SOURCE}" "${FRAMEBUFFER_GUARD_REQUIREMENT}" FRAMEBUFFER_GUARD_POS)
	if(FRAMEBUFFER_GUARD_POS EQUAL -1)
		message(FATAL_ERROR "OpenGL framebuffer capability guard is missing: ${FRAMEBUFFER_GUARD_REQUIREMENT}")
	endif()
endforeach()

file(READ "${NINSLASH_SOURCE_DIR}/.github/workflows/build.yaml" BUILD_WORKFLOW)
foreach(STEAM_INTERNAL_WORKFLOW_REQUIREMENT
	"steam-internal:"
	"github.ref == 'refs/heads/dev'"
	"environment: steam-beta"
	"--set-live internal"
	"STEAMCMD_AUTH_B64"
	"steam-macos-depots:"
	"steam-windows-depots:"
	"runs-on: windows-latest"
	"cmake -S . -B build-steam-windows -A x64"
	"ninslash-steam-windows-build"
	"--no-build"
	"--platform macos"
	"1812704"
	"5016794"
)
	string(FIND "${BUILD_WORKFLOW}" "${STEAM_INTERNAL_WORKFLOW_REQUIREMENT}" STEAM_INTERNAL_WORKFLOW_POS)
	if(STEAM_INTERNAL_WORKFLOW_POS EQUAL -1)
		message(FATAL_ERROR "Steam internal workflow is missing: ${STEAM_INTERNAL_WORKFLOW_REQUIREMENT}")
	endif()
endforeach()

file(READ "${NINSLASH_SOURCE_DIR}/CMakeLists.txt" ROOT_CMAKE)
string(REGEX MATCH
	"add_executable\\(ninslash_test_pve_definitions[^\\)]*pve_cards\\.inc[^\\)]*pve_contracts\\.inc"
	PVE_DEFINITIONS_GENERATED_SOURCES
	"${ROOT_CMAKE}"
)
if(NOT PVE_DEFINITIONS_GENERATED_SOURCES)
	message(FATAL_ERROR "ninslash_test_pve_definitions must depend on both generated PvE embeds")
endif()
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
