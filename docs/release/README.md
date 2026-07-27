# Steam release gates

**English · [简体中文](README_zh-CN.md)**

This directory tracks work that must be complete before a public commercial
distribution, including a free Steam release. Passing a build is not evidence
that the bundled art, audio, fonts, maps, or animations may legally be shipped.

The assigned Ninslash Steam application ID is `1812700`.

## Asset provenance

`asset_licenses.csv` is ordered from broad defaults to specific overrides; the
last matching pattern wins. A row may only be marked `approved` after its
source, author, license, and required attribution have been verified. Files
marked `review_required` or `rejected` block a release build.

Run a progress report:

```sh
python3 scripts/audit_release_assets.py
```

Run the release gate:

```sh
python3 scripts/audit_release_assets.py --strict
```

After staging and rendering SteamPipe manifests, run the offline depot and
binary gate. Pass every release-candidate path that is available:

```sh
python3 scripts/verify_steam_release.py \
  --manifests dist/steampipe \
  --linux-client dist/steam/linux-client \
  --linux-server dist/steam/linux-server \
  --windows-client dist/steam/windows-client \
  --windows-server dist/steam/windows-server \
  --standalone-linux-client build-standalone/ninslash \
  --standalone-linux-server build-standalone/ninslash_srv
```

Partner configuration and account/device checks are in
`docs/release/steam-partner-checklist.md`. Run them only after the offline
gates pass and before promoting an `internal` BuildID.

The project owner has confirmed that `data/**` uses the Ninslash asset license,
CC-BY-SA 3.0, unless a file carries a more specific third-party license. The
broad approved rule records that default; later narrow rows override it for
files such as third-party fonts.

## Player data

The only first-release Steam Cloud file is `pve_progress.json`:

- Windows root: `%APPDATA%/Ninslash`
- Linux root: `$HOME/.ninslash`
- Cloud pattern: `pve_progress.json`
- Cloud settings must exclude `.bak`, `.tmp`, settings, passwords, logs,
  screenshots, demos, videos, and downloaded maps.
- Configure Auto-Cloud conflict resolution to ask the player when Steam cannot
  determine a newer copy. Ninslash consumes only Steam's resolved
  `pve_progress.json`; it never merges fields from two divergent saves because
  that could duplicate research progress.
- Conflict test: produce different valid saves on two test accounts/machines,
  keep Steam offline for the second write, reconnect, resolve each side once,
  and verify the selected whole file loads while the other file is not silently
  merged. Then repeat with a corrupt and a future-schema file to verify backup
  recovery and read-only protection.

`pve_progress.json` is intentionally local-player-authoritative. It must never
contain current-run weapons, gold, inventory, server credentials, IP addresses,
or chat data.

Official server logs use verified SteamID64 as the moderation key and include
authentication state, room type and Mod hash. `steam_bans.cfg` persists SteamID
bans. Operational log rotation must delete routine records after 90 days; active
abuse investigations and legal holds are the documented exceptions.