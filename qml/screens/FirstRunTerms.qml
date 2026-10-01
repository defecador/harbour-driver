import QtQuick 2.6
import "../theme"
import "../components"

Item {
    id: root

    Rectangle { anchors.fill: parent; color: M.bg }

    Column {
        anchors.centerIn: parent
        width: parent.width - M.spacingL * 2
        spacing: M.spacingM

        Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "directions"; size: 44 * M.dp; color: M.primary }

        MText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            variant: "title"
            text: qsTr("Welcome to Driver")
        }

        MText {
            width: parent.width
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: M.inkMuted
            text: qsTr("Driver plans trips using HERE's routing, transit and map services. By continuing, you agree to HERE's End User Terms and Privacy Policy, which apply once you add your own HERE API key in Settings.")
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: M.spacingM
            TextButton {
                text: qsTr("HERE Terms")
                onClicked: Qt.openUrlExternally("https://legal.here.com/en-gb/terms")
            }
            TextButton {
                text: qsTr("Privacy Policy")
                onClicked: Qt.openUrlExternally("https://legal.here.com/en-gb/privacy")
            }
        }

        PrimaryButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Continue")
            onClicked: AppSettings.termsAccepted = true
        }
    }
}
