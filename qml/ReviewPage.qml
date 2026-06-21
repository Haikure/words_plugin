import QtQuick 2.12
import "components"

// 复习页：题干 + 四选一，答后即时反馈并自动进入下一题。
Flickable {
    id: root
    property var theme
    signal back()

    readonly property var q: wordController ? wordController.currentQuestion : ({})
    readonly property bool answeredCorrect: answered >= 0 && answered === q.answerIndex
    property int answered: -1

    Connections {
        target: wordController
        ignoreUnknownSignals: true
        function onCurrentQuestionChanged() {
            root.answered = -1;
            feedbackTimer.stop();
            scrollTopAnim.stop();
            scrollTopAnim.from = root.contentY;
            scrollTopAnim.to = 0;
            scrollTopAnim.start();
        }
    }

    NumberAnimation {
        id: scrollTopAnim
        target: root
        property: "contentY"
        duration: 360
        easing.type: Easing.OutCubic
    }

    Timer {
        id: feedbackTimer
        interval: 850
        onTriggered: if (wordController) wordController.advanceReview()
    }

    function choose(idx) {
        if (root.answered >= 0) return;
        root.answered = idx;
        if (wordController) wordController.answerReview(idx);
        feedbackTimer.start();
    }

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
            title: q.isConsolidate ? "巩固" : "复习"
            rightText: (q.index ? q.index : 0) + " / " + (q.total ? q.total : 0)
            onBack: root.back()
        }

        Rectangle {
            id: promptCard
            width: parent.width
            height: Math.max(50, promptCol.height + 16)
            radius: 7
            color: theme ? theme.surface : "#141A22"
            border.width: 1
            border.color: theme ? theme.border : "#2B3646"
            clip: true

            Rectangle {
                width: 78
                height: parent.height + 12
                anchors.right: parent.right
                color: theme ? theme.accentSoft : "#163733"
                opacity: 0.34
                rotation: -10
            }

            Column {
                id: promptCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    width: parent.width
                    text: q.prompt ? q.prompt : ""
                    color: theme ? theme.textPrimary : "#F4F7F8"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: q.askEnToCn ? 22 : 15
                    font.bold: q.askEnToCn === true
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: q.askEnToCn ? 17 : 12
                    wrapMode: Text.Wrap
                    maximumLineCount: q.askEnToCn ? 2 : 3
                    elide: Text.ElideRight
                }
                Text {
                    visible: !!q.promptPhonetic
                    text: q.promptPhonetic ? "[" + q.promptPhonetic + "]" : ""
                    color: theme ? theme.accentAlt : "#F0B45A"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 11
                }
            }
        }

        Column {
            width: parent.width
            spacing: 6

            Repeater {
                model: q.options ? q.options : []
                delegate: OptionButton {
                    theme: root.theme
                    width: parent.width
                    height: implicitHeight
                    text: modelData
                    enabled: root.answered < 0
                    revealed: root.answered >= 0
                    selected: index === root.answered
                    correct: index === q.answerIndex
                    onClicked: root.choose(index)
                }
            }
        }

        Text {
            width: parent.width
            visible: root.answered >= 0
            text: root.answeredCorrect ? "答对了，继续保持节奏。" : "答错了，再试一次。"
            color: root.answeredCorrect
                   ? (theme ? theme.success : "#8CD879")
                   : (theme ? theme.danger : "#FF7A7A")
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
        }

        ProgressBar {
            theme: root.theme
            width: parent.width
            value: (q.total && q.total > 0) ? ((q.index ? q.index : 0) / q.total) : 0
            fillColor: theme ? theme.accent : "#78D6C6"
        }

        Item { width: 1; height: 3 }
    }
}
