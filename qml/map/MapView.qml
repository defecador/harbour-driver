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

    // While navigating, real GPS fixes drive Navigator (unless a simulated
    // run is feeding it instead — see NavigationScreen.qml) and the map
    // follows the position instead of sitting still.
    property bool navigating: false
    property bool simulating: false

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
            mapLoader.item.addRoute("routeSel", it.coordinates, "" + M.primary, true, dashed)
        }
    }

    // --- Navigation follow mode ---------------------------------------
    // tilt > 0 keeps the map rotated to the heading and pitched (car);
    // tilt 0 keeps it flat and north-up (walk/bike).
    function startFollowMode(tilt, simulate) {
        root.navigating = true
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
        preferredPositioningMethods: PositionSource.SatellitePositioningMethods
        // 1s fixes while navigating (turns need to be caught promptly);
        // 4s the rest of the time, since the blue dot doesn't need to be
        // millisecond-accurate and this is easier on the battery.
        updateInterval: root.navigating ? 1000 : 4000
        onPositionChanged: {
            root.hasFix = position.latitudeValid && position.longitudeValid
            if (root.hasFix) {
                root.myLat = position.coordinate.latitude
                root.myLng = position.coordinate.longitude
            }
            if (mapLoader.item)
                mapLoader.item.setMyLocation(root.myLat, root.myLng, root.hasFix)

            // A fix with no reported accuracy, or a very poor one (common
            // for the first fix or two indoors, where the positioning
            // backend can fall back to a network/cell-based estimate),
            // must not be allowed to snap the follow camera somewhere
            // wrong — better to keep waiting than to jump to a bad guess.
            var accuracy = position.horizontalAccuracyValid ? position.horizontalAccuracy : -1
            var fixIsUsable = root.hasFix && accuracy >= 0 && accuracy <= 100
            if (root.navigating && !root.simulating && fixIsUsable) {
                var speed = position.speedValid ? position.speed : 0
                var heading = position.directionValid ? position.direction : -1
                Navigator.updatePosition(root.myLat, root.myLng, accuracy, speed, heading)
                root.updateFollowPosition(root.myLat, root.myLng, heading >= 0 ? heading : 0)
            }
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
