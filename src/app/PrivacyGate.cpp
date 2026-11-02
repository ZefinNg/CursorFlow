#include "app/PrivacyGate.h"

#include <QSettings>

namespace {

const char kKeyAccepted[] = "privacy/accepted";

} // namespace

PrivacyGate::PrivacyGate(QObject* parent)
    : QObject(parent)
{
    loadAccepted();
}

void PrivacyGate::accept()
{
    if (accepted_) {
        return;
    }
    accepted_ = true;

    QSettings settings;
    settings.setValue(QLatin1String(kKeyAccepted), true);
    settings.sync();

    emit acceptedChanged(true);
}

void PrivacyGate::decline()
{
    // 暂不使用：不写任何状态，下次启动再次询问（04 号文档 §2）
}

void PrivacyGate::loadAccepted()
{
    QSettings settings;
    accepted_ = settings.value(QLatin1String(kKeyAccepted), false).toBool();
}
