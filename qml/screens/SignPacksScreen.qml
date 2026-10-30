import QtQuick 2.6
import "../theme"
import "../components"

// Choose which countries' stop sign, give-way sign and speed camera data is
// downloaded to the device (see src/SignPacks.h).
Item {
    id: root
    signal closed()

    property string filter: ""
    // The continent being browsed; "" shows the list of continents.
    property string continent: ""
    property string errorText: ""

    function fmtSize(bytes) {
        if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + " MB"
        return Math.max(1, Math.round(bytes / 1024)) + " kB"
    }

    function fmtDate(d) {
        var s = "" + d
        return s.length === 8 ? s.substring(0, 4) + "-" + s.substring(4, 6) + "-" + s.substring(6, 8) : ""
    }

    function continentOf(p) {
        return p.continent || qsTr("Other")
    }

    // Continents with how many countries and downloads each has; typing a
    // name searches all of them, and a chosen continent lists its countries.
    function rows(list, text, chosen) {
        var out = []
        var q = text.toLowerCase()
        var i
        if (q.length > 0 || chosen.length > 0) {
            for (i = 0; i < list.length; i++) {
                if (q.length > 0 ? list[i].name.toLowerCase().indexOf(q) >= 0 : root.continentOf(list[i]) === chosen)
                    out.push(list[i])
            }
            return out
        }
        var seen = {}
        for (i = 0; i < list.length; i++) {
            var c = root.continentOf(list[i])
            if (!seen[c]) {
                seen[c] = {"isContinent": true, "name": c, "total": 0, "installed": 0}
                out.push(seen[c])
            }
            seen[c].total++
            if (list[i].installed)
                seen[c].installed++
        }
        out.sort(function(a, b) { return a.name.localeCompare(b.name) })
        return out
    }

    Component.onCompleted: SignPacks.refreshIndex()

    Connections {
        target: SignPacks
        onError: root.errorText = message
    }

    Rectangle { anchors.fill: parent; color: M.bg }

    Column {
        anchors.fill: parent

        Item { width: 1; height: M.topInset }

        Item {
            width: parent.width
            height: 56 * M.dp
            RoundIconButton {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: M.spacingM
                iconName: "arrow_back"
                elevation: 0
                onClicked: {
                    if (root.continent.length > 0)
                        root.continent = ""
                    else
                        root.closed()
                }
            }
            MText {
                anchors.centerIn: parent
                variant: "subtitle"
                text: root.continent.length > 0 ? root.continent : qsTr("Sign data by country")
            }
        }
        Divider { width: parent.width }

        Item {
            width: parent.width
            height: searchField.height + M.spacingM * 2
            TextFieldM {
                id: searchField
                x: M.spacingM
                y: M.spacingM
                width: parent.width - M.spacingM * 2
                label: qsTr("Country")
                placeholder: qsTr("Search")
                onTextChanged: root.filter = text
            }
        }

        MText {
            visible: SignPacks.status.length > 0 || root.errorText.length > 0
            x: M.spacingM
            width: parent.width - M.spacingM * 2
            wrapMode: Text.WordWrap
            color: M.bad
            text: root.errorText.length > 0 ? root.errorText : SignPacks.status
        }

        ListView {
            id: list
            width: parent.width
            height: Math.max(0, root.height - y - footer.height)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.rows(SignPacks.packs, root.filter, root.continent)
            delegate: Item {
                id: row
                width: list.width
                height: 64 * M.dp
                readonly property bool busy: !modelData.isContinent && modelData.progress >= 0

                MouseArea {
                    anchors.fill: parent
                    enabled: modelData.isContinent === true
                    onClicked: root.continent = modelData.name
                }

                Column {
                    anchors.left: parent.left
                    anchors.right: actions.left
                    anchors.leftMargin: M.spacingM
                    anchors.rightMargin: M.spacingS
                    anchors.verticalCenter: parent.verticalCenter
                    MText {
                        width: parent.width
                        variant: "subtitle"
                        elide: Text.ElideRight
                        text: modelData.name
                    }
                    MText {
                        width: parent.width
                        variant: "caption"
                        color: M.inkMuted
                        elide: Text.ElideRight
                        text: modelData.isContinent
                              ? (modelData.total === 1 ? qsTr("1 country") : qsTr("%1 countries").arg(modelData.total))
                                + (modelData.installed > 0 ? ", " + qsTr("%1 downloaded").arg(modelData.installed) : "")
                              : row.busy ? qsTr("Downloading... %1%").arg(Math.round(modelData.progress * 100))
                              : modelData.installed
                                ? (modelData.updateAvailable ? qsTr("Downloaded - update available") : qsTr("Downloaded"))
                                  + " · " + root.fmtSize(modelData.bytes)
                                : root.fmtSize(modelData.bytes) + " · " + root.fmtDate(modelData.date)
                    }
                }

                Row {
                    id: actions
                    anchors.right: parent.right
                    anchors.rightMargin: M.spacingM
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: M.spacingS
                    Icon {
                        visible: modelData.isContinent === true
                        anchors.verticalCenter: parent.verticalCenter
                        name: "chevron_right"
                        size: 24 * M.dp
                        color: M.inkMuted
                    }
                    RoundIconButton {
                        visible: !modelData.isContinent && !row.busy && (!modelData.installed || modelData.updateAvailable)
                        elevation: 0
                        iconName: "download"
                        onClicked: {
                            root.errorText = ""
                            SignPacks.download(modelData.id)
                        }
                    }
                    RoundIconButton {
                        visible: !modelData.isContinent && !row.busy && modelData.installed === true
                        elevation: 0
                        iconName: "delete"
                        onClicked: SignPacks.remove(modelData.id)
                    }
                }

                Rectangle {
                    visible: row.busy
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: M.spacingM
                    height: 3 * M.dp
                    width: (parent.width - M.spacingM * 2) * Math.max(0.02, modelData.progress)
                    color: M.primary
                }

                Divider { anchors.bottom: parent.bottom; width: parent.width }
            }
        }

        Item {
            id: footer
            width: parent.width
            // Dropped while the keyboard has squeezed the window.
            visible: root.height > 480 * M.dp
            height: visible ? credit.height + M.spacingM * 2 : 0
            MText {
                id: credit
                x: M.spacingM
                y: M.spacingM
                width: parent.width - M.spacingM * 2
                wrapMode: Text.WordWrap
                variant: "caption"
                color: M.inkMuted
                text: qsTr("Stop sign, give-way sign and speed camera data (c) OpenStreetMap contributors, ODbL. Downloaded packs work offline.")
            }
        }
    }
}
