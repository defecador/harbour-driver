import QtQuick 2.6
import "../theme"
import "../components"

// "You are here" marker, centered on the pixel MapboxMap reports (not
// bottom-anchored). A plain pulsing dot normally; while navigating it turns
// into a heading arrow so the map reads like a real turn-by-turn view.
Item {
    id: root
    width: 20 * M.dp
    height: 20 * M.dp
    visible: false

    property bool navigating: false
    property real heading: 0

    Rectangle {
        visible: !root.navigating
        anchors.centerIn: parent
        width: parent.width * 1.8
        height: width
        radius: width / 2
        color: M.location
        opacity: 0.18

        SequentialAnimation on scale {
            loops: Animation.Infinite
            running: root.visible && !root.navigating
            NumberAnimation { from: 0.6; to: 1.15; duration: 1400; easing.type: Easing.OutQuad }
            NumberAnimation { from: 1.15; to: 0.6; duration: 1400; easing.type: Easing.InQuad }
        }
    }

    Rectangle {
        visible: !root.navigating
        anchors.centerIn: parent
        width: parent.width
        height: width
        radius: width / 2
        color: M.location
        border.width: 2.5 * M.dp
        border.color: "#ffffff"
    }

    // Heading arrow: a plain circle backdrop (so the glyph reads on any map
    // style) plus the "navigation" glyph, which points north at rotation 0,
    // rotated clockwise to match the reported heading.
    Rectangle {
        visible: root.navigating
        anchors.centerIn: parent
        width: parent.width * 1.6
        height: width
        radius: width / 2
        color: M.location
        border.width: 2.5 * M.dp
        border.color: "#ffffff"

        Icon {
            anchors.centerIn: parent
            name: "navigation"
            size: parent.width * 0.62
            color: "#ffffff"
            rotation: root.heading
            Behavior on rotation { RotationAnimation { duration: 200; direction: RotationAnimation.Shortest } }
        }
    }
}
