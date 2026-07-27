# SteamPipe 模板

**[English](README.md) · 简体中文**

Ninslash 使用 AppID `1812700`；Ninslash Dedicated Server 使用 Tool AppID `5016790`。已分配的 Windows/Linux 客户端 Depot 为 `1812702`/`1812703`，Windows/Linux 专用服务器 Depot 为 `5016792`/`5016793`。不要提交 Steam 凭据、缓存的登录令牌或仅限 Partner 使用的 SDK 文件。

以下包装脚本会配置并重新编译两个平台构建、暂存四个 Depot、渲染 VDF 并运行离线验证器。Windows 构建目录不存在时，会使用仓库内的 MinGW64 toolchain 自动创建。除非明确传入 `--upload`，否则不会上传：

```sh
python3 scripts/publish_steam_depots.py \
  --linux-build-dir build \
  --windows-build-dir build-windows-steam \
  --sdk-root "$HOME/sdk"
```

使用 SteamCMD 上传已验证的客户端和专用服务器构建：

```sh
python3 scripts/publish_steam_depots.py \
  --linux-build-dir build \
  --windows-build-dir build-windows-steam \
  --sdk-root "$HOME/sdk" \
  --upload --steam-account YOUR_PARTNER_ACCOUNT
```

包装器永不接收密码。密码和 Steam Guard 交互由 SteamCMD 自行处理。需要时可设置环境变量 `STEAM_ACCOUNT` 和 `STEAMCMD`；使用 `--no-build` 打包已有二进制，使用 `--strict-assets` 执行最终公开发行素材门禁。
使用 `--upload-target client` 或 `--upload-target server` 可以只重试一个 AppID，避免为已经成功的 AppID 再创建一次构建。
添加 `--set-live internal`（或其他已配置的 beta 分支名）可在上传后自动让新 BuildID 在该分支生效。SteamPipe 不能自动将公开 `default` 分支设为 live；应省略 `--set-live` 完成上传，再到 Steamworks App Admin 的 Builds 页面手动提升该 Build。
排查 commit 失败时应先省略 `--set-live`。上传权限可以创建 Build，但修改 live 分支可能需要额外的 Steamworks 发布权限。成功创建并测试 Build 后，再单独提升分支。

暂存时会把 SDL3、pnglite 等直接依赖的非系统 Linux 运行库复制到可执行文件旁。不要依赖只安装在构建机上的库：Steam Linux Runtime 不继承 `/usr/local/lib`，也不提供 `libpnglite.so.0` 等项目专用库。
客户端 Depot 还会包含 `ninslash_srv`/`ninslash_srv.exe`，因为“本地游戏”菜单会启动这个同目录子进程。这一有意的重复与专用服务器 Tool Depot 相互独立。

SteamCMD 报告 `No Connection` 时，先运行 `steamcmd +login anonymous +quit` 测试匿名连接。使用 Fake-IP/TUN 的代理软件必须代理 SteamCMD 进程以及 Steam TCP/UDP 流量；否则应暂时关闭代理，使 Steam 域名解析到真实地址而非 `198.18.0.0/15`。上传失败不会使已渲染的清单失效；修复网络后可直接重新执行两个 `+run_app_build` 命令。

使用已登记 ID 和私有绝对路径渲染可上传模板：

```sh
python3 scripts/render_steam_build.py --output dist/steampipe \
  --build-output /private/steam-output --content-root /private/content \
  --windows-client-root dist/steam/windows-client \
  --linux-client-root dist/steam/linux-client \
  --windows-server-root dist/steam/windows-server \
  --linux-server-root dist/steam/linux-server
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
```

分别使用 `internal`、`beta` 和 `public` 分支。先上传到 `internal`，通过 Steam 在干净设备上安装并执行发行测试矩阵，之后再提升同一 BuildID；不要为分支提升单独重新构建。

上传前，对渲染后的清单、已暂存的 Steam Depot 和独立版二进制运行 `scripts/verify_steam_release.py`。`sv_register_steam` 只控制 Steam 广告；`sv_register` 保留开放的旧主列表通道。
