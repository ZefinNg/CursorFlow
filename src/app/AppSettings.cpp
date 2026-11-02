#include "app/AppSettings.h"

#include <QCoreApplication>
#include <QSettings>

#include <cmath>

namespace {

const QLatin1String kAutostart("general/autostart");
const QLatin1String kLanguage("general/language");
const QLatin1String kRetentionDays("general/retentionDays");
const QLatin1String kThemeMode("display/themeMode");
const QLatin1String kFontScale("display/fontScale");
const QLatin1String kAnimations("display/animationsEnabled");
const QLatin1String kCalibrationInch("calibration/diagonalInch");
const QLatin1String kCalibrationPpi("calibration/ppi");

// 自启注册键（04 号文档 §2.3：Win 注册表 Run；macOS/Linux 机制在跨平台任务接入）
void applyPlatformAutostart(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings run(
        QStringLiteral(
            "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    if (enabled) {
        run.setValue(QStringLiteral("CursorFlow"),
                     QStringLiteral("\"%1\"").arg(QCoreApplication::applicationFilePath()));
    } else {
        run.remove(QStringLiteral("CursorFlow"));
    }
#else
    Q_UNUSED(enabled);
#endif
}

} // namespace

AppSettings::AppSettings(DataManager* dataManager, QObject* parent)
    : QObject(parent)
    , dataManager_(dataManager)
{
    QSettings settings;
    autostart_ = settings.value(kAutostart, false).toBool();
    language_ = settings.value(kLanguage, QStringLiteral("system")).toString();
    retentionDays_ = settings.value(kRetentionDays, 365).toInt();
    themeMode_ = settings.value(kThemeMode, 0).toInt();
    fontScale_ = settings.value(kFontScale, 1.0).toDouble();
    animationsEnabled_ = settings.value(kAnimations, true).toBool();
    calibrationDiagonalInch_ = settings.value(kCalibrationInch, 0.0).toDouble();
    calibrationPpi_ = settings.value(kCalibrationPpi, 96).toInt();
}

void AppSettings::setAutostart(bool enabled)
{
    if (autostart_ == enabled) {
        return;
    }
    autostart_ = enabled;
    QSettings settings;
    settings.setValue(kAutostart, enabled);
    applyPlatformAutostart(enabled);
    emit autostartChanged(enabled);
}

void AppSettings::setLanguage(const QString& language)
{
    if (language_ == language) {
        return;
    }
    language_ = language;
    QSettings settings;
    settings.setValue(kLanguage, language);
    emit languageChanged(language);
}

void AppSettings::setRetentionDays(int days)
{
    if (retentionDays_ == days) {
        return;
    }
    retentionDays_ = days;
    QSettings settings;
    settings.setValue(kRetentionDays, days);
    if (dataManager_) {
        dataManager_->setRetentionDays(days);
    }
    emit retentionDaysChanged(days);
}

void AppSettings::setThemeMode(int mode)
{
    if (themeMode_ == mode) {
        return;
    }
    themeMode_ = mode;
    QSettings settings;
    settings.setValue(kThemeMode, mode);
    emit themeModeChanged(mode);
}

void AppSettings::setFontScale(double scale)
{
    if (qFuzzyCompare(fontScale_, scale)) {
        return;
    }
    fontScale_ = scale;
    QSettings settings;
    settings.setValue(kFontScale, scale);
    emit fontScaleChanged(scale);
}

void AppSettings::setAnimationsEnabled(bool enabled)
{
    if (animationsEnabled_ == enabled) {
        return;
    }
    animationsEnabled_ = enabled;
    QSettings settings;
    settings.setValue(kAnimations, enabled);
    emit animationsEnabledChanged(enabled);
}

void AppSettings::setCalibration(double diagonalInch, int resolutionWidth,
                                 int resolutionHeight)
{
    double inch = diagonalInch;
    int ppi = 96;
    if (diagonalInch > 0.0 && resolutionWidth > 0 && resolutionHeight > 0) {
        // PPI = 对角线物理像素 / 对角线英寸（02 号文档 §4.1 距离换算）
        const double diagonalPx = std::sqrt(static_cast<double>(resolutionWidth)
                                                * resolutionWidth
                                            + static_cast<double>(resolutionHeight)
                                                  * resolutionHeight);
        inch = diagonalInch;
        ppi = qRound(diagonalPx / diagonalInch);
        if (ppi <= 0) {
            inch = 0.0;
            ppi = 96;
        }
    } else {
        inch = 0.0; // 任一输入无效 → 重置为未校准（UI 显示"估算值"）
    }

    if (qFuzzyCompare(calibrationDiagonalInch_, inch) && calibrationPpi_ == ppi) {
        return;
    }
    calibrationDiagonalInch_ = inch;
    calibrationPpi_ = ppi;
    QSettings settings;
    settings.setValue(kCalibrationInch, inch);
    settings.setValue(kCalibrationPpi, ppi);
    emit calibrationChanged();
}

bool AppSettings::systemPrefersDark() const
{
#ifdef Q_OS_WIN
    QSettings personalize(
        QStringLiteral(
            "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    const QVariant light = personalize.value(QStringLiteral("AppsUseLightTheme"));
    if (light.isValid()) {
        return !light.toBool();
    }
#endif
    return true; // 无系统偏好时默认深色（PRD 默认深色主题）
}
