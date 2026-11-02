#include <QtTest>

#include <QSignalSpy>

#include "capture/InputCaptureWorker.h"

namespace {

HookEvent makeMove(double x, double y, qint64 timestampMs)
{
    HookEvent event;
    event.kind = HookEventKind::MouseMove;
    event.x = x;
    event.y = y;
    event.timestampMs = timestampMs;
    return event;
}

HookEvent makeKey(quint32 vkCode, bool isModifier, bool isAutoRepeat, qint64 timestampMs)
{
    HookEvent event;
    event.kind = HookEventKind::KeyDown;
    event.vkCode = vkCode;
    event.isModifier = isModifier;
    event.isAutoRepeat = isAutoRepeat;
    event.timestampMs = timestampMs;
    return event;
}

HookEvent makeClick(HookEventKind kind, qint64 timestampMs)
{
    HookEvent event;
    event.kind = kind;
    event.timestampMs = timestampMs;
    return event;
}

HookEvent makeWheel(int steps, qint64 timestampMs)
{
    HookEvent event;
    event.kind = HookEventKind::WheelScroll;
    event.scrollSteps = steps;
    event.timestampMs = timestampMs;
    return event;
}

} // namespace

class TestInputCaptureWorker : public QObject
{
    Q_OBJECT

private slots:
    // 02 号文档 §4 算法要点
    void mouseDistanceAccumulation(); // 欧氏距离累加 + DPI 换算
    void dpiConversion();             // 校准 DPI 联动
    void jumpFiltering();             // 跳变剔除（含阈值边界）
    void keyboardCounting();          // 普通键/自动重复/修饰键规则
    void clickAndScrollCounting();    // 点击分类 + 滚动绝对值
    void activeTimeInference();       // 活跃推断
    void pauseResume();               // 暂停冲刷 + 基线重置
    void flushClearsState();          // flush 后缓冲清零

private:
    static constexpr double kPxToMeter96Dpi = 25.4 / 96.0 / 1000.0;
};

void TestInputCaptureWorker::mouseDistanceAccumulation()
{
    InputCaptureWorker worker;
    worker.setVirtualScreenMaxDim(1920);

    worker.handleEvent(makeMove(0, 0, 1000));
    worker.handleEvent(makeMove(96, 0, 1100));  // 96 px
    worker.handleEvent(makeMove(96, 96, 1200)); // 96 px

    const InputDelta delta = worker.flush(1300);
    QCOMPARE(delta.distanceMeters, 192 * kPxToMeter96Dpi);
    QCOMPARE(delta.activeSeconds, 0); // 首次 flush 仅建立时间基线
}

void TestInputCaptureWorker::dpiConversion()
{
    InputCaptureWorker worker;
    worker.setDpi(192.0); // 校准后的 PPI（02 号文档 §4.1）

    worker.handleEvent(makeMove(0, 0, 1000));
    worker.handleEvent(makeMove(192, 0, 1100)); // 192 px = 1 inch

    const InputDelta delta = worker.flush(1200);
    QCOMPARE(delta.distanceMeters, 0.0254);
}

void TestInputCaptureWorker::jumpFiltering()
{
    InputCaptureWorker worker;
    worker.setDpi(96.0);
    worker.setVirtualScreenMaxDim(1000); // 阈值 2000

    worker.handleEvent(makeMove(0, 0, 1000));
    worker.handleEvent(makeMove(96, 0, 1100));   // 96 px
    worker.handleEvent(makeMove(5000, 0, 1200)); // 4904 px > 2000 → 丢弃
    worker.handleEvent(makeMove(5096, 0, 1300)); // 96 px

    const InputDelta delta = worker.flush(1400);
    QCOMPARE(delta.distanceMeters, 192 * kPxToMeter96Dpi);

    // 恰好等于阈值（2 × maxDim）应计入
    InputCaptureWorker boundary;
    boundary.setDpi(96.0);
    boundary.setVirtualScreenMaxDim(1000);
    boundary.handleEvent(makeMove(0, 0, 1000));
    boundary.handleEvent(makeMove(2000, 0, 1100));
    QCOMPARE(boundary.flush(1200).distanceMeters, 2000 * kPxToMeter96Dpi);
}

