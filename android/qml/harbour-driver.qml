import QtQuick
import QtQuick.Controls

// Android / desktop Qt 6 host window for AppRoot. No cover and no orientation
// lock: the window follows whatever size the system gives it (rotation,
// split screen, freeform), and AppRoot lays itself out from its own size.
ApplicationWindow {
    id: appWindow
    visible: true
    width: 1000
    height: 700
    title: "Driver"

    AppRoot {
        id: appRoot
        anchors.fill: parent
        anchors.leftMargin: AppTools.insetLeft
        anchors.rightMargin: AppTools.insetRight
        anchors.bottomMargin: AppTools.insetBottom
    }

    // The Android back key steps back through the screen stack and only
    // leaves the app at the root.
    onClosing: (close) => {
        if (appRoot.goBack())
            close.accepted = false
    }

    Component.onCompleted: AppTools.refreshInsets()
    onWidthChanged: AppTools.refreshInsets()
    onHeightChanged: AppTools.refreshInsets()
}
