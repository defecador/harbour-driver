import QtQuick 2.6
import QtGraphicalEffects 1.0
import "../theme"

Item {
    id: root
    property real radius: M.radiusCard
    property real elevation: 1
    default property alias content: body.data

    RectangularGlow {
        anchors.fill: body
        visible: root.elevation > 0
        glowRadius: 6 * M.dp * root.elevation
        spread: 0.15
        color: Qt.rgba(0, 0, 0, 0.18)
        cornerRadius: body.radius + glowRadius
    }

    Rectangle {
        id: body
        anchors.fill: parent
        radius: root.radius
        color: M.surface
        clip: true
    }
}
