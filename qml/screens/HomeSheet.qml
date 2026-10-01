import QtQuick 2.6
import "../theme"
import "../components"

Item {
    id: root
    width: parent ? parent.width : 360
    height: parent ? parent.height : 400

    signal searchTapped()
    signal placeTapped(var place)
    signal settingsTapped()
    signal aboutTapped()

    Flickable {
        anchors.fill: parent
        contentHeight: col.height
        clip: true
        interactive: false

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
                    label: AppSettings.home && AppSettings.home.title ? AppSettings.home.title : qsTr("Home")
                    onClicked: {
                        if (AppSettings.home && AppSettings.home.title)
                            root.placeTapped(AppSettings.home)
                        else
                            root.searchTapped()
                    }
                }
                Chip {
                    iconName: "work"
                    label: AppSettings.work && AppSettings.work.title ? AppSettings.work.title : qsTr("Work")
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
                title: qsTr("About Matka")
                showChevron: true
                showDivider: false
                onClicked: root.aboutTapped()
            }

            Item { width: 1; height: M.spacingL }
        }
    }
}
