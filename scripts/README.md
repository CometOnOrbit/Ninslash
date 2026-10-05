# Project scripts

This directory contains **current project tooling**, not every experiment ever used
while developing NinslashA. Run commands from the repository root. Some authoring
and Windows smoke tools need optional dependencies or a local build; they are not
all CI steps and should not be executed blindly as a batch.

## Build and release

- `cmd5.py`, `content_hash.py`, `embed_binary.py`: build-time hashes and embedded data.
- `audit_release_assets.py`: release asset allow/deny audit.
- `render_steam_build.py`, `stage_steam_build.py`, `verify_steam_release.py`,
  `publish_steam_depots.py`, `local_steam_test.sh`: current Steam build/release tools.
  Publishing requires an explicit release decision and credentials.

## Current Boss art and runtime checks

- `build_boss_v5.py`, `build_abyss_angler.py`, `build_skitter_matriarch.py`:
  current v5/angler/matriarch authoring sources.
- `rasterize_boss_native_style.py`, `validate_boss_native_style.py`:
  native-style texture generation and validation.
- `test_boss_v5_runtime_windows.py`, `test_skitter_matriarch_runtime_windows.py`:
  current Windows runtime smoke checks.
- `smoke_pve_matrix.py`: deterministic PvE runtime smoke matrix.

## Weapons, localization and source maintenance

- `check_official_weapon_hash.py`: compare the runtime official weapon hash with
  the current canonical source inputs.
- `weapon_lua_files.py`, `format_weapon_lua.py`, `normalize_weapon_floats.py`,
  `export_legacy_weapon_visuals.py`: weapon content helpers and visual baselines.
  The visual exporter remains in use; “legacy” in a filename alone is not a
  reason to delete a tool.
- `check_localization.py`, `check_text_layout.py`, `update_localization.py`,
  `update_localization_server.py`: language/text maintenance.
- `check_header_guards.py`: source maintenance.

## Retirement policy and 2026-10-06 cleanup

33 confirmed obsolete files were retired: 12 superseded industrial/rail/v4 Boss
and gallery tools, plus 21 old Python 2/SVN/build/package/font/statistics/network
experiment or completed migration files. The unused `Dat2c` function in `bam.lua`
was removed along with its retired implementation.

The initial script cleanup did **not** delete game data, current art sources,
baked socket tables, or tests. A subsequent, explicitly requested test cleanup
removed seven implementation-coupled checks, including two source-pattern/API
scanning tools from this directory. There are now **27 current tool files** here,
plus this README. See `tests/README.md` and `design/test-cleanup-20261006/` for
that separate change and its backup; the archive below describes the initial
33-file script cleanup only.

`src/game/industrial_boss_sockets.h` remains a live compatibility input; its
historical generator is archived, not a command maintainers should run against
current v5 assets. Historical design and handoff documents retain their original
references and are not current runbooks.

The exact retired paths, original bytes and SHA-256 hashes are preserved in:

`design/maintenance-cleanup-20261006/retired-scripts-backup.zip`

Extract only the needed files into a temporary directory for inspection. Do not
restore an obsolete generator or overwrite current Boss art blindly. The archive
also includes the pre-cleanup `bam.lua` and socket header for targeted rollback.

See `tests/README.md` for the retained test suite. Future retirement should require
reference/dependency checks and an explicit replacement or obsolete-use rationale,
not a tool's age, filename or whether AI helped write it.
