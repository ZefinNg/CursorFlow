#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

#include <functional>

// 平台钩子上报的原始输入事件（02 号文档 §3）。
// vkCode 仅用于修饰键/自动重复判断，不进入任何存储，保证隐私（02 号文档 §6）。
enum class HookEventKind {
    MouseMove,
    MouseLeftClick,
    MouseRightClick,
    MouseMidClick,
    WheelScroll,
    KeyDown,
};

struct HookEvent {
    HookEventKind kind = HookEventKind::MouseMove;
    double x = 0.0;             // 虚拟桌面全局坐标
    double y = 0.0;
    int scrollSteps = 0;        // WheelScroll：带方向步数
    quint32 vkCode = 0;         // KeyDown：平台键码
    bool isModifier = false;    // 平台判定：修饰键
    bool isAutoRepeat = false;  // 平台判定：OS 自动重复
    qint64 timestampMs = 0;     // 事件时间戳（测试可注入）
};

// 平台钩子抽象（01 号文档 §5）。
// 回调在钩子线程同步调用，必须快速返回（02 号文档 §3.1）。
class IPlatformHook
{
public:
    virtual ~IPlatformHook() = default;

    virtual bool start(std::function<void(const HookEvent&)> callback) = 0;
    virtual void stop() = 0;

    // macOS 专用：辅助功能权限是否已授予（01 号文档 §5）。
    virtual bool isAccessibilityGranted() const { return true; }

    // start 失败时的用户可读原因（04 号文档 §6 采集失败横幅），成功/未知返回空串。
    virtual QString lastError() const { return QString(); }
};
