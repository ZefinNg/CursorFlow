#include <QtTest>

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "app/AppSettings.h"

class TestAppSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultsOnFirstStart();
    void settersPersistAndNotify();
    void calibration();       // 4.4：英寸+分辨率→PPI，持久化与无效输入重置
    void autostartRegistry(); // 放最后：Windows 会写真实 HKCU Run 键，测试末尾恢复

private:
    QTemporaryDir tempDir_;
};

void TestAppSettings::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("CursorFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("CursorFlow"));

    // 配置写入临时目录，避免污染开发机真实设置
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir_.path());
}

void TestAppSettings::defaultsOnFirstStart()
{
    AppSettings settings(nullptr);
    QVERIFY(!settings.autostart());
    QCOMPARE(settings.language(), QStringLiteral("system"));
    QCOMPARE(settings.retentionDays(), 365);
    QCOMPARE(settings.themeMode(), 0);
    QCOMPARE(settings.fontScale(), 1.0);
    QVERIFY(settings.animationsEnabled());
}

void TestAppSettings::settersPersistAndNotify()
{
    QSettings().clear();
    AppSettings settings(nullptr);
    QSignalSpy themeSpy(&settings, &AppSettings::themeModeChanged);
    QSignalSpy scaleSpy(&settings, &AppSettings::fontScaleChanged);
    QSignalSpy retentionSpy(&settings, &AppSettings::retentionDaysChanged);

    settings.setThemeMode(2);
    settings.setFontScale(1.2);
    settings.setRetentionDays(90);
    settings.setLanguage(QStringLiteral("en"));
    settings.setAnimationsEnabled(false);

    QCOMPARE(themeSpy.count(), 1);
    QCOMPARE(scaleSpy.count(), 1);
    QCOMPARE(retentionSpy.count(), 1);

    // 新实例恢复（模拟重启）
    AppSettings restored(nullptr);
    QCOMPARE(restored.themeMode(), 2);
    QCOMPARE(restored.fontScale(), 1.2);
    QCOMPARE(restored.retentionDays(), 90);
    QCOMPARE(restored.language(), QStringLiteral("en"));
    QVERIFY(!restored.animationsEnabled());

    // 写入幂等：同值不重复通知
    settings.setThemeMode(2);
    QCOMPARE(themeSpy.count(), 1);
}

void TestAppSettings::calibration()
{
    QSettings().clear();
    AppSettings settings(nullptr);
    QSignalSpy spy(&settings, &AppSettings::calibrationChanged);

    // 未校准默认 96 DPI 估算（06 号文档 §6）
    QCOMPARE(settings.calibrationDiagonalInch(), 0.0);
    QCOMPARE(settings.calibrationPpi(), 96);

    // 24 英寸 1920×1080：√(1920²+1080²)=2202.9px / 24 ≈ 91.8 → 92
    settings.setCalibration(24.0, 1920, 1080);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(settings.calibrationDiagonalInch(), 24.0);
    QCOMPARE(settings.calibrationPpi(), 92);

    // 新实例恢复（模拟重启）
    AppSettings restored(nullptr);
    QCOMPARE(restored.calibrationDiagonalInch(), 24.0);
    QCOMPARE(restored.calibrationPpi(), 92);

    // 无效输入 → 重置为未校准 96 估算
    settings.setCalibration(0.0, 1920, 1080);
    QCOMPARE(settings.calibrationDiagonalInch(), 0.0);
    QCOMPARE(settings.calibrationPpi(), 96);

    // 同值幂等：不重复通知
    settings.setCalibration(24.0, 1920, 1080);
    settings.setCalibration(24.0, 1920, 1080);
    QCOMPARE(spy.count(), 3);
}

void TestAppSettings::autostartRegistry()
{
    AppSettings settings(nullptr);
    QSignalSpy spy(&settings, &AppSettings::autostartChanged);

    settings.setAutostart(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(settings.autostart());

#ifdef Q_OS_WIN
    QSettings run(QStringLiteral(
                      "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    QVERIFY(run.contains(QStringLiteral("CursorFlow")));
#endif

    settings.setAutostart(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!settings.autostart());

#ifdef Q_OS_WIN
    QVERIFY(!run.contains(QStringLiteral("CursorFlow")));
#endif
}

QTEST_GUILESS_MAIN(TestAppSettings)
#include "tst_AppSettings.moc"
