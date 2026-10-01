pragma Singleton
import QtQuick 2.6
import Sailfish.Silica 1.0 as Silica

// Design tokens for the WeGo-style UI: sizes, colours and text styles.
// Kept separate from Silica's own Theme so the app doesn't inherit Silica's look.
QtObject {
    id: root

    readonly property real dp: AppTools ? AppTools.dp : 2.0

    readonly property bool dark: AppSettings.theme === "dark"
            || (AppSettings.theme === "auto" && Silica.Theme.colorScheme === Silica.Theme.LightOnDark)

    // --- Colours -----------------------------------------------------
    readonly property color bg: dark ? "#1b1b1f" : "#f2f2f7"
    readonly property color surface: dark ? "#242428" : "#ffffff"
    readonly property color surfaceVariant: dark ? "#2e2e33" : "#eef1f4"
    readonly property color ink: dark ? "#e7e7ea" : "#1a1c1e"
    readonly property color inkMuted: dark ? "#a9acb0" : "#5c6066"
    readonly property color outline: dark ? "#3a3a3f" : "#dfe2e6"
    readonly property color scrim: "#80000000"

    readonly property color primary: "#1a73e8"
    readonly property color primaryInk: "#ffffff"
    readonly property color location: "#0f9d8a"
    readonly property color traffic: "#e8710a"
    readonly property color good: "#1e8e3e"
    readonly property color bad: "#d93025"
    readonly property color routeAlt: dark ? "#55565c" : "#c4c7cc"

    readonly property var lineDefault: "#5f6368"

    // --- Metrics -------------------------------------------------------
    readonly property real radiusCard: 12 * dp
    readonly property real radiusSheet: 16 * dp
    readonly property real radiusPill: 28 * dp
    readonly property real radiusChip: 8 * dp
    readonly property real spacingXs: 4 * dp
    readonly property real spacingS: 8 * dp
    readonly property real spacingM: 16 * dp
    readonly property real spacingL: 24 * dp

    // --- Type ------------------------------------------------------
    readonly property string fontFamily: "Fira Sans"
    readonly property real sizeTitle: 20 * dp
    readonly property real sizeSubtitle: 16 * dp
    readonly property real sizeBody: 14 * dp
    readonly property real sizeCaption: 12 * dp
    readonly property real sizeButton: 15 * dp

    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.Medium
    readonly property int weightSemiBold: Font.DemiBold

    readonly property int fadeShort: 120
    readonly property int fadeMedium: 200
}
