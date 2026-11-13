#pragma once

#include <QQuickImageProvider>
#include <QImage>

class GifPlayer;

// M7：GifPlayer → QML Image 的桥接（08 号文档 §9）
//
// Pull 模式：requestImage 调用时从 GifPlayer::currentActive()
// 拉取 currentFrameImage()，完全解耦、避免多帧事件开销。
// GifPlayer 构造时自动注册为活跃实例，析构时恢复上一个
// （支持嵌套/多实例切换）。
class GifImageProvider : public QQuickImageProvider
{
public:
    GifImageProvider();

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;
};
