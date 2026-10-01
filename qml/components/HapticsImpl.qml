import QtQuick 2.6
import QtFeedback 5.0

// Loaded through Haptics.qml's Loader, isolated in its own file so a missing
// or wrong-versioned QtFeedback import only fails this Loader target rather
// than the whole screen (same defensive pattern as MapView/MapboxMapImpl.qml).
Item {
    function vibrate() { effect.start() }

    HapticsEffect {
        id: effect
        duration: 180
        intensity: 0.8
    }
}
