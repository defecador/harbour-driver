import QtQuick 2.6

// Minimal push/pop navigation stack with a WeGo-style slide transition.
// Qt 5.6 has no Controls 2 StackView, so this is built from Qt.createComponent directly.
Item {
    id: root
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    // 0 = the whole parent; a positive width makes the stack a side panel
    // (wide windows keep the map visible beside the screens).
    property real panelWidth: 0
    width: panelWidth > 0 ? panelWidth : parent.width
    property int depth: 0
    property var _stack: []

    function push(url, props) {
        var comp = Qt.createComponent(url)
        if (comp.status !== Component.Ready) {
            console.warn("ScreenStack: failed to load", url, comp.errorString())
            return null
        }
        var item = comp.createObject(root, props || {})
        // Bound, so a resized window (split screen, rotation) re-flows live.
        item.width = Qt.binding(function() { return root.width })
        item.height = Qt.binding(function() { return root.height })
        item.x = root.width
        item.z = _stack.length
        _stack.push(item)
        root.depth = _stack.length
        pushAnim.stop()
        pushAnim.target = item
        pushAnim.from = root.width
        pushAnim.to = 0
        pushAnim.start()
        return item
    }

    function pop() {
        if (_stack.length === 0) return
        var item = _stack.pop()
        root.depth = _stack.length
        popAnim.stop()
        popAnim.item = item
        popAnim.target = item
        popAnim.from = 0
        popAnim.to = root.width
        popAnim.start()
    }

    function clear() {
        pushAnim.stop()
        popAnim.stop()
        while (_stack.length > 0) {
            var item = _stack.pop()
            item.destroy()
        }
        root.depth = 0
    }

    NumberAnimation {
        id: pushAnim
        property: "x"
        duration: 260
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: popAnim
        property: "x"
        duration: 220
        easing.type: Easing.InCubic
        property var item: null
        onStopped: {
            if (item) {
                item.destroy()
                item = null
            }
        }
    }
}
