import QtQuick 2.6
import QtGraphicalEffects 1.0
import "../theme"

Item {
    id: root
    property string message: ""
    property string actionText: ""
    signal action()

    width: parent ? parent.width - M.spacingM * 2 : 300
    height: 48 * M.dp
    visible: false
    opacity: 0

    function show(msg, action_, durationMs) {
        root.message = msg
        root.actionText = action_ || ""
        root.visible = true
        root.opacity = 1
        hideTimer.interval = durationMs || 3000
        hideTimer.restart()
    }

    Timer {
        id: hideTimer
        onTriggered: root.opacity = 0
    }

    Behavior on opacity {
        NumberAnimation { duration: M.fadeMedium }
    }

    onOpacityChanged: if (opacity === 0) root.visible = false

    RectangularGlow {
        anchors.fill: body
        glowRadius: 8 * M.dp
        spread: 0.15
        color: Qt.rgba(0, 0, 0, 0.25)
        cornerRadius: body.radius + glowRadius
    }

    Rectangle {
        id: body
        anchors.fill: parent
        radius: M.radiusChip
        color: M.dark ? "#3a3a3f" : "#323234"
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: M.spacingM
        anchors.rightMargin: M.spacingS
        spacing: M.spacingS

        MText {
            width: parent.width - (actionBtn.visible ? actionBtn.width + M.spacingS : 0)
            anchors.verticalCenter: parent.verticalCenter
            text: root.message
            color: "#ffffff"
            wrapMode: Text.WordWrap
        }

        TextButton {
            id: actionBtn
            visible: root.actionText.length > 0
            anchors.verticalCenter: parent.verticalCenter
            text: root.actionText
            color_: "#8ab4f8"
            onClicked: {
                root.action()
                root.opacity = 0
            }
        }
    }
}
