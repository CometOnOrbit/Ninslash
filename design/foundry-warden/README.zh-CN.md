# 熔炉监工 · Foundry Warden

一台四足重型废料拆解机：液压钳压迫近身空间，熔渣迫击逼玩家换位；连续重攻击后开甲散热，暴露橙色炉芯作为集中输出窗口。

**本次提交完成美术、骨架、九组动作与离线预览。敌人注册、服务器 AI、碰撞／伤害判定和联机接入仍需后续实现。**

![关键姿势](motion-v2-keyposes.png)

## 查看与使用

- 下载并在本地打开 [preview-motion-v2.html](preview-motion-v2.html)：可对照初版、慢放、拖动时间轴、查看骨骼及挂点、关闭特效、录制当前动作。
- [动作视频节选](motion-v2-showcase.mp4) 展示钳击、重踏和开甲。
- [动作精修说明](MOTION-V2.zh-CN.md) 记录每个动作的时长、时间标记和引擎接入边界。
- 原生文件在 `data/anim/foundry_warden/`，使用 `warden-motion-v2` 同名的 `.json`、`.atlas`、`.png`。
- `.combat.json` 是制作侧的建议参数与挂点说明，不是引擎会自动执行的游戏配置。

## 战斗提案

适合工业主题的中后段房间，以可读的蓄力和反击窗口为核心：横扫抬钳、迫击锁定落点、重踏下蹲起跳。建议连续两次重攻击后强制开甲，45% 生命进入第二阶段；第二阶段增加连招和缩短部分收招，保留前摇与炉芯暴露窗口。

初始建议为 4200 生命，炉芯暴露期间受伤倍率 1.6；横扫、迫击和重踏的基础伤害分别为 18、12、22。这些数值尚未经过对局平衡测试。

建议场地净宽 36–44 格、净高至少 15 格，提供侧上方抓钩支点和可暂避迫击的高台。行走碰撞箱应围绕承重躯干单独定义，不使用包含展开双钳的整幅矩形。

## 制作与验证

54 根骨骼、49 个插槽、26 个贴图区域、九组动作。沿用 Ninslash 粗黑描边、灰白金属和蓝色独眼的视觉语言，采用钳指分段发力、承重底盘／上身分离、脚尖滚动、液压杆伸缩、交替后坐与错峰开甲。

制作输入固定存放在 `source/`。贴图经过图像模型重新绘制；骨骼动作由脚本离线制作。重建动画无需 API 或认证信息。

```sh
python -m pip install -r design/foundry-warden/requirements.txt
python design/foundry-warden/polish_animation.py
python design/foundry-warden/verify_motion.py
```

Python 检查覆盖每动作 121 个导出姿势、足部约束、循环首尾与可见轮廓。启用 CMake 的 `NINSLASH_BUILD_TESTS` 后，`foundry_warden_assets` 使用项目真实的 Spine 读取器检查新资源。具体构建命令见 [English README](README.md)。这些检查验证资源和预览，不能代替游戏内测试。

素材来源、修改和授权见 [ATTRIBUTION.md](ATTRIBUTION.md)。
