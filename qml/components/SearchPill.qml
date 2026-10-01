import QtQuick 2.6
import QtGraphicalEffects 1.0
import "../theme"

// The "Where to?" pill on Home (editable: false, tap to open Search) and the
// live search box on the Search screen itself (editable: true).
Item {
    id: root
    property bool editable: false
    property alias text: input.text
    property string placeholder: qsTr("Where to?")
    property bool showBack: false
    property real elevation: 1
    signal clicked()
    signal submitted()
    signal backClicked()
    signal cleared()

    height: 48 * M.dp

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
        radius: M.radiusPill
        color: M.surface
        border.width: M.restyled ? M.borderW : 0
        border.color: M.borderColor
    }

    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: M.spacingS
        anchors.rightMargin: M.spacingS
        spacing: M.spacingXs

        RoundIconButton {
            iconName: root.showBack ? "arrow_back" : "search"
            elevation: 0
            bg: "transparent"
            anchors.verticalCenter: parent.verticalCenter
            visible: root.editable
            onClicked: root.backClicked()
        }

        Icon {
            visible: !root.editable
            name: "search"
            size: 22 * M.dp
            color: M.inkMuted
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: M.spacingS
        }

        Item {
            width: parent.width - (root.editable ? 44 * M.dp : 22 * M.dp + M.spacingM)
                   - (root.editable && input.text.length > 0 ? 44 * M.dp : 0) - M.spacingS
            height: parent.height
            anchors.verticalCenter: parent.verticalCenter

            TextInput {
                id: input
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: root.editable ? 0 : M.spacingS
                visible: root.editable
                enabled: root.editable
                font.family: M.fontFamily
                font.pixelSize: M.sizeBody
                color: M.ink
                clip: true
                selectByMouse: true
                onAccepted: root.submitted()
            }

            MText {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: root.editable ? 0 : M.spacingS
                visible: root.editable ? input.text.length === 0 : true
                text: root.editable ? root.placeholder : (root.text.length > 0 ? root.text : root.placeholder)
                color: M.inkMuted
            }
        }

        RoundIconButton {
            visible: root.editable && input.text.length > 0
            iconName: "close"
            elevation: 0
            bg: "transparent"
            iconSize: 18 * M.dp
            anchors.verticalCenter: parent.verticalCenter
            onClicked: {
                input.text = ""
                root.cleared()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: !root.editable
        onClicked: root.clicked()
    }
}
