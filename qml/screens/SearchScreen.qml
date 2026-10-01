import QtQuick 2.6
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

Item {
    id: root
    signal placeChosen(var place)
    signal closed()

    function resultIcon(resultType) {
        if (resultType === "place") return "place"
        if (resultType === "street") return "directions"
        return "search"
    }

    Rectangle { anchors.fill: parent; color: M.bg }

    // About 1 cm of breathing room above the search field.
    readonly property real topGap: M.topInset

    Column {
        anchors.fill: parent

        Item { width: 1; height: root.topGap }

        Item {
            width: parent.width
            height: 56 * M.dp
            SearchPill {
                id: pill
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: M.spacingM
                anchors.rightMargin: M.spacingM
                editable: true
                showBack: true
                text: SearchController.query
                onTextChanged: SearchController.query = text
                onSubmitted: SearchController.submit()
                onBackClicked: root.closed()
                onCleared: SearchController.query = ""
                Component.onCompleted: text = SearchController.query
            }
        }

        MText {
            visible: SearchController.error.length > 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: M.spacingM
            text: SearchController.error
            color: M.bad
            wrapMode: Text.WordWrap
        }

        MText {
            visible: SearchController.query.length === 0 && AppSettings.recents.length > 0
            anchors.left: parent.left
            anchors.margins: M.spacingM
            text: qsTr("Recent")
            variant: "caption"
            color: M.inkMuted
        }

        ListView {
            id: list
            width: parent.width
            height: parent.height - 56 * M.dp - root.topGap
            clip: true
            model: SearchController.query.length === 0 ? AppSettings.recents : SearchController.results
            delegate: ListRow {
                width: list.width
                iconName: root.resultIcon(modelData.resultType)
                iconBg: M.surfaceVariant
                title: modelData.title
                subtitle: modelData.address
                trailingText: modelData.distance !== undefined && modelData.distance >= 0 ? Fmt.distance(modelData.distance, AppSettings.units) : ""
                onClicked: root.placeChosen(modelData)
            }
        }
    }
}
