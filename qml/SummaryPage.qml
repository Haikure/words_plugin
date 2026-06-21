import QtQuick 2.12
import QtGraphicalEffects 1.12
import "components"

Flickable {
    id: root
    property var theme
    signal done()

    readonly property var summary: wordController ? wordController.currentSummary : ({})
    readonly property real accuracy: summary.accuracy ? summary.accuracy : 0
    readonly property int accuracyPercent: Math.round(accuracy * 100)
    readonly property bool isStudySummary: summary.kind === "study"

    contentWidth: width
    contentHeight: Math.max(height + 1, col.height + 14)
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    Column {
        id: col
        x: 10
        y: 7
        width: root.width - 20
        spacing: 6

        TopBar {
            theme: root.theme
            width: parent.width
            title: summary.title ? summary.title : "本轮完成"
            rightText: (summary.due ? summary.due : 0) + " 个待复习"
            onBack: root.done()
        }

        Rectangle {
            id: summaryCard
            width: parent.width
            height: 64
            radius: 7
            color: theme ? theme.surface : "#141A22"
            border.width: 1
            border.color: theme ? theme.border : "#2B3646"
            clip: true
            layer.enabled: true
            layer.smooth: true
            layer.effect: OpacityMask {
                maskSource: Rectangle {
                    width: summaryCard.width
                    height: summaryCard.height
                    radius: summaryCard.radius
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                color: root.accuracy >= 0.8
                       ? (theme ? theme.success : "#8CD879")
                       : (theme ? theme.accentAlt : "#F0B45A")
            }

            Rectangle {
                width: 90
                height: parent.height + 22
                anchors.right: parent.right
                color: theme ? theme.accentSoft : "#163733"
                opacity: 0.34
                rotation: -10
            }

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text {
                    text: root.isStudySummary
                          ? ("完成 " + (summary.total ? summary.total : 0) + " 个新词")
                          : ("完成 " + (summary.total ? summary.total : 0) + " 个复习词")
                    color: theme ? theme.textPrimary : "#F4F7F8"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 19
                    font.bold: true
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: 15
                    width: parent.width - 92
                    elide: Text.ElideRight
                }
                Text {
                    text: root.isStudySummary
                          ? "学得不错，接着进入下一次学习或复习吧"
                          : ((summary.correct ? summary.correct : 0) + " 个答对，" +
                             (summary.wrong ? summary.wrong : 0) + " 个答错")
                    color: theme ? theme.textSecondary : "#A9B7C3"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 10
                    width: parent.width - 92
                    wrapMode: Text.Wrap
                }
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: root.isStudySummary
                      ? ((summary.due ? summary.due : 0) + " 待复习")
                      : (root.accuracyPercent + "%")
                color: root.isStudySummary
                       ? (theme ? theme.accent : "#78D6C6")
                       : (root.accuracy >= 0.8
                          ? (theme ? theme.success : "#8CD879")
                          : (theme ? theme.accentAlt : "#F0B45A"))
                font.family: theme ? theme.fontFamily : "sans-serif"
                font.pixelSize: 12
                font.bold: true
            }

            ProgressBar {
                theme: root.theme
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 7
                value: root.isStudySummary
                       ? Math.min(1, (summary.due ? summary.due : 0) > 0 ? 1 : 0.35)
                       : root.accuracy
                fillColor: root.isStudySummary
                           ? (theme ? theme.accent : "#78D6C6")
                           : (root.accuracy >= 0.8
                              ? (theme ? theme.success : "#8CD879")
                              : (theme ? theme.accentAlt : "#F0B45A"))
            }
        }

        Row {
            width: parent.width
            spacing: 6
            StatTile {
                theme: root.theme
                width: (parent.width - 12) / 3
                height: 38
                value: "" + (summary.total ? summary.total : 0)
                label: root.isStudySummary ? "新词" : "复习词"
            }
            StatTile {
                theme: root.theme
                width: (parent.width - 12) / 3
                height: 38
                value: "" + (summary.due ? summary.due : 0)
                label: "待复习"
                accentColor: theme ? theme.accentAlt : "#F0B45A"
            }
            StatTile {
                theme: root.theme
                width: (parent.width - 12) / 3
                height: 38
                value: root.isStudySummary
                       ? "" + (summary.correct ? summary.correct : 0)
                       : "" + root.accuracyPercent
                label: root.isStudySummary ? "巩固答对" : "正确率"
                accentColor: root.isStudySummary
                             ? (theme ? theme.success : "#8CD879")
                             : (root.accuracy >= 0.8
                                ? (theme ? theme.success : "#8CD879")
                                : (theme ? theme.accent : "#78D6C6"))
            }
        }

        PillButton {
            theme: root.theme
            width: parent.width
            primary: true
            text: "完成"
            onClicked: root.done()
        }

        Item { width: 1; height: 3 }
    }
}
