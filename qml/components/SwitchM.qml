import QtQuick 2.6
import "../theme"

Item {
    id: root
    property bool checked: false
    signal toggled(bool value)

    width: 44 * M.dp
    height: 26 * M.dp

    Rectangle {
        id: track
        anchors.fill: parent
        radius: M.restyled ? 0 : height / 2
        color: root.checked ? M.buttonFill : (M.quiet ? M.surfaceVariant : M.outline)
        border.width: M.sign ? M.borderW : 0
        border.color: M.borderColor
        Behavior on color { ColorAnimation { duration: M.fadeShort } }
    }

    Rectangle {
        id: thumb
        width: 20 * M.dp
        height: 20 * M.dp
        radius: M.restyled ? 0 : width / 2
        color: M.sign ? "#111111" : "#ffffff"
        anchors.verticalCenter: parent.verticalCenter
        x: root.checked ? (parent.width - width - 3 * M.dp) : 3 * M.dp
        Behavior on x { NumberAnimation { duration: M.fadeShort; easing.type: Easing.OutQuad } }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
