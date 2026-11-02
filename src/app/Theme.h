#pragma once

#include <QObject>
#include <QString>

#include "app/AppSettings.h"

// 主题 token（04 号文档 §4）：按 themeMode + 系统深浅色解析，
// 经 setContextProperty 以 "theme" 注入 QML（04 号文档 §1）。
// primary/secondary 为品牌色，深浅两套共用。
class Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isDark READ isDark NOTIFY refreshed)
    Q_PROPERTY(QString bg READ bg NOTIFY refreshed)
    Q_PROPERTY(QString surface READ surface NOTIFY refreshed)
    Q_PROPERTY(QString border READ border NOTIFY refreshed)
    Q_PROPERTY(QString primary READ primary CONSTANT)
    Q_PROPERTY(QString secondary READ secondary CONSTANT)
    Q_PROPERTY(QString text READ text NOTIFY refreshed)
    Q_PROPERTY(QString textDim READ textDim NOTIFY refreshed)
    Q_PROPERTY(double fontScale READ fontScale NOTIFY refreshed)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)

public:
    explicit Theme(AppSettings* settings, QObject* parent = nullptr);

    Q_INVOKABLE void refresh();

    bool isDark() const { return isDark_; }
    QString bg() const { return bg_; }
    QString surface() const { return surface_; }
    QString border() const { return border_; }
    static QString primary() { return QStringLiteral("#6C7BFF"); }
    static QString secondary() { return QStringLiteral("#3DD6C3"); }
    QString text() const { return text_; }
    QString textDim() const { return textDim_; }
    double fontScale() const { return settings_->fontScale(); }

    // 平台 UI 字体（04 号文档 §4：中文回退系统字体；Inter 内嵌推迟到打包任务 5.4）
    static QString fontFamily();

signals:
    void refreshed();

private:
    AppSettings* settings_;
    bool isDark_ = true;
    QString bg_;
    QString surface_;
    QString border_;
    QString text_;
    QString textDim_;
};
