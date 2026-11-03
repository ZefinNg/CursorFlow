import QtQuick 2.15
import QtQuick.Controls 2.15

// 统计卡片（04 号文档 §2.1）：标签 + 大数字，值变化时数字滚动动画
Rectangle {
    id: root

    property string label
    property string mode // distance / count / minutes
    property real value: 0
    property color accent: theme.primary
    property real shown: value

    Behavior on shown {
        enabled: settings.animationsEnabled
        NumberAnimation {
            duration: 450
            easing.type: Easing.OutCubic
        }
    }

    color: theme.surface
    radius: 12
    border.color: theme.border
    border.width: 1

    // 千分位分组：QML V4 引擎对 \B lookahead 正则支持异常（出现 "3,,835"），
    // 改用纯字符串每 3 位分组，各引擎行为一致
    function groupThousands(n) {
        var s = String(n)
        var out = ""
        while (s.length > 3) {
            out = "," + s.slice(-3) + out
            s = s.slice(0, -3)
        }
        return s + out
    }

    function format(v) {
        if (root.mode === "distance") {
            return v < 1000 ? v.toFixed(2) + " m"
                            : (v / 1000).toFixed(2) + " km"
        }
        if (root.mode === "minutes") {
            if (v < 60) {
                return Math.round(v) + " " + qsTr("分钟")
            }
            const h = Math.floor(v / 60)
            const m = Math.round(v % 60)
            return h + " " + qsTr("小时") + (m > 0 ? " " + m + " " + qsTr("分") : "")
        }
        return groupThousands(Math.round(v))
    }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Row {
            spacing: 8
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root.accent
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                text: root.label
                color: theme.textDim
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Label {
            text: root.format(root.shown)
            color: theme.text
            font.pixelSize: 26 * theme.fontScale
            font.weight: Font.DemiBold
        }
    }
}
