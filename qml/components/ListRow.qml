import QtQuick 2.6
import "../theme"

Item {
    id: root
    property string iconName: ""
    property color iconBg: "transparent"
    property color iconColor: M.inkMuted
    property string title: ""
    property string subtitle: ""
    property string trailingText: ""
    property bool showChevron: false
    property bool showDivider: true
    signal clicked()

    width: parent ? parent.width : 320
    height: subtitle.length > 0 ? 64 * M.dp : 52 * M.dp

    Row {
        anchors.left: parent.left
        anchors.right: trailing.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: M.spacingM
        anchors.rightMargin: M.spacingS
        spacing: M.spacingM

        Rectangle {
            visible: root.iconName.length > 0
            width: 36 * M.dp
            height: 36 * M.dp
            radius: M.restyled ? 0 : width / 2
            color: root.iconBg
            anchors.verticalCenter: parent.verticalCenter
            Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 20 * M.dp
                color: root.iconColor
            }
        }

        Column {
            width: parent.width - (root.iconName.length > 0 ? (36 * M.dp + M.spacingM) : 0)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2 * M.dp
            MText {
                width: parent.width
                text: root.title
                variant: "subtitle"
            }
            MText {
                width: parent.width
                visible: root.subtitle.length > 0
                text: root.subtitle
                variant: "caption"
                color: M.inkMuted
            }
        }
    }

    Row {
        id: trailing
        anchors.right: parent.right
        anchors.rightMargin: M.spacingM
        anchors.verticalCenter: parent.verticalCenter
        spacing: M.spacingXs
        MText {
            visible: root.trailingText.length > 0
            text: root.trailingText
            variant: "caption"
            color: M.inkMuted
            anchors.verticalCenter: parent.verticalCenter
        }
        Icon {
            visible: root.showChevron
            name: "chevron_right"
            size: 20 * M.dp
            color: M.inkMuted
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Divider {
        visible: root.showDivider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.iconName.length > 0 ? (M.spacingM + 36 * M.dp + M.spacingM) : M.spacingM
    }

    Ripple {
        rippleColor: Qt.rgba(0, 0, 0, 0.06)
        onClicked: root.clicked()
    }
}
