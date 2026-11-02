#include <QtTest>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>

#include "app/AppSettings.h"
#include "app/Theme.h"
#include "data/DataManager.h"
#include "data/Database.h"

// 5.3 性能专项（04 号文档 §2.2 / §7）：TrendChart ≤30 点一次 requestPaint
// 全量重绘 < 200ms（PRD §4 指标）。软件后端渲染，无 GPU 依赖，CI 可跑。
class TestChartPerf : public QObject
{
    Q_OBJECT

private slots:
    void trendChartPaintUnder200ms();

private:
    Database db_;
    QTemporaryDir tempDir_;
};

void TestChartPerf::trendChartPaintUnder200ms()
{
    // Theme 依赖链（AppSettings → DataManager → Database），与主程序一致
    QVERIFY(db_.open(tempDir_.filePath("stats.db")));
    DataManager dataManager(&db_);
    dataManager.start(tempDir_.filePath("stats.db"));
    AppSettings appSettings(&dataManager);
    Theme appTheme(&appSettings);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &appTheme);
    engine.rootContext()->setContextProperty(QStringLiteral("stats"), &dataManager);
    engine.rootContext()->setContextProperty(QStringLiteral("settings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("history"), nullptr);

    // 从源码树加载组件（qrc 编译在主程序目标内，测试直读文件系统）
    QQmlComponent component(&engine, QUrl::fromLocalFile(
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../qml/components/TrendChart.qml")));
    if (component.isError()) {
        QSKIP(qPrintable(component.errorString())); // 组件不可用时跳过（不阻塞流水线）
    }

    // 30 点合成数据（30 天 Tab 的最坏情况）
    QVariantList series;
    for (int i = 0; i < 30; ++i) {
        QVariantMap point;
        point.insert(QStringLiteral("date"),
                     QDate(2026, 10, 1).addDays(i).toString(Qt::ISODate));
        point.insert(QStringLiteral("distance"), 1200.0 + i * 45.0);
        point.insert(QStringLiteral("keys"), 3000 + i * 137);
        series.append(point);
    }

    QQuickWindow window;
    QQuickItem* chart = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(chart != nullptr);
    chart->setParentItem(window.contentItem());
    chart->setSize(QSizeF(800, 400));
    chart->setProperty("series", series);
    window.resize(800, 400);
    window.show();

    // 等待首轮 onPaint 完成（lastPaintMs 由组件内 performance.now() 计时）
    QTRY_VERIFY_WITH_TIMEOUT(chart->property("lastPaintMs").toDouble() > 0.0, 5000);

    // 悬停态重绘计时（hoverIndex 触发第二次全量重绘）
    chart->setProperty("hoverIndex", 15);
    QTest::qWait(100);

    const double paintMs = chart->property("lastPaintMs").toDouble();
    qInfo("TrendChart 30 点全量重绘耗时: %.2f ms（阈值 200ms）", paintMs);
    QVERIFY2(paintMs < 200.0, qPrintable(QStringLiteral("渲染超时: %1 ms").arg(paintMs)));

    delete chart;
}

QTEST_MAIN(TestChartPerf)
#include "perf_ChartRender.moc"
