#include "export/ExporterController.h"

#include <QDateTime>
#include <QFileDialog>

#include "data/Database.h"
#include "export/Exporter.h"

ExporterController::ExporterController(Database* database, QObject* parent)
    : QObject(parent)
    , db_(database)
{
}

void ExporterController::exportRange(int days, const QString& format)
{
    const QDate today = QDate::currentDate();
    const QDate from = today.addDays(-qMax(1, days) + 1);

    const bool csv = format == QLatin1String("csv");
    // 默认文件名含日期区间，便于用户归档
    const QString suffix = csv ? QStringLiteral("csv") : QStringLiteral("json");
    const QString defaultName =
        QStringLiteral("CursorFlow_%1_%2.%3").arg(from.toString(Qt::ISODate),
                                                  today.toString(Qt::ISODate), suffix);
    const QString filter = csv ? QStringLiteral("CSV 文件 (*.csv)") : QStringLiteral("JSON 文件 (*.json)");

    const QString filePath = QFileDialog::getSaveFileName(
        nullptr, QObject::tr("导出统计数据"), defaultName, filter);
    if (filePath.isEmpty()) {
        return; // 用户取消，不提示
    }

    QString error;
    const bool ok = csv ? Exporter::exportCsv(db_, from, today, filePath, &error)
                        : Exporter::exportJson(db_, from, today, filePath, &error);
    if (ok) {
        emit exportFinished(filePath);
    } else {
        emit exportFailed(error.isEmpty() ? QObject::tr("导出失败") : error);
    }
}
