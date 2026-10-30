import QtQuick 2.6
import "../theme"

// Posted speed limit of the road being driven (RoadAlerts.speedLimit): a
// white roundel with a red ring.
Item {
    id: root
    property int limit: RoadAlerts.speedLimit
    readonly property int shown: AppSettings.units === "imperial" ? Math.round(limit / 1.609) : limit

    visible: limit > 0 && AppSettings.speedLimitSign
    width: 64 * M.dp
    height: width

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "#ffffff"
        border.width: width * 0.12
        border.color: "#d32f2f"
    }
    Text {
        anchors.centerIn: parent
        text: root.shown
        color: "#111111"
        font.bold: true
        font.pixelSize: root.width * (root.shown >= 100 ? 0.3 : 0.4)
    }
}
