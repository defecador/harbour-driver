import QtQuick 2.6
import Nemo.KeepAlive 1.1
import Nemo.Notifications 1.0

// Platform services for navigation on Sailfish OS. The Android build
// replaces this file (android/qml/components/NavSystem.qml) with the same
// interface.
Item {
    // Guidance is running: keep the CPU (and so GPS and timers) alive.
    property bool active: false
    // Keep the display lit as well.
    property bool screenOn: false

    // Published for alight/alightNow/missed so a transit rider with the
    // screen off (or the app minimised) still gets woken up in time.
    function notify(text) {
        notification.previewSummary = text
        notification.summary = text
        notification.publish()
    }

    KeepAlive { enabled: active }

    Binding { target: DisplayBlanking; property: "preventBlanking"; value: screenOn }

    Notification {
        id: notification
        appName: "Driver"
        category: "x-driver.transit"
        urgency: Notification.Critical
    }

    Component.onDestruction: DisplayBlanking.preventBlanking = false
}
