#pragma once

#include <QDate>
#include <QString>

class Database;

// 数据导出（03 号文档 §6，PRD 3.4）：
// CSV —— 指定日期区间 daily_stats 原始行，UTF-8 BOM（兼容 Excel 中文）；
// JSON —— 同区间 daily 数组 + 逐日 hourly 数组（QJsonDocument）。
// 纯写出逻辑（供单元测试）；文件路径选择由 ExporterController 承担。
namespace Exporter
{
// 导出 [from, to] 的 daily 行；成功返回 true，失败时填充 *error
bool exportCsv(Database* db, const QDate& from, const QDate& to,
               const QString& filePath, QString* error = nullptr);

// 导出 [from, to] 的 daily + hourly 数据；成功返回 true，失败时填充 *error
bool exportJson(Database* db, const QDate& from, const QDate& to,
                const QString& filePath, QString* error = nullptr);
} // namespace Exporter
