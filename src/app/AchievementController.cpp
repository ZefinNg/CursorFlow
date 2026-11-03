#include "app/AchievementController.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QDebug>

namespace {

// 首期 12 枚成就（07 号文档 §3.1）；tier：0 青铜 / 1 白银 / 2 黄金。
// 文案用 QT_TRANSLATE_NOOP 显式声明上下文（匿名命名空间内 QT_TR_NOOP 无法
// 绑定类上下文，lupdate 会跳过），运行时经 QCoreApplication::translate 取值。
const AchievementController::Def kDefs[] = {
    {"round_city",   QT_TRANSLATE_NOOP("AchievementController", "绕城一圈"),   QT_TRANSLATE_NOOP("AchievementController", "累计鼠标移动距离达到 10 km"),
     0, AchievementController::TotalDistanceM, 10000.0},
    {"marathon",     QT_TRANSLATE_NOOP("AchievementController", "马拉松选手"), QT_TRANSLATE_NOOP("AchievementController", "累计鼠标移动距离达到 42.195 km"),
     1, AchievementController::TotalDistanceM, 42195.0},
    {"earth_lap",    QT_TRANSLATE_NOOP("AchievementController", "环球旅行家"), QT_TRANSLATE_NOOP("AchievementController", "累计鼠标移动距离达到 40075 km（绕赤道一圈）"),
     2, AchievementController::TotalDistanceM, 40075000.0},
    {"moon_walker",  QT_TRANSLATE_NOOP("AchievementController", "登月宇航员"), QT_TRANSLATE_NOOP("AchievementController", "累计鼠标移动距离达到 38 万 km（地月距离）"),
     2, AchievementController::TotalDistanceM, 380000000.0},
    {"first_1k",     QT_TRANSLATE_NOOP("AchievementController", "千键斩"),     QT_TRANSLATE_NOOP("AchievementController", "累计键盘敲击达到 1000 次"),
     0, AchievementController::TotalKeys, 1000.0},
    {"piano_player", QT_TRANSLATE_NOOP("AchievementController", "键盘钢琴家"), QT_TRANSLATE_NOOP("AchievementController", "累计键盘敲击达到 10 万次"),
     1, AchievementController::TotalKeys, 100000.0},
    {"million_keys", QT_TRANSLATE_NOOP("AchievementController", "百万敲击"),   QT_TRANSLATE_NOOP("AchievementController", "累计键盘敲击达到 100 万次"),
     2, AchievementController::TotalKeys, 1000000.0},
    {"alice",        QT_TRANSLATE_NOOP("AchievementController", "致爱丽丝"),   QT_TRANSLATE_NOOP("AchievementController", "累计键盘敲击达到 3 万次（约《致爱丽丝》100 遍）"),
     1, AchievementController::TotalKeys, 30000.0},
    {"sprint_day",   QT_TRANSLATE_NOOP("AchievementController", "单日万米"),   QT_TRANSLATE_NOOP("AchievementController", "单日鼠标移动距离达到 10 km"),
     1, AchievementController::TodayDistanceM, 10000.0},
    {"typewriter",   QT_TRANSLATE_NOOP("AchievementController", "人肉打字机"), QT_TRANSLATE_NOOP("AchievementController", "单日键盘敲击达到 2 万次"),
     1, AchievementController::TodayKeys, 20000.0},
    {"night_owl",    QT_TRANSLATE_NOOP("AchievementController", "深夜码字人"), QT_TRANSLATE_NOOP("AchievementController", "23:00–5:00 期间单日敲击达到 5000 次"),
     0, AchievementController::NightKeys, 5000.0},
    {"week_streak",  QT_TRANSLATE_NOOP("AchievementController", "七日不辍"),   QT_TRANSLATE_NOOP("AchievementController", "连续 7 天保持记录"),
     2, AchievementController::StreakDays, 7.0},
};

constexpr int kDefCount = sizeof(kDefs) / sizeof(kDefs[0]);

} // namespace

