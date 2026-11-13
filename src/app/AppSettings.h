#pragma once

#include <QObject>
#include <QString>

#include "data/DataManager.h"

// 应用设置（04 号文档 §2.3 表格）：QSettings 后端，设置项变更即写。
// 通用组三项（自启/语言/保留天数）+ 显示组三项（主题/字体/动画）
// + 校准组（英寸+分辨率→PPI，任务 4.4，联动采集线程 DPI）。
class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(int retentionDays READ retentionDays WRITE setRetentionDays
                   NOTIFY retentionDaysChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(double fontScale READ fontScale WRITE setFontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled
                   NOTIFY animationsEnabledChanged)
    // 校准（04 号文档 §2.3）：diagonalInch <= 0 表示未校准，UI 标注"估算值"
    Q_PROPERTY(double calibrationDiagonalInch READ calibrationDiagonalInch
                   NOTIFY calibrationChanged)
    Q_PROPERTY(int calibrationPpi READ calibrationPpi NOTIFY calibrationChanged)
    // 首启引导完成标记（任务 6.2，M4）：true 后不再弹出 OnboardingDialog
    Q_PROPERTY(bool onboarded READ onboarded WRITE setOnboarded NOTIFY onboardedChanged)
    // 关闭窗口行为（任务 6.4，M4）：0=每次询问（默认）1=最小化到托盘 2=退出程序
    Q_PROPERTY(int closeAction READ closeAction WRITE setCloseAction NOTIFY closeActionChanged)
    // 悬浮窗今日概览（M6 8.2，PRD 3.9）：最小化/隐藏时显示；位置 -1 表示首次默认
    Q_PROPERTY(bool showFloatingOverlay READ showFloatingOverlay WRITE setShowFloatingOverlay
                   NOTIFY showFloatingOverlayChanged)
    Q_PROPERTY(int overlayX READ overlayX WRITE setOverlayX NOTIFY overlayPosChanged)
    Q_PROPERTY(int overlayY READ overlayY WRITE setOverlayY NOTIFY overlayPosChanged)
    // M7 GIF 动效（PRD 3.10.4，08 号文档 §5）
    Q_PROPERTY(bool gifEnabled READ gifEnabled WRITE setGifEnabled NOTIFY gifConfigChanged)
    Q_PROPERTY(QString gifSource READ gifSource WRITE setGifSource NOTIFY gifConfigChanged)
    Q_PROPERTY(int gifMapMode READ gifMapMode WRITE setGifMapMode NOTIFY gifConfigChanged)
    Q_PROPERTY(int gifCpmMin READ gifCpmMin WRITE setGifCpmMin NOTIFY gifConfigChanged)
    Q_PROPERTY(int gifCpmMax READ gifCpmMax WRITE setGifCpmMax NOTIFY gifConfigChanged)
    Q_PROPERTY(double gifSpeedMin READ gifSpeedMin WRITE setGifSpeedMin NOTIFY gifConfigChanged)
    Q_PROPERTY(double gifSpeedMax READ gifSpeedMax WRITE setGifSpeedMax NOTIFY gifConfigChanged)
    Q_PROPERTY(int gifIdleTimeoutSec READ gifIdleTimeoutSec WRITE setGifIdleTimeoutSec
                   NOTIFY gifConfigChanged)
    Q_PROPERTY(double gifSmoothFactor READ gifSmoothFactor WRITE setGifSmoothFactor
                   NOTIFY gifConfigChanged)
    // M7：实时输出（只读，驱动 QML 动态绑定）
    Q_PROPERTY(double gifCurrentSpeed READ gifCurrentSpeed NOTIFY gifStateChanged)
    Q_PROPERTY(double gifCurrentCpm READ gifCurrentCpm NOTIFY gifStateChanged)
    Q_PROPERTY(bool gifIsIdle READ gifIsIdle NOTIFY gifStateChanged)
    // 软件信息（任务 6.1，M4）：编译期注入，运行期不变（CONSTANT）
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString buildInfo READ buildInfo CONSTANT)

