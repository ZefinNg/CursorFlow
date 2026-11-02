#include <QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "data/Database.h"

namespace {

DailyStats makeDaily(const QDate& date, int seed)
{
    DailyStats record;
    record.date = date;
    record.mouseDistanceM = seed * 1.5;
    record.keyPressCount = seed * 10;
    record.leftClickCount = seed;
    record.rightClickCount = seed * 2;
    record.scrollSteps = seed * 3;
    record.activeMinutes = seed * 4;
    return record;
}

} // namespace

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // 03 号文档 §8 测试要点
    void openCreatesSchema();
    void upsertDailyIdempotent();     // UPSERT 幂等：同一快照写两次数据不翻倍
    void upsertHourlyIdempotent();    // 覆盖写语义 + 唯一约束
    void queryDailyRange();           // 区间查询边界与排序
    void queryHourlyZeroFill();       // 缺失小时补零
    void retentionBoundary();         // 保留天数清理边界：恰好第 N 天应保留
    void transactionRollback();       // 回滚后无残留
    void corruptionRecovery();        // 损坏检测：备份重建（03 号文档 §7）
    void queryDailyPerformance();     // 10 万行规模查询性能

private:
    QString dbPath() const { return tempDir_->filePath(QStringLiteral("stats.db")); }

    QTemporaryDir* tempDir_ = nullptr;
    Database db_;
};

void TestDatabase::init()
{
    tempDir_ = new QTemporaryDir();
    QVERIFY(tempDir_->isValid());
}

void TestDatabase::cleanup()
{
    db_.close();
    delete tempDir_;
    tempDir_ = nullptr;
}

void TestDatabase::openCreatesSchema()
{
    QVERIFY(db_.open(dbPath()));
    QVERIFY(db_.isOpen());
    QCOMPARE(db_.schemaVersion(), Database::kSchemaVersion);

    // 关闭后重开，schema 仍在
    db_.close();
    QVERIFY(db_.open(dbPath()));
    QCOMPARE(db_.schemaVersion(), Database::kSchemaVersion);

    // WAL 模式（03 号文档 §3.2）
    QSqlQuery query(QSqlDatabase::database(Database::connectionName()));
    query.exec(QStringLiteral("PRAGMA journal_mode"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QLatin1String("wal"));
}

void TestDatabase::upsertDailyIdempotent()
{
    QVERIFY(db_.open(dbPath()));

    const QDate date(2026, 10, 30);
    const DailyStats record = makeDaily(date, 7);
    QVERIFY(db_.upsertDaily(record));
    QVERIFY(db_.upsertDaily(record));

    const auto rows = db_.queryDaily(date, date);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().date, date);
    QCOMPARE(rows.first().mouseDistanceM, 7 * 1.5);
    QCOMPARE(rows.first().keyPressCount, 70);
    QCOMPARE(rows.first().activeMinutes, 28);
}

