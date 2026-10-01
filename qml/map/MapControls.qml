import QtQuick 2.6
import "../theme"
import "../components"

// Floating round buttons docked to the map's top or bottom-right corner.
Column {
    id: root
    spacing: M.spacingS
    signal layersClicked()
    signal locateClicked()
    property bool locating: false

    RoundIconButton {
        iconName: "layers"
        onClicked: root.layersClicked()
    }

    RoundIconButton {
        iconName: "my_location"
        iconColor: root.locating ? M.primary : M.ink
        onClicked: root.locateClicked()
    }
}
