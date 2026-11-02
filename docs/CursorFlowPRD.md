# 鼠标键盘活动统计工具 PRD（Product Requirements Document）

## 1. 项目概述
一款轻量级的桌面统计工具，运行于 Windows/macOS/Linux，实时记录并可视化每日鼠标移动距离（单位：米/千米）和键盘按键次数（含左键/右键/滚轮）。强调 UI 美观、低资源占用、隐私安全。

## 2. 目标用户
- 程序员、设计师、重度办公用户，希望了解自己日常操作强度
- 对个人生产力数据感兴趣的极客
- 需要长时间使用电脑的健康监测用户（配合休息提醒）

## 3. 核心功能需求

### 3.1 数据采集（后台常驻服务）
- **鼠标移动距离**：通过全局钩子获取每次鼠标位置变化，计算欧氏距离并累加。过滤掉因多屏切换、远程桌面导致的异常跳变（超过屏幕对角线长度 2 倍视为无效）。
- **键盘按键次数**：统计所有按键按下次数（不含组合键修饰符重复计数）。仅累计总数，不记录具体按键内容，保证隐私。
- **鼠标点击次数**：分别统计左键、右键、中键点击次数。
- **滚轮滚动量**：统计滚轮滚动步数（向上为正，向下为负，绝对值累加）。
- **数据持久化**：每小时自动写入 SQLite 数据库，按天分表存储。

### 3.2 主界面仪表盘
- **今日概览**：大数字显示当日鼠标距离、按键总数、活跃时长（基于输入间隔推断）。
- **历史趋势**：折线图展示过去 7/30 天的日统计数据。
- **小时热力图**：24 小时方格热力图，颜色深浅表示该小时的操作密集程度。
- **实时动态**：鼠标移动轨迹的轻微粒子动画（可选开关，减少 GPU 消耗）。

### 3.3 托盘菜单
- 隐藏/显示主窗口
- 暂停/恢复统计（临时停止采集）
- 查看今日统计摘要
- 退出程序（带二次确认）

### 3.4 设置面板
- 通用：开机自启、语言（中文/英文）、数据保留天数（自动清理旧数据）
- 显示：主题（深色/浅色/跟随系统）、字体大小、动画开关
- 校准：屏幕尺寸（英寸）与分辨率 → 自动计算每像素对应物理距离，提高鼠标距离精确度
- 导出：将统计数据导出为 CSV / JSON

### 3.5 隐私与安全
- 所有数据仅存储在本地 SQLite，不上传云端。
- 键盘仅统计次数，不记录字符序列。
- 首次启动弹出隐私声明，用户同意后才开始采集。

## 4. 非功能需求
- **性能**：后台进程 CPU 占用 < 1%（空闲时），内存 < 50 MB。
- **响应速度**：主窗口打开 < 500ms，图表渲染 < 200ms。
- **兼容性**：Windows 10/11，macOS 12+，Ubuntu 20.04+。
- **安装包大小**：< 15 MB（静态编译 Qt 核心库）。

## 5. 技术选型

| 层面 | 技术 | 理由 |
|------|------|------|
| 编程语言 | C++17 | 高性能、系统级 API 调用 |
| UI 框架 | Qt 5.15.2 (QML + Quick Controls 2) | 原生质感、流畅动画、跨平台一致，LTS 版本稳定 |
| 全局钩子 | Windows: `SetWindowsHookEx` / macOS: `CGEventTap` / Linux: `XInput2` | 标准系统 API，稳定可靠 |
| 数据存储 | SQLite via Qt SQL module | 零配置、单文件、嵌入式 |
| 图表库 | Qt Charts 或自定义 Canvas 绘制 | 轻量，无需第三方 JS 引擎 |
| 构建工具 | CMake + Ninja | 跨平台构建标准 |
| 打包工具 | CPack (NSIS for Win, DMG for Mac, AppImage for Linux) | 原生安装体验 |

