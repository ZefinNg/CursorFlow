import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

// 悬浮窗今日概览（M6 8.1，PRD 3.9，04 号文档 §2.7）：主窗口最小化/隐藏到托盘时
// 显示的半透明置顶小窗；仅展示今日距离/按键/活跃时长三项，随采集增量动态刷新。
// 可拖拽（位置记忆 overlay/x、overlay/y），可关闭（记住选择）。
// 由 Main.qml Component.createObject(null) 动态创建为完全独立窗口（M6 修复）。
Window {
    id: overlay

    // 点击"打开主窗口"按钮（M6 8.2 增补）→ Main.qml 激活主窗口
    signal showMainRequested()

    width: 220
    height: 132
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    title: qsTr("CursorFlow 今日概览")
    color: "transparent" // 半透明窗体（动态创建后可用，PRD 3.9）

    // 位置：首次 = 屏幕右下角（04 号文档 §2.7），之后用持久化记忆位置
    x: settings.overlayX >= 0 ? settings.overlayX
                              : Screen.desktopAvailableWidth - width - 24
    y: settings.overlayY >= 0 ? settings.overlayY
                              : Screen.desktopAvailableHeight - height - 40

    // 纯字符串分段千分位（QML V4 兼容，复用 6.5 修复教训，不用正则）
    function formatInt(n) {
        const s = String(Math.round(n))
        let out = ""
        let rest = s
        while (rest.length > 3) {
            out = "," + rest.slice(-3) + out
            rest = rest.slice(0, -3)
        }
        return rest + out
    }
    // 距离格式化：<1 km 保留两位（6913f96 同规则）
    function formatDistance(m) {
        if (m < 1000) {
            return Math.round(m) + " m"
        }
        return (m / 1000).toFixed(2) + " km"
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: 12
        opacity: 0.88 // 半透明（PRD 3.9）
        color: theme.surface
        border.color: theme.border

        // 手动拖拽（04 号文档 §2.7）：系统级 startSystemMove 无法编程控制，
        // 边界钳制与边缘吸附需手动管理位置；拖拽全程以按下时所在屏幕为界
        MouseArea {
            id: dragArea
            anchors.fill: parent
            cursorShape: Qt.SizeAllCursor

            property point pressPos          // 按下点（面板坐标）
            property rect screenBounds       // 按下时所在屏的可用区域（虚拟桌面绝对坐标）
            readonly property int snapDist: 16 // 距边缘小于此值自动吸附

            onPressed: {
                pressPos = Qt.point(mouse.x, mouse.y)
                screenBounds = Qt.rect(Screen.virtualX, Screen.virtualY,
                                       Screen.desktopAvailableWidth,
                                       Screen.desktopAvailableHeight)
            }
            onPositionChanged: {
                if (!pressed) {
                    return
                }
                // 位置钳制：整个窗口不超出屏幕可用范围（PRD 3.9）
                var nx = overlay.x + (mouse.x - pressPos.x)
                var ny = overlay.y + (mouse.y - pressPos.y)
                nx = Math.max(screenBounds.x,
                              Math.min(nx, screenBounds.x + screenBounds.width - overlay.width))
                ny = Math.max(screenBounds.y,
                              Math.min(ny, screenBounds.y + screenBounds.height - overlay.height))
                overlay.x = nx
                overlay.y = ny
            }
            onReleased: {
                // 边缘吸附：贴近任一边 < snapDist 时贴边（上下左右独立判定）
                var left = screenBounds.x
                var top = screenBounds.y
                var right = screenBounds.x + screenBounds.width
                var bottom = screenBounds.y + screenBounds.height
                if (overlay.x - left < snapDist) {
                    overlay.x = left
                } else if (right - (overlay.x + overlay.width) < snapDist) {
                    overlay.x = right - overlay.width
                }
                if (overlay.y - top < snapDist) {
                    overlay.y = top
                } else if (bottom - (overlay.y + overlay.height) < snapDist) {
                    overlay.y = bottom - overlay.height
                }
                // 位置持久化记忆（04 号文档 §2.7）
                settings.overlayX = overlay.x
                settings.overlayY = overlay.y
            }
        }

        Column {
            x: 16
            y: 10
            width: parent.width - 32
            spacing: 7

            Item {
                width: parent.width
                height: 18

                Label {
                    text: qsTr("今日概览")
                    color: theme.textDim
                    font.pixelSize: 11
                    font.letterSpacing: 1
                    anchors.verticalCenter: parent.verticalCenter
                }

                // 打开主窗口按钮（M6 8.2 增补）：点击唤起主窗口，悬浮窗随显隐联动自动隐藏
                Item {
                    id: showMainBtn
                    width: 18
                    height: 18
                    anchors.right: parent.right
                    anchors.rightMargin: 24
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: showMainArea.containsMouse ? theme.border : "transparent"
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "\u29C9" // ⧉ 重叠窗口
                        color: theme.textDim
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: showMainArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: overlay.showMainRequested()
                    }
                    ToolTip.visible: showMainArea.containsMouse
                    ToolTip.text: qsTr("打开主窗口")
                    ToolTip.delay: 400
                }

                // 关闭按钮（记住选择，PRD 3.9）：设置页开关可重新开启
                Item {
                    id: closeBtn
                    width: 18
                    height: 18
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: closeArea.containsMouse ? theme.border : "transparent"
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "\u00D7" // ×
                        color: theme.textDim
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settings.showFloatingOverlay = false
                    }
                }
            }

            // 三项指标：绑定 stats（NOTIFY todayStatsChanged，动态刷新，04 号文档 §2.7）
            Item {
                width: parent.width
                height: 20
                Label {
                    text: qsTr("鼠标距离")
                    color: theme.textDim
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label {
                    text: overlay.formatDistance(stats.todayDistanceM)
                    color: theme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Item {
                width: parent.width
                height: 20
                Label {
                    text: qsTr("键盘次数")
                    color: theme.textDim
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label {
                    text: overlay.formatInt(stats.todayKeyPresses)
                    color: theme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Item {
                width: parent.width
                height: 20
                Label {
                    text: qsTr("活跃时长")
                    color: theme.textDim
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label {
                    // 活跃时长（分钟）按现有口径转 h:mm 展示（纯字符串补零，规避 V4 兼容性）
                    text: Math.floor(stats.todayActiveMinutes / 60) + ":" +
                          ("0" + (stats.todayActiveMinutes % 60)).slice(-2)
                    color: theme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
