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
        visible: root.elevation > 0 && !M.restyled
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

    // Restyled looks: a hard outline (street sign) or just a hairline under
    // the card (quiet), instead of a drop shadow.
    Rectangle {
        anchors.fill: body
        visible: M.sign
        radius: body.radius
        color: "transparent"
        border.width: M.borderW
        border.color: M.borderColor
    }
    Rectangle {
        visible: M.quiet
        anchors.left: body.left
        anchors.right: body.right
        anchors.bottom: body.bottom
        height: M.borderW
        color: M.borderColor
    }
}
