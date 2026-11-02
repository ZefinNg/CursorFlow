#include <QtTest>

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "app/PrivacyGate.h"

class TestPrivacyGate : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void firstStartNotAccepted();
    void acceptPersistsAndNotifies();
    void declineKeepsUnaccepted();

private:
    QTemporaryDir tempDir_;
};

void TestPrivacyGate::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("CursorFlow"));
    QCoreApplication::setApplicationName(QStringLiteral("CursorFlow"));

    // 配置写入临时目录，避免污染开发机真实设置
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir_.path());
}

void TestPrivacyGate::firstStartNotAccepted()
{
    PrivacyGate gate;
    QVERIFY(!gate.accepted());
}

void TestPrivacyGate::acceptPersistsAndNotifies()
{
    PrivacyGate gate;
    QSignalSpy spy(&gate, &PrivacyGate::acceptedChanged);

    gate.accept();
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toBool());
    QVERIFY(gate.accepted());

    gate.accept(); // 重复 accept 不重复通知
    QCOMPARE(spy.count(), 1);

    // 新实例从 QSettings 恢复（模拟重启）
    PrivacyGate restored;
    QVERIFY(restored.accepted());
}

void TestPrivacyGate::declineKeepsUnaccepted()
{
    QSettings().clear(); // 清除前一测试写入的授权
    PrivacyGate gate;
    gate.decline();
    QVERIFY(!gate.accepted());
}

QTEST_GUILESS_MAIN(TestPrivacyGate)
#include "tst_PrivacyGate.moc"
