#include "app/GifImageProvider.h"
#include "app/GifPlayer.h"

// URL 形式：image://gif/<token>?f=<帧序号>
// 查询参数仅用于让 QML 认为 URL 变化从而重新请求帧，这里需要剔除。

GifImageProvider::GifImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage GifImageProvider::requestImage(const QString& id, QSize* size,
                                      const QSize& /*requestedSize*/)
{
    QString token = id;
    const int queryPos = token.indexOf(QLatin1Char('?'));
    if (queryPos >= 0) {
        token.truncate(queryPos);
    }

    GifPlayer* player = GifPlayer::findByToken(token);
    if (!player) {
        if (size) *size = QSize(0, 0);
        return QImage();
    }

    const QImage frame = player->currentFrameImage();
    if (size) {
        *size = frame.size();
    }
    return frame;
}
