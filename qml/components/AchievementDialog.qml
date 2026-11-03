import QtQuick 2.15
import QtQuick.Controls 2.15

// 成就达成弹窗（M5 7.3，PRD 3.8.2，07 号文档 §3.2）：
// AchievementController.unlocked 信号（仅首次达成）触发展示，同风格深色卡片。
// "保存图片/复制图片"一键分享为任务 7.4，本任务先提供确认关闭。
Dialog {
    id: dialog

    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: parent
    width: 340
    title: qsTr("成就达成")

    property int tier: 0
    property string achTitle: ""
    property string achDesc: ""

    // 等级色（04 号文档 §2.6：青铜 #C9A86A / 白银 #C0C7D1 / 黄金 #E8B54A）
    readonly property color tierColor: tier === 2 ? "#E8B54A" : tier === 1 ? "#C0C7D1" : "#C9A86A"

    background: Rectangle {
        color: theme.surface
        radius: 10
        border.color: theme.border
    }

    contentItem: Column {
        spacing: 14

        // 徽标圆环（等级色）
        Rectangle {
            width: 72
            height: 72
            radius: 36
            anchors.horizontalCenter: parent.horizontalCenter
            color: "transparent"
            border.color: dialog.tierColor
            border.width: 3

            Rectangle {
                width: 56
                height: 56
                radius: 28
                anchors.centerIn: parent
                color: theme.bg

                Label {
                    anchors.centerIn: parent
                    text: dialog.tier === 2 ? qsTr("黄金")
                        : dialog.tier === 1 ? qsTr("白银") : qsTr("青铜")
                    color: dialog.tierColor
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
            }
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: dialog.achTitle
            color: theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: dialog.achDesc
            color: theme.textDim
            font.pixelSize: 12
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("太棒了")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: dialog.close()
        }
    }

    // 首次达成信号 → 弹窗（07 号文档 §3.2 触发链路）
    Connections {
        target: achievements
        function onUnlocked(id, title, description, tier) {
            dialog.achTitle = title
            dialog.achDesc = description
            dialog.tier = tier
            dialog.open()
        }
    }
}
