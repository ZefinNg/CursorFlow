# CursorFlow GIF 动效与按键速度设计

> 关联 PRD：3.10 实时按键速度与动态 GIF 展示（M7，任务 9.x）
> 上游设计：[01-技术架构设计.md](01-技术架构设计.md)、[02-数据采集模块设计.md](02-数据采集模块设计.md)
> UI 落点：[04-UI实现方案.md](04-UI实现方案.md) §2.1 Dashboard 实时动态区 / §2.7 悬浮窗

## 1. 目标与边界

### 1.1 核心目标

将按键速度（CPM，Characters Per Minute）这一实时指标，以 ≤ 500 ms 的延迟转化为 GIF 动画的播放速度倍率，为用户提供打字节奏的即时视觉反馈。

### 1.2 设计约束

| 约束 | 原因 |
|------|------|
| 复用 Qt 5.15 基础设施，不引入新的三方库 | 控制包体（05 号文档目标 < 15 MB） |
| 不修改现有 3s flush 采集链路 | 保证今日统计、历史数据、成就系统等既有功能不受影响 |
| GIF 速度调节基于 QMovie 原生能力 | Qt 5.15 无 AnimatedImage.speed（Qt 6.2+ 才有），QMovie::setSpeed() 是现成方案 |
| 隐私红线不变 | 速度计算只需要按键时间戳，键值内容不进入任何缓冲区（02 号文档 §6 约束延续） |

### 1.3 模块边界

```
采集线程                          主线程 (UI)
┌─────────────────────┐          ┌──────────────────────────┐
│ InputCaptureWorker  │          │ AppSettings (gif/*)      │
│  ├─ 原子按键计数器    │          │ SpeedMapper (纯算法类)    │
│  ├─ 上次按键时间戳    │──250ms──▶│ KeySpeedController      │
│  └─ keySpeedTimer   │   signal  │  ├─ CPM 平滑             │
└─────────────────────┘          │  └─ 速度倍率输出           │
                                 └──────────┬───────────────┘
                                            │ property binding
                                            ▼
                                 ┌──────────────────────────┐
                                 │ GifPlayer (C++, QML 注册) │
                                 │  ├─ 封装 QMovie           │
                                 │  ├─ Q_PROPERTY speed      │
                                 │  └─ frameChanged signal   │
                                 └──────────┬───────────────┘
                                            │ frameChanged
                                            ▼
                                 ┌──────────────────────────┐
                                 │ QML: Image + Timer        │
                                 │  或 QML: AnimatedImage    │
                                 │  根据 frameIndex 切帧     │
                                 └──────────────────────────┘
```

## 2. GifPlayer 组件设计

### 2.1 选型说明

Qt 提供三种 GIF 播放方案：

| 方案 | 速度控制 | Qt 版本要求 | 复杂度 |
|------|----------|------------|--------|
| QML AnimatedImage | `speed` 属性（0.0~∞） | **Qt 6.2+** | 最低，但本项目 5.15 不支持 |
| QMovie（C++） | `setSpeed(int percent)` | **Qt 5.0+** | 低，原生支持百分比调速（1%~400%） |
| QImageReader 手动解码帧 | 完全自控 | Qt 5.0+ | 中高，需自己处理循环/帧延迟 |

本项目选 **QMovie 封装**：原生 API 成熟、跨平台一致、调速精度足够（整数百分比粒度，对应倍率 0.01x~4x，满足需求的 0.1x~4.0x）。

### 2.2 类定义

```cpp
// src/app/GifPlayer.h
class GifPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY sourceChanged)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY frameChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY sourceChanged)

public:
    explicit GifPlayer(QObject* parent = nullptr);
    ~GifPlayer();

    QString source() const;
    void setSource(const QString& path);   // 支持 qrc:/ 和本地文件路径

    double speed() const;                  // 倍率，1.0 = 原速
    void setSpeed(double multiplier);       // 范围 0.1 ~ 4.0，内部转 percent

    bool isPlaying() const;
    void setPlaying(bool play);

    int frameCount() const;
    int currentFrame() const;
    bool isValid() const;

    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();

signals:
    void sourceChanged();
    void speedChanged();
    void playingChanged();
    void frameChanged();                   // 当前帧变化，供 QML 切帧绑定

private slots:
    void onMovieChanged();                 // QMovie::frameChanged / stateChanged

private:
    void ensureMovie();                    // 懒创建 QMovie
    void applySpeed();                     // speed → movie->setSpeed(percent)

    QMovie* movie_ = nullptr;
    QString source_;
    double speed_ = 1.0;                   // 默认原速
    bool playing_ = false;
    bool valid_ = false;
    static constexpr double kSpeedMin = 0.1;
    static constexpr double kSpeedMax = 4.0;
};
```

