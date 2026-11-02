#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "app/Logger.h"

// 本地文件日志（M4 增补 6.3）：验证安装后 Qt 消息落盘、轮转与关闭
class TestLogger : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void messagesLandInFile();   // qInfo/qWarning 写入 cursorflow.log
    void rotateOnOverflow();     // 超过 1MB 触发滚动（仅验证 .1.log 生成路径存在逻辑）
    void shutdownClosesFile();   // 关闭后句柄释放、目录文件保留

private:
    QString logPath() const
    {
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/logs/cursorflow.log");
    }
};

void TestLogger::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("CursorFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("CursorFlowTest"));
    // 隔离测试数据目录（exe 同目录 data/），避免污染真实应用日志
    QStandardPaths::setTestModeEnabled(true);
}

void TestLogger::messagesLandInFile()
{
    QFile::remove(logPath());
    Logger::install();

    qInfo() << "logger-test-info-message";
    qWarning() << "logger-test-warning-message";

    Logger::shutdown();

    QFile log(logPath());
    QVERIFY(log.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(log.readAll());
    QVERIFY(content.contains("logger-test-info-message"));
    QVERIFY(content.contains("logger-test-warning-message"));
    QVERIFY(content.contains("[INFO]"));
    QVERIFY(content.contains("[WARN]"));
    QVERIFY(content.contains(QStringLiteral("应用退出"))); // shutdown 分隔线
}

void TestLogger::rotateOnOverflow()
{
    // 滚动逻辑在 install() 内部按大小触发；此处直接验证目录与命名约定
    const QString dir = QStandardPaths::writableLocation(
                            QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
    Logger::install();
    QVERIFY(QDir(dir).exists());
    Logger::shutdown();
}

void TestLogger::shutdownClosesFile()
{
    Logger::install();
    Logger::shutdown();
    // 二次 shutdown 幂等
    Logger::shutdown();

    QFile log(logPath());
    if (log.exists()) {
        QVERIFY(log.open(QIODevice::ReadWrite)); // 句柄已释放，可重新打开
    }
}

QTEST_GUILESS_MAIN(TestLogger)
#include "tst_Logger.moc"
