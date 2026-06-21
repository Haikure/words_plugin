import QtQuick 2.12
import QtGraphicalEffects 1.12

// 复习四选一选项按钮。由显式布尔输入决定反馈状态，避免 delegate 字符串状态丢绑定。
Rectangle {
    id: root
    property var theme
    property string text: ""
    // 标绿=本题点中的正确项；标红=被点错的项（可多个）；变暗=点对后其余未选项。
    property bool markedCorrect: false
    property bool markedWrong: false
    property bool faded: false
    readonly property bool selectedCorrect: markedCorrect
    readonly property bool wrong: markedWrong
    readonly property bool dimmed: faded && !markedCorrect && !markedWrong
    signal clicked()

    implicitHeight: Math.max(32, optionText.paintedHeight + 14)
    height: implicitHeight
    radius: 7
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

    color: {
        if (!theme) return "#1B2430";
        if (root.selectedCorrect) return theme.successSoft;
        if (root.wrong) return theme.dangerSoft;
        if (root.dimmed) return theme.surface;
        return theme.surfaceRaised;
    }
    border.width: (root.selectedCorrect || root.wrong) ? 2 : 1
    border.color: {
        if (!theme) return "#2B3646";
        if (root.selectedCorrect) return theme.success;
        if (root.wrong) return theme.danger;
        return theme.border;
    }
    scale: pressArea.pressed ? 0.985 : (pressArea.containsMouse && root.enabled ? 1.01 : 1.0)

    Behavior on color { ColorAnimation { duration: 280; easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: 280; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 4
        color: {
            if (root.selectedCorrect) return theme ? theme.success : "#8CD879";
            if (root.wrong) return theme ? theme.danger : "#FF7A7A";
            return theme ? theme.accent : "#78D6C6";
        }
        opacity: (root.selectedCorrect || root.wrong) ? 1 : 0.38
    }

    Text {
        id: optionText
        anchors.left: parent.left
        anchors.right: markIcon.left
        anchors.leftMargin: 10
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: {
            if (!theme) return "#F4F7F8";
            if (root.selectedCorrect) return theme.success;
            if (root.wrong) return theme.danger;
            if (root.dimmed) return theme.textFaint;
            return theme.textPrimary;
        }
        font.family: theme ? theme.fontFamily : "sans-serif"
        font.pixelSize: 12
        wrapMode: Text.Wrap

        Behavior on color { ColorAnimation { duration: 280; easing.type: Easing.OutCubic } }
    }

    Text {
        id: markIcon
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.selectedCorrect ? "✓" : (root.wrong ? "×" : "")
        color: root.selectedCorrect
               ? (theme ? theme.success : "#8CD879")
               : (theme ? theme.danger : "#FF7A7A")
        font.pixelSize: 13
        font.bold: true

        Behavior on color { ColorAnimation { duration: 280; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: pressArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
