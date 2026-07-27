# Steam integration boundaries

**English · [简体中文](steam-integration_zh-CN.md)**

The default build remains the existing UDP client and `ninslash_srv` server.
Steam support is opt-in: pass `-DENABLE_STEAMWORKS=ON` for the client and
`-DENABLE_STEAM_GAMESERVER=ON` for a dedicated server, with
`STEAMWORKS_SDK_ROOT` pointing to a locally installed SDK. SDK files, login
tokens and Web API keys are never stored in this tree.

The development configuration used for the Steam client, GameServer, embedded
Listen Server, and Lua sandbox is:

```sh
cmake .. \
  -DENABLE_STEAMWORKS=ON \
  -DSTEAMWORKS_SDK_ROOT="$HOME/sdk" \
  -DSTEAM_APP_ID=1812700 \
  -DENABLE_STEAM_GAMESERVER=ON \
  -DSTEAM_GAMESERVER_APP_ID=5016790 \
  -DENABLE_STEAM_LISTEN_SERVER=ON \
  -DENABLE_LUA_MODS=ON
cmake --build . -j2
ctest --output-on-failure
```

The published SteamPipe mapping is client AppID `1812700` with Windows/Linux
depots `1812702`/`1812703`, and Dedicated Server Tool AppID `5016790` with
Windows/Linux depots `5016792`/`5016793`.
These are exposed as CMake cache variables and rendered into build-ready VDF
files under `<build>/steam/`; staged depot roots remain outside the source tree.

Steam is an optional identity and discovery provider, not the base network.
`sv_register` continues to control the legacy master list, while
`sv_register_steam` independently controls Steam GameServer advertising. LAN
discovery and direct UDP addresses remain available in Steam and standalone
builds.

`sv_steam_auth` has three modes: `0` is open and records no trusted Steam
identity, `1` accepts explicit anonymous standalone clients but requires valid
tickets from clients claiming Steam identity, and `2` requires verified Steam
identity. Invalid, replayed, expired, timed-out or unavailable claimed Steam
identity never falls back to anonymous. `sv_official 1` forces mode `2`, both
discovery registrations, and rejects Mod configuration. A standalone server
build can run modes 0 and 1, but refuses to start in mode 2 or official mode.
The client waits for `GetAuthSessionTicketResponse_t` before transmitting a new
ticket; the server allows 30 seconds for the two asynchronous Steam callbacks.

Platform-auth request, response and result messages are appended to the legacy
system-message enum. After version/password validation the server requests an
explicit `anonymous` or `steam` identity before map transfer whenever auth or
Mod validation is active. The response carries identity kind, SteamID, opaque
ticket and installed Mod collection. A server with `sv_mod_hash` only permits
an exact match. This prevents a map, character, or PvE result being created
before the selected identity policy succeeds.

Workshop content must include `ninslash_mod.json`. Steam's UGC install is first
validated and then copied through a temporary, revalidated directory into the
user storage `workshop/<PublishedFileID>/`; the embedded and dedicated servers
load only from that independent directory. Validation rejects absolute and
traversal paths, symbolic links/reparse points, undeclared files, native
executables/libraries, invalid identity or hashes, protocol mismatches, and
oversized packages. Dependencies require exact PublishedFileID, version and
content hash matches. Collection hashes are stable and dependency ordered.
Every manifest must declare `content_rating` as `everyone`, `teen`, or
`mature`; the same value is applied as a Workshop publication tag. A minimal
manifest is:

```json
{
  "published_file_id": "1234567890",
  "name": "Example rules",
  "version": "1.0.0",
  "author": "Example author",
  "target_protocol": "<current GAME_NETVERSION>",
  "content_hash": "<64 lowercase hex characters>",
  "content_rating": "everyone",
  "api_version": 1,
  "capabilities": ["gameplay_rules"],
  "dependencies": [],
  "maps": [],
  "resources": [],
  "scripts": ["rules/main.lua"]
}
```

With `ENABLE_LUA_MODS`, gameplay scripts run only inside the server or embedded
Listen Server. Each Mod receives an isolated Lua 5.4 state with deterministic
random numbers, a memory ceiling and a per-event instruction budget. `io`,
`os`, `debug`, `package`, native modules, network access and file writes are not
available. A script error or budget violation disables that Mod runtime. The
client never executes gameplay Lua; the versioned API reserves capabilities for
future rules, weapons, items, resources and client themes.

