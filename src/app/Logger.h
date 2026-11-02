#pragma once

// 本地文件日志（M4 增补 6.3）：安装后接管 Qt 消息系统，
// qDebug/qInfo/qWarning/qCritical 统一落盘，用于问题回溯与分析。
// 日志仅含运行状态与错误信息，不含用户输入内容（与 PRD 隐私承诺一致）。
namespace Logger {

// 幂等安装：确定日志目录、滚动旧文件、以追加模式常驻打开当前日志。
// 须在 QApplication 构造并设置组织名/应用名之后调用（路径依赖 AppLocalDataLocation）。
void install();

// 退出前调用：写入结束分隔线并关闭文件句柄。
void shutdown();

} // namespace Logger
