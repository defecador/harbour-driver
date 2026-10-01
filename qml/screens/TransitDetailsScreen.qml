import QtQuick 2.6
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

Item {
    id: root
    signal closed()
    signal startNavigation()

    readonly property var itinerary: TripPlanner.selected
    property var expandedMap: ({})

    function toggleExpand(i) {
        var m = {}
        for (var k in expandedMap) m[k] = expandedMap[k]
        m[i] = !m[i]
        expandedMap = m
    }

    function delayColor(delay) {
        if (delay === undefined || delay === null || delay < 0) return M.ink
        return Fmt.isLate(delay) ? M.bad : M.good
    }

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
                    text: (root.itinerary.departureText || "") + "  →  " + (root.itinerary.arrivalText || "")
                }
                MText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    variant: "body"
                    color: M.inkMuted
                    text: Fmt.duration(root.itinerary.duration) + " · "
                          + (root.itinerary.transfers === 1 ? qsTr("1 transfer") : qsTr("%1 transfers").arg(root.itinerary.transfers || 0))
                }
            }
        }
        Divider { width: parent.width }

        Flickable {
            width: parent.width
            height: parent.height - 88 * M.dp - 64 * M.dp
            clip: true
            contentHeight: timeline.height + M.spacingM

            Column {
                id: timeline
                x: M.spacingM
                y: M.spacingS
                width: parent.width - M.spacingM * 2

                Repeater {
                    model: root.itinerary.legs || []
                    delegate: Column {
                        id: legItem
                        width: timeline.width
                        spacing: M.spacingXs

                        Row {
                            width: parent.width
                            spacing: M.spacingS
                            Rectangle {
                                width: 10 * M.dp; height: 10 * M.dp; radius: 5 * M.dp
                                color: modelData.type === "transit" ? Fmt.hexColor(modelData.lineColor, M.primary) : M.inkMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            MText {
                                text: modelData.from.timeText || ""
                                variant: "body"
                                color: root.delayColor(modelData.from.delay)
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            MText {
                                text: modelData.from.name || ""
                                variant: "body"
                                color: M.inkMuted
                                elide: Text.ElideRight
                                width: parent.width - 100 * M.dp
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: M.spacingS
                            Rectangle {
                                width: 2 * M.dp
                                height: Math.max(32 * M.dp, segCol.height)
                                color: modelData.type === "transit" ? Fmt.hexColor(modelData.lineColor, M.primary) : M.outline
                            }
                            Column {
                                id: segCol
                                width: parent.width - 20 * M.dp
                                spacing: M.spacingXs

                                Item { width: 1; height: M.spacingXs }

                                Row {
                                    spacing: M.spacingXs
                                    LineBadge {
                                        visible: modelData.type === "transit"
                                        label: modelData.lineName || Fmt.transitLabel(modelData.transportMode)
                                        iconName: Fmt.transitIcon(modelData.transportMode)
                                        lineColor: Fmt.hexColor(modelData.lineColor, M.primary)
                                        textColor: Fmt.hexColor(modelData.lineTextColor, "#ffffff")
                                    }
                                    MText {
                                        visible: modelData.type === "transit" && (modelData.headsign || "").length > 0
                                        variant: "caption"
                                        color: M.inkMuted
                                        text: qsTr("→ %1").arg(modelData.headsign)
                                    }
                                    Icon { visible: modelData.type !== "transit"; name: "directions_walk"; size: 18 * M.dp; color: M.inkMuted }
                                    MText {
                                        visible: modelData.type !== "transit"
                                        variant: "body"
                                        color: M.inkMuted
                                        text: qsTr("Walk %1").arg(Fmt.duration(modelData.duration))
                                    }
                                }

                                MText {
                                    visible: modelData.type === "transit" && (modelData.agency || "").length > 0
                                    variant: "caption"
                                    color: M.inkMuted
                                    text: modelData.agency || ""
                                }

                                Item {
                                    // A MouseArea can't anchors.fill a Row/Column directly: the positioner's
                                    // implicit height depends on its children's heights, and an anchored
                                    // child's height depends right back on the positioner's - the resulting
                                    // circular binding is what was undersizing this row (and everything
                                    // below it in segCol). Sizing this wrapper Item from stopsRow explicitly
                                    // breaks that cycle.
                                    visible: modelData.type === "transit" && (modelData.stops || []).length > 0
                                    width: stopsRow.width
                                    height: stopsRow.height

                                    Row {
                                        id: stopsRow
                                        spacing: 2 * M.dp
                                        Icon {
                                            name: root.expandedMap[index] ? "expand_less" : "expand_more"
                                            size: 16 * M.dp
                                            color: M.primary
                                        }
                                        MText {
                                            variant: "caption"
                                            color: M.primary
                                            text: root.expandedMap[index] ? qsTr("Hide stops") : qsTr("%1 stops").arg((modelData.stops || []).length)
                                        }
                                    }

                                    MouseArea { anchors.fill: parent; onClicked: root.toggleExpand(index) }
                                }

                                Column {
                                    visible: root.expandedMap[index] === true
                                    width: parent.width
                                    spacing: 2 * M.dp
                                    Repeater {
                                        model: modelData.stops || []
                                        delegate: Row {
                                            spacing: M.spacingXs
                                            MText { variant: "caption"; color: M.inkMuted; text: modelData.timeText || "" }
                                            MText { variant: "caption"; color: M.inkMuted; text: modelData.name || "" }
                                        }
                                    }
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: M.spacingS
                            visible: index === ((root.itinerary.legs || []).length - 1)
                            Rectangle {
                                width: 10 * M.dp; height: 10 * M.dp; radius: 5 * M.dp
                                color: M.bad
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            MText {
                                text: modelData.to.timeText || ""
                                variant: "body"
                                color: root.delayColor(modelData.to.delay)
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            MText {
                                text: modelData.to.name || ""
                                variant: "body"
                                color: M.inkMuted
                                elide: Text.ElideRight
                                width: parent.width - 100 * M.dp
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }

                Item { width: 1; height: M.spacingM }

                Repeater {
                    model: root.itinerary.attributions || []
                    delegate: MText {
                        width: timeline.width
                        wrapMode: Text.WordWrap
                        variant: "caption"
                        color: M.inkMuted
                        text: modelData.text || ""
                    }
                }
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
