import QtQuick

// Platform services for navigation on Android (same interface as the
// Sailfish components/NavSystem.qml, which this replaces).
Item {
    // Guidance is running: keep the CPU (and so GPS and timers) alive.
    property bool active: false
    // Keep the display lit as well.
    property bool screenOn: false

    // Published for alight/alightNow/missed so a transit rider with the
    // screen off (or the app minimised) still gets woken up in time.
    function notify(text) {
        AppTools.vibrate(400)
        AppTools.notify("Driver", text)
    }

    // Nothing extra for `active`: the app is a foreground activity that
    // holds the screen on while guiding. Background guidance with a
    // foreground service is a later step.
    Component.onCompleted: AppTools.setKeepScreenOn(screenOn || active)
    onScreenOnChanged: AppTools.setKeepScreenOn(screenOn || active)
    onActiveChanged: AppTools.setKeepScreenOn(screenOn || active)
    Component.onDestruction: AppTools.setKeepScreenOn(false)
}
