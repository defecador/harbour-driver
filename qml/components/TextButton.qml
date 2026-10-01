import QtQuick 2.6
import "../theme"

Item {
    id: root
    property string text: ""
    property color color_: M.primary
    signal clicked()

    height: 40 * M.dp
    width: label.implicitWidth + M.spacingM * 2

    MText {
        id: label
        anchors.centerIn: parent
        text: root.text
        variant: "button"
        color: root.color_
    }

    Ripple {
        rippleColor: Qt.rgba(0, 0, 0, 0.1)
        onClicked: root.clicked()
    }
}
