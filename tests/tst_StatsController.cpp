#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "app/StatsController.h"
#include "data/DataManager.h"

class TestStatsController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void reflectsInitialToday();
    void updatesOnDelta();
    void pauseFlagNotifies();

private:
    QTemporaryDir tempDir_;
};

void TestStatsController::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("CursorFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("CursorFlow"));
}

void TestStatsController::reflectsInitialToday()
{
    Database database;
    DataManager manager(&database);
    QVERIFY(manager.start(tempDir_.path() + QStringLiteral("/init.db")));

    StatsController controller(&manager);
    QCOMPARE(controller.todayDistanceM(), 0.0);
    QCOMPARE(controller.todayKeyPresses(), 0);
    QCOMPARE(controller.todayActiveMinutes(), 0);
}

void TestStatsController::updatesOnDelta()
{
    Database database;
    DataManager manager(&database);
    QVERIFY(manager.start(tempDir_.path() + QStringLiteral("/delta.db")));
    StatsController controller(&manager);

    QSignalSpy spy(&manager, &DataManager::todayChanged);

    InputDelta delta;
    delta.distanceMeters = 1.5;
    delta.keyPresses = 42;
    delta.leftClicks = 3;
    delta.rightClicks = 1;
    delta.scrollStepsAbs = 7;
    delta.activeSeconds = 90;
    manager.applyDelta(delta);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.todayDistanceM(), 1.5);
    QCOMPARE(controller.todayKeyPresses(), 42);
    QCOMPARE(controller.todayLeftClicks(), 3);
    QCOMPARE(controller.todayRightClicks(), 1);
    QCOMPARE(controller.todayScrollSteps(), 7);
    QCOMPARE(controller.todayActiveMinutes(), 1); // 90s → 1 分钟
}

void TestStatsController::pauseFlagNotifies()
{
    Database database;
    DataManager manager(&database);
    QVERIFY(manager.start(tempDir_.path() + QStringLiteral("/pause.db")));
    StatsController controller(&manager);

    QSignalSpy spy(&controller, &StatsController::capturePausedChanged);
    QVERIFY(!controller.capturePaused());

    controller.setCapturePaused(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toBool());
    QVERIFY(controller.capturePaused());

    controller.setCapturePaused(true); // 幂等：不重复通知
    QCOMPARE(spy.count(), 1);

    controller.setCapturePaused(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.capturePaused());
}

QTEST_GUILESS_MAIN(TestStatsController)
#include "tst_StatsController.moc"
