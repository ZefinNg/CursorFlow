#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "data/DataManager.h"
#include "data/Database.h"

// 成就系统（M5 任务 7.3，07 号文档 §3）：累计/单日阈值达成 → 幂等落库（INSERT OR
// IGNORE）→ unlocked 信号驱动 QML 弹窗。成就定义（阈值/文案）在代码内维护，
// 解锁记录存 achievements 表（03 号文档 §2 增补）。
class AchievementController : public QObject
{
    Q_OBJECT
    // 全部成就（含未达成），供成就列表弹窗：
    // {id, title, description, tier, unlocked, unlockedAt}
    Q_PROPERTY(QVariantList achievementList READ achievementList NOTIFY achievementsChanged)

public:
    explicit AchievementController(DataManager* dataManager, Database* database,
                                   QObject* parent = nullptr);

    QVariantList achievementList() const { return list_; }

    Q_INVOKABLE void refresh();

    // 指标与成就定义对外可见：定义表在 .cpp 匿名命名空间内维护（07 号文档 §3.1）
    enum Metric {
        TotalDistanceM, // 累计距离（米）
        TotalKeys,      // 累计按键
        TodayDistanceM, // 单日距离
        TodayKeys,      // 单日按键
        NightKeys,      // 23:00–5:00 单日按键
        StreakDays      // 连续活跃天数
    };

    struct Def {
        const char* id;
        const char* title;      // tr() 上下文：AchievementController
        const char* description;
        int tier;               // 0 青铜 / 1 白银 / 2 黄金
        Metric metric;
        double threshold;
    };

signals:
    // 仅"新"解锁时发出（每项成就至多一次，07 号文档 §3.2），QML 弹成就卡片
    void unlocked(const QString& id, const QString& title, const QString& description,
                  int tier);
    void achievementsChanged();

private:
    struct Totals {
        double distanceM = 0.0;
        qint64 keys = 0;
    };

    void check();
    Totals totals() const;
    int streakDays() const;
    int nightKeysToday() const;
    QString translate(const char* source) const; // 显式上下文取词（kDefs 为 QT_TRANSLATE_NOOP）

    DataManager* dataManager_;
    Database* database_;
    QVariantList list_;
};
