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

---

## Building

Ninslash supports two build systems: **CMake** (recommended) and the legacy
**bam** builder used by some Linux workflows and CI packaging.

| | Minimum |
| --- | --- |
| CMake | 3.11+ |
| Language | C++17 |
| Python | 3.x (code generation during the build) |
| Windows ABI | x86-64 and x86 (32-bit) |

### Dependencies

| Component | Client | Dedicated server | Notes |
| --- | --- | --- | --- |
| C++ compiler + CMake | yes | yes | MSVC, GCC or Clang |
| Python 3 | yes | yes | Required by CMake for generated sources |
| SDL3 | yes | no | Bundled for Windows under `other/sdl3/` |
| FreeType | yes | no | Bundled for Windows under `other/freetype/` |
| GLEW | yes | no | Bundled/static; source also under `other/glew/` |
| zlib | yes | yes | System lib preferred; source fallback in-tree |
| pnglite, WavPack | yes | no | Prefer system libs when compatible; otherwise bundled under `src/engine/external/` |
| OpenGL / platform libs | yes | no | OpenGL + OS sockets/UI libraries |

On **Windows**, `PREFER_BUNDLED_LIBS` defaults to **ON**: CMake selects the
matching `lib64` or `lib32` libraries in `other/` and copies `SDL3.dll` and
`freetype.dll` next to the client executable after the build. You normally do
**not** need vcpkg or system-wide SDL/FreeType installs for a stock Windows
client build.

On **Linux/macOS**, system packages are preferred unless you pass
`-DPREFER_BUNDLED_LIBS=ON`.

### Clone

```sh
git clone --recursive https://github.com/CometOnOrbit/Ninslash.git
cd Ninslash

# If you already cloned without submodules:
# git submodule update --init --recursive
```

#### Choose a build path

| Goal | Recommended host and generator | Result |
| --- | --- | --- |
| Linux/macOS development | Native CMake build | Client and/or dedicated server for the current host |
| Windows release or local development | Native Windows + Visual Studio CMake generator | Windows x64 or Win32 binaries |
| Windows standalone build from Linux | MinGW-w64 toolchain file | Windows x64 or Win32 binaries without Steam integration |
| Steam Windows depot | Native Windows runner | Steamworks-compatible Windows depot |

Use a separate build directory for every operating system, compiler, generator
and architecture. For example, do not reuse an x64 CMake directory for Win32;
configure `build-win32` with `-A Win32` instead. CMake copies generated sources,
`data/` and the runtime DLLs into the build directory, but `cfg/` remains in the
repository root and must be copied when creating a portable release folder.

---

### Building on Linux or macOS (CMake)

#### Install dependencies

```sh
# Debian / Ubuntu
sudo apt update
sudo apt install -y build-essential cmake git python3 \
  libfreetype6-dev libglew-dev zlib1g-dev \
  libjpeg-dev ninja-build \
  libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxss-dev libwayland-dev libxkbcommon-dev \
  libgl1-mesa-dev libegl1-mesa-dev libdbus-1-dev libudev-dev

# SDL3 is required by the client. If your distribution provides libsdl3-dev,
# install it and skip the source-build block below:
# sudo apt install -y libsdl3-dev

# If libsdl3-dev is unavailable, build and install SDL3 before configuring
# Ninslash:
git clone --depth 1 https://github.com/libsdl-org/SDL.git /tmp/SDL3
cmake -S /tmp/SDL3 -B /tmp/SDL3/build \
  -DCMAKE_BUILD_TYPE=Release -DSDL_TESTS=OFF
sudo cmake --build /tmp/SDL3/build --parallel
sudo cmake --install /tmp/SDL3/build
sudo ldconfig

# Fedora
sudo dnf install @development-tools cmake gcc-c++ git python3 \
  freetype-devel glew-devel SDL3-devel zlib-devel

# Arch Linux
sudo pacman -S --needed base-devel cmake git python freetype2 glew sdl3 zlib

# macOS (Homebrew)
brew install cmake freetype sdl3 glew python3
```

pnglite and a compatible WavPack are optional system packages; when missing or
API-incompatible the build falls back to the in-tree sources automatically.

#### Configure and build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

With Ninja (usually faster):

