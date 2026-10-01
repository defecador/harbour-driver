import QtQuick 2.6
import "../theme"

Item {
    id: root
    property string label: ""
    property string iconName: ""
    property color lineColor: M.primary
    property color textColor: "#ffffff"
    property real fontSize: M.sizeCaption

    width: row.width + M.spacingS * 2
    height: 22 * M.dp

    Rectangle {
        anchors.fill: parent
        radius: 6 * M.dp
        color: root.lineColor
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 2 * M.dp
        Icon {
            visible: root.iconName.length > 0
            name: root.iconName
            size: 14 * M.dp
            color: root.textColor
            anchors.verticalCenter: parent.verticalCenter
        }
        MText {
            text: root.label
            font.pixelSize: root.fontSize
            font.weight: M.weightSemiBold
            font.family: M.fontFamily
            color: root.textColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
