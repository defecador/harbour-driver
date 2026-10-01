import QtQuick 2.6
import "../theme"

Item {
    id: root
    property string iconName: ""
    property string label: ""
    property bool filled: false
    // Icon colour; defaults to the chip's own text colours.
    property color iconColor: filled ? M.buttonInk : M.inkMuted
    signal clicked()

    width: row.width + M.spacingM * 2
    height: 36 * M.dp

    Rectangle {
        anchors.fill: parent
        radius: M.restyled ? 0 : height / 2
        color: root.filled ? M.buttonFill : M.surface
        border.width: M.sign ? M.borderW : (root.filled ? 0 : Math.max(1, Math.round(M.dp)))
        border.color: M.sign ? M.borderColor : M.outline
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: M.spacingXs
        Icon {
            visible: root.iconName.length > 0
            name: root.iconName
            size: 18 * M.dp
            color: root.iconColor
            anchors.verticalCenter: parent.verticalCenter
        }
        MText {
            text: root.label
            variant: "body"
            color: root.filled ? M.buttonInk : M.ink
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Ripple {
        rippleColor: Qt.rgba(0, 0, 0, 0.12)
        onClicked: root.clicked()
    }
}
