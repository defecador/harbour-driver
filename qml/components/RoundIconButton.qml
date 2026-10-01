import QtQuick 2.6
import QtGraphicalEffects 1.0
import "../theme"

Item {
    id: root
    property string iconName: ""
    property real iconSize: 22 * M.dp
    property color bg: M.surface
    property color iconColor: M.ink
    property real elevation: 1
    signal clicked()

    width: 44 * M.dp
    height: 44 * M.dp

    RectangularGlow {
        anchors.fill: circle
        visible: root.elevation > 0
        glowRadius: 5 * M.dp * root.elevation
        spread: 0.15
        color: Qt.rgba(0, 0, 0, 0.2)
        cornerRadius: circle.radius + glowRadius
    }

    Rectangle {
        id: circle
        anchors.fill: parent
        radius: width / 2
        color: root.bg
    }

    Icon {
        anchors.centerIn: parent
        name: root.iconName
        size: root.iconSize
        color: root.iconColor
    }

    Ripple {
        rippleColor: Qt.rgba(0, 0, 0, 0.15)
        onClicked: root.clicked()
    }
}
