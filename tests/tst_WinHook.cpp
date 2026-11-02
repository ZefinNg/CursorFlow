#include <QtTest>

#ifdef Q_OS_WIN

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <QDateTime>

#include "capture/InputCaptureWorker.h"
#include "capture/platform/WinHook.h"

namespace {

// 绝对坐标移动：不受系统指针加速影响，保证注入像素与钩子所见一致
void sendAbsoluteMove(int px, int py)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.mi.dx = static_cast<LONG>(px * 65535 / (GetSystemMetrics(SM_CXSCREEN) - 1));
    input.mi.dy = static_cast<LONG>(py * 65535 / (GetSystemMetrics(SM_CYSCREEN) - 1));
    SendInput(1, &input, sizeof(INPUT));
}

void sendKey(WORD vk, bool up)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    SendInput(1, &input, sizeof(INPUT));
}

InputDelta accumulateFlush(InputCaptureWorker& worker, double targetMeters, int rounds)
{
    InputDelta total;
    for (int i = 0; i < rounds; ++i) {
        QTest::qWait(50);
        const InputDelta delta = worker.flush(QDateTime::currentMSecsSinceEpoch());
        total.distanceMeters += delta.distanceMeters;
        total.keyPresses += delta.keyPresses;
        if (total.distanceMeters >= targetMeters) {
            break; // 提前达标即退出，缩短测试时长
        }
    }
    return total;
}

} // namespace

class TestWinHook : public QObject
{
    Q_OBJECT

private slots:
    // SendInput 注入 2000px，钩子所见距离误差应 <5%（06 号文档任务 2.2）
    void mouseDistanceMatchesSendInput();
    // 键盘注入端到端：普通键计数 + 自动重复去重
    void keyboardCountingEndToEnd();
};

void TestWinHook::mouseDistanceMatchesSendInput()
{
    InputCaptureWorker worker;
    worker.setVirtualScreenMaxDim(10); // 阈值 20px：10px 步长不被跳变剔除
    worker.setFlushIntervalMs(60000);  // 关闭周期冲刷干扰，仅手动 flush

    WinHook hook;
    QVERIFY(hook.start([&worker](const HookEvent& event) { worker.handleEvent(event); }));

    // 定位到主屏中心并以 ±5px 两个锚点建立基准
    POINT center{};
    GetCursorPos(&center);
    const int cx = static_cast<int>(center.x);
    const int cy = static_cast<int>(center.y);
    SetCursorPos(cx + 5, cy);
    QTest::qWait(100);
    worker.flush(QDateTime::currentMSecsSinceEpoch()); // 建立坐标基线

    // 200 次交替移动 × 10px = 2000px
    constexpr int kMoves = 200;
    for (int i = 0; i < kMoves; ++i) {
        sendAbsoluteMove(i % 2 == 0 ? cx - 5 : cx + 5, cy);
        if (i % 20 == 19) {
            QTest::qWait(10); // 给系统处理队列留出时间
        }
    }

    const double expected = kMoves * 10.0 * 25.4 / 96.0 / 1000.0; // ≈ 0.5292 m
    const InputDelta total = accumulateFlush(worker, expected * 0.95, 60);

    QVERIFY2(total.distanceMeters > 0.0, "no mouse move events captured");
    // 真实钩子计数允许 ±5% 漂移（测试期间系统注入/微动等干扰）
    QVERIFY2(qAbs(total.distanceMeters / expected - 1.0) < 0.05,
             qPrintable(QStringLiteral("ratio=%1").arg(total.distanceMeters / expected)));
    worker.stop();
}

void TestWinHook::keyboardCountingEndToEnd()
{
    InputCaptureWorker worker;
    worker.setFlushIntervalMs(60000);

    WinHook hook;
    QVERIFY(hook.start([&worker](const HookEvent& event) { worker.handleEvent(event); }));

    QTest::qWait(100);

    // 10 次 'A' down/up 交替 → 计 10 次
    for (int i = 0; i < 10; ++i) {
        sendKey(0x41, false);
        QTest::qWait(5);
        sendKey(0x41, true);
        QTest::qWait(5);
    }

    int pressed = 0;
    for (int i = 0; i < 20 && pressed < 10; ++i) {
        QTest::qWait(50);
        pressed += worker.flush(QDateTime::currentMSecsSinceEpoch()).keyPresses;
    }
    QCOMPARE(pressed, 10);

    // 长按：连续 3 次 down 无 up → 仅首次计数（自动重复去重，02 号文档 §4.2）
    sendKey(0x42, false);
    QTest::qWait(10);
    sendKey(0x42, false);
    QTest::qWait(10);
    sendKey(0x42, false);
    QTest::qWait(100);

    int repeatPressed = 0;
    for (int i = 0; i < 10; ++i) {
        QTest::qWait(50);
        repeatPressed += worker.flush(QDateTime::currentMSecsSinceEpoch()).keyPresses;
        if (repeatPressed >= 1) {
            break;
        }
    }
    QCOMPARE(repeatPressed, 1);

    sendKey(0x42, true); // 抬起，恢复干净状态
    QTest::qWait(50);
    worker.flush(QDateTime::currentMSecsSinceEpoch());
    worker.stop();
}

QTEST_GUILESS_MAIN(TestWinHook)
#include "tst_WinHook.moc"

#endif // Q_OS_WIN
