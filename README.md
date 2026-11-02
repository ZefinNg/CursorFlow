# CursorFlow

轻量级鼠标键盘活动统计工具：统计你每天推动鼠标走了多少米、敲了多少次键。

**只计数，不记内容** —— 不记录按键文本、不记录坐标轨迹、不联网、不上传，所有数据仅保存在本地（MIT 开源，可自行审计）。

## 功能

- **今日概览**：鼠标距离 / 按键总数 / 活跃时长，光标轨迹粒子动画，24 小时热力分布
- **历史趋势**：近 7 / 30 天双轴折线图（距离 + 按键），悬停查看明细
- **校准**：显示器英寸 + 分辨率 → PPI，米数按真实物理尺寸换算
- **托盘常驻**：暂停/恢复统计、今日摘要、退出二次确认；关闭窗口即最小化
- **导出**：CSV（UTF-8 BOM，Excel 直开）/ JSON（逐日 + 逐时）
- **i18n**：简体中文 / English，深浅色主题，字体缩放，动效开关
- **跨平台采集**：Windows 低级钩子 / macOS CGEventTap（含辅助功能权限引导）/ Linux XInput2（Wayland 明确降级提示）

## 下载安装

见 [docs/用户手册.md](docs/用户手册.md)（含隐私说明与常见问题）。

- Windows：`CursorFlow-x.y.z-win64.exe`（NSIS 安装器）
- macOS：`CursorFlow-x.y.z.dmg`
- Linux：`CursorFlow-x.y.z.AppImage`

> 注：Windows 低级键盘钩子可能触发杀软误报，请加入白名单；软件不包含任何网络代码。

## 从源码构建

依赖：CMake ≥ 3.16、C++17 编译器、Qt 5.15.2（Quick / QuickControls2 / Sql(SQLite)）。

```bash
# Windows（VS 2019 + Qt 5.15.2 msvc2019_64）
cmake -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\msvc2019_64"
cmake --build build --config Release
cd build; ctest -C Release; cd ..

# macOS / Linux（Linux 需 libx11-dev libxi-dev）
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt安装目录>
cmake --build build -j
cd build; ctest; cd ..
```

安装规则内置 windeployqt / macdeployqt 依赖收集与包体剪裁；`cpack` 生成 NSIS / DMG（Linux AppImage 用 `packaging/linux/build-appimage.sh`）。正式发布的 <15MB 安装包采用 CI 静态 Qt 裁剪编译（见 docs/tech/05）。

## 文档

- [产品需求（PRD）](docs/CursorFlowPRD.md)
- [用户手册（含隐私说明）](docs/用户手册.md)
- 技术设计：[架构](docs/tech/01-技术架构设计.md) · [采集](docs/tech/02-数据采集模块设计.md) · [存储](docs/tech/03-数据存储设计.md) · [UI](docs/tech/04-UI实现方案.md) · [构建打包](docs/tech/05-构建与打包方案.md) · [开发计划](docs/tech/06-开发计划与任务拆解.md) · [性能达标报告](docs/tech/性能达标报告.md)

## 许可证

[MIT](packaging/LICENSE.txt) © 2026 CursorFlow contributors。Qt 在 LGPL v3 下使用。
