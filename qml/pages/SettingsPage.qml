import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

// 设置页（04 号文档 §2.3）：通用组 + 显示组 + 校准组（4.4）。
// 导出组（5.2）后续任务接入。变更即写 QSettings。
Item {
    id: root

    property int pageIndex: 2

    Flickable {
        anchors.fill: parent
        contentHeight: content.height + 56
        clip: true

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: content

            x: 28
            y: 28
            width: parent.width - 56
            spacing: 10

            Label {
                text: qsTr("设置")
                color: theme.text
                font.pixelSize: 18 * theme.fontScale
                font.weight: Font.DemiBold
            }

            // ---- 通用 ----
            Label {
                text: qsTr("通用")
                color: theme.textDim
                font.pixelSize: 12
                font.letterSpacing: 1
            }
            Rectangle {
                width: parent.width
                height: generalCol.implicitHeight + 24
                radius: 12
                color: theme.surface
                border.color: theme.border

                Column {
                    id: generalCol
                    x: 20
                    y: 12
                    width: parent.width - 40
                    spacing: 0

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("开机自启")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("登录后自动开始统计")
                            color: theme.textDim
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: autostartSwitch.left
                            anchors.rightMargin: 12
                        }
                        Switch {
                            id: autostartSwitch
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            checked: settings.autostart
                            onToggled: settings.autostart = checked
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("语言")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            id: restartHint
                            visible: false
                            text: qsTr("语言切换将在重启后生效")
                            color: theme.textDim
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: languageBox.left
                            anchors.rightMargin: 12
                        }
                        ComboBox {
                            id: languageBox
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            width: 140

                            property var langValues: ["system", "zh_CN", "en"]
                            model: [qsTr("跟随系统"), qsTr("中文"), qsTr("English")]
                            currentIndex: langValues.indexOf(settings.language)
                            onActivated: {
                                settings.language = langValues[currentIndex]
                                restartHint.visible = settings.language !== "system"
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("数据保留天数")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        SpinBox {
                            id: retentionBox
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            from: 30
                            to: 3650
                            value: settings.retentionDays
                            onValueModified: settings.retentionDays = value
                        }
                    }
                }
            }

            // ---- 显示 ----
            Label {
                text: qsTr("显示")
                color: theme.textDim
                font.pixelSize: 12
                font.letterSpacing: 1
            }
            Rectangle {
                width: parent.width
                height: displayCol.implicitHeight + 24
                radius: 12
                color: theme.surface
                border.color: theme.border

                Column {
                    id: displayCol
                    x: 20
                    y: 12
                    width: parent.width - 40
                    spacing: 0

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("主题")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            width: 140
                            model: [qsTr("深色"), qsTr("浅色"), qsTr("跟随系统")]
                            currentIndex: settings.themeMode
                            onActivated: settings.themeMode = currentIndex
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("字体大小")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: Math.round(fontSlider.value * 100) + "%"
                            color: theme.textDim
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: fontSlider.left
                            anchors.rightMargin: 12
                        }
                        Slider {
                            id: fontSlider
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            width: 160
                            from: 0.9
                            to: 1.3
                            stepSize: 0.05
                            value: settings.fontScale
                            onMoved: settings.fontScale = value
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("动画效果")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Switch {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            checked: settings.animationsEnabled
                            onToggled: settings.animationsEnabled = checked
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    // 关闭窗口行为（任务 6.4）：0 每次询问 / 1 最小化到托盘 / 2 退出程序
                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("关闭窗口时")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            width: 140
                            model: [qsTr("每次询问"), qsTr("最小化到托盘"), qsTr("退出程序")]
                            currentIndex: settings.closeAction
                            onActivated: settings.closeAction = currentIndex
                        }
                    }
                }
            }

            // ---- 校准（04 号文档 §2.3；02 号文档 §4.1 PPI 换算）----
            Label {
                text: qsTr("校准")
                color: theme.textDim
                font.pixelSize: 12
                font.letterSpacing: 1
            }
            Rectangle {
                width: parent.width
                height: calibCol.implicitHeight + 24
                radius: 12
                color: theme.surface
                border.color: theme.border

                Column {
                    id: calibCol
                    x: 20
                    y: 12
                    width: parent.width - 40
                    spacing: 0

                    // 任一输入变化即重算 PPI 并持久化；主程序联动更新采集线程 DPI
                    function applyCalibration() {
                        settings.setCalibration(inchBox.value, widthBox.value, heightBox.value)
                    }

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("屏幕对角线")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("英寸")
                            color: theme.textDim
                            font.pixelSize: 11
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: inchBox.left
                            anchors.rightMargin: 12
                        }
                        SpinBox {
                            id: inchBox
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            width: 120
                            editable: true
                            from: 5
                            to: 200
                            stepSize: 1 // Qt5 SpinBox 仅支持整数；英寸取整对 PPI 精度影响可忽略
                            value: settings.calibrationDiagonalInch > 0
                                       ? Math.round(settings.calibrationDiagonalInch) : 24
                            onValueModified: calibCol.applyCalibration()
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("分辨率")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        // 物理像素（逻辑值 × 缩放比）预填，可手动修改
                        SpinBox {
                            id: widthBox
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: resolutionX.left
                            anchors.rightMargin: 8
                            width: 104
                            editable: true
                            from: 320
                            to: 16384
                            value: Math.round(Screen.width * Screen.devicePixelRatio)
                            onValueModified: calibCol.applyCalibration()
                        }
                        Label {
                            id: resolutionX
                            text: "×"
                            color: theme.textDim
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: heightBox.left
                            anchors.rightMargin: 8
                        }
                        SpinBox {
                            id: heightBox
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            width: 104
                            editable: true
                            from: 240
                            to: 16384
                            value: Math.round(Screen.height * Screen.devicePixelRatio)
                            onValueModified: calibCol.applyCalibration()
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("像素密度")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            // 校准前按 96 DPI 估算并标注（06 号文档 §6 风险应对）
                            text: settings.calibrationDiagonalInch > 0
                                      ? settings.calibrationPpi + " PPI"
                                      : qsTr("96 PPI（估算值）")
                            color: settings.calibrationDiagonalInch > 0
                                       ? theme.text : theme.textDim
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    Label {
                        width: parent.width
                        text: qsTr("填写屏幕尺寸与分辨率后，鼠标距离换算更精确；距离 = 像素位移 ÷ PPI × 2.54 cm")
                        color: theme.textDim
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        topPadding: 10
                        bottomPadding: 8
                    }
                }
            }

            // ---- 导出（04 号文档 §2.3；03 号文档 §6 格式）----
            Label {
                text: qsTr("导出")
                color: theme.textDim
                font.pixelSize: 12
                font.letterSpacing: 1
            }
            Rectangle {
                width: parent.width
                height: exportCol.implicitHeight + 24
                radius: 12
                color: theme.surface
                border.color: theme.border

                Column {
                    id: exportCol
                    x: 20
                    y: 12
                    width: parent.width - 40
                    spacing: 0

                    property int rangeDays: 30

                    Item {
                        width: parent.width
                        height: 52
                        Label {
                            text: qsTr("导出范围")
                            color: theme.text
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            width: 140
                            model: [qsTr("近 7 天"), qsTr("近 30 天"), qsTr("近 90 天"), qsTr("近 365 天")]
                            property var dayValues: [7, 30, 90, 365]
                            currentIndex: 1
                            onActivated: exportCol.rangeDays = dayValues[currentIndex]
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: theme.border
                    }

                    Item {
                        width: parent.width
                        height: 52
                        Button {
                            width: 96
                            height: 30
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            text: qsTr("导出 CSV")
                            onClicked: exporter.exportRange(exportCol.rangeDays, "csv")
                        }
                        Button {
                            width: 96
                            height: 30
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 112
                            text: qsTr("导出 JSON")
                            onClicked: exporter.exportRange(exportCol.rangeDays, "json")
                        }
                        Label {
                            id: exportStatus
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            text: ""
                            color: theme.textDim
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            width: Math.min(320, parent.width - 224)
                        }
                    }

                    Label {
                        width: parent.width
                        text: qsTr("导出为 UTF-8 编码文件，仅包含本地统计数据，可直接用 Excel 或文本编辑器打开")
                        color: theme.textDim
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        topPadding: 10
                        bottomPadding: 8
                    }
                }
            }

            // 导出结果状态提示（信号驱动）
            Connections {
                target: exporter
                function onExportFinished(filePath) {
                    exportStatus.text = qsTr("已导出到 %1").arg(filePath)
                    exportStatus.color = theme.secondary
                }
                function onExportFailed(message) {
                    exportStatus.text = message
                    exportStatus.color = "#F0B429"
                }
            }

            // ---- 关于（任务 6.1，M4；04 号文档 §2.3）：只读软件信息 ----
            Label {
                text: qsTr("关于")
                color: theme.textDim
                font.pixelSize: 12
                font.letterSpacing: 1
            }
            Rectangle {
                width: parent.width
                height: aboutCol.implicitHeight + 24
                radius: 12
                color: theme.surface
                border.color: theme.border

                Column {
                    id: aboutCol
                    x: 20
                    y: 12
                    width: parent.width - 40
                    spacing: 8

                    // 版本号与 project(VERSION)/安装包同源（CMake version.h 注入）
                    Label {
                        text: "CursorFlow v" + settings.appVersion
                        color: theme.text
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: qsTr("基于 Qt %1 · %2")
                                  .arg(settings.qtVersion).arg(settings.buildInfo)
                        color: theme.textDim
                        font.pixelSize: 12
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("开源许可：本应用基于 MIT 许可证发布；Qt 框架在 LGPL v3 下使用。数据仅保存在本机，不联网。")
                        color: theme.textDim
                        font.pixelSize: 12
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("项目主页：<a href=\"https://github.com/ZefinNg/CursorFlow\">github.com/ZefinNg/CursorFlow</a>")
                        color: theme.textDim
                        font.pixelSize: 12
                        onLinkActivated: function (link) { Qt.openUrlExternally(link) }
                    }
                }
            }
        }
    }
}
