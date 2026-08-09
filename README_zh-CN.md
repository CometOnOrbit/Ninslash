<div align="center">
  <img src="data/gui/logo.png" alt="Ninslash" width="520">

  **[English](README.md) · 简体中文**

  **一款融合模块化武器、建造、PvP 与合作 Roguelite 模式的快节奏 2D 动作游戏。**

  [Steam](https://store.steampowered.com/app/1812700/) ·
  [版本下载](https://github.com/CometOnOrbit/Ninslash/releases) ·
  [问题反馈](https://github.com/CometOnOrbit/Ninslash/issues)

  [![Build](https://github.com/CometOnOrbit/Ninslash/actions/workflows/build.yaml/badge.svg)](https://github.com/CometOnOrbit/Ninslash/actions/workflows/build.yaml)
</div>

## 关于 Ninslash

Ninslash 是一款源自 Teeworlds 的多人动作游戏。游戏将灵活的平台跳跃和射击战斗，与可破坏的战斗场景、建造系统、程序生成的 PvE 任务，以及由不同模块拼装而成的武器结合在一起。

本仓库同时包含图形客户端和无界面的专用服务器。玩家可以加入公共服务器、运行社区服务器，也可以直接在客户端中创建由游戏自动管理的本地服务器。

## 游戏特色

- 支持键盘、鼠标和手柄的高速在线战斗。
- 包含 DM、TDM、CTF、Ball 等竞技模式和服务器预设。
- 包含 Invasion、Horde 和 Extraction 合作模式，以及任务目标、Boss、契约、研究和 Roguelite 构筑。
- 模块化远程及近战武器，支持蓄力、贯穿、反弹、爆炸和 Forge 锻造组合。
- 玩家可以建造防御设施、炮塔和其他可交互建筑。
- 程序生成地图和内置地图编辑器。
- 专用服务器、公共服务器浏览器和一键本地开服功能。
- 提供二十多种社区翻译，包括简体中文和繁体中文。

## 下载

开发构建和带版本标签的发行包会发布在
[GitHub Releases](https://github.com/CometOnOrbit/Ninslash/releases) 页面。

Ninslash 的 Steam AppID 是 **1812700**。Steam 发行版本仍在准备中，实际可用状态请查看
[Steam 商店页面](https://store.steampowered.com/app/1812700/)。

---

## 编译

Ninslash 支持两套构建系统：**CMake**（推荐）以及部分 Linux / CI 仍在使用的遗留 **bam**。

| | 最低要求 |
| --- | --- |
| CMake | 3.11+ |
| 语言标准 | C++17 |
| Python | 3.x（构建时生成代码） |
| Windows | 支持 x86-64 与 x86（32 位） |

### 依赖一览

| 组件 | 客户端 | 专用服务器 | 说明 |
| --- | --- | --- | --- |
| C++ 编译器 + CMake | 需要 | 需要 | MSVC / GCC / Clang |
| Python 3 | 需要 | 需要 | 配置与生成阶段都会用到 |
| SDL3 | 需要 | 否 | Windows 预置在 `other/sdl3/` |
| FreeType | 需要 | 否 | Windows 预置在 `other/freetype/` |
| GLEW | 需要 | 否 | 预置/静态链接；源码在 `other/glew/` |
| zlib | 需要 | 需要 | 优先系统库，仓库内也有回退 |
| pnglite、WavPack | 需要 | 否 | 系统库可用则优先；否则用 `src/engine/external/` |
| OpenGL / 平台库 | 需要 | 否 | OpenGL 与系统套接字等 |

**Windows** 上 `PREFER_BUNDLED_LIBS` 默认为 **ON**：CMake 根据目标架构使用仓库 `other/` 下的 `lib64` 或 `lib32` 预编译库，并在构建后把 `SDL3.dll`、`freetype.dll` 复制到客户端可执行文件旁边。常规 Windows 客户端构建**不需要** vcpkg 或另行安装系统级 SDL/FreeType。

**Linux / macOS** 默认优先系统包，可用 `-DPREFER_BUNDLED_LIBS=ON` 强制使用仓库内依赖。

### 获取源码

```sh
git clone --recursive https://github.com/CometOnOrbit/Ninslash.git
cd Ninslash

# 若已克隆但未拉子模块：
# git submodule update --init --recursive
```

---

### Linux / macOS 用 CMake 编译

#### 安装依赖

```sh
# Debian / Ubuntu
sudo apt update
sudo apt install -y build-essential cmake git python3 \
  libfreetype6-dev libglew-dev zlib1g-dev \
  libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxss-dev libwayland-dev libxkbcommon-dev \
  libgl1-mesa-dev libegl1-mesa-dev libdbus-1-dev libudev-dev

# 发行版若没有足够新的 SDL3，可从源码安装：
#   git clone https://github.com/libsdl-org/SDL.git && cd SDL
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   sudo cmake --build build --parallel && sudo cmake --install build

# Fedora
sudo dnf install @development-tools cmake gcc-c++ git python3 \
  freetype-devel glew-devel SDL3-devel zlib-devel

# Arch Linux
sudo pacman -S --needed base-devel cmake git python freetype2 glew sdl3 zlib

# macOS（Homebrew）
brew install cmake freetype sdl3 glew python3
```

pnglite 与兼容的 WavPack 为可选系统包；缺失或 API 不兼容时构建会自动使用仓库内嵌源码。

#### 配置与编译

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

使用 Ninja（通常更快）：

```sh
# Debian/Ubuntu: sudo apt install ninja-build
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物位于构建目录：

- 客户端：`build/ninslash`
- 专用服务器：`build/ninslash_srv`

请在包含 `data/`（以及最好有 `cfg/`）的目录下运行；CMake 会把 `data/`、`autoexec.cfg`、`autoexec_client.cfg`、`storage.cfg` 复制到构建目录，也可直接在 `build/` 中运行。

后续重编译只需再执行 `cmake --build`。

#### 仅服务器

适用于无图形库的无头主机：

```sh
cmake -S . -B build-server -DCMAKE_BUILD_TYPE=Release -DCLIENT=OFF
cmake --build build-server --parallel
```

---

### Linux 用 bam 编译

历史 Bam 构建在 Linux CI 发行包中仍在使用：

```sh
# Debian/Ubuntu
sudo apt install bam   # 或自行编译 https://github.com/matricks/bam

# 装好客户端依赖与 SDL3 后：
bam release
```

其他目标示例：`bam` / `bam game`、`bam server`、`bam client`。本仓库 Bam 规则下，发行构建产物多为源码根目录下的 `ninslash` 与 `ninslash_srv`。

---

### Windows：MSVC + Bam（不推荐）

Bam 仍支持独立版 Windows 构建，但 Windows 平台推荐使用 CMake。本仓库的
Bam 规则会根据目标架构使用 `lib64` 或 `lib32` 目录中的 SDL3、FreeType 和
GLEW 预置库，因此不需要 vcpkg。Bam 不会编译 Steamworks 集成；Steam 版本
请使用原生 Windows CMake 流程。

#### 安装工具

安装 [Visual Studio 生成工具](https://visualstudio.microsoft.com/visual-cpp-build-tools/)，
勾选 **使用 C++ 的桌面开发**，其中包括 MSVC、Windows SDK 和资源编译器。
另外安装 [Python 3](https://www.python.org/downloads/) 与 Git。Python 必须能在
构建命令提示符中通过 `python` 命令调用，因为 Bam 需要它生成源文件。

#### 编译 Bam

下载 Bam **v0.5.1** 源码，或克隆其仓库，然后在匹配的 Visual Studio 本机工具
命令提示符中编译 Bam：

```bat
git clone --depth 1 --branch v0.5.1 https://github.com/matricks/bam.git C:\tools\bam
cd /d C:\tools\bam

:: 在 x64 Native Tools Command Prompt for VS 中执行
make_win64_msvc.bat

:: 编译 32 位时，使用 x86 Native Tools 命令提示符并改为执行：
:: make_win32_msvc.bat

copy /Y bam.exe C:\path\to\Ninslash\
```

如果 `bam.exe` 已经在 `PATH` 中，则不需要复制到 Ninslash 源码目录。

#### 配置并编译 Ninslash

在 Ninslash 源码根目录执行：

```bat
cd /d C:\path\to\Ninslash
where cl
where python

:: 为当前 Windows 编译器和架构生成 config.lua
bam config
bam config print

:: 编译 Release 客户端和专用服务器
bam game_release
```

当前 `bam.lua` 提供的其他目标：

```bat
bam release          :: 所有 Release 目标
bam game_debug       :: 客户端和服务器 Debug 构建
bam client_release   :: 仅编译客户端
bam server_release   :: 仅编译专用服务器
```

对于本仓库，应使用 `bam release` 或 `bam game_release`，而不是
`bam conf=release`；当前规则定义的是明确的 `*_release` 目标。

二进制文件和运行时 DLL 会写入源码根目录：

```text
ninslash.exe
ninslash_srv.exe
SDL3.dll
freetype.dll
glew32.dll
```

请从源码根目录运行客户端，以便找到 `data/`：

```bat
ninslash.exe
ninslash_srv.exe -f cfg\default.cfg
```

在 x64 与 x86 之间切换时，建议使用新的源码副本；或者先删除生成的
`objs`、`.bam`、`config.lua`、旧的可执行文件和 DLL，再重新执行 `bam config`。
这样可以避免不同架构复用旧的目标文件和生成配置。

---

### Windows：Visual Studio + CMake（推荐）

官方 Windows CI 与开源 Release 压缩包走这条路径：**MSVC + CMake + Visual Studio 生成器**，架构 **x64**。仓库已附带 Windows x64 与 x86 的 SDL3 / FreeType / GLEW，标准客户端构建通常只需编译器工具链与 Python。

#### 1. 安装工具

1. [Visual Studio](https://visualstudio.microsoft.com/) 2019 或更新版本（Community 即可），勾选：
   - **使用 C++ 的桌面开发**
   - 可选：**Python 开发**
2. 或安装更轻量的
   [Visual Studio 生成工具](https://visualstudio.microsoft.com/visual-cpp-build-tools/)，同样勾选 C++ 工作负载。
3. [Python 3](https://www.python.org/downloads/) — 安装时勾选 **Add python.exe to PATH**（配置与编译阶段都需要）。
4. [Git for Windows](https://git-scm.com/download/win) 与
   [CMake](https://cmake.org/download/) 3.11+（也可用 VS 自带的 CMake）。

新开终端确认：

```powershell
cmake --version
python --version
git --version
```

#### 2. 克隆仓库

```powershell
git clone --recursive https://github.com/CometOnOrbit/Ninslash.git
cd Ninslash
```

#### 3. 配置（x64 或 Win32）

在 “适用于 VS 的 x64 本机工具命令提示” 或任意能找到 VS 生成器的 shell 中：

```powershell
cmake -S . -B build -A x64
```

说明：

- 64 位使用 `-A x64`，32 位 Windows 使用 `-A Win32`；CMake 会自动选择对应的 `lib64` 或 `lib32` 依赖。
- Visual Studio 等多配置生成器在配置阶段通常不吃单一 `CMAKE_BUILD_TYPE`，构建时再选 `Release` / `Debug`。
- Windows 默认 `PREFER_BUNDLED_LIBS=ON`，直接用仓库预置库即可。
- 配置成功时应能看到 FreeType / SDL3 / GLEW（bundled），且必须找到 Python 3。

也可用 CMake GUI：Source 选仓库根目录，Build 如 `build`，Configure，选 **Visual Studio** + **x64** 或 **Win32**，再 Generate。

#### 4. 编译

```powershell
cmake --build build --config Release --parallel
```

调试版：

```powershell
cmake --build build --config Debug --parallel
```

也可打开 `build/Ninslash.sln`，配置设为 **Release**、平台 **x64**，编译 `ninslash` / `ninslash_srv` 或整个解决方案。

#### 5. 运行

| 二进制 | 典型路径 |
| --- | --- |
| 客户端 | `build\Release\ninslash.exe` |
| 服务器 | `build\Release\ninslash_srv.exe` |
| 运行时 DLL | `build\Release\SDL3.dll`、`build\Release\freetype.dll` |

构建后步骤会把捆绑的 DLL 复制到客户端旁，并把 `data/` 拷进构建树。便携目录可按 CI 方式打包：

```powershell
$dir = "ninslash-windows_x64-release"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
Copy-Item -Recurse cfg, data $dir
Copy-Item build\Release\ninslash.exe, build\Release\ninslash_srv.exe $dir
Copy-Item build\Release\SDL3.dll, build\Release\freetype.dll $dir
Copy-Item autoexec.cfg, autoexec_client.cfg, storage.cfg, license.txt $dir
```

在该目录中运行 `ninslash.exe`（保证能找到 `data/`）。

用户配置与日志位于 `%APPDATA%\Ninslash\`（可用 `other\config_directory.bat` 打开）。

#### 常见问题

| 现象 | 处理 |
| --- | --- |
| 配置时找不到 Python | 安装 Python 3，保证 `python` 在 `PATH` 中，然后重新 configure |
| 体系结构 / 配置错乱 | 使用 `cmake -S . -B build -A x64` 或 `-A Win32`，构建时务必 `--config Release`（或 Debug） |
| 客户端缺少 DLL | 保证 `SDL3.dll`、`freetype.dll` 与 `ninslash.exe` 同目录（从 `other/sdl3/windows/lib64` / `lib32`、`other/freetype/windows/lib64` / `lib32` 复制匹配架构的版本） |
| 找不到 data / 无音效资源 | 在包含 `data/` 的目录运行（仓库根、CMake 复制后的 build 目录或发行包目录） |
| 改用系统库失败 | 保持默认捆绑库；或自行安装匹配的 x64 SDL3/FreeType/GLEW |
| 只需服务器 | `cmake -S . -B build -A x64 -DCLIENT=OFF` 后只编 `ninslash_srv` |

---

### Windows：MinGW + CMake

也可使用原生 MinGW-w64（推荐 MSYS2 的 **UCRT64** 或 **MINGW64** 环境）：

```sh
# MSYS2 示例（包名随环境略有差异）：
# pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
#           mingw-w64-ucrt-x86_64-ninja python git

cmake -S . -B build-mingw -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --parallel
```

默认仍使用仓库内 Windows 库。MSVC 的 GLEW 导入库不会被 MinGW 链接，构建会改为从 `other/glew/src` 编译 GLEW。

在 Linux 上交叉编译 Windows 可使用仓库内工具链文件：

```sh
# Debian/Ubuntu: sudo apt install g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw64.toolchain \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows --parallel
```

32 位 Windows 交叉编译：

```sh
# Debian/Ubuntu: sudo apt install g++-mingw-w64-i686 binutils-mingw-w64-i686
cmake -S . -B build-windows32 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw32.toolchain \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows32 --parallel
```

Steam 打包脚本默认保持 64 位；配置 32 位 Steam 构建时传入 `--windows-bits 32`。

---

### CMake 选项

在配置命令中传入（`cmake -S . -B build …`）：

| 选项 | 默认 | 含义 |
| --- | --- | --- |
| `CLIENT` | `ON` | 编译图形客户端 |
| `PREFER_BUNDLED_LIBS` | Windows 为 `ON`，其它平台为 `OFF` | 优先使用 `other/*` 内置库 |
| `DEV` | `OFF` | 跳过面向打包的步骤，加快开发编译 |
| `ENABLE_LUA_MODS` | `ON` | 沙箱化 Lua 玩法 Mod 运行时 |
| `ENABLE_STEAMWORKS` | `OFF` | 可选 Steam 客户端集成 |
| `ENABLE_STEAM_GAMESERVER` | `OFF` | Steam 游戏服务器注册 |
| `ENABLE_STEAM_LISTEN_SERVER` | `OFF` | 客户端内 Steam 中继监听服 |
| `NINSLASH_BUILD_TESTS` | `OFF` | 发布就绪相关 CTest 目标 |
| `STEAMWORKS_SDK_ROOT` | 空 | 本地 Steamworks SDK 路径 |

示例：

```sh
# 更快的调试向构建
cmake -S . -B build-dev -DDEV=ON -DCMAKE_BUILD_TYPE=Debug

# 仅专用服务器
cmake -S . -B build-srv -DCLIENT=OFF -DCMAKE_BUILD_TYPE=Release
```

---

## 运行服务器

使用配置预设启动专用服务器：

```sh
# Linux / macOS
./build/ninslash_srv -f cfg/default.cfg

# Windows
.\build\Release\ninslash_srv.exe -f cfg/default.cfg
```

不同模式的配置预设置于 [`cfg/`](cfg/)。服务器设置可写在配置文件中，也可作为控制台参数传入。不要使用弱 RCON 密码把服务器暴露到公网。

要在专用服务器上启用 Mod，把各个包目录安装到服务器保存目录下的 `workshop/`，并只配置根包 ID，例如：

```cfg
sv_mod_ids 9000000001
```

包文件夹可用易读名称；内容身份由 `ninslash_content.json` 中的 `published_file_id` 决定。服务端启动时会解析依赖并按实际安装内容计算 `sv_mod_hash`，无需从客户端手工复制哈希。客户端必须启用相同版本的包；不一致时日志会列出两端的集合哈希与 ID。

若只需单人或私人房间，在客户端打开 **本地游戏**：会把专用服务器作为子进程启动、自动选端口并可自动加入。本地游戏期望 `ninslash_srv` 与客户端同目录（发行包会同时带上两者）。

## 测试

编译完成后可运行仓库主要检查：

```sh
python3 scripts/check_localization.py
python3 scripts/check_text_layout.py
python3 scripts/smoke_pve_matrix.py --server build/ninslash_srv
```

独立 CMake 测试：

```sh
cmake -S . -B build-tests -DCLIENT=OFF -DNINSLASH_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

Windows：

```powershell
cmake -S . -B build-tests -A x64 -DCLIENT=OFF -DNINSLASH_BUILD_TESTS=ON
cmake --build build-tests --config Release --parallel
ctest --test-dir build-tests -C Release --output-on-failure
```

## Steamworks 开发

普通开源构建默认关闭 Steam 集成，本仓库**不**分发 Steamworks SDK。拥有 Partner 权限与 SDK 的环境可启用：

```sh
cmake -S . -B build-steam -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_STEAMWORKS=ON \
  -DENABLE_STEAM_GAMESERVER=ON \
  -DENABLE_STEAM_LISTEN_SERVER=ON \
  -DENABLE_LUA_MODS=ON \
  -DSTEAMWORKS_SDK_ROOT=/path/to/steamworks_sdk
cmake --build build-steam --parallel
```

Windows（MSVC、x64）：

```powershell
cmake -S . -B build-steam -A x64 `
  -DENABLE_STEAMWORKS=ON `
  -DENABLE_STEAM_GAMESERVER=ON `
  -DENABLE_STEAM_LISTEN_SERVER=ON `
  -DENABLE_LUA_MODS=ON `
  "-DSTEAMWORKS_SDK_ROOT=C:\path\to\steamworks_sdk"
cmake --build build-steam --config Release --parallel
```

`STEAMWORKS_SDK_ROOT` 需包含 `public/steam/steam_api.h`，以及 `redistributable_bin/` 下对应平台动态库（Windows x64：`win64/steam_api64.dll` 与对应导入库；Windows x86：`steam_api.dll` 与 `steam_api.lib`）。Steam 构建要求 `ENABLE_LUA_MODS=ON`。打包与上传说明见 [`packaging/steam/`](packaging/steam/)。

## 参与贡献

欢迎提交错误报告和 Pull Request。提交修改前请注意：

- 将无关修改拆分到不同提交或 Pull Request。
- 修改共享代码后，同时编译客户端和专用服务器。
- 运行与修改内容相关的本地化、文本布局和 PvE 冒烟测试。
- 新增素材时必须同时提供来源及允许再分发的许可证。
- 不要提交 Steamworks SDK、账号凭据或 `steam_appid.txt`。

翻译文件位于 [`data/languages/`](data/languages/)。修改玩法或网络协议时应尽量保持兼容；若必须破坏兼容性，应明确升级网络协议版本。

## 许可证

源代码使用 [`license.txt`](license.txt) 中说明的宽松许可证，并保留对 Teeworlds 和 Ninslash 原作者的署名。随附依赖拥有各自的许可证，概览见
[`THIRD_PARTY_LICENSES_zh-CN.md`](THIRD_PARTY_LICENSES_zh-CN.md)。

除非文件带有更具体的第三方声明，`data/` 中的内容采用 Ninslash 素材许可证 CC-BY-SA 3.0。
