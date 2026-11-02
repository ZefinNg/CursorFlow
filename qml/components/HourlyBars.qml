import QtQuick 2.15
import QtQuick.Controls 2.15

// 逐小时柱状图（任务 6.5，M4）：今日分布卡片放大视图。
// 24 根柱，活跃度归一口径与 HeatmapGrid 一致（距离/键数分别取峰值比取大）；
// 悬停浮层文案与热力图共用 "HH:00 – 距离 · N 键"。
Canvas {
    id: root

    property var hours: []          // 24 项 {hour, distance, keys}
    property int hoverHour: -1
    readonly property int axisHeight: 18 // 底部小时刻度带

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if (hours.length !== 24) {
            return
        }

        // 峰值归一（HeatmapGrid.levelOf 同口径）
        let maxD = 0
        let maxK = 0
        for (let i = 0; i < 24; ++i) {
            maxD = Math.max(maxD, hours[i].distance)
            maxK = Math.max(maxK, hours[i].keys)
        }
        const plotH = height - axisHeight
        const gap = 6
        const barW = (width - 23 * gap) / 24

        for (let h = 0; h < 24; ++h) {
            const cell = hours[h]
            const act = Math.max(maxD > 0 ? cell.distance / maxD : 0,
                                 maxK > 0 ? cell.keys / maxK : 0)
            const barH = Math.max(act > 0 ? 4 : 2, act * (plotH - 8))
            const x = h * (barW + gap)

            ctx.fillStyle = theme.primary
            ctx.globalAlpha = h === hoverHour ? 1.0 : 0.55
            // 底部圆角矩形：两段矩形近似圆角，避免逐路径开销
            ctx.fillRect(x, plotH - barH, barW, barH)
            if (act > 0) {
                ctx.fillRect(x + barW * 0.12, plotH - barH - 2,
                             barW * 0.76, 2)
            }

            // 悬停柱描边高亮
            if (h === hoverHour) {
                ctx.globalAlpha = 1.0
                ctx.strokeStyle = theme.textDim
                ctx.lineWidth = 1
                ctx.strokeRect(x - 0.5, plotH - barH - 0.5, barW + 1, barH + 1)
            }
        }
        ctx.globalAlpha = 1.0

        // 底部小时刻度：0/6/12/18/23（Canvas 用通用字体，避免 fontFamily 引发
        // "font families specified are invalid" 警告）
        ctx.fillStyle = theme.textDim
        ctx.font = "10px sans-serif"
        for (const h of [0, 6, 12, 18, 23]) {
            const label = h + ":00"
            const x = h * (barW + gap)
            const tx = Math.max(0, Math.min(width - ctx.measureText(label).width,
                                            x - ctx.measureText(label).width / 2))
            ctx.fillText(label, tx, height - 4)
        }
    }

    onHoursChanged: requestPaint()
    onHoverHourChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function fmtDist(v) {
        return v < 1000 ? Math.round(v) + " m" : (v / 1000).toFixed(2) + " km"
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        onPositionChanged: (mouse) => {
            if (mouse.x < 0 || mouse.x > width) {
                root.hoverHour = -1
                return
            }
            const gap = 6
            const barW = (width - 23 * gap) / 24
            root.hoverHour = Math.max(0, Math.min(23, Math.floor(mouse.x / (barW + gap))))
        }
        onExited: root.hoverHour = -1
    }

    // 悬停浮层（与 HeatmapGrid 同款文案）
    Rectangle {
        visible: root.hoverHour >= 0 && root.hours.length === 24
        y: -height - 8
        x: {
            const gap = 6
            const barW = (root.width - 23 * gap) / 24
            const cx = root.hoverHour * (barW + gap) + barW / 2
            return Math.max(0, Math.min(root.width - width, cx - width / 2))
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
