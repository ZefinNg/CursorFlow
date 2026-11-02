#include "capture/platform/X11InputHook.h"

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>

#include <poll.h>

#include <chrono>
#include <cstring>

namespace {

// 启动时预计算的修饰键 keysym（02 号文档 §4.2：单独修饰键不计，组合键才计）
const KeySym kModifierKeysyms[] = {
    XK_Shift_L, XK_Shift_R,     XK_Control_L, XK_Control_R,
    XK_Alt_L,   XK_Alt_R,       XK_Meta_L,    XK_Meta_R,
    XK_Super_L, XK_Super_R,     XK_Hyper_L,   XK_Hyper_R,
    XK_ISO_Level3_Shift, XK_ISO_Level5_Shift, XK_Mode_switch,
};

} // namespace

X11InputHook::~X11InputHook()
{
    stop();
}

bool X11InputHook::start(std::function<void(const HookEvent&)> callback)
{
    if (installed_.load()) {
        return true;
    }
    callback_ = std::move(callback);

    // Wayland 会话检测（06 号文档 §6 风险：Wayland 无全局采集方案，降级提示）
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY")
        || qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland")) {
        const std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = QStringLiteral(
            "Wayland 会话暂不支持输入采集，请退出后选择 X11 会话登录以启用统计");
        return false;
    }

    stopRequested_.store(false);
    display_ = XOpenDisplay(nullptr);
    if (display_ == nullptr) {
        const std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = QStringLiteral("无法连接 X 服务器，输入采集未启动");
        return false;
    }

    // XInput2 扩展与版本检查
    int eventBase = 0, errorBase = 0;
    if (!XQueryExtension(display_, "XInputExtension", &xiOpcode_, &eventBase, &errorBase)) {
        const std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = QStringLiteral("X 服务器不支持 XInput2 扩展，输入采集未启动");
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }
    int major = 2, minor = 0;
    if (XIQueryVersion(display_, &major, &minor) != Success) {
        const std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = QStringLiteral("XInput2 版本过低（需要 2.0+），输入采集未启动");
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    // 修饰键码预计算
    modifierKeys_.clear();
    for (const KeySym keysym : kModifierKeysyms) {
        const KeyCode keycode = XKeysymToKeycode(display_, keysym);
        if (keycode != 0) {
            modifierKeys_.insert(keycode);
        }
    }
    pressedKeys_.clear();

    // 根窗口订阅 raw 事件（XIAllMasterDevices：对所有设备生效）
    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = XIMaskLen(XI_LastEvent);
    mask.mask = static_cast<unsigned char*>(calloc(1, static_cast<size_t>(mask.mask_len)));
    XISetMask(mask.mask, XI_RawMotion);
    XISetMask(mask.mask, XI_RawButtonPress);
    XISetMask(mask.mask, XI_RawButtonRelease);
    XISetMask(mask.mask, XI_RawKeyDown);
    XISetMask(mask.mask, XI_RawKeyUp);
    const Status status =
        XISelectEvents(display_, DefaultRootWindow(display_), &mask, 1);
    free(mask.mask);
    if (status != Success) {
        const std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = QStringLiteral("XInput2 事件订阅失败，输入采集未启动");
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    installed_.store(true);
    thread_ = std::thread(&X11InputHook::runThread, this);
    return true;
}

void X11InputHook::runThread()
{
    const int fd = ConnectionNumber(display_);

    while (!stopRequested_.load()) {
        XFlush(display_); // 刷出订阅请求

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        const int ready = poll(&pfd, 1, 100); // 100ms 超时 → 检查停止标志
        if (ready <= 0) {
            continue;
        }

        while (XPending(display_) > 0 && !stopRequested_.load()) {
            XEvent event;
            XNextEvent(display_, &event);
            if (event.type != GenericEvent || event.xcookie.extension != xiOpcode_) {
                continue;
            }
            if (!XGetEventData(display_, &event.xcookie)) {
                continue;
            }

            const XIRawEvent* raw = static_cast<const XIRawEvent*>(event.xcookie.data);
            HookEvent out;
            switch (event.xcookie.evtype) {
            case XI_RawMotion: {
                // raw motion 不含坐标，主动查询指针位置（虚拟桌面全局坐标，含多屏）
                Window rootReturn = None, childReturn = None;
                int rootX = 0, rootY = 0, winX = 0, winY = 0;
                unsigned int buttons = 0;
                if (XQueryPointer(display_, DefaultRootWindow(display_), &rootReturn,
                                  &childReturn, &rootX, &rootY, &winX, &winY, &buttons)) {
                    out.kind = HookEventKind::MouseMove;
                    out.x = rootX;
                    out.y = rootY;
                    emitEvent(out);
                }
                break;
            }
            case XI_RawButtonPress: {
                const int button = static_cast<int>(raw->detail);
                if (button == 1) {
                    out.kind = HookEventKind::MouseLeftClick;
                } else if (button == 2) {
                    out.kind = HookEventKind::MouseMidClick;
                } else if (button == 3) {
                    out.kind = HookEventKind::MouseRightClick;
                } else if (button == 4 || button == 5) {
                    // 传统滚轮映射为 4/5 号按键（raw 事件下无平滑滚轮位移值）
                    out.kind = HookEventKind::WheelScroll;
                    out.scrollSteps = button == 4 ? 1 : -1;
                } else {
                    break; // 其余侧键不统计
                }
                emitEvent(out);
                break;
            }
            case XI_RawKeyDown: {
                const KeyCode keycode = static_cast<KeyCode>(raw->detail);
                out.kind = HookEventKind::KeyDown;
                out.vkCode = keycode;
                out.isAutoRepeat = pressedKeys_.count(keycode) > 0; // down 未 up 即重复
                out.isModifier = modifierKeys_.count(keycode) > 0;
                pressedKeys_.insert(keycode);
                emitEvent(out);
                break;
            }
            case XI_RawKeyUp:
                pressedKeys_.erase(static_cast<KeyCode>(raw->detail));
                break;
            default:
                break;
            }
            XFreeEventData(display_, &event.xcookie);
        }
    }
}

void X11InputHook::stop()
{
    if (!installed_.load()) {
        return;
    }
    stopRequested_.store(true); // 事件线程 100ms 内轮询到标志后退出
    if (thread_.joinable()) {
        thread_.join();
    }
    if (display_ != nullptr) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
    installed_.store(false);
}

void X11InputHook::emitEvent(const HookEvent& event)
{
    // X 服务器时间为启动毫秒而非 epoch，置 0 由 worker 统一打当前时间戳（02 号文档 §3）
    HookEvent out = event;
    out.timestampMs = 0;
    if (callback_) {
        callback_(out);
    }
}

QString X11InputHook::lastError() const
{
    const std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

#endif // Q_OS_UNIX && !Q_OS_MACOS
