#pragma once

#include <QObject>
#include <QTimer>

#include <atomic>

#include "capture/IPlatformHook.h"
#include "data/InputDelta.h"

// 采集线程核心（02 号文档 §2）：事件回调内仅做轻量原子累加，
// 定时/手动 flush 取出增量并经 deltaReady 队列信号提交聚合层。
// 距离算法、跳变剔除、键盘计数规则、活跃推断见 02 号文档 §4。
class InputCaptureWorker : public QObject
{
    Q_OBJECT

public:
    explicit InputCaptureWorker(QObject* parent = nullptr);

    // 启动采集：装载平台钩子（可为空：仅运行内部 flush 定时器，供测试/演示）。
    bool start(IPlatformHook* hook = nullptr);
    void stop();

    // 托盘"暂停/恢复统计"（02 号文档 §5）：暂停前先冲刷积压增量，恢复时重置坐标基准。
    void pause();
    void resume();
    bool isPaused() const { return paused_; }

    // 休眠恢复等场景由外部调用，防止恢复后首步距离误判。
    void resetPositionBaseline();

    void setVirtualScreenMaxDim(double px); // 跳变阈值 = 2 × maxDim（02 号文档 §4.1）
    void setDpi(double dpi);                // 校准联动（任务 4.4），默认 96
    void setIdleThresholdMs(qint64 ms);     // 活跃推断空闲阈值，默认 120s
    void setFlushIntervalMs(int ms);        // 默认 3000ms（02 号文档 §2）

public slots:
    // 处理单个输入事件：钩子线程同步调用，轻量无锁。
    void handleEvent(const HookEvent& event);

    // 取出增量快照并发送 deltaReady；nowMs 为归属时间基准（测试可注入）。
    InputDelta flush(qint64 nowMs);

signals:
    void deltaReady(InputDelta delta);

private slots:
    void onFlushTimer();

private:
    void handleKeyDown(const HookEvent& event);

    IPlatformHook* hook_ = nullptr;

    // 增量缓冲区（02 号文档 §2）：回调线程写，flush 同线程读
    std::atomic<double> distancePx_{0.0};
    std::atomic<int> keyPresses_{0};
    std::atomic<int> leftClicks_{0};
    std::atomic<int> rightClicks_{0};
    std::atomic<int> midClicks_{0};
    std::atomic<int> scrollSteps_{0};
    std::atomic<qint64> lastInputMs_{0};

    // 仅回调线程访问（低级钩子回调与 flush 同在采集线程）
    bool hasLastPos_ = false;
    double lastX_ = 0.0;
    double lastY_ = 0.0;
    int pendingModifiers_ = 0;
    bool paused_ = false;
    qint64 lastFlushMs_ = 0;

    double jumpThresholdPx_ = 3840.0; // 默认 2 × 1920
    double dpi_ = 96.0;
    qint64 idleThresholdMs_ = 120 * 1000;
    int flushIntervalMs_ = 3 * 1000; // 3s flush（02 号文档 §2）
    // 定时器在 start() 内于采集线程懒创建（worker 在主线程构造后 moveToThread，
    // 若成员 QTimer 无 parent 会停留在主线程亲和性，跨线程 start 触发告警且不生效）
    QTimer* flushTimer_ = nullptr;
};