## 6. 架构设计
┌─────────────────────────────┐

│        QML UI Layer         │

│  (Dashboard, Settings, etc) │

├─────────────────────────────┤

│      C++ Controller         │

│  (DataManager, ChartModel)  │

├─────────────────────────────┤

│    InputCapture Thread      │

│  (Platform-specific hooks)  │

├─────────────────────────────┤

│       Storage Layer         │

│  (SQLite via Qt SQL)        │

└─────────────────────────────┘
- **InputCapture**：独立线程运行，使用锁保护共享计数器，每隔 3 秒将增量提交给 DataManager。
- **DataManager**：负责聚合、定时写入数据库、对外提供查询接口（信号槽通知 UI 更新）。
- **UI**：QML 绑定 C++ 对象属性，实现数据驱动刷新。

## 7. 数据模型（SQLite）
sql

CREATE TABLE daily_stats (

date TEXT PRIMARY KEY,          -- '2026-10-30'

mouse_distance_m REAL DEFAULT 0,

key_press_count INTEGER DEFAULT 0,

left_click_count INTEGER DEFAULT 0,

right_click_count INTEGER DEFAULT 0,

scroll_steps INTEGER DEFAULT 0,

active_minutes INTEGER DEFAULT 0

);

CREATE TABLE hourly_stats (

id INTEGER PRIMARY KEY AUTOINCREMENT,

date TEXT NOT NULL,

hour INTEGER NOT NULL CHECK(hour BETWEEN 0 AND 23),

mouse_distance_m REAL DEFAULT 0,

key_press_count INTEGER DEFAULT 0

);
## 8. 界面设计原则（UI 美化）

- **主题**：默认深色磨砂玻璃效果（Acrylic），支持浅色模式。
- **配色**：低饱和度渐变色（蓝紫→青绿），字体使用 Inter 或系统 San Francisco。
- **布局**：顶部大数字卡片（今日数据），中间双折线图（距离+按键），底部热力图。
- **动效**：页面切换平滑过渡，数字滚动更新，鼠标悬停图表显示详情。
- **图标**：统一使用 Material Design Icons 或 Font Awesome（内嵌 SVG）。

## 9. 开发计划

### Phase 1：MVP（4 周）
- 完成 Windows 平台全局钩子 + 数据采集
- 实现 SQLite 存储与基础查询
- 搭建 QML 主界面，显示今日数字（无图表）
- 托盘图标 + 最小化功能

### Phase 2：可视化增强（2 周）
- 集成 Qt Charts 绘制历史折线图、热力图
- 添加设置面板（主题切换、开机自启）
- 完善 macOS / Linux 钩子适配

### Phase 3：打磨与发布（2 周）
- 国际化（中/英）
- 导出 CSV
- 性能优化、内存泄漏检测
- 打包安装程序，编写用户文档

## 10. 风险与对策

| 风险 | 对策 |
|------|------|
| 杀毒软件误报全局钩子 | 代码签名证书、提交白名单申请、开源代码审核 |
| macOS 权限弹窗导致首次失败 | 引导用户前往系统偏好设置开启辅助功能权限 |
| 高 DPI 下鼠标距离偏差 | 提供屏幕尺寸手动校准功能，默认按 96 DPI 估算 |
| 用户误以为记录键盘内容 | 首页显著位置显示“仅统计次数，不记录内容”提示 |

## 11. 附录：技术参考

- [Qt 5.15 官方文档](https://doc.qt.io/qt-5/)
- [Qt 5.15 QML 教程](https://doc.qt.io/qt-5/qml-tutorial.html)
- [Windows Low-Level Keyboard Hook](https://docs.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc)
- [macOS Event Tap](https://developer.apple.com/documentation/coregraphics/cgeventtap)
- [Linux XInput2 示例](https://www.x.org/releases/X11R7.7/doc/libXi/xi2proto.txt)

---