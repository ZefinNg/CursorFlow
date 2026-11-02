#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTime>

#include "data/DataManager.h"
#include "data/Database.h"

namespace {

InputDelta makeDelta(double meters, int keys)
{
    InputDelta delta;
    delta.distanceMeters = meters;
    delta.keyPresses = keys;
    delta.leftClicks = 3;
    delta.rightClicks = 1;
    delta.scrollStepsAbs = 7;
    delta.activeSeconds = 90;
    return delta;
}

} // namespace

class TestDataManager : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void exitFlushPersists();          // 退出 flush（01 号文档 §4.2）
    void saveTimerPersists();          // 定时落盘
    void restartRestoresCache();       // 重启恢复缓存，覆盖写不丢数据
    void purgeOnStart();               // 启动清理过期数据（03 号文档 §3.3）
    void saveFailureRetainsCache();    // 写入失败保留缓存，恢复后补写（01 号文档 §6）
    void todayAccessors();             // today()/todayHourly() 供 UI 读取

private:
    QString dbPath() const { return tempDir_->filePath(QStringLiteral("stats.db")); }
    QDate today() const { return QDate::currentDate(); }

    QTemporaryDir* tempDir_ = nullptr;
    Database* db_ = nullptr;
    DataManager* mgr_ = nullptr;
};

void TestDataManager::init()
{
    tempDir_ = new QTemporaryDir();
    QVERIFY(tempDir_->isValid());
    db_ = new Database();
    mgr_ = new DataManager(db_);
}

void TestDataManager::cleanup()
{
    delete mgr_;
    mgr_ = nullptr;
    delete db_;
    db_ = nullptr;
    delete tempDir_;
    tempDir_ = nullptr;
}

void TestDataManager::exitFlushPersists()
{
    QVERIFY(mgr_->start(dbPath()));

    QSignalSpy spy(mgr_, &DataManager::statsSaved);
    mgr_->applyDelta(makeDelta(100.5, 42));
    mgr_->stop(); // 退出 flush

    QCOMPARE(spy.count(), 1);

    const auto rows = db_->queryDaily(today(), today());
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().mouseDistanceM, 100.5);
    QCOMPARE(rows.first().keyPressCount, 42);
    QCOMPARE(rows.first().leftClickCount, 3);
    QCOMPARE(rows.first().rightClickCount, 1);
    QCOMPARE(rows.first().scrollSteps, 7);
    QCOMPARE(rows.first().activeMinutes, 1); // 90s → 1min

    const auto hours = db_->queryHourly(today());
    QCOMPARE(hours[QTime::currentTime().hour()].keyPressCount, 42);
    QCOMPARE(hours[QTime::currentTime().hour()].mouseDistanceM, 100.5);
}

void TestDataManager::saveTimerPersists()
{
    mgr_->setSaveIntervalMs(50);
    QVERIFY(mgr_->start(dbPath()));

    QSignalSpy spy(mgr_, &DataManager::statsSaved);
    mgr_->applyDelta(makeDelta(5.0, 10));

    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2000);

    const auto rows = db_->queryDaily(today(), today());
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().keyPressCount, 10);
}

void TestDataManager::restartRestoresCache()
{
    QVERIFY(mgr_->start(dbPath()));
    mgr_->applyDelta(makeDelta(10.0, 20));
    mgr_->stop();

    // 模拟重启：新 DataManager 复用同一数据库
    DataManager second(db_);
    QVERIFY(second.start(dbPath()));
    second.applyDelta(makeDelta(15.0, 30));
    second.stop();

    const auto rows = db_->queryDaily(today(), today());
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().mouseDistanceM, 25.0);
    QCOMPARE(rows.first().keyPressCount, 50);
}

void TestDataManager::purgeOnStart()
{
    // 直接预置一条过期数据
    QVERIFY(db_->open(dbPath()));
    DailyStats old;
    old.date = today().addDays(-30);
    old.keyPressCount = 99;
    QVERIFY(db_->upsertDaily(old));
    db_->close();

    DataManager mgr(db_);
    mgr.setRetentionDays(7);
    QVERIFY(mgr.start(dbPath()));
    mgr.stop();

    const auto rows = db_->queryDaily(today().addDays(-30), today().addDays(-30));
    QVERIFY(rows.isEmpty());
}

void TestDataManager::saveFailureRetainsCache()
{
    QVERIFY(mgr_->start(dbPath()));
    mgr_->setSaveIntervalMs(30);
    mgr_->applyDelta(makeDelta(7.0, 14));

    // 关闭底层连接模拟写入失败
    db_->close();
    QSignalSpy spy(mgr_, &DataManager::statsSaved);
    QTest::qWait(120); // 至少经历一次失败的定时保存
    QCOMPARE(spy.count(), 0);

    // 恢复连接后，缓存中的数据应完整落盘。
    // 注意：保存定时器仍在 30ms 周期触发，statsSaved 可能多次发射，
    // 断言"至少成功一次"而非精确 1 次（避免慢机器上的计数竞态）。
    QVERIFY(db_->open(dbPath()));
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 1, 2000);

    const auto rows = db_->queryDaily(today(), today());
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().mouseDistanceM, 7.0);
    QCOMPARE(rows.first().keyPressCount, 14);
}

void TestDataManager::todayAccessors()
{
    QVERIFY(mgr_->start(dbPath()));

    const DailyStats empty = mgr_->today();
    QCOMPARE(empty.activeMinutes, 0);
    QCOMPARE(mgr_->todayHourly().size(), 24);

    QSignalSpy spy(mgr_, &DataManager::todayChanged);
    mgr_->applyDelta(makeDelta(1.0, 2));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(mgr_->today().keyPressCount, 2);
    QCOMPARE(mgr_->today().mouseDistanceM, 1.0);
    QCOMPARE(mgr_->todayHourly()[QTime::currentTime().hour()].keyPressCount, 2);
}

QTEST_MAIN(TestDataManager)
#include "tst_DataManager.moc"
