import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

import "components"
import "pages"

// 主窗口（04 号文档 §2）：TitleBar + Sidebar + StackView 三页面。
// stats / settings / theme 由 C++ setContextProperty 注入（04 号文档 §1）。
ApplicationWindow {
    id: root

    width: 1000
    height: 680
    minimumWidth: 860
    minimumHeight: 560
    visible: true
    title: qsTr("CursorFlow")
    color: theme.bg
    font.family: theme.fontFamily
    font.pixelSize: 13

    // 自定义标题栏（任务 6.6，M4；04 号文档 §2.5）：移除系统标题栏，
    // 窗口控制/拖拽/缩放由 TitleBar.qml 与下方边缘热点区实现
    flags: Qt.Window | Qt.FramelessWindowHint

    // 最大化/还原（标题栏按钮与双击共用）
    function toggleMaximized() {
        if (visibility === Window.Maximized) {
            showNormal()
        } else {
            showMaximized()
        }
    }

    readonly property bool resizable: visibility !== Window.Maximized

    // Fusion 控件深色化（04 号文档 §8 深色基底）
    palette {
        window: theme.bg
        windowText: theme.text
        base: theme.surface
        text: theme.text
        button: theme.surface
        buttonText: theme.text
        highlight: theme.primary
        highlightedText: "#FFFFFF"
        toolTipBase: theme.surface
        toolTipText: theme.text
        mid: theme.border
        dark: theme.border
    }

    // 关闭行为（任务 6.4；PRD 3.4）：0 每次询问 / 1 最小化 / 2 退出。
    // QML 全权处理（C++ 不再拦截，避免双重响应）：退出走 Qt.quit() 触发冲刷。
    onClosing: function (close) {
        close.accepted = false
        if (settings.closeAction === 2) {
            Qt.quit()
        } else if (settings.closeAction === 1) {
            root.visible = false
        } else {
            closeAskDialog.open()
        }
    }

    property bool privacyDeclined: false

    function openPage(index) {
        const comps = [dashboardComp, historyComp, achievementsComp, settingsComp]
        if (stack.currentItem && stack.currentItem.pageIndex === index) {
            return
        }
        // 进入概览时随机换一条类比（PRD 3.8.1：每次进入概览随机挑选换算参照）
        if (index === 0) {
            funFacts.randomizeAnalogy()
        }
        stack.replace(comps[index], {}, StackView.Immediate)
    }

    // 顶部标题栏 + 导航/内容区（04 号文档 §2.5）
    Column {
        anchors.fill: parent
        spacing: 0

        TitleBar {
            id: titleBar
            width: parent.width
            window: root
        }

        Row {
            width: parent.width
            height: parent.height - titleBar.height
            spacing: 0

            Sidebar {
                id: sidebar
                height: parent.height
                onNavigate: root.openPage(index)
            }

            StackView {
                id: stack
                width: parent.width - sidebar.width
                height: parent.height
                initialItem: dashboardComp
            }
        }
    }

    // ---- 无边框窗口边缘缩放热点（04 号文档 §2.5）：四边 6px + 四角 12px，
    //      startSystemResize 交由系统处理；最大化时禁用 ----
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        enabled: root.resizable
        cursorShape: Qt.SizeHorCursor
        onPressed: root.startSystemResize(Qt.LeftEdge)
    }
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        enabled: root.resizable
        cursorShape: Qt.SizeHorCursor
        onPressed: root.startSystemResize(Qt.RightEdge)
    }
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 6
        enabled: root.resizable
        cursorShape: Qt.SizeVerCursor
        onPressed: root.startSystemResize(Qt.TopEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 6
        enabled: root.resizable
        cursorShape: Qt.SizeVerCursor
        onPressed: root.startSystemResize(Qt.BottomEdge)
    }
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        width: 14
        height: 14
        enabled: root.resizable
        cursorShape: Qt.SizeFDiagCursor
        onPressed: root.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
    }
    MouseArea {
        anchors.top: parent.top
        anchors.right: parent.right
        width: 14
        height: 14
        enabled: root.resizable
        cursorShape: Qt.SizeBDiagCursor
        onPressed: root.startSystemResize(Qt.TopEdge | Qt.RightEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 14
        height: 14
        enabled: root.resizable
        cursorShape: Qt.SizeBDiagCursor
        onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 14
        height: 14
        enabled: root.resizable
        cursorShape: Qt.SizeFDiagCursor
        onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
    }

    Component {
        id: dashboardComp
        DashboardPage {}
    }
    Component {
        id: historyComp
        HistoryPage {}
    }
    Component {
        id: achievementsComp
        AchievementsPage {}
    }
    Component {
        id: settingsComp
        SettingsPage {}
    }

    // 用户拒绝授权后的提示横幅（04 号文档 §2）
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: bannerLabel.implicitWidth + 32
        height: bannerLabel.implicitHeight + 20
        radius: 8
        color: theme.surface
        border.color: theme.border
        visible: !privacy.accepted && root.privacyDeclined

        Label {
            id: bannerLabel
            anchors.centerIn: parent
            text: qsTr("未启用采集：仅统计次数，不记录内容；数据仅保存在本地")
            color: theme.textDim
            font.pixelSize: 12
        }
    }

    // 首次启动弹出隐私声明；用命令式 open() 避免 Popup 绑定被 close() 断开。
    // 隐私同意后进入首启引导（任务 6.2，M4）；已同意隐私但未引导过的老用户也补一次。
    PrivacyDialog {
        id: privacyDialog

        onAccepted: {
            privacy.accept()
            if (!settings.onboarded) {
                onboardingDialog.open()
            }
        }
        onRejected: root.privacyDeclined = true

        Component.onCompleted: {
            if (!privacy.accepted) {
                open()
            } else if (!settings.onboarded) {
                onboardingDialog.open()
            }
        }
    }

    // 首启引导（04 号文档 §2.4）：欢迎 → 屏幕尺寸设置（可跳过）→ 完成提示
    OnboardingDialog {
        id: onboardingDialog
    }

    // 关闭窗口询问（任务 6.4 视觉优化）：同风格自定义弹窗替代原生 QMessageBox
    CloseAskDialog {
        id: closeAskDialog

        onQuitRequested: Qt.quit()
        onMinimizeRequested: root.visible = false
    }

    // 成就达成弹窗（M5 7.3）：全局唯一实例，AchievementController.unlocked 驱动
    AchievementDialog {
        id: achievementDialog
    }

    // 悬浮窗今日概览（M6 8.1/8.2，PRD 3.9，04 号文档 §2.7）：主窗口最小化或
    // 隐藏到托盘时显示；开关与引导完成状态联动（隐私红线：onboarded 前不显示）。
    // 关键实现约束：Window 不能静态嵌套声明在 ApplicationWindow 内再延迟 show——
    // 主窗口隐藏后静态子窗口无论 transientParent 是否 null 都无法 show（Qt 5.15
    // 实测，overlay_repro1-10）；必须 Component.createObject(null) 动态创建为
    // 完全独立窗口，随后可见性全部命令式管理。
    property var floatingOverlayObj: null

    Component {
        id: floatingOverlayComp
        FloatingOverlay { }
    }

    onVisibleChanged: syncOverlay()
    onVisibilityChanged: syncOverlay()

    function syncOverlay() {
        var want = (root.visibility === Window.Minimized || !root.visible)
                   && settings.showFloatingOverlay && privacy.accepted
        if (want && floatingOverlayObj === null) {
            floatingOverlayObj = floatingOverlayComp.createObject(null)
            // 悬浮窗"打开主窗口"按钮（M6 8.2 增补）：恢复窗口并前置，
            // visibility 变化经 onVisibilityChanged → syncOverlay 自动隐藏悬浮窗
            floatingOverlayObj.showMainRequested.connect(function () {
                root.showNormal()
                root.raise()
                root.requestActivate()
            })
        }
        if (floatingOverlayObj !== null) {
            floatingOverlayObj.visible = want
        }
    }

    Connections {
        target: settings
        // 设置页开关即时生效
        function onShowFloatingOverlayChanged() { syncOverlay() }
    }
}
