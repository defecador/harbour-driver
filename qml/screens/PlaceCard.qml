import QtQuick 2.6
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

// Map stays visible through this screen: only a back button and a bottom
// card are drawn, WeGo-style, rather than a full opaque page.
Item {
    id: root
    property var place: ({})
    signal directionsClicked(var place)
    signal closed()

    RoundIconButton {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: M.spacingM
        iconName: "arrow_back"
        onClicked: root.closed()
    }

    Card {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: M.spacingM
        height: content.height + M.spacingM * 2

        Column {
            id: content
            width: parent.width - M.spacingM * 2
            x: M.spacingM
            y: M.spacingM
            spacing: M.spacingS

            MText {
                width: parent.width
                variant: "title"
                text: root.place.title || ""
                wrapMode: Text.WordWrap
            }
            MText {
                width: parent.width
                visible: (root.place.address || "").length > 0
                variant: "body"
                color: M.inkMuted
                text: root.place.address || ""
                wrapMode: Text.WordWrap
            }
            MText {
                visible: root.place.distance !== undefined && root.place.distance >= 0
                variant: "caption"
                color: M.inkMuted
                text: Fmt.distance(root.place.distance || 0, AppSettings.units)
            }

            Item { width: 1; height: M.spacingXs }

            PrimaryButton {
                text: qsTr("Directions")
                iconName: "directions"
                onClicked: root.directionsClicked(root.place)
            }

            Row {
                spacing: M.spacingM
                TextButton {
                    text: qsTr("Set as Home")
                    onClicked: AppSettings.home = root.place
                }
                TextButton {
                    text: qsTr("Set as Work")
                    onClicked: AppSettings.work = root.place
                }
            }
        }
    }
}
