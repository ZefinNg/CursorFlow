#pragma once

#include "capture/IPlatformHook.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <set>
#include <thread>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

// Linux X11 事件采集（02 号文档 §3.2，任务 4.6）：
// XInput2 raw 事件（XI_RawMotion / RawButtonPress / RawKeyDown）+ 专用事件线程，
// 与 Qt 事件循环解耦；只读监听，不拦截不改写事件。
// Wayland 会话不受支持：start 返回 false 并给出降级提示（04 号文档 §6），
// UI 横幅提示用户改用 X11 会话。
class X11InputHook : public IPlatformHook
{
public:
    X11InputHook() = default;
    ~X11InputHook() override;

    X11InputHook(const X11InputHook&) = delete;
    X11InputHook& operator=(const X11InputHook&) = delete;

    bool start(std::function<void(const HookEvent&)> callback) override;
    void stop() override;
    QString lastError() const override;

    // 仅供本文件内的 X11 事件线程调用
    void emitEvent(const HookEvent& event);

private:
    void runThread();

    std::function<void(const HookEvent&)> callback_;
    std::thread thread_;
    std::atomic<bool> installed_{false};
    std::atomic<bool> stopRequested_{false}; // 事件线程 poll 超时后检查

    // 仅事件线程访问
    struct _XDisplay* display_ = nullptr;
    std::set<KeyCode> pressedKeys_;  // 自动重复检测：down 未 up 即重复
    std::set<KeyCode> modifierKeys_; // 启动时按 keysym 预计算的修饰键码
    int xiOpcode_ = 0;

    std::mutex mutex_;
    QString lastError_;
};

#endif // Q_OS_UNIX && !Q_OS_MACOS
