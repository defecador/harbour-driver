import QtQuick 2.6
import "../theme"
import "../theme/Fmt.js" as Fmt

// Car / Transit / Walk / Bike tabs, each showing its own best duration underneath
// so the modes can be compared at a glance, WeGo-style.
Item {
    id: root
    property var modes: ["car", "transit", "pedestrian", "bicycle"]
    property string selectedMode: "car"
    property var results: ({})   // mode -> array of itineraries
    property var status: ({})    // mode -> "loading" | "ok" | "empty" | "error"
    signal modeSelected(string mode)

    height: 60 * M.dp

    function bestDuration(mode) {
        var list = root.results[mode]
        if (!list || list.length === 0) return ""
        return Fmt.duration(list[0].duration)
    }

    Row {
        anchors.fill: parent
        Repeater {
            model: root.modes
            delegate: Item {
                id: tab
                width: root.width / root.modes.length
                height: root.height
                property bool selected: modelData === root.selectedMode

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4 * M.dp
                    radius: M.radiusChip
                    color: tab.selected ? M.buttonFill : "transparent"
                    border.width: M.sign && tab.selected ? M.borderW : 0
                    border.color: M.borderColor
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 2 * M.dp
                    Icon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        name: Fmt.modeIcon(modelData)
                        size: 22 * M.dp
                        color: tab.selected ? M.buttonInk : M.inkMuted
                    }
                    MText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        variant: "caption"
                        color: tab.selected ? M.buttonInk : M.inkMuted
                        text: root.status[modelData] === "loading" ? qsTr("…")
                            : root.status[modelData] === "error" ? qsTr("!")
                            : root.bestDuration(modelData)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.modeSelected(modelData)
                }
            }
        }
    }
}
