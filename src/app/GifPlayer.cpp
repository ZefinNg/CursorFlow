#include "app/GifPlayer.h"

#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QMovie>
#include <QPixmap>

namespace {

// token → GifPlayer 实例注册表。
// 用实例地址的十六进制串作为 token，QML Image.source 通过
// "image://gif/<token>?f=<帧>" 访问对应实例当前帧。
QHash<QString, GifPlayer*>& registry()
{
    static QHash<QString, GifPlayer*> s_registry;
    return s_registry;
}

// QML 传进来的是 "qrc:/qml/dog.gif"，而 QFile/QMovie/QImageReader
// 需要的资源路径形式是 ":/qml/dog.gif"（冒号前缀，不是 qrc: 协议），
// 这里做归一化；普通本地文件路径原样返回。
QString toLocalPath(const QString& path)
{
    return path.startsWith(QLatin1String("qrc:"))
               ? QLatin1Char(':') + path.mid(4)
               : path;
}

} // namespace

GifPlayer::GifPlayer(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<QMovie::MovieState>("QMovie::MovieState");
    token_ = QString::number(reinterpret_cast<quintptr>(this), 16);
    registry().insert(token_, this);
}

GifPlayer::~GifPlayer()
{
    registry().remove(token_);

    if (movie_) {
        movie_->stop();
        delete movie_;
        movie_ = nullptr;
    }
}

GifPlayer* GifPlayer::findByToken(const QString& token)
{
    return registry().value(token, nullptr);
}

QString GifPlayer::imageUrl() const
{
    return QStringLiteral("image://gif/") + token_;
}

void GifPlayer::ensureMovie()
{
    if (movie_) {
        return;
    }
    movie_ = new QMovie(this);
    movie_->setCacheMode(QMovie::CacheAll);  // GIF 通常 < 2MB，全缓存可接受
    connect(movie_, &QMovie::frameChanged,
            this, &GifPlayer::onMovieFrameChanged);
}

void GifPlayer::invalidate()
{
    valid_ = false;
    if (movie_) {
        movie_->stop();
        movie_->setFileName(QString());
    }
}

void GifPlayer::setSource(const QString& path)
{
    if (source_ == path) {
        return;
    }
    source_ = path;

    ensureMovie();

    const QString filePath = toLocalPath(path);

    // 大文件保护：> 5MB 拒绝加载
    QFileInfo fi(filePath);
    if (fi.exists() && fi.size() > 5 * 1024 * 1024) {
        qWarning() << "GifPlayer: 文件过大，拒绝加载" << path
                   << "（" << fi.size() << " bytes > 5MB）";
        invalidate();
        emit sourceChanged();
        return;
    }

    const bool oldPlaying = playing_;
    playing_ = false;

    movie_->stop();
    movie_->setFileName(filePath);

    // 注意：QMovie::isValid() 只有在 start() 之后才返回 true，
    // 因此在播放前改用 QImageReader 做一次格式探测。
    QImageReader reader(filePath);
    valid_ = reader.canRead();
    if (!valid_) {
        qWarning() << "GifPlayer: 加载失败" << path
                   << "·" << reader.errorString();
        invalidate();
        emit sourceChanged();
        return;
    }

    applySpeed();

    // 先启动以产出首帧（frameCount/currentPixmap 此时才有效）；
    // 若当前不应播放（未启用播放或速度为空闲态），立刻暂停并停在首帧。
    movie_->start();
    playing_ = oldPlaying;
    if (!playing_ || pausedBySpeed_) {
        movie_->setPaused(true);
        movie_->jumpToFrame(0);
    }

    qInfo() << "GifPlayer: 已加载" << path
            << "·帧数=" << movie_->frameCount()
            << "·帧延迟=" << movie_->nextFrameDelay() << "ms";

    emit sourceChanged();
}

void GifPlayer::setSpeed(double multiplier)
{
    // <= 0 视为"暂停"语义（AppSettings 空闲态输出 0）
    const bool shouldPause = (multiplier <= 0.0);
    const double bounded = qBound(kSpeedMin, multiplier, kSpeedMax);

    if (pausedBySpeed_ == shouldPause && qFuzzyCompare(speed_, bounded)) {
        return;
    }

    pausedBySpeed_ = shouldPause;
    speed_ = bounded;
    applySpeed();
    applyPlaying();
    emit speedChanged();
}

void GifPlayer::applySpeed()
{
    if (!movie_) {
        return;
    }
    // QMovie::setSpeed 接受百分比整数：100 = 原速，200 = 2x
    const int percent = qRound(speed_ * 100.0);
    movie_->setSpeed(qBound(1, percent, 400));
}

void GifPlayer::setPlaying(bool play)
{
    if (playing_ == play) {
        return;
    }
    playing_ = play;
    applyPlaying();
    emit playingChanged();
}

void GifPlayer::applyPlaying()
{
    if (!movie_ || !valid_) {
        return;
    }
    if (playing_ && !pausedBySpeed_) {
        if (movie_->state() != QMovie::Running) {
            movie_->start();
        }
    } else {
        movie_->setPaused(true);
    }
}

void GifPlayer::play()
{
    setPlaying(true);
}

void GifPlayer::pause()
{
    setPlaying(false);
}

void GifPlayer::stop()
{
    if (!movie_) {
        return;
    }
    playing_ = false;
    movie_->stop();
    emit playingChanged();
}

void GifPlayer::restart()
{
    if (!movie_ || !valid_) {
        return;
    }
    movie_->jumpToFrame(0);
    movie_->start();
    playing_ = true;
    applyPlaying();
    emit playingChanged();
}

int GifPlayer::frameCount() const
{
    return movie_ && valid_ ? movie_->frameCount() : 0;
}

int GifPlayer::currentFrame() const
{
    return movie_ ? movie_->currentFrameNumber() : -1;
}

QImage GifPlayer::currentFrameImage() const
{
    if (!movie_ || !valid_) {
        return QImage();
    }
    return movie_->currentPixmap().toImage();
}

double GifPlayer::currentSpeedPercent() const
{
    return pausedBySpeed_ ? 0.0 : speed_;
}

void GifPlayer::onMovieFrameChanged(int /*frameNumber*/)
{
    emit frameChanged();
}
