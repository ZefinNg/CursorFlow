#include "app/HistoryController.h"

#include <QDate>
#include <QHash>
#include <QVariantMap>

HistoryController::HistoryController(Database* database, DataManager* dataManager,
                                     QObject* parent)
    : QObject(parent)
    , db_(database)
    , dataManager_(dataManager)
{
    Q_ASSERT(db_);
    Q_ASSERT(dataManager_);
    connect(dataManager_, &DataManager::todayChanged, this, &HistoryController::dataChanged);
    connect(dataManager_, &DataManager::statsSaved, this, &HistoryController::dataChanged);
}

QVariantMap HistoryController::hourlyToMap(const HourlyStats& record)
{
    QVariantMap map;
    map.insert(QStringLiteral("hour"), record.hour);
    map.insert(QStringLiteral("distance"), record.mouseDistanceM);
    map.insert(QStringLiteral("keys"), record.keyPressCount);
    return map;
}

QVariantList HistoryController::todayHourly() const
{
    const QList<HourlyStats> hours = dataManager_->todayHourly();
    QVariantList list;
    list.reserve(hours.size());
    for (const HourlyStats& record : hours) {
        list.append(hourlyToMap(record));
    }
    return list;
}

QVariantList HistoryController::dailySeries(int days) const
{
    QVariantList list;
    if (days <= 0) {
        return list;
    }

    const QDate today = QDate::currentDate();
    const QDate from = today.addDays(-(days - 1));

    QHash<QDate, DailyStats> byDate;
    const QList<DailyStats> rows = db_->queryDaily(from, today);
    byDate.reserve(rows.size());
    for (const DailyStats& row : rows) {
        byDate.insert(row.date, row);
    }

    // 今日以内存缓存为准：1h 落盘间隔内的实时增量尚不在库中（03 号文档 §3.2）
    const DailyStats live = dataManager_->today();
    if (live.date == today) {
        byDate.insert(today, live);
    }

    // 缺失日期补零（03 号文档 §4 queryDaily 语义）
    list.reserve(days);
    for (QDate date = from; date <= today; date = date.addDays(1)) {
        QVariantMap map;
        map.insert(QStringLiteral("date"), date.toString(Qt::ISODate));
        const auto it = byDate.constFind(date);
        map.insert(QStringLiteral("distance"),
                   it == byDate.constEnd() ? 0.0 : it->mouseDistanceM);
        map.insert(QStringLiteral("keys"), it == byDate.constEnd() ? 0 : it->keyPressCount);
        list.append(map);
    }
    return list;
}

QVariantList HistoryController::hourlySeries(const QDate& date) const
{
    if (date == QDate::currentDate()) {
        return todayHourly(); // 今日含未落盘的实时增量
    }

    QVariantList list;
    const QList<HourlyStats> hours = db_->queryHourly(date); // 24 项，已补零
    list.reserve(hours.size());
    for (const HourlyStats& record : hours) {
        list.append(hourlyToMap(record));
    }
    return list;
}
