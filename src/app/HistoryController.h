#pragma once

#include <QObject>
#include <QVariantList>

#include "data/DataManager.h"
#include "data/Database.h"

// 历史查询绑定层（04 号文档 §2.2）：为 TrendChart / HeatmapGrid 提供
// 日/小时序列。查询同步执行（03 号文档 §4：数据量小，主线程延迟可忽略）。
// 今日数据优先取 DataManager 内存缓存（可能尚未落盘），历史日期取数据库并补零。
class HistoryController : public QObject
{
    Q_OBJECT
    // 今日 24 小时序列（24 项，缺失补零），Dashboard 底部热力图
    Q_PROPERTY(QVariantList todayHourly READ todayHourly NOTIFY dataChanged)

public:
    explicit HistoryController(Database* database, DataManager* dataManager,
                               QObject* parent = nullptr);

    HistoryController(const HistoryController&) = delete;
    HistoryController& operator=(const HistoryController&) = delete;

    QVariantList todayHourly() const;

    // 最近 days 天（含今日）日序列，按日期升序、缺失日期补零（03 号文档 §4）。
    // 返回 [{date: "yyyy-MM-dd", distance: 米, keys: 次数}, ...]
    Q_INVOKABLE QVariantList dailySeries(int days) const;

    // 指定日期 24 小时序列（Database::queryHourly 已补零，03 号文档 §4）。
    // 今日实时值合并内存缓存。
    Q_INVOKABLE QVariantList hourlySeries(const QDate& date) const;

signals:
    // 今日缓存变化（3s 增量）或落盘完成时通知 QML 重绘
    void dataChanged();

private:
    static QVariantMap hourlyToMap(const HourlyStats& record);

    Database* db_;
    DataManager* dataManager_;
};
