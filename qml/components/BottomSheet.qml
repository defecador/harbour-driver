import QtQuick 2.6
import QtGraphicalEffects 1.0
import "../theme"

// Draggable bottom sheet with three snap points: peek, half and full.
// The background rectangle is taller than the visible sheet and anchored so its
// bottom corners fall off-screen, giving the illusion of only-top rounded corners
// without needing per-corner radius (not available in Qt 5.6).
Item {
    id: root
    x: sidePanel && sheetState === "hidden" ? -width : 0
    Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    width: sidePanel ? panelWidth : parent.width
    height: parent.height

    // Wide windows: a full-height panel on the left instead of a bottom sheet.
    property bool sidePanel: false
    property real panelWidth: 420 * M.dp
    property real peekHeight: 172 * M.dp
    property real halfFraction: 0.55
    property real fullFraction: 0.92
    property string sheetState: "peek"   // hidden | peek | half | full
    default property alias content: contentColumn.data

    signal dragged()

    // A hidden sheet is pushed this far below the window, so its handle does
    // not show in the system bar area under the app.
    readonly property real hiddenOffset: 80 * M.dp
    readonly property real halfHeight: parent.height * halfFraction
    readonly property real fullHeight: parent.height * fullFraction

    function heightFor(s) {
        if (s === "full") return fullHeight
        if (s === "half") return halfHeight
        if (s === "hidden") return -hiddenOffset
        return peekHeight
    }

    property real sheetHeight: heightFor(sheetState)
    Behavior on sheetHeight {
        enabled: !drag.dragging
        NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
    }

    function snapTo(s) {
        sheetState = s
    }

    y: sidePanel ? 0 : parent.height - sheetHeight

    RectangularGlow {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 24 * M.dp
        visible: !M.restyled
        glowRadius: 10 * M.dp
        spread: 0.2
        color: Qt.rgba(0, 0, 0, 0.18)
    }

    Rectangle {
        anchors.fill: parent
        radius: M.radiusSheet
        color: M.surface
        border.width: M.restyled ? M.borderW : 0
        border.color: M.borderColor
    }

    // Drag-catcher: sits behind contentColumn and spans the whole sheet, so a
    // swipe starting almost anywhere (search pill margins, between chips, the
    // handle) drags the sheet. Buttons/chips still get their own taps first,
    // since they're separate MouseAreas nested on top of this one.
    MouseArea {
        id: drag
        anchors.fill: parent
        // Also present in the side panel (where it does not drag anything):
        // it keeps touches on the panel from reaching the map beneath.
        property bool dragging: false
        property real startGlobalY: 0
        property real startHeight: 0

        // mouse.y is local to this MouseArea, which moves with root as the drag
        // itself changes root.y (y: parent.height - sheetHeight) - using it directly
        // would feed the drag's own output back into its input and make the sheet
        // jitter. Mapping into root.parent (stationary) breaks that feedback loop.
        onPressed: {
            if (root.sidePanel) return
            dragging = true
            startGlobalY = mapToItem(root.parent, mouse.x, mouse.y).y
            startHeight = root.sheetHeight
        }
        onPositionChanged: {
            if (!dragging) return
            var curGlobalY = mapToItem(root.parent, mouse.x, mouse.y).y
            var delta = startGlobalY - curGlobalY
            var h = startHeight + delta
            h = Math.max(0, Math.min(root.fullHeight, h))
            root.sheetHeight = h
            root.dragged()
        }
        onReleased: settle()
        onCanceled: settle()

        function settle() {
            if (!dragging) return
            dragging = false
            var h = root.sheetHeight
            if (h < root.peekHeight * 0.5) {
                root.sheetState = "hidden"
                root.sheetHeight = root.heightFor("hidden")
                return
            }
            var mid = (root.peekHeight + root.halfHeight) / 2
            var mid2 = (root.halfHeight + root.fullHeight) / 2
            if (h < mid) root.sheetState = "peek"
            else if (h < mid2) root.sheetState = "half"
            else root.sheetState = "full"
        }
    }

    // Hides the sheet / panel and brings it back. A swipe on the sheet only
    // works where nothing else takes the touch (buttons and the search pill
    // do), so this gives a dependable way to do it.
    Rectangle {
        id: tab
        z: 5
        x: root.sidePanel ? root.width : (root.width - width) / 2
        y: root.sidePanel ? 24 * M.dp : -height - (root.sheetState === "hidden" ? root.hiddenOffset : 0)
        width: root.sidePanel ? 28 * M.dp : 64 * M.dp
        height: root.sidePanel ? 56 * M.dp : 28 * M.dp
        radius: root.sidePanel || M.restyled ? 0 : 8 * M.dp
        color: M.surface
        border.width: M.restyled ? M.borderW : 1
        border.color: M.restyled ? M.borderColor : M.outline
        Icon {
            anchors.centerIn: parent
            size: 22 * M.dp
            color: M.ink
            name: root.sidePanel ? (root.sheetState === "hidden" ? "chevron_right" : "chevron_left")
                             : (root.sheetState === "hidden" ? "expand_less" : "expand_more")
        }
        MouseArea {
            anchors.fill: parent
            anchors.margins: -8 * M.dp
            onClicked: root.sheetState = root.sheetState === "hidden" ? "peek" : "hidden"
        }
    }

    Item {
        id: handleArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 28 * M.dp

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 8 * M.dp
            width: 36 * M.dp
            height: 4 * M.dp
            radius: 2 * M.dp
            color: M.outline
        }
    }

    Column {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: handleArea.bottom
        anchors.bottom: parent.bottom
        clip: true
    }
}
