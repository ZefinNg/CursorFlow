#include "app/AppSettings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>

#include "version.h"

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
const QLatin1String kOnboarded("general/onboarded");
const QLatin1String kCloseAction("general/closeAction");
const QLatin1String kFloatingOverlay("display/floatingOverlay");
const QLatin1String kOverlayX("overlay/x");
const QLatin1String kOverlayY("overlay/y");

// M7 GIF 动效配置键（PRD 3.10.4，08 号文档 §5.1）
const QLatin1String kGifEnabled("gif/enabled");
const QLatin1String kGifSource("gif/source");
const QLatin1String kGifMapMode("gif/mapMode");
const QLatin1String kGifCpmMin("gif/cpmMin");
const QLatin1String kGifCpmMax("gif/cpmMax");
const QLatin1String kGifSpeedMin("gif/speedMin");
const QLatin1String kGifSpeedMax("gif/speedMax");
const QLatin1String kGifIdleTimeoutSec("gif/idleTimeoutSec");
const QLatin1String kGifSmoothFactor("gif/smoothFactor");

// 自启注册键（04 号文档 §2.3：Win 注册表 Run；macOS/Linux 机制在跨平台任务接入）
void applyPlatformAutostart(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings run(
        QStringLiteral(
            "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat);
    if (enabled) {
        // 路径必须写成反斜杠的本地形式：Run 值由 Explorer 直接作为命令行启动，
        // applicationFilePath() 返回的正斜杠（C:/Program Files/...）在部分系统上
        // 会导致启动项静默失效（任务管理器中仍显示"已启用"）。
        const QString exePath =
            QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        const QString value = QStringLiteral("\"%1\"").arg(exePath);
        if (run.value(QStringLiteral("CursorFlow")).toString() != value) {
            run.setValue(QStringLiteral("CursorFlow"), value);
            run.sync();
        }
        if (run.status() != QSettings::NoError) {
            qWarning() << "自启注册表写入失败 · 键" << run.fileName() << "· 状态"
                       << int(run.status());
        } else {
            qInfo() << "开机自启注册表已就绪 ·" << value;
        }
    } else if (run.contains(QStringLiteral("CursorFlow"))) {
        run.remove(QStringLiteral("CursorFlow"));
        run.sync();
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
    onboarded_ = settings.value(kOnboarded, false).toBool();
    closeAction_ = settings.value(kCloseAction, 0).toInt();
    showFloatingOverlay_ = settings.value(kFloatingOverlay, true).toBool();
    overlayX_ = settings.value(kOverlayX, -1).toInt();
    overlayY_ = settings.value(kOverlayY, -1).toInt();

    // M7 GIF 动效配置初始化（08 号文档 §5.1 默认值）
    gifEnabled_ = settings.value(kGifEnabled, false).toBool();
    gifSource_ = settings.value(kGifSource, QStringLiteral("qrc:/qml/dog.gif")).toString();
    gifMapMode_ = settings.value(kGifMapMode, 0).toInt();
    gifCpmMin_ = settings.value(kGifCpmMin, 10).toInt();
    gifCpmMax_ = settings.value(kGifCpmMax, 300).toInt();
    gifSpeedMin_ = settings.value(kGifSpeedMin, 0.0).toDouble();
    gifSpeedMax_ = settings.value(kGifSpeedMax, 3.0).toDouble();
    gifIdleTimeoutSec_ = settings.value(kGifIdleTimeoutSec, 1).toInt();
    gifSmoothFactor_ = settings.value(kGifSmoothFactor, 0.15).toDouble();

    // 自愈：设置里已开启自启时，每次启动校验并纠正注册表 Run 值
    // （安装路径变更、被清理工具删除、旧版本写入正斜杠路径等都会导致
    // 任务管理器显示"已启用"但开机不启动）。
    applyPlatformAutostart(autostart_);
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

void AppSettings::setOnboarded(bool done)
{
    if (onboarded_ == done) {
        return;
    }
    onboarded_ = done;
    QSettings settings;
    settings.setValue(kOnboarded, done);
    emit onboardedChanged(done);
}

QString AppSettings::appVersion() const
{
    return QCoreApplication::applicationVersion();
}

QString AppSettings::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString AppSettings::buildInfo() const
{
    // 构建日期 + 编译器（version.h 由 CMake configure_file 生成，任务 6.1）
    return QStringLiteral("%1 · %2")
        .arg(QString::fromLatin1(CURSORFLOW_BUILD_DATE),
             QString::fromLatin1(CURSORFLOW_BUILD_COMPILER));
}

void AppSettings::setCloseAction(int action)
{
    // 仅接受合法值（0 每次询问 / 1 最小化 / 2 退出，任务 6.4）
    if (action != 0 && action != 1 && action != 2) {
        action = 0;
    }
    if (closeAction_ == action) {
        return;
    }
    closeAction_ = action;
    QSettings settings;
    settings.setValue(kCloseAction, action);
    emit closeActionChanged(action);
}

void AppSettings::setShowFloatingOverlay(bool show)
{
    if (showFloatingOverlay_ == show) {
        return;
    }
    showFloatingOverlay_ = show;
    QSettings settings;
    settings.setValue(kFloatingOverlay, show);
    emit showFloatingOverlayChanged(show);
}

void AppSettings::setOverlayX(int x)
{
    if (overlayX_ == x) {
        return;
    }
    overlayX_ = x;
    QSettings settings;
    settings.setValue(kOverlayX, x);
    emit overlayPosChanged();
}

void AppSettings::setOverlayY(int y)
{
    if (overlayY_ == y) {
        return;
    }
    overlayY_ = y;
    QSettings settings;
    settings.setValue(kOverlayY, y);
    emit overlayPosChanged();
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

// ============================================================================
// M7 GIF 动效配置 setter（08 号文档 §5）
// ============================================================================

void AppSettings::setGifEnabled(bool enabled)
{
    if (gifEnabled_ == enabled) return;
    gifEnabled_ = enabled;
    QSettings settings;
    settings.setValue(kGifEnabled, enabled);
    emit gifConfigChanged();
}

void AppSettings::setGifSource(const QString& source)
{
    if (gifSource_ == source) return;
    gifSource_ = source;
    QSettings settings;
    settings.setValue(kGifSource, source);
    emit gifConfigChanged();
}

void AppSettings::setGifMapMode(int mode)
{
    if (gifMapMode_ == mode) return;
    gifMapMode_ = mode;
    QSettings settings;
    settings.setValue(kGifMapMode, mode);
    emit gifConfigChanged();
}

void AppSettings::setGifCpmMin(int val)
{
    val = qBound(1, val, gifCpmMax_);
    if (gifCpmMin_ == val) return;
    gifCpmMin_ = val;
    QSettings settings;
    settings.setValue(kGifCpmMin, val);
    emit gifConfigChanged();
}

void AppSettings::setGifCpmMax(int val)
{
    val = qBound(gifCpmMin_, val, 500);
    if (gifCpmMax_ == val) return;
    gifCpmMax_ = val;
    QSettings settings;
    settings.setValue(kGifCpmMax, val);
    emit gifConfigChanged();
}

void AppSettings::setGifSpeedMin(double val)
{
    val = qBound(0.1, val, gifSpeedMax_);
    if (qFuzzyCompare(gifSpeedMin_, val)) return;
    gifSpeedMin_ = val;
    QSettings settings;
    settings.setValue(kGifSpeedMin, val);
    emit gifConfigChanged();
}

void AppSettings::setGifSpeedMax(double val)
{
    val = qBound(gifSpeedMin_, val, 4.0);
    if (qFuzzyCompare(gifSpeedMax_, val)) return;
    gifSpeedMax_ = val;
    QSettings settings;
    settings.setValue(kGifSpeedMax, val);
    emit gifConfigChanged();
}

void AppSettings::setGifIdleTimeoutSec(int sec)
{
    sec = qBound(5, sec, 60);
    if (gifIdleTimeoutSec_ == sec) return;
    gifIdleTimeoutSec_ = sec;
    QSettings settings;
    settings.setValue(kGifIdleTimeoutSec, sec);
    emit gifConfigChanged();
}

void AppSettings::setGifSmoothFactor(double factor)
{
    factor = qBound(0.05, factor, 0.5);
    if (qFuzzyCompare(gifSmoothFactor_, factor)) return;
    gifSmoothFactor_ = factor;
    QSettings settings;
    settings.setValue(kGifSmoothFactor, factor);
    emit gifConfigChanged();
}

// ============================================================================
// M7 速度映射（08 号文档 §4）
// ============================================================================

double AppSettings::mapCpmToSpeed(double cpm) const
{
    // 钳制到 [cpmMin, cpmMax]
    const double lo = static_cast<double>(gifCpmMin_);
    const double hi = static_cast<double>(gifCpmMax_);
    cpm = qBound(lo, cpm, hi);

    const double sLo = gifSpeedMin_;
    const double sHi = gifSpeedMax_;

    if (gifMapMode_ == 0) {
        // 线性映射：speed = sLo + (cpm - lo) / (hi - lo) × (sHi - sLo)
        if (qFuzzyCompare(hi, lo)) return sLo;
        const double t = (cpm - lo) / (hi - lo);
        return sLo + t * (sHi - sLo);
    }

    // 分段映射（4 段，基于 cpmMin~cpmMax 划分）
    // 段边界：25%, 50%, 75% 处
    const double span = hi - lo;
    const double b1 = lo + span * 0.25;
    const double b2 = lo + span * 0.50;
    const double b3 = lo + span * 0.75;

    // 对应倍率区间：[sLo, s1], [s1, s2], [s2, s3], [s3, sHi]
    const double s1 = sLo + (sHi - sLo) * 0.4;
    const double s2 = sLo + (sHi - sLo) * 0.65;
    const double s3 = sLo + (sHi - sLo) * 0.85;

    if (cpm <= b1) {
        if (qFuzzyCompare(b1, lo)) return sLo;
        const double t = (cpm - lo) / (b1 - lo);
        return sLo + t * (s1 - sLo);
    } else if (cpm <= b2) {
        const double t = (cpm - b1) / (b2 - b1);
        return s1 + t * (s2 - s1);
    } else if (cpm <= b3) {
        const double t = (cpm - b2) / (b3 - b2);
        return s2 + t * (s3 - s2);
    } else {
        const double t = (cpm - b3) / (hi - b3);
        return s3 + t * (sHi - s3);
    }
}

// ============================================================================
// M7 实时处理（08 号文档 §4.3）
// ============================================================================

void AppSettings::onKeySpeedUpdated(double cpm)
{
    // 更新原始 CPM
    gifCurrentCpm_ = cpm;

    // 空闲判定
    const bool nowIdle = (cpm < static_cast<double>(gifCpmMin_));
    gifIsIdle_ = nowIdle;

    // GIF 全局关闭时直接输出 0
    if (!gifEnabled_ || !animationsEnabled_) {
        gifCurrentSpeed_ = 0.0;
        emit gifStateChanged();
        return;
    }

    double rawSpeed;
    if (nowIdle) {
        // 空闲行为：以最低倍率持续播放（不敲键盘时动图不完全静止，
        // 而是保持 speedMin 慢速运转，保证"活"的感觉）
        rawSpeed = gifSpeedMin_;
    } else {
        rawSpeed = mapCpmToSpeed(cpm);
    }

    // EMA 平滑（08 号文档 §4.3）
    // 进入空闲时直接赋值（立即落到最低倍率），打字中变化才平滑；
    // 从空闲恢复时由 minSpeed 经 EMA 平滑加速，视觉过渡自然
    const double alpha = gifSmoothFactor_;
    if (nowIdle || gifCurrentSpeed_ == 0.0) {
        gifCurrentSpeed_ = rawSpeed;
    } else {
        gifCurrentSpeed_ = alpha * rawSpeed + (1.0 - alpha) * gifCurrentSpeed_;
    }

    emit gifStateChanged();
}