```sh
# Debian/Ubuntu: sudo apt install ninja-build
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Outputs land in the build directory:

- Client: `build/ninslash`
- Dedicated server: `build/ninslash_srv`

Run them from the repository root or a package directory that also contains
`data/` (and preferably `cfg/`). CMake copies `data/`, `autoexec.cfg`,
`autoexec_client.cfg` and `storage.cfg` into the build directory so you can
also run directly from `build/`.

On later rebuilds only the `cmake --build` step is needed.

For a Steam Linux release, build on Ubuntu 22.04 (the CI publishing job is
pinned to that runner) or an equivalent container. Building on a newer host can
produce binaries requiring a newer glibc than the Steam Linux Runtime provides,
for example `GLIBC_2.38 not found`.

#### Server-only

Useful on headless machines without graphics libraries:

```sh
cmake -S . -B build-server -DCMAKE_BUILD_TYPE=Release -DCLIENT=OFF
cmake --build build-server --parallel
```

---

### Building on Linux (bam)

The historical Bam build is still used for the Linux CI release artifact.

```sh
# Debian/Ubuntu
sudo apt install bam   # or build https://github.com/matricks/bam

# After installing client dependencies (see above) and SDL3:
bam release
```

Other useful targets: `bam` / `bam game` (default debug-oriented flow),
`bam server`, `bam client`. Release mode is selected with `release` or
`conf=release` depending on the Bam version/scripts in this tree.
Executables are written as `ninslash` and `ninslash_srv` in the source root
when using the project’s Bam rules.

---

### Building on Windows with MSVC & Bam (legacy)

Bam is still supported for standalone Windows builds, but CMake is the
recommended Windows build system. The repository’s Bam rules use the bundled
SDL3, FreeType and GLEW libraries from the matching `lib64` or `lib32`
directory, so vcpkg is not required. Bam does not build the Steamworks
integration; use the native Windows CMake workflow for Steam builds.

#### Install the tools

Install [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
with **Desktop development with C++**, including MSVC, the Windows SDK and
the resource compiler. Also install [Python 3](https://www.python.org/downloads/)
and Git. Python must be available as `python` in the build prompt because Bam
uses it for generated sources.

#### Build Bam

Download Bam **v0.5.1** source, or clone its repository, then build Bam from
the matching Visual Studio Native Tools prompt:

```bat
git clone --depth 1 --branch v0.5.1 https://github.com/matricks/bam.git C:\tools\bam
cd /d C:\tools\bam

:: x64 Native Tools Command Prompt for VS
make_win64_msvc.bat

:: For a 32-bit build, use the x86 Native Tools prompt and run instead:
:: make_win32_msvc.bat

copy /Y bam.exe C:\path\to\Ninslash\
```

If `bam.exe` is already available on `PATH`, copying it into the Ninslash
source directory is not necessary.

#### Configure and build Ninslash

Run these commands from the Ninslash source root:

```bat
cd /d C:\path\to\Ninslash
where cl
where python

:: Generate config.lua for the current Windows compiler and architecture.
bam config
bam config print

