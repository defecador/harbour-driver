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
        radius: height / 2
        color: root.checked ? M.primary : M.outline
        Behavior on color { ColorAnimation { duration: M.fadeShort } }
    }

    Rectangle {
        id: thumb
        width: 20 * M.dp
        height: 20 * M.dp
        radius: width / 2
        color: "#ffffff"
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
