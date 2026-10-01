import QtQuick 2.6
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

Item {
    id: root
    signal closed()
    signal startNavigation()

    readonly property var itinerary: TripPlanner.selected

    Rectangle { anchors.fill: parent; color: M.bg }

    Column {
        anchors.fill: parent

        Item {
            width: parent.width
            height: 88 * M.dp

            RoundIconButton {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: M.spacingS
                iconName: "arrow_back"
                elevation: 0
                onClicked: root.closed()
            }

            Column {
                anchors.centerIn: parent
                spacing: 2 * M.dp
                MText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    variant: "title"
                    text: Fmt.duration(root.itinerary.duration)
                }
                MText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    variant: "body"
                    color: M.inkMuted
                    text: Fmt.distance(root.itinerary.length || 0, AppSettings.units) + " · " + qsTr("arrive ") + (root.itinerary.arrivalText || "")
                }
            }
        }
        Divider { width: parent.width }

        ListView {
            width: parent.width
            height: parent.height - 88 * M.dp - 64 * M.dp
            clip: true
            model: root.itinerary.actions || []
            delegate: ListRow {
                width: parent.width
                iconName: Fmt.turnIcon(modelData.action, modelData.direction)
                iconBg: M.surfaceVariant
                title: modelData.instruction || ""
                trailingText: modelData.length > 0 ? Fmt.distance(modelData.length, AppSettings.units) : ""
            }
        }

        Item {
            width: parent.width
            height: 64 * M.dp
            PrimaryButton {
                anchors.centerIn: parent
                text: qsTr("Start navigation")
                iconName: "navigation"
                onClicked: root.startNavigation()
            }
        }
    }
}
