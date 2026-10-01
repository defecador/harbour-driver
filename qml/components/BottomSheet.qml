import QtQuick 2.6
import QtGraphicalEffects 1.0
import "../theme"

// Draggable bottom sheet with three snap points: peek, half and full.
// The background rectangle is taller than the visible sheet and anchored so its
// bottom corners fall off-screen, giving the illusion of only-top rounded corners
// without needing per-corner radius (not available in Qt 5.6).
Item {
    id: root
    anchors.left: parent.left
    anchors.right: parent.right
    height: parent.height

    property real peekHeight: 172 * M.dp
    property real halfFraction: 0.55
    property real fullFraction: 0.92
    property string sheetState: "peek"   // peek | half | full
    default property alias content: contentColumn.data

    signal dragged()

    readonly property real halfHeight: parent.height * halfFraction
    readonly property real fullHeight: parent.height * fullFraction

    function heightFor(s) {
        if (s === "full") return fullHeight
        if (s === "half") return halfHeight
        if (s === "hidden") return 0
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

    y: parent.height - sheetHeight

    RectangularGlow {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 24 * M.dp
        glowRadius: 10 * M.dp
        spread: 0.2
        color: Qt.rgba(0, 0, 0, 0.18)
    }

    Rectangle {
        anchors.fill: parent
        radius: M.radiusSheet
        color: M.surface
    }

    // Drag-catcher: sits behind contentColumn and spans the whole sheet, so a
    // swipe starting almost anywhere (search pill margins, between chips, the
    // handle) drags the sheet. Buttons/chips still get their own taps first,
    // since they're separate MouseAreas nested on top of this one.
    MouseArea {
        id: drag
        anchors.fill: parent
        property bool dragging: false
        property real startGlobalY: 0
        property real startHeight: 0

        // mouse.y is local to this MouseArea, which moves with root as the drag
        // itself changes root.y (y: parent.height - sheetHeight) - using it directly
        // would feed the drag's own output back into its input and make the sheet
        // jitter. Mapping into root.parent (stationary) breaks that feedback loop.
        onPressed: {
            dragging = true
            startGlobalY = mapToItem(root.parent, mouse.x, mouse.y).y
            startHeight = root.sheetHeight
        }
        onPositionChanged: {
            if (!dragging) return
            var curGlobalY = mapToItem(root.parent, mouse.x, mouse.y).y
            var delta = startGlobalY - curGlobalY
            var h = startHeight + delta
            h = Math.max(root.peekHeight, Math.min(root.fullHeight, h))
            root.sheetHeight = h
            root.dragged()
        }
        onReleased: settle()
        onCanceled: settle()

        function settle() {
            dragging = false
            var h = root.sheetHeight
            var mid = (root.peekHeight + root.halfHeight) / 2
            var mid2 = (root.halfHeight + root.fullHeight) / 2
            if (h < mid) root.sheetState = "peek"
            else if (h < mid2) root.sheetState = "half"
            else root.sheetState = "full"
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
