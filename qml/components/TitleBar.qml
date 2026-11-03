import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

// 自定义标题栏（任务 6.6，M4；04 号文档 §2.5）：替代系统原生标题栏，
// 配合 Main.qml 的 FramelessWindowHint 无边框窗口。
// 底色与 Sidebar 一致实现视觉一体；整栏可拖拽移动（startSystemMove 保留系统贴靠），
// 双击切换最大化；右上角最小化/最大化-还原/关闭（关闭走 Main.qml onClosing 询问流程）。
Rectangle {
    id: root

    // 宿主窗口（Main.qml 传入 root）：用于移动/最大化/关闭
    property var window: null
    readonly property bool maximized: window ? window.visibility === Window.Maximized : false

    height: 40
    color: theme.isDark ? "#12141C" : "#EBEDF3"

    // 底部分隔线（与 Sidebar 右侧分隔线同款）
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: theme.border
    }

    // 拖拽移动 + 双击最大化（按钮为上层兄弟项，点击优先命中按钮）
    MouseArea {
        anchors.fill: parent
        onPressed: if (root.window) root.window.startSystemMove()
        onDoubleClicked: if (root.window) root.window.toggleMaximized()
    }

    // 左侧：Logo + 软件名
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        Image {
            source: "qrc:/qml/icons/CursorFlow.png"
            sourceSize: Qt.size(18, 18)
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            text: "CursorFlow"
            color: theme.text
            font.pixelSize: 12
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // 右上角：窗口控制按钮
    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height

        // 最小化
        CaptionButton {
            Rectangle {
                anchors.centerIn: parent
                width: 10
                height: 1
                color: theme.text
            }
            onActivated: if (root.window) root.window.showMinimized()
        }

        // 最大化 / 还原
        CaptionButton {
            Item {
                anchors.centerIn: parent
                width: 10
                height: 10

                // 还原态：叠放双框（前框以标题栏底色遮挡形成 ❐）
                Rectangle {
                    visible: root.maximized
                    x: 2
                    y: 0
                    width: 8
                    height: 8
                    radius: 1
                    color: "transparent"
                    border.color: theme.text
                    border.width: 1
                }
                Rectangle {
                    visible: root.maximized
                    x: 0
                    y: 2
                    width: 8
                    height: 8
                    radius: 1
                    color: root.color
                    border.color: theme.text
                    border.width: 1
                }
                // 最大化态：单框
                Rectangle {
                    visible: !root.maximized
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    radius: 1
                    color: "transparent"
                    border.color: theme.text
                    border.width: 1
                }
            }
            onActivated: if (root.window) root.window.toggleMaximized()
        }

        // 关闭（悬停红底，Windows 惯例；走 onClosing 询问流程）
        CaptionButton {
            danger: true

            Item {
                anchors.centerIn: parent
                width: 12
                height: 12
                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 1
                    color: theme.text
                    rotation: 45
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 1
                    color: theme.text
                    rotation: -45
                }
            }
            onActivated: if (root.window) root.window.close()
        }
    }

    // 窗口控制按钮模板（Qt 5.15 inline component）：悬停浮起，danger 悬停红底
    component CaptionButton: Item {
        id: btn

        property bool danger: false
        signal activated()
        default property alias content: glyph.data

        width: 46
        height: parent.height

        Rectangle {
            anchors.fill: parent
            color: area.containsMouse
                   ? (btn.danger ? "#E81123" : (theme.isDark ? "#1F2331" : "#DDE1EF"))
                   : "transparent"
        }

        Item {
            id: glyph
            anchors.fill: parent
        }

        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            onClicked: btn.activated()
        }
    }
}
