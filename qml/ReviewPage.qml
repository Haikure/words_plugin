import QtQuick 2.12
import "components"

// 复习页：题干 + 四选一，点错标红留在本题，点对后自动进入下一题。
Flickable {
    id: root
    property var theme
    signal back()

    readonly property var q: wordController ? wordController.currentQuestion : ({})
    // 一题可多次作答：点错标红并保留题目，直到点中正确项才进入下一题。
    property int firstPick: -1    // 首次作答项：仅此一次交给后端结算复习调度
    property var wrongPicks: []   // 已点错的项索引（标红，可多个）
    property int correctPick: -1  // 点中正确项的索引；>=0 表示本题完成
    readonly property bool resolved: correctPick >= 0

    Connections {
        target: wordController
        ignoreUnknownSignals: true
        function onCurrentQuestionChanged() {
            root.firstPick = -1;
            root.wrongPicks = [];
            root.correctPick = -1;
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
        if (root.correctPick >= 0) return;             // 已点对，本题锁定
        if (root.wrongPicks.indexOf(idx) >= 0) return; // 该项已标红，忽略重复点击

        var correct;
        if (root.firstPick < 0) {
            // 首次作答：交给后端结算长期复习状态与计数，并以其判定为准。
            root.firstPick = idx;
            correct = wordController ? wordController.answerReview(idx)
                                     : (idx === root.q.answerIndex);
        } else {
            // 后续作答：本题已结算，仅在前端判断对错，引导用户点到对为止。
            correct = (idx === root.q.answerIndex);
        }

        if (correct) {
            root.correctPick = idx;
            feedbackTimer.start();      // 展示正确反馈后进入下一题
        } else {
            var arr = root.wrongPicks.slice();
            arr.push(idx);
            root.wrongPicks = arr;      // 重新赋值以触发选项重新着色
        }
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
            title: q.isConsolidate ? "巩固" : (q.isRandomReview ? "随机复习" : "复习")
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
                    enabled: !root.resolved && root.wrongPicks.indexOf(index) < 0
                    markedCorrect: index === root.correctPick
                    markedWrong: root.wrongPicks.indexOf(index) >= 0
                    faded: root.resolved
                    onClicked: root.choose(index)
                }
            }
        }

        Text {
            width: parent.width
            visible: root.firstPick >= 0
            text: root.resolved ? "答对了，继续保持节奏。" : "答错了，再试一次。"
            color: root.resolved
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
