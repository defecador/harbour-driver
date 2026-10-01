import QtQuick 2.6
import "../theme"
import "../components"

Item {
    id: root
    signal closed()

    property string keyStatus: ""
    property bool keyOk: false

    Connections {
        target: SearchController
        onKeyTested: {
            root.keyStatus = message
            root.keyOk = ok
        }
    }

    Rectangle { anchors.fill: parent; color: M.bg }

    Column {
        anchors.fill: parent

        Item { width: 1; height: M.topInset }

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
                text: qsTr("Settings")
            }
        }
        Divider { width: parent.width }

        Column {
            width: parent.width - M.spacingM * 2
            x: M.spacingM
            y: M.spacingM
            spacing: M.spacingM

            TextFieldM {
                width: parent.width
                label: qsTr("HERE API key")
                placeholder: qsTr("Paste your key")
                text: AppSettings.hereApiKey
                onTextChanged: AppSettings.hereApiKey = text
            }

            Row {
                spacing: M.spacingS
                PrimaryButton {
                    text: qsTr("Test key")
                    iconName: "vpn_key"
                    onClicked: SearchController.testKey()
                }
            }

            MText {
                visible: root.keyStatus.length > 0
                width: parent.width
                wrapMode: Text.WordWrap
                color: root.keyOk ? M.good : M.bad
                text: root.keyStatus
            }

            Divider { width: parent.width }

            Item {
                width: parent.width
                height: 56 * M.dp
                MText {
                    anchors.left: parent.left
                    anchors.right: view3dSwitch.left
                    anchors.rightMargin: M.spacingS
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("3D view while driving (tilted map with 3D buildings)")
                }
                SwitchM {
                    id: view3dSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: AppSettings.view3d
                    onToggled: AppSettings.view3d = checked
                }
            }

            Item {
                width: parent.width
                height: 56 * M.dp
                MText {
                    anchors.left: parent.left
                    anchors.right: vibrationSwitch.left
                    anchors.rightMargin: M.spacingS
                    anchors.verticalCenter: parent.verticalCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Vibrate on navigation prompts")
                }
                SwitchM {
                    id: vibrationSwitch
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: AppSettings.vibration
                    onToggled: AppSettings.vibration = checked
                }
            }

            MText { variant: "caption"; color: M.inkMuted; text: qsTr("Look") }
            Row {
                spacing: M.spacingS
                Chip {
                    label: qsTr("New")
                    filled: AppSettings.uiStyle !== "classic"
                    onClicked: AppSettings.uiStyle = "new"
                }
                Chip {
                    label: qsTr("Classic")
                    filled: AppSettings.uiStyle === "classic"
                    onClicked: AppSettings.uiStyle = "classic"
                }
            }

            MText { variant: "caption"; color: M.inkMuted; text: qsTr("Day or night") }
            Row {
                spacing: M.spacingS
                Chip {
                    iconName: "brightness_auto"
                    label: qsTr("System")
                    filled: AppSettings.theme === "auto"
                    onClicked: AppSettings.theme = "auto"
                }
                Chip {
                    iconName: "light_mode"
                    label: qsTr("Day")
                    filled: AppSettings.theme === "light"
                    onClicked: AppSettings.theme = "light"
                }
                Chip {
                    iconName: "dark_mode"
                    label: qsTr("Night")
                    filled: AppSettings.theme === "dark"
                    onClicked: AppSettings.theme = "dark"
                }
            }

            MText { variant: "caption"; color: M.inkMuted; text: qsTr("Units") }
            Row {
                spacing: M.spacingS
                Chip {
                    label: qsTr("Metric")
                    filled: AppSettings.units !== "imperial"
                    onClicked: AppSettings.units = "metric"
                }
                Chip {
                    label: qsTr("Imperial")
                    filled: AppSettings.units === "imperial"
                    onClicked: AppSettings.units = "imperial"
                }
            }
        }
    }
}
