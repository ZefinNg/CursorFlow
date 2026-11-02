import QtQuick 2.15
import QtQuick.Controls 2.15

import "../components"

// 历史页（04 号文档 §2.2）：趋势折线（7/30 天 Tab、双轴、悬停 tooltip）
// + 今日 24 小时热力图。
// 性能约束（04 号文档 §7）：页面由 StackView 按需创建，首次进入才请求数据（懒加载）。
Item {
    id: root

    property int pageIndex: 1
    property int days: 7
    property var series: []
    // 今日分布放大态（任务 6.5）：放大后趋势图收起，柱状图填满剩余区域
    property bool expanded: false

    function reload() {
        series = history.dailySeries(root.days)
    }

    Component.onCompleted: reload()

    // 今日缓存每 3s 变化；仅页面可见时重算，避免后台浪费
    Connections {
        target: history
        function onDataChanged() { if (root.visible) root.reload() }
    }
    onVisibleChanged: if (visible) root.reload()

    Column {
        id: content

        x: 28
        y: 28
        width: parent.width - 56
        height: parent.height - 56
        spacing: 16

        // 标题 + 7/30 天 Tab
        Item {
            width: parent.width
            height: 34

            Label {
                text: qsTr("历史趋势")
                color: theme.text
                font.pixelSize: 18 * theme.fontScale
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }

            TabBar {
                id: rangeBar

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                background: Rectangle { color: "transparent" }

                onCurrentIndexChanged: {
                    root.days = currentIndex === 0 ? 7 : 30
                    root.reload()
                }

                TabButton {
                    width: implicitWidth + 24
                    text: qsTr("近 7 天")

                    background: Rectangle {
                        radius: 8
                        color: parent.checked ? theme.surface : "transparent"
                        border.color: parent.checked ? theme.border : "transparent"
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 12
                        color: parent.checked ? theme.text : theme.textDim
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                TabButton {
                    width: implicitWidth + 24
                    text: qsTr("近 30 天")

                    background: Rectangle {
                        radius: 8
                        color: parent.checked ? theme.surface : "transparent"
                        border.color: parent.checked ? theme.border : "transparent"
                    }
                    contentItem: Text {
                        text: parent.text
                        font.pixelSize: 12
                        color: parent.checked ? theme.text : theme.textDim
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // 趋势图卡片（放大今日分布时动画收起，任务 6.5）
        Rectangle {
            width: parent.width
            height: root.expanded ? 0 : content.height - 34 - 16 - 16 - 130
            radius: 12
            color: theme.surface
            border.color: theme.border
            clip: true
            visible: height > 4
            opacity: visible ? 1 : 0

            Behavior on height {
                NumberAnimation { duration: 260; easing.type: Easing.InOutCubic }
            }
            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }

            TrendChart {
                anchors.fill: parent
                anchors.margins: 12
                visible: root.series.length > 0
                series: root.series
            }

            // 空态（04 号文档 §6）
            Column {
                anchors.centerIn: parent
                spacing: 8
                visible: root.series.length === 0

                Label {
                    text: qsTr("数据正在积累中")
                    color: theme.text
                    font.pixelSize: 15
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Label {
                    text: qsTr("使用一天后这里将展示趋势图")
                    color: theme.textDim
                    font.pixelSize: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // 今日 24 小时分布卡片（任务 6.5：可放大填满剩余区域，切换柱状图）
        Rectangle {
            width: parent.width
            height: root.expanded ? content.height - 34 - 16 - 16 : 130
            radius: 12
            color: theme.surface
            border.color: theme.border
            clip: true

            Behavior on height {
                NumberAnimation { duration: 260; easing.type: Easing.InOutCubic }
            }

            Label {
                x: 20
                y: 14
                text: qsTr("今日 24 小时分布")
                color: theme.textDim
                font.pixelSize: 12
                font.letterSpacing: 1
            }

            // 放大/缩小按钮（右上角）
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 10
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
                height: root.expanded ? 0 : 56
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

            // 放大态：逐小时柱状图（任务 6.5）
            HourlyBars {
                x: 20
                y: 40
                width: parent.width - 40
                height: root.expanded ? parent.height - 40 - 14 : 0
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
}
