#include "app/StatsController.h"

StatsController::StatsController(DataManager* dataManager, QObject* parent)
    : QObject(parent)
    , dataManager_(dataManager)
{
    Q_ASSERT(dataManager_);
    connect(dataManager_, &DataManager::todayChanged, this, &StatsController::refresh);
    refresh();
}

void StatsController::refresh()
{
    stats_ = dataManager_->today();
    emit todayStatsChanged();
}

void StatsController::setCapturePaused(bool paused)
{
    if (capturePaused_ == paused) {
        return;
    }
    capturePaused_ = paused;
    emit capturePausedChanged(paused);
}

void StatsController::setCaptureError(const QString& error)
{
    if (captureError_ == error) {
        return;
    }
    captureError_ = error;
    emit captureErrorChanged(error);
}
