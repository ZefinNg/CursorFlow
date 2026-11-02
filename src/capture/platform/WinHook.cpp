#include "capture/platform/WinHook.h"

#ifdef Q_OS_WIN

#include <QDebug>

#include <unordered_set>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

// 低级钩子过程要求独立于实例的全局入口（02 号文档 §3.1）
WinHook* g_instance = nullptr;
HHOOK g_mouseHook = nullptr;
HHOOK g_keyHook = nullptr;

// 已按下键集合：WM_KEYDOWN 重复出现判定为自动重复（02 号文档 §4.2）
std::unordered_set<quint32> g_downKeys;

bool isModifierVk(quint32 vk)
{
    switch (vk) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
        return true;
    default:
        return false;
    }
}

HookEvent makeMouseEvent(HookEventKind kind, const MSLLHOOKSTRUCT& info)
{
    HookEvent event;
    event.kind = kind;
    event.x = info.pt.x; // 虚拟桌面全局坐标
    event.y = info.pt.y;
    // MSLLHOOKSTRUCT.time 是系统启动 tick 而非 epoch，置 0 由 worker 以当前时间归属
    event.timestampMs = 0;
    return event;
}

LRESULT CALLBACK lowLevelMouseProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_instance) {
        const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        switch (wParam) {
        case WM_MOUSEMOVE:
            g_instance->emitEvent(makeMouseEvent(HookEventKind::MouseMove, *info));
            break;
        case WM_LBUTTONDOWN:
            g_instance->emitEvent(makeMouseEvent(HookEventKind::MouseLeftClick, *info));
            break;
        case WM_RBUTTONDOWN:
            g_instance->emitEvent(makeMouseEvent(HookEventKind::MouseRightClick, *info));
            break;
        case WM_MBUTTONDOWN:
            g_instance->emitEvent(makeMouseEvent(HookEventKind::MouseMidClick, *info));
            break;
        case WM_MOUSEWHEEL: {
            HookEvent event = makeMouseEvent(HookEventKind::WheelScroll, *info);
            event.scrollSteps = static_cast<int>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            g_instance->emitEvent(event);
            break;
        }
        default:
            break;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK lowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_instance) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (info->flags & LLKHF_UP) {
            g_downKeys.erase(info->vkCode); // 抬起后清除，下次按下重新计数
        } else if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            HookEvent event;
            event.kind = HookEventKind::KeyDown;
            event.vkCode = info->vkCode;
            event.isModifier = isModifierVk(info->vkCode);
            // 新插入集合说明是首次按下；集合中已存在即自动重复
            event.isAutoRepeat = !g_downKeys.insert(info->vkCode).second;
            event.timestampMs = 0;
            g_instance->emitEvent(event);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

} // namespace

WinHook::~WinHook()
{
    stop();
}

void WinHook::emitEvent(const HookEvent& event)
{
    if (callback_) {
        callback_(event);
    }
}

void WinHook::runThread()
{
    threadId_ = GetCurrentThreadId();

    // PostThreadMessage 要求目标线程已有消息队列，先隐式创建
    MSG warmup;
    PeekMessage(&warmup, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    g_instance = this;
    g_downKeys.clear();
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, lowLevelMouseProc, nullptr, 0);
    g_keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, lowLevelKeyboardProc, nullptr, 0);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        installed_ = g_mouseHook && g_keyHook;
    }
    installedCv_.notify_all();

    if (installed_) {
        // 持续 pump 消息（02 号文档 §3.1）；WM_QUIT 由 stop() 投递
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } else {
        qWarning() << "WinHook: SetWindowsHookEx failed, error" << GetLastError();
    }

    if (g_keyHook) {
        UnhookWindowsHookEx(g_keyHook);
        g_keyHook = nullptr;
    }
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
    g_instance = nullptr;
}

bool WinHook::start(std::function<void(const HookEvent&)> callback)
{
    if (thread_.joinable()) {
        return false; // 已在运行
    }
    callback_ = std::move(callback);

    thread_ = std::thread(&WinHook::runThread, this);

    std::unique_lock<std::mutex> lock(mutex_);
    const bool ok = installedCv_.wait_for(lock, std::chrono::seconds(3),
                                          [this] { return installed_.load(); });

    if (!ok) {
        qWarning() << "WinHook: install timeout";
        stop();
        return false;
    }
    return true;
}

void WinHook::stop()
{
    if (threadId_ != 0) {
        PostThreadMessage(threadId_, WM_QUIT, 0, 0);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    threadId_ = 0;
    installed_ = false;
}

#endif // Q_OS_WIN
