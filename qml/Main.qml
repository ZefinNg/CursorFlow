import QtQuick 2.15
import QtQuick.Controls 2.15

import "components"
import "pages"

// 主窗口（04 号文档 §2）：Sidebar + StackView 三页面。
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

    // 关闭按钮 → 隐藏到托盘，从托盘菜单退出（PRD 5.2 / 04 号文档 §3）
    onClosing: function (close) {
        close.accepted = false
        root.visible = false
    }

    property bool privacyDeclined: false

    function openPage(index) {
        const comps = [dashboardComp, historyComp, settingsComp]
        if (stack.currentItem && stack.currentItem.pageIndex === index) {
            return
        }
        stack.replace(comps[index], {}, StackView.Immediate)
    }

    Row {
        anchors.fill: parent
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

    Component {
        id: dashboardComp
        DashboardPage {}
    }
    Component {
        id: historyComp
        HistoryPage {}
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
}
