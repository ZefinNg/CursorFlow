import QtQuick 2.15
import QtQuick.Controls 2.15

// 首次启动隐私声明（PRD 5.4）：用户同意前不启动采集。
// 关闭方式仅限底部按钮（NoAutoClose），保证用户明确做出选择。
Dialog {
    id: dialog

    modal: true
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: parent
    width: 420
    title: qsTr("隐私声明")

    background: Rectangle {
        color: theme.surface
        radius: 10
        border.color: theme.border
    }

    Label {
        width: parent.width
        wrapMode: Text.WordWrap
        color: theme.text
        font.pixelSize: 13
        text: qsTr("CursorFlow 仅统计鼠标移动距离与按键、点击、滚动的次数，不记录任何按键内容、屏幕画面或窗口标题。所有数据仅保存在本机数据库，不上传到任何服务器。你可以随时在系统托盘菜单中暂停统计或退出程序。")
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("同意并开始统计")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
        Button {
            text: qsTr("暂不使用")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
