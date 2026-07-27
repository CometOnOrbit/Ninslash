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

## 编译

Ninslash 主要使用 CMake。编译客户端需要 C++ 编译器、Python 3、SDL3、FreeType、GLEW、WavPack、zlib 和 pnglite。

克隆仓库及其子模块：

```sh
git clone --recursive https://github.com/CometOnOrbit/Ninslash.git
cd Ninslash
```

配置并编译 Release 版本：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

使用 Visual Studio 等多配置生成器时：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

生成的程序分别名为 `ninslash` 和 `ninslash_srv`，Windows 平台会带有常规的 `.exe` 后缀。为兼容已有开发和 CI 流程，旧版 Bam 构建仍然可用：

```sh
bam release
```

## 运行服务器

使用配置预设启动专用服务器：

```sh
./build/ninslash_srv -f cfg/default.cfg
```

不同模式的配置预设置于 [`cfg/`](cfg/) 目录中。服务器设置既可以写入配置文件，也可以作为控制台参数传入。不要使用弱 RCON 密码将服务器暴露到互联网。

如果只需要单人或私人房间，可以在客户端中打开“本地游戏”。客户端会将专用服务器作为受管理的子进程启动，自动选择可用端口，并可以自动加入服务器。

## 测试

完成编译后，可以运行仓库的主要检查：

```sh
python3 scripts/check_localization.py
python3 scripts/check_text_layout.py
python3 scripts/smoke_pve_matrix.py --server build/ninslash_srv
```

独立的 CMake 测试可以通过以下方式运行：

```sh
cmake -S . -B build-tests -DCLIENT=OFF -DNINSLASH_BUILD_TESTS=ON
cmake --build build-tests --target ninslash_test_pve_progress
ctest --test-dir build-tests --output-on-failure
```

## Steamworks 开发

普通开源构建默认关闭 Steam 集成。本仓库不分发 Steamworks SDK。拥有 Steam Partner 权限的构建环境可以通过以下方式启用：

```sh
cmake -S . -B build-steam -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_STEAMWORKS=ON \
  -DSTEAMWORKS_SDK_ROOT=/path/to/steamworks_sdk
cmake --build build-steam --parallel
```

Steam 集成边界见 [`docs/steam-integration_zh-CN.md`](docs/steam-integration_zh-CN.md)。SteamPipe 分包与 Depot 模板见
[`packaging/steam/README_zh-CN.md`](packaging/steam/README_zh-CN.md)，Steam Cloud 路径和发行检查见
[`docs/release/README_zh-CN.md`](docs/release/README_zh-CN.md)。

## 参与贡献

欢迎提交错误报告和 Pull Request。提交修改前请注意：

- 将无关修改拆分到不同提交或 Pull Request。
- 修改共享代码后，同时编译客户端和专用服务器。
- 运行与修改内容相关的本地化、文本布局和 PvE 冒烟测试。
- 新增素材时必须同时提供来源及允许再分发的许可证。
- 不要提交 Steamworks SDK、账号凭据或 `steam_appid.txt`。

翻译文件位于 [`data/languages/`](data/languages/) 中。修改玩法或网络协议时应尽量保持兼容；如果必须破坏兼容性，则应明确升级网络协议版本。

## 许可证

源代码使用 [`license.txt`](license.txt) 中说明的宽松许可证，并保留对 Teeworlds 和 Ninslash 原作者的署名。随附依赖拥有各自的许可证，概览见
[`THIRD_PARTY_LICENSES_zh-CN.md`](THIRD_PARTY_LICENSES_zh-CN.md)。

除非文件带有更具体的第三方声明，`data/` 中的内容采用 Ninslash 素材许可证 CC-BY-SA 3.0；该许可证不同于源代码许可证。每次公开发行仍必须通过
[素材授权审计](docs/release/README_zh-CN.md)。