### 2.3 实现要点

```cpp
// 核心调速逻辑
void GifPlayer::setSpeed(double multiplier) {
    if (qFuzzyCompare(speed_, multiplier)) return;
    speed_ = qBound(kSpeedMin, multiplier, kSpeedMax);
    applySpeed();
    emit speedChanged();
}

void GifPlayer::applySpeed() {
    if (!movie_) return;
    // QMovie::setSpeed 接受百分比整数：100 = 原速，200 = 2x
    int percent = qRound(speed_ * 100.0);
    movie_->setSpeed(qBound(1, percent, 400));
}
```

- **线程亲和性**：`QMovie` 必须在创建它的线程（本项目为 UI 主线程）内使用，这已被 Qt 的 QObject 线程亲和性机制保证。
- **资源管理**：GIF 文件加载失败时 `valid_=false`、`frameCount=0`，QML 层可据此显示占位图。
- **大文件保护**：`setSource` 时检查文件大小 > 5 MB 则拒绝加载（`valid_=false`，`sourceChanged` 携带错误提示的 Q_PROPERTY 可选，首期可简化）。
- **QML 注册**：`main.cpp` 中 `qmlRegisterType<GifPlayer>("CursorFlow.GifPlayer", 1, 0, "GifPlayer")`。

### 2.4 QML 使用方式

```qml
import CursorFlow.GifPlayer 1.0

GifPlayer {
    id: gif
    source: "qrc:/assets/image/dog.gif"
    speed: gifSpeedController.currentMultiplier   // 来自速度映射层
    playing: true

    onFrameChanged: {
        // 更新 Image 组件显示当前帧（两种方案选其一）
        // 方案 A：手动切帧（适用于需要精确帧索引的场景）
        // image.source = gif.frameAt(gif.currentFrame)  // 需额外辅助函数
    }
}

// QML Image 组件的两种帧驱动方案：
// 方案 1：利用 QMovie::currentPixmap() —— 简单但需要 QML C++ 桥接
// 方案 2：QML Timer + GifPlayer.frameCount/currentFrame 属性切换
```

**推荐的最简方案**：不手动切帧，而是直接把 `GifPlayer` 内嵌到一个小的 QML C++ 辅助组件中，用 `QMovie::lastFrame()` 配合 `QImage` 转换为 QML 可绑定的 `Q_PROPERTY(QPixmap currentFrame)` —— 但这会让组件变复杂。

**更简洁的方案**：让 `GifPlayer` 暴露一个 `QImage` 属性供 Image.source 绑定：

```cpp
// GifPlayer 追加：
Q_PROPERTY(QImage currentFrameImage READ currentFrameImage NOTIFY frameChanged)
QImage currentFrameImage() const { return movie_ ? movie_->currentPixmap().toImage() : QImage(); }
```

```qml
Image {
    source: "image://gif/frame"   // 或直接绑定 GifPlayer.currentFrameImage
    // 但 QImage → QML Image 需要走 QQuickImageProvider
}
```

**最终推荐实现**：为 `GifPlayer` 提供一个 `QQuickImageProvider` 派生类，注册为 `gif` provider，QML 用 `Image { source: "image://gif/current" }` 绑定。这条链路是 Qt 官方推荐的 C++ 图像数据传递到 QML 的方式。

## 3. 按键速度采集通道

### 3.1 现状分析