void TestInputCaptureWorker::keyboardCounting()
{
    InputCaptureWorker worker;

    // 普通按键 +1
    worker.handleEvent(makeKey(0x41, false, false, 1100)); // 'A'
    worker.handleEvent(makeKey(0x42, false, false, 1150)); // 'B'
    worker.handleEvent(makeKey(0x43, false, false, 1200)); // 'C'

    // 长按自动重复丢弃（02 号文档 §4.2）
    worker.handleEvent(makeKey(0x41, false, true, 1250));

    // 纯修饰键：无后续非修饰键则计入一次
    worker.handleEvent(makeKey(0x10, true, false, 1300)); // Shift

    QCOMPARE(worker.flush(1400).keyPresses, 4);

    // 组合键 Ctrl+C：修饰符不重复计数
    worker.handleEvent(makeKey(0x11, true, false, 1500));  // Ctrl down
    worker.handleEvent(makeKey(0x43, false, false, 1600)); // C down → Ctrl 待定清除

    QCOMPARE(worker.flush(1700).keyPresses, 1);

    // Ctrl 按住连打两个字母：两个字母各 +1，Ctrl 不计
    worker.handleEvent(makeKey(0x11, true, false, 1800));
    worker.handleEvent(makeKey(0x58, false, false, 1900)); // 'X'
    worker.handleEvent(makeKey(0x59, false, false, 1950)); // 'Y'

    QCOMPARE(worker.flush(2000).keyPresses, 2);
}

void TestInputCaptureWorker::clickAndScrollCounting()
{
    InputCaptureWorker worker;

    worker.handleEvent(makeClick(HookEventKind::MouseLeftClick, 1000));
    worker.handleEvent(makeClick(HookEventKind::MouseLeftClick, 1010));
    worker.handleEvent(makeClick(HookEventKind::MouseRightClick, 1020));
    worker.handleEvent(makeClick(HookEventKind::MouseMidClick, 1030));
    worker.handleEvent(makeWheel(3, 1040));
    worker.handleEvent(makeWheel(-2, 1050)); // 绝对值累计

    const InputDelta delta = worker.flush(1100);
    QCOMPARE(delta.leftClicks, 2);
    QCOMPARE(delta.rightClicks, 1);
    QCOMPARE(delta.midClicks, 1);
    QCOMPARE(delta.scrollStepsAbs, 5);
}

void TestInputCaptureWorker::activeTimeInference()
{
    InputCaptureWorker worker;
    worker.setIdleThresholdMs(120 * 1000);

    worker.handleEvent(makeMove(0, 0, 1000));
    worker.flush(2000); // 建立基线

    worker.handleEvent(makeMove(10, 0, 3000));
    // 区间 [2000, 6000)，最近输入在 3000 ≤ 阈值 → 整个区间活跃
    QCOMPARE(worker.flush(6000).activeSeconds, 4);

    // 超过空闲阈值 → 不活跃
    InputCaptureWorker idle;
    idle.setIdleThresholdMs(1000);
    idle.flush(9000);
    idle.handleEvent(makeMove(0, 0, 10000));
    QCOMPARE(idle.flush(20000).activeSeconds, 0); // 10000ms > 1000ms 阈值
}

void TestInputCaptureWorker::pauseResume()
{
    InputCaptureWorker worker;
    worker.setVirtualScreenMaxDim(1000);
    QSignalSpy spy(&worker, &InputCaptureWorker::deltaReady);

    worker.handleEvent(makeKey(0x41, false, false, 1000));
    worker.handleEvent(makeMove(0, 0, 1050));

    worker.pause(); // 暂停前冲刷积压增量
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<InputDelta>().keyPresses, 1);

    worker.handleEvent(makeKey(0x42, false, false, 1100)); // 暂停期间忽略
    worker.resume();
    worker.handleEvent(makeMove(1000, 0, 1200)); // 基线已重置 → 不计距离

    const InputDelta delta = worker.flush(1300);
    QCOMPARE(delta.distanceMeters, 0.0);
    QCOMPARE(delta.keyPresses, 0);
}

void TestInputCaptureWorker::flushClearsState()
{
    InputCaptureWorker worker;

    worker.handleEvent(makeKey(0x41, false, false, 1000));
    worker.handleEvent(makeClick(HookEventKind::MouseLeftClick, 1010));
    const InputDelta first = worker.flush(1100);
    QCOMPARE(first.keyPresses, 1);
    QCOMPARE(first.leftClicks, 1);

    const InputDelta second = worker.flush(1200);
    QCOMPARE(second.keyPresses, 0);
    QCOMPARE(second.leftClicks, 0);
    QCOMPARE(second.rightClicks, 0);
    QCOMPARE(second.midClicks, 0);
    QCOMPARE(second.scrollStepsAbs, 0);
    QCOMPARE(second.distanceMeters, 0.0);
}

QTEST_MAIN(TestInputCaptureWorker)
#include "tst_InputCaptureWorker.moc"
