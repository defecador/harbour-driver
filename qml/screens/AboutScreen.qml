import QtQuick 2.6
import "../theme"
import "../components"

Item {
    id: root
    signal closed()

    Rectangle { anchors.fill: parent; color: M.bg }

    Column {
        anchors.fill: parent

        Item {
            width: parent.width
            height: 56 * M.dp
            RoundIconButton {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: M.spacingM
                iconName: "arrow_back"
                elevation: 0
                onClicked: root.closed()
            }
            MText {
                anchors.centerIn: parent
                variant: "subtitle"
                text: qsTr("About Driver")
            }
        }
        Divider { width: parent.width }

        Flickable {
            width: parent.width
            height: parent.height - 56 * M.dp
            clip: true
            contentHeight: body.height

            Column {
                id: body
                x: M.spacingM
                y: M.spacingM
                width: parent.width - M.spacingM * 2
                spacing: M.spacingM

                MText {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: qsTr("Driver is an independent, unofficial trip planner for Sailfish OS. Version %1.").arg(AppTools.appVersion)
                }

                MText { variant: "subtitle"; text: qsTr("Data & maps") }
                MText {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: M.inkMuted
                    text: qsTr("Routing, transit, search and map tiles © HERE. Use of the HERE services is subject to HERE's End User Terms and Privacy Policy.")
                }
                MText {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: M.inkMuted
                    text: qsTr("When no HERE key is set, the map falls back to OpenFreeMap / OpenStreetMap contributors.")
                }

                MText { variant: "subtitle"; text: qsTr("Fonts & icons") }
                MText {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: M.inkMuted
                    text: qsTr("Fira Sans (SIL Open Font License 1.1) by Mozilla / Telefonica. Material Symbols (Apache License 2.0) by Google.")
                }

                MText { variant: "subtitle"; text: qsTr("License") }
                MText {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: M.inkMuted
                    text: qsTr("Driver is free software, released under the GNU General Public License v3.0.")
                }

                Item { width: 1; height: M.spacingM }
            }
        }
    }
}