void TestDatabase::upsertHourlyIdempotent()
{
    QVERIFY(db_.open(dbPath()));

    const QDate date(2026, 10, 30);
    HourlyStats record;
    record.hour = 9;
    record.mouseDistanceM = 12.5;
    record.keyPressCount = 100;
    QVERIFY(db_.upsertHourly(date, record));

    // 覆盖写：第二次写入的值生效且不累加
    record.mouseDistanceM = 25.0;
    QVERIFY(db_.upsertHourly(date, record));

    const auto hours = db_.queryHourly(date);
    QCOMPARE(hours.size(), 24);
    QCOMPARE(hours[9].mouseDistanceM, 25.0);
    QCOMPARE(hours[9].keyPressCount, 100);

    QSqlQuery query(QSqlDatabase::database(Database::connectionName()));
    query.exec(QStringLiteral("SELECT COUNT(*) FROM hourly_stats"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestDatabase::queryDailyRange()
{
    QVERIFY(db_.open(dbPath()));

    for (int day = 26; day <= 30; ++day) {
        QVERIFY(db_.upsertDaily(makeDaily(QDate(2026, 10, day), day)));
    }

    const auto rows = db_.queryDaily(QDate(2026, 10, 27), QDate(2026, 10, 29));
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows[0].date, QDate(2026, 10, 27));
    QCOMPARE(rows[1].date, QDate(2026, 10, 28));
    QCOMPARE(rows[2].date, QDate(2026, 10, 29));
    QCOMPARE(rows[0].keyPressCount, 270);
}

void TestDatabase::queryHourlyZeroFill()
{
    QVERIFY(db_.open(dbPath()));

    const QDate date(2026, 10, 30);
    HourlyStats first;
    first.hour = 0;
    first.keyPressCount = 5;
    HourlyStats last;
    last.hour = 23;
    last.keyPressCount = 95;
    QVERIFY(db_.upsertHourly(date, first));
    QVERIFY(db_.upsertHourly(date, last));

    const auto hours = db_.queryHourly(date);
    QCOMPARE(hours.size(), 24);
    QCOMPARE(hours[0].keyPressCount, 5);
    QCOMPARE(hours[23].keyPressCount, 95);
    for (int hour = 1; hour < 23; ++hour) {
        QCOMPARE(hours[hour].keyPressCount, 0);
        QCOMPARE(hours[hour].mouseDistanceM, 0.0);
    }
}

void TestDatabase::retentionBoundary()
{
    QVERIFY(db_.open(dbPath()));

    const QDate cutoff(2026, 10, 30);
    QVERIFY(db_.upsertDaily(makeDaily(cutoff.addDays(-1), 1)));
    QVERIFY(db_.upsertDaily(makeDaily(cutoff, 2)));
    QVERIFY(db_.upsertDaily(makeDaily(cutoff.addDays(1), 3)));

    HourlyStats record;
    record.hour = 5;
    record.keyPressCount = 9;
    QVERIFY(db_.upsertHourly(cutoff.addDays(-1), record));
    QVERIFY(db_.upsertHourly(cutoff, record));

    // 恰好等于 cutoff 的数据保留，更早的清理
    QCOMPARE(db_.purgeBefore(cutoff), 1);

    QCOMPARE(db_.queryDaily(QDate(2026, 1, 1), QDate(2027, 1, 1)).size(), 2);

    const auto purgedHours = db_.queryHourly(cutoff.addDays(-1));
    QCOMPARE(purgedHours[5].keyPressCount, 0);
    const auto keptHours = db_.queryHourly(cutoff);
    QCOMPARE(keptHours[5].keyPressCount, 9);
}

void TestDatabase::transactionRollback()
{
    QVERIFY(db_.open(dbPath()));

    const QDate date(2026, 10, 30);
    QVERIFY(db_.beginTransaction());
    QVERIFY(db_.upsertDaily(makeDaily(date, 5)));
    db_.rollbackTransaction();

    QVERIFY(db_.queryDaily(date, date).isEmpty());
}

void TestDatabase::corruptionRecovery()
{
    QVERIFY(db_.open(dbPath()));
    QVERIFY(db_.upsertDaily(makeDaily(QDate(2026, 10, 30), 1)));
    db_.close();

    // 破坏文件头，制造损坏
    QFile file(dbPath());
    QVERIFY(file.open(QIODevice::ReadWrite));
    file.write(QByteArray(4096, 'X'));
    file.close();

    // 重新打开：应自动备份损坏文件并重建（03 号文档 §7）
    QVERIFY(db_.open(dbPath()));

    QDir dir(QFileInfo(dbPath()).absolutePath());
    const QStringList corrupted =
        dir.entryList(QStringList() << QStringLiteral("*.corrupt-*"), QDir::Files);
    QVERIFY(!corrupted.isEmpty());

    // 重建后的库可正常读写
    QVERIFY(db_.upsertDaily(makeDaily(QDate(2026, 10, 30), 2)));
    const auto rows = db_.queryDaily(QDate(2026, 10, 30), QDate(2026, 10, 30));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().keyPressCount, 20);
}

void TestDatabase::queryDailyPerformance()
{
    QVERIFY(db_.open(dbPath()));

    const QDate start(2000, 1, 1);
    QVERIFY(db_.beginTransaction());
    DailyStats record;
    for (int i = 0; i < 100000; ++i) {
        record = makeDaily(start.addDays(i), i % 100);
        QVERIFY(db_.upsertDaily(record));
    }
    QVERIFY(db_.commitTransaction());

    QElapsedTimer timer;
    timer.start();
    const auto rows = db_.queryDaily(start.addDays(50000), start.addDays(50030));
    const qint64 elapsed = timer.elapsed();
    QCOMPARE(rows.size(), 31);
    // 设计目标 < 10 ms（03 号文档 §8），CI 环境波动放宽至 200 ms
    QVERIFY2(elapsed < 200,
             qPrintable(QStringLiteral("queryDaily took %1 ms").arg(elapsed)));
}

QTEST_MAIN(TestDatabase)
#include "tst_Database.moc"
