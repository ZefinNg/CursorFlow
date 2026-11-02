#include <QtTest>

#include <QDate>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "app/HistoryController.h"

class TestHistoryController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void dailySeriesPadsMissingDays(); // 缺失日期补零
    void dailySeriesMergesLiveToday(); // 今日优先内存缓存
    void hourlySeriesPadsAndMerges();  // 24 项补零 + 今日实时增量
    void todayHourlyAlways24();

private:
    Database db_;
    QTemporaryDir tempDir_;
    QDate today_;
};

void TestHistoryController::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("CursorFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("CursorFlow"));

    today_ = QDate::currentDate();
    QVERIFY(db_.open(tempDir_.filePath("stats.db")));

    // 昨天：日行 5000m / 1000 键；前天 hour 5 小时行 200m / 30 键
    DailyStats yesterday;
    yesterday.date = today_.addDays(-1);
    yesterday.mouseDistanceM = 5000.0;
    yesterday.keyPressCount = 1000;
    yesterday.activeMinutes = 120;
    QVERIFY(db_.upsertDaily(yesterday));

    HourlyStats hour5;
    hour5.hour = 5;
    hour5.mouseDistanceM = 200.0;
    hour5.keyPressCount = 30;
    QVERIFY(db_.upsertHourly(today_.addDays(-2), hour5));
}

void TestHistoryController::dailySeriesPadsMissingDays()
{
    DataManager dataManager(&db_);
    dataManager.start(tempDir_.filePath("stats.db"));
    HistoryController history(&db_, &dataManager);

    const QVariantList series = history.dailySeries(7);
    QCOMPARE(series.size(), 7);

    // 首项 = 7 天前，末项 = 今天
    const QVariantMap first = series.first().toMap();
    QCOMPARE(first.value("date").toString(), today_.addDays(-6).toString(Qt::ISODate));
    QCOMPARE(first.value("distance").toDouble(), 0.0);
    QCOMPARE(first.value("keys").toInt(), 0);

    // 昨天有数据
    const QVariantMap last2 = series.at(5).toMap();
    QCOMPARE(last2.value("date").toString(), today_.addDays(-1).toString(Qt::ISODate));
    QCOMPARE(last2.value("distance").toDouble(), 5000.0);
    QCOMPARE(last2.value("keys").toInt(), 1000);

    // 升序：日期单调递增
    for (int i = 1; i < series.size(); ++i) {
        const QDate prev = QDate::fromString(series.at(i - 1).toMap().value("date").toString(),
                                             Qt::ISODate);
        const QDate curr = QDate::fromString(series.at(i).toMap().value("date").toString(),
                                             Qt::ISODate);
        QVERIFY(curr > prev);
    }
}

void TestHistoryController::dailySeriesMergesLiveToday()
{
    DataManager dataManager(&db_);
    dataManager.start(tempDir_.filePath("stats.db"));
    HistoryController history(&db_, &dataManager);

    // 未落盘的今日内存缓存应出现在末项（03 号文档 §3.2 覆盖写 + 1h 落盘间隔）
    InputDelta delta;
    delta.distanceMeters = 123.5;
    delta.keyPresses = 42;
    dataManager.applyDelta(delta);

    const QVariantList series = history.dailySeries(7);
    const QVariantMap todayMap = series.last().toMap();
    QCOMPARE(todayMap.value("date").toString(), today_.toString(Qt::ISODate));
    QCOMPARE(todayMap.value("distance").toDouble(), 123.5);
    QCOMPARE(todayMap.value("keys").toInt(), 42);
}

void TestHistoryController::hourlySeriesPadsAndMerges()
{
    DataManager dataManager(&db_);
    dataManager.start(tempDir_.filePath("stats.db"));
    HistoryController history(&db_, &dataManager);

    // 历史日期：固定 24 项，hour 5 有值其余补零
    const QVariantList past = history.hourlySeries(today_.addDays(-2));
    QCOMPARE(past.size(), 24);
    const QVariantMap h5 = past.at(5).toMap();
    QCOMPARE(h5.value("hour").toInt(), 5);
    QCOMPARE(h5.value("distance").toDouble(), 200.0);
    QCOMPARE(h5.value("keys").toInt(), 30);
    QCOMPARE(past.at(4).toMap().value("distance").toDouble(), 0.0);
    QCOMPARE(past.at(6).toMap().value("keys").toInt(), 0);

    // 今日：内存实时增量落在当前小时
    InputDelta delta;
    delta.distanceMeters = 88.0;
    delta.keyPresses = 7;
    dataManager.applyDelta(delta);
    const int currentHour = QTime::currentTime().hour();
    const QVariantList liveToday = history.hourlySeries(today_);
    QCOMPARE(liveToday.size(), 24);
    QCOMPARE(liveToday.at(currentHour).toMap().value("distance").toDouble(), 88.0);
    QCOMPARE(liveToday.at(currentHour).toMap().value("keys").toInt(), 7);
}

void TestHistoryController::todayHourlyAlways24()
{
    DataManager dataManager(&db_);
    dataManager.start(tempDir_.filePath("stats.db"));
    HistoryController history(&db_, &dataManager);

    // 属性路径（Dashboard 热力图数据源）同样恒为 24 项
    const QVariantList hourly = history.todayHourly();
    QCOMPARE(hourly.size(), 24);
    for (int i = 0; i < 24; ++i) {
        QCOMPARE(hourly.at(i).toMap().value("hour").toInt(), i);
    }
}

QTEST_GUILESS_MAIN(TestHistoryController)
#include "tst_HistoryController.moc"
