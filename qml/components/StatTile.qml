import QtQuick 2.12
import QtGraphicalEffects 1.12

// 统计小卡：强调数字，顶部色条建立可扫读层次。
Rectangle {
    id: root
    property var theme
    property string value: "0"
    property string label: ""
    property color accentColor: theme ? theme.accent : "#78D6C6"

    radius: 7
    color: theme ? theme.surface : "#141A22"
    border.width: 1
    border.color: theme ? theme.border : "#2B3646"
    clip: true
    layer.enabled: true
    layer.smooth: true
    layer.effect: OpacityMask {
        maskSource: Rectangle {
            width: root.width
            height: root.height
            radius: root.radius
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 2
        color: root.accentColor
        opacity: 0.92
    }

    Column {
        anchors.centerIn: parent
        spacing: 1
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.value
            color: root.accentColor
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 18
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.label
            color: theme ? theme.textSecondary : "#A9B7C3"
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 9
        }
    }
}
