import QtQuick 2.12
import "components"

// 错词本：列出当前词库复习答错的词（最近答错的在前）。
// 点词条展开完整释义；底部按钮发起错词专项复习，连对 3 次自动移出。
Flickable {
    id: root
    property var theme
    signal back()
    signal requestErrorReview()

    property var words: []
    readonly property int wordCount: words.length

    function refresh() {
        words = wordController ? wordController.errorWordList() : [];
    }
    Component.onCompleted: refresh()
    Connections {
        target: wordController
        ignoreUnknownSignals: true
        function onStatsChanged() { root.refresh(); }
    }

    contentWidth: width
    contentHeight: Math.max(height + 1, col.height + 16)
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    Column {
        id: col
        x: 10
        y: 7
        width: parent.width - 20
        spacing: 6

        TopBar {
            theme: root.theme
            width: parent.width
            title: "错词本"
            rightText: root.wordCount > 0 ? root.wordCount + " 个词" : ""
            onBack: root.back()
        }

        // 空状态
        Item {
            width: parent.width
            height: Math.max(80, root.height * 0.6)
            visible: root.wordCount === 0
            Text {
                anchors.centerIn: parent
                horizontalAlignment: Text.AlignHCenter
                text: "错词本还是空的\n复习时答错的词会自动收进来"
                color: theme ? theme.textSecondary : "#A9B7C3"
                font.family: theme ? theme.fontFamily : "sans-serif"
                font.pixelSize: 11
                lineHeight: 1.4
            }
        }

        Repeater {
            model: root.words
            delegate: Rectangle {
                id: card
                property bool expanded: false
                readonly property bool halfKnown:
                    modelData.totalAttempts > 0 &&
                    modelData.totalCorrect * 2 >= modelData.totalAttempts

                width: parent.width
                height: contentCol.height + 16
                radius: 7
                color: theme ? theme.surface : "#141A22"
                border.width: 1
                border.color: theme ? theme.border : "#2B3646"
                clip: true

                Column {
                    id: contentCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    spacing: 3

                    Row {
                        width: parent.width
                        spacing: 8

                        Text {
                            width: parent.width - statText.width - parent.spacing
                            text: modelData.word
                            color: theme ? theme.textPrimary : "#F4F7F8"
                            font.family: theme ? theme.fontFamily : "sans-serif"
                            font.pixelSize: 14
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            id: statText
                            anchors.verticalCenter: parent.verticalCenter
                            text: "对 " + modelData.totalCorrect + " / " + modelData.totalAttempts
                            color: card.halfKnown
                                   ? (theme ? theme.success : "#8CD879")
                                   : (theme ? theme.danger : "#FF7A7A")
                            font.family: theme ? theme.fontFamily : "sans-serif"
                            font.pixelSize: 9
                        }
                    }

                    Text {
                        visible: !!modelData.phonetic
                        text: modelData.phonetic ? "[" + modelData.phonetic + "]" : ""
                        color: theme ? theme.accentAlt : "#F0B45A"
                        font.family: theme ? theme.fontFamily : "sans-serif"
                        font.pixelSize: 10
                    }

                    Text {
                        width: parent.width
                        text: modelData.translation
                        color: theme ? theme.textSecondary : "#A9B7C3"
                        font.family: theme ? theme.fontFamily : "sans-serif"
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                        maximumLineCount: card.expanded ? 100 : 2
                        elide: Text.ElideRight
                        lineHeight: 1.15
                    }

                    Text {
                        visible: card.expanded
                        width: parent.width
                        text: "点一下收起"
                        color: theme ? theme.textFaint : "#647482"
                        font.family: theme ? theme.fontFamily : "sans-serif"
                        font.pixelSize: 8
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: card.expanded = !card.expanded
                }
            }
        }

        PillButton {
            theme: root.theme
            width: parent.width
            primary: true
            visible: root.wordCount > 0
            text: "复习错词"
            onClicked: root.requestErrorReview()
        }

        Text {
            width: parent.width
            visible: root.wordCount > 0
            text: "每轮从错词本抽取 " + (wordController ? wordController.batchSize : 10) +
                  " 个；连对 3 次的词自动移出错词本。"
            color: theme ? theme.textFaint : "#647482"
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 9
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.2
        }

        Item { width: 1; height: 3 }
    }
}