public:
    // retentionDays 变更联动聚合层（可空，仅测试场景传 nullptr）
    explicit AppSettings(DataManager* dataManager, QObject* parent = nullptr);

    bool autostart() const { return autostart_; }
    void setAutostart(bool enabled);

    QString language() const { return language_; }
    void setLanguage(const QString& language);

    int retentionDays() const { return retentionDays_; }
    void setRetentionDays(int days);

    // 0 深色 / 1 浅色 / 2 跟随系统（04 号文档 §4）
    int themeMode() const { return themeMode_; }
    void setThemeMode(int mode);

    double fontScale() const { return fontScale_; }
    void setFontScale(double scale);

    bool animationsEnabled() const { return animationsEnabled_; }
    void setAnimationsEnabled(bool enabled);

    // 系统级深浅色偏好（Qt 5.15 无公开 API，Windows 读注册表，其余平台默认深色）
    Q_INVOKABLE bool systemPrefersDark() const;

    // 校准（02 号文档 §4.1）：由英寸+分辨率计算 PPI 并持久化（calibration/diagonalInch、
    // calibration/ppi），CHANGE 信号联动主程序更新采集线程 DPI。任一参数无效时重置为未校准。
    Q_INVOKABLE void setCalibration(double diagonalInch, int resolutionWidth,
                                    int resolutionHeight);

    double calibrationDiagonalInch() const { return calibrationDiagonalInch_; }
    int calibrationPpi() const { return calibrationPpi_; }

    bool onboarded() const { return onboarded_; }
    void setOnboarded(bool done);

    int closeAction() const { return closeAction_; }
    void setCloseAction(int action);

    bool showFloatingOverlay() const { return showFloatingOverlay_; }
    void setShowFloatingOverlay(bool show);
    int overlayX() const { return overlayX_; }
    void setOverlayX(int x);
    int overlayY() const { return overlayY_; }
    void setOverlayY(int y);

    // M7 GIF 动效 getter/setter
    bool gifEnabled() const { return gifEnabled_; }
    void setGifEnabled(bool enabled);

    QString gifSource() const { return gifSource_; }
    void setGifSource(const QString& source);

    int gifMapMode() const { return gifMapMode_; }
    void setGifMapMode(int mode);

    int gifCpmMin() const { return gifCpmMin_; }
    void setGifCpmMin(int val);

    int gifCpmMax() const { return gifCpmMax_; }
    void setGifCpmMax(int val);

    double gifSpeedMin() const { return gifSpeedMin_; }
    void setGifSpeedMin(double val);

    double gifSpeedMax() const { return gifSpeedMax_; }
    void setGifSpeedMax(double val);

    int gifIdleTimeoutSec() const { return gifIdleTimeoutSec_; }
    void setGifIdleTimeoutSec(int sec);

    double gifSmoothFactor() const { return gifSmoothFactor_; }
    void setGifSmoothFactor(double factor);

    // 只读实时输出（M7，驱动 QML 绑定）
    double gifCurrentSpeed() const { return gifCurrentSpeed_; }
    double gifCurrentCpm() const { return gifCurrentCpm_; }
    bool gifIsIdle() const { return gifIsIdle_; }

    // M7：接收采集线程推送的 CPM（08 号文档 §4）
    Q_INVOKABLE void onKeySpeedUpdated(double cpm);

    // 映射函数（供测试用）
    Q_INVOKABLE double mapCpmToSpeed(double cpm) const;

    // 软件信息 getter（M4）
    QString appVersion() const;
    QString qtVersion() const;
    QString buildInfo() const;

signals:
    void autostartChanged(bool enabled);
    void languageChanged(const QString& language);
    void retentionDaysChanged(int days);
    void themeModeChanged(int mode);
    void fontScaleChanged(double scale);
    void animationsEnabledChanged(bool enabled);
    void calibrationChanged();
    void onboardedChanged(bool done);
    void closeActionChanged(int action);
    void showFloatingOverlayChanged(bool show);
    void overlayPosChanged();
    // M7
    void gifConfigChanged();
    void gifStateChanged();

private:
    DataManager* dataManager_;

    bool autostart_ = false;
    QString language_;
    int retentionDays_ = 365;
    int themeMode_ = 0;
    double fontScale_ = 1.0;
    bool animationsEnabled_ = true;
    double calibrationDiagonalInch_ = 0.0;
    int calibrationPpi_ = 96;
    bool onboarded_ = false;
    int closeAction_ = 0;
    bool showFloatingOverlay_ = true;
    int overlayX_ = -1;
    int overlayY_ = -1;

    // M7 GIF 动效配置
    bool gifEnabled_ = false;
    QString gifSource_ = QStringLiteral("qrc:/qml/dog.gif");
    int gifMapMode_ = 0;           // 0=线性 1=分段
    int gifCpmMin_ = 10;
    int gifCpmMax_ = 300;
    double gifSpeedMin_ = 0.5;
    double gifSpeedMax_ = 3.0;
    int gifIdleTimeoutSec_ = 1;
    double gifSmoothFactor_ = 0.15;

    // M7 实时输出
    double gifCurrentSpeed_ = 0.0;  // 0 = 暂停
    double gifCurrentCpm_ = 0.0;
    bool gifIsIdle_ = true;
};
