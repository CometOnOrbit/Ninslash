# SteamPipe templates

**English · [简体中文](README_zh-CN.md)**

Ninslash uses AppID `1812700`; Ninslash Playtest uses AppID `1812730` and
shares the same client depots; Ninslash Dedicated Server uses Tool AppID
`5016790`. The assigned Windows/Linux/macOS client depots are
`1812702`/`1812703`/`1812704`; the dedicated server depots are
`5016792`/`5016793`/`5016794`. Do not commit Steam
credentials, cached login tokens, or partner-only SDK files.

The wrapper below configures and rebuilds Linux and Windows, imports pre-staged
macOS content, stages all six depots, renders the VDF files and runs the offline
verifier. A missing Windows build directory is created automatically with the
tracked MinGW64 toolchain. It stops before uploading unless `--upload` is
explicitly supplied:

```sh
python3 scripts/publish_steam_depots.py \
  --linux-build-dir build \
  --windows-build-dir build-windows-steam \
  --macos-client-depot dist/steam-macos/macos-client \
  --macos-server-depot dist/steam-macos/macos-server \
  --sdk-root "$HOME/sdk"
```

Upload the verified client and Dedicated Server builds with SteamCMD:

```sh
python3 scripts/publish_steam_depots.py \
  --linux-build-dir build \
  --windows-build-dir build-windows-steam \
  --macos-client-depot dist/steam-macos/macos-client \
  --macos-server-depot dist/steam-macos/macos-server \
  --sdk-root "$HOME/sdk" \
  --upload --steam-account YOUR_PARTNER_ACCOUNT
```

By default SteamCMD handles password and Steam Guard interaction itself, but an
upload must never send a password while a cached login exists: SteamCMD then
re-issues Steam Guard, invalidates the cached session and forces interactive
verification on every upload. The upload script checks for a cached login under
`~/.steam/steam/config/config.vdf` or `~/Steam/config/config.vdf` and ignores
`STEAM_PASSWORD` when one is found (printing a notice); if neither a cache nor
a password is available it fails with a hint to refresh `STEAMCMD_AUTH_B64`.
Use the password only for the very first interactive login on a machine. The
password never appears in command arguments or logs. Set `STEAM_ACCOUNT` and
`STEAMCMD` in the environment when desired; use `--no-build` to package
existing binaries and `--strict-assets` for the final public-release asset
gate. CI restores the cached login from `STEAMCMD_AUTH_B64` to both possible
data directories and must not set `STEAM_PASSWORD` at the same time.
SteamCMD uses exit code 6 for both permanent rejection and temporary SteamPipe
CDN failures. The wrapper retries up to three times only when logs written by
the current attempt contain HTTP 5xx; stale logs and permission/configuration
errors do not trigger a retry. Override this with `--upload-attempts` and
`--upload-retry-delay` when necessary.
Use `--upload-target client` or `--upload-target server` to retry only one AppID
without creating another build for an AppID that already succeeded. Playtest
`1812730` cannot be uploaded via SteamPipe (shared depots are owned by main AppID
`1812700`); promote Playtest `internal` manually in Steamworks after the client
Build succeeds.
On a Linux workstation without a macOS toolchain, use
`--platforms linux,windows`; the rendered app manifests then contain only the
four selected depots.
Add `--set-live internal` (or another configured branch name) to make the new
BuildID live on that beta branch after upload. SteamPipe cannot automatically
set the public `default` branch live; upload without `--set-live`, then promote
the Build from the Steamworks App Admin Builds page.
When diagnosing a commit failure, omit `--set-live` first. Upload permission can
create a Build, while changing a live branch may require additional Steamworks
publish permission. Promote the successfully created Build separately after it
has been tested.

Staging copies direct non-system Linux dependencies such as SDL3 and pnglite
beside the executable. Do not rely on libraries installed only on the build
machine: Steam Linux Runtime does not inherit `/usr/local/lib` and does not
provide project-specific libraries such as `libpnglite.so.0`.
The client depots also contain `ninslash_srv`/`ninslash_srv.exe`, because the
Local Game menu starts that sibling process. This deliberate duplication is
independent of the Dedicated Server Tool depot.

If SteamCMD reports `No Connection`, test `steamcmd +login anonymous +quit`
before retrying an account. Fake-IP/TUN proxy software must either route the
SteamCMD process and Steam TCP/UDP traffic, or be disabled temporarily so Steam
hostnames resolve to real addresses instead of `198.18.0.0/15`. A failed upload
does not invalidate the rendered manifests; after fixing connectivity, rerun
the corresponding `+run_app_build` commands directly.

Render upload-ready templates with the tracked IDs and private absolute paths:

```sh
python3 scripts/render_steam_build.py --output dist/steampipe \
  --build-output /private/steam-output --content-root /private/content \
  --windows-client-root dist/steam/windows-client \
  --linux-client-root dist/steam/linux-client \
  --macos-client-root dist/steam/macos-client \
  --windows-server-root dist/steam/windows-server \
  --linux-server-root dist/steam/linux-server \
  --macos-server-root dist/steam/macos-server
```

For local SDK testing only, copy `steam_appid.txt.example` next to the built
client and rename it to `steam_appid.txt`. The staging script rejects that file
if it is ever placed in depot content.

Stage content before upload:

