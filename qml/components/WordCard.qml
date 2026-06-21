import QtQuick 2.12
import QtGraphicalEffects 1.12

// 学习单词卡：单词、音标、释义和复习提示统一在高对比面板中。
Item {
    id: root
    property var theme
    property var card: ({})
    property bool revealDetails: true

    implicitHeight: col.height + 18

    function tagLabel(tag) {
        if (!tag) return "";
        var map = { "zk": "中考", "gk": "高考", "cet4": "四级", "cet6": "六级",
                    "ky": "考研", "toefl": "托福", "ielts": "雅思", "gre": "GRE" };
        var parts = ("" + tag).split(" ");
        var out = [];
        for (var i = 0; i < parts.length; i++)
            if (map[parts[i]]) out.push(map[parts[i]]);
        return out.join(" · ");
    }

    Rectangle {
        id: cardPanel
        anchors.fill: parent
        radius: 7
        color: theme ? theme.surface : "#141A22"
        border.width: 1
        border.color: theme ? theme.border : "#2B3646"
        clip: true
        layer.enabled: true
        layer.smooth: true
        layer.effect: OpacityMask {
            maskSource: Rectangle {
                width: cardPanel.width
                height: cardPanel.height
                radius: cardPanel.radius
            }
        }

        Rectangle {
            width: 80
            height: parent.height + 20
            anchors.right: parent.right
            anchors.top: parent.top
            color: theme ? theme.accentSoft : "#163733"
            opacity: 0.32
            rotation: -12
            transformOrigin: Item.Center
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            color: card.needsReview ? (theme ? theme.danger : "#FF7A7A")
                                    : (theme ? theme.accent : "#78D6C6")
        }
    }

    Column {
        id: col
        x: 12
        y: 8
        width: root.width - 24
        spacing: 5

        Row {
            width: parent.width
            spacing: 6
            Rectangle {
                visible: root.tagLabel(card.tag) !== ""
                radius: 6
                color: theme ? theme.accentSoft : "#163733"
                height: typeText.height + 5
                width: typeText.width + 10
                Text {
                    id: typeText
                    anchors.centerIn: parent
                    text: root.tagLabel(card.tag)
                    color: theme ? theme.accent : "#78D6C6"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
            Rectangle {
                visible: card.isReview === true
                radius: 6
                color: theme ? theme.surfaceGlass : "#202C39"
                height: rvText.height + 5
                width: rvText.width + 10
                Text {
                    id: rvText
                    anchors.centerIn: parent
                    text: "回看"
                    color: theme ? theme.textSecondary : "#A9B7C3"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 9
                }
            }
            Rectangle {
                visible: card.needsReview === true
                radius: 6
                color: theme ? theme.dangerSoft : "#3B2226"
                height: reinforceText.height + 5
                width: reinforceText.width + 10
                Text {
                    id: reinforceText
                    anchors.centerIn: parent
                    text: card.reinforceLevel >= 2 ? "重点再看" : "再确认"
                    color: theme ? theme.danger : "#FF7A7A"
                    font.family: theme ? theme.fontFamily : "sans-serif"
                    font.pixelSize: 9
                    font.bold: true
                }
            }
        }

        Text {
            width: parent.width
            text: card.word ? card.word : ""
            color: theme ? theme.textPrimary : "#F4F7F8"
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 28
            font.bold: true
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 19
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Text {
            visible: !!card.phonetic
            text: card.phonetic ? "[" + card.phonetic + "]" : ""
            color: theme ? theme.accentAlt : "#F0B45A"
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 12
        }

        Text {
            width: parent.width
            visible: root.revealDetails && !!card.translation
            text: card.translation ? card.translation : ""
            color: theme ? theme.textPrimary : "#F4F7F8"
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 13
            wrapMode: Text.Wrap
            lineHeight: 1.16
            opacity: root.revealDetails ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        }

        Text {
            width: parent.width
            visible: root.revealDetails && !!card.exchange
            text: card.exchange ? "变形  " + card.exchange : ""
            color: theme ? theme.textFaint : "#647482"
            font.family: theme ? theme.fontFamily : "sans-serif"
            font.pixelSize: 10
            wrapMode: Text.Wrap
        }
    }
}
