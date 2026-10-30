import QtQuick 2.6
import "../theme"
import "../theme/Fmt.js" as Fmt

// Warning for the stop sign, give-way sign or speed camera ahead
// (RoadAlerts): a drawn road sign, what it is and how far away.
Card {
    id: root
    // "stop" | "giveWay" | "camera"
    property string kind: RoadAlerts.kind
    property int distance: RoadAlerts.distance
    // Speed limit in km/h shown beside a camera, 0 = unknown.
    property int maxspeed: RoadAlerts.maxspeed
    // Sign and distance only, for narrow spaces.
    property bool compact: false

    readonly property real s: 46 * M.dp
    readonly property string label: kind === "stop" ? qsTr("Stop sign")
                                  : kind === "giveWay" ? qsTr("Give way")
                                  : qsTr("Speed camera")
    readonly property int limit: AppSettings.units === "imperial" ? Math.round(maxspeed / 1.609) : maxspeed

    visible: kind !== ""
    elevation: 2
    width: row.width + M.spacingS * 2
    height: s + M.spacingS * 2

    Row {
        id: row
        anchors.centerIn: parent
        spacing: M.spacingS

        // The road sign itself.
        Item {
            width: root.s
            height: root.s

            // Stop: red octagon (a square turned 45 degrees and clipped to its own
            // bounding box is a regular octagon).
            Item {
                anchors.fill: parent
                visible: root.kind === "stop"
                Repeater {
                    model: [{"k": 1.0, "c": "#ffffff"}, {"k": 0.86, "c": "#d32f2f"}]
                    delegate: Item {
                        anchors.centerIn: parent
                        width: root.s * modelData.k
                        height: width
                        clip: true
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width
                            height: parent.height
                            rotation: 45
                            color: modelData.c
                        }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    text: "STOP"
                    color: "#ffffff"
                    font.bold: true
                    font.pixelSize: root.s * 0.27
                }
            }

            // Give way: white triangle with a red border, point down.
            Canvas {
                id: triangle
                anchors.fill: parent
                visible: root.kind === "giveWay"
                onWidthChanged: requestPaint()
                onVisibleChanged: if (visible) requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var w = width, h = height, m = w * 0.04
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(m, h * 0.1)
                    ctx.lineTo(w - m, h * 0.1)
                    ctx.lineTo(w / 2, h - m)
                    ctx.closePath()
                    ctx.fillStyle = "#ffffff"
                    ctx.fill()
                    ctx.lineWidth = w * 0.13
                    ctx.strokeStyle = "#d32f2f"
                    ctx.stroke()
                }
            }

            // Speed camera: dark square with a camera.
            Rectangle {
                anchors.fill: parent
                visible: root.kind === "camera"
                radius: M.restyled ? 0 : root.s * 0.14
                color: "#1c1c1e"
                Icon {
                    anchors.centerIn: parent
                    name: "photo_camera"
                    size: root.s * 0.6
                    color: "#ffffff"
                }
            }
        }

        // Speed limit roundel for cameras that have one.
        Rectangle {
            visible: root.kind === "camera" && root.maxspeed > 0
            anchors.verticalCenter: parent.verticalCenter
            width: root.s * 0.9
            height: width
            radius: width / 2
            color: "#ffffff"
            border.width: width * 0.14
            border.color: "#d32f2f"
            Text {
                anchors.centerIn: parent
                text: root.limit
                color: "#111111"
                font.bold: true
                font.pixelSize: parent.width * 0.38
            }
        }

        Column {
            visible: !root.compact
            anchors.verticalCenter: parent.verticalCenter
            MText { variant: "subtitle"; text: root.label }
            MText { variant: "caption"; color: M.inkMuted; text: Fmt.distance(root.distance, AppSettings.units) }
        }
        MText {
            visible: root.compact
            anchors.verticalCenter: parent.verticalCenter
            variant: "subtitle"
            text: Fmt.distance(root.distance, AppSettings.units)
        }
    }
}