```sh
python3 scripts/stage_steam_build.py --platform windows --kind client \
  --build-dir build --output dist/steam/windows-client \
  --steam-api /path/to/steam_api64.dll
python3 scripts/stage_steam_build.py --platform linux --kind server \
  --build-dir build --output dist/steam/linux-server \
  --steam-api /path/to/libsteam_api.so
python3 scripts/stage_steam_build.py --platform macos --kind client \
  --build-dir build-macos --output dist/steam/macos-client \
  --steam-api /path/to/libsteam_api.dylib
```

Use separate `internal`, `beta`, and `public` branches. Upload to `internal`,
install through Steam on a clean machine, run the release test matrix, and only
then promote the same BuildID; do not rebuild independently for promotion.

Run `scripts/verify_steam_release.py` against rendered manifests, staged Steam
depots and standalone binaries before upload. `sv_register_steam` controls only
Steam advertising; `sv_register` preserves the open legacy master-list route.

## GitHub Actions internal publishing

The `Publish Steam internal` job runs after every successful push to the Git `dev`
branch. It waits for the release-readiness suite and all Linux, Windows, and
macOS release builds, creates all six Steam depots on their native builders,
verifies them, uploads the main client AppID and the dedicated-server Tool AppID.
Playtest is not uploaded by default because shared client depot content is
written by the main client build. By default CI **does not** change live
branches; set the repository variable `STEAM_SET_LIVE_BRANCH` to `internal`
once the build account can publish on AppIDs `1812700`, `1812730`, and
`5016790`. Pull requests, tags, and all other Git branches do not upload to
Steam.

In Steamworks, configure Playtest `1812730` with **Add Shared Depot** for client
depots `1812702`/`1812703`/`1812704` from main AppID `1812700`, then **publish**
the Playtest SteamPipe settings. CI/scripts upload only the main client and
dedicated-server Tool AppIDs. Promote Playtest manually after the client Build
succeeds:

1. Open Playtest `1812730` → **SteamPipe → Builds**
2. Select the `internal` branch (or create a beta branch)
3. Promote it to a Build that references the latest manifests for depots
   `1812702`/`1812703`/`1812704` (the UI usually lets you create a Build from the
   shared depot manifests)

SteamPipe rejects Playtest uploads with both `FileMapping` and `DepotFromApp`
(`I/O Operation Failed`). That is a Valve shared-depot limitation, not a script
misconfiguration. The build account needs edit/publish permission on AppIDs
`1812700`, `1812730`, and `5016790`.

The Steam Linux build runs on the pinned `ubuntu-22.04` runner so the executable
and bundled SDL3/C++ runtime remain compatible with Steam users on older glibc
systems. Keep this pin unless the Linux runtime baseline is deliberately raised.

To be honest, will anyone else watch this except for me six months from now?

Nope.

Create a protected GitHub Environment named `steam-beta` and configure these
environment secrets:

- `STEAMWORKS_SDK_REPOSITORY`: private GitHub repository in `owner/repo` form;
  its root (or `sdk/`) must contain the Steamworks SDK.
- `STEAMWORKS_SDK_TOKEN`: read-only fine-grained token for that private
  repository.
- `STEAM_ACCOUNT`: Steam partner build account with edit/publish access to the
  main, Playtest, and dedicated-server AppIDs. Upload permission is enough for
  CI; changing live branches additionally requires publish permission on each
  AppID (enable via `STEAM_SET_LIVE_BRANCH`, see below).
- `STEAMCMD_AUTH_B64`: base64-encoded gzip tar containing SteamCMD's
  `config/config.vdf` and any `ssfn*` files after an interactive login on a
  trusted Linux machine. Do not commit this archive, and do not include the
  entire `config/` directory with its browser cache.

First complete an interactive SteamCMD login with the publishing account.
The standalone SteamCMD client stores that login under `~/.steam/steam/config/`
(newer clients) or `~/Steam/config/` (older clients), rather than beside
`steamcmd.sh`. Then, from any temporary working directory, run:

```bash
if [ -d "$HOME/.steam/steam/config" ]; then
  auth_root="$HOME/.steam/steam"
elif [ -d "$HOME/Steam/config" ]; then
  auth_root="$HOME/Steam"
else
  echo "SteamCMD login state not found; run an interactive +login first" >&2
  exit 1
fi
archive="$PWD/steamcmd-auth.tar.gz"
(
  cd "$auth_root"
  set -- config/config.vdf
  for file in ssfn*; do
    [ -e "$file" ] && set -- "$@" "$file"
  done
  tar -czf "$archive" "$@"
)
base64 -w0 steamcmd-auth.tar.gz > steamcmd-auth.txt
```

SteamCMD does not create `ssfn*` files for every login. The command includes
them when present but only takes `config/config.vdf` from `config/`, excluding
the large `config/htmlcache/`. Inspect the archive with
`tar -tzf steamcmd-auth.tar.gz`, store the contents of
`steamcmd-auth.txt` as `STEAMCMD_AUTH_B64`, then delete both local export files.
Environment protection rules can require approval before each upload without
changing the workflow.

Optional repository variable (Settings → Secrets and variables → Actions →
Variables):

- `STEAM_SET_LIVE_BRANCH`: when set to `internal`, CI passes `--set-live
  internal` after each successful upload. Leave unset while diagnosing upload
  failures or when the account can create builds but cannot publish branches.
