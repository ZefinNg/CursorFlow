#include "capture/InputCaptureWorker.h"

#include <QDateTime>
#include <QDebug>

#include <cmath>
#include <utility>

InputCaptureWorker::InputCaptureWorker(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<InputDelta>("InputDelta");
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
    return true;
}

void InputCaptureWorker::stop()
{
    if (flushTimer_ != nullptr) {
        flushTimer_->stop();
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
}

void InputCaptureWorker::resume()
{
    if (!paused_) {
        return;
    }
    paused_ = false;
    resetPositionBaseline();
}

void InputCaptureWorker::resetPositionBaseline()
{
    hasLastPos_ = false;
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
