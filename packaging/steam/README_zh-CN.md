# SteamPipe 模板

**[English](README.md) · 简体中文**

Ninslash 使用 AppID `1812700`；Ninslash Playtest 使用 AppID `1812730` 并共用同一套客户端 Depot；Ninslash Dedicated Server 使用 Tool AppID `5016790`。Windows/Linux/macOS 客户端 Depot 为 `1812702`/`1812703`/`1812704`，专用服务器 Depot 为 `5016792`/`5016793`/`5016794`。不要提交 Steam 凭据、缓存的登录令牌或仅限 Partner 使用的 SDK 文件。

以下包装脚本会重新编译 Linux/Windows、导入已在 macOS runner 暂存的内容、汇总六个 Depot、渲染 VDF 并运行离线验证器。Windows 构建目录不存在时，会使用仓库内的 MinGW64 toolchain 自动创建。除非明确传入 `--upload`，否则不会上传：

```sh
python3 scripts/publish_steam_depots.py \
  --linux-build-dir build \
  --windows-build-dir build-windows-steam \
  --macos-client-depot dist/steam-macos/macos-client \
  --macos-server-depot dist/steam-macos/macos-server \
  --sdk-root "$HOME/sdk"
```

使用 SteamCMD 上传已验证的客户端和专用服务器构建：

```sh
python3 scripts/publish_steam_depots.py \
  --linux-build-dir build \
  --windows-build-dir build-windows-steam \
  --macos-client-depot dist/steam-macos/macos-client \
  --macos-server-depot dist/steam-macos/macos-server \
  --sdk-root "$HOME/sdk" \
  --upload --steam-account YOUR_PARTNER_ACCOUNT
```

默认由 SteamCMD 处理密码和 Steam Guard 交互。本机包装脚本可通过临时 `STEAM_PASSWORD` 环境变量提供密码；密码不会进入命令行参数或日志。需要时可设置环境变量 `STEAM_ACCOUNT` 和 `STEAMCMD`；使用 `--no-build` 打包已有二进制，使用 `--strict-assets` 执行最终公开发行素材门禁。
SteamCMD 对永久拒绝和临时 SteamPipe CDN 故障都会返回退出码 6。包装器现在仅在本次上传新写入的日志明确包含 HTTP 5xx 时重试，默认最多三次；旧日志、权限错误和配置错误不会触发重试。需要时可用 `--upload-attempts` 和 `--upload-retry-delay` 调整。
使用 `--upload-target client`、`--upload-target playtest` 或 `--upload-target server` 可以只重试一个 AppID，避免为已经成功的 AppID 再创建一次构建。`client` 也会上传 Playtest（共用客户端 Depot）；仅重试 Playtest 时用 `playtest`（AppID `1812730`）。
在没有 macOS 工具链的 Linux 工作站上使用 `--platforms linux,windows`；生成的 App manifest 将只包含四个所选 Depot。
添加 `--set-live internal`（或其他已配置的 beta 分支名）可在上传后自动让新 BuildID 在该分支生效。SteamPipe 不能自动将公开 `default` 分支设为 live；应省略 `--set-live` 完成上传，再到 Steamworks App Admin 的 Builds 页面手动提升该 Build。
排查 commit 失败时应先省略 `--set-live`。上传权限可以创建 Build，但修改 live 分支可能需要额外的 Steamworks 发布权限。成功创建并测试 Build 后，再单独提升分支。

暂存时会把 SDL3、pnglite 等直接依赖的非系统 Linux 运行库复制到可执行文件旁。不要依赖只安装在构建机上的库：Steam Linux Runtime 不继承 `/usr/local/lib`，也不提供 `libpnglite.so.0` 等项目专用库。
客户端 Depot 还会包含 `ninslash_srv`/`ninslash_srv.exe`，因为“本地游戏”菜单会启动这个同目录子进程。这一有意的重复与专用服务器 Tool Depot 相互独立。

