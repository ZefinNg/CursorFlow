import QtQuick 2.15
import QtQuick.Controls 2.15

import "../components"

// 今日概览（04 号文档 §2.1）：三张 StatCard + 粒子动态区（4.3）
// + 小时热力图（4.2）+ 采集失败横幅（§6）。
Item {
    id: root

    property int pageIndex: 0
    // 今日分布放大态（任务 6.5）：放大时收起统计卡与粒子区，今日分布填满剩余区域
    property bool expanded: false

    Column {
        id: content

        x: 28
        y: 28
        width: parent.width - 56
        height: parent.height - 56
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
            id: errorBanner
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
            height: root.expanded ? 0 : 120
            clip: true
            spacing: 16

            Behavior on height {
                NumberAnimation { duration: 260; easing.type: Easing.InOutCubic }
            }

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
        // 放大今日分布时动画收起（任务 6.5）。
        Rectangle {
            width: parent.width
            height: root.expanded ? 0 : 120
            radius: 12
            color: theme.surface
            border.color: theme.border
            clip: true
            visible: height > 4

            Behavior on height {
                NumberAnimation { duration: 260; easing.type: Easing.InOutCubic }
            }

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

        // 今日 24 小时分布（04 号文档 §2.1 底部热力图；任务 6.5 可放大填满剩余区域）
        Rectangle {
            width: parent.width
            height: root.expanded
                    ? content.height - 26 - 14
                      - (errorBanner.visible ? errorBanner.height + 14 : 0)
                    : 120
            radius: 12
            color: theme.surface
            border.color: theme.border
            clip: true

            Behavior on height {
                NumberAnimation { duration: 260; easing.type: Easing.InOutCubic }
            }

            Label {
                x: 20
                y: 12
                text: qsTr("今日 24 小时分布")
                color: theme.textDim
                font.pixelSize: 12
                font.letterSpacing: 1
            }

            // 放大/缩小按钮（右上角，任务 6.5）
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 8
                width: 26
                height: 26
                radius: 8
                color: expandBtn.containsMouse ? theme.border : "transparent"
                border.color: theme.border

                Label {
                    anchors.centerIn: parent
                    text: root.expanded ? "−" : "＋"
                    color: theme.text
                    font.pixelSize: 14
                }
                MouseArea {
                    id: expandBtn
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.expanded = !root.expanded
                }
            }

            // 缩小态：24 格热力图
            HeatmapGrid {
                x: 20
                y: 40
                width: parent.width - 40
                height: root.expanded ? 0 : 50
                opacity: root.expanded ? 0 : 1
                visible: height > 1
                hours: history.todayHourly

                Behavior on height {
                    NumberAnimation { duration: 220; easing.type: Easing.InOutCubic }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 180 }
                }
            }

            // 放大态：逐小时柱状图
            HourlyBars {
                x: 20
                y: 40
                width: parent.width - 40
                height: root.expanded ? parent.height - 40 - 12 : 0
                opacity: root.expanded ? 1 : 0
                visible: height > 1
                hours: history.todayHourly

                Behavior on height {
                    NumberAnimation { duration: 220; easing.type: Easing.InOutCubic }
                }
                Behavior on opacity {
                    NumberAnimation { duration: 180 }
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
