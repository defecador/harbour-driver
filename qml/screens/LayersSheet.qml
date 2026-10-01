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
                text: qsTr("Map layers")
            }
        }
        Divider { width: parent.width }

        Item { width: 1; height: M.spacingM }

        MText { x: M.spacingM; variant: "caption"; color: M.inkMuted; text: qsTr("Map style") }

        ListRow {
            width: parent.width
            iconName: "map"
            title: qsTr("HERE Explore")
            trailingText: ""
            showChevron: AppSettings.mapStyle === "explore" && AppSettings.hasHereKey
            onClicked: if (AppSettings.hasHereKey) AppSettings.mapStyle = "explore"
            opacity: AppSettings.hasHereKey ? 1.0 : 0.4
        }
        ListRow {
            width: parent.width
            iconName: "satellite"
            title: qsTr("HERE Satellite")
            showChevron: AppSettings.mapStyle === "satellite" && AppSettings.hasHereKey
            onClicked: if (AppSettings.hasHereKey) AppSettings.mapStyle = "satellite"
            opacity: AppSettings.hasHereKey ? 1.0 : 0.4
        }
        MText {
            visible: !AppSettings.hasHereKey
            x: M.spacingM
            width: parent.width - M.spacingM * 2
            wrapMode: Text.WordWrap
            variant: "caption"
            color: M.inkMuted
            text: qsTr("Add a HERE API key in Settings to use HERE map styles. Using OpenFreeMap for now.")
        }

        Item { width: 1; height: M.spacingM }
        Divider { width: parent.width }

        ListRow {
            width: parent.width
            iconName: "traffic"
            title: qsTr("Traffic")
            showDivider: false
            SwitchM {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: M.spacingM
                checked: AppSettings.traffic
                onToggled: AppSettings.traffic = checked
            }
            onClicked: AppSettings.traffic = !AppSettings.traffic
        }

        Item { width: 1; height: M.spacingM }
        Divider { width: parent.width }

        MText { x: M.spacingM; variant: "caption"; color: M.inkMuted; text: qsTr("Theme") }
        Row {
            x: M.spacingM
            spacing: M.spacingS
            Chip { iconName: "brightness_auto"; label: qsTr("Auto"); filled: AppSettings.theme === "auto"; onClicked: AppSettings.theme = "auto" }
            Chip { iconName: "light_mode"; label: qsTr("Light"); filled: AppSettings.theme === "light"; onClicked: AppSettings.theme = "light" }
            Chip { iconName: "dark_mode"; label: qsTr("Dark"); filled: AppSettings.theme === "dark"; onClicked: AppSettings.theme = "dark" }
        }
    }
}
