import QtQuick 2.6
import QtPositioning 5.3
import "../theme"
import "../components"
import "../theme/Fmt.js" as Fmt

// Public map surface used by the screens. Loads the real MapboxMap wrapper
// through a Loader so a missing mapboxgl-qml plugin degrades to a placeholder
// instead of crashing the app (see MapboxMapImpl.qml).
Item {
    id: root
    anchors.fill: parent

    property real myLat: 0
    property real myLng: 0
    property bool hasFix: false
    // Accuracy (m) of the latest fix, or -1; Navigator only gets fixes
    // better than maxFixAccuracy.
    property real fixAccuracy: -1
    readonly property real maxFixAccuracy: AppTools.android ? 150 : 100

    // While navigating, real GPS fixes drive Navigator (unless a simulated
    // run is feeding it instead — see NavigationScreen.qml) and the map
    // follows the position instead of sitting still.
    property bool navigating: false
    property bool simulating: false

    // Free drive: without a destination the map follows the position, and
    // once the car moves at driving speed it tilts into the same heading-up
    // view navigation uses (without a route).
    property bool followMe: false
    property bool freeDriving: false
    // False while another screen is open: follow-me never starts over those.
    property bool freeDriveAllowed: true
    // Latest speed (m/s) and last known heading (degrees), -1 when unknown.
    property real mySpeed: -1
    property real myHeading: -1
    property bool _autoFollowDone: false
    property int _fastFixes: 0
    property real _slowSinceMs: 0
    property real _lastFixMs: 0
    property real _lastFixLat: 0
    property real _lastFixLng: 0
    property real _headRefLat: 0
    property real _headRefLng: 0
    property bool _haveHeadRef: false

    // Smooth movement: GPS reports about once a second, so the marker and
    // camera are moved many times a second towards where the car should be
    // by now (see _renderTick).
    property bool _haveTarget: false
    property real _tgtLat: 0
    property real _tgtLng: 0
    property real _tgtHeading: 0
    property real _tgtSpeed: 0
    property real _tgtMs: 0
    property real _showLat: 0
    property real _showLng: 0
    property real _showHeading: 0
    property real _lastTickMs: 0

    signal mapClicked(var coordinate)
    signal mapLongPressed(var coordinate)
    signal followInterrupted()

    function debugSetStyle(json) {
        if (mapLoader.item) mapLoader.item.setStyleJson(json)
    }

    // True while the pitched driving view is on: the map then uses the
    // 3D-buildings vector style (see AppTools.request3dStyle) instead of the
    // flat HERE/OpenFreeMap style.
    property bool view3d: false

    function applyStyle() {
        if (!mapLoader.item) return
        if (root.view3d) {
            AppTools.request3dStyle(AppSettings.traffic, M.dark)
            return
        }
        applyFlatStyle()
    }

    function applyFlatStyle() {
        if (!mapLoader.item) return
        if (AppSettings.hasHereKey) {
            var json = AppTools.hereStyleJson(AppSettings.mapStyle, AppSettings.traffic, M.dark)
            if (json.length > 0) {
                mapLoader.item.setStyleJson(json)
                return
            }
        }
        mapLoader.item.setStyleUrl("https://tiles.openfreemap.org/styles/liberty")
    }

    function setMargins(left, top, right, bottom) {
        if (mapLoader.item) mapLoader.item.panelMargins(left, top, right, bottom)
    }

    function fitCoordinates(coords, preserve) {
        if (mapLoader.item) mapLoader.item.fitCoordinates(coords, preserve)
    }

    function fitInRect(coords, x, y, w, h) {
        if (mapLoader.item) mapLoader.item.fitInRect(coords, x, y, w, h)
    }

    function centerOnMe() {
        if (mapLoader.item && root.hasFix) {
            mapLoader.item.setCenter(root.myLat, root.myLng)
            mapLoader.item.setZoom(15)
        }
    }

    // Locate-me: centre on the position and keep following it.
    function locateMe() {
        root.centerOnMe()
        root.setFollowMe(true, false)
    }

    // keep3d: a navigation that starts right after wants the 3D style as well,
    // so leaving the driving view must not flatten the map in between.
    function setFollowMe(on, keep3d) {
        if (on === root.followMe) return
        root.followMe = on
        if (root.navigating) return
        root._haveTarget = false
        if (on) {
            root._fastFixes = 0
            root._slowSinceMs = 0
            if (mapLoader.item) {
                mapLoader.item.setFollow(true, 0)
                mapLoader.item.setArrow(false)
            }
        } else {
            root._leaveFreeDrive(keep3d === true)
            if (mapLoader.item) mapLoader.item.setFollow(false, 0)
        }
    }

    function _enterFreeDrive() {
        if (root.freeDriving || root.navigating) return
        root.freeDriving = true
        var tilt = AppSettings.view3d ? 60 : 0
        root.view3d = tilt > 0
        if (root.view3d) root.applyStyle()
        if (mapLoader.item) mapLoader.item.setFollow(true, tilt)
    }

    function _leaveFreeDrive(keep3d) {
        if (!root.freeDriving) return
        root.freeDriving = false
        root._fastFixes = 0
        root._slowSinceMs = 0
        if (root.navigating) return
        if (root.view3d && keep3d !== true) {
            root.view3d = false
            root.applyStyle()
        }
        if (mapLoader.item) {
            mapLoader.item.setFollow(root.followMe, 0)
            mapLoader.item.setArrow(false)
        }
    }

    // New position to glide to. speed < 0: worked out from the previous one.
    function setFollowTarget(lat, lng, heading, speed) {
        var now = new Date().getTime()
        var here = QtPositioning.coordinate(lat, lng)
        var moved = root._haveTarget ? QtPositioning.coordinate(root._tgtLat, root._tgtLng).distanceTo(here) : 0
        if (speed < 0) {
            var dt = (now - root._tgtMs) / 1000
            speed = root._haveTarget && dt > 0.05 ? moved / dt : 0
        }
        // A real jump (reroute, simulation start) is not something to glide over.
        var jump = !root._haveTarget || moved > 100
        root._tgtLat = lat
        root._tgtLng = lng
        root._tgtHeading = heading
        root._tgtSpeed = speed >= 1 ? speed : 0
        root._tgtMs = now
        if (jump) {
            root._showLat = lat
            root._showLng = lng
            root._showHeading = heading
            root._lastTickMs = now
            root.updateFollowPosition(lat, lng, heading)
        }
        root._haveTarget = true
    }

    function _renderTick() {
        var now = new Date().getTime()
        var dt = Math.min(0.2, (now - root._lastTickMs) / 1000)
        root._lastTickMs = now
        // Where the car should be by now, for at most 1.5s if fixes stop coming.
        var dist = root._tgtSpeed * Math.min(1.5, (now - root._tgtMs) / 1000)
        var rad = root._tgtHeading * Math.PI / 180
        var cosLat = Math.max(0.2, Math.cos(root._tgtLat * Math.PI / 180))
        var lat = root._tgtLat + Math.cos(rad) * dist / 111320
        var lng = root._tgtLng + Math.sin(rad) * dist / (111320 * cosLat)
        var a = 1 - Math.exp(-dt / 0.12)
        var dLat = (lat - root._showLat) * a
        var dLng = (lng - root._showLng) * a
        var turn = ((root._tgtHeading - root._showHeading + 540) % 360) - 180
        var dHead = turn * (1 - Math.exp(-dt / 0.3))
        // Standing still: nothing to redraw.
        if (Math.abs(dLat) < 2e-8 && Math.abs(dLng) < 2e-8 && Math.abs(dHead) < 0.02)
            return
        root._showLat += dLat
        root._showLng += dLng
        root._showHeading = (root._showHeading + dHead + 360) % 360
        root.updateFollowPosition(root._showLat, root._showLng, root._showHeading)
    }

    Timer {
        id: renderTimer
        interval: 33
        repeat: true
        running: (root.navigating || root.followMe) && root._haveTarget
        onRunningChanged: if (running) root._lastTickMs = new Date().getTime()
        onTriggered: root._renderTick()
    }

    // One usable GPS fix (or a simulated one). speed/heading are -1 when the
    // receiver didn't report them.
    function _handleFix(lat, lng, accuracy, speed, heading, nowMs) {
        var here = QtPositioning.coordinate(lat, lng)
        var navSpeed = speed >= 0 ? speed : 0
        var navHeading = heading
        if (speed < 0 && root._lastFixMs > 0) {
            var dt = (nowMs - root._lastFixMs) / 1000
            if (dt > 0.05)
                speed = QtPositioning.coordinate(root._lastFixLat, root._lastFixLng).distanceTo(here) / dt
        }
        root._lastFixMs = nowMs
        root._lastFixLat = lat
        root._lastFixLng = lng
        root.mySpeed = speed >= 0 ? speed : 0
        // Receivers report no heading when standing still; keep the last good one.
        if (heading < 0 || root.mySpeed < 1) {
            heading = -1
            if (!root._haveHeadRef) {
                root._headRefLat = lat
                root._headRefLng = lng
                root._haveHeadRef = true
            } else if (QtPositioning.coordinate(root._headRefLat, root._headRefLng).distanceTo(here) >= 8) {
                heading = QtPositioning.coordinate(root._headRefLat, root._headRefLng).azimuthTo(here)
                root._headRefLat = lat
                root._headRefLng = lng
            }
        } else {
            root._headRefLat = lat
            root._headRefLng = lng
            root._haveHeadRef = true
        }
        if (heading >= 0) root.myHeading = heading

        if (root.navigating) {
            if (!root.simulating) {
                Navigator.updatePosition(lat, lng, accuracy, navSpeed, navHeading)
                // On the route the car is drawn on the road, not where GPS
                // wandered to; off it, where it really is.
                if (Navigator.snapped)
                    root.setFollowTarget(Navigator.snappedLat, Navigator.snappedLng,
                                         Navigator.snappedHeading, root.mySpeed)
                else
                    root.setFollowTarget(lat, lng, root.myHeading >= 0 ? root.myHeading : 0, root.mySpeed)
            }
        } else if (root.followMe) {
            if (root.mySpeed >= 4.2) {
                root._fastFixes++
                root._slowSinceMs = 0
            } else {
                root._fastFixes = 0
                if (root._slowSinceMs === 0) root._slowSinceMs = nowMs
            }
            if (!root.freeDriving && root._fastFixes >= 3) {
                root._enterFreeDrive()
            } else if (root.freeDriving && root._slowSinceMs > 0 && nowMs - root._slowSinceMs > 120000) {
                // Parked, not just waiting at a red light.
                root._leaveFreeDrive(false)
                if (mapLoader.item) mapLoader.item.setZoom(16)
            }
            root.setFollowTarget(lat, lng, root.myHeading >= 0 ? root.myHeading : 0, root.mySpeed)
        }
        RoadAlerts.updatePosition(lat, lng, root.mySpeed, heading, nowMs)
    }

    // Dev screens: pretend the GPS reported this fix.
    function simulateFix(lat, lng, speed, heading) {
        root.myLat = lat
        root.myLng = lng
        root.hasFix = true
        if (mapLoader.item && !renderTimer.running) mapLoader.item.setMyLocation(lat, lng, true)
        root._handleFix(lat, lng, 5, speed, heading, new Date().getTime())
    }

    onFollowInterrupted: {
        // The user took the map: stop following. Flattening swaps the style on
        // Android, which must not happen under their finger, hence the delay.
        if (root.followMe && !root.navigating) {
            root.followMe = false
            flattenTimer.restart()
        }
    }

    Timer {
        id: flattenTimer
        interval: 700
        onTriggered: {
            if (root.followMe || root.navigating) return
            root._leaveFreeDrive(false)
            if (mapLoader.item) mapLoader.item.setFollow(false, 0)
        }
    }

    function clearRoutes() {
        if (mapLoader.item) mapLoader.item.clearRoutes()
    }

    // itineraries: array of {mode, coordinates}; selectedIndex highlighted, rest grey.
    function showRoutes(itineraries, selectedIndex) {
        clearRoutes()
        if (!mapLoader.item || !itineraries) return
        for (var i = itineraries.length - 1; i >= 0; i--) {
            if (i === selectedIndex) continue
            mapLoader.item.addRoute("route" + i, itineraries[i].coordinates, "" + M.routeAlt, false, false)
        }
        if (selectedIndex >= 0 && selectedIndex < itineraries.length) {
            var it = itineraries[selectedIndex]
            var dashed = it.mode === "pedestrian"
            var segs = []
            var tr = it.traffic || []
            for (var k = 0; k < tr.length; k++) {
                if (tr[k].end > tr[k].start)
                    segs.push({"coordinates": it.coordinates.slice(tr[k].start, tr[k].end + 1),
                               "color": tr[k].level >= 2 ? "#e5251b" : "#ff9500"})
            }
            mapLoader.item.addRoute("routeSel", it.coordinates, "" + M.routeLine, true, dashed, segs)
        }
    }

    // --- Navigation follow mode ---------------------------------------
    // tilt > 0 keeps the map rotated to the heading and pitched (car);
    // tilt 0 keeps it flat and north-up (walk/bike).
    function startFollowMode(tilt, simulate) {
        root.navigating = true
        root._haveTarget = false
        root.view3d = tilt > 0
        if (root.view3d) root.applyStyle()
        // While simulating, the real PositionSource's fixes (if any) must
        // not also feed Navigator alongside the simulated ones.
        root.simulating = simulate === true
        if (mapLoader.item) mapLoader.item.setFollow(true, tilt)
        // setMargins() is deliberately NOT called here any more: on a real
        // device it was pulling the camera away to a completely different,
        // far-off view the instant navigation started — before any GPS fix
        // could even be involved, so framing math against the plugin's own
        // fit-bounds state is the likely cause. Needs verifying against the
        // actual plugin behaviour (not just its exposed API) before trying
        // again; until then the banner/bottom bar simply overlap the map.
    }

    function setPovZoom(z) {
        if (mapLoader.item) mapLoader.item.setPovZoom(z)
    }

    function stopFollowMode() {
        root.navigating = false
        root.simulating = false
        root._haveTarget = false
        if (mapLoader.item) mapLoader.item.setFollow(false, 0)
        if (root.view3d) {
            root.view3d = false
            root.applyStyle()
        }
        clearTraveledOverlay()
    }

    // Toggles just the camera-follow behaviour without touching margins or
    // the traveled overlay — used for the "Overview"/"Follow" button and to
    // resume following after a pan (MapView.followInterrupted) interrupted it.
    function setFollowActive(active, tilt) {
        if (mapLoader.item) mapLoader.item.setFollow(active, tilt)
    }

    function updateFollowPosition(lat, lng, headingDeg) {
        if (mapLoader.item) mapLoader.item.followTo(lat, lng, headingDeg)
    }

    function setTraveledCoordinates(coords) {
        if (mapLoader.item) mapLoader.item.setTraveledOverlay(coords, "" + M.routeAlt)
    }

    function clearTraveledOverlay() {
        if (mapLoader.item) mapLoader.item.clearTraveledOverlay()
    }

    function clearPins() {
        if (mapLoader.item) mapLoader.item.clearPins()
    }

    function showPin(id, place, kind) {
        if (mapLoader.item && place)
            mapLoader.item.addPin(id, place.navLat !== undefined ? place.navLat : place.lat,
                                   place.navLng !== undefined ? place.navLng : place.lng, kind)
    }

    function removePin(id) {
        if (mapLoader.item) mapLoader.item.removePin(id)
    }

    PositionSource {
        id: posSource
        active: true
        // Network/WiFi-based positioning can self-report a perfectly
        // reasonable accuracy while being wrong by kilometres (seen first-
        // hand: right after a reboot, before GPS has a satellite lock, a
        // fix like this snapped the follow camera to a completely wrong
        // place). GPS-only is slower to get a first fix but trustworthy,
        // which matters far more for turn-by-turn than for the blue dot.
        // On Android the satellite-only mode never produces a fix on a tablet
        // without a GPS chip (or indoors), so the fused/network position
        // is used there too.
        preferredPositioningMethods: AppTools.android ? PositionSource.AllPositioningMethods
                                                      : PositionSource.SatellitePositioningMethods
        // 1s fixes while navigating (turns need to be caught promptly);
        // 4s the rest of the time, since the blue dot doesn't need to be
        // millisecond-accurate and this is easier on the battery.
        updateInterval: root.navigating || root.freeDriving ? 1000 : (root.followMe ? 2000 : 4000)
        onPositionChanged: {
            root.hasFix = position.latitudeValid && position.longitudeValid
            if (root.hasFix) {
                root.myLat = position.coordinate.latitude
                root.myLng = position.coordinate.longitude
            }
            // While gliding, the marker is placed by _renderTick (on the road).
            if (mapLoader.item && (!root.hasFix || !renderTimer.running))
                mapLoader.item.setMyLocation(root.myLat, root.myLng, root.hasFix)

            // A fix with no reported accuracy, or a very poor one (common
            // for the first fix or two indoors, where the positioning
            // backend can fall back to a network/cell-based estimate),
            // must not be allowed to snap the follow camera somewhere
            // wrong — better to keep waiting than to jump to a bad guess.
            var accuracy = position.horizontalAccuracyValid ? position.horizontalAccuracy : -1
            root.fixAccuracy = root.hasFix ? accuracy : -1
            var fixIsUsable = root.hasFix && accuracy >= 0 && accuracy <= root.maxFixAccuracy
            if (!fixIsUsable || root.simulating)
                return
            // First good fix after launch: show where we are and keep following.
            if (!root._autoFollowDone) {
                root._autoFollowDone = true
                if (root.freeDriveAllowed && !root.navigating && AppTools.devScreen === "") {
                    root.centerOnMe()
                    root.setFollowMe(true, false)
                }
            }
            root._handleFix(root.myLat, root.myLng, accuracy,
                            position.speedValid ? position.speed : -1,
                            position.directionValid ? position.direction : -1,
                            new Date().getTime())
        }
    }

    Connections {
        target: AppSettings
        onMapStyleChanged: root.applyStyle()
        onTrafficChanged: root.applyStyle()
        onHereApiKeyChanged: root.applyStyle()
    }

    Connections {
        target: M
        onDarkChanged: root.applyStyle()
    }

    Connections {
        target: AppTools
        onStyle3dReady: {
            if (!root.view3d)
                return
            if (json.length > 0 && mapLoader.item)
                mapLoader.item.setStyleJson(json)
            else
                root.applyFlatStyle()
        }
    }

    Loader {
        id: mapLoader
        anchors.fill: parent
        source: "MapboxMapImpl.qml"
        asynchronous: false
        onStatusChanged: {
            if (status === Loader.Ready) root.applyStyle()
        }
        Connections {
            target: mapLoader.item
            ignoreUnknownSignals: true
            onMapClicked: root.mapClicked(coordinate)
            onMapLongPressed: root.mapLongPressed(coordinate)
            onFollowInterrupted: root.followInterrupted()
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: mapLoader.status === Loader.Error
        color: M.bg

        Column {
            anchors.centerIn: parent
            width: parent.width * 0.72
            spacing: M.spacingM
            Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "map"; size: 40 * M.dp; color: M.inkMuted }
            MText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                variant: "subtitle"
                color: M.inkMuted
                text: qsTr("Map unavailable — install mapboxgl-qml from Chum")
            }
        }
    }
}
