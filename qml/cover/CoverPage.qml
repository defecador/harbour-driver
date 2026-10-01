import QtQuick 2.0
import Sailfish.Silica 1.0
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

CoverBackground {
    id: cover

    readonly property var selected: TripPlanner ? TripPlanner.selected : ({})
    readonly property bool hasRoute: selected && Object.keys(selected).length > 0
    readonly property bool navigating: Navigator && (Navigator.state === "guiding" || Navigator.state === "waitingForGps")
    readonly property bool onFootOrWheels: !Navigator || Navigator.mode !== "transit" || Navigator.transitPhase === "walk"

    Rectangle {
        anchors.fill: parent
        color: navigating && Navigator.mode === "transit" && Navigator.transitPhase === "alight" ? M.good
                : navigating && (Navigator.offRoute || (cover.onFootOrWheels && Navigator.hurry)) ? M.traffic
                : M.primary
    }

    Column {
        visible: !hasRoute && !cover.navigating
        anchors.centerIn: parent
        spacing: Theme.paddingSmall
        Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "directions"; size: 40 * M.dp; color: "#ffffff" }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Matka"
            color: "#ffffff"
            font.pixelSize: Theme.fontSizeLarge
        }
    }

    // Planned-but-not-yet-navigating: preview of the selected route.
    Column {
        visible: hasRoute && !cover.navigating
        anchors.centerIn: parent
        spacing: Theme.paddingSmall
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: cover.hasRoute ? Fmt.modeIcon(cover.selected.mode) : ""
            size: 44 * M.dp
            color: "#ffffff"
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: cover.hasRoute ? Fmt.duration(cover.selected.duration) : ""
            color: "#ffffff"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: cover.hasRoute ? (qsTr("Arrive ") + cover.selected.arrivalText) : ""
            color: Qt.rgba(1, 1, 1, 0.85)
            font.pixelSize: Theme.fontSizeExtraSmall
        }
    }

    // Actively navigating (car/walk/bike, or a transit walking leg): the
    // cover people glance at while driving with the app minimised, so it
    // shows the live turn instead of the summary.
    Column {
        visible: cover.navigating && cover.onFootOrWheels
        anchors.centerIn: parent
        spacing: Theme.paddingSmall
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: Navigator.state === "waitingForGps" ? "my_location" : Fmt.turnIcon(Navigator.maneuverAction, Navigator.maneuverDirection)
            size: 44 * M.dp
            color: "#ffffff"
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Navigator.state === "waitingForGps" ? qsTr("Waiting for GPS…") : Fmt.distance(Navigator.distanceToManeuver, AppSettings.units)
            color: "#ffffff"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: Navigator.state !== "waitingForGps"
            text: qsTr("Arrive ") + Navigator.arrivalText
            color: Qt.rgba(1, 1, 1, 0.85)
            font.pixelSize: Theme.fontSizeExtraSmall
        }
    }

    // Actively navigating a transit wait/ride/alight phase: the line badge
    // plus the one thing that matters at a glance (countdown, stop count,
    // or "get off now").
    Column {
        visible: cover.navigating && !cover.onFootOrWheels
        anchors.centerIn: parent
        spacing: Theme.paddingSmall
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: Fmt.transitIcon(Navigator.transportMode)
            size: 44 * M.dp
            color: "#ffffff"
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Navigator.lineName
            color: "#ffffff"
            font.pixelSize: Theme.fontSizeMedium
            font.bold: true
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Navigator.transitPhase === "wait" ? Fmt.duration(Navigator.secondsToDeparture)
                  : Navigator.transitPhase === "alight" ? qsTr("Get off next")
                  : qsTr("%1 stops").arg(Navigator.stopsRemaining)
            color: "#ffffff"
            font.pixelSize: Theme.fontSizeLarge
            font.bold: true
        }
    }
}
