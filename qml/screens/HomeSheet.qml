import QtQuick 2.6
import "../theme"
import "../components"

Item {
    id: root
    width: parent ? parent.width : 360
    height: parent ? parent.height : 400

    // True when the sheet is tall enough to scroll its content instead of
    // being dragged (the side panel, or the sheet pulled all the way up).
    property bool scrollable: false

    signal searchTapped()
    signal placeTapped(var place)
    signal settingsTapped()
    signal aboutTapped()

    Flickable {
        anchors.fill: parent
        contentHeight: col.height
        clip: true
        interactive: root.scrollable && contentHeight > height
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: col
            width: parent.width
            spacing: M.spacingM

            Item { width: 1; height: M.spacingXs }

            SearchPill {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: M.spacingM
                anchors.rightMargin: M.spacingM
                onClicked: root.searchTapped()
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: M.spacingM
                spacing: M.spacingS

                Chip {
                    iconName: "home"
                    // A saved address turns the icon green; the label stays short.
                    iconColor: AppSettings.home && AppSettings.home.title ? M.good : M.inkMuted
                    label: qsTr("Home")
                    onClicked: {
                        if (AppSettings.home && AppSettings.home.title)
                            root.placeTapped(AppSettings.home)
                        else
                            root.searchTapped()
                    }
                }
                Chip {
                    iconName: "work"
                    iconColor: AppSettings.work && AppSettings.work.title ? M.good : M.inkMuted
                    label: qsTr("Work")
                    onClicked: {
                        if (AppSettings.work && AppSettings.work.title)
                            root.placeTapped(AppSettings.work)
                        else
                            root.searchTapped()
                    }
                }
                Chip {
                    iconName: "add_location_alt"
                    label: qsTr("Add place")
                    onClicked: root.searchTapped()
                }
            }

            // Places you saved ("Save place" on a place card): a row under the
            // buttons that swipes left and right. Press and hold removes one.
            Flickable {
                id: savedRow
                visible: AppSettings.savedPlaces.length > 0
                width: col.width
                height: visible ? 36 * M.dp : 0
                contentWidth: savedItems.width + M.spacingM * 2
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentWidth > width
                clip: true

                Row {
                    id: savedItems
                    x: M.spacingM
                    spacing: M.spacingS
                    Repeater {
                        model: AppSettings.savedPlaces
                        delegate: Chip {
                            iconName: "place"
                            iconColor: M.good
                            // First part of the address only: the full line is too long for a chip.
                            label: (modelData.title || "").split(",")[0]
                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.placeTapped(modelData)
                                onPressAndHold: AppSettings.removeSavedPlace(index)
                            }
                        }
                    }
                }
            }

            Item { width: 1; height: M.spacingXs }
            Divider { width: parent.width; visible: AppSettings.recents.length > 0 }

            Repeater {
                model: AppSettings.recents
                delegate: ListRow {
                    width: col.width
                    iconName: "history"
                    iconBg: M.surfaceVariant
                    title: modelData.title
                    subtitle: modelData.address
                    showDivider: index < AppSettings.recents.length - 1
                    onClicked: root.placeTapped(modelData)
                }
            }

            Divider { width: parent.width }

            ListRow {
                width: col.width
                iconName: "settings"
                iconBg: M.surfaceVariant
                title: qsTr("Settings")
                showChevron: true
                onClicked: root.settingsTapped()
            }
            ListRow {
                width: col.width
                iconName: "info"
                iconBg: M.surfaceVariant
                title: qsTr("About Driver")
                showChevron: true
                showDivider: false
                onClicked: root.aboutTapped()
            }

            Item { width: 1; height: M.spacingL }
        }
    }
}
