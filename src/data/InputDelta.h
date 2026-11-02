#pragma once

#include <QMetaType>
#include <QtGlobal>

// 采集线程每 3 秒提交的增量快照（02 号文档 §2）。
// 距离已按 DPI 校准换算为米；滚动为绝对值累计（PRD 3.1）。
struct InputDelta {
    double distanceMeters = 0.0;
    int keyPresses = 0;
    int leftClicks = 0;
    int rightClicks = 0;
    int midClicks = 0;      // daily_stats 无对应列（03 号文档 §2），暂不持久化
    int scrollStepsAbs = 0;
    int activeSeconds = 0;  // 按 5 分钟空闲阈值推断（02 号文档 §4.3）
};

Q_DECLARE_METATYPE(InputDelta) // 跨线程队列信号（02 号文档 §2）