The Steam client build can host an embedded Listen Server with
`steam_lobby_create [invite|friends|public]`. The host joins over loopback UDP;
invited clients consume the Lobby's `steam:<HostSteamID64>` connect value and
use Steam NetworkingSockets P2P on virtual port 1. `steam_lobby_invite` opens
the Steam invite dialog and `steam_lobby_leave` closes the room. There is no
host migration: loss of Lobby ownership shuts down the embedded server. The
standalone dedicated server and LAN browser continue to use UDP unchanged.
The console command `steam_lobby_status` prints the current Lobby ID.
The Internet browser queries Steam's GameServer list using the base game AppID
`1812700`. The Tool AppID `5016790` is only the SteamCMD/Depot distribution
container; the server initializes authentication and discovery as the base
game so client tickets belong to the same application. Returned endpoints are merged with the UDP master list by
`NETADDR`; duplicate servers appear once, with `[OFFICIAL]`, `[COMMUNITY]`, or
`[COMMUNITY MODDED]` labels derived from authenticated GameServer tags.
Public rooms can be queried with `steam_lobby_refresh`, inspected after the
asynchronous callback with `steam_lobby_list`, and joined with
`steam_lobby_join <LobbyID>`. Results expose host identity/name, mode, map,
region, players, password, Mod and friend-hosted status. Invalid protocol,
owner, connect or Mod metadata is never accepted as a join target.

Workshop diagnostics and selection are available through:

```text
steam_workshop_refresh
steam_workshop_list
steam_workshop_select <comma-separated-root-IDs>
steam_workshop_disable <PublishedFileID>
steam_workshop_enable <PublishedFileID>
steam_workshop_unsubscribe <PublishedFileID>
steam_workshop_open <PublishedFileID>
steam_workshop_create
steam_workshop_publish <PublishedFileID> <content-directory> [preview-file]
steam_workshop_publish_status
```

On Linux, launch from Steam for Overlay support. When launching a development
binary outside Steam, preload Steam's 64-bit renderer before starting the game:

```sh
LD_PRELOAD="$HOME/.local/share/Steam/ubuntu12_64/gameoverlayrenderer.so" \
  /tmp/ninslash-steam-build/ninslash
```

For installations under `~/.steam`, the equivalent renderer is commonly
`$HOME/.steam/ubuntu12_64/gameoverlayrenderer.so`. The architecture must match
the 64-bit game. Steam must already be running and the game must belong to the
logged-in test account.

The client links `libsteam_api.so` from the configured SDK through RUNPATH.
Steam itself supplies `steamclient.so`; a standard install places it under
`~/.local/share/Steam/linux64/`. If the SDK runtime still probes
`~/.steam/sdk64/`, create that directory/symlink as part of local machine setup,
not in the repository.

```sh
mkdir -p "$HOME/.steam/sdk64"
ln -sf "$HOME/.local/share/Steam/linux64/steamclient.so" \
  "$HOME/.steam/sdk64/steamclient.so"
```

The diagnostic line `Loaded '.../linux64/steamclient.so' OK` means the fallback
worked. A later `SteamInternal_SetMinidumpSteamID` line is informational and is
not itself an initialization failure; use the game's `[steam]: initialized for
user ...` log line to confirm `SteamAPI_Init` completed.

The embedded server uses an immutable room-settings snapshot. Network, auth,
map, mode, password, player limit and Mod values are reapplied while the room
is running, and the previous process-global values are restored after shutdown.
This prevents runtime mutation without rewriting every legacy configuration
read into an instance-owned system.

Server-confirmed events drive the twelve achievements, cooperative completion
stat, Invasion depth and fixed-seed Horde time leaderboards. Failed client API
operations persist in `steam_pending_events.dat` for retry. Leaderboards are
accepted only from official, forced-auth, matching-protocol, unmodified,
non-debug dedicated games; Listen/P2P and community games are excluded.
Dedicated-server moderation commands are `steam_ban <SteamID64> [minutes]
[reason]`, `steam_unban <SteamID64>`, and `steam_bans`. Authentication and
moderation logs include SteamID64, result, room type and Mod hash.

The Steam menu has separate Rooms and Workshop views. It covers public room
refresh/join, friend-room creation/invites, install and validation status,
download/upload progress, collection selection, local enable/disable,
unsubscribe, item creation and validated content/preview publication. Steam's
Workshop item page remains the authority for legal-agreement acceptance,
community reports, copyright complaints and moderation. Publishers must select
accurate Steam content descriptors in Partner/Community tooling; the game does
not infer a rating from arbitrary Mod assets.
