import QtQuick 2.15
import QtQuick.Controls 2.15

// 24 格小时热力图（04 号文档 §2.2）：主题色 blue-violet 5 档透明度，
// 悬停显示 "HH:00 – 距离/按键数"；0 活动时段为最低档（全灰观感，§6 空态）。
Item {
    id: root

    property var hours: []          // 24 项 {hour, distance, keys}
    property color baseColor: theme.primary
    property int hoverHour: -1
    readonly property int cellSpacing: 4
    readonly property var opacities: [0.05, 0.18, 0.38, 0.62, 0.92]

    // 相对当日峰值分 5 档（0–4）；≤24 项实时计算开销可忽略
    function levelOf(index) {
        if (hours.length !== 24) {
            return 0
        }
        let maxD = 0
        let maxK = 0
        for (let i = 0; i < 24; ++i) {
            maxD = Math.max(maxD, hours[i].distance)
            maxK = Math.max(maxK, hours[i].keys)
        }
        const cell = hours[index]
        const act = Math.max(maxD > 0 ? cell.distance / maxD : 0,
                             maxK > 0 ? cell.keys / maxK : 0)
        if (act <= 0) {
            return 0
        }
        if (act < 0.25) {
            return 1
        }
        if (act < 0.5) {
            return 2
        }
        if (act < 0.75) {
            return 3
        }
        return 4
    }

    function fmtDist(v) {
        return v < 1000 ? Math.round(v) + " m" : (v / 1000).toFixed(2) + " km"
    }

    // 24 格均分宽度
    Item {
        id: cells
        anchors.fill: parent

        Repeater {
            model: 24

            Item {
                readonly property int hour: index
                x: index * (cells.width + root.cellSpacing) / 24
                width: (cells.width - 23 * root.cellSpacing) / 24
                height: cells.height

                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: theme.surface
                    border.color: theme.border
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 4
                    color: root.baseColor
                    opacity: root.opacities[root.levelOf(index)]
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    onEntered: root.hoverHour = hour
                    onExited: root.hoverHour = -1
                }
            }
        }
    }

    // 悬停浮层（04 号文档 §2.2："HH:00 – 距离/按键数"）
    Rectangle {
        visible: root.hoverHour >= 0
        y: -height - 8
        x: {
            const cellX = root.hoverHour * (root.width + root.cellSpacing) / 24
            return Math.max(0, Math.min(root.width - width, cellX - width / 2))
        }
        width: tipLabel.implicitWidth + 20
        height: tipLabel.implicitHeight + 14
        radius: 6
        color: theme.surface
        border.color: theme.border
        z: 10

        Label {
            id: tipLabel
            anchors.centerIn: parent
            font.pixelSize: 11
            color: theme.text
            text: root.hoverHour >= 0 && root.hours.length === 24
                  ? qsTr("%1:00 – %2 · %3 键").arg(root.hoverHour).arg(
                        root.fmtDist(root.hours[root.hoverHour].distance)).arg(
                        root.hours[root.hoverHour].keys)
                  : ""
        }
    }
}
