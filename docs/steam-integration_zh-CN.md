# Steam 集成边界

**[English](steam-integration.md) · 简体中文**

默认构建仍使用现有 UDP 客户端和 `ninslash_srv` 服务器。Steam 支持需要主动启用：客户端传入 `-DENABLE_STEAMWORKS=ON`，专用服务器传入 `-DENABLE_STEAM_GAMESERVER=ON`，并让 `STEAMWORKS_SDK_ROOT` 指向本地安装的 SDK。本仓库绝不保存 SDK 文件、登录令牌或 Web API 密钥。

Steam 客户端、GameServer、内嵌 Listen Server 和 Lua 沙箱的开发配置如下：

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

已发布的 SteamPipe 映射为：客户端 AppID `1812700`，Windows/Linux Depot `1812702`/`1812703`；专用服务器 Tool AppID `5016790`，Windows/Linux Depot `5016792`/`5016793`。这些值作为 CMake 缓存变量公开，并渲染到 `<build>/steam/` 下可用于构建的 VDF 文件；暂存的 Depot 根目录位于源码树之外。

Steam 是可选的身份与发现服务，不是基础网络。`sv_register` 继续控制旧主列表，`sv_register_steam` 独立控制 Steam GameServer 广告。Steam 构建和独立构建均保留 LAN 发现与直接 UDP 地址。

`sv_steam_auth` 有三种模式：`0` 为开放模式，不记录可信 Steam 身份；`1` 接受明确选择匿名身份的独立客户端，但声称 Steam 身份的客户端必须提供有效票据；`2` 要求已验证的 Steam 身份。无效、重放、过期、超时或服务不可用的已声明 Steam 身份绝不降级为匿名。`sv_official 1` 强制使用模式 `2`、同时启用两种发现注册并拒绝 Mod 配置。独立服务器构建可运行模式 0 和 1，但会拒绝以模式 2 或官方模式启动。
客户端会等待 `GetAuthSessionTicketResponse_t` 后再发送新票据；服务器为两段异步 Steam 回调提供 30 秒处理窗口。

平台认证请求、响应和结果消息追加在旧系统消息枚举末尾。版本和密码验证后，只要启用了认证或 Mod 验证，服务器就会在地图传输前要求客户端明确选择 `anonymous` 或 `steam` 身份。响应包含身份类型、SteamID、不透明票据和已安装 Mod 集合。设置 `sv_mod_hash` 的服务器只允许完全匹配。这能防止认证成功前创建地图角色、产生 PvE 结果或消耗进度。

Workshop 内容必须包含 `ninslash_mod.json`。Steam UGC 安装内容会先被验证，再通过临时且重新验证的目录复制到用户存储 `workshop/<PublishedFileID>/`；内嵌服务器和专用服务器只从该独立目录加载内容。验证会拒绝绝对路径、路径穿越、符号链接或重解析点、未声明文件、原生可执行文件或动态库、无效身份或哈希、协议不匹配和超大包。依赖必须精确匹配 PublishedFileID、版本与内容哈希。集合哈希稳定，并按依赖顺序生成。

每份清单必须将 `content_rating` 声明为 `everyone`、`teen` 或 `mature`，同一值也会用作 Workshop 发布标签。最小清单如下：

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

启用 `ENABLE_LUA_MODS` 后，玩法脚本只在专用服务器或内嵌 Listen Server 中执行。每个 Mod 拥有独立的 Lua 5.4 状态、确定性随机数、内存上限和单事件指令预算。沙箱不提供 `io`、`os`、`debug`、`package`、原生模块、网络访问或文件写入。脚本错误或预算超限会停用对应 Mod 运行时。客户端从不执行玩法 Lua；带版本的 API 为未来规则、武器、物品、资源和客户端主题预留能力。

Steam 客户端构建可通过 `steam_lobby_create [invite|friends|public]` 托管内嵌 Listen Server。房主通过回环 UDP 加入；受邀客户端读取 Lobby 的 `steam:<HostSteamID64>` 连接值，并在虚拟端口 1 上使用 Steam NetworkingSockets P2P。`steam_lobby_invite` 打开 Steam 邀请对话框，`steam_lobby_leave` 关闭房间。不支持房主迁移：Lobby 所有权丢失时会关闭内嵌服务器。独立专用服务器和 LAN 浏览器继续使用 UDP，不受影响。控制台命令 `steam_lobby_status` 可显示当前 Lobby ID。

