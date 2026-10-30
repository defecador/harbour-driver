import QtQuick 2.6
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

Item {
    id: root
    signal openDetails()
    signal closed()
    signal editField(string field)
    // The expanded card's map window, in window coordinates. AppRoot frames
    // the (shared, full-screen) map so the route fits inside it.
    signal previewFrame(real x, real y, real w, real h)

    // Index of the card showing its route map, or -1.
    property int previewIndex: -1
    property Item holeItem: null
    // False in wide windows: the route is then shown on the big map beside
    // the list instead of in a map window inside the card.
    property bool inlineMap: true

    function emitFrame() {
        if (previewIndex < 0)
            return
        if (!inlineMap) {
            previewFrame(0, 0, 0, 0)
            return
        }
        if (!holeItem || holeItem.height < 40)
            return
        var p = holeItem.mapToItem(null, 0, 0)
        previewFrame(p.x, p.y, holeItem.width, holeItem.height)
    }

    Timer { id: frameTimer; interval: 320; onTriggered: root.emitFrame() }
    onPreviewIndexChanged: frameTimer.restart()
    onHoleItemChanged: frameTimer.restart()
    onInlineMapChanged: frameTimer.restart()

    Connections {
        target: TripPlanner
        onSelectedModeChanged: root.previewIndex = -1
        onResultsChanged: root.previewIndex = -1
    }

    Component.onCompleted: if (AppTools.devScreen === "plannerpreview") previewIndex = 0

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: header.height
        color: M.bg
    }

    Column {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        Item { width: 1; height: M.topInset }

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
        id: listArea
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        readonly property var list_: TripPlanner.results[TripPlanner.selectedMode] || []
        readonly property string status_: TripPlanner.status[TripPlanner.selectedMode] || ""

        // Opaque backdrop for the loading/error states; when routes are
        // listed every row paints its own background so the map can show
        // through the preview window.
        Rectangle { anchors.fill: parent; color: M.bg; visible: parent.status_ !== "ok" }

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

        // Which countries' toll roads to avoid (car routes only).
        Item {
            id: tollCard
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            visible: TripPlanner.selectedMode === "car" && TripPlanner.tollCountries.length > 0
            height: visible ? tollColumn.height + M.spacingS * 2 : 0
            readonly property var notAvoided: (listArea.list_[0] || {}).tollsNotAvoided || []

            Rectangle { anchors.fill: parent; color: M.bg }

            Column {
                id: tollColumn
                x: M.spacingM
                y: M.spacingS
                width: parent.width - M.spacingM * 2

                MText { variant: "caption"; color: M.inkMuted; text: qsTr("Toll roads on this route") }

                Repeater {
                    model: TripPlanner.tollCountries
                    delegate: Item {
                        width: tollColumn.width
                        height: 44 * M.dp
                        MText {
                            anchors.left: parent.left
                            anchors.right: tollSwitch.left
                            anchors.rightMargin: M.spacingS
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            text: qsTr("Avoid tolls in %1").arg(Fmt.countryName(modelData.code))
                        }
                        SwitchM {
                            id: tollSwitch
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            checked: modelData.avoid
                            onToggled: TripPlanner.setAvoidToll(modelData.code, checked)
                        }
                    }
                }

                MText {
                    visible: tollCard.notAvoided.length > 0
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: M.bad
                    text: {
                        var names = []
                        for (var i = 0; i < tollCard.notAvoided.length; i++)
                            names.push(Fmt.countryName(tollCard.notAvoided[i]))
                        return qsTr("Couldn't avoid all tolls in %1.").arg(names.join(", "))
                    }
                }
            }
        }

        Flickable {
            id: routeList
            anchors.top: tollCard.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            visible: parent.status_ === "ok"
            contentWidth: width
            contentHeight: rows.height + filler.height
            boundsBehavior: Flickable.StopAtBounds
            onMovementEnded: root.emitFrame()

            Column {
                id: rows
                width: routeList.width

                Repeater {
                    model: listArea.list_
                    delegate: Item {
                        id: delegateRoot
                        readonly property bool expanded: root.previewIndex === index
                        property real prog: expanded ? 1 : 0
                        Behavior on prog { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
                        readonly property real mapH: root.inlineMap ? 240 * M.dp : 0

                        width: rows.width
                        height: M.spacingM + (cardLoader.item ? cardLoader.item.height : 0) + (mapH + M.spacingM) * prog + M.spacingM

                        // Background everywhere except the map window (hole).
                        Rectangle { x: 0; y: 0; width: parent.width; height: hole.y; color: M.bg }
                        Rectangle { x: 0; y: hole.y; width: hole.x; height: hole.height; color: M.bg }
                        Rectangle { x: hole.x + hole.width; y: hole.y; width: parent.width - x; height: hole.height; color: M.bg }
                        Rectangle { x: 0; y: hole.y + hole.height; width: parent.width; height: parent.height - y; color: M.bg }

                        Loader {
                            id: cardLoader
                            x: M.spacingM
                            y: M.spacingM
                            width: parent.width - M.spacingM * 2
                            sourceComponent: TripPlanner.selectedMode === "transit" ? transitCardComp : routeCardComp
                            onLoaded: {
                                item.itinerary = modelData
                                item.selected = (index === TripPlanner.selectedIndex)
                            }
                        }

                        // The map shows through here (see previewFrame).
                        Item {
                            id: hole
                            x: M.spacingM
                            y: M.spacingM + (cardLoader.item ? cardLoader.item.height : 0) + M.spacingM
                            width: parent.width - M.spacingM * 2
                            height: delegateRoot.mapH * delegateRoot.prog
                            visible: delegateRoot.prog > 0.01 && root.inlineMap
                            Rectangle {
                                anchors.fill: parent
                                radius: M.radiusCard
                                color: "transparent"
                                border.width: 2 * M.dp
                                border.color: M.primary
                            }
                        }

                        Binding { target: root; property: "holeItem"; value: hole; when: delegateRoot.expanded }

                        Connections {
                            target: TripPlanner
                            onSelectionChanged: if (cardLoader.item) cardLoader.item.selected = (index === TripPlanner.selectedIndex)
                        }

                        Connections {
                            target: cardLoader.item
                            ignoreUnknownSignals: true
                            onClicked: {
                                if (index === TripPlanner.selectedIndex)
                                    root.previewIndex = delegateRoot.expanded ? -1 : index
                                else {
                                    TripPlanner.selectedIndex = index
                                    root.previewIndex = index
                                }
                            }
                            onGoClicked: root.openDetails()
                        }
                    }
                }
            }

            // Fills the rest of the screen below the last row.
            Rectangle {
                id: filler
                y: rows.height
                width: routeList.width
                height: Math.max(0, routeList.height - rows.height)
                color: M.bg
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
