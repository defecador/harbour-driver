import QtQuick 2.6
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

Card {
    id: root
    property var itinerary: ({})
    property bool selected: false
    signal clicked()
    signal goClicked()

    width: parent ? parent.width : 320
    height: col.height + M.spacingM * 2
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

    Column {
        id: col
        x: M.spacingM
        y: M.spacingM
        width: parent.width - M.spacingM * 2
        spacing: M.spacingS
        z: 2

        Row {
            width: parent.width
            Column {
                width: parent.width - 56 * M.dp
                spacing: 2 * M.dp
                MText {
                    variant: "title"
                    text: (root.itinerary.departureText || "") + "  →  " + (root.itinerary.arrivalText || "")
                }
                MText {
                    variant: "body"
                    color: M.inkMuted
                    text: Fmt.duration(root.itinerary.duration) + " · "
                          + (root.itinerary.transfers === 1 ? qsTr("1 transfer") : qsTr("%1 transfers").arg(root.itinerary.transfers || 0))
                          + (root.itinerary.walkDuration > 0 ? (" · " + Fmt.duration(root.itinerary.walkDuration) + " " + qsTr("walk")) : "")
                }
            }
            RoundIconButton {
                iconName: "navigation"
                bg: M.primary
                iconColor: M.primaryInk
                onClicked: root.goClicked()
            }
        }

        Flow {
            width: parent.width
            spacing: M.spacingXs
            Repeater {
                model: root.itinerary.legs || []
                delegate: Row {
                    spacing: 2 * M.dp
                    LineBadge {
                        visible: modelData.type === "transit"
                        label: modelData.lineName || Fmt.transitLabel(modelData.transportMode)
                        iconName: Fmt.transitIcon(modelData.transportMode)
                        lineColor: Fmt.hexColor(modelData.lineColor, M.primary)
                        textColor: Fmt.hexColor(modelData.lineTextColor, "#ffffff")
                    }
                    Icon {
                        visible: modelData.type !== "transit"
                        name: "directions_walk"
                        size: 18 * M.dp
                        color: M.inkMuted
                    }
                    Icon {
                        visible: index < (root.itinerary.legs.length - 1)
                        name: "chevron_right"
                        size: 16 * M.dp
                        color: M.inkMuted
                    }
                }
            }
        }
    }

    Ripple {
        z: 1
        rippleColor: Qt.rgba(0, 0, 0, 0.05)
        onClicked: root.clicked()
    }
}
