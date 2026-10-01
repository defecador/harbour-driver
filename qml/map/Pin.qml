import QtQuick 2.6
import "../theme"
import "../components"

// Map marker anchored at its bottom-center tip, positioned by MapboxMapImpl
// from the pixel MapboxMap reports for a tracked coordinate.
Item {
    id: root
    property string kind: "place"   // origin | destination | place

    readonly property color pinColor: kind === "origin" ? M.location
                                     : kind === "destination" ? M.bad : M.primary
    readonly property string iconName_: kind === "destination" ? "flag"
                                       : kind === "origin" ? "" : "place"

    width: 30 * M.dp
    height: 40 * M.dp
    visible: false
    z: 10

    Rectangle {
        width: 16 * M.dp
        height: 16 * M.dp
        radius: 2 * M.dp
        color: root.pinColor
        rotation: 45
        transformOrigin: Item.Center
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4 * M.dp
    }

    Rectangle {
        width: root.kind === "origin" ? 16 * M.dp : 28 * M.dp
        height: width
        radius: width / 2
        color: root.pinColor
        border.width: 2 * M.dp
        border.color: "#ffffff"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top

        Icon {
            visible: root.iconName_.length > 0
            anchors.centerIn: parent
            name: root.iconName_
            size: 16 * M.dp
            color: "#ffffff"
        }
    }
}
