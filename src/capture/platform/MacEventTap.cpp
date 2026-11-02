#include "capture/platform/MacEventTap.h"

#ifdef Q_OS_MACOS

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#include <QMutex>

#include <chrono>

namespace {

// 订阅事件掩码：移动/拖拽、三类按键、滚轮、键按下。
// 不订阅 kCGEventFlagsChanged：单独修饰键不产生 KeyDown，
// 与 02 号文档 §4.2 "修饰键待定、组合键才计数"规则一致。
constexpr CGEventMask kSubscribedMask =
    CGEventMaskBit(kCGEventMouseMoved) | CGEventMaskBit(kCGEventLeftMouseDragged)
    | CGEventMaskBit(kCGEventRightMouseDragged) | CGEventMaskBit(kCGEventOtherMouseDragged)
    | CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventRightMouseDown)
    | CGEventMaskBit(kCGEventOtherMouseDown) | CGEventMaskBit(kCGEventScrollWheel)
    | CGEventMaskBit(kCGEventKeyDown);

// CGEventTap 回调（listen-only）：仅做类型映射与轻量转发，禁止阻塞（02 号文档 §3.2）
CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event,
                            void* userInfo)
{
    Q_UNUSED(proxy);
    auto* tap = static_cast<MacEventTap*>(userInfo);
    if (tap == nullptr || event == nullptr) {
        return event;
    }

    // 系统判定 tap 响应超时会临时禁用并投递该回执；listen-only 不会触发，防御性忽略
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        return event;
    }

    HookEvent out;
    const CGPoint pos = CGEventGetLocation(event);
    out.x = pos.x;
    out.y = pos.y;
    // CGEventGetTimestamp 为纳秒，换算毫秒
    out.timestampMs = static_cast<qint64>(CGEventGetTimestamp(event) / 1000000ULL);

    switch (type) {
    case kCGEventMouseMoved:
    case kCGEventLeftMouseDragged:
    case kCGEventRightMouseDragged:
    case kCGEventOtherMouseDragged:
        out.kind = HookEventKind::MouseMove;
        break;
    case kCGEventLeftMouseDown:
        out.kind = HookEventKind::MouseLeftClick;
        break;
    case kCGEventRightMouseDown:
        out.kind = HookEventKind::MouseRightClick;
        break;
    case kCGEventOtherMouseDown:
        out.kind = HookEventKind::MouseMidClick;
        break;
    case kCGEventScrollWheel:
        out.kind = HookEventKind::WheelScroll;
        // 带方向步数，绝对值累加在 worker 内完成（02 号文档 §4.4）
        out.scrollSteps = static_cast<int>(
            CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1));
        break;
    case kCGEventKeyDown: {
        out.kind = HookEventKind::KeyDown;
        out.vkCode = static_cast<quint32>(
            CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
        // OS 自动重复事件由字段直接判定（02 号文档 §4.2 长按仅计首次）
        out.isAutoRepeat =
            CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;
        out.isModifier = false; // 单独修饰键走 FlagsChanged，未订阅
        break;
    }
    default:
        return event; // 其余类型不转发
    }

    tap->emitEvent(out);
    return event;
}

} // namespace

MacEventTap::~MacEventTap()
{
    stop();
}

bool MacEventTap::isAccessibilityGranted() const
{
    // 非弹窗模式查询：不主动拉起系统授权框（授权引导由 UI 横幅承担，04 号文档 §6）
    const void* keys[] = { kAXTrustedCheckOptionPrompt };
    const void* values[] = { kCFBooleanFalse };
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 1, &kCFCopyStringDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    const bool trusted = AXIsProcessTrustedWithOptions(options);
    CFRelease(options);
    return trusted;
}

bool MacEventTap::start(std::function<void(const HookEvent&)> callback)
{
    if (installed_.load()) {
        return true;
    }
    callback_ = std::move(callback);

    // 权限前置检查（06 号文档 §6 风险：授权失败致采集为空）
    if (!isAccessibilityGranted()) {
        const QMutexLocker lock(&mutex_);
        lastError_ = QStringLiteral(
            "需要辅助功能权限：打开 系统设置 → 隐私与安全性 → 辅助功能，"
            "勾选 CursorFlow 后重新启动应用");
        return false;
    }

    stopping_.store(false);
    thread_ = std::thread(&MacEventTap::runThread, this);

    std::unique_lock<std::mutex> lock(mutex_);
    if (!installedCv_.wait_for(lock, std::chrono::seconds(3),
                               [this] { return installed_.load(); })) {
        lastError_ = QStringLiteral("CGEventTap 创建失败，请重新启动应用");
        stopping_.store(true);
        lock.unlock();
        if (thread_.joinable()) {
            thread_.join();
        }
        return false;
    }
    return true;
}

void MacEventTap::runThread()
{
    CFRunLoopRef runLoop = CFRunLoopGetCurrent();

    CGEventTapRef tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                                         kCGEventTapOptionListenOnly, kSubscribedMask,
                                         eventTapCallback, this);
    CFRunLoopSourceRef tapSource = nullptr;
    if (tap != nullptr) {
        tapSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
        if (tapSource != nullptr) {
            CFRunLoopAddSource(runLoop, tapSource, kCFRunLoopDefaultMode);
            CGEventTapEnable(tap, true);
            {
                const QMutexLocker lock(&mutex_);
                lastError_.clear();
            }
            installed_.store(true);
        } else {
            CFRelease(tap);
        }
    }
    installedCv_.notify_all();

    if (!installed_.load()) {
        return;
    }

    // 分片运行便于及时响应 stop（0.25s 粒度，事件回调不受影响）
    while (!stopping_.load()) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.25, false);
    }

    CFRunLoopRemoveSource(runLoop, tapSource, kCFRunLoopDefaultMode);
    CFRelease(tapSource);
    CFRelease(tap);
}

void MacEventTap::stop()
{
    if (!installed_.load()) {
        return;
    }
    stopping_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
    installed_.store(false);
}

void MacEventTap::emitEvent(const HookEvent& event)
{
    if (callback_) {
        callback_(event);
    }
}

QString MacEventTap::lastError() const
{
    const QMutexLocker lock(&mutex_);
    return lastError_;
}

#endif // Q_OS_MACOS
