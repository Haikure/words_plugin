import QtQuick 2.12

// 细长进度条。value 取 0..1。
Item {
    id: root
    property var theme
    property real value: 0
    property color fillColor: theme ? theme.accent : "#78D6C6"
    property color trackColor: theme ? theme.surfaceGlass : "#202C39"
    implicitHeight: 5
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.trackColor
    }
    Rectangle {
        height: parent.height
        radius: height / 2
        color: root.fillColor
        width: Math.max(0, Math.min(1, root.value)) * parent.width
        Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.max(1, parent.height / 2)
            radius: height / 2
            color: "#FFFFFF"
            opacity: 0.22
        }
    }
}
