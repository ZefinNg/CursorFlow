#pragma once

#include <QObject>
#include <QTimer>

#include <QDate>
#include <QList>

#include "data/Database.h"
#include "data/InputDelta.h"

// 聚合层：合并采集增量、维护当日内存缓存、定时落盘
// （01 号文档 §4 数据流、03 号文档 §3 写入策略）。
// 仅限主线程使用；落盘采用覆盖写，缓存以数据库当日行为初值，
// 重启后恢复缓存可保证覆盖写不丢数据。
class DataManager : public QObject
{
    Q_OBJECT

public:
    explicit DataManager(Database* database, QObject* parent = nullptr);

    // 打开数据库、恢复当日缓存、清理过期数据并启动定时落盘。
    bool start(const QString& filePath = Database::defaultFilePath());

    // 停止定时器并落盘（应用退出时调用，01 号文档 §4.2 "退出 flush"）。
    void stop();

    // 立即落盘（手动触发）。
    void flush();

    // 采集线程经排队信号提交的 3 秒增量（02 号文档 §2）。
    void applyDelta(const InputDelta& delta);

    // 测试与调试用：默认 1 小时（03 号文档 §3.2）。
    void setSaveIntervalMs(int ms);
    void setRetentionDays(int days);

    DailyStats today() const;               // activeMinutes 由秒换算的整分钟
    QList<HourlyStats> todayHourly() const; // 24 项，供实时热力图（04 号文档 §2.2）

signals:
    void todayChanged();
    void statsSaved();

private slots:
    void onSaveTimer();

private:
    struct HourBucket {
        double distanceM = 0.0;
        int keyPresses = 0;
        bool active() const { return distanceM > 0.0 || keyPresses > 0; }
    };

    bool rolloverIfNeeded();
    bool saveSnapshot();
    void resetCache(const QDate& date);
    void loadFromDatabase();

    Database* db_ = nullptr;
    QTimer saveTimer_;
    DailyStats cache_;
    HourBucket hours_[24];
    int activeSeconds_ = 0;
    int saveIntervalMs_ = 3600 * 1000;
    int retentionDays_ = 365;
};
