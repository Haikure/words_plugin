import QtQuick 2.12
import "qrc:/qml/commons"
import "components"

// 背单词插件根页面。固定 320×170，深色主题。
// 必须声明 backButtonClicked()，宿主据此销毁插件页（即退出插件）。
Rectangle {
    id: root
    width: 320
    height: 170
    radius: 8
    clip: true
    gradient: Gradient {
        GradientStop { position: 0.0; color: theme.bgAlt }
        GradientStop { position: 0.54; color: theme.bg }
        GradientStop { position: 1.0; color: "#070A0F" }
    }

    signal backButtonClicked()
    property string pluginName: ""

    // 主题
    Theme { id: theme }

    Rectangle {
        width: parent.width + 80
        height: 34
        x: -34
        y: -12
        rotation: -8
        color: theme.accentSoft
        opacity: 0.34
    }

    Rectangle {
        width: parent.width + 60
        height: 22
        x: -20
        y: parent.height - 25
        rotation: 6
        color: theme.accentAltSoft
        opacity: 0.24
    }

    // mode==0（空闲）时显示哪个内部页：home / dictSelect
    property string internalPage: "home"

    // 简易 toast
    function toast(msg) { toastText.text = msg; toastRect.opacity = 1; toastTimer.restart(); }

    // 退出插件（保存进度后通知宿主销毁）
    function exitPlugin() {
        if (wordController) wordController.saveState();
        root.backButtonClicked();
    }

    // ---- 页面路由 ----
    Loader {
        id: pageLoader
        anchors.fill: parent
        z: 2
        sourceComponent: {
            var m = wordController ? wordController.sessionMode : 0;
            if (m === 1) return studyComp;
            if (m === 2 || m === 3) return reviewComp;
            if (m === 4) return summaryComp;
            return root.internalPage === "dictSelect" ? dictComp : homeComp;
        }
    }

    Component {
        id: homeComp
        HomePage {
            theme: theme
            onRequestStudy: {
                if (wordController && !wordController.startStudy())
                    root.toast("没有新词可学啦，去复习吧");
            }
            onRequestReview: {
                if (wordController && !wordController.startReview())
                    root.toast("暂无可复习的单词");
            }
            onRequestDictSelect: root.internalPage = "dictSelect"
            onRequestExit: root.exitPlugin()
        }
    }
    Component {
        id: dictComp
        DictSelectPage {
            theme: theme
            onBack: root.internalPage = "home"
        }
    }
    Component {
        id: studyComp
        StudyPage {
            theme: theme
            onBack: if (wordController) wordController.goHome()
        }
    }
    Component {
        id: reviewComp
        ReviewPage {
            theme: theme
            onBack: if (wordController) wordController.goHome()
        }
    }
    Component {
        id: summaryComp
        SummaryPage {
            theme: theme
            onDone: if (wordController) wordController.goHome()
        }
    }

    // ---- 续学弹窗 ----
    YTwoButtonDialog {
        id: resumeDialog
        anchors.fill: parent
        z: 999
        tipItem.text: "上次有未完成的学习，是否继续？"
        onClickedConfirm: { if (wordController) wordController.resumeSession(); close(); }
        onClickedCancel:  { if (wordController) wordController.discardSession(); close(); }
    }

    // ---- toast ----
    Rectangle {
        id: toastRect
        z: 1000
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        radius: 6
        color: theme.surfaceRaised
        border.width: 1
        border.color: theme.border
        opacity: 0
        width: toastText.width + 24
        height: toastText.height + 14
        Behavior on opacity { NumberAnimation { duration: 200 } }
        Text {
            id: toastText
            anchors.centerIn: parent
            color: theme.textPrimary
            font.family: theme.fontFamily
            font.pixelSize: 12
        }
        Timer { id: toastTimer; interval: 1600; onTriggered: toastRect.opacity = 0 }
    }

    Component.onCompleted: {
        if (wordController && wordController.hasUnfinishedSession)
            resumeDialog.show();
    }

    // 页面被宿主销毁（退出插件/切页/Home键）时兜底保存进度
    Component.onDestruction: {
        if (wordController) wordController.saveState();
    }
}
