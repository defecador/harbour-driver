import QtQuick 2.6

// Material-style press feedback + tap handling, meant to be the top-most child
// of a button/row so it can sit over any background/content underneath it.
Item {
    id: root
    anchors.fill: parent
    clip: true

    property color rippleColor: Qt.rgba(0, 0, 0, 0.12)
    property bool enabled: true

    signal clicked()
    signal pressAndHold()

    Rectangle {
        id: circle
        width: 10 * (AppTools ? AppTools.dp : 2)
        height: width
        radius: width / 2
        color: root.rippleColor
        opacity: 0
        scale: 0
        transformOrigin: Item.Center
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: root.enabled
        onPressed: {
            circle.x = mouse.x - circle.width / 2
            circle.y = mouse.y - circle.height / 2
            circle.opacity = 1
            circle.scale = 0
            growAnim.restart()
        }
        onReleased: fadeAnim.restart()
        onCanceled: fadeAnim.restart()
        onClicked: root.clicked()
        onPressAndHold: root.pressAndHold()
    }

    NumberAnimation {
        id: growAnim
        target: circle
        property: "scale"
        to: (Math.max(root.width, root.height) * 2.4) / Math.max(1, circle.width)
        duration: 380
        easing.type: Easing.OutQuad
    }

    SequentialAnimation {
        id: fadeAnim
        PauseAnimation { duration: 60 }
        NumberAnimation { target: circle; property: "opacity"; to: 0; duration: 220 }
    }
}
