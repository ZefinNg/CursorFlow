#include "export/Exporter.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "data/Database.h"

namespace {

// CSV 表头（中文 + BOM，Excel 直接打开不乱码，03 号文档 §6）。
// 注意：中文不能用 QLatin1String（Latin-1 无汉字），须 QStringLiteral。
const QString kCsvHeader = QStringLiteral("日期,鼠标距离(米),按键次数,左键单击,右键单击,滚轮步数,活跃分钟");

bool writeTextFile(const QString& filePath, const QString& content, bool withBom,
                   QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QObject::tr("无法写入文件：%1").arg(filePath);
        }
        return false;
    }
    if (withBom) {
        file.write("\xEF\xBB\xBF"); // UTF-8 BOM
    }
    file.write(content.toUtf8());
    return true;
}

} // namespace

bool Exporter::exportCsv(Database* db, const QDate& from, const QDate& to,
                         const QString& filePath, QString* error)
{
    if (db == nullptr || !db->isOpen()) {
        if (error) {
            *error = QObject::tr("数据库未打开，导出失败");
        }
        return false;
    }

    QString out;
    out += kCsvHeader;
    out += QLatin1Char('\n');
    for (const DailyStats& row : db->queryDaily(from, to)) {
        out += row.date.toString(Qt::ISODate);
        out += QStringLiteral(",%1,%2,%3,%4,%5,%6\n")
                   .arg(row.mouseDistanceM, 0, 'f', 2)
                   .arg(row.keyPressCount)
                   .arg(row.leftClickCount)
                   .arg(row.rightClickCount)
                   .arg(row.scrollSteps)
                   .arg(row.activeMinutes);
    }
    return writeTextFile(filePath, out, true, error);
}

bool Exporter::exportJson(Database* db, const QDate& from, const QDate& to,
                          const QString& filePath, QString* error)
{
    if (db == nullptr || !db->isOpen()) {
        if (error) {
            *error = QObject::tr("数据库未打开，导出失败");
        }
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("exportedAt"),
                QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("from"), from.toString(Qt::ISODate));
    root.insert(QStringLiteral("to"), to.toString(Qt::ISODate));

    // daily 数组（03 号文档 §6：同区间 daily + hourly 两数组）
    QJsonArray daily;
    QJsonArray hourly;
    for (const DailyStats& row : db->queryDaily(from, to)) {
        QJsonObject day;
        day.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
        day.insert(QStringLiteral("mouseDistanceM"), row.mouseDistanceM);
        day.insert(QStringLiteral("keyPressCount"), row.keyPressCount);
        day.insert(QStringLiteral("leftClickCount"), row.leftClickCount);
        day.insert(QStringLiteral("rightClickCount"), row.rightClickCount);
        day.insert(QStringLiteral("scrollSteps"), row.scrollSteps);
        day.insert(QStringLiteral("activeMinutes"), row.activeMinutes);
        daily.append(day);

        // 逐日 24 小时数组（缺失小时补零，与热力图口径一致）
        QJsonObject dayHourly;
        dayHourly.insert(QStringLiteral("date"), row.date.toString(Qt::ISODate));
        QJsonArray hours;
        for (const HourlyStats& hour : db->queryHourly(row.date)) {
            QJsonObject h;
            h.insert(QStringLiteral("hour"), hour.hour);
            h.insert(QStringLiteral("mouseDistanceM"), hour.mouseDistanceM);
            h.insert(QStringLiteral("keyPressCount"), hour.keyPressCount);
            hours.append(h);
        }
        dayHourly.insert(QStringLiteral("hours"), hours);
        hourly.append(dayHourly);
    }
    root.insert(QStringLiteral("daily"), daily);
    root.insert(QStringLiteral("hourly"), hourly);

    const QJsonDocument doc(root);
    return writeTextFile(filePath,
                         QString::fromUtf8(doc.toJson(QJsonDocument::Indented)), false,
                         error);
}
