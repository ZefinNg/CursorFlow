import QtQuick 2.15
import QtQuick.Controls 2.15

import "../components"

// 今日概览（04 号文档 §2.1）：三张 StatCard + 粒子动态区（4.3）
// + 小时热力图（4.2）+ 采集失败横幅（§6）。
Item {
    id: root

    property int pageIndex: 0

    Column {
        id: content

        x: 28
        y: 28
        width: parent.width - 56
        spacing: 14

        // 标题 + 暂停标签
        Item {
            width: parent.width
            height: 26

            Label {
                text: qsTr("今日概览")
                color: theme.text
                font.pixelSize: 18 * theme.fontScale
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }

            // 采集暂停标签（04 号文档 §6）
            Rectangle {
                visible: stats.capturePaused
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                width: badgeRow.implicitWidth + 20
                height: 26
                radius: 13
                color: theme.surface
                border.color: theme.border

                Row {
                    id: badgeRow
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#F0B429"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        text: qsTr("已暂停")
                        color: theme.textDim
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // 采集失败横幅（04 号文档 §6：原因 + 引导，如 macOS 辅助功能授权路径）
        Rectangle {
            visible: stats.captureError !== ""
            width: parent.width
            height: Math.max(36, errorLabel.implicitHeight + 20)
            radius: 8
            color: theme.surface
            border.color: "#F0B429"

            Label {
                id: errorLabel
                anchors.centerIn: parent
                width: parent.width - 32
                text: stats.captureError
                color: theme.text
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Row {
            id: cardsRow

            width: parent.width
            spacing: 16

            // 显式三实例（不用 Repeater）：value 绑定随 todayChanged 逐卡片动画，
            // 避免数组 model 重建导致动画重置
            StatCard {
                width: (cardsRow.width - 32) / 3
                height: 120
                label: qsTr("鼠标距离")
                mode: "distance"
                value: stats.todayDistanceM
                accent: theme.primary
            }
            StatCard {
                width: (cardsRow.width - 32) / 3
                height: 120
                label: qsTr("按键总数")
                mode: "count"
                value: stats.todayKeyPresses
                accent: theme.secondary
            }
            StatCard {
                width: (cardsRow.width - 32) / 3
                height: 120
                label: qsTr("活跃时长")
                mode: "minutes"
                value: stats.todayActiveMinutes
                accent: theme.primary
            }
        }

        // 实时动态区：粒子轨迹（04 号文档 §2.1）。动效关闭时不创建实例（§7）。
        Rectangle {
            width: parent.width
            height: 120
            radius: 12
            color: theme.surface
            border.color: theme.border

            Loader {
                anchors.fill: parent
                active: settings.animationsEnabled
                sourceComponent: ParticleTrail {}
            }

            Label {
                visible: !settings.animationsEnabled
                anchors.centerIn: parent
                text: qsTr("动效已关闭")
                color: theme.textDim
                font.pixelSize: 12
            }
        }

        // 今日 24 小时分布（04 号文档 §2.1 底部热力图）
        Rectangle {
            width: parent.width
            height: 120
            radius: 12
            color: theme.surface
            border.color: theme.border

            Column {
                x: 20
                y: 12
                width: parent.width - 40
                spacing: 12

                Label {
                    text: qsTr("今日 24 小时分布")
                    color: theme.textDim
                    font.pixelSize: 12
                    font.letterSpacing: 1
                }

                HeatmapGrid {
                    width: parent.width
                    height: 50
                    hours: history.todayHourly
                }
            }
        }
    }

    // 常驻隐私徽标（06 号文档 §6 风险应对：用户误解键盘记录）
    Label {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("仅统计次数，不记录内容；数据仅保存在本地")
        color: theme.textDim
        font.pixelSize: 11
        opacity: 0.8
    }
}
