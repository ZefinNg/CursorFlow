#include <QtTest>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "data/Database.h"
#include "export/Exporter.h"

// 导出格式验证（03 号文档 §6）：CSV UTF-8 BOM + 表头/数据行；JSON daily + hourly 结构
class TestExporter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void csvContainsBomAndRows();
    void jsonStructure();

private:
    Database db_;
    QTemporaryDir tempDir_;
};

void TestExporter::initTestCase()
{
    QVERIFY(db_.open(tempDir_.filePath("stats.db")));

    DailyStats day;
    day.date = QDate(2026, 10, 29);
    day.mouseDistanceM = 1234.5;
    day.keyPressCount = 6789;
    day.leftClickCount = 100;
    day.rightClickCount = 50;
    day.scrollSteps = 20;
    day.activeMinutes = 300;
    QVERIFY(db_.upsertDaily(day));

    HourlyStats hour;
    hour.hour = 9;
    hour.mouseDistanceM = 100.5;
    hour.keyPressCount = 42;
    QVERIFY(db_.upsertHourly(QDate(2026, 10, 29), hour));
}

void TestExporter::csvContainsBomAndRows()
{
    const QString path = tempDir_.filePath("out.csv");
    QString error;
    QVERIFY(Exporter::exportCsv(&db_, QDate(2026, 10, 23), QDate(2026, 10, 29), path,
                                &error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray raw = file.readAll();

    // UTF-8 BOM 兼容 Excel 中文（03 号文档 §6）
    QVERIFY(raw.startsWith("\xEF\xBB\xBF"));
    const QString text = QString::fromUtf8(raw.mid(3));

    QVERIFY(text.contains(QStringLiteral("日期,鼠标距离(米),按键次数,左键单击,右键单击,滚轮步数,活跃分钟")));
    QVERIFY(text.contains(QStringLiteral("2026-10-29,1234.50,6789,100,50,20,300")));
    // 缺失日期不产生空行（只导出库中已有行）
    QCOMPARE(text.count(QLatin1Char('\n')), 2);
}

void TestExporter::jsonStructure()
{
    const QString path = tempDir_.filePath("out.json");
    QString error;
    QVERIFY(Exporter::exportJson(&db_, QDate(2026, 10, 23), QDate(2026, 10, 29), path,
                                 &error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);

    const QJsonObject root = doc.object();
    QCOMPARE(root.value("from").toString(), QStringLiteral("2026-10-23"));
    QCOMPARE(root.value("to").toString(), QStringLiteral("2026-10-29"));
    QVERIFY(!root.value("exportedAt").toString().isEmpty());

    // daily 数组：一行数据
    const QJsonArray daily = root.value("daily").toArray();
    QCOMPARE(daily.size(), 1);
    const QJsonObject day = daily.at(0).toObject();
    QCOMPARE(day.value("date").toString(), QStringLiteral("2026-10-29"));
    QCOMPARE(day.value("mouseDistanceM").toDouble(), 1234.5);
    QCOMPARE(day.value("keyPressCount").toInt(), 6789);
    QCOMPARE(day.value("activeMinutes").toInt(), 300);

    // hourly 数组：该日 24 小时补零，hour 9 有值
    const QJsonArray hourly = root.value("hourly").toArray();
    QCOMPARE(hourly.size(), 1);
    const QJsonArray hours = hourly.at(0).toObject().value("hours").toArray();
    QCOMPARE(hours.size(), 24);
    QCOMPARE(hours.at(9).toObject().value("hour").toInt(), 9);
    QCOMPARE(hours.at(9).toObject().value("keyPressCount").toInt(), 42);
    QCOMPARE(hours.at(8).toObject().value("mouseDistanceM").toDouble(), 0.0);
}

QTEST_GUILESS_MAIN(TestExporter)
#include "tst_Exporter.moc"
