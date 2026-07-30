<div align="center">
  <img src="data/gui/logo.png" alt="Ninslash" width="520">

  **English · [简体中文](README_zh-CN.md)**

  **A fast-paced 2D action game with modular weapons, building, PvP and cooperative roguelite modes.**

  [Steam](https://store.steampowered.com/app/1812700/) ·
  [Releases](https://github.com/CometOnOrbit/Ninslash/releases) ·
  [Issues](https://github.com/CometOnOrbit/Ninslash/issues)

  [![Build](https://github.com/CometOnOrbit/Ninslash/actions/workflows/build.yaml/badge.svg)](https://github.com/CometOnOrbit/Ninslash/actions/workflows/build.yaml)
</div>

## About

Ninslash is a multiplayer action game descended from Teeworlds. It combines
responsive platforming and shooting with destructible combat spaces, building,
procedurally generated PvE missions and weapons assembled from interchangeable
modules.

The repository contains both the graphical game client and the headless
dedicated server. You can join public servers, host a community server or start
a managed local game directly from the client.

## Features

- Fast online combat with keyboard, mouse and gamepad support.
- Competitive modes including DM, TDM, CTF, Ball and other server presets.
- Cooperative Invasion, Horde and Extraction modes with objectives, bosses,
  contracts, research and roguelite character builds.
- Modular ranged and melee weapons with charging, penetration, ricochet,
  explosions and Forge combinations.
- Player-built defenses, turrets and other interactive structures.
- Procedural maps and an integrated map editor.
- Dedicated servers, a public server browser and one-click local hosting.
- Community translations for more than twenty languages, including Simplified
  and Traditional Chinese.

## Download

Development builds and tagged releases are published on the
[GitHub Releases page](https://github.com/CometOnOrbit/Ninslash/releases).

Ninslash has the Steam AppID **1812700**. The Steam release is still being
prepared; availability is shown on the
[Steam store page](https://store.steampowered.com/app/1812700/).

## Building

Ninslash primarily uses CMake. Building the client requires a C++ compiler,
Python 3, SDL3, FreeType, GLEW, WavPack, zlib and pnglite.

Clone the repository including its submodules:

```sh
git clone --recursive https://github.com/CometOnOrbit/Ninslash.git
cd Ninslash
```

Configure and build a release version:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

On multi-configuration generators such as Visual Studio, use:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

The resulting executables are named `ninslash` and `ninslash_srv`, with the
usual `.exe` suffix on Windows. The legacy Bam build remains available for
existing development and CI workflows:

```sh
bam release
```

## Running a server

Start a dedicated server with a configuration preset:

```sh
./build/ninslash_srv -f cfg/default.cfg
```

Mode-specific presets live in [`cfg/`](cfg/). Server settings can be supplied
in a configuration file or as command-line console arguments. Do not expose a
server with a weak RCON password.

To host Mods, install each package directory under the server save directory's
`workshop/` folder and set only the root package IDs, for example:

```cfg
sv_mod_ids 9000000001
```

The folder name may be descriptive; package identity comes from
`published_file_id` in `ninslash_content.json`. The server resolves dependencies
and computes `sv_mod_hash` from the installed content at startup. Do not copy a
client hash into the server configuration manually. Clients must have the same
package versions enabled or the server log reports both collection hashes and
ID lists.

For a private or solo session, open **Local Game** in the client. It starts the
dedicated server as a managed child process, selects an available port and can
join it automatically.

## Testing

After building, the main repository checks can be run with:

```sh
python3 scripts/check_localization.py
python3 scripts/check_text_layout.py
python3 scripts/smoke_pve_matrix.py --server build/ninslash_srv
```

Focused CMake tests are available separately:

```sh
cmake -S . -B build-tests -DCLIENT=OFF -DNINSLASH_BUILD_TESTS=ON
cmake --build build-tests --target ninslash_test_pve_progress
ctest --test-dir build-tests --output-on-failure
```

## Steamworks development

Steam integration is optional and disabled in normal open-source builds. The
Steamworks SDK is not distributed in this repository. Partner builds can
enable it with:

```sh
cmake -S . -B build-steam -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_STEAMWORKS=ON \
  -DSTEAMWORKS_SDK_ROOT=/path/to/steamworks_sdk
cmake --build build-steam --parallel
```

## Contributing

Bug reports and pull requests are welcome. Before submitting a change:

- Keep unrelated changes separate.
- Build both the client and dedicated server when shared code changes.
- Run the relevant localization, layout and PvE smoke tests.
- Include the source and redistribution license for every new asset.
- Never commit Steamworks SDK files, credentials or `steam_appid.txt`.

Translations are stored in [`data/languages/`](data/languages/). Gameplay and
network protocol changes should preserve compatibility unless the network
version is intentionally updated.

## License

The source code uses the permissive license described in
[`license.txt`](license.txt), with attribution to Teeworlds and the original
Ninslash authors. Bundled libraries have their own licenses, summarized in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).

Unless a file carries a more specific third-party notice, content in `data/`
uses the Ninslash asset license, CC-BY-SA 3.0.
