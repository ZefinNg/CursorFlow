#include "app/FunFactsController.h"

#include <QDate>
#include <QRandomGenerator>
#include <cmath>

FunFactsController::FunFactsController(AppSettings* settings, DataManager* dataManager,
                                       Database* database, QObject* parent)
    : QObject(parent)
    , settings_(settings)
    , dataManager_(dataManager)
    , database_(database)
{
    Q_ASSERT(settings_);
    Q_ASSERT(dataManager_);
    Q_ASSERT(database_);

    // 数据落盘 → 重算倍数（不换条目，保持"进入概览时选定"的语义）；
    // 语言切换 → 全部文案重算
    connect(dataManager_, &DataManager::statsSaved, this, &FunFactsController::refresh);
    connect(dataManager_, &DataManager::todayChanged, this, &FunFactsController::refresh);
    connect(settings_, &AppSettings::languageChanged, this, &FunFactsController::refresh);

    refresh();
}

QString FunFactsController::pickKind(double distanceM, qint64 keys)
{
    if (distanceM > 0.0) {
        return QStringLiteral("distance");
    }
    if (keys > 0) {
        return QStringLiteral("keystrokes");
    }
    return QString();
}

void FunFactsController::randomizeAnalogy()
{
    const DailyStats today = dataManager_->today();
    const QString kind = pickKind(today.mouseDistanceM, today.keyPressCount);
    if (kind.isEmpty() || database_ == nullptr) {
        hasSelection_ = false;
        applySelection();
        return;
    }

    const QList<AnalogyRule> rules = database_->analogyRules(kind);
    if (rules.isEmpty()) {
        hasSelection_ = false;
        applySelection();
        return;
    }

    const double raw = kind == QLatin1String("distance")
                           ? today.mouseDistanceM
                           : static_cast<double>(today.keyPressCount);

    // 优先从"倍数落在 1~9999"的候选集中随机（数值直观）；区间为空则全库随机
    QList<int> candidates;
    for (int i = 0; i < rules.size(); ++i) {
        const double n = raw / rules[i].refValue;
        if (n >= 1.0 && n <= 9999.0) {
            candidates.append(i);
        }
    }
    if (candidates.isEmpty()) {
        candidates.reserve(rules.size());
        for (int i = 0; i < rules.size(); ++i) {
            candidates.append(i);
        }
    }

    const int pick = candidates.at(
        static_cast<int>(QRandomGenerator::global()->bounded(
            static_cast<quint32>(candidates.size()))));
    selectedKind_ = kind;
    selectedRef_ = rules.at(pick).refValue;
    selectedText_ = rules.at(pick).text;
    hasSelection_ = true;
    applySelection();
}

void FunFactsController::applySelection()
{
    QString fact;
    if (hasSelection_) {
        const double raw = selectedKind_ == QLatin1String("distance")
                               ? dataManager_->today().mouseDistanceM
                               : static_cast<double>(dataManager_->today().keyPressCount);
        const double n = raw / selectedRef_;
        // 类比条目描述暂仅中文（docs/tech/数据类比示例.md），英文界面回退中文描述
        fact = tr("相当于%1的 %2 倍").arg(selectedText_, formatCount(n));
    }

    if (fact != todayFunFact_) {
        todayFunFact_ = fact;
        emit funFactsChanged();
    }
}

QString FunFactsController::formatCount(qreal x) const
{
    // <100 保留 1 位小数（去尾 .0），≥100 千分位取整（07 号文档 §2.2）
    if (x >= 100.0) {
        const long long rounded = std::llround(x);
        QString s = QString::number(rounded);
        for (int i = s.size() - 3; i > 0; i -= 3) {
            s.insert(i, QLatin1Char(','));
        }
        return s;
    }
    QString s = QString::number(x, 'f', 1);
    if (s.endsWith(QStringLiteral(".0"))) {
        s.chop(2);
    }
    return s;
}

QString FunFactsController::formatDistance(double meters)
{
    if (meters < 1000.0) {
        return QStringLiteral("%1 m").arg(qRound(meters));
    }
    return QStringLiteral("%1 km").arg(meters / 1000.0, 0, 'f', 2);
}

QString FunFactsController::briefFor(const DailyStats& stats) const
{
    if (stats.mouseDistanceM <= 0.0 && stats.keyPressCount <= 0) {
        return QString();
    }

    // 强度分档（07 号文档 §4）：轻松 / 适中 / 爆肝
    int tier;
    if (stats.mouseDistanceM >= 5000.0 || stats.keyPressCount >= 20000) {
        tier = 2;
    } else if (stats.mouseDistanceM < 500.0 && stats.keyPressCount < 500) {
        tier = 0;
    } else {
        tier = 1;
    }

    // 当日确定性：以日期为随机种子，同一自然日内简报不抖动（07 号文档 §4）
    const uint seed = static_cast<uint>(QDate::currentDate().toJulianDay());
    const int index = static_cast<int>(QRandomGenerator(seed).bounded(2));

    const QString dist = formatDistance(stats.mouseDistanceM);
    switch (tier) {
    case 0:
        return index == 0
            ? tr("今天节奏轻松：鼠标游荡了 %1，键盘敲了 %2 次，记得常起来走动。")
                  .arg(dist).arg(stats.keyPressCount)
            : tr("今日手部运动量不大——鼠标路程 %1、敲击 %2 次，适合活动一下手腕。")
                  .arg(dist).arg(stats.keyPressCount);
    case 1:
        return index == 0
            ? tr("今天你的鼠标游荡了 %1，键盘敲击 %2 次，节奏刚刚好。")
                  .arg(dist).arg(stats.keyPressCount)
            : tr("今日工作强度适中：鼠标奔走 %1、键盘 %2 次，继续保持。")
                  .arg(dist).arg(stats.keyPressCount);
    default:
        return index == 0
            ? tr("今天高强度输出：鼠标奔袭 %1、键盘 %2 次，别忘了让双手休息。")
                  .arg(dist).arg(stats.keyPressCount)
            : tr("今日爆肝预警——鼠标 %1、按键 %2 次，起来喝口水休息一下吧。")
                  .arg(dist).arg(stats.keyPressCount);
    }
}

void FunFactsController::refresh()
{
    const DailyStats today = dataManager_->today();
    const QString brief = briefFor(today);

    // 数据指标类别变化（如距离从无到有）→ 重新选条；否则保持条目只重算倍数
    const QString kind = pickKind(today.mouseDistanceM, today.keyPressCount);
    if (kind.isEmpty()) {
        hasSelection_ = false;
        applySelection();
    } else if (!hasSelection_ || kind != selectedKind_) {
        randomizeAnalogy();
    } else {
        applySelection();
    }

    if (brief != todayBrief_) {
        todayBrief_ = brief;
        emit funFactsChanged();
    }
}