互联网服务器浏览器使用基础游戏 AppID `1812700` 查询 Steam GameServer 列表。Tool AppID `5016790` 只作为 SteamCMD/Depot 分发容器；服务器以基础游戏身份初始化认证与发现，使客户端票据和服务器处于同一应用上下文。返回的终端地址按 `NETADDR` 与 UDP 主列表合并，重复服务器只显示一次，并根据已认证的 GameServer 标签显示 `[OFFICIAL]`、`[COMMUNITY]` 或 `[COMMUNITY MODDED]`。

使用 `steam_lobby_refresh` 查询公开房间；异步回调完成后可用 `steam_lobby_list` 查看，并通过 `steam_lobby_join <LobbyID>` 加入。结果包含房主身份和名称、模式、地图、地区、人数、密码、Mod 与好友房间状态。协议、所有者、连接值或 Mod 元数据无效时，绝不会接受该加入目标。

Workshop 诊断和选择命令如下：

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

Linux 上需要从 Steam 启动游戏才能使用 Overlay。从 Steam 外启动开发二进制时，应在启动前预加载 Steam 的 64 位渲染器：

```sh
LD_PRELOAD="$HOME/.local/share/Steam/ubuntu12_64/gameoverlayrenderer.so" \
  /tmp/ninslash-steam-build/ninslash
```

Steam 安装在 `~/.steam` 时，对应渲染器通常位于 `$HOME/.steam/ubuntu12_64/gameoverlayrenderer.so`。架构必须与 64 位游戏匹配。Steam 必须已运行，且登录的测试账号必须拥有该游戏。

客户端通过 RUNPATH 链接 SDK 中的 `libsteam_api.so`。`steamclient.so` 由 Steam 自身提供，标准安装通常将其放在 `~/.local/share/Steam/linux64/`。如果 SDK 运行时仍探测 `~/.steam/sdk64/`，请在本地机器上创建目录或符号链接，不要提交到仓库：

```sh
mkdir -p "$HOME/.steam/sdk64"
ln -sf "$HOME/.local/share/Steam/linux64/steamclient.so" \
  "$HOME/.steam/sdk64/steamclient.so"
```

诊断行 `Loaded '.../linux64/steamclient.so' OK` 表示回退路径成功。后续的 `SteamInternal_SetMinidumpSteamID` 只是信息，并不表示初始化失败；应以游戏日志中的 `[steam]: initialized for user ...` 判断 `SteamAPI_Init` 是否完成。

内嵌服务器使用不可变的房间设置快照。房间运行期间会重新应用网络、认证、地图、模式、密码、人数上限和 Mod 值，关闭后恢复此前的进程全局值。这能在无需将所有旧配置读取重写为实例所有权系统的前提下，防止运行期配置突变。

十二个成就、合作完成统计、Invasion 深度和固定种子 Horde 时间排行榜均由服务器确认事件驱动。失败的客户端 API 操作持久保存在 `steam_pending_events.dat` 中等待重试。只有官方、强制认证、协议匹配、无修改且无调试参数的专用服务器对局才能提交排行榜；Listen/P2P 和社区对局均被排除。

专用服务器管理命令为 `steam_ban <SteamID64> [minutes] [reason]`、`steam_unban <SteamID64>` 和 `steam_bans`。认证和管理日志包含 SteamID64、结果、房间类型和 Mod 哈希。

Steam 菜单提供独立的房间和 Workshop 视图，覆盖公开房间刷新与加入、好友房间创建与邀请、安装与验证状态、下载与上传进度、集合选择、本地启用或禁用、取消订阅、物品创建，以及经过验证的内容和预览图发布。Steam Workshop 物品页面仍是接受法律协议、社区举报、版权投诉和内容管理的权威入口。发布者必须在 Partner/Community 工具中选择准确的 Steam 内容描述；游戏不会根据任意 Mod 素材自行推断分级。
