import QtQuick 2.6
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

// Car / walk / bike summary card: big duration, traffic delay in orange, distance + arrival.
Card {
    id: root
    property var itinerary: ({})
    property bool selected: false
    signal clicked()
    signal goClicked()

    width: parent ? parent.width : 320
    height: row.height + M.spacingM * 2
    elevation: selected ? 2 : 1
    radius: M.radiusCard

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: root.selected ? 2 * M.dp : 0
        border.color: M.primary
        z: 5
    }

    Row {
        id: row
        x: M.spacingM
        y: M.spacingM
        width: parent.width - M.spacingM * 2
        spacing: M.spacingM
        z: 2

        Column {
            width: parent.width - 56 * M.dp - M.spacingM
            spacing: 2 * M.dp

            Row {
                spacing: M.spacingXs
                MText {
                    id: durationText
                    variant: "title"
                    text: Fmt.duration(root.itinerary.duration)
                }
                MText {
                    visible: (root.itinerary.trafficDelay || 0) >= 60
                    variant: "body"
                    color: M.traffic
                    anchors.baseline: durationText.baseline
                    text: qsTr("+%1 min traffic").arg(Math.round((root.itinerary.trafficDelay || 0) / 60))
                }
            }

            MText {
                variant: "body"
                color: M.inkMuted
                text: Fmt.distance(root.itinerary.length || 0, AppSettings.units) + " · " + qsTr("arrive ") + (root.itinerary.arrivalText || "")
            }
        }

        RoundIconButton {
            iconName: "navigation"
            bg: M.primary
            iconColor: M.primaryInk
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.goClicked()
        }
    }

    Ripple {
        z: 1
        rippleColor: Qt.rgba(0, 0, 0, 0.05)
        onClicked: root.clicked()
    }
}
