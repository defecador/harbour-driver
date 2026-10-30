import QtQuick 2.6
import QtPositioning 5.3
import MapboxMap 1.0
import "../theme"

// Thin wrapper around the mapboxgl-qml plugin, isolated in its own file so a
// missing plugin only fails this Loader target rather than the whole app
// (see MapView.qml for the fallback).
Item {
    id: root
    anchors.fill: parent

    property real zoomLevel: 13
    property bool ready: false

    signal mapClicked(var coordinate)
    signal mapLongPressed(var coordinate)
    signal followInterrupted()

    function setStyleJson(json) { if (json && json.length > 0) map.styleJson = json }
    function setStyleUrl(url) { map.styleUrl = url }
    function setCenter(lat, lng) { map.center = QtPositioning.coordinate(lat, lng) }
    function setZoom(z) { map.zoomLevel = z }
    function panelMargins(left, top, right, bottom) { map.setMargins(left, top, right, bottom) }
    function fitCoordinates(coords, preserve) { if (coords && coords.length > 0) map.fitView(coords, preserve === true) }
    function stopFit() { map.stopFitView() }

    // Frames the coordinates inside a rectangle of the window (rx, ry, rw, rh
    // in this item's pixels) — used by the route-preview window, which sits
    // over only part of the screen. Plain web-mercator math instead of
    // setMargins(), which misbehaved on the device.
    function fitInRect(coords, rx, ry, rw, rh) {
        if (!coords || coords.length === 0 || rw < 20 || rh < 20)
            return
        var minLat = 90, maxLat = -90, minLng = 180, maxLng = -180
        for (var i = 0; i < coords.length; i++) {
            var la = coords[i].latitude, lo = coords[i].longitude
            if (la < minLat) minLat = la
            if (la > maxLat) maxLat = la
            if (lo < minLng) minLng = lo
            if (lo > maxLng) maxLng = lo
        }
        function wx(lng) { return (lng + 180) / 360 }
        function wy(lat) { var s = Math.sin(lat * Math.PI / 180); return 0.5 - Math.log((1 + s) / (1 - s)) / (4 * Math.PI) }
        var x0 = wx(minLng), x1 = wx(maxLng), y0 = wy(maxLat), y1 = wy(minLat)
        var dx = Math.max(x1 - x0, 1e-7), dy = Math.max(y1 - y0, 1e-7)
        var pad = 0.12 * Math.min(rw, rh)
        var tile = 512 * root._mapScale
        var z = Math.log(Math.min((rw - 2 * pad) / (dx * tile), (rh - 2 * pad) / (dy * tile))) / Math.LN2
        z = Math.max(3, Math.min(18, z))
        var scale = tile * Math.pow(2, z)
        var cx = (x0 + x1) / 2, cy = (y0 + y1) / 2
        var shiftX = (rx + rw / 2) - map.width / 2
        var shiftY = (ry + rh / 2) - map.height / 2
        var mx = cx - shiftX / scale, my = cy - shiftY / scale
        var lng = mx * 360 - 180
        var lat = (2 * Math.atan(Math.exp(Math.PI * (1 - 2 * my))) - Math.PI / 2) * 180 / Math.PI
        map.stopFitView()
        map.bearing = 0
        map.pitch = 0
        map.zoomLevel = z
        map.center = QtPositioning.coordinate(lat, lng)
    }
    // Item pixels per mapbox "512 tile" pixel; calibrated on the device.
    property real _mapScale: 1.0

    // --- Navigation follow mode ---------------------------------------
    // Car tilts and rotates to heading; walk/bike stay flat and north-up.
    property bool _following: false
    property real _followTilt: 0
    // Zoom is applied on the first position update, not in setFollow(): the
    // route-fit animation started just before navigation would override it.
    property bool _needFollowZoom: false
    // How close the pitched driving view sits; 18.3 is the default street-level look.
    property real _povZoom: 18.3

    function setPovZoom(z) {
        root._povZoom = z
        if (root._following && root._followTilt > 0 && !root._needFollowZoom)
            map.zoomLevel = z
    }

    function setFollow(active, tilt) {
        root._following = active
        root._followTilt = tilt
        map.pitch = active ? tilt : 0
        if (!active || tilt <= 0)
            map.bearing = 0
        root._needFollowZoom = active && tilt > 0
        meDot.navigating = active
    }

    // Heading arrow (navigation) or plain dot (follow-me).
    function setArrow(on) { meDot.navigating = on }

    function followTo(lat, lng, headingDeg) {
        // The map itself is rotated to the heading in the pitched view, so the
        // marker must point straight up there.
        meDot.heading = root._followTilt > 0 && root._following ? 0 : headingDeg
        // Simulated runs have no real fix feeding setMyLocation(), so keep
        // the marker on the position being followed.
        map.trackLocation("me", QtPositioning.coordinate(lat, lng))
        root._tracking = true
        if (!root._following)
            return
        if (root._followTilt > 0) {
            // The pitched view owns the zoom (panning or pinching ends follow
            // mode), so re-assert it: the route-fit animation or a style swap
            // can otherwise leave the camera at the wrong zoom.
            if (root._needFollowZoom) {
                map.stopFitView()
                root._needFollowZoom = false
            }
            if (Math.abs(map.zoomLevel - root._povZoom) > 0.01)
                map.zoomLevel = root._povZoom
        }
        if (root._followTilt > 0) {
            // Aim the camera ahead of the car so the car sits low on the
            // screen and most of the view shows the road to come.
            var d = 130 * Math.pow(2, 17 - map.zoomLevel)  // metres ahead; shrinks as zoom grows
            var rad = headingDeg * Math.PI / 180
            var dLat = Math.cos(rad) * d / 111320
            var dLng = Math.sin(rad) * d / (111320 * Math.max(0.2, Math.cos(lat * Math.PI / 180)))
            map.center = QtPositioning.coordinate(lat + dLat, lng + dLng)
            map.bearing = headingDeg
        } else {
            map.center = QtPositioning.coordinate(lat, lng)
        }
    }

    Connections {
        target: map
        onGestureInProgressChanged: {
            if (map.gestureInProgress && root._following) {
                root._following = false
                root.followInterrupted()
            }
        }
    }

    // --- Routes ----------------------------------------------------
    property var _routeIds: []
    property bool _traveledAdded: false

    property var _trafficIds: []

    function clearRoutes() {
        for (var t = 0; t < _trafficIds.length; t++) {
            map.removeLayer(_trafficIds[t])
            map.removeSource(_trafficIds[t])
        }
        _trafficIds = []
        for (var i = 0; i < _routeIds.length; i++) {
            var sid = _routeIds[i]
            map.removeLayer(sid + "-outline")
            map.removeLayer(sid)
            map.removeSource(sid)
        }
        _routeIds = []
        clearTraveledOverlay()
    }

    // Greys out the part of the selected route already driven/walked, drawn
    // on top of it. Called by MapView as navigation progress advances.
    function setTraveledOverlay(coordinates, color) {
        if (!coordinates || coordinates.length < 2) {
            clearTraveledOverlay()
            return
        }
        if (!root._traveledAdded) {
            map.addSourceLine("navTraveled", coordinates)
            map.addLayer("navTraveled", {
                "type": "line",
                "source": "navTraveled",
                "layout": {"line-cap": "round", "line-join": "round"},
                "paint": {"line-color": color, "line-width": 7.5}
            })
            root._traveledAdded = true
        } else {
            map.updateSourceLine("navTraveled", coordinates)
        }
    }

    function clearTraveledOverlay() {
        if (root._traveledAdded) {
            map.removeLayer("navTraveled")
            map.removeSource("navTraveled")
            root._traveledAdded = false
        }
    }

    // trafficSegments: [{coordinates, color}] drawn over the route line (the
    // slow stretches in orange/red).
    function addRoute(sourceId, coordinates, color, selected, dashed, trafficSegments) {
        map.addSourceLine(sourceId, coordinates)
        map.addLayer(sourceId + "-outline", {
            "type": "line",
            "source": sourceId,
            "layout": {"line-cap": "round", "line-join": "round"},
            "paint": {"line-color": "#ffffff", "line-width": selected ? 10.5 : 7.0,
                      "line-opacity": selected ? 1.0 : 0.85}
        })
        var paint = {"line-color": color, "line-width": selected ? 7.5 : 5.0}
        if (dashed) paint["line-dasharray"] = [2, 2]
        map.addLayer(sourceId, {
            "type": "line",
            "source": sourceId,
            "layout": {"line-cap": "round", "line-join": "round"},
            "paint": paint
        })
        _routeIds.push(sourceId)
        var segs = trafficSegments || []
        for (var k = 0; k < segs.length; k++) {
            var tid = sourceId + "-t" + k
            map.addSourceLine(tid, segs[k].coordinates)
            map.addLayer(tid, {
                "type": "line",
                "source": tid,
                "layout": {"line-cap": "round", "line-join": "round"},
                "paint": {"line-color": segs[k].color, "line-width": selected ? 7.5 : 5.0}
            })
            _trafficIds.push(tid)
        }
    }

    // --- Pins --------------------------------------------------------
    property var _pinItems: ({})

    function clearPins() {
        for (var id in _pinItems) removePin(id)
    }

    function addPin(id, lat, lng, kind) {
        removePin(id)
        map.trackLocation("pin-" + id, QtPositioning.coordinate(lat, lng))
        _pinItems[id] = pinComponent.createObject(root, {"kind": kind})
    }

    function removePin(id) {
        var item = _pinItems[id]
        if (item) {
            map.removeLocationTracking("pin-" + id)
            item.destroy()
            delete _pinItems[id]
        }
    }

    Component {
        id: pinComponent
        Pin {}
    }

    // --- My location -------------------------------------------------
    property bool _tracking: false

    function setMyLocation(lat, lng, hasFix) {
        if (!hasFix) {
            if (_tracking) { map.removeLocationTracking("me"); _tracking = false }
            meDot.visible = false
            return
        }
        map.trackLocation("me", QtPositioning.coordinate(lat, lng))
        _tracking = true
    }

    LocationDot { id: meDot; z: 10 }

    Connections {
        target: map
        onLocationChanged: {
            if (id === "me") {
                meDot.visible = visible
                meDot.x = pixel.x - meDot.width / 2
                meDot.y = pixel.y - meDot.height / 2
            } else if (id.indexOf("pin-") === 0) {
                var pinId = id.substring(4)
                var item = root._pinItems[pinId]
                if (item) {
                    item.visible = visible
                    item.x = pixel.x - item.width / 2
                    item.y = pixel.y - item.height
                }
            }
        }
    }

    MapboxMap {
        id: map
        anchors.fill: parent
        center: QtPositioning.coordinate(52.52, 13.405)
        zoomLevel: root.zoomLevel
        minimumZoomLevel: 2
        maximumZoomLevel: 21.5
        onZoomLevelChanged: root.zoomLevel = zoomLevel
        Component.onCompleted: root.ready = true

        MapboxMapGestureArea {
            id: gestures
            map: map
            activeClickedGeo: true
            activePressAndHoldGeo: true
            onClickedGeo: root.mapClicked(geocoordinate)
            onPressAndHoldGeo: root.mapLongPressed(geocoordinate)
        }
    }
}
