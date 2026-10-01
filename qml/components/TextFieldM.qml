import QtQuick 2.6
import "../theme"

Item {
    id: root
    property alias text: input.text
    property string placeholder: ""
    property string label: ""
    property bool echoModeMask: false
    signal accepted()

    height: (label.length > 0 ? 18 * M.dp : 0) + 44 * M.dp

    Column {
        anchors.fill: parent
        spacing: 2 * M.dp

        MText {
            visible: root.label.length > 0
            text: root.label
            variant: "caption"
            color: M.inkMuted
        }

        Rectangle {
            width: parent.width
            height: 44 * M.dp
            radius: M.radiusChip
            color: M.surfaceVariant
            border.width: input.activeFocus ? Math.max(1, 2 * M.dp) : (M.sign ? M.borderW : 0)
            border.color: input.activeFocus ? M.primary : M.borderColor

            TextInput {
                id: input
                anchors.fill: parent
                anchors.leftMargin: M.spacingM
                anchors.rightMargin: M.spacingM
                verticalAlignment: TextInput.AlignVCenter
                font.family: M.fontFamily
                font.pixelSize: M.sizeBody
                color: M.ink
                echoMode: root.echoModeMask ? TextInput.Password : TextInput.Normal
                clip: true
                selectByMouse: true
                onAccepted: root.accepted()

                MText {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: input.text.length === 0 && !input.activeFocus
                    text: root.placeholder
                    color: M.inkMuted
                }
            }
        }
    }
}
