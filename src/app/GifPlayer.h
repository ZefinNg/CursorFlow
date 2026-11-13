#pragma once

#include <QObject>
#include <QImage>
#include <QString>

class QMovie;

// M7：GIF 播放组件（08 号文档 §2）
//
// Qt 5.15 下 AnimatedImage 没有 speed 属性（Qt 6.2+ 才引入），
// 用原生 QMovie 封装实现动态调速：QMovie::setSpeed(int percent)
// 支持 1%~400% 的整数百分比调速，对应 0.01x ~ 4.0x 倍速。
//
// 帧图像通过 GifImageProvider（main.cpp 注册为 "gif" provider）
// 桥接到 QML Image 组件：每个实例有唯一 token，QML 用
// imageUrl + 帧序号作为 URL 强制刷新。
class GifPlayer : public QObject
{
    Q_OBJECT

    // ---- Q_PROPERTY ----
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY sourceChanged)
    Q_PROPERTY(int currentFrame READ currentFrame NOTIFY frameChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY sourceChanged)

    // 0 = 暂停，正数播放倍率（0.1x ~ 4.0x）
    Q_PROPERTY(double currentSpeedPercent READ currentSpeedPercent NOTIFY speedChanged)

    // QML Image.source 前缀："image://gif/<token>"
    Q_PROPERTY(QString imageUrl READ imageUrl CONSTANT)

public:
    explicit GifPlayer(QObject* parent = nullptr);
    ~GifPlayer() override;

    QString source() const { return source_; }
    void setSource(const QString& path);

    double speed() const { return speed_; }
    void setSpeed(double multiplier);

    bool playing() const { return playing_; }
    void setPlaying(bool play);

    int frameCount() const;
    int currentFrame() const;
    bool valid() const { return valid_; }

    // 供外部（GifImageProvider）获取当前帧图像
    QImage currentFrameImage() const;

    double currentSpeedPercent() const;

    QString imageUrl() const;

    // 按 token 查找实例（GifImageProvider 用）
    static GifPlayer* findByToken(const QString& token);

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart();

signals:
    void sourceChanged();
    void speedChanged();
    void playingChanged();
    void frameChanged();

private slots:
    void onMovieFrameChanged(int frameNumber);

private:
    void ensureMovie();
    void applySpeed();
    void applyPlaying();
    void invalidate();

    QMovie* movie_ = nullptr;
    QString token_;            // 实例唯一标识（注册表键）
    QString source_;
    double speed_ = 1.0;       // 倍率：1.0 = 原速，0.1 = 慢 10 倍，4.0 = 快 4 倍
    bool playing_ = false;
    bool valid_ = false;
    // speed <= 0 表示"暂停"语义（来自 AppSettings 的空闲态），
    // 此时保留 speed_ 的最小合法值供恢复，但 movie 处于暂停。
    bool pausedBySpeed_ = false;

    static constexpr double kSpeedMin = 0.1;
    static constexpr double kSpeedMax = 4.0;
};
