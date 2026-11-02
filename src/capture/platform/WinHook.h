#pragma once

#include "capture/IPlatformHook.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#ifdef Q_OS_WIN

// Windows 低级钩子实现（02 号文档 §3.1）：
// WH_MOUSE_LL / WH_KEYBOARD_LL 要求注册线程持续 pump 消息，
// 因此用专用 std::thread 运行 GetMessage 循环，与 Qt 事件循环解耦；
// 钩子回调内仅做轻量转发，保证快速返回不超时。
class WinHook : public IPlatformHook
{
public:
    WinHook() = default;
    ~WinHook() override;

    WinHook(const WinHook&) = delete;
    WinHook& operator=(const WinHook&) = delete;

    bool start(std::function<void(const HookEvent&)> callback) override;
    void stop() override;

    // 仅供本文件内的低级钩子过程调用（file-static 回调访问不到私有成员）
    void emitEvent(const HookEvent& event);

private:
    void runThread();

    std::function<void(const HookEvent&)> callback_;
    std::thread thread_;
    std::atomic<unsigned long> threadId_{0}; // DWORD
    std::atomic<bool> installed_{false};
    std::mutex mutex_;
    std::condition_variable installedCv_;
};

#endif // Q_OS_WIN
