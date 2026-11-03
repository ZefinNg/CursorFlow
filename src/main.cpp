#include "app/AppSettings.h"
#include "app/AchievementController.h"
#include "app/FunFactsController.h"
#include "app/HistoryController.h"
#include "app/Logger.h"
#include "app/PrivacyGate.h"
#include "app/StatsController.h"
#include "app/Theme.h"
#include "capture/InputCaptureWorker.h"
#include "capture/IPlatformHook.h"
#include "data/DataManager.h"
#include "data/Database.h"
#include "export/ExporterController.h"

#ifdef Q_OS_WIN
#include "capture/platform/WinHook.h"
#elif defined(Q_OS_MACOS)
#include "capture/platform/MacEventTap.h"
#else
#include "capture/platform/X11InputHook.h"
#endif

#include <QGuiApplication>
#include <QIcon>
#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QDateTime>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QPushButton>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLocale>
#include <QPalette>
#include <QQuickStyle>
#include <QScreen>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>

#include "version.h"
#include <QTranslator>

namespace {

// 托盘图标（04 号文档 §3）：统一采用 assets/image 素材（任务 6.4 图标接入），
// 正常态原图；暂停态灰度化区分（保留 alpha，避免出现底色方块）。
QPixmap makeTrayIconPixmap(bool paused)
{
    const QPixmap icon = QIcon(QStringLiteral(":/qml/icons/CursorFlow.png")).pixmap(32, 32);
    if (!paused) {
        return icon;
    }
    QImage img = icon.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb c = img.pixel(x, y);
            const int gray = qGray(c);
            img.setPixel(x, y, qRgba(gray, gray, gray, qAlpha(c)));
        }
    }
    return QPixmap::fromImage(img);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv); // 托盘菜单需要 QtWidgets
    QApplication::setOrganizationName(QStringLiteral("CursorFlow"));
    QApplication::setApplicationName(QStringLiteral("CursorFlow"));
    // 版本号与 project(VERSION)/安装包同源（任务 6.1，M4；version.h 由 CMake 生成）
    QApplication::setApplicationVersion(QString::fromLatin1(CURSORFLOW_VERSION));

    // 本地文件日志（M4 增补 6.3）：此后所有 Qt 消息落盘，供问题回溯
    Logger::install();
    qInfo() << "应用启动 · 版本" << CURSORFLOW_VERSION << "· 构建" << CURSORFLOW_BUILD_DATE
            << CURSORFLOW_BUILD_COMPILER << "· Qt" << qVersion()
            << "· 平台" << QGuiApplication::platformName();

    // 应用图标（M4 增补）：窗口/任务栏统一使用 assets/image 素材
    app.setWindowIcon(QIcon(QStringLiteral(":/qml/icons/CursorFlow.png")));

    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // 深色调色板：QMessageBox / QMenu 等原生弹窗跟随（04 号文档 §8 深色基底）
    QPalette pal;
    const QColor bgColor(0x0F, 0x11, 0x17);
    const QColor surfaceColor(0x1A, 0x1D, 0x27);
    const QColor textColor(0xE8, 0xEA, 0xF2);
    pal.setColor(QPalette::Window, bgColor);
    pal.setColor(QPalette::WindowText, textColor);
    pal.setColor(QPalette::Base, surfaceColor);
    pal.setColor(QPalette::AlternateBase, bgColor);
    pal.setColor(QPalette::Text, textColor);
    pal.setColor(QPalette::Button, surfaceColor);
    pal.setColor(QPalette::ButtonText, textColor);
    pal.setColor(QPalette::Highlight, QColor(0x6C, 0x7B, 0xFF));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::ToolTipBase, surfaceColor);
    pal.setColor(QPalette::ToolTipText, textColor);
    pal.setColor(QPalette::PlaceholderText, QColor(0x8A, 0x90, 0xA5));
    app.setPalette(pal);

    // ---- 数据层（01 号文档 §2）----
    Database database;
    DataManager dataManager(&database);
    dataManager.start();
    qInfo() << "数据管理器已启动 · 数据目录"
            << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    // ---- 设置与 QML 绑定层（04 号文档 §1：注入 stats / settings / theme / history）----
    AppSettings appSettings(&dataManager);
    StatsController statsController(&dataManager);
    Theme appTheme(&appSettings);
    HistoryController historyController(&database, &dataManager);
    ExporterController exporterController(&database);

    // ---- 采集层：专用线程 + 3s 增量队列信号（02 号文档 §2）----
    QThread captureThread;
    auto* captureWorker = new InputCaptureWorker();
    captureWorker->moveToThread(&captureThread);
    QObject::connect(&captureThread, &QThread::finished, captureWorker, &QObject::deleteLater);

    // 跳变剔除阈值 = 2 × 虚拟桌面 maxDim（02 号文档 §4.1）
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect geometry = screen->virtualGeometry();
        captureWorker->setVirtualScreenMaxDim(qMax(geometry.width(), geometry.height()));
    }

    QObject::connect(captureWorker, &InputCaptureWorker::deltaReady, &dataManager,
                     &DataManager::applyDelta, Qt::QueuedConnection);

    // ---- 隐私授权：同意后才开始采集（PRD 5.4）----
    PrivacyGate privacy;

    const auto startCapture = [&appSettings, &statsController, captureWorker] {
#if defined(Q_OS_WIN)
        static WinHook winHook; // 静态存储：worker 内持有裸指针，须跨调用存活
        IPlatformHook* hook = &winHook;
#elif defined(Q_OS_MACOS)
        static MacEventTap macTap;
        IPlatformHook* hook = &macTap;
#else
        static X11InputHook x11Hook;
        IPlatformHook* hook = &x11Hook;
#endif
        // 校准 PPI → 采集 DPI（任务 4.4，02 号文档 §4.1 距离换算）
        const int ppi = appSettings.calibrationPpi();
        QMetaObject::invokeMethod(
            captureWorker,
            [captureWorker, hook, &statsController, ppi] {
                captureWorker->setDpi(ppi);
                if (hook && !captureWorker->start(hook)) {
                    // 采集失败横幅（04 号文档 §6）：macOS 权限引导 / Wayland 降级 / 钩子被拦
                    QString error = hook->lastError();
                    if (error.isEmpty()) {
                        error = QObject::tr("采集启动失败，可能被安全软件拦截，请重启应用或查看文档");
                    }
                    qWarning() << "采集启动失败：" << error; // 问题回溯关键点（6.3）
                    QMetaObject::invokeMethod(
                        &statsController,
                        [&statsController, error] { statsController.setCaptureError(error); },
                        Qt::QueuedConnection);
                } else {
                    qInfo() << "采集已启动 · DPI =" << ppi;
                }
            },
            Qt::QueuedConnection);
    };
    QObject::connect(&privacy, &PrivacyGate::acceptedChanged, &app, startCapture);

    // 校准变更 → 实时更新采集线程 DPI（04 号文档 §2.3 校准组联动）
    QObject::connect(&appSettings, &AppSettings::calibrationChanged, &app,
                     [&appSettings, captureWorker] {
                         const int ppi = appSettings.calibrationPpi();
                         qInfo() << "校准变更 · 采集 DPI 联动更新为" << ppi;
                         QMetaObject::invokeMethod(
                             captureWorker,
                             [captureWorker, ppi] { captureWorker->setDpi(ppi); },
                             Qt::QueuedConnection);
                     });

    // ---- 国际化（04 号文档 §5）：按设置加载翻译；切换语言后重启生效 ----
    QTranslator translator;
    {
        const QString lang = appSettings.language();
        QString qmName;
        if (lang == QLatin1String("zh_CN") || lang == QLatin1String("en")) {
            qmName = QStringLiteral("CursorFlow_") + lang;
        } else {
            // 跟随系统：中文环境用中文，其余用英文（源文案为中文，en.ts 承担英译）
            qmName = QLocale::system().name().startsWith(QLatin1String("zh"))
                ? QStringLiteral("CursorFlow_zh_CN")
                : QStringLiteral("CursorFlow_en");
        }
        if (translator.load(QStringLiteral(":/qml/i18n/") + qmName
                            + QStringLiteral(".qm"))) {
            app.installTranslator(&translator);
            qInfo() << "翻译已加载：" << qmName;
        } else {
            qWarning() << "翻译加载失败：" << qmName << "（回退中文源文案）";
        }
    }

    // ---- QML（engine 须在托盘引用 window 之前加载）----
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("privacy"), &privacy);
    engine.rootContext()->setContextProperty(QStringLiteral("stats"), &statsController);
    engine.rootContext()->setContextProperty(QStringLiteral("settings"), &appSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &appTheme);
    engine.rootContext()->setContextProperty(QStringLiteral("history"), &historyController);
    engine.rootContext()->setContextProperty(QStringLiteral("exporter"), &exporterController);
    // 趣味数据（M5 7.1/7.3，07 号文档）：类比引擎与成就系统
    FunFactsController funFactsController(&appSettings, &dataManager);
    AchievementController achievementController(&dataManager, &database);
    engine.rootContext()->setContextProperty(QStringLiteral("funFacts"), &funFactsController);
    engine.rootContext()->setContextProperty(QStringLiteral("achievements"), &achievementController);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML 主窗口加载失败，应用退出（QML 错误见上方输出）";
        return -1;
    }
    qInfo() << "QML 主窗口加载成功";
    QObject* window = engine.rootObjects().first();

    // 关闭窗口询问由 QML 侧 CloseAskDialog + onClosing 全权处理（任务 6.4；
    // 原生 QMessageBox 与深色主题风格割裂且曾与 QML onClosing 双重响应）。

    captureThread.start();
    if (privacy.accepted()) {
        startCapture();
    }

    // ---- 系统托盘（PRD 5.2；完整版在任务 3.3）----
    QSystemTrayIcon tray(makeTrayIconPixmap(false));
    tray.setToolTip(QStringLiteral("CursorFlow"));

    QMenu trayMenu;
    QObject::connect(trayMenu.addAction(QObject::tr("打开 CursorFlow")), &QAction::triggered,
                     [window] { window->setProperty("visible", true); });

    auto* pauseAction = trayMenu.addAction(QObject::tr("暂停统计"));
    pauseAction->setCheckable(true);
    QObject::connect(
        pauseAction, &QAction::toggled,
        [captureWorker, pauseAction, &statsController, &tray](bool paused) {
            qInfo() << "用户" << (paused ? "暂停" : "恢复") << "统计";
            QMetaObject::invokeMethod(
                captureWorker,
                [captureWorker, paused] { paused ? captureWorker->pause() : captureWorker->resume(); },
                Qt::QueuedConnection);
            statsController.setCapturePaused(paused); // Dashboard "已暂停"标签
            tray.setIcon(makeTrayIconPixmap(paused)); // 托盘灰色图标（04 号文档 §3）
            pauseAction->setText(paused ? QObject::tr("恢复统计") : QObject::tr("暂停统计"));
        });

    trayMenu.addSeparator();

    // 今日摘要只读项（04 号文档 §3）：statsSaved + 分钟级定时刷新
    auto* todayAction = trayMenu.addAction(QString());
    todayAction->setEnabled(false);
    const auto updateTodayItem = [todayAction, &dataManager] {
        const DailyStats stats = dataManager.today();
        const QString dist = stats.mouseDistanceM >= 1000.0
            ? QStringLiteral("%1 km").arg(stats.mouseDistanceM / 1000.0, 0, 'f', 2)
            : QStringLiteral("%1 m").arg(qRound(stats.mouseDistanceM));
        todayAction->setText(QObject::tr("今日：%1 · %2 键").arg(dist).arg(stats.keyPressCount));
    };
    QObject::connect(&dataManager, &DataManager::statsSaved, updateTodayItem);
    QTimer trayRefreshTimer;
    QObject::connect(&trayRefreshTimer, &QTimer::timeout, updateTodayItem);
    trayRefreshTimer.start(60 * 1000);
    updateTodayItem();

    trayMenu.addSeparator();
    QObject::connect(trayMenu.addAction(QObject::tr("退出…")), &QAction::triggered, [&app] {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            nullptr, QObject::tr("退出 CursorFlow"),
            QObject::tr("退出将停止采集统计，确定要退出吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            app.quit();
        }
    });

    tray.setContextMenu(&trayMenu);
    tray.show();

    // 托盘左键单击显示主窗口（任务 6.4；PRD 5.2 托盘交互）
    QObject::connect(
        &tray, &QSystemTrayIcon::activated, window,
        [window](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                // showNormal/raise/requestActivate 均为 QML Window 可调用的 INVOKABLE
                // （activateWindow 不存在，曾触发 "No such method" 警告）
                QMetaObject::invokeMethod(window, "showNormal");
                QMetaObject::invokeMethod(window, "raise");
                QMetaObject::invokeMethod(window, "requestActivate");
            }
        });

    // 导出完成托盘气泡（03 号文档 §6）
    QObject::connect(&exporterController, &ExporterController::exportFinished, &tray,
                     [&tray](const QString& filePath) {
                         qInfo() << "数据导出成功：" << filePath;
                         tray.showMessage(QObject::tr("导出完成"),
                                          QObject::tr("数据已导出到 %1").arg(filePath),
                                          QSystemTrayIcon::Information, 4000);
                     });
    QObject::connect(&exporterController, &ExporterController::exportFailed, &tray,
                     [&tray](const QString& message) {
                         qWarning() << "数据导出失败：" << message;
                         tray.showMessage(QObject::tr("导出失败"), message,
                                          QSystemTrayIcon::Critical, 4000);
                     });

    // ---- 退出前冲刷积压增量并落盘（02 号文档 §5）----
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
        qInfo() << "应用开始退出 · 冲刷积压增量";
        if (captureThread.isRunning()) {
            QMetaObject::invokeMethod(
                captureWorker,
                [captureWorker] {
                    captureWorker->flush(QDateTime::currentMSecsSinceEpoch());
                    captureWorker->stop();
                },
                Qt::BlockingQueuedConnection);
            QCoreApplication::sendPostedEvents(); // 派发冲刷产生的 deltaReady
            captureThread.quit();
            captureThread.wait();
        }
        dataManager.stop();
        qInfo() << "数据已落盘，关闭日志";
        Logger::shutdown();
    });

    return app.exec();
}
