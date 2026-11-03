#include "data/Database.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

namespace {

QString dateKey(const QDate& date)
{
    return date.toString(Qt::ISODate);
}

} // namespace

Database::~Database()
{
    close();
}

QString Database::connectionName()
{
    return QStringLiteral("cursorflow");
}

// GenericDataLocation 在三平台分别对应 %APPDATA%、~/Library/Application Support、
// ~/.local/share，与 03 号文档 §2 的路径表一致。
QString Database::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/CursorFlow/stats.db");
}

bool Database::open(const QString& filePath)
{
    close();
    filePath_ = filePath;

    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) {
        qWarning() << "Database: cannot create directory for" << filePath;
        return false;
    }

    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName());
    db_.setDatabaseName(filePath);
    if (!db_.open()) {
        qWarning() << "Database: open failed:" << db_.lastError().text();
        return false;
    }

    QSqlQuery pragma(db_);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

    QSqlQuery check(db_);
    check.exec(QStringLiteral("PRAGMA quick_check"));
    if (!check.isActive() || !check.next() || check.value(0).toString() != QLatin1String("ok")) {
        qWarning() << "Database: quick_check failed, file may be corrupted:" << filePath;
        return handleCorruption();
    }

    if (!createSchema()) {
        return false;
    }

    const int version = schemaVersion();
    if (version > kSchemaVersion) {
        qWarning() << "Database: schema version" << version << "is newer than supported"
                   << kSchemaVersion << "(program may misbehave)";
    }

    return true;
}

void Database::close()
{
    if (db_.isValid()) {
        const QString name = db_.connectionName();
        db_.close();
        db_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(name);
    }
}

bool Database::beginTransaction()
{
    return db_.transaction();
}

bool Database::commitTransaction()
{
    return db_.commit();
}

void Database::rollbackTransaction()
{
    db_.rollback();
}

bool Database::createSchema()
{
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS daily_stats ("
                       "date TEXT PRIMARY KEY, "
                       "mouse_distance_m REAL NOT NULL DEFAULT 0, "
                       "key_press_count INTEGER NOT NULL DEFAULT 0, "
                       "left_click_count INTEGER NOT NULL DEFAULT 0, "
                       "right_click_count INTEGER NOT NULL DEFAULT 0, "
                       "scroll_steps INTEGER NOT NULL DEFAULT 0, "
                       "active_minutes INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS hourly_stats ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "date TEXT NOT NULL, "
                       "hour INTEGER NOT NULL CHECK(hour BETWEEN 0 AND 23), "
                       "mouse_distance_m REAL NOT NULL DEFAULT 0, "
                       "key_press_count INTEGER NOT NULL DEFAULT 0, "
                       "UNIQUE(date, hour))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_hourly_date ON hourly_stats(date)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS meta ("
                       "key TEXT PRIMARY KEY, "
                       "value TEXT NOT NULL)"),
        // 成就表（M5 7.3，07 号文档 §3.2）：只记已达成 id 与时间，定义在代码内维护
        QStringLiteral("CREATE TABLE IF NOT EXISTS achievements ("
                       "id TEXT PRIMARY KEY, "
                       "unlocked_at TEXT NOT NULL)"),
        QStringLiteral("INSERT OR IGNORE INTO meta(key, value) VALUES('schema_version', '%1')")
            .arg(kSchemaVersion),
        QStringLiteral("INSERT OR IGNORE INTO meta(key, value) VALUES('first_run_date', '%1')")
            .arg(dateKey(QDate::currentDate())),
    };

    QSqlQuery query(db_);
    for (const QString& sql : statements) {
        if (!query.exec(sql)) {
            qWarning() << "Database: createSchema failed:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool Database::handleCorruption()
{
    close();

    const QString stamp = QString::number(QDateTime::currentSecsSinceEpoch());
    const QStringList sidecars = {
        filePath_,
        filePath_ + QStringLiteral("-wal"),
        filePath_ + QStringLiteral("-shm"),
    };
    for (const QString& file : sidecars) {
        if (QFileInfo::exists(file)) {
            const QString backup = file + QStringLiteral(".corrupt-") + stamp;
            if (!QFile::rename(file, backup)) {
                qWarning() << "Database: failed to back up corrupted file:" << file;
            }
        }
    }

    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName());
    db_.setDatabaseName(filePath_);
    if (!db_.open()) {
        qWarning() << "Database: reopen after corruption failed:" << db_.lastError().text();
        return false;
    }

    QSqlQuery pragma(db_);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

    if (!createSchema()) {
        return false;
    }

    qWarning() << "Database: corrupted database was backed up and rebuilt:" << filePath_;
    return true;
}

bool Database::upsertDaily(const DailyStats& record)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "INSERT INTO daily_stats(date, mouse_distance_m, key_press_count, "
        "left_click_count, right_click_count, scroll_steps, active_minutes) "
        "VALUES(?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(date) DO UPDATE SET "
        "mouse_distance_m = excluded.mouse_distance_m, "
        "key_press_count = excluded.key_press_count, "
        "left_click_count = excluded.left_click_count, "
        "right_click_count = excluded.right_click_count, "
        "scroll_steps = excluded.scroll_steps, "
        "active_minutes = excluded.active_minutes"));
    query.addBindValue(dateKey(record.date));
    query.addBindValue(record.mouseDistanceM);
    query.addBindValue(record.keyPressCount);
    query.addBindValue(record.leftClickCount);
    query.addBindValue(record.rightClickCount);
    query.addBindValue(record.scrollSteps);
    query.addBindValue(record.activeMinutes);

    if (!query.exec()) {
        qWarning() << "Database: upsertDaily failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool Database::upsertHourly(const QDate& date, const HourlyStats& record)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "INSERT INTO hourly_stats(date, hour, mouse_distance_m, key_press_count) "
        "VALUES(?, ?, ?, ?) "
        "ON CONFLICT(date, hour) DO UPDATE SET "
        "mouse_distance_m = excluded.mouse_distance_m, "
        "key_press_count = excluded.key_press_count"));
    query.addBindValue(dateKey(date));
    query.addBindValue(record.hour);
    query.addBindValue(record.mouseDistanceM);
    query.addBindValue(record.keyPressCount);

    if (!query.exec()) {
        qWarning() << "Database: upsertHourly failed:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<DailyStats> Database::queryDaily(const QDate& from, const QDate& to) const
{
    QList<DailyStats> result;

    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "SELECT date, mouse_distance_m, key_press_count, left_click_count, "
        "right_click_count, scroll_steps, active_minutes FROM daily_stats "
        "WHERE date >= ? AND date <= ? ORDER BY date"));
    query.addBindValue(dateKey(from));
    query.addBindValue(dateKey(to));

    if (!query.exec()) {
        qWarning() << "Database: queryDaily failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        DailyStats record;
        record.date = QDate::fromString(query.value(0).toString(), Qt::ISODate);
        record.mouseDistanceM = query.value(1).toDouble();
        record.keyPressCount = query.value(2).toInt();
        record.leftClickCount = query.value(3).toInt();
        record.rightClickCount = query.value(4).toInt();
        record.scrollSteps = query.value(5).toInt();
        record.activeMinutes = query.value(6).toInt();
        result.append(record);
    }
    return result;
}

