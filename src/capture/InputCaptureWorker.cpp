#include "capture/InputCaptureWorker.h"

#include <QDateTime>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <utility>

InputCaptureWorker::InputCaptureWorker(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<InputDelta>("InputDelta");
    // M7：keySpeedUpdated 跨线程推送 double，Qt 自动支持无需注册；
    // 这里提前预注册保证跨线程连接零开销。
    qRegisterMetaType<double>("double");
}

bool InputCaptureWorker::start(IPlatformHook* hook)
{
    hook_ = hook;
    if (hook_ && !hook_->start([this](const HookEvent& event) { handleEvent(event); })) {
        qWarning() << "InputCaptureWorker: hook start failed";
        hook_ = nullptr;
        return false;
    }
    if (flushTimer_ == nullptr) {
        // 采集线程内创建，保证线程亲和性正确（M1 遗留问题修复）
        flushTimer_ = new QTimer(this);
        connect(flushTimer_, &QTimer::timeout, this, &InputCaptureWorker::onFlushTimer);
        flushTimer_->setInterval(flushIntervalMs_);
    }
    lastFlushMs_ = 0;
    flushTimer_->start();

    // M7：速度通道定时器，同样在采集线程内懒创建
    if (keySpeedTimer_ == nullptr) {
        keySpeedTimer_ = new QTimer(this);
        keySpeedTimer_->setInterval(250);
        connect(keySpeedTimer_, &QTimer::timeout,
                this, &InputCaptureWorker::onKeySpeedTimer);
    }
    keySpeedTimer_->start();

    return true;
}

void InputCaptureWorker::stop()
{
    if (flushTimer_ != nullptr) {
        flushTimer_->stop();
    }
    if (keySpeedTimer_ != nullptr) {
        keySpeedTimer_->stop();  // M7
    }
    if (hook_) {
        hook_->stop();
        hook_ = nullptr;
    }
}

void InputCaptureWorker::pause()
{
    if (paused_) {
        return;
    }
    // 暂停前冲刷积压增量，保证归属时段正确（02 号文档 §5）
    flush(QDateTime::currentMSecsSinceEpoch());
    paused_ = true;
    if (keySpeedTimer_ != nullptr) {
        keySpeedTimer_->stop();  // M7：停止速度推送
    }
}

void InputCaptureWorker::resume()
{
    if (!paused_) {
        return;
    }
    paused_ = false;
    resetPositionBaseline();
    if (keySpeedTimer_ != nullptr) {
        keySpeedTimer_->start();  // M7：重启速度推送
    }
}

void InputCaptureWorker::resetPositionBaseline()
{
    hasLastPos_ = false;
    // M7：清空按键速度滑动窗口
    keyTimeBufWriteIdx_ = 0;
    std::fill(keyTimeBuf_.begin(), keyTimeBuf_.end(), 0);
}

void InputCaptureWorker::setVirtualScreenMaxDim(double px)
{
    jumpThresholdPx_ = px * 2.0;
}

void InputCaptureWorker::setDpi(double dpi)
{
    if (dpi > 0.0) {
        dpi_ = dpi;
    }
}

void InputCaptureWorker::setIdleThresholdMs(qint64 ms)
{
    idleThresholdMs_ = ms;
}

void InputCaptureWorker::setFlushIntervalMs(int ms)
{
    flushIntervalMs_ = ms;
    if (flushTimer_ != nullptr) {
        flushTimer_->setInterval(ms);
        if (flushTimer_->isActive()) {
            flushTimer_->stop();
            flushTimer_->start();
        }
    }
}

void InputCaptureWorker::handleEvent(const HookEvent& event)
{
    if (paused_) {
        return; // 暂停：停止累加，事件全部忽略（02 号文档 §5）
    }

    // 平台钩子未提供 epoch 时间戳（如 MSLLHOOKSTRUCT.time 为启动 tick）时以当前时间归属
    const qint64 timestampMs = event.timestampMs > 0
        ? event.timestampMs
        : QDateTime::currentMSecsSinceEpoch();
    lastInputMs_.store(timestampMs, std::memory_order_relaxed);

    switch (event.kind) {
    case HookEventKind::MouseMove: {
        if (hasLastPos_) {
            const double dx = event.x - lastX_;
            const double dy = event.y - lastY_;
            const double step = std::sqrt(dx * dx + dy * dy);
            // 跳变剔除：超过 2 × maxDim 视为多屏切换/远程桌面异常（02 号文档 §4.1）
            if (step <= jumpThresholdPx_) {
                // C++17 无 atomic<double>::fetch_add（C++20），用 CAS 循环累加
                double current = distancePx_.load(std::memory_order_relaxed);
                while (!distancePx_.compare_exchange_weak(current, current + step,
                                                          std::memory_order_relaxed)) {
                }
            }
        }
        lastX_ = event.x;
        lastY_ = event.y;
        hasLastPos_ = true;
        break;
    }
    case HookEventKind::MouseLeftClick:
        leftClicks_.fetch_add(1, std::memory_order_relaxed);
        break;
    case HookEventKind::MouseRightClick:
        rightClicks_.fetch_add(1, std::memory_order_relaxed);
        break;
    case HookEventKind::MouseMidClick:
        midClicks_.fetch_add(1, std::memory_order_relaxed);
        break;
    case HookEventKind::WheelScroll:
        // 滚动按绝对值累计，方向不落库（02 号文档 §4.4）
        scrollSteps_.fetch_add(event.scrollSteps < 0 ? -event.scrollSteps : event.scrollSteps,
                               std::memory_order_relaxed);
        break;
    case HookEventKind::KeyDown:
        handleKeyDown(event);
        break;
    }
}