:: Build client and dedicated server in Release mode.
bam game_release
```

Other targets provided by the current `bam.lua` are:

```bat
bam release          :: all Release targets
bam game_debug       :: client and server Debug builds
bam client_release   :: client only
bam server_release   :: dedicated server only
```

For this repository, use `bam release` or `bam game_release` rather than
`bam conf=release`; the current rules define explicit `*_release` targets.

The binaries and runtime DLLs are written to the repository root:

```text
ninslash.exe
ninslash_srv.exe
SDL3.dll
freetype.dll
glew32.dll
```

Run the client from the repository root so that `data/` is found:

```bat
ninslash.exe
ninslash_srv.exe -f cfg\default.cfg
```

When switching between x64 and x86, use a separate clone or remove the
generated `objs`, `.bam`, `config.lua`, previous executables and DLLs before
running `bam config` again. This prevents object files and generated
configuration from one architecture being reused by the other.

---

### Building on Windows with Visual Studio & CMake (recommended)

Windows CI and open release zips follow this path: **MSVC + CMake + Visual
Studio generator**, using **x64 by default**. The same project also supports
Win32 (x86). Prebuilt SDL3 / FreeType / GLEW for both Windows x64 and x86 ship
in the repo, so a stock client build only needs the compiler toolchain and
Python.

#### 1. Install tools

1. [Visual Studio](https://visualstudio.microsoft.com/) 2019 or newer
   (Community is fine) with workloads:
   - **Desktop development with C++**
   - Optional but convenient: **Python development**
2. Or install the lighter
   [Build Tools for Visual Studio](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
   with the same C++ workload.
3. [Python 3](https://www.python.org/downloads/) — enable **Add python.exe to
   PATH** during setup (required at configure/build time).
4. [Git for Windows](https://git-scm.com/download/win) and
   [CMake](https://cmake.org/download/) 3.11+ (or the CMake that ships with
   Visual Studio).

Confirm in a new terminal:

```powershell
cmake --version
python --version
git --version
```

#### 2. Clone

```powershell
git clone --recursive https://github.com/CometOnOrbit/Ninslash.git
cd Ninslash
```

#### 3. Configure (x64 or Win32)

From a “x64 Native Tools Command Prompt for VS” **or** any shell where `cmake`
can find the VS generators:

```powershell
cmake -S . -B build -A x64
```

Notes:

- Use `-A x64` for 64-bit or `-A Win32` for 32-bit Windows. The matching
  bundled libraries are selected automatically from `other/*/windows/lib64`
  or `lib32`.
- Multi-config generators (Visual Studio) ignore a single
  `CMAKE_BUILD_TYPE` at configure time; pick the configuration when building.
- Windows defaults to bundled libraries (`PREFER_BUNDLED_LIBS=ON`). Leave that
  alone unless you intentionally point CMake at system libraries.
- Successful config should report FreeType/SDL3/GLEW as found (bundled) and
  must find Python 3.

CMake GUI alternative: Browse Source → repo root, Browse Build → e.g.
`build`, Configure, choose **Visual Studio** + **x64** or **Win32**, Generate.

#### 4. Build

```powershell
cmake --build build --config Release --parallel
```

Debug:

```powershell
cmake --build build --config Debug --parallel
```

Or open `build/Ninslash.sln` in Visual Studio, set the configuration to
**Release** and the platform to **x64**, then build the `ninslash` /
`ninslash_srv` targets (or the full solution).

#### 5. Run

Visual Studio multi-config output:

| Binary | Typical path |
| --- | --- |
| Client | `build\Release\ninslash.exe` |
| Server | `build\Release\ninslash_srv.exe` |
| Runtime DLLs | `build\Release\SDL3.dll`, `build\Release\freetype.dll` |

Post-build steps already copy the bundled DLLs beside the client. `data/` is
copied into the build tree as well; for a portable folder, package as CI does:

```powershell
$dir = "ninslash-windows_x64-release"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
Copy-Item -Recurse cfg, data $dir
Copy-Item build\Release\ninslash.exe, build\Release\ninslash_srv.exe $dir
Copy-Item build\Release\SDL3.dll, build\Release\freetype.dll $dir
Copy-Item autoexec.cfg, autoexec_client.cfg, storage.cfg, license.txt $dir
```

Then run `ninslash.exe` from that directory (so `data/` is found).

User settings and logs go under `%APPDATA%\Ninslash\` (open with
`other\config_directory.bat`).

#### Common Windows issues

| Symptom | Fix |
| --- | --- |
| Missing Python at configure | Install Python 3 and ensure `python` is on `PATH`, then reconfigure |
| Generator / arch errors | Re-run `cmake -S . -B build -A x64` or `-A Win32` and always build `--config Release` (or Debug) |
| DLL load failure for the client | Ensure `SDL3.dll` and `freetype.dll` sit next to `ninslash.exe` (rebuild client or copy from the matching `other/sdl3/windows/lib64`/`lib32` and `other/freetype/windows/lib64`/`lib32` directory) |
| Blank / no-audio / missing `data` | Run from a directory that contains `data/` (repo root, build dir after CMake copy, or a release package) |
| “Prefer system libs” fails | Stay on the default bundled libs, or install matching-architecture SDL3/FreeType/GLEW yourself |
| Want server only | `cmake -S . -B build -A x64 -DCLIENT=OFF` then build `ninslash_srv` |

For a Win32 build, use `-A Win32` and copy dependencies from the corresponding
`other/sdl3/windows/lib32` and `other/freetype/windows/lib32` directories if a
portable folder is assembled manually.

---

### Building on Windows with MinGW & CMake

Native MinGW-w64 (MSYS2 recommended) also works. Prefer the **UCRT64** or
**MINGW64** environment with a C++ toolchain.

```sh
# MSYS2 example packages (names may vary slightly by environment):
# pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
#           mingw-w64-ucrt-x86_64-ninja python git

cmake -S . -B build-mingw -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --parallel
```

Bundled Windows libraries are still used by default. GLEW’s MSVC import libs
are not linked from MinGW; the build compiles GLEW from `other/glew/src`
instead.

Cross-compile from Linux to Windows with the tracked toolchain file:

```sh
# Debian/Ubuntu:
sudo apt install g++-mingw-w64-x86-64-posix \
  gcc-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw64.toolchain \
  -DCMAKE_BUILD_TYPE=Release -DCLIENT=ON -DPREFER_BUNDLED_LIBS=ON
cmake --build build-windows --parallel
```

For a 32-bit Windows build, install the i686 toolchain and select the matching
file:

```sh
# Debian/Ubuntu:
sudo apt install g++-mingw-w64-i686 gcc-mingw-w64-i686 \
  binutils-mingw-w64-i686
cmake -S . -B build-windows32 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw32.toolchain \
  -DCMAKE_BUILD_TYPE=Release -DCLIENT=ON -DPREFER_BUNDLED_LIBS=ON
cmake --build build-windows32 --parallel
```

The resulting executable and DLLs are in the selected build directory. To make
a portable standalone package from the x64 example:

```sh
release=ninslash-windows-x64-release
mkdir -p "$release"
cp -a cfg data "$release/"
cp build-windows/ninslash.exe build-windows/ninslash_srv.exe \
  build-windows/SDL3.dll build-windows/freetype.dll "$release/"
cp autoexec.cfg autoexec_client.cfg storage.cfg license.txt \
  THIRD_PARTY_LICENSES.md README.md README_zh-CN.md "$release/"
```

Check the architecture and Windows dependency closure before distributing it:

```sh
file build-windows/ninslash.exe
python3 scripts/verify_steam_release.py \
  --standalone-windows-client "$release/ninslash.exe" \
  --standalone-windows-server "$release/ninslash_srv.exe"
```

Linux MinGW cross-compilation is intended for **standalone Windows builds**.
Do not use it as the production path for the Steam Windows depot: the
Steamworks Windows SDK redistributables and import libraries must match the
target architecture, and the release workflow builds that depot on a native
Windows runner. Use the Visual Studio section or the native Windows CI job for
Steam Windows builds.

---

### CMake options

Pass these on the configure line (`cmake -S . -B build …`):

| Option | Default | Meaning |
| --- | --- | --- |
| `CLIENT` | `ON` | Build the graphical client |
| `PREFER_BUNDLED_LIBS` | `ON` on Windows, `OFF` elsewhere | Prefer `other/*` ships over system libraries |
| `DEV` | `OFF` | Skip packaging-oriented extras for faster dev builds |
| `ENABLE_LUA_MODS` | `ON` | Sandboxed Lua gameplay Mod runtime |
| `ENABLE_STEAMWORKS` | `OFF` | Optional Steam client integration |
| `ENABLE_STEAM_GAMESERVER` | `OFF` | Steam GameServer registration |
| `ENABLE_STEAM_LISTEN_SERVER` | `OFF` | In-client Steam Relay listen-server support |
| `NINSLASH_BUILD_TESTS` | `OFF` | Focused release-readiness CTest targets |
| `STEAMWORKS_SDK_ROOT` | empty | Path to a local Steamworks SDK |

`CMAKE_BUILD_TYPE=Release` applies to single-config generators such as Ninja
and Makefiles. Visual Studio uses a multi-config generator, so select the
configuration during the build with `--config Release` or `--config Debug`.
`CMAKE_TOOLCHAIN_FILE` is only needed for cross-compiling and must be supplied
at the first configure step.

Examples:

```sh
# Faster iteration, no packaging bits
cmake -S . -B build-dev -DDEV=ON -DCMAKE_BUILD_TYPE=Debug

# Dedicated server only
cmake -S . -B build-srv -DCLIENT=OFF -DCMAKE_BUILD_TYPE=Release
```

---

## Running a server

Start a dedicated server with a configuration preset:

```sh
# Linux / macOS
./build/ninslash_srv -f cfg/default.cfg

# Windows
.\build\Release\ninslash_srv.exe -f cfg/default.cfg
```

Mode-specific presets live in [`cfg/`](cfg/). Server settings can be supplied
in a configuration file or as command-line console arguments. Do not expose a
server with a weak RCON password.

To host Mods, install each package directory under the server save directory’s
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
join it automatically. Local Game expects `ninslash_srv` next to the client
binary (release packages include both).

## Testing

After building, the main repository checks can be run with:

```sh
python3 scripts/check_localization.py
python3 scripts/check_text_layout.py
python3 scripts/smoke_pve_matrix.py --server build/ninslash_srv
```

Focused CMake tests:

```sh
cmake -S . -B build-tests -DCLIENT=OFF -DNINSLASH_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

Windows-friendly equivalent:

```powershell
cmake -S . -B build-tests -A x64 -DCLIENT=OFF -DNINSLASH_BUILD_TESTS=ON
cmake --build build-tests --config Release --parallel
ctest --test-dir build-tests -C Release --output-on-failure
```

## Steamworks development

Steam integration is optional and disabled in normal open-source builds. The
Steamworks SDK is **not** distributed in this repository. Partners who have the
SDK can enable it with:

```sh
cmake -S . -B build-steam -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_STEAMWORKS=ON \
  -DENABLE_STEAM_GAMESERVER=ON \
  -DENABLE_STEAM_LISTEN_SERVER=ON \
  -DENABLE_LUA_MODS=ON \
  -DSTEAMWORKS_SDK_ROOT="$HOME/sdk"
cmake --build build-steam --parallel
```

Windows (MSVC, x64):

```powershell
cmake -S . -B build-steam -A x64 `
  -DENABLE_STEAMWORKS=ON `
  -DENABLE_STEAM_GAMESERVER=ON `
  -DENABLE_STEAM_LISTEN_SERVER=ON `
  -DENABLE_LUA_MODS=ON `
  "-DSTEAMWORKS_SDK_ROOT=C:\path\to\steamworks_sdk"
cmake --build build-steam --config Release --parallel
```

For a native Windows Win32 Steam build, use a new build directory and replace
`-A x64` with `-A Win32`:

```powershell
cmake -S . -B build-steam-win32 -A Win32 `
  -DENABLE_STEAMWORKS=ON `
  -DENABLE_STEAM_GAMESERVER=ON `
  -DENABLE_STEAM_LISTEN_SERVER=ON `
  -DENABLE_LUA_MODS=ON `
  "-DSTEAMWORKS_SDK_ROOT=C:\path\to\steamworks_sdk"
cmake --build build-steam-win32 --config Release --parallel
```

`STEAMWORKS_SDK_ROOT` must contain `public/steam/steam_api.h` and the
platform redistributables under `redistributable_bin/` (Windows x64:
`win64/steam_api64.dll` and its matching import library; Windows x86:
`steam_api.dll` and `steam_api.lib`; Linux x64:
`linux64/libsteam_api.so`; macOS:
`osx/libsteam_api.dylib`). Steam builds require `ENABLE_LUA_MODS=ON`.
The SDK path is not downloaded by CMake and must be supplied by the developer
or CI secret. Never commit the SDK or `steam_appid.txt`.
Packaging and upload steps are documented under
[`packaging/steam/`](packaging/steam/).

## Contributing

Bug reports and pull requests are welcome. Before submitting a change:

- Keep unrelated changes separate.
- Build both the client and dedicated server when shared code changes.
- Run the relevant localization, layout and PvE smoke tests.
- Include the source and redistribution license for every new asset.
- Never commit Steamworks SDK files, credentials or `steam_appid.txt`.

Translations live in [`data/languages/`](data/languages/). Gameplay and
network protocol changes should preserve compatibility unless the network
version is intentionally updated.

## License

The source code uses the permissive license described in
[`license.txt`](license.txt), with attribution to Teeworlds and the original
Ninslash authors. Bundled libraries have their own licenses, summarized in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).

Unless a file carries a more specific third-party notice, content in `data/`
uses the Ninslash asset license, CC-BY-SA 3.0.
