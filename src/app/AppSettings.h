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
    // 趣味数据（M5 7.5，07 号文档 §5）：类比风格 0=运动系 1=文艺系 2=极客系
    Q_PROPERTY(int funStyle READ funStyle WRITE setFunStyle NOTIFY funStyleChanged)
    // 自定义参照物：名称为空表示禁用；量值单位为米（距离类参照）
    Q_PROPERTY(QString customRefName READ customRefName WRITE setCustomRefName
                   NOTIFY customRefChanged)
    Q_PROPERTY(double customRefValue READ customRefValue WRITE setCustomRefValue
                   NOTIFY customRefChanged)
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

    int funStyle() const { return funStyle_; }
    void setFunStyle(int style);

    QString customRefName() const { return customRefName_; }
    void setCustomRefName(const QString& name);
    double customRefValue() const { return customRefValue_; }
    void setCustomRefValue(double value);

    // 软件信息（任务 6.1，M4）：版本与 project(VERSION)/安装包同源（version.h）
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
    void funStyleChanged(int style);
    void customRefChanged();

private:
    DataManager* dataManager_;

    bool autostart_ = false;
    QString language_;
    int retentionDays_ = 365;
    int themeMode_ = 0;
    double fontScale_ = 1.0;
    bool animationsEnabled_ = true;
    double calibrationDiagonalInch_ = 0.0;
    int calibrationPpi_ = 96; // 未校准默认 96 DPI 估算（06 号文档 §6 风险应对）
    bool onboarded_ = false;
    int closeAction_ = 0; // 0=每次询问（6.4）
    int funStyle_ = 0;    // 0=运动系 1=文艺系 2=极客系（M5 7.5）
    QString customRefName_;
    double customRefValue_ = 0.0;
};
