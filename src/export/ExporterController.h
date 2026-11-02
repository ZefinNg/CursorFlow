#pragma once

#include <QObject>
#include <QString>

#include <QDate>

class Database;

// 导出流程控制（04 号文档 §2.3 导出组入口，任务 5.2）：
// Q_INVOKABLE 由 QML 调用 → 弹 QFileDialog 选择保存路径 → 调用 Exporter 写出
// → 结果经信号回 QML 状态提示与托盘气泡（03 号文档 §6 "完成后托盘气泡提示"）。
class ExporterController : public QObject
{
    Q_OBJECT

public:
    explicit ExporterController(Database* database, QObject* parent = nullptr);

    // days：导出最近 N 天（含今日）；format："csv" | "json"
    Q_INVOKABLE void exportRange(int days, const QString& format);

signals:
    void exportFinished(const QString& filePath);
    void exportFailed(const QString& message);

private:
    Database* db_;
};
