import QtQuick 2.15

// 粒子轨迹（04 号文档 §2.1）：32 个粒子跟随最近鼠标速度向量衰减运动，
// 双色（primary/secondary）按速度淡入淡出。
// 动效约束（04 号文档 §7）：animationsEnabled=false 时不创建实例（外层 Loader 控制），
// 窗口隐藏即暂停 Timer（running 绑定 visible）；仅区域悬停时注入速度向量。
Canvas {
    id: root

    readonly property int particleCount: 32
    property color colorA: theme.primary
    property color colorB: theme.secondary
    property var particles: [] // {x, y, vx, vy}
    property real cursorVx: 0
    property real cursorVy: 0
    property bool hasCursor: false

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        for (let i = 0; i < particles.length; ++i) {
            const p = particles[i]
            const speed = Math.sqrt(p.vx * p.vx + p.vy * p.vy)
            ctx.globalAlpha = Math.min(0.85, 0.12 + speed * 0.05)
            ctx.fillStyle = i % 2 === 0 ? colorA : colorB
            ctx.beginPath()
            ctx.arc(p.x, p.y, 2.2, 0, Math.PI * 2)
            ctx.fill()
        }
        ctx.globalAlpha = 1
    }

    onColorAChanged: requestPaint()
    onColorBChanged: requestPaint()

    Timer {
        interval: 33 // ~30fps，装饰性动画不需要更高帧率
        running: root.visible && settings.animationsEnabled
        repeat: true
        onTriggered: root.tick()
    }

    // 仅跟踪悬停速度，不接受点击（不阻挡下层交互）
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        property real lastX: 0
        property real lastY: 0
        onPositionChanged: {
            root.cursorVx = mouse.x - lastX
            root.cursorVy = mouse.y - lastY
            root.hasCursor = true
            lastX = mouse.x
            lastY = mouse.y
        }
        onExited: root.hasCursor = false
    }

    Component.onCompleted: {
        const list = []
        for (let i = 0; i < particleCount; ++i) {
            list.push({ x: Math.random() * width, y: Math.random() * height, vx: 0, vy: 0 })
        }
        particles = list
        requestPaint()
    }

    // 粒子运动：向最近鼠标速度向量收敛 + 轻微抖动；无输入时按 0.92 衰减漂移（§2.1）
    function tick() {
        for (let i = 0; i < particles.length; ++i) {
            const p = particles[i]
            if (hasCursor) {
                p.vx = p.vx * 0.7 + cursorVx * 0.3 + (Math.random() - 0.5) * 0.6
                p.vy = p.vy * 0.7 + cursorVy * 0.3 + (Math.random() - 0.5) * 0.6
            } else {
                p.vx *= 0.92
                p.vy *= 0.92
            }
            p.x += p.vx
            p.y += p.vy
            // 边缘回绕保持粒子数恒定
            if (p.x < 0) {
                p.x += width
            } else if (p.x > width) {
                p.x -= width
            }
            if (p.y < 0) {
                p.y += height
            } else if (p.y > height) {
                p.y -= height
            }
        }
        requestPaint()
    }
}
