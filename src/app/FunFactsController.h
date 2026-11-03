#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "app/AppSettings.h"
#include "data/DataManager.h"

// 趣味类比引擎（M5 任务 7.1，07 号文档 §2）：将今日距离/按键换算为生活场景类比，
// 并生成"今日工作强度"每日简报（07 号文档 §4）。
// 规则库 JSON 随 qrc 内置（assets/funfacts.json，经 qml.qrc 别名 funfacts.json）；
// 选条策略：1 ≤ X ≤ 999 中最接近 10 的一条（07 号文档 §2.2），自定义参照物命中时优先。
class FunFactsController : public QObject
{
    Q_OBJECT
    // 仪表盘趣味换算卡片文案（空串 = 无数据，卡片隐藏）
    Q_PROPERTY(QString todayFunFact READ todayFunFact NOTIFY funFactsChanged)
    // 每日简报文案（空串 = 当日无活动，简报条隐藏）
    Q_PROPERTY(QString todayBrief READ todayBrief NOTIFY funFactsChanged)

public:
    explicit FunFactsController(AppSettings* settings, DataManager* dataManager,
                                QObject* parent = nullptr);

    QString todayFunFact() const { return todayFunFact_; }
    QString todayBrief() const { return todayBrief_; }

    Q_INVOKABLE void refresh();

signals:
    void funFactsChanged();

private:
    struct Rule {
        QString id;
        QString metric;  // distance / keys
        QString scope;   // any / today / total
        QString style;   // sport / art / geek
        double refValue = 1.0; // 距离类单位米、按键类单位次（每 1 个 X 的量值）
        QString text;
        QString textEn;
    };

    void loadRules();
    QString pickFunFact(double distanceM, qint64 keys) const;
    QString briefFor(const DailyStats& stats) const;
    QString formatCount(qreal x) const;
    static QString formatDistance(double meters);

    AppSettings* settings_;
    DataManager* dataManager_;
    QList<Rule> rules_;
    QString todayFunFact_;
    QString todayBrief_;
};