AchievementController::AchievementController(DataManager* dataManager, Database* database,
                                             QObject* parent)
    : QObject(parent)
    , dataManager_(dataManager)
    , database_(database)
{
    Q_ASSERT(dataManager_);
    Q_ASSERT(database_);

    connect(dataManager_, &DataManager::statsSaved, this, &AchievementController::check);
    refresh();
}

AchievementController::Totals AchievementController::totals() const
{
    // 数据量小（保留 365 天），直接全表求和（07 号文档 §1 性能预算内）
    Totals result;
    const auto rows = database_->queryDaily(QDate(2000, 1, 1), QDate::currentDate());
    for (const DailyStats& row : rows) {
        result.distanceM += row.mouseDistanceM;
        result.keys += row.keyPressCount;
    }
    return result;
}

int AchievementController::streakDays() const
{
    const auto rows = database_->queryDaily(QDate::currentDate().addDays(-45),
                                            QDate::currentDate());
    if (rows.isEmpty()) {
        return 0;
    }
    QHash<QDate, bool> active; // date → 有活动
    for (const DailyStats& row : rows) {
        active.insert(row.date, row.mouseDistanceM > 0.0 || row.keyPressCount > 0);
    }
    // 从今天倒推连续活跃天数；今日尚无活动不扣连击（从昨天起算）
    QDate cursor = QDate::currentDate();
    if (!active.value(cursor, false)) {
        cursor = cursor.addDays(-1);
    }
    int streak = 0;
    while (active.value(cursor, false)) {
        ++streak;
        cursor = cursor.addDays(-1);
    }
    return streak;
}

// 显式上下文取词（配合 kDefs 的 QT_TRANSLATE_NOOP）
QString AchievementController::translate(const char* source) const
{
    return QCoreApplication::translate("AchievementController", source);
}

int AchievementController::nightKeysToday() const
{
    const auto hours = database_->queryHourly(QDate::currentDate());
    int sum = 0;
    for (const HourlyStats& hour : hours) {
        // 23:00–5:00（07 号文档 §3.1 night_owl 口径）
        if (hour.hour >= 23 || hour.hour < 5) {
            sum += hour.keyPressCount;
        }
    }
    return sum;
}

void AchievementController::check()
{
    const DailyStats today = dataManager_->today();
    const Totals sum = totals();
    const QVariantMap unlockedMap = database_->achievementUnlocks();

    for (int i = 0; i < kDefCount; ++i) {
        const Def& def = kDefs[i];
        if (unlockedMap.contains(QString::fromLatin1(def.id))) {
            continue; // 已达成，不重复（07 号文档 §3.2 幂等）
        }
        double value = 0.0;
        switch (def.metric) {
        case TotalDistanceM: value = sum.distanceM; break;
        case TotalKeys:      value = static_cast<double>(sum.keys); break;
        case TodayDistanceM: value = today.mouseDistanceM; break;
        case TodayKeys:      value = static_cast<double>(today.keyPressCount); break;
        case NightKeys:      value = nightKeysToday(); break;
        case StreakDays:     value = streakDays(); break;
        }
        if (value < def.threshold) {
            continue;
        }
        if (database_->recordAchievement(QString::fromLatin1(def.id),
                                         QDateTime::currentDateTime().toString(Qt::ISODate))) {
            const QString title = translate(def.title);
            qInfo() << "Achievement unlocked:" << def.id;
            refresh();
            emit unlocked(QString::fromLatin1(def.id), title, translate(def.description),
                          def.tier);
        }
    }
}

void AchievementController::refresh()
{
    const QVariantMap unlockedMap = database_->achievementUnlocks();
    QVariantList list;
    list.reserve(kDefCount);
    for (int i = 0; i < kDefCount; ++i) {
        const Def& def = kDefs[i];
        const QString id = QString::fromLatin1(def.id);
        QVariantMap item;
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("title"), translate(def.title));
        item.insert(QStringLiteral("description"), translate(def.description));
        item.insert(QStringLiteral("tier"), def.tier);
        const bool has = unlockedMap.contains(id);
        item.insert(QStringLiteral("unlocked"), has);
        item.insert(QStringLiteral("unlockedAt"), unlockedMap.value(id).toString());
        list.append(item);
    }
    list_ = list;
    emit achievementsChanged();
}
