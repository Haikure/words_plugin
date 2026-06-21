import QtQuick 2.12

// 主按钮 / 次按钮。primary=true 时高对比填充，否则低调描边。
Rectangle {
    id: root
    property var theme
    property string text: ""
    property bool primary: false
    property bool enabled: true
    property bool selected: false
    signal clicked()

    implicitHeight: 30
    height: implicitHeight
    radius: 7
    opacity: (enabled || selected) ? 1.0 : 0.4

    color: {
        if (!theme) return primary ? "#78D6C6" : (selected ? "#163733" : "#1B2430");
        if (primary) return theme.accent;
        if (selected) return theme.accentSoft;
        return theme.surfaceRaised;
    }
    border.width: primary ? 0 : 1
    border.color: selected ? (theme ? theme.accent : "#78D6C6")
                           : (theme ? theme.border : "#2B3646")

    Behavior on color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: 180; easing.type: Easing.OutCubic } }

    Text {
        anchors.centerIn: parent
        text: root.text
        color: {
            if (root.primary) return "#07110F";
            if (root.selected) return theme ? theme.accent : "#78D6C6";
            return theme ? theme.textPrimary : "#F4F7F8";
        }
        font.family: theme ? theme.fontFamily : "sans-serif"
        font.pixelSize: 12
        font.bold: root.primary || root.selected
    }

    scale: pressArea.pressed ? 0.96 : (pressArea.containsMouse ? 1.02 : 1.0)
    Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }

    MouseArea {
        id: pressArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
