# Steam 发行门禁

**[English](README.md) · 简体中文**

本目录记录公开商业分发前必须完成的工作，其中也包括免费的 Steam 发行。构建成功并不能证明内置的美术、音频、字体、地图或动画拥有合法分发权限。

Ninslash 已分配的 Steam AppID 为 `1812700`。

## 素材来源

`asset_licenses.csv` 按从宽泛默认规则到精确覆盖规则的顺序排列，最后一个匹配规则生效。只有在来源、作者、许可证和必要署名均经过验证后，条目才能标记为 `approved`。标记为 `review_required` 或 `rejected` 的文件会阻止发行构建。

查看审核进度：

```sh
python3 scripts/audit_release_assets.py
```

执行发行门禁：

```sh
python3 scripts/audit_release_assets.py --strict
```

暂存 Depot 并渲染 SteamPipe 清单后，执行离线 Depot 和二进制门禁。传入所有可用的候选发行路径：

```sh
python3 scripts/verify_steam_release.py \
  --manifests dist/steampipe \
  --linux-client dist/steam/linux-client \
  --linux-server dist/steam/linux-server \
  --windows-client dist/steam/windows-client \
  --windows-server dist/steam/windows-server \
  --standalone-linux-client build-standalone/ninslash \
  --standalone-linux-server build-standalone/ninslash_srv
```

Partner 配置及账号、设备检查见 `docs/release/steam-partner-checklist_zh-CN.md`。只有在离线门禁通过之后、提升 `internal` BuildID 之前才执行这些检查。

项目所有者已确认：除非文件带有更具体的第三方许可证，`data/**` 默认采用 Ninslash 素材许可证 CC-BY-SA 3.0。宽泛的已批准规则记录这一默认值；后续精确规则会覆盖第三方字体等文件。

## 玩家数据

首个发行版本中，Steam Cloud 唯一同步的文件是 `pve_progress.json`：

- Windows 根目录：`%APPDATA%/Ninslash`
- Linux 根目录：`$HOME/.ninslash`
- Cloud 匹配规则：`pve_progress.json`
- Cloud 设置必须排除 `.bak`、`.tmp`、设置、密码、日志、截图、录像、视频和已下载地图。
- 当 Steam 无法判断哪个副本更新时，将 Auto-Cloud 冲突处理配置为询问玩家。Ninslash 只读取 Steam 最终选定的完整 `pve_progress.json`，不会合并两个分歧存档的字段，以免重复研究进度。
- 冲突测试：在两个测试账号或设备上生成不同但有效的存档，第二次写入时保持 Steam 离线；重新联网后分别选择两侧文件，确认选择的完整文件能加载，另一文件不会被静默合并。然后使用损坏文件和未来架构版本文件重复测试，确认备份恢复和只读保护有效。

`pve_progress.json` 刻意采用本地玩家权威模型。它不得包含当前对局武器、金币、物品栏、服务器凭据、IP 地址或聊天数据。

官方服务器日志使用已验证的 SteamID64 作为管理主键，并记录认证状态、房间类型和 Mod 哈希。`steam_bans.cfg` 持久保存 SteamID 封禁。运营日志轮转必须在 90 天后删除常规记录；正在进行的滥用调查和法律保留是有文档记录的例外。
