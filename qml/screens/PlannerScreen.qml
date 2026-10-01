import QtQuick 2.6
import "../theme"
import "../components"

Item {
    id: root
    signal openDetails()
    signal closed()
    signal editField(string field)

    Rectangle { anchors.fill: parent; color: M.bg }

    Column {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        Item {
            width: parent.width
            height: 100 * M.dp

            RoundIconButton {
                id: back
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: M.spacingM
                iconName: "arrow_back"
                elevation: 0
                onClicked: root.closed()
            }

            Column {
                anchors.left: back.right
                anchors.right: swap.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: M.spacingS
                anchors.rightMargin: M.spacingS
                spacing: M.spacingXs

                Rectangle {
                    width: parent.width
                    height: 40 * M.dp
                    radius: M.radiusChip
                    color: M.surfaceVariant
                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: M.spacingS
                        spacing: M.spacingXs
                        Rectangle { width: 8 * M.dp; height: 8 * M.dp; radius: 4 * M.dp; color: M.location; anchors.verticalCenter: parent.verticalCenter }
                        MText { text: TripPlanner.from.title || qsTr("Your location"); anchors.verticalCenter: parent.verticalCenter }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.editField("from") }
                }
                Rectangle {
                    width: parent.width
                    height: 40 * M.dp
                    radius: M.radiusChip
                    color: M.surfaceVariant
                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: M.spacingS
                        spacing: M.spacingXs
                        Icon { name: "place"; size: 16 * M.dp; color: M.bad; anchors.verticalCenter: parent.verticalCenter }
                        MText { text: TripPlanner.to.title || qsTr("Choose destination"); anchors.verticalCenter: parent.verticalCenter }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.editField("to") }
                }
            }

            RoundIconButton {
                id: swap
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: M.spacingM
                iconName: "swap_vert"
                elevation: 0
                onClicked: TripPlanner.swap()
            }
        }

        ModeTabs {
            width: parent.width
            selectedMode: TripPlanner.selectedMode
            results: TripPlanner.results
            status: TripPlanner.status
            onModeSelected: TripPlanner.selectedMode = mode
        }
        Divider { width: parent.width }
    }

    Item {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        readonly property var list_: TripPlanner.results[TripPlanner.selectedMode] || []
        readonly property string status_: TripPlanner.status[TripPlanner.selectedMode] || ""

        Column {
            visible: parent.status_ === "loading"
            anchors.centerIn: parent
            spacing: M.spacingS
            MText { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Planning…"); color: M.inkMuted }
        }

        Column {
            visible: parent.status_ === "error" || parent.status_ === "empty"
            anchors.centerIn: parent
            anchors.margins: M.spacingL
            width: parent.width - M.spacingL * 2
            spacing: M.spacingS
            Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "error"; size: 32 * M.dp; color: M.inkMuted }
            MText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: M.inkMuted
                text: parent.status_ === "error" ? (TripPlanner.errors[TripPlanner.selectedMode] || qsTr("Couldn't plan this route."))
                                                  : qsTr("No routes found.")
            }
        }

        ListView {
            id: routeList
            anchors.fill: parent
            clip: true
            visible: parent.status_ === "ok"
            model: parent.list_
            spacing: M.spacingM
            topMargin: M.spacingM
            bottomMargin: M.spacingM
            delegate: Item {
                id: delegateRoot
                width: routeList.width
                height: cardLoader.item ? cardLoader.item.height : 0

                Loader {
                    id: cardLoader
                    width: parent.width
                    sourceComponent: TripPlanner.selectedMode === "transit" ? transitCardComp : routeCardComp
                    onLoaded: {
                        item.itinerary = modelData
                        item.selected = (index === TripPlanner.selectedIndex)
                    }
                }

                Connections {
                    target: TripPlanner
                    onSelectionChanged: if (cardLoader.item) cardLoader.item.selected = (index === TripPlanner.selectedIndex)
                }

                Connections {
                    target: cardLoader.item
                    ignoreUnknownSignals: true
                    onClicked: TripPlanner.selectedIndex = index
                    onGoClicked: root.openDetails()
                }
            }
        }
    }

    Component {
        id: routeCardComp
        RouteCard {}
    }
    Component {
        id: transitCardComp
        TransitRouteCard {}
    }
}
