#include "data/DataManager.h"

#include <QDebug>
#include <QTime>

DataManager::DataManager(Database* database, QObject* parent)
    : QObject(parent)
    , db_(database)
{
    saveTimer_.setInterval(saveIntervalMs_);
    connect(&saveTimer_, &QTimer::timeout, this, &DataManager::onSaveTimer);
}

bool DataManager::start(const QString& filePath)
{
    if (!db_->isOpen() && !db_->open(filePath)) {
        return false;
    }

    resetCache(QDate::currentDate());
    loadFromDatabase();

    // 每日首次 flush 前的启动清理（03 号文档 §3.3）
    if (retentionDays_ > 0) {
        db_->purgeBefore(QDate::currentDate().addDays(-retentionDays_));
    }

    saveTimer_.start();
    return true;
}

void DataManager::stop()
{
    saveTimer_.stop();
    flush();
}

void DataManager::flush()
{
    saveSnapshot();
}

void DataManager::applyDelta(const InputDelta& delta)
{
    rolloverIfNeeded();

    cache_.mouseDistanceM += delta.distanceMeters;
    cache_.keyPressCount += delta.keyPresses;
    cache_.leftClickCount += delta.leftClicks;
    cache_.rightClickCount += delta.rightClicks;
    cache_.scrollSteps += delta.scrollStepsAbs;
    activeSeconds_ += delta.activeSeconds;

    const int hour = QTime::currentTime().hour();
    hours_[hour].distanceM += delta.distanceMeters;
    hours_[hour].keyPresses += delta.keyPresses;

    emit todayChanged();
}

void DataManager::setSaveIntervalMs(int ms)
{
    saveIntervalMs_ = ms;
    saveTimer_.setInterval(ms);
    if (saveTimer_.isActive()) {
        saveTimer_.stop();
        saveTimer_.start();
    }
}

void DataManager::setRetentionDays(int days)
{
    retentionDays_ = days;
}

DailyStats DataManager::today() const
{
    DailyStats result = cache_;
    result.activeMinutes = activeSeconds_ / 60;
    return result;
}

QList<HourlyStats> DataManager::todayHourly() const
{
    QList<HourlyStats> result;
    for (int hour = 0; hour < 24; ++hour) {
        HourlyStats record;
        record.hour = hour;
        record.mouseDistanceM = hours_[hour].distanceM;
        record.keyPressCount = hours_[hour].keyPresses;
        result.append(record);
    }
    return result;
}

void DataManager::onSaveTimer()
{
    rolloverIfNeeded();
    saveSnapshot();
}

// 跨零点：先落盘上一天，再开启新一天缓存（02 号文档 §5，以合并时间归属）。
bool DataManager::rolloverIfNeeded()
{
    const QDate today = QDate::currentDate();
    if (today == cache_.date) {
        return true;
    }
    saveSnapshot();
    resetCache(today);
    return true;
}

bool DataManager::saveSnapshot()
{
    const QDate date = cache_.date;
    if (!date.isValid()) {
        return false;
    }

    DailyStats daily = cache_;
    daily.activeMinutes = activeSeconds_ / 60;

    // 写入失败重试 3 次，仍失败则保留内存缓存等待下次触发（01 号文档 §6）
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (db_->beginTransaction()) {
            bool ok = db_->upsertDaily(daily);
            for (int hour = 0; ok && hour < 24; ++hour) {
                if (!hours_[hour].active()) {
                    continue;
                }
                HourlyStats record;
                record.hour = hour;
                record.mouseDistanceM = hours_[hour].distanceM;
                record.keyPressCount = hours_[hour].keyPresses;
                ok = db_->upsertHourly(date, record);
            }
            if (ok && db_->commitTransaction()) {
                emit statsSaved();
                return true;
            }
            db_->rollbackTransaction();
        }
        qWarning() << "DataManager: save attempt" << attempt + 1 << "failed, cache retained";
    }
    return false;
}

void DataManager::resetCache(const QDate& date)
{
    cache_ = DailyStats();
    cache_.date = date;
    activeSeconds_ = 0;
    for (HourBucket& bucket : hours_) {
        bucket = HourBucket();
    }
}

// 以数据库当日行为初值恢复缓存，保证覆盖写语义下重启不丢已有数据。
void DataManager::loadFromDatabase()
{
    const QDate today = cache_.date;

    const auto daily = db_->queryDaily(today, today);
    if (!daily.isEmpty()) {
        cache_ = daily.first();
    }
    activeSeconds_ = cache_.activeMinutes * 60; // 秒级精度不落库，按分钟近似恢复

    const auto hourly = db_->queryHourly(today);
    for (const HourlyStats& record : hourly) {
        hours_[record.hour].distanceM = record.mouseDistanceM;
        hours_[record.hour].keyPresses = record.keyPressCount;
    }
}
