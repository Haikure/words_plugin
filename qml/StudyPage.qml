import QtQuick 2.12
import "components"

// 学习页：先回忆，再用二选一判断揭示释义。
Flickable {
    id: root
    property var theme
    signal back()

    readonly property var card: wordController ? wordController.currentCard : ({})
    property bool revealed: false
    property int selectedRating: -1

    Connections {
        target: wordController
        ignoreUnknownSignals: true
        function onCurrentCardChanged() {
            root.revealed = false;
            root.selectedRating = -1;
        }
    }

    contentWidth: width
    contentHeight: Math.max(height + 1, col.height + 14)
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    function nextCard() {
        if (!wordController) return;
        scrollTopAnim.stop();
        scrollTopAnim.from = root.contentY;
        scrollTopAnim.to = 0;
        scrollTopAnim.start();
    }

    function chooseRating(rating) {
        if (root.selectedRating >= 0) return;
        root.selectedRating = rating;
        root.revealed = true;
    }

    function submitRating() {
        if (!wordController) return;
        if (root.selectedRating < 0) return;
        root.nextCard();
        wordController.rateStudy(root.selectedRating);
    }

    NumberAnimation {
        id: scrollTopAnim
        target: root
        property: "contentY"
        duration: 360
        easing.type: Easing.OutCubic
    }

    Column {
        id: col
        x: 10
        y: 7
        width: root.width - 20
        spacing: 6

        TopBar {
            theme: root.theme
            width: parent.width
            title: card.needsReview ? "再看一眼" : "学习"
            rightText: (card.index ? card.index : 0) + " / " + (card.total ? card.total : 0)
            onBack: root.back()
        }

        WordCard {
            id: wordCard
            theme: root.theme
            card: root.card
            revealDetails: root.revealed
            width: parent.width
            height: implicitHeight
        }

        ProgressBar {
            theme: root.theme
            width: parent.width
            value: (card.total && card.total > 0) ? (card.index / card.total) : 0
            fillColor: theme ? theme.accent : "#78D6C6"
        }

        Text {
            width: parent.width
            text: card.needsReview
                  ? (root.revealed ? "核对释义后，按刚才的真实记忆判断。" : "这个词刚卡住过，再独立回忆一次。")
                  : (root.revealed ? "核对释义后，判断刚才有没有记对。" : "先回忆释义，再做判断。")
            color: theme ? theme.textSecondary : "#A9B7C3"
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 10
            wrapMode: Text.Wrap
            lineHeight: 1.15
        }

        Row {
            width: parent.width
            spacing: 6

            PillButton {
                theme: root.theme
                width: (parent.width - 6) / 2
                text: "不认识"
                selected: root.selectedRating === 1
                enabled: root.selectedRating < 0
                onClicked: root.chooseRating(1)
            }

            PillButton {
                theme: root.theme
                width: (parent.width - 6) / 2
                text: "认识"
                selected: root.selectedRating === 0
                enabled: root.selectedRating < 0
                onClicked: root.chooseRating(0)
            }
        }

        PillButton {
            theme: root.theme
            width: parent.width
            visible: root.revealed
            primary: true
            enabled: root.selectedRating >= 0
            text: root.card && root.card.isLast ? "进入巩固" : "继续"
            onClicked: root.submitRating()
        }

        Item { width: 1; height: 3 }
    }
}
