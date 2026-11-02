#pragma once

#include "capture/IPlatformHook.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#ifdef Q_OS_MACOS

// macOS 事件采集（02 号文档 §3.2，任务 4.5）：
// CGEventTap（listen-only，只监听不拦截不改写事件）+ 专用 CFRunLoop 线程，
// 与 Qt 事件循环解耦；事件回调内仅做轻量转发。
// 首次使用需"辅助功能"权限：未授予时 start 返回 false，lastError 给出
// 图文引导文案（04 号文档 §6 采集失败横幅 → 系统设置路径）。
class MacEventTap : public IPlatformHook
{
public:
    MacEventTap() = default;
    ~MacEventTap() override;

    MacEventTap(const MacEventTap&) = delete;
    MacEventTap& operator=(const MacEventTap&) = delete;

    bool start(std::function<void(const HookEvent&)> callback) override;
    void stop() override;
    bool isAccessibilityGranted() const override;
    QString lastError() const override;

    // 仅供本文件内的 CGEventTap 回调调用（file-static 回调访问不到私有成员）
    void emitEvent(const HookEvent& event);

private:
    void runThread();

    std::function<void(const HookEvent&)> callback_;
    std::thread thread_;
    std::atomic<bool> installed_{false};
    std::atomic<bool> stopping_{false};
    std::mutex mutex_;
    std::condition_variable installedCv_;
    QString lastError_; // start 前后由主线程/钩子线程互斥读写
};

#endif // Q_OS_MACOS
