#include "app/Theme.h"

Theme::Theme(AppSettings* settings, QObject* parent)
    : QObject(parent)
    , settings_(settings)
{
    Q_ASSERT(settings_);
    connect(settings_, &AppSettings::themeModeChanged, this, &Theme::refresh);
    connect(settings_, &AppSettings::fontScaleChanged, this, &Theme::refresh);
    refresh();
}

void Theme::refresh()
{
    bool dark = true;
    switch (settings_->themeMode()) {
    case 0:
        dark = true;
        break;
    case 1:
        dark = false;
        break;
    default: // 2：跟随系统
        dark = settings_->systemPrefersDark();
        break;
    }

    if (dark) {
        bg_ = QStringLiteral("#0F1117");
        surface_ = QStringLiteral("#1A1D27");
        border_ = QStringLiteral("#2A2E3E");
        text_ = QStringLiteral("#E8EAF2");
        textDim_ = QStringLiteral("#8A90A5");
    } else {
        bg_ = QStringLiteral("#F5F6FA");
        surface_ = QStringLiteral("#FFFFFF");
        border_ = QStringLiteral("#E2E4EC");
        text_ = QStringLiteral("#22242E");
        textDim_ = QStringLiteral("#6B7080");
    }
    isDark_ = dark;
    emit refreshed();
}

QString Theme::fontFamily()
{
#ifdef Q_OS_WIN
    return QStringLiteral("Microsoft YaHei UI");
#elif defined(Q_OS_MAC)
    return QStringLiteral("PingFang SC");
#else
    return QStringLiteral("Noto Sans CJK SC");
#endif
}
