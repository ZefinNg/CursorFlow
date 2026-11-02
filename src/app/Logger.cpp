#include "app/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>

#include <cstdio>

namespace Logger {

namespace {

constexpr qint64 kMaxLogBytes = 1024 * 1024; // 单文件 1MB，超出滚动
constexpr int kMaxRollFiles = 2;             // 保留 cursorflow.1/2.log 两份历史

QMutex g_mutex;
QFile* g_file = nullptr;

QString logDir()
{
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    return base + QStringLiteral("/logs");
}

// 简单滚动：log → .1.log → .2.log（最旧删除），避免日志无限增长
void rotateLocked()
{
    const QString dir = logDir();
    const QString current = dir + QStringLiteral("/cursorflow.log");
    if (!QFileInfo::exists(current)
        || QFileInfo(current).size() < kMaxLogBytes) {
        return;
    }
    QFile::remove(dir + QStringLiteral("/cursorflow.2.log"));
    QFile::rename(dir + QStringLiteral("/cursorflow.1.log"),
                  dir + QStringLiteral("/cursorflow.2.log"));
    QFile::rename(current, dir + QStringLiteral("/cursorflow.1.log"));
}

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "UNKNOWN";
}

// Qt 消息统一入口：所有线程串行进入（互斥），格式固定便于 grep 分析
void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString line = QStringLiteral("[%1] [%2]%3 %4\n")
                             .arg(QDateTime::currentDateTime().toString(
                                      QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")),
                                  QString::fromLatin1(levelName(type)),
                                  context.category != nullptr && qstrcmp(context.category, "default") != 0
                                      ? QStringLiteral("[%1]").arg(QString::fromLatin1(context.category))
                                      : QString(),
                                  message);

    QMutexLocker locker(&g_mutex);
    if (g_file != nullptr && g_file->isOpen()) {
        g_file->write(line.toUtf8());
        g_file->flush(); // 每行即刷：崩溃/断电场景不丢尾部日志
    }
    // 同步透传 stderr，开发期控制台可见（GUI 程序发布后无害）。
    // 用本地编码（中文 Windows = GBK）与 Qt Creator 输出窗格一致，避免 UTF-8 乱码；
    // 文件日志仍为 UTF-8（上方 write）。
    const QByteArray localLine = line.toLocal8Bit();
    std::fwrite(localLine.constData(), 1, static_cast<size_t>(localLine.size()), stderr);
    if (type == QtFatalMsg) {
        std::fflush(stderr);
    }
}

} // namespace

void install()
{
    QMutexLocker locker(&g_mutex);
    if (g_file != nullptr) {
        return; // 幂等
    }

    const QString dir = logDir();
    QDir().mkpath(dir);

    auto* file = new QFile(dir + QStringLiteral("/cursorflow.log"));
    rotateLocked();
    if (file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        g_file = file;
        qInstallMessageHandler(messageHandler);
    } else {
        // 打不开日志文件（权限/磁盘问题）时保留默认 stderr 行为
        delete file;
    }
}

void shutdown()
{
    QMutexLocker locker(&g_mutex);
    if (g_file == nullptr) {
        return;
    }
    g_file->write(QStringLiteral("[%1] ================ 应用退出 ================\n")
                      .arg(QDateTime::currentDateTime().toString(
                          QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")))
                      .toUtf8());
    g_file->flush();
    g_file->close();
    delete g_file;
    g_file = nullptr;
    qInstallMessageHandler(nullptr);
}

} // namespace Logger
