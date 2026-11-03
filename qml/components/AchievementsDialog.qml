import QtQuick 2.15
import QtQuick.Controls 2.15

// 成就列表弹窗（M5 7.5，PRD 3.8.2，04 号文档 §2.6）：设置页"查看成就"入口，
// 网格展示全部成就；未达成灰度且不显示阈值（避免剧透，07 号文档测试要点）。
Dialog {
    id: dialog

    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: parent
    width: 480
    height: 480
    title: qsTr("我的成就")

    function tierColor(tier) {
        return tier === 2 ? "#E8B54A" : tier === 1 ? "#C0C7D1" : "#C9A86A"
    }

    background: Rectangle {
        color: theme.surface
        radius: 10
        border.color: theme.border
    }

    contentItem: ListView {
        id: listView
        model: achievements.achievementList
        spacing: 10
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollIndicator.vertical: ScrollIndicator {}

        delegate: Rectangle {
            width: listView.width
            height: 56
            radius: 8
            color: theme.bg
            border.color: theme.border
            opacity: modelData.unlocked ? 1.0 : 0.55

            Row {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12

                // 徽标圆点：达成=等级色实心，未达成=灰
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    anchors.verticalCenter: parent.verticalCenter
                    color: modelData.unlocked ? dialog.tierColor(modelData.tier) : theme.surface
                    border.color: modelData.unlocked ? "transparent" : theme.border

                    Label {
                        anchors.centerIn: parent
                        text: "\u2605" // ★ 实心星（达成态）
                        color: theme.bg
                        font.pixelSize: 14
                        visible: modelData.unlocked
                    }
                    Rectangle {
                        visible: !modelData.unlocked
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        radius: 2
                        color: "transparent"
                        border.color: theme.textDim
                        border.width: 1
                    }
                }

                Column {
                    spacing: 3
                    anchors.verticalCenter: parent.verticalCenter

                    Label {
                        text: modelData.title
                        color: modelData.unlocked ? theme.text : theme.textDim
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: modelData.unlocked
                            ? qsTr("已于 %1 达成").arg(String(modelData.unlockedAt).split("T")[0])
                            : qsTr("继续使用以解锁")
                        color: theme.textDim
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            flat: true
            text: qsTr("关闭")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: dialog.close()
        }
    }
}
