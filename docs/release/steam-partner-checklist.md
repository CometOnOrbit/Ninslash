# Steam Partner and manual acceptance checklist

**English · [简体中文](steam-partner-checklist_zh-CN.md)**

This document contains only work that requires Partner access, real Steam
accounts, physical devices, legal decisions, or branch promotion. Run the
offline verifier and strict asset audit before starting this checklist.

## Partner definitions

Create these achievements with no progress stat or numeric reward. API names
are protocol-facing and must match exactly; display names and descriptions may
be localized without changing the API name.

| API name | Suggested English display name |
|---|---|
| `ACH_FIRST_INVASION` | First Invasion |
| `ACH_FIRST_HORDE` | First Horde Clear |
| `ACH_FIRST_EXTRACTION` | First Extraction |
| `ACH_INVASION_10` | Invasion Floor 10 |
| `ACH_INVASION_30` | Invasion Floor 30 |
| `ACH_INVASION_60` | Invasion Floor 60 |
| `ACH_FIRST_FORGE` | First Forge |
| `ACH_FIRST_BUILD` | First Build |
| `ACH_COOP_RESCUE` | Cooperative Rescue |
| `ACH_FIRST_PVP_WIN` | First PvP Win |
| `ACH_FIRST_COOP_COMPLETE` | First Co-op Completion |
| `ACH_FIRST_BOSS` | First Boss Defeated |

Create integer stat `STAT_COOP_COMPLETIONS`, default `0`, increment-only. Create
leaderboard `Invasion Highest Floor` as descending/numeric and `Fixed Seed
Clear Time` as ascending/time in milliseconds. Keep these names exact.

Configure Auto-Cloud for only `pve_progress.json` under `%APPDATA%/Ninslash`
on Windows and `$HOME/.ninslash` on Linux. Exclude settings, backups, temporary
files, logs, demos, screenshots, downloaded maps, Workshop content and current
run equipment. Use whole-file conflict selection; do not merge fields.

Publish `data/steam_input_manifest.vdf` for AppID `1812700`. Confirm game,
menu, spectator and chat action sets and all localized glyph bindings.

Enable anonymous SteamCMD installation for Dedicated Server Tool `5016790`.
Confirm client depots `1812702`/`1812703` and server depots `5016792`/`5016793`.

## Two-account network acceptance

- [ ] Invite-only Lobby is absent from public/friend searches; direct invite works.
- [ ] Friends-only Lobby is visible to a friend and accepts an Overlay invite.
- [ ] Public Lobby shows host, mode, map, region, players, password, Mod hash and unofficial status.
- [ ] A second household/NAT joins through `steam:<HostSteamID64>` without port forwarding.
- [ ] Owner transfer or host exit closes the Listen Server; no unsupported migration occurs.
- [ ] Missing Workshop content downloads, validates and requires a deliberate rejoin.
- [ ] Hash mismatch, dependency conflict and locally disabled Mod all block joining.
- [ ] P2P, community and Mod games never upload official leaderboard scores.

## Dedicated server acceptance

- [ ] A public server appears in both the legacy master list and Steam GameServer list once.
- [ ] `sv_register_steam 0` removes only the Steam listing; UDP, LAN, direct address and legacy master remain usable.
- [ ] A non-Steam client joins a Steam-enabled `sv_steam_auth 1` community server as anonymous.
- [ ] Anonymous clients are rejected by `sv_steam_auth 2` and official servers.
- [ ] Forged, expired and replayed Steam tickets are rejected before map transfer.
- [ ] Steam auth outage rejects claimed Steam identities but leaves anonymous optional-auth access available.
- [ ] SteamCMD installs and starts Tool `5016790` anonymously on clean Windows and Linux hosts.

## Device, Cloud and release acceptance

- [ ] Steam Input works on a physical controller and SDL fallback works without Steam.
- [ ] Overlay opens from Steam on Windows, Linux and SteamOS.
- [ ] Two machines create divergent Cloud saves; each selected whole file loads without merging.
- [ ] Achievements retry after an offline session and unlock only from eligible server events.
- [ ] Stats and both leaderboards update with their configured sort/display rules.
- [ ] Routine SteamID moderation logs are removed after 90 days unless under a documented hold.
- [ ] Legal identity/contact fields and every asset provenance decision are complete.
- [ ] Upload to `internal`, install the uploaded BuildID on clean machines, then promote that same BuildID to `beta` and finally `public`.
