import QtQuick 2.6
import "../theme"
import "../theme/Icons.js" as Icons

Text {
    id: root
    property string name: ""
    property real size: 24 * M.dp

    text: Icons.glyph(name)
    color: M.ink
    font.family: "Material Icons Round"
    font.pixelSize: size
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    width: size
    height: size
    renderType: Text.NativeRendering
}
