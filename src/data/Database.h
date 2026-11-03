#pragma once

#include <QDate>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>

// 当日聚合统计（对应 daily_stats 表）
struct DailyStats {
    QDate date;
    double mouseDistanceM = 0.0;
    int keyPressCount = 0;
    int leftClickCount = 0;
    int rightClickCount = 0;
    int scrollSteps = 0;
    int activeMinutes = 0;
};

// 单小时统计（对应 hourly_stats 表）
struct HourlyStats {
    int hour = 0; // 0-23
    double mouseDistanceM = 0.0;
    int keyPressCount = 0;
};

// SQLite 存储层封装（设计见 docs/tech/03-数据存储设计.md）。
// 仅限主线程使用；写入为"覆盖写"UPSERT，同一快照重复写入不翻倍。
class Database
{
public:
    static constexpr int kSchemaVersion = 1;

    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    static QString connectionName();

    // 默认数据库文件位置（03 号文档 §2：%APPDATA%\CursorFlow\stats.db 等）
    static QString defaultFilePath();

    // 打开数据库并确保 schema 就绪。
    // 检测到损坏文件时自动备份（*.corrupt-<ts>）后重建，见 03 号文档 §7。
    bool open(const QString& filePath);
    void close();
    bool isOpen() const { return db_.isOpen(); }

    bool beginTransaction();
    bool commitTransaction();
    void rollbackTransaction();

    bool upsertDaily(const DailyStats& record);
    bool upsertHourly(const QDate& date, const HourlyStats& record);

    // 指定日期区间（含边界）的日统计，按日期升序。
    QList<DailyStats> queryDaily(const QDate& from, const QDate& to) const;

    // 固定返回 24 项，缺失小时补零（03 号文档 §4）。
    QList<HourlyStats> queryHourly(const QDate& date) const;

    // 删除早于 cutoff 的日/小时数据，返回删除的 daily 行数，失败返回 -1（03 号文档 §3.3）。
    int purgeBefore(const QDate& cutoff);

    // 成就（M5 7.3，07 号文档 §3.2）：INSERT OR IGNORE 幂等写入，返回是否"新"解锁
    // （false = 已存在或写入失败，不重复弹窗）。
    bool recordAchievement(const QString& id, const QString& unlockedAtIso);

    // 已解锁成就：id → 达成时间（ISO 字符串）
    QVariantMap achievementUnlocks() const;

    int schemaVersion() const;

private:
    bool createSchema();
    bool handleCorruption();

    QSqlDatabase db_;
    QString filePath_;
};