QList<HourlyStats> Database::queryHourly(const QDate& date) const
{
    QList<HourlyStats> result;
    for (int hour = 0; hour < 24; ++hour) {
        HourlyStats record;
        record.hour = hour;
        result.append(record);
    }

    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "SELECT hour, mouse_distance_m, key_press_count FROM hourly_stats WHERE date = ?"));
    query.addBindValue(dateKey(date));

    if (!query.exec()) {
        qWarning() << "Database: queryHourly failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        const int hour = query.value(0).toInt();
        if (hour >= 0 && hour < 24) {
            result[hour].mouseDistanceM = query.value(1).toDouble();
            result[hour].keyPressCount = query.value(2).toInt();
        }
    }
    return result;
}

int Database::purgeBefore(const QDate& cutoff)
{
    if (!beginTransaction()) {
        return -1;
    }

    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM daily_stats WHERE date < ?"));
    query.addBindValue(dateKey(cutoff));
    if (!query.exec()) {
        qWarning() << "Database: purge daily failed:" << query.lastError().text();
        rollbackTransaction();
        return -1;
    }
    const int removed = query.numRowsAffected();

    query.prepare(QStringLiteral("DELETE FROM hourly_stats WHERE date < ?"));
    query.addBindValue(dateKey(cutoff));
    if (!query.exec()) {
        qWarning() << "Database: purge hourly failed:" << query.lastError().text();
        rollbackTransaction();
        return -1;
    }

    if (!commitTransaction()) {
        return -1;
    }
    return removed;
}

bool Database::recordAchievement(const QString& id, const QString& unlockedAtIso)
{
    QSqlQuery query(db_);
    query.prepare(
        QStringLiteral("INSERT OR IGNORE INTO achievements(id, unlocked_at) VALUES(?, ?)"));
    query.addBindValue(id);
    query.addBindValue(unlockedAtIso);
    if (!query.exec()) {
        qWarning() << "Database: record achievement failed:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0; // 0 = 已存在（幂等，不重复弹窗，07 号文档 §3.2）
}

QVariantMap Database::achievementUnlocks() const
{
    QVariantMap result;
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("SELECT id, unlocked_at FROM achievements"))) {
        qWarning() << "Database: query achievements failed:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.insert(query.value(0).toString(), query.value(1).toString());
    }
    return result;
}

int Database::schemaVersion() const
{
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("SELECT value FROM meta WHERE key = 'schema_version'"))
        || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}
