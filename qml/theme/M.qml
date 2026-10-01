pragma Singleton
import QtQuick 2.6
import Sailfish.Silica 1.0 as Silica

// Design tokens for the WeGo-style UI: sizes, colours and text styles.
// Kept separate from Silica's own Theme so the app doesn't inherit Silica's look.
QtObject {
    id: root

    readonly property real dp: AppTools ? AppTools.dp : 2.0
    // About 1 cm of room above page headers, for the front camera cut-out.
    readonly property real topInset: AppTools ? AppTools.cm : 0

    readonly property bool dark: (AppTools && AppTools.devDark) ? true : (AppTools && AppTools.devLight) ? false
            : AppSettings.theme === "dark"
            || (AppSettings.theme === "auto" && Silica.Theme.colorScheme === Silica.Theme.LightOnDark)

    // Two looks share these tokens. "new" is a bold street-sign style by day
    // (sign) and a quiet, card-less style by night (quiet); "classic" is the
    // original Material-style look.
    readonly property bool sign: AppSettings.uiStyle === "new" && !dark
    readonly property bool quiet: AppSettings.uiStyle === "new" && dark
    readonly property bool restyled: sign || quiet

    // --- Colours -----------------------------------------------------
    readonly property color bg: sign ? "#f4f1e8" : quiet ? "#16232b" : dark ? "#1b1b1f" : "#f2f2f7"
    readonly property color surface: sign ? "#ffffff" : quiet ? "#16232b" : dark ? "#242428" : "#ffffff"
    readonly property color surfaceVariant: sign ? "#ebe7da" : quiet ? "#1d2f39" : dark ? "#2e2e33" : "#eef1f4"
    readonly property color ink: sign ? "#111111" : quiet ? "#e8f1f5" : dark ? "#e7e7ea" : "#1a1c1e"
    readonly property color inkMuted: sign ? "#444444" : quiet ? "#9db2bd" : dark ? "#a9acb0" : "#5c6066"
    readonly property color outline: sign ? "#111111" : quiet ? "#2a3b45" : dark ? "#3a3a3f" : "#dfe2e6"
    readonly property color scrim: "#80000000"

    readonly property color primary: sign ? "#00704a" : quiet ? "#ff7a45" : "#1a73e8"
    readonly property color primaryInk: quiet ? "#10181d" : "#ffffff"
    readonly property color location: "#0f9d8a"
    readonly property color traffic: "#e8710a"
    readonly property color good: "#1e8e3e"
    readonly property color bad: "#d93025"
    readonly property color routeAlt: sign ? "#bdb8a8" : quiet ? "#2e414d" : dark ? "#55565c" : "#c4c7cc"

    // Accent for buttons and selected tabs: sign-yellow by day, the warm
    // accent by night, the blue in the classic look.
    readonly property color buttonFill: sign ? "#ffd400" : primary
    readonly property color buttonInk: sign ? "#111111" : primaryInk
    // Navigation banner background.
    readonly property color banner: quiet ? "#101b22" : primary
    // Outline drawn around cards and controls (none in the classic look).
    readonly property real borderW: sign ? Math.round(2 * dp) : quiet ? Math.max(1, Math.round(dp)) : 0
    readonly property color borderColor: sign ? "#111111" : outline

    readonly property var lineDefault: "#5f6368"

    // --- Metrics -------------------------------------------------------
    readonly property real radiusCard: restyled ? 0 : 12 * dp
    readonly property real radiusSheet: restyled ? 0 : 16 * dp
    readonly property real radiusPill: restyled ? 0 : 28 * dp
    readonly property real radiusChip: restyled ? 0 : 8 * dp
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