当前 [InputCaptureWorker.cpp](file:///d:/githubProjects/CursorFlow/src/capture/InputCaptureWorker.cpp#L150-L162) 的 `handleKeyDown` 仅做原子累加：

```cpp
void InputCaptureWorker::handleKeyDown(const HookEvent& event) {
    if (event.isAutoRepeat) return;
    if (event.isModifier) { pendingModifiers_ += 1; return; }
    pendingModifiers_ = 0;
    keyPresses_.fetch_add(1, std::memory_order_relaxed);
}
```

要计算 CPM，需要额外维护：
1. **上一次按键的时间戳**（`std::atomic<qint64> lastKeyDownMs_`）—— 但单独一个时间戳不够，需要滑动窗口内的按键时间点序列
2. **滑动窗口按键计数** —— 例如"最近 3 秒内按了多少键"

### 3.2 设计：独立高速通道

现有 3s flush 是"增量累加 → 持久化"的链路，不适合驱动需要 < 1s 响应的 UI。我们在 `InputCaptureWorker` 上**并行增加一个独立的高速推送通道**：

```cpp
// InputCaptureWorker 新增成员
private:
    // 按键速度相关（与现有 keyPresses_ 原子计数器并行，独立不干扰）
    std::atomic<qint64> lastKeyDownMs_{0};        // 最近一次 KeyDown 时间戳
    // 滑动窗口：存储最近 N 个按键时间戳（循环缓冲区）
    static constexpr int kSpeedWindowMaxKeys = 200;
    std::array<qint64, kSpeedWindowMaxKeys> keyTimeBuf_{};
    int keyTimeBufWriteIdx_ = 0;                  // 仅采集线程写，无需 atomic
    QTimer* keySpeedTimer_ = nullptr;             // 250ms 周期，采集线程内

signals:
    // 新增：速度推送（独立于 deltaReady，跨线程队列信号）
    void keySpeedUpdated(double cpm);              // 当前 CPM 值

private slots:
    void onKeySpeedTimer();                        // 250ms 触发一次
```

### 3.3 CPM 计算逻辑

```cpp
void InputCaptureWorker::handleKeyDown(const HookEvent& event) {
    // ... 现有逻辑保留（isAutoRepeat / isModifier 过滤）...
    
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    lastKeyDownMs_.store(now, std::memory_order_relaxed);
    
    // 写入滑动窗口（仅采集线程访问，无需锁）
    keyTimeBuf_[keyTimeBufWriteIdx_] = now;
    keyTimeBufWriteIdx_ = (keyTimeBufWriteIdx_ + 1) % kSpeedWindowMaxKeys;
    
    keyPresses_.fetch_add(1, std::memory_order_relaxed);  // 原有累加不变
}

void InputCaptureWorker::onKeySpeedTimer() {
    // 计算最近 3 秒内的按键次数 → CPM
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 windowStart = now - 3000;  // 3 秒窗口（起停响应 < 3s）
    
    // 遍历循环缓冲区，统计落在窗口内的按键
    int count = 0;
    int total = qMin(keyTimeBufWriteIdx_, kSpeedWindowMaxKeys);
    for (int i = 0; i < total; ++i) {
        if (keyTimeBuf_[i] >= windowStart) {
            ++count;
        }
    }
    
    // CPM = count / 3s × 60s = count × 20
    double cpm = count * 20.0;
    // 停手判定：1s 内无按键 → cpm=0（GIF 约 1.25s 内暂停，不等窗口滑空）
    const qint64 lastKey = lastKeyDownMs_.load(std::memory_order_relaxed);
    if (lastKey > 0 && now - lastKey > 1000) {
        cpm = 0.0;
    }
    
    emit keySpeedUpdated(cpm);
}
```

### 3.4 线程安全

- `lastKeyDownMs_`：`std::atomic<qint64>`，采集线程写、250ms timer 读 → 无锁安全。
- `keyTimeBuf_` 和 `keyTimeBufWriteIdx_`：均仅在采集线程访问（`handleKeyDown` 由钩子回调在采集线程调用，`keySpeedTimer_` 也在采集线程触发）→ 无需加锁。
- `keySpeedUpdated(cpm)` 信号：跨线程队列信号，Qt 自动排队到主线程。
- **暂停联动**：`pause()` 时 `keySpeedTimer_` 停止推送（或推送 cpm=0），`resume()` 后重置时间基线。

### 3.5 与现有 flush 的关系

```
采集线程
├── handleKeyDown
│   ├── keyPresses_++          → 3s flush → DataManager → 今日累计（不变）
│   ├── keyTimeBuf_ 写入       → 3s 窗口内的按键时间戳
│   └── lastKeyDownMs_ 更新   → 活跃推断 & 空闲判定复用
│
├── 3s flush Timer → deltaReady (现有的)
│
└── 250ms keySpeedTimer → keySpeedUpdated(cpm) (新增的高速通道)
```

两条通道**完全独立**，互不干扰：
- flush 通道负责"**数量**"（累计按键数 → 持久化）
- keySpeed 通道负责"**节奏**"（实时速度 → UI 动效）

## 4. 速度映射算法

### 4.1 三层数据流

```
采集线程 CPM ──▶ KeySpeedController ──▶ GifPlayer.speed
                 │
                 ├── 空闲判定（< cpmMin 或超时）
                 ├── 映射模式（线性 / 分段）
                 └── EMA 平滑
```

`KeySpeedController` 是一个新的 QML 单例（`ContextProperty`），负责把原始 CPM 转换为 GIF 速度倍率。也可以直接把这个逻辑内联在 `AppSettings` 的 Q_INVOKABLE 方法 + 一个 Timer 中，避免新增 Controller 类。

**推荐方案：内联到 AppSettings**（减少文件数和复杂度）：

```cpp
// AppSettings 新增
Q_PROPERTY(double gifCurrentSpeed READ gifCurrentSpeed NOTIFY gifSpeedChanged)

double gifCurrentSpeed() const { return gifCurrentSpeed_; }

// 接收 keySpeedUpdated 信号的槽：
void AppSettings::onKeySpeedUpdated(double cpm) {
    // 1. 空闲判定
    if (cpm < gifCpmMin_ || isIdle()) {
        handleIdle();
        return;
    }
    
    // 2. 映射
    double raw = mapCpmToSpeed(cpm);
    
    // 3. EMA 平滑
    gifCurrentSpeed_ = emaSmooth(raw);
    emit gifSpeedChanged();
}
```

### 4.2 映射函数

**线性映射**（默认）：
```
speed = speedMin + clamp((cpm - cpmMin) / (cpmMax - cpmMin), 0, 1) × (speedMax - speedMin)
```

**分段映射**：

| CPM 区间 | 对应倍率 |
|----------|----------|
| 10~40    | 0.5x ~ 1.0x |
| 40~100   | 1.0x ~ 1.8x |
| 100~200  | 1.8x ~ 2.5x |
| 200+     | 2.5x ~ 3.0x |

分段映射使用线性插值连接各段边界，避免跳变。

### 4.3 EMA 平滑

```
smoothed_prev = gifCurrentSpeed_    // 上一次输出
raw = 映射后的原始倍率
alpha = gifSmoothFactor_ (0.15 默认)

gifCurrentSpeed_ = alpha × raw + (1 - alpha) × smoothed_prev
```

- `alpha` 越小，过渡越柔和但响应越慢。
- `alpha` 可配置范围 0.05 ~ 0.5，典型值 0.15。
- 平滑逻辑在 **主线程** 执行，250ms 一次，计算量可忽略。

### 4.4 空闲行为（以最低倍率持续播放）

空闲（cpm < cpmMin 或停手 1s）时不暂停，而是以用户配置的最低倍率 `gifSpeedMin_`
持续慢速播放，保持"活"的感觉（不敲键盘时动图不完全静止）：

```cpp
double rawSpeed;
if (nowIdle) {
    rawSpeed = gifSpeedMin_;    // 空闲 → 最低倍率慢速运转
} else {
    rawSpeed = mapCpmToSpeed(cpm);
}

// 进入空闲瞬间直接赋值（立即落到最低倍率，不平滑）；
// 从空闲恢复时由 minSpeed 经 EMA 平滑加速，视觉过渡自然
```

在 `GifPlayer` 侧收到 `speed=0.0` 时仍执行 `movie_->setPaused(true)`（该情况只出现在 GIF 全局开关关闭时）；空闲态传的是 `gifSpeedMin_`，GIF 持续慢速播放。

## 5. 设置项与 AppSettings 新增

### 5.1 新增 QSettings 键

| 键 | 类型 | 默认值 | 对应 PRD 3.10.4 |
|----|------|--------|----------------|
| `gif/enabled` | bool | false | 总开关 |
| `gif/source` | string | `qrc:/qml/dog.gif` | GIF 文件路径 |
| `gif/mapMode` | int | 0 | 0=线性 / 1=分段 |
| `gif/cpmMin` | int | 10 | CPM 下限（空闲阈值） |
| `gif/cpmMax` | int | 300 | CPM 上限（封顶值） |
| `gif/speedMin` | double | 0.5 | 倍率下限 |
| `gif/speedMax` | double | 3.0 | 倍率上限 |
| `gif/idleTimeoutSec` | int | 1 | 停手判定超时（秒）；当前由采集线程常量 `kSpeedStopMs` 固定为 1s |
| `gif/smoothFactor` | double | 0.15 | EMA α |

### 5.2 AppSettings Q_PROPERTY 新增

```cpp
// AppSettings.h 新增
Q_PROPERTY(bool gifEnabled READ gifEnabled WRITE setGifEnabled NOTIFY gifEnabledChanged)
Q_PROPERTY(QString gifSource READ gifSource WRITE setGifSource NOTIFY gifSourceChanged)
Q_PROPERTY(int gifMapMode READ gifMapMode WRITE setGifMapMode NOTIFY gifConfigChanged)
Q_PROPERTY(int gifCpmMin READ gifCpmMin WRITE setGifCpmMin NOTIFY gifConfigChanged)
Q_PROPERTY(int gifCpmMax READ gifCpmMax WRITE setGifCpmMax NOTIFY gifConfigChanged)
Q_PROPERTY(double gifSpeedMin READ gifSpeedMin WRITE setGifSpeedMin NOTIFY gifConfigChanged)
Q_PROPERTY(double gifSpeedMax READ gifSpeedMax WRITE setGifSpeedMax NOTIFY gifConfigChanged)
Q_PROPERTY(int gifIdleBehavior READ gifIdleBehavior WRITE setGifIdleBehavior NOTIFY gifConfigChanged)
Q_PROPERTY(int gifIdleTimeoutSec READ gifIdleTimeoutSec WRITE setGifIdleTimeoutSec NOTIFY gifConfigChanged)
Q_PROPERTY(double gifSmoothFactor READ gifSmoothFactor WRITE setGifSmoothFactor NOTIFY gifConfigChanged)
Q_PROPERTY(int gifDashboardMode READ gifDashboardMode WRITE setGifDashboardMode NOTIFY gifConfigChanged)
Q_PROPERTY(double gifCurrentSpeed READ gifCurrentSpeed NOTIFY gifSpeedChanged)
Q_PROPERTY(double gifCurrentCpm READ gifCurrentCpm NOTIFY gifSpeedChanged)
```

所有设置项变更即时写 QSettings（与 AppSettings 现有模式一致），同时触发 `gifConfigChanged` 通知需要重新读取映射参数的上层。

### 5.3 与全局动效开关的联动

```cpp
void AppSettings::onAnimationsEnabledChanged(bool enabled) {
    if (!enabled) {
        // 动效总开关关闭时，强制 GIF 也关闭（UI 不渲染 GifPlayer）
        gifCurrentSpeed_ = 0.0;
        emit gifSpeedChanged();
    }
    // animationsEnabled=false 的信号已连接到 UI 层，GIF 组件会被销毁
}
```

## 6. 完整数据流（GIF 动效链路）

```
1. 系统事件 → 平台钩子回调
       ↓
2. InputCaptureWorker::handleKeyDown
   ├── keyPresses_++                    // 现有，3s flush
   └── keyTimeBuf_ + lastKeyDownMs_    // 新增，速度通道
       ↓
3. InputCaptureWorker::onKeySpeedTimer (每 250ms)
   ├── 遍历 keyTimeBuf_ 计算 3s 窗口 CPM
   └── emit keySpeedUpdated(cpm)       // 跨线程队列信号
       ↓
4. AppSettings::onKeySpeedUpdated (主线程槽)
   ├── 空闲判定（cpm < cpmMin 或超时）
   ├── 映射函数（线性/分段）
   ├── EMA 平滑
   └── 更新 gifCurrentSpeed_ + gifCurrentCpm_
       ↓
5. GifPlayer.speed ← AppSettings.gifCurrentSpeed (Q_PROPERTY binding)
   ├── applySpeed() → movie_->setSpeed(percent)
   └── movie_->start() / movie_->setPaused()
       ↓
6. GifPlayer.frameChanged 信号
       ↓
7. QML Image 组件切帧展示
```

## 7. 线程安全

| 资源 | 拥有者 | 访问者 | 同步机制 |
|------|--------|--------|----------|
| `keyTimeBuf_` | 采集线程 | 采集线程（handleKeyDown + keySpeedTimer） | 无需同步 |
| `keyTimeBufWriteIdx_` | 采集线程 | 采集线程 | 无需同步 |
| `lastKeyDownMs_` | 采集线程 | 采集线程 + 250ms timer | `std::atomic<qint64>` |
| `keySpeedUpdated(cpm)` | 采集线程 | 主线程 AppSettings | Qt 队列信号（跨线程自动排队） |
| `GifPlayer.movie_` | UI 主线程 | UI 主线程 | Qt 对象亲和性保证 |
| `gifCurrentSpeed_` | AppSettings (主线程) | UI 绑定 | Q_PROPERTY NOTIFY 信号 |
| 设置项 QSettings | 任何线程的 set* 方法 | 任何线程 | QSettings 自身线程安全（文件锁） |

## 8. 性能与边界

### 8.1 CPU 开销

| 路径 | 频率 | 单次耗时估算 | 备注 |
|------|------|------------|------|
| keySpeedTimer | 250ms | < 5μs（200 次循环缓冲区遍历） | 可忽略 |
| CPM → 映射 → EMA | 250ms | < 1μs（纯数学运算） | 可忽略 |
| QMovie 解码 + QML 渲染 | 取决于 GIF 帧率 | 典型 GIF 帧率 10~30fps，对应 33~100ms/帧 | GIF 卡片与粒子分处不同区域，CPU 互不叠加 |
| 空闲态 GIF 慢速播放 | ~0.5x（speedMin） | 最低倍率下解码频率减半，CPU 增量极小 | — |

**总增量 CPU**：估计空闲时 < 0.1%，打字时（含 GIF 播放）0.5~2%，远低于 PRD 4 的 < 1% 空闲要求。

### 8.2 内存

| 组件 | 估算 |
|------|------|
| keyTimeBuf_（200 × 8B） | 1.6 KB |
| QMovie 内部帧缓存（上限 30 帧） | ≈ 30 × w × h × 4B（rgba），典型 64×64 GIF ≈ 50 KB |
| GifPlayer C++ 对象 | < 1 KB |

总增量 < 100 KB，远低于 PRD 4 的 < 50 MB 要求。

### 8.3 边界条件

| 场景 | 处理 |
|------|------|
| 用户自选 GIF > 5 MB | 拒绝加载，UI 提示"GIF 文件过大（≤5MB）" |
| GIF 文件格式损坏 | QMovie 状态变为 Invalid，`valid_=false`，QML 显示占位 |
| CPM 计算窗口内完全无按键 | count=0 → cpm=0 → 触发空闲逻辑 |
| 暂停采集（托盘） | `keySpeedTimer_` 停止推送（或推送 cpm=0），GIF 进入空闲态 |
| 恢复采集 | `keyTimeBuf_` 清空，`keySpeedTimer_` 重启，CPM 从 0 开始积累 |
| 系统休眠/恢复 | 复用已有的"重置位置基线"逻辑（02 §5），追加清空滑动窗口 |
| 跨天零点 | 速度通道不涉及持久化，不受影响 |
| 窗口隐藏 | QML Loader 控制 GifPlayer 销毁，QMovie 随之释放；恢复显示时重新加载 |
| 用户切换不同 GIF 文件 | `GifPlayer.setSource()` 先 stop 旧 movie 再 load 新 movie |

## 9. QML 集成方案

### 9.1 ImageProvider 桥接

```cpp
// src/app/GifImageProvider.h
class GifImageProvider : public QQuickImageProvider {
public:
    GifImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    // 注册 gifProvider，GifPlayer 通过 Q_PROPERTY 把当前帧图片写入 provider
    void setFrame(const QImage& frame) { frame_ = frame; }
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        Q_UNUSED(id); Q_UNUSED(requestedSize);
        if (size) *size = frame_.size();
        return frame_;
    }
private:
    QImage frame_;
};
```

- `main.cpp`：`engine->addImageProvider("gif", new GifImageProvider);`
- `GifPlayer` 在 `frameChanged` 时调 `gifProvider->setFrame(movie_->currentPixmap().toImage())`
- QML：`Image { source: "image://gif/frame" }`

### 9.2 Dashboard 集成（打字动效卡片）

GIF 展示在"活跃时长"后的第 4 张卡片（`settings.gifEnabled && settings.animationsEnabled`
时显示，隐藏时其余三卡恢复三等分）；原实时动态区保持 ParticleTrail 粒子轨迹：

```qml
// cardsRow 内，"活跃时长" StatCard 之后
Rectangle {
    id: gifCard
    visible: cardsRow.gifOn          // gifEnabled && animationsEnabled
    width: (cardsRow.width - 48) / 4
    height: 120
    color: theme.surface
    border.color: theme.border

    GifPlayer {
        id: cardGif
        source: settings.gifSource
        speed: settings.gifCurrentSpeed   // 空闲时为 speedMin（最低倍率慢速播放）
        playing: gifCard.visible
    }

    Image {
        id: cardGifImage
        anchors.centerIn: parent
        property int frameTick: 0
        source: cardGif.imageUrl + "?f=" + frameTick   // 帧序号触发重新取帧
        cache: false
        asynchronous: false
    }

    Connections {
        target: cardGif
        function onFrameChanged() { cardGifImage.frameTick += 1 }
        function onSourceChanged() { cardGifImage.frameTick += 1 }
    }
}
```

### 9.3 设置页集成

在 [SettingsPage.qml](file:///d:/githubProjects/CursorFlow/qml/pages/SettingsPage.qml) 中，"显示"组之后新增独立的"动效"分组（PRD 3.10.4），包含 8 个设置项 + 一个实时预览区域（选 GIF 后立即可见当前速度效果）。

## 10. CMakeLists.txt 变更

```cmake
# 新增 GIF 相关源文件
set(GIF_SOURCES
    src/app/GifPlayer.cpp
    src/app/GifPlayer.h
)

# Quick 已链接（GifPlayer 用到 QMovie 属于 Qt5::Gui，已包含在 Quick 里）
# 无新增 Qt 模块依赖
```

`QMovie` 所在的 `Qt5::Gui` 模块已被 `Qt5::Quick` 传递性依赖覆盖，不需要额外 `find_package` 条目。

## 11. 测试要点

### 11.1 单元测试

| 测试项 | 方法 |
|--------|------|
| CPM 计算正确性 | 注入伪造时间戳序列 → 验证 3s 窗口内 count × 20 = cpm |
| 空闲判定 | 模拟长间隔无按键 → cpm 变为 0，GIF 落到最低倍率 |
| 线性映射边界 | cpm=cpmMin → speed=speedMin；cpm=cpmMax → speed=speedMax；中间值线性插值 |
| 分段映射各段 | 4 个 CPM 区间 → 对应倍率区间边界正确 |
| EMA 平滑收敛 | 固定 raw 值 → 多次迭代后 gifCurrentSpeed_ 应逼近 raw（α=0.15 约 30 次后误差 < 1%） |
| GifPlayer.speed 范围 | setSpeed(-1) → 0.1；setSpeed(99) → 4.0；边界钳制 |
| 设置项即写即读 | 每个 gif/* 键写入后重新加载 QSettings → 值一致 |

### 11.2 集成测试

| 场景 | 验证 |
|------|------|
| 实时打字 → GIF 加速 | 手动敲击键盘（可用宏脚本），观察 GIF 速度随 CPM 变化 |
| 停止打字 → GIF 空闲 | 停手 1s 后 cpm 归零，GIF 约 1.25s 内落到最低倍率慢速播放 |
| 暂停采集 → GIF 停摆 | 托盘暂停后 GIF 立即进入空闲态 |
| 恢复采集 → GIF 恢复 | 恢复后 GIF 从 0 开始积累，不会使用暂停期间的按键 |
| 大文件拒绝加载 | 构造 >5MB 的 GIF → valid_=false，QML 不崩 |
| 损坏 GIF 容错 | 构造格式错误的文件 → QMovie 不崩溃，UI 显示占位 |
| 跨平台一致 | 在 Windows / macOS / Linux 上验证同一 GIF 调速效果一致 |
| 动效总开关联动 | 关闭 settings.animationsEnabled → GifPlayer 被 Loader 销毁 → 0 CPU |

### 11.3 性能测试

| 指标 | 目标 |
|------|------|
| 空闲 CPU（GIF 启用但暂停） | 增量 < 0.1%（总空闲仍 < 1%） |
| 打字时 CPU | 增量 < 2% |
| 内存增量 | < 200 KB |
| CPM → GifPlayer.speed 端到端延迟 | < 1 s（主要是 EMA 平滑和 QMovie 下一帧切换） |
| GIF 切换文件 | < 500 ms（源文件有效时） |
