#include "app/FunFactsController.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDate>
#include <QRandomGenerator>
#include <cmath>

FunFactsController::FunFactsController(AppSettings* settings, DataManager* dataManager,
                                       QObject* parent)
    : QObject(parent)
    , settings_(settings)
    , dataManager_(dataManager)
{
    Q_ASSERT(settings_);
    Q_ASSERT(dataManager_);

    loadRules();

    // 数据落盘/设置变更 → 重新选条（07 号文档 §2.2 自定义参照物与风格实时生效）
    connect(dataManager_, &DataManager::statsSaved, this, &FunFactsController::refresh);
    connect(dataManager_, &DataManager::todayChanged, this, &FunFactsController::refresh);
    connect(settings_, &AppSettings::funStyleChanged, this, &FunFactsController::refresh);
    connect(settings_, &AppSettings::customRefChanged, this, &FunFactsController::refresh);
    connect(settings_, &AppSettings::languageChanged, this, &FunFactsController::refresh);

    refresh();
}

void FunFactsController::loadRules()
{
    rules_.clear();

    QFile file(QStringLiteral(":/qml/funfacts.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "FunFacts: 规则库加载失败 :/qml/funfacts.json";
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonArray arr = doc.object().value(QStringLiteral("rules")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Rule rule;
        rule.id = o.value(QStringLiteral("id")).toString();
        rule.metric = o.value(QStringLiteral("metric")).toString();
        rule.scope = o.value(QStringLiteral("scope")).toString();
        rule.style = o.value(QStringLiteral("style")).toString();
        rule.refValue = o.value(QStringLiteral("refValue")).toDouble();
        rule.text = o.value(QStringLiteral("text")).toString();
        rule.textEn = o.value(QStringLiteral("textEn")).toString();
        if (!rule.id.isEmpty() && rule.refValue > 0.0) {
            rules_.append(rule);
        }
    }
    qInfo() << "FunFacts: 规则库已加载" << rules_.size() << "条";
}

QString FunFactsController::formatCount(qreal x) const
{
    // <100 保留 1 位小数（去尾 .0），≥100 取整（07 号文档 §2.2 数值适中）
    if (x >= 100.0) {
        return QString::number(qRound(x));
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

QString FunFactsController::pickFunFact(double distanceM, qint64 keys) const
{
    const QString language = settings_->language();
    const bool english = language == QLatin1String("en");
    static const char* kStyles[] = {"sport", "art", "geek"};
    const QString style = QString::fromLatin1(kStyles[qBound(0, settings_->funStyle(), 2)]);

    // 候选结构：metric 匹配 + scope 匹配 + style 匹配（07 号文档 §2.2 过滤链）
    const auto matches = [this, &style](const Rule& rule, const QString& metric,
                                        const QString& scope, bool styleStrict) {
        if (rule.metric != metric) {
            return false;
        }
        if (rule.scope != QLatin1String("any") && rule.scope != scope) {
            return false;
        }
        return !styleStrict || rule.style == style;
    };

    // 两轮选条：先严格当前风格，无在区间候选时回退全部风格（07 号文档 §2.2）
    const auto pickFrom = [&](const QString& metric, double raw, const QString& scope) {
        for (int pass = 0; pass < 2; ++pass) {
            const bool styleStrict = pass == 0;
            const Rule* best = nullptr;
            double bestScore = 1e18;
            for (const Rule& rule : rules_) {
                if (!matches(rule, metric, scope, styleStrict)) {
                    continue;
                }
                const double x = raw / rule.refValue;
                if (x < 1.0 || x > 999.0) {
                    continue;
                }
                // 最具体优先：X 越接近 10 越好（log10 距离度量）
                const double score = std::fabs(std::log10(x) - 1.0);
                if (score < bestScore) {
                    bestScore = score;
                    best = &rule;
                }
            }
            if (best) {
                const QString tpl = english ? best->textEn : best->text;
                return tpl.arg(formatCount(raw / best->refValue));
            }
        }
        return QString();
    };

    // 自定义参照物（M5 7.5）：命中且在区间时优先展示（07 号文档 §2.2）
    const QString refName = settings_->customRefName().trimmed();
    const double refValue = settings_->customRefValue();
    if (!refName.isEmpty() && refValue > 0.0 && distanceM > 0.0) {
        const double x = distanceM / refValue;
        if (x >= 1.0 && x <= 999.0) {
            return english
                ? QStringLiteral("Equal to the length of %1 × \"%2\"")
                      .arg(formatCount(x), refName)
                : QStringLiteral("相当于 %1 个「%2」的长度").arg(formatCount(x), refName);
        }
    }

    if (distanceM > 0.0) {
        const QString fact = pickFrom(QStringLiteral("distance"), distanceM,
                                      QStringLiteral("today"));
        if (!fact.isEmpty()) {
            return fact;
        }
    }
    if (keys > 0) {
        return pickFrom(QStringLiteral("keys"), static_cast<double>(keys),
                        QStringLiteral("today"));
    }
    return QString();
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
    const QString fact = pickFunFact(today.mouseDistanceM, today.keyPressCount);
    const QString brief = briefFor(today);

    if (fact != todayFunFact_ || brief != todayBrief_) {
        todayFunFact_ = fact;
        todayBrief_ = brief;
        emit funFactsChanged();
    }
}