void InputCaptureWorker::handleKeyDown(const HookEvent& event)
{
    if (event.isAutoRepeat) {
        return; // 长按自动重复仅计首次（02 号文档 §4.2）
    }
    if (event.isModifier) {
        // 修饰键待定：若期间出现非修饰键（组合键）则不计（02 号文档 §4.2）
        pendingModifiers_ += 1;
        return;
    }
    pendingModifiers_ = 0;

    // M7：记录按键时间戳到滑动窗口（独立于 keyPresses_ 原子累加）
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    lastInputMs_.store(now, std::memory_order_relaxed);
    lastKeyDownMs_.store(now, std::memory_order_relaxed);
    keyTimeBuf_[keyTimeBufWriteIdx_] = now;
    keyTimeBufWriteIdx_ = (keyTimeBufWriteIdx_ + 1) % kSpeedWindowMaxKeys;

    keyPresses_.fetch_add(1, std::memory_order_relaxed);
}

void InputCaptureWorker::onFlushTimer()
{
    flush(QDateTime::currentMSecsSinceEpoch());
}

InputDelta InputCaptureWorker::flush(qint64 nowMs)
{
    InputDelta delta;
    delta.distanceMeters = distancePx_.exchange(0.0, std::memory_order_relaxed) * 25.4 / dpi_ / 1000.0;
    delta.keyPresses =
        keyPresses_.exchange(0, std::memory_order_relaxed) + std::exchange(pendingModifiers_, 0);
    delta.leftClicks = leftClicks_.exchange(0, std::memory_order_relaxed);
    delta.rightClicks = rightClicks_.exchange(0, std::memory_order_relaxed);
    delta.midClicks = midClicks_.exchange(0, std::memory_order_relaxed);
    delta.scrollStepsAbs = scrollSteps_.exchange(0, std::memory_order_relaxed);

    if (lastFlushMs_ == 0) {
        lastFlushMs_ = nowMs; // 首次 flush 仅建立时间基线
    }

    // 活跃推断（02 号文档 §4.3）：距最近输入 ≤ 阈值则本区间记为活跃
    const qint64 lastInput = lastInputMs_.load(std::memory_order_relaxed);
    if (lastInput > 0 && nowMs - lastInput <= idleThresholdMs_) {
        delta.activeSeconds = static_cast<int>(qMax<qint64>(0, nowMs - lastFlushMs_) / 1000);
    }
    lastFlushMs_ = nowMs;

    emit deltaReady(delta);
    return delta;
}

void InputCaptureWorker::onKeySpeedTimer()
{
    // M7：每 250ms 计算一次最近 3 秒内的 CPM（02 号文档 §7.4）
    // 窗口取 3s 而非 10s：10s 窗口下首键只有 6 CPM（低于 cpmMin 判定为空闲，
    // 需要 2 键以上才脱离空闲），且停手后要 10s 才衰减到 0，动效起停明显滞后。
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 windowStart = now - kSpeedWindowMs;

    // 遍历滑动窗口统计落在窗口内的按键数
    int count = 0;
    const int total = qMin(keyTimeBufWriteIdx_, kSpeedWindowMaxKeys);
    for (int i = 0; i < total; ++i) {
        if (keyTimeBuf_[i] >= windowStart) {
            ++count;
        }
    }

    double cpm = count * kSpeedCpmPerKey;  // count / 3s × 60s = count × 20

    // 停手判定：只按 1 秒验键间隔，不依赖窗口滑空（否则要等 3s 窗口逐步衰减，
    // 表现为"停手后动图还在跑"）。超时直接归零，GIF 在约 1.25s 内暂停。
    const qint64 lastKey = lastKeyDownMs_.load(std::memory_order_relaxed);
    if (lastKey > 0 && now - lastKey > kSpeedStopMs) {
        cpm = 0.0;
    }

    emit keySpeedUpdated(cpm);
}
