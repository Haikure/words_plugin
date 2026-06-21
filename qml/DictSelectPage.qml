import QtQuick 2.12
import QtGraphicalEffects 1.12
import "components"

// 词库选择：卡片式列表，点选即切换当前词库。
Item {
    id: root
    property var theme
    signal back()

    function reload() { listView.model = wordController ? wordController.dictList() : []; }
    Component.onCompleted: reload()

    ListView {
        id: listView
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        footer: Item { width: 1; height: 8 }

        header: Column {
            width: listView.width
            spacing: 6
            TopBar {
                theme: root.theme
                width: parent.width
                title: "选择词库"
                rightText: listView.count + " 本"
                onBack: root.back()
            }
        }

        delegate: Rectangle {
            id: dictItem
            width: listView.width
            height: 56
            radius: 7
            color: theme ? theme.surface : "#141A22"
            border.width: modelData.current ? 2 : 1
            border.color: modelData.current ? (theme ? theme.accent : "#78D6C6")
                                            : (theme ? theme.border : "#2B3646")
            clip: true
            layer.enabled: true
            layer.smooth: true
            layer.effect: OpacityMask {
                maskSource: Rectangle {
                    width: dictItem.width
                    height: dictItem.height
                    radius: dictItem.radius
                }
            }
            scale: dictArea.pressed ? 0.985 : (dictArea.containsMouse ? 1.01 : 1.0)

            Behavior on border.color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                color: modelData.current ? (theme ? theme.accent : "#78D6C6")
                                         : (theme ? theme.surfaceGlass : "#202C39")
            }

            Column {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 10
                anchors.topMargin: 7
                anchors.bottomMargin: 7
                spacing: 4

                Row {
                    width: parent.width
                    spacing: 6
                    Text {
                        width: parent.width - (modelData.current ? currentBadge.width + 8 : 0)
                        text: modelData.name
                        color: theme ? theme.textPrimary : "#F4F7F8"
                        font.family: theme ? theme.fontFamily : "sans-serif"
                        font.pixelSize: 12
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        id: currentBadge
                        visible: modelData.current
                        width: currentText.width + 10
                        height: currentText.height + 5
                        radius: 6
                        color: theme ? theme.accentSoft : "#163733"
                        Text {
                            id: currentText
                            anchors.centerIn: parent
                            text: "当前"
                            color: theme ? theme.accent : "#78D6C6"
                            font.family: theme ? theme.fontFamily : "sans-serif"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }
                ProgressBar {
                    theme: root.theme
                    width: parent.width
                    value: modelData.progress
                }
                Text {
                    text: "已学 " + modelData.learnedCount + " / " + modelData.wordCount
                    color: theme ? theme.textSecondary : "#A9B7C3"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 9
                }
            }

            MouseArea {
                id: dictArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (wordController && wordController.selectDict(modelData.dictId)) {
                        root.reload();
                        root.back();
                    }
                }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: 72
            radius: 8
            visible: listView.count === 0
            color: theme ? theme.surface : "#141A22"
            border.width: 1
            border.color: theme ? theme.border : "#2B3646"
            Text {
                anchors.centerIn: parent
                text: "未发现词库\n请将 .db 放入 dicts/ 目录"
                horizontalAlignment: Text.AlignHCenter
                color: theme ? theme.textSecondary : "#A9B7C3"
                font.family: theme ? theme.fontFamily : "sans-serif"
                font.pixelSize: 12
                lineHeight: 1.2
            }
        }
    }
}
