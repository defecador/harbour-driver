import QtQuick

// Android vibration, through AppTools. Same interface as the Sailfish
// QtFeedback version, which this replaces.
Item {
    function vibrate() { AppTools.vibrate(180) }
}
