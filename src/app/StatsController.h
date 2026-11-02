#pragma once

#include <QObject>

#include "data/DataManager.h"

// QML 绑定层（04 号文档 §1/§2.1）：以只读属性暴露今日统计，
// 供 DashboardPage 实时刷新。增量每 3s 到达一次，天然满足 §7 的 ≥1s 刷新节流。
class StatsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double todayDistanceM READ todayDistanceM NOTIFY todayStatsChanged)
    Q_PROPERTY(int todayKeyPresses READ todayKeyPresses NOTIFY todayStatsChanged)
    Q_PROPERTY(int todayLeftClicks READ todayLeftClicks NOTIFY todayStatsChanged)
    Q_PROPERTY(int todayRightClicks READ todayRightClicks NOTIFY todayStatsChanged)
    Q_PROPERTY(int todayScrollSteps READ todayScrollSteps NOTIFY todayStatsChanged)
    Q_PROPERTY(int todayActiveMinutes READ todayActiveMinutes NOTIFY todayStatsChanged)
    Q_PROPERTY(bool capturePaused READ capturePaused WRITE setCapturePaused
                   NOTIFY capturePausedChanged)
    // 采集失败原因（macOS 辅助功能权限 / Wayland 降级 / Win 钩子被拦截），空串=正常
    Q_PROPERTY(QString captureError READ captureError WRITE setCaptureError
                   NOTIFY captureErrorChanged)

public:
    explicit StatsController(DataManager* dataManager, QObject* parent = nullptr);

    double todayDistanceM() const { return stats_.mouseDistanceM; }
    int todayKeyPresses() const { return stats_.keyPressCount; }
    int todayLeftClicks() const { return stats_.leftClickCount; }
    int todayRightClicks() const { return stats_.rightClickCount; }
    int todayScrollSteps() const { return stats_.scrollSteps; }
    int todayActiveMinutes() const { return stats_.activeMinutes; }

    // 托盘"暂停统计"时同步置位，Dashboard 显示"已暂停"标签（04 号文档 §6）。
    // 真实暂停状态在采集线程 worker，此处仅维护 UI 可见的镜像。
    bool capturePaused() const { return capturePaused_; }
    void setCapturePaused(bool paused);

    QString captureError() const { return captureError_; }
    void setCaptureError(const QString& error);

    Q_INVOKABLE void refresh();

signals:
    void todayStatsChanged();
    void capturePausedChanged(bool paused);
    void captureErrorChanged(const QString& error);

private:
    DataManager* dataManager_;
    DailyStats stats_;
    bool capturePaused_ = false;
    QString captureError_;
};
