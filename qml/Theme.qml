import QtQuick 2.12

// 深色高对比主题。集中管理配色与字体，由 main.qml 注入各页面。
QtObject {
    // 背景层次
    readonly property color bg:            "#0B0F14"
    readonly property color bgAlt:         "#101923"
    readonly property color surface:       "#141A22"
    readonly property color surfaceRaised: "#1B2430"
    readonly property color surfaceGlass:  "#202C39"
    readonly property color border:        "#2B3646"

    // 文字
    readonly property color textPrimary:   "#F4F7F8"
    readonly property color textSecondary: "#A9B7C3"
    readonly property color textFaint:     "#647482"

    // 强调
    readonly property color accent:        "#78D6C6"
    readonly property color accentDeep:    "#2BA99A"
    readonly property color accentSoft:    "#163733"
    readonly property color accentAlt:     "#F0B45A"
    readonly property color accentAltSoft: "#3A2C18"

    // 语义色
    readonly property color success:       "#8CD879"
    readonly property color successSoft:   "#20371F"
    readonly property color danger:        "#FF7A7A"
    readonly property color dangerSoft:    "#3B2226"
    readonly property color warning:       "#F0B45A"
    readonly property color warningSoft:   "#3A2C18"

    // 中文字体（取宿主全局字体，回退到无衬线）
    readonly property string fontFamily:
        (typeof qmlGlobal !== "undefined" && qmlGlobal.fontFamilyZhCn)
            ? qmlGlobal.fontFamilyZhCn : "sans-serif"
}