SteamCMD 报告 `No Connection` 时，先运行 `steamcmd +login anonymous +quit` 测试匿名连接。使用 Fake-IP/TUN 的代理软件必须代理 SteamCMD 进程以及 Steam TCP/UDP 流量；否则应暂时关闭代理，使 Steam 域名解析到真实地址而非 `198.18.0.0/15`。上传失败不会使已渲染的清单失效；修复网络后可直接重新执行对应的 `+run_app_build` 命令。

使用已登记 ID 和私有绝对路径渲染可上传模板：

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

仅在本地 SDK 测试时，将 `steam_appid.txt.example` 复制到客户端二进制旁并重命名为 `steam_appid.txt`。暂存脚本会拒绝将该文件放入 Depot。

上传前暂存内容：

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

分别使用 `internal`、`beta` 和 `public` 分支。先上传到 `internal`，通过 Steam 在干净设备上安装并执行发行测试矩阵，之后再提升同一 BuildID；不要为分支提升单独重新构建。

上传前，对渲染后的清单、已暂存的 Steam Depot 和独立版二进制运行 `scripts/verify_steam_release.py`。`sv_register_steam` 只控制 Steam 广告；`sv_register` 保留开放的旧主列表通道。

## GitHub Actions 自动发布 internal

每次成功推送到 Git 的 `dev` 分支后，`Publish Steam internal` Job 会等待发行测试以及 Linux、Windows、macOS 三个平台全部构建成功，在原生 runner 上生成并汇总六个 Steam Depot，完成离线校验，上传主客户端 AppID、Playtest AppID（共用同一套客户端 Depot）以及专用服务器 Tool AppID，并将新 Build 设为 Steam `internal` 分支的 live 版本。PR、标签和其他 Git 分支都不会上传 Steam。

请在 Steamworks 中为 Playtest `1812730` 使用 **Add Shared Depot**，挂载主应用 `1812700` 名下的三个客户端 Depot。上传脚本复用这些 Depot ID，不会另建 Playtest 专属内容 Depot。构建账号需要对 AppID `1812700`、`1812730`、`5016790` 具备编辑/发布权限，并能将各应用的 `internal` 分支设为 live。

Steam Linux 构建固定使用 `ubuntu-22.04` runner，确保可执行文件及随 Depot 打包的 SDL3/C++ 运行库兼容较旧 glibc 的 Steam 用户。除非有意提高 Linux 运行时基线，否则不要改回滚动的 `ubuntu-latest`。

说真的会有除了半年后的我以外的人看这个吗？

我觉得这是写给我自己看的。

请新建受保护的 GitHub Environment `steam-beta`，并配置以下 Environment Secrets：

- `STEAMWORKS_SDK_REPOSITORY`：存放 Steamworks SDK 的私有 GitHub 仓库，格式为 `owner/repo`；SDK 可位于仓库根目录或 `sdk/`。
- `STEAMWORKS_SDK_TOKEN`：仅具有该私有仓库读取权限的 fine-grained token。
- `STEAM_ACCOUNT`：对主应用、Playtest 与专用服务器 AppID 具有编辑、发布以及修改 `internal` live 分支权限的 Steam Partner 构建账号。
- `STEAMCMD_AUTH_B64`：在可信 Linux 机器上完成 SteamCMD 交互登录后，将 SteamCMD 的 `config/config.vdf` 及可选的 `ssfn*` 文件打包并进行 base64 编码的内容。不得将该档案提交到仓库，也不要打包包含浏览器缓存的整个 `config/` 目录。

先使用发布账号完成一次 SteamCMD 交互登录。独立版 SteamCMD 会将登录状态写入
`~/Steam/config/`，而不是 `steamcmd.sh` 所在目录。随后在任意临时目录执行：

```bash
auth_root="$HOME/Steam"
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

`ssfn*` 文件并非每次登录都会生成；以上命令会在它们存在时自动加入，但始终只从
`config/` 中包含 `config/config.vdf`，不会导出体积很大的 `config/htmlcache/`。使用
`tar -tzf steamcmd-auth.tar.gz` 检查归档后，将
`steamcmd-auth.txt` 的内容保存为 `STEAMCMD_AUTH_B64`，随后删除两个本地导出文件。
可以通过 Environment protection rules 为每次上传增加人工批准，而无需修改 workflow。
