import QtQuick 2.15
import QtQuick.Controls 2.15

// 关闭窗口询问（任务 6.4；视觉优化：替换原生 QMessageBox）：
// 与主界面同风格的自定义深色弹窗，三选一 + 记住选择。
// 退出动作由 Main.qml 的信号处理（Qt.quit() 触发 C++ aboutToQuit 冲刷）。
Dialog {
    id: dialog

    modal: true
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: parent
    width: 420
    title: qsTr("关闭 CursorFlow")

    // Main.qml 接线：quitRequested → Qt.quit()；minimizeRequested → root.hide()
    signal quitRequested()
    signal minimizeRequested()

    function applyRemember(action) {
        if (rememberBox.checked) {
            settings.closeAction = action
        }
    }

    background: Rectangle {
        color: theme.surface
        radius: 10
        border.color: theme.border
    }

    contentItem: Column {
        spacing: 16

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("要退出程序还是最小化到系统托盘？退出将停止统计，最小化后仍会在后台记录。")
            color: theme.text
            font.pixelSize: 13
        }

        CheckBox {
            id: rememberBox
            text: qsTr("记住我的选择，不再询问")
            font.pixelSize: 12
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("退出程序")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: {
                dialog.applyRemember(2)
                dialog.quitRequested()
            }
        }
        Button {
            text: qsTr("最小化窗口")
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: {
                dialog.applyRemember(1)
                dialog.close() // 先关弹窗再隐藏窗口，否则重新打开主窗口时弹窗仍在
                dialog.minimizeRequested()
            }
        }
        Button {
            flat: true
            text: qsTr("取消")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: dialog.close()
        }
    }
}
