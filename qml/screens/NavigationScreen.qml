import QtQuick 2.6
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

// Full-screen turn-by-turn overlay, pushed on top of the (still visible,
// edge-to-edge) map. Deliberately has no opaque background of its own —
// unlike the other pushed screens — so the map shows through between the
// top banner and the bottom bar.
Item {
    id: root
    signal closed()
    // Emitted each simulated tick so AppRoot can steer the map's follow
    // camera (which lives outside this screen, alongside MapView).
    signal positionSimulated(real lat, real lng, real heading)
    // Emitted when the Follow/Overview button is tapped; AppRoot applies it
    // to the map and, on a later out-of-band pan, sets `following` back to
    // false itself (see MapView.followInterrupted).
    signal followToggled(bool wantFollow)

    property bool simulate: false
    property bool following: true
    // Place the user tapped on the map to maybe add as a stop (walk/bike
    // only); AppRoot fills it in and listens for the two signals below.
    property var stopCandidate: null
    signal addStopRequested()
    signal stopCandidateDismissed()
    // Camera-distance slider for the pitched driving view (zoom level:
    // higher = closer). AppRoot supplies povControls and listens for changes.
    property bool povControls: false
    property real povZoom: 18.3
    readonly property real povZoomMin: 16.8
    readonly property real povZoomMax: 21.0
    property bool muted: false
    // Accuracy (m) of the latest GPS fix, -1 before any; AppRoot keeps it up to date.
    property real gpsAccuracy: -1

    Haptics { id: haptics }

    // Keeps the CPU, the 1s transit tick and GPS running with the screen off,
    // holds the display on when it should be, and publishes the transit
    // alerts (see components/NavSystem.qml, platform specific). While riding
    // transit the screen may sleep (the choice made for this feature): the
    // alight/missed notification wakes the rider back up in time.
    NavSystem {
        id: navSystem
        active: Navigator.state === "guiding" || Navigator.state === "waitingForGps"
                || Navigator.state === "rerouting"
        screenOn: active && (Navigator.mode !== "transit" || Navigator.transitPhase !== "ride")
    }

    Connections {
        target: Navigator
        onAnnounce: {
            if (!root.muted && AppSettings.vibration) haptics.vibrate()
            if (kind === "alight" || kind === "alightNow" || kind === "missed")
                navSystem.notify(text)
        }
    }

    // --- Simulated GPS (dev/testing only) -----------------------------
    // Walks the planned route's own coordinate list at a plausible speed so
    // navigation can be exercised and screenshotted without a real drive.
    property var _simCoords: []
    property real _simTraveled: 0
    property real _simTotalLength: 0
    property real _simSpeed: 14

    function _simSetup() {
        var itin = TripPlanner.selected
        root._simCoords = (itin && itin.coordinates) ? itin.coordinates : []
        root._simTraveled = 0
        var mode = itin ? itin.mode : "car"
        // Transit walks its own flattened coordinate list (every leg's own
        // polyline back to back) at one average pace: Navigator's own tick
        // (sped up via setSimulationClock, see AppRoot.startNavigation) is
        // what actually drives wait/ride phase timing, not this GPS walk —
        // so this only needs to be plausible, not leg-accurate.
        root._simSpeed = mode === "pedestrian" ? 1.4 : mode === "bicycle" ? 4.2 : mode === "transit" ? 6 : 14
        var total = 0
        for (var i = 0; i < root._simCoords.length - 1; i++)
            total += root._simCoords[i].distanceTo(root._simCoords[i + 1])
        root._simTotalLength = total
    }

    function _simStep(deltaMeters) {
        if (root._simCoords.length < 2)
            return null
        root._simTraveled = Math.min(root._simTraveled + deltaMeters, root._simTotalLength)
        var acc = 0
        for (var i = 0; i < root._simCoords.length - 1; i++) {
            var segLen = root._simCoords[i].distanceTo(root._simCoords[i + 1])
            if (acc + segLen >= root._simTraveled || i === root._simCoords.length - 2) {
                var t = segLen > 0 ? Math.min(1, Math.max(0, (root._simTraveled - acc) / segLen)) : 1
                var a = root._simCoords[i], b = root._simCoords[i + 1]
                return {
                    lat: a.latitude + (b.latitude - a.latitude) * t,
                    lng: a.longitude + (b.longitude - a.longitude) * t,
                    heading: a.azimuthTo(b),
                    done: root._simTraveled >= root._simTotalLength
                }
            }
            acc += segLen
        }
        return null
    }

    Component.onCompleted: {
        if (root.simulate)
            root._simSetup()
    }

    Timer {
        id: simTimer
        interval: 250
        repeat: true
        // Played back at 4x so a multi-minute trip finishes in seconds.
        running: root.simulate && root._simCoords.length > 1 && Navigator.state !== "arrived"
        onTriggered: {
            var p = root._simStep(root._simSpeed * 4 * (interval / 1000))
            if (!p)
                return
            Navigator.updatePosition(p.lat, p.lng, 5, root._simSpeed, p.heading)
            root.positionSimulated(p.lat, p.lng, p.heading)
            if (p.done)
                stop()
        }
    }

    // Whether the current leg is turn-by-turn (car/walk/bike, or a transit
    // walking leg) rather than one of the wait/ride/alight transit phases.
    readonly property bool onFootOrWheels: Navigator.mode !== "transit" || Navigator.transitPhase === "walk"

    // --- Top banner ----------------------------------------------------
    // A square card in the top-left corner while turn-by-turn guidance is
    // showing; its size follows the window (so it shrinks in split screen).
    // The other states (transit, waiting for GPS, ...) use a short, wider
    // card. Motorway ramps and exits get the blue motorway-sign look.
    readonly property bool turnCard: onFootOrWheels && Navigator.state === "guiding" && !Navigator.rerouting
    readonly property real cardSide: Math.max(104 * M.dp, Math.min(240 * M.dp, Math.min(width, height) * 0.36))
    readonly property var roadSign: Fmt.roadSign(Navigator.maneuverAction, Navigator.instruction)
    readonly property bool motorway: turnCard && Navigator.mode === "car" && roadSign.highway

    Rectangle {
        id: banner
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: M.spacingS
        width: root.turnCard ? root.cardSide
                             : Math.min(root.width - 2 * M.spacingS, Math.max(root.cardSide * 2, 300 * M.dp))
        height: root.turnCard ? root.cardSide : Math.max(80 * M.dp, root.cardSide * 0.6)
        radius: M.restyled ? 0 : 12 * M.dp
        // Tints amber while off the planned route or hurrying for a
        // connection, green once it's time to get off, blue for motorway
        // ramps and exits.
        color: Navigator.mode === "transit" && Navigator.transitPhase === "alight" ? M.good
                : (Navigator.state === "guiding" && (Navigator.offRoute || (root.onFootOrWheels && Navigator.hurry))) ? M.traffic
                : root.motorway ? "#00549f"
                : M.banner
        visible: Navigator.state !== "arrived"

        Rectangle {
            visible: M.sign && !root.motorway
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: M.borderW * 1.5
            border.color: M.borderColor
        }
        Rectangle {
            visible: M.quiet
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4 * M.dp
            color: M.primary
        }
        // The white inner frame of a motorway sign.
        Rectangle {
            visible: root.motorway
            anchors.fill: parent
            anchors.margins: 3 * M.dp
            radius: Math.max(0, parent.radius - 3 * M.dp)
            color: "transparent"
            border.width: Math.max(2, 2 * M.dp)
            border.color: "#ffffff"
        }

        Item {
            id: turnContent
            visible: root.turnCard
            anchors.fill: parent
            readonly property real pad: root.cardSide * 0.08

            Icon {
                id: turnIcon
                x: turnContent.pad
                y: turnContent.pad
                name: Fmt.turnIcon(Navigator.maneuverAction, Navigator.maneuverDirection)
                size: root.cardSide * 0.34
                color: "#ffffff"
            }
            MText {
                id: turnDistance
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: turnIcon.bottom
                anchors.margins: turnContent.pad
                anchors.topMargin: 0
                font.pixelSize: root.cardSide * 0.17
                font.weight: M.weightSemiBold
                color: "#ffffff"
                text: Fmt.distance(Navigator.distanceToManeuver, AppSettings.units)
            }
            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: turnDistance.bottom
                anchors.bottom: parent.bottom
                anchors.margins: turnContent.pad
                anchors.topMargin: 0
                clip: true
                spacing: 1
                MText {
                    visible: root.motorway && (root.roadSign.ref !== "" || root.roadSign.exitNo !== "")
                    width: parent.width
                    font.pixelSize: root.cardSide * 0.115
                    font.weight: M.weightSemiBold
                    color: "#ffffff"
                    text: (root.roadSign.exitNo !== "" ? qsTr("Exit %1").arg(root.roadSign.exitNo) : "")
                          + (root.roadSign.exitNo !== "" && root.roadSign.ref !== "" ? " · " : "")
                          + root.roadSign.ref
                }
                MText {
                    width: parent.width
                    font.pixelSize: root.cardSide * 0.085
                    color: Qt.rgba(1, 1, 1, 0.92)
                    wrapMode: Text.WordWrap
                    maximumLineCount: root.motorway && root.roadSign.ref !== "" ? 2 : 3
                    elide: Text.ElideRight
                    // First sentence only; the full instruction is still spoken.
                    text: Navigator.mode === "transit" && Navigator.hurry
                          ? qsTr("Hurry — %1 departs in %2").arg(Navigator.lineName).arg(Fmt.duration(Navigator.secondsToDeparture))
                          : (root.motorway && root.roadSign.toward !== "" ? root.roadSign.toward
                                                                         : (Navigator.instruction || "").split(". ")[0])
                }
            }
        }

        Column {
            id: thenChip
            visible: root.turnCard && Navigator.hasThen
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root.cardSide * 0.08
            spacing: 0
            MText {
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: root.cardSide * 0.08
                color: Qt.rgba(1, 1, 1, 0.85)
                text: qsTr("Then")
            }
            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: Fmt.turnIcon(Navigator.thenAction, Navigator.thenDirection)
                size: root.cardSide * 0.2
                color: "#ffffff"
            }
        }

        // --- Transit: waiting to board --------------------------------
        Row {
            visible: Navigator.mode === "transit" && Navigator.transitPhase === "wait" && !Navigator.missedConnection
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: M.spacingM
            spacing: M.spacingM

            LineBadge {
                anchors.verticalCenter: parent.verticalCenter
                label: Navigator.lineName || Fmt.transitLabel(Navigator.transportMode)
                iconName: Fmt.transitIcon(Navigator.transportMode)
                lineColor: Fmt.hexColor(Navigator.lineColor, "#ffffff")
                textColor: Fmt.hexColor(Navigator.lineTextColor, M.primary)
            }
            Column {
                width: parent.width - 120 * M.dp
                spacing: 2 * M.dp
                MText {
                    variant: "title"
                    color: "#ffffff"
                    text: qsTr("→ %1").arg(Navigator.headsign)
                }
                MText {
                    variant: "caption"
                    color: Qt.rgba(1, 1, 1, 0.9)
                    text: (Navigator.platform ? qsTr("Platform %1 · ").arg(Navigator.platform) : "")
                          + qsTr("departs %1").arg(Navigator.departureText)
                          + (Navigator.departureDelay > 0 ? " (+" + Math.round(Navigator.departureDelay / 60) + ")" : "")
                }
            }
            MText {
                anchors.verticalCenter: parent.verticalCenter
                variant: "title"
                color: "#ffffff"
                text: Fmt.duration(Navigator.secondsToDeparture)
            }
        }

        // --- Transit: missed connection --------------------------------
        Column {
            visible: Navigator.mode === "transit" && Navigator.missedConnection
            anchors.centerIn: parent
            spacing: M.spacingS
            MText { anchors.horizontalCenter: parent.horizontalCenter; variant: "subtitle"; color: "#ffffff"; text: qsTr("Missed %1?").arg(Navigator.lineName) }
            TextButton {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Find next connection")
                color_: "#ffffff"
                onClicked: Navigator.replanTransit()
            }
        }

        // --- Transit: riding / about to alight --------------------------
        Row {
            visible: Navigator.mode === "transit" && (Navigator.transitPhase === "ride" || Navigator.transitPhase === "alight")
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: M.spacingM
            spacing: M.spacingM

            LineBadge {
                anchors.verticalCenter: parent.verticalCenter
                label: Navigator.lineName || Fmt.transitLabel(Navigator.transportMode)
                iconName: Fmt.transitIcon(Navigator.transportMode)
                lineColor: Fmt.hexColor(Navigator.lineColor, "#ffffff")
                textColor: Fmt.hexColor(Navigator.lineTextColor, M.primary)
            }
            Column {
                width: parent.width - 60 * M.dp
                spacing: 2 * M.dp
                MText {
                    variant: "title"
                    color: "#ffffff"
                    text: Navigator.transitPhase === "alight"
                          ? qsTr("Get off at the next stop")
                          : (Navigator.stopsRemaining === 1 ? qsTr("1 stop to %1").arg(Navigator.alightStopName)
                                                             : qsTr("%1 stops to %2").arg(Navigator.stopsRemaining).arg(Navigator.alightStopName))
                }
                MText {
                    variant: "caption"
                    visible: Navigator.transitPhase === "ride" && Navigator.nextStopName.length > 0
                    color: Qt.rgba(1, 1, 1, 0.9)
                    text: qsTr("Next: %1").arg(Navigator.nextStopName)
                }
            }
        }

        Column {
            visible: Navigator.state === "waitingForGps"
            anchors.centerIn: parent
            spacing: M.spacingXs
            MText { anchors.horizontalCenter: parent.horizontalCenter; variant: "subtitle"; color: "#ffffff"; text: qsTr("Waiting for GPS…") }
            MText {
                anchors.horizontalCenter: parent.horizontalCenter
                variant: "caption"
                color: Qt.rgba(1, 1, 1, 0.85)
                text: root.gpsAccuracy < 0 ? qsTr("No location yet")
                                           : qsTr("Accuracy %1 m, too rough").arg(Math.round(root.gpsAccuracy))
            }
        }

        Column {
            // `rerouting` is a separate bool, not a state value: Navigator
            // stays in "guiding" the whole time a reroute request is out.
            visible: root.onFootOrWheels && Navigator.state === "guiding" && Navigator.rerouting
            anchors.centerIn: parent
            spacing: M.spacingXs
            MText { anchors.horizontalCenter: parent.horizontalCenter; variant: "subtitle"; color: "#ffffff"; text: qsTr("Recalculating…") }
        }

        Column {
            visible: Navigator.state === "error"
            anchors.centerIn: parent
            spacing: M.spacingXs
            MText { anchors.horizontalCenter: parent.horizontalCenter; variant: "subtitle"; color: "#ffffff"; text: qsTr("Couldn't start navigation") }
        }
    }

    RoundIconButton {
        id: muteButton
        anchors.left: parent.left
        anchors.top: banner.bottom
        anchors.margins: M.spacingM
        iconName: root.muted ? "volume_off" : "volume_up"
        onClicked: root.muted = !root.muted
    }

    RoundIconButton {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: M.spacingM
        iconName: root.following ? "fullscreen" : "navigation"
        onClicked: {
            root.following = !root.following
            root.followToggled(root.following)
        }
    }

    // Stop / give-way sign or speed camera ahead (RoadAlerts). Right of the
    // turn card when there is room before the follow button, else under the
    // mute button.
    RoadAlertChip {
        id: alertChip
        readonly property real room: root.width - (banner.x + banner.width) - M.spacingS
                                     - (44 * M.dp + 2 * M.spacingM)
        readonly property bool beside: room >= 124 * M.dp
        compact: room < 230 * M.dp
        x: beside ? banner.x + banner.width + M.spacingS : M.spacingM
        y: beside ? banner.y : muteButton.y + muteButton.height + M.spacingS
        z: 5
        visible: Navigator.mode === "car" && Navigator.state !== "arrived" && RoadAlerts.kind !== ""
    }

    // Posted speed limit of the road, lower left above the trip bar.
    SpeedLimitSign {
        anchors.left: parent.left
        anchors.bottom: bottomBar.top
        anchors.margins: M.spacingM
        visible: Navigator.mode === "car" && Navigator.state !== "arrived" && RoadAlerts.speedLimit > 0
                 && AppSettings.speedLimitSign
    }

    // Closer/further slider for the 3D driving view (top = closer).
    Rectangle {
        id: povSlider
        visible: root.povControls && root.following
        anchors.right: parent.right
        anchors.rightMargin: M.spacingM
        anchors.verticalCenter: parent.verticalCenter
        width: 52 * M.dp
        height: 260 * M.dp
        radius: width / 2
        color: M.surfaceVariant
        opacity: 0.92

        readonly property real pad: 36 * M.dp
        readonly property real trackH: height - pad * 2
        readonly property real frac: (root.povZoom - root.povZoomMin) / (root.povZoomMax - root.povZoomMin)

        Icon { anchors.horizontalCenter: parent.horizontalCenter; y: 8 * M.dp; name: "add"; size: 20 * M.dp; color: M.inkMuted }
        Icon { anchors.horizontalCenter: parent.horizontalCenter; y: parent.height - 28 * M.dp; name: "remove"; size: 20 * M.dp; color: M.inkMuted }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: povSlider.pad
            width: 4 * M.dp
            height: povSlider.trackH
            radius: 2 * M.dp
            color: M.outline
        }
        Rectangle {
            width: 28 * M.dp
            height: width
            radius: width / 2
            color: M.primary
            anchors.horizontalCenter: parent.horizontalCenter
            y: povSlider.pad + (1 - povSlider.frac) * povSlider.trackH - height / 2
        }

        MouseArea {
            anchors.fill: parent
            function setFrom(my) {
                var f = 1 - Math.max(0, Math.min(1, (my - povSlider.pad) / povSlider.trackH))
                root.povZoom = root.povZoomMin + f * (root.povZoomMax - root.povZoomMin)
            }
            onPressed: setFrom(mouse.y)
            onPositionChanged: if (pressed) setFrom(mouse.y)
        }
    }

    // "Add this place as a stop?" card, shown after tapping the map while
    // walking or cycling.
    Card {
        id: stopCard
        visible: root.stopCandidate !== null && Navigator.state === "guiding"
                 && (Navigator.mode === "pedestrian" || Navigator.mode === "bicycle")
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomBar.top
        anchors.margins: M.spacingM
        height: 88 * M.dp
        radius: M.radiusCard

        Icon { id: stopIcon; x: M.spacingM; anchors.verticalCenter: parent.verticalCenter; name: "place"; size: 26 * M.dp; color: M.bad }
        Column {
            anchors.left: stopIcon.right
            anchors.leftMargin: M.spacingS
            anchors.right: stopButtons.left
            anchors.rightMargin: M.spacingS
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2 * M.dp
            MText {
                width: parent.width
                elide: Text.ElideRight
                variant: "subtitle"
                text: root.stopCandidate ? (root.stopCandidate.title || "") : ""
            }
            MText {
                width: parent.width
                elide: Text.ElideRight
                variant: "caption"
                color: M.inkMuted
                text: root.stopCandidate && root.stopCandidate.address ? root.stopCandidate.address : qsTr("Add this place as a stop?")
            }
        }
        Row {
            id: stopButtons
            anchors.right: parent.right
            anchors.rightMargin: M.spacingS
            anchors.verticalCenter: parent.verticalCenter
            spacing: M.spacingXs
            RoundIconButton {
                iconName: "add"
                bg: M.buttonFill
                iconColor: M.buttonInk
                onClicked: root.addStopRequested()
            }
            RoundIconButton {
                iconName: "close"
                elevation: 0
                onClicked: root.stopCandidateDismissed()
            }
        }
    }

    // --- Bottom bar -----------------------------------------------------
    Card {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 96 * M.dp
        radius: M.radiusSheet
        visible: Navigator.state !== "arrived" && Navigator.state !== "error"

        Row {
            anchors.fill: parent
            anchors.leftMargin: M.spacingM
            anchors.rightMargin: M.spacingM

            Column {
                width: parent.width - 44 * M.dp - M.spacingM
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2 * M.dp
                MText {
                    variant: "title"
                    // remainingDistance isn't tracked across transit legs (only
                    // remainingSeconds, from each leg's own scheduled times is);
                    // showing a stale/leftover distance would be worse than
                    // just leaving it out for this mode.
                    text: Navigator.mode === "transit" ? Fmt.duration(Navigator.remainingSeconds)
                          : Fmt.duration(Navigator.remainingSeconds) + "  ·  " + Fmt.distance(Navigator.remainingDistance, AppSettings.units)
                }
                MText {
                    variant: "caption"
                    color: M.inkMuted
                    text: (Navigator.mode === "transit" ? qsTr("Leg %1 of %2 · ").arg(Navigator.legIndex + 1).arg(Navigator.legCount) : "")
                          + (Navigator.stopCount > 0 ? qsTr("Via %1 · ").arg(Navigator.stops[0].title || qsTr("stop")) : "")
                          + qsTr("Arrive ") + Navigator.arrivalText
                }
            }

            RoundIconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "close"
                bg: M.bad
                iconColor: "#ffffff"
                onClicked: root.closed()
            }
        }
    }

    // --- Arrival ---------------------------------------------------
    Card {
        anchors.centerIn: parent
        width: parent.width * 0.8
        height: 180 * M.dp
        visible: Navigator.state === "arrived"

        Column {
            anchors.centerIn: parent
            spacing: M.spacingM
            Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "flag"; size: 36 * M.dp; color: M.good }
            MText { anchors.horizontalCenter: parent.horizontalCenter; variant: "subtitle"; text: qsTr("You have arrived") }
            PrimaryButton { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Done"); onClicked: root.closed() }
        }
    }
}
