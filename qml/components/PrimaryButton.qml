import QtQuick 2.6
import "../theme"

Item {
    id: root
    property string text: ""
    property string iconName: ""
    property bool enabled: true
    signal clicked()

    height: 48 * M.dp
    width: Math.max(120 * M.dp, row.width + M.spacingL * 2)
    opacity: enabled ? 1 : 0.45

    Rectangle {
        anchors.fill: parent
        radius: M.radiusPill
        color: M.primary
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: M.spacingXs
        Icon {
            visible: root.iconName.length > 0
            name: root.iconName
            size: 20 * M.dp
            color: M.primaryInk
            anchors.verticalCenter: parent.verticalCenter
        }
        MText {
            text: root.text
            variant: "button"
            color: M.primaryInk
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Ripple {
        enabled: root.enabled
        rippleColor: Qt.rgba(1, 1, 1, 0.25)
        onClicked: root.clicked()
    }
}
