import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

// 首次启动引导（任务 6.2，M4；04 号文档 §2.4；PRD 3.6）：
// 隐私要点 → 屏幕尺寸设置（复用校准项，可跳过）→ 完成提示。
// 由 Main.qml 在隐私同意且 settings.onboarded=false 时阻断加载。
Dialog {
    id: dialog

    modal: true
    closePolicy: Popup.NoAutoClose
    anchors.centerIn: parent
    width: 520
    title: step === 0 ? qsTr("欢迎使用 CursorFlow")
                      : step === 1 ? qsTr("设置屏幕尺寸") : qsTr("设置完成")

    property int step: 0

    // 完成或跳过：写入标记并向导自动关闭（Main.qml 无需额外处理）
    function finish() {
        settings.onboarded = true
        close()
    }

    background: Rectangle {
        color: theme.surface
        radius: 10
        border.color: theme.border
    }

    contentItem: Column {
        width: dialog.availableWidth
        spacing: 14

        // ---- 第 1 步：欢迎 + 隐私要点 ----
        Label {
            visible: dialog.step === 0
            width: parent.width
            wrapMode: Text.WordWrap
            color: theme.text
            font.pixelSize: 13
            text: qsTr("只需 10 秒完成初始设置。CursorFlow 仅统计鼠标移动距离与按键、点击、滚动的次数，不记录任何按键内容，所有数据仅保存在本机。")
        }

        // ---- 第 2 步：屏幕尺寸设置（与设置页校准组同一数据源）----
        Column {
            visible: dialog.step === 1
            width: parent.width
            spacing: 10

            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                color: theme.textDim
                font.pixelSize: 12
                text: qsTr("填写显示器对角线尺寸与分辨率，让“鼠标距离”按真实物理米数换算；不设置则按 96 PPI 估算。")
            }

            Row {
                spacing: 8
                Label {
                    text: qsTr("对角线尺寸（英寸）")
                    color: theme.text
                    font.pixelSize: 13
                    anchors.verticalCenter: parent.verticalCenter
                }
                SpinBox {
                    id: inchBox
                    from: 5
                    to: 200
                    value: settings.calibrationDiagonalInch > 0
                               ? Math.round(settings.calibrationDiagonalInch) : 24
                }
            }

            Row {
                spacing: 8
                Label {
                    text: qsTr("分辨率（宽 × 高）")
                    color: theme.text
                    font.pixelSize: 13
                    anchors.verticalCenter: parent.verticalCenter
                }
                SpinBox {
                    id: widthBox
                    from: 640
                    to: 15360
                    // 预填当前窗口所在屏幕分辨率（与设置页校准组一致的预填策略）
                    value: Math.round(Screen.width * Screen.devicePixelRatio)
                }
                Label {
                    text: "×"
                    color: theme.textDim
                    anchors.verticalCenter: parent.verticalCenter
                }
                SpinBox {
                    id: heightBox
                    from: 480
                    to: 8640
                    value: Math.round(Screen.height * Screen.devicePixelRatio)
                }
            }

            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                color: theme.textDim
                font.pixelSize: 11
                text: qsTr("换算结果：%1 PPI（当前系统屏幕：%2 × %3）")
                          .arg(suggestPpi()).arg(Screen.width).arg(Screen.height)
            }
        }

        // ---- 第 3 步：完成提示（PRD 3.6 明确要求引导用户可回设置修改）----
        Label {
            visible: dialog.step === 2
            width: parent.width
            wrapMode: Text.WordWrap
            color: theme.text
            font.pixelSize: 13
            text: qsTr("屏幕尺寸信息已保存。你可以随时在 设置 → 校准 中修改，未校准时鼠标距离按 96 PPI 估算。")
        }
    }

    // 屏幕参考：实际像素尺寸（QML Screen 附加属性以窗口所在屏幕为准）
    function suggestPpi() {
        const diagonalPx = Math.sqrt(Screen.width * Screen.width + Screen.height * Screen.height)
        return Math.round(diagonalPx / Math.max(1, inchBox.value))
    }

    footer: DialogButtonBox {
        Button {
            visible: dialog.step === 1
            flat: true
            text: qsTr("跳过")
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialog.finish()
        }
        Button {
            visible: dialog.step === 1
            flat: true
            text: qsTr("上一步")
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialog.step -= 1
        }
        Button {
            visible: dialog.step < 2
            // 向导中间步骤必须用 ActionRole：AcceptRole 会让 Dialog 自动 accept()
            // 并关闭弹窗，导致第 2、3 步永远无法进入。
            text: qsTr("下一步")
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: {
                if (dialog.step === 1) {
                    settings.setCalibration(inchBox.value, widthBox.value, heightBox.value)
                }
                dialog.step += 1
            }
        }
        Button {
            visible: dialog.step === 2
            text: qsTr("完成")
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialog.finish()
        }
    }
}
