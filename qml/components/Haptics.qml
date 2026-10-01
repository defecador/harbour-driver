import QtQuick 2.6

// A short vibration pulse for navigation announcements. Loads the real
// QtFeedback effect through a Loader (see HapticsImpl.qml) so a device
// without that plugin, or without a vibration motor, just makes vibrate()
// a silent no-op instead of breaking navigation.
Item {
    id: root
    function vibrate() {
        if (loader.item && loader.item.vibrate)
            loader.item.vibrate()
    }

    Loader {
        id: loader
        source: "HapticsImpl.qml"
        asynchronous: false
        onStatusChanged: if (status === Loader.Error) console.warn("Haptics: QtFeedback unavailable, vibration disabled")
    }
}
