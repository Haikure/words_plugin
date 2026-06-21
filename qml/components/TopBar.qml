import QtQuick 2.12

Item {
    id: root
    property var theme
    property string title: ""
    property string rightText: ""
    property bool showBack: true
    signal back()

    implicitHeight: 24
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        radius: 7
        color: theme ? theme.surfaceGlass : "#202C39"
        opacity: 0.78
        border.width: 1
        border.color: theme ? theme.border : "#2B3646"
    }

    Text {
        id: backIcon
        visible: root.showBack
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: "‹"
        color: theme ? theme.textPrimary : "#F4F7F8"
        font.family: theme ? theme.fontFamily : "sans-serif"
        font.pixelSize: 16
        font.bold: true
    }

    Text {
        anchors.left: root.showBack ? backIcon.right : parent.left
        anchors.leftMargin: root.showBack ? 5 : 8
        anchors.right: rightLabel.visible ? rightLabel.left : parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        color: theme ? theme.textPrimary : "#F4F7F8"
        font.family: theme ? theme.fontFamily : "sans-serif"
        font.pixelSize: 12
        font.bold: true
        elide: Text.ElideRight
    }

    Text {
        id: rightLabel
        visible: root.rightText !== ""
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.rightText
        color: theme ? theme.accent : "#78D6C6"
        font.family: theme ? theme.fontFamily : "sans-serif"
        font.pixelSize: 10
        font.bold: true
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 38
        enabled: root.showBack
        onClicked: root.back()
    }
}
