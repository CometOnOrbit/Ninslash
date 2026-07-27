# Steam Partner 与人工验收清单

**[English](steam-partner-checklist.md) · 简体中文**

本文只包含需要 Partner 权限、真实 Steam 账号、实体设备、法律决策或分支提升的工作。开始本清单前，应先通过离线验证器和严格素材审计。

## Partner 定义

创建以下成就，不设置进度统计或数值奖励。API 名称属于协议接口，必须完全一致；显示名称和描述可以本地化，不得修改 API 名称。

| API 名称 | 建议英文显示名称 |
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

创建整数统计 `STAT_COOP_COMPLETIONS`，默认值为 `0`，只允许递增。创建排行榜 `Invasion Highest Floor`，排序方式为降序/数值；创建排行榜 `Fixed Seed Clear Time`，排序方式为升序/毫秒时间。名称必须完全一致。

Windows 在 `%APPDATA%/Ninslash`、Linux 在 `$HOME/.ninslash` 下配置 Auto-Cloud，且只同步 `pve_progress.json`。排除设置、备份、临时文件、日志、录像、截图、已下载地图、Workshop 内容和当前对局装备。冲突时选择完整文件，不合并字段。

为 AppID `1812700` 发布 `data/steam_input_manifest.vdf`。确认游戏、菜单、观战和聊天动作集，以及所有本地化按键图标绑定。

为专用服务器 Tool `5016790` 启用 SteamCMD 匿名安装。确认客户端 Depot `1812702`/`1812703` 和服务器 Depot `5016792`/`5016793`。

## 双账号网络验收

- [ ] 仅邀请 Lobby 不出现在公开或好友搜索中，直接邀请可以加入。
- [ ] 仅好友 Lobby 对好友可见，并接受 Overlay 邀请。
- [ ] 公开 Lobby 显示房主、模式、地图、地区、人数、密码、Mod 哈希和非官方状态。
- [ ] 位于另一家庭网络或 NAT 后的账号无需端口转发即可通过 `steam:<HostSteamID64>` 加入。
- [ ] Lobby 所有者变化或房主退出会关闭 Listen Server，不执行未支持的迁移。
- [ ] 缺失的 Workshop 内容会下载并验证，随后要求玩家明确重新加入。
- [ ] 哈希不匹配、依赖冲突和本地禁用 Mod 都会阻止加入。
- [ ] P2P、社区和 Mod 对局绝不上传官方排行榜成绩。

## 专用服务器验收

- [ ] 公开服务器同时出现在旧主列表和 Steam GameServer 列表中，且只显示一次。
- [ ] `sv_register_steam 0` 只移除 Steam 列表；UDP、LAN、直接地址和旧主列表仍可使用。
- [ ] 非 Steam 客户端能以匿名身份加入启用 Steam 且设置 `sv_steam_auth 1` 的社区服务器。
- [ ] `sv_steam_auth 2` 和官方服务器拒绝匿名客户端。
- [ ] 伪造、过期和重放的 Steam 票据在地图传输前被拒绝。
- [ ] Steam 认证服务中断时拒绝声称 Steam 身份的客户端，但可选认证仍允许匿名访问。
- [ ] SteamCMD 能在干净的 Windows 和 Linux 主机上匿名安装并启动 Tool `5016790`。

## 设备、Cloud 与发行验收

- [ ] Steam Input 可在实体手柄上使用，无 Steam 时 SDL 回退正常。
- [ ] 从 Steam 启动后，Windows、Linux 和 SteamOS 均可打开 Overlay。
- [ ] 两台设备生成分歧 Cloud 存档后，任一选定的完整文件均可加载且不会合并。
- [ ] 离线会话后的成就能够补发，并且只由符合条件的服务器事件解锁。
- [ ] 统计和两个排行榜按配置的排序、显示规则更新。
- [ ] 常规 SteamID 管理日志在 90 天后删除，除非处于有记录的保留状态。
- [ ] 法律主体、联系方式和所有素材来源决策均已完成。
- [ ] 上传到 `internal`，在干净设备上安装已上传的 BuildID，然后将同一 BuildID 依次提升至 `beta` 和 `public`。
