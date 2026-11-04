#pragma once

#include <QObject>
#include <QString>

#include "app/AppSettings.h"
#include "data/DataManager.h"
#include "data/Database.h"

// 趣味类比引擎（M5 任务 7.1，07 号文档 §2；M6 增补改为数据库随机选条）：
// 将今日距离/按键换算为生活场景类比，并生成"今日工作强度"每日简报（07 号文档 §4）。
// 类比条目存于 analogy_rules 表（首次建库从 qrc analogy.json 导入 200 条）；
// 每次进入概览页时 randomizeAnalogy() 随机选一条，运行中数据增量只重算倍数不换条目。
class FunFactsController : public QObject
{
    Q_OBJECT
    // 仪表盘趣味换算卡片文案（空串 = 无数据/未选条，卡片隐藏）
    Q_PROPERTY(QString todayFunFact READ todayFunFact NOTIFY funFactsChanged)
    // 每日简报文案（空串 = 当日无活动，简报条隐藏）
    Q_PROPERTY(QString todayBrief READ todayBrief NOTIFY funFactsChanged)

public:
    explicit FunFactsController(AppSettings* settings, DataManager* dataManager,
                                Database* database, QObject* parent = nullptr);

    QString todayFunFact() const { return todayFunFact_; }
    QString todayBrief() const { return todayBrief_; }

    // 进入概览页时调用：按当日数据选定类比条目（07 号文档 §2.2 数值区间随机）
    Q_INVOKABLE void randomizeAnalogy();
    Q_INVOKABLE void refresh();

signals:
    void funFactsChanged();

private:
    QString briefFor(const DailyStats& stats) const;
    // 按当前选定条目重算倍数并更新 todayFunFact_
    void applySelection();
    // 选条用的指标类别：距离优先，其次按键；无数据返回空
    static QString pickKind(double distanceM, qint64 keys);
    QString formatCount(qreal x) const;
    static QString formatDistance(double meters);

    AppSettings* settings_;
    DataManager* dataManager_;
    Database* database_;

    QString selectedKind_;   // 当前选定条目类别 distance / keystrokes
    double selectedRef_ = 0.0;
    QString selectedText_;
    bool hasSelection_ = false;

    QString todayFunFact_;
    QString todayBrief_;
};
