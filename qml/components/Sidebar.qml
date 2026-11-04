import QtQuick 2.15
import QtQuick.Controls 2.15

// 图标导航侧栏（04 号文档 §2）：概览 / 历史 / 成就 / 设置
Rectangle {
    id: root

    signal navigate(int index)
    property int currentIndex: 0

    width: 64
    color: theme.isDark ? "#12141C" : "#EBEDF3"

    // 右侧分隔线
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: theme.border
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 16
        width: parent.width
        spacing: 6

        Repeater {
            model: [
                { icon: "qrc:/qml/icons/overview.svg", text: qsTr("概览") },
                { icon: "qrc:/qml/icons/history.svg", text: qsTr("历史") },
                { icon: "qrc:/qml/icons/achievements.svg", text: qsTr("成就") },
                { icon: "qrc:/qml/icons/settings.svg", text: qsTr("设置") },
            ]

            delegate: ItemDelegate {
                id: navItem

                required property int index
                required property var modelData

                width: 64
                height: 56
                hoverEnabled: true
                onClicked: {
                    root.currentIndex = index
                    root.navigate(index)
                }

                background: Rectangle {
                    radius: 10
                    color: navItem.index === root.currentIndex
                           ? (theme.isDark ? "#1F2331" : "#DDE1EF")
                           : (navItem.hovered
                              ? (theme.isDark ? "#191C28" : "#E4E7F2") : "transparent")
                    anchors.fill: parent
                    anchors.margins: 6
                }

                // 选中左侧指示条
                Rectangle {
                    visible: navItem.index === root.currentIndex
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: 22
                    radius: 1.5
                    color: theme.primary
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 4

                    Image {
                        source: navItem.modelData.icon
                        sourceSize: Qt.size(20, 20)
                        opacity: navItem.index === root.currentIndex ? 1.0 : 0.6
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Label {
                        text: navItem.modelData.text
                        color: navItem.index === root.currentIndex ? theme.text : theme.textDim
                        font.pixelSize: 10
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }
    }
}
