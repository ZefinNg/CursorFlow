import QtQuick 2.15
import QtQuick.Controls 2.15

// 趋势折线图（04 号文档 §2.2）：Canvas 自绘双序列——鼠标距离（左轴，primary）
// 与按键数（右轴，secondary）；悬停最近点显示 tooltip（日期 + 数值）。
// 数据 ≤30 点，onSeriesChanged / onHoverIndexChanged 各一次 requestPaint
// 全量重绘，避免逐点动画残留（04 号文档 §7 性能约束）。
Canvas {
    id: canvas

    property var series: []       // [{date: "yyyy-MM-dd", distance: 米, keys: 次数}]
    property int hoverIndex: -1
    property real lastPaintMs: 0  // 单次重绘耗时（5.3 性能专项采样用）

    // 绘图区边距（左右留出双轴刻度）
    readonly property real plotX: 48
    readonly property real plotY: 16
    readonly property real plotW: width - 48 - 48
    readonly property real plotH: height - 16 - 26

    onSeriesChanged: requestPaint()
    onHoverIndexChanged: requestPaint()

    // 轴上限取"好看"的整刻度（1/2/2.5/5/10 × 10^n）
    function niceMax(v) {
        if (v <= 0) {
            return 1
        }
        const exp = Math.pow(10, Math.floor(Math.log(v) / Math.LN10))
        const candidates = [1, 2, 2.5, 5, 10]
        for (let i = 0; i < candidates.length; ++i) {
            if (candidates[i] * exp >= v) {
                return candidates[i] * exp
            }
        }
        return 10 * exp
    }

    function fmtDist(v) {
        return v < 1000 ? Math.round(v) + " m" : (v / 1000).toFixed(1) + " km"
    }

    function pointX(i) {
        const n = series.length
        if (n <= 1) {
            return plotX + plotW / 2
        }
        return plotX + plotW * i / (n - 1)
    }

    function pointY(value, maxV) {
        return plotY + plotH * (1 - value / maxV)
    }

    onPaint: {
        const t0 = Date.now() // Qt5 QML 无 performance 对象，Date.now() 毫秒精度足够
        const ctx = getContext("2d")
        ctx.reset()

        const n = series.length
        let maxDist = 0
        let maxKeys = 0
        for (let i = 0; i < n; ++i) {
            maxDist = Math.max(maxDist, series[i].distance)
            maxKeys = Math.max(maxKeys, series[i].keys)
        }
        maxDist = niceMax(maxDist)
        maxKeys = niceMax(maxKeys)

        // 网格 + 双轴刻度（4 段）
        ctx.font = "10px sans-serif"
        ctx.textBaseline = "middle"
        for (let g = 0; g <= 4; ++g) {
            const y = plotY + plotH * g / 4
            ctx.strokeStyle = theme.border
            ctx.globalAlpha = 0.6
            ctx.beginPath()
            ctx.moveTo(plotX, y)
            ctx.lineTo(plotX + plotW, y)
            ctx.stroke()
            ctx.globalAlpha = 1

            ctx.fillStyle = theme.textDim
            ctx.textAlign = "right"
            ctx.fillText(fmtDist(maxDist * (4 - g) / 4), plotX - 8, y)
            ctx.textAlign = "left"
            ctx.fillText(String(Math.round(maxKeys * (4 - g) / 4)), plotX + plotW + 8, y)
        }

        // X 轴日期标签（最多 6 个，MM-dd）
        if (n > 0) {
            ctx.textAlign = "center"
            ctx.textBaseline = "alphabetic"
            const step = Math.max(1, Math.ceil(n / 6))
            for (let i = 0; i < n; i += step) {
                ctx.fillStyle = theme.textDim
                ctx.fillText(series[i].date.substring(5), pointX(i), height - 8)
            }
        }

        drawSeries("distance", maxDist, theme.primary, true)
        drawSeries("keys", maxKeys, theme.secondary, false)

        // 悬停指示：虚线 + 双序列圆点
        if (hoverIndex >= 0 && hoverIndex < n) {
            const x = pointX(hoverIndex)
            ctx.strokeStyle = theme.textDim
            ctx.setLineDash([3, 3])
            ctx.beginPath()
            ctx.moveTo(x, plotY)
            ctx.lineTo(x, plotY + plotH)
            ctx.stroke()
            ctx.setLineDash([])
            ctx.fillStyle = theme.primary
            ctx.beginPath()
            ctx.arc(x, pointY(series[hoverIndex].distance, maxDist), 3.5, 0, Math.PI * 2)
            ctx.fill()
            ctx.fillStyle = theme.secondary
            ctx.beginPath()
            ctx.arc(x, pointY(series[hoverIndex].keys, maxKeys), 3.5, 0, Math.PI * 2)
            ctx.fill()
        }

        canvas.lastPaintMs = Date.now() - t0
    }

    function drawSeries(field, maxV, color, filled) {
        const n = series.length
        if (n === 0) {
            return
        }
        const ctx = getContext("2d")
        ctx.beginPath()
        for (let i = 0; i < n; ++i) {
            const x = pointX(i)
            const y = pointY(series[i][field], maxV)
            if (i === 0) {
                ctx.moveTo(x, y)
            } else {
                ctx.lineTo(x, y)
            }
        }
        ctx.strokeStyle = color
        ctx.lineWidth = 2
        ctx.lineJoin = "round"
        ctx.stroke()
        if (filled) { // 距离序列下方淡填充，强化主序列
            ctx.lineTo(pointX(n - 1), plotY + plotH)
            ctx.lineTo(pointX(0), plotY + plotH)
            ctx.closePath()
            ctx.globalAlpha = 0.12
            ctx.fillStyle = color
            ctx.fill()
            ctx.globalAlpha = 1
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        onPositionChanged: {
            const n = canvas.series.length
            if (n === 0) {
                return
            }
            const i = Math.round((mouse.x - canvas.plotX) / canvas.plotW * (n - 1))
            canvas.hoverIndex = Math.max(0, Math.min(n - 1, i))
        }
        onExited: canvas.hoverIndex = -1
    }

    // 悬停 tooltip（日期 + 数值，04 号文档 §2.2）
    Rectangle {
        visible: canvas.hoverIndex >= 0 && canvas.series.length > 0
        y: 0
        x: {
            const point = canvas.pointX(canvas.hoverIndex)
            return Math.max(0, Math.min(canvas.width - width, point - width / 2))
        }
        width: tipCol.implicitWidth + 20
        height: tipCol.implicitHeight + 14
        radius: 6
        color: theme.surface
        border.color: theme.border
        z: 10

        Column {
            id: tipCol
            anchors.centerIn: parent
            spacing: 2

            Label {
                text: canvas.hoverIndex >= 0 ? canvas.series[canvas.hoverIndex].date : ""
                color: theme.textDim
                font.pixelSize: 10
            }
            Label {
                text: canvas.hoverIndex >= 0
                      ? qsTr("距离 %1 · %2 键").arg(
                            canvas.fmtDist(canvas.series[canvas.hoverIndex].distance)).arg(
                            canvas.series[canvas.hoverIndex].keys)
                      : ""
                color: theme.text
                font.pixelSize: 11
            }
        }
    }
}
