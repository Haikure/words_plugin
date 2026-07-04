import QtQuick 2.12
import QtGraphicalEffects 1.12
import "components"

// 主页：主操作优先，统计与词库进度保持在同一视觉系统内。
Flickable {
    id: root
    property var theme

    signal requestStudy()
    signal requestReview()
    signal requestDictSelect()
    signal requestExit()

    readonly property var batchOptions: [5, 10, 15, 20, 25, 30]
    readonly property var reviewModeOptions: [
        { "value": 0, "label": "混合" },
        { "value": 1, "label": "中选英" },
        { "value": 2, "label": "英选中" }
    ]
    readonly property int learnedCount: wordController ? wordController.learnedCount : 0
    readonly property int totalWords: wordController ? wordController.totalWords : 0
    readonly property int dueCount: wordController ? wordController.dueCount : 0
    readonly property real dictProgress: totalWords > 0 ? learnedCount / totalWords : 0

    contentWidth: width
    contentHeight: Math.max(height + 1, col.height + 16)
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    Column {
        id: col
        x: 10
        y: 7
        width: parent.width - 20
        spacing: 6

        TopBar {
            theme: root.theme
            width: parent.width
            title: "背单词"
            rightText: "连续 " + (wordController ? wordController.streakDays : 0) + " 天"
            onBack: root.requestExit()
        }

        Rectangle {
            id: dictCard
            width: parent.width
            height: 66
            radius: 7
            color: theme ? theme.surface : "#141A22"
            border.width: 1
            border.color: theme ? theme.border : "#2B3646"
            clip: true
            layer.enabled: true
            layer.smooth: true
            layer.effect: OpacityMask {
                maskSource: Rectangle {
                    width: dictCard.width
                    height: dictCard.height
                    radius: dictCard.radius
                }
            }
            scale: dictArea.pressed ? 0.985 : (dictArea.containsMouse ? 1.01 : 1.0)
            Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                color: theme ? theme.accent : "#78D6C6"
            }

            Rectangle {
                width: 86
                height: parent.height
                anchors.right: parent.right
                color: theme ? theme.accentSoft : "#163733"
                opacity: 0.38
                rotation: -10
                transformOrigin: Item.Center
            }

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.top: parent.top
                anchors.topMargin: 7
                spacing: 1

                Text {
                    text: "当前词库"
                    color: theme ? theme.accent : "#78D6C6"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 9
                    font.bold: true
                }
                Text {
                    width: parent.width
                    text: wordController ? wordController.currentDictName : "未选择词库"
                    color: theme ? theme.textPrimary : "#F4F7F8"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 16
                    font.bold: true
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 13
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: "已学 " + root.learnedCount + " / " + root.totalWords + "    待复习 " + root.dueCount
                    color: theme ? theme.textSecondary : "#A9B7C3"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
            }

            ProgressBar {
                theme: root.theme
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 7
                value: root.dictProgress
                fillColor: theme ? theme.accent : "#78D6C6"
            }

            MouseArea {
                id: dictArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.requestDictSelect()
            }
        }

        Row {
            width: parent.width
            spacing: 6
            PillButton {
                theme: root.theme
                width: (parent.width - 6) / 2
                primary: true
                text: "开始学习"
                onClicked: root.requestStudy()
            }
            PillButton {
                theme: root.theme
                width: (parent.width - 6) / 2
                text: root.dueCount > 0 ? "开始复习"
                      : (root.learnedCount > 0 ? "随机复习" : "开始复习")
                onClicked: root.requestReview()
            }
        }

        Item {
            width: parent.width
            height: 12
            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: "每轮词数"
                color: theme ? theme.textSecondary : "#A9B7C3"
                font.family: theme ? theme.fontFamily : "sans-serif"
                font.pixelSize: 10
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: (wordController ? wordController.batchSize : 0) + " 个"
                color: theme ? theme.accentAlt : "#F0B45A"
                font.family: theme ? theme.fontFamily : "sans-serif"
                font.pixelSize: 10
                font.bold: true
            }
        }

        Flow {
            width: parent.width
            spacing: 6
            Repeater {
                model: root.batchOptions
                delegate: Rectangle {
                    id: batchChip
                    property bool sel: wordController && wordController.batchSize === modelData
                    width: 39
                    height: 24
                    radius: 6
                    color: sel ? (theme ? theme.accent : "#78D6C6")
                               : (theme ? theme.surfaceRaised : "#1B2430")
                    border.width: sel ? 0 : 1
                    border.color: theme ? theme.border : "#2B3646"
                    scale: chipArea.pressed ? 0.94 : (chipArea.containsMouse ? 1.05 : 1.0)

                    Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: parent.sel ? "#07110F" : (theme ? theme.textPrimary : "#F4F7F8")
                        font.family: theme ? theme.fontFamily : "sans-serif"
                        font.pixelSize: 12
                        font.bold: parent.sel
                    }

                    MouseArea {
                        id: chipArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: if (wordController) wordController.setBatchSize(modelData)
                    }
                }
            }
        }

        Rectangle {
            id: prefsCard
            width: parent.width
            height: 88
            radius: 7
            color: theme ? theme.surface : "#141A22"
            border.width: 1
            border.color: theme ? theme.border : "#2B3646"
            clip: true

            Rectangle {
                width: 74
                height: parent.height + 10
                anchors.right: parent.right
                anchors.top: parent.top
                color: theme ? theme.accentAltSoft : "#3A2C18"
                opacity: 0.38
                rotation: -9
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5

                Row {
                    width: parent.width
                    height: 13

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "偏好设置"
                        color: theme ? theme.accentAlt : "#F0B45A"
                        font.family: theme ? theme.fontFamily : "sans-serif"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "选择模式"
                        color: theme ? theme.textSecondary : "#A9B7C3"
                        font.family: theme ? theme.fontFamily : "sans-serif"
                        font.pixelSize: 10
                    }
                }

                Row {
                    width: parent.width
                    spacing: 4

                    Repeater {
                        model: root.reviewModeOptions
                        delegate: Rectangle {
                            id: modeChip
                            property bool sel: wordController && wordController.reviewMode === modelData.value
                            width: (parent.width - 8) / 3
                            height: 24
                            radius: 6
                            color: sel ? (theme ? theme.accentAlt : "#F0B45A")
                                       : (theme ? theme.surfaceRaised : "#1B2430")
                            border.width: sel ? 0 : 1
                            border.color: theme ? theme.border : "#2B3646"
                            scale: modeArea.pressed ? 0.94 : (modeArea.containsMouse ? 1.04 : 1.0)

                            Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: parent.sel ? "#100B02" : (theme ? theme.textPrimary : "#F4F7F8")
                                font.family: theme ? theme.fontFamily : "sans-serif"
                                font.pixelSize: 11
                                font.bold: parent.sel
                            }

                            MouseArea {
                                id: modeArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: if (wordController) wordController.setReviewMode(modelData.value)
                            }
                        }
                    }
                }

                Row {
                    width: parent.width
                    height: 24
                    spacing: 6

                    Text {
                        width: parent.width - difficultySwitch.width - 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: "题目难度"
                        color: theme ? theme.textSecondary : "#A9B7C3"
                        font.family: theme ? theme.fontFamily : "sans-serif"
                        font.pixelSize: 10
                    }

                    Rectangle {
                        id: difficultySwitch
                        property bool hard: wordController && wordController.highDifficulty
                        width: 74
                        height: 24
                        radius: 6
                        color: hard ? (theme ? theme.dangerSoft : "#3B2226")
                                    : (theme ? theme.surfaceRaised : "#1B2430")
                        border.width: 1
                        border.color: hard ? (theme ? theme.danger : "#FF7A7A")
                                           : (theme ? theme.border : "#2B3646")
                        scale: difficultyArea.pressed ? 0.94 : (difficultyArea.containsMouse ? 1.04 : 1.0)

                        Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Behavior on border.color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

                        Text {
                            anchors.centerIn: parent
                            text: parent.hard ? "高难" : "普通"
                            color: parent.hard
                                   ? (theme ? theme.danger : "#FF7A7A")
                                   : (theme ? theme.textPrimary : "#F4F7F8")
                            font.family: theme ? theme.fontFamily : "sans-serif"
                            font.pixelSize: 11
                            font.bold: parent.hard
                        }

                        MouseArea {
                            id: difficultyArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: if (wordController) wordController.setHighDifficulty(!wordController.highDifficulty)
                        }
                    }
                }
            }
        }

        Row {
            width: parent.width
            spacing: 6
            StatTile {
                theme: root.theme
                width: (parent.width - 12) / 3
                height: 44
                value: "" + (wordController ? wordController.todayLearned : 0)
                label: "今日学习"
            }
            StatTile {
                theme: root.theme
                width: (parent.width - 12) / 3
                height: 44
                value: "" + (wordController ? wordController.todayReviewed : 0)
                label: "今日复习"
                accentColor: theme ? theme.success : "#8CD879"
            }
            StatTile {
                theme: root.theme
                width: (parent.width - 12) / 3
                height: 44
                value: "" + root.dueCount
                label: "待复习"
                accentColor: theme ? theme.accentAlt : "#F0B45A"
            }
        }

        Item { width: 1; height: 3 }
    }
}
