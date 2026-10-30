import QtQuick
import QtLocation
import QtPositioning
import MapLibre
import "../theme"

// MapLibre Native (Qt Location plugin) implementation of the map used on
// Android and the desktop Qt 6 build. It keeps exactly the interface that
// the Sailfish mapboxgl-qml wrapper (qml/map/MapboxMapImpl.qml) has, so
// MapView.qml is shared. Isolated in its own file, loaded through a Loader,
// so a missing plugin only fails the map and not the whole app.
Item {
    id: root
    anchors.fill: parent

    property real zoomLevel: 13
    property bool ready: false

    signal mapClicked(var coordinate)
    signal mapLongPressed(var coordinate)
    signal followInterrupted()

    // The loaded Map, or null while a style swap is rebuilding it.
    readonly property var map: mapLoader.item
    property string _styleUrl: ""
    // The plugin takes its style list when the Map is created, so a new style
    // means a new Map: the camera is carried over in these.
    property real _lat: 52.52
    property real _lng: 13.405
    property real _bearing: 0
    property real _tilt: 0
    property real _mLeft: 0
    property real _mTop: 0
    property real _mRight: 0
    property real _mBottom: 0

    // The plugin reads its style only when a Map is created, so a different
    // style needs a new Map: the Loader is switched off for a moment, and the
    // routes drawn so far are put back once the new one is up (_replay).
    property bool _rebuilding: false
    function _applyStyleUrl(url) {
        if (url === root._styleUrl)
            return
        var hadMap = mapLoader.item !== null
        root._styleUrl = url
        if (hadMap) {
            root._rebuilding = true
            rebuildTimer.restart()
        }
    }
    Timer { id: rebuildTimer; interval: 50; onTriggered: root._rebuilding = false }

    function setStyleJson(json) {
        if (json && json.length > 0)
            root._applyStyleUrl(AppTools.writeStyleFile(json))
    }
    function setStyleUrl(url) { root._applyStyleUrl(url) }

    function setCenter(lat, lng) {
        root._lat = lat
        root._lng = lng
        if (map) map.center = QtPositioning.coordinate(lat, lng)
    }
    function setZoom(z) {
        root.zoomLevel = z
        if (map) map.zoomLevel = z
    }
    function panelMargins(left, top, right, bottom) {
        root._mLeft = left; root._mTop = top; root._mRight = right; root._mBottom = bottom
    }
    function fitCoordinates(coords, preserve) {
        if (!coords || coords.length === 0 || !map)
            return
        if (coords.length === 1) {
            setCenter(coords[0].latitude, coords[0].longitude)
            if (!preserve) setZoom(Math.max(map.zoomLevel, 15))
            return
        }
        fitInRect(coords, root._mLeft, root._mTop,
                  root.width - root._mLeft - root._mRight, root.height - root._mTop - root._mBottom)
    }
    function stopFit() {}

    // Frames the coordinates inside a rectangle of the window (rx, ry, rw, rh
    // in this item's pixels) — used by the route-preview window, which sits
    // over only part of the screen. Plain web-mercator math.
    function fitInRect(coords, rx, ry, rw, rh) {
        if (!coords || coords.length === 0 || rw < 20 || rh < 20 || !map)
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
        map.bearing = 0
        map.tilt = 0
        map.zoomLevel = z
        map.center = QtPositioning.coordinate(lat, lng)
    }
    // Item pixels per "512 tile" pixel.
    property real _mapScale: 1.0

    // --- Navigation follow mode ---------------------------------------
    // Car tilts and rotates to heading; walk/bike stay flat and north-up.
    property bool _following: false
    property real _followTilt: 0
    // Zoom is applied on the first position update, not in setFollow().
    property bool _needFollowZoom: false
    // How close the pitched driving view sits; 18.3 is the default street-level look.
    property real _povZoom: 18.3

    function setPovZoom(z) {
        root._povZoom = z
        if (map && root._following && root._followTilt > 0 && !root._needFollowZoom)
            map.zoomLevel = z
    }

    function setFollow(active, tilt) {
        root._following = active
        root._followTilt = tilt
        root._tilt = active ? tilt : 0
        if (map) map.tilt = root._tilt
        if (!active || tilt <= 0) {
            root._bearing = 0
            if (map) map.bearing = 0
        }
        root._needFollowZoom = active && tilt > 0
        meDot.navigating = active
    }

    // Heading arrow (navigation) or plain dot (follow-me).
    function setArrow(on) { meDot.navigating = on }

    function followTo(lat, lng, headingDeg) {
        meDot.heading = root._followTilt > 0 && root._following ? 0 : headingDeg
        root._me = QtPositioning.coordinate(lat, lng)
        root._tracking = true
        root._reposition()
        if (!root._following || !map)
            return
        if (root._followTilt > 0) {
            // A freshly built Map ignores the tilt until it is ready.
            if (Math.abs(map.tilt - root._followTilt) > 0.5)
                map.tilt = root._followTilt
            if (root._needFollowZoom)
                root._needFollowZoom = false
            if (Math.abs(map.zoomLevel - root._povZoom) > 0.01)
                map.zoomLevel = root._povZoom
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

    // --- Routes ----------------------------------------------------
    // Each route is a GeoJSON source plus line layers, added to the map's
    // MapLibre style as parameter objects.
    property var _routeIds: []
    property var _params: ({})   // style id -> parameter object
    property bool _traveledAdded: false
    property var _routeDefs: []     // what addRoute() was given, to redraw after a style swap
    property var _traveledDef: null
    property bool _replaying: false

    function _replay() {
        for (var id in root._params) {
            try { root._params[id].destroy() } catch (e) {}   // already gone with the old Map
        }
        root._params = {}
        root._routeIds = []
        root._trafficIds = []
        root._traveledAdded = false
        root._replaying = true
        var defs = root._routeDefs
        for (var i = 0; i < defs.length; i++)
            addRoute(defs[i].sourceId, defs[i].coordinates, defs[i].color, defs[i].selected, defs[i].dashed, defs[i].traffic)
        if (root._traveledDef)
            setTraveledOverlay(root._traveledDef.coordinates, root._traveledDef.color)
        root._replaying = false
    }

    function _geo(coords) {
        var c = []
        for (var i = 0; i < coords.length; i++)
            c.push([coords[i].longitude, coords[i].latitude])
        return {"type": "Feature", "properties": {}, "geometry": {"type": "LineString", "coordinates": c}}
    }

    function _addSource(id, coords) {
        if (!map) return
        var p = Qt.createQmlObject('import MapLibre; SourceParameter { type: "geojson"; property var data: ({}) }', root)
        p.styleId = id
        p.data = _geo(coords)
        root._params[id] = p
        map.styleObj.addParameter(p)
    }

    function _addLine(id, source, paint) {
        if (!map) return
        var p = Qt.createQmlObject('import MapLibre; LayerParameter { type: "line"; property string source: "" }', root)
        p.styleId = id
        p.source = source
        p.layout = {"line-cap": "round", "line-join": "round"}
        p.paint = paint
        root._params[id] = p
        map.styleObj.addParameter(p)
    }

    function _remove(id) {
        var p = root._params[id]
        if (!p) return
        if (map) map.styleObj.removeParameter(p)
        p.destroy()
        delete root._params[id]
    }

    property var _trafficIds: []

    function clearRoutes() {
        for (var t = 0; t < _trafficIds.length; t++) {
            _remove(_trafficIds[t])
            _remove(_trafficIds[t] + "-src")
        }
        _trafficIds = []
        for (var i = 0; i < _routeIds.length; i++) {
            var sid = _routeIds[i]
            _remove(sid + "-outline")
            _remove(sid)
            _remove(sid + "-src")
        }
        _routeIds = []
        _routeDefs = []
        clearTraveledOverlay()
    }

    // Greys out the part of the selected route already driven/walked, drawn
    // on top of it. Called by MapView as navigation progress advances.
    function setTraveledOverlay(coordinates, color) {
        if (!coordinates || coordinates.length < 2) {
            clearTraveledOverlay()
            return
        }
        if (!root._replaying)
            root._traveledDef = {"coordinates": coordinates, "color": color}
        if (root._traveledAdded) {
            // Cheapest correct update: swap the data on the existing source.
            var src = root._params["navTraveled-src"]
            if (src) src.data = _geo(coordinates)
            return
        }
        _addSource("navTraveled-src", coordinates)
        _addLine("navTraveled", "navTraveled-src", {"line-color": color, "line-width": 7.5})
        root._traveledAdded = true
    }

    function clearTraveledOverlay() {
        root._traveledDef = null
        if (root._traveledAdded) {
            _remove("navTraveled")
            _remove("navTraveled-src")
            root._traveledAdded = false
        }
    }

    // trafficSegments: [{coordinates, color}] drawn over the route line (the
    // slow stretches in orange/red).
    function addRoute(sourceId, coordinates, color, selected, dashed, trafficSegments) {
        if (!root._replaying)
            root._routeDefs.push({"sourceId": sourceId, "coordinates": coordinates, "color": color,
                                  "selected": selected, "dashed": dashed, "traffic": trafficSegments})
        _addSource(sourceId + "-src", coordinates)
        _addLine(sourceId + "-outline", sourceId + "-src",
                 {"line-color": "#ffffff", "line-width": selected ? 10.5 : 7.0,
                  "line-opacity": selected ? 1.0 : 0.85})
        var paint = {"line-color": color, "line-width": selected ? 7.5 : 5.0}
        if (dashed) paint["line-dasharray"] = [2, 2]
        _addLine(sourceId, sourceId + "-src", paint)
        _routeIds.push(sourceId)
        var segs = trafficSegments || []
        for (var k = 0; k < segs.length; k++) {
            var tid = sourceId + "-t" + k
            _addSource(tid + "-src", segs[k].coordinates)
            _addLine(tid, tid + "-src", {"line-color": segs[k].color, "line-width": selected ? 7.5 : 5.0})
            _trafficIds.push(tid)
        }
    }

    // --- Pins --------------------------------------------------------
    property var _pinItems: ({})
    property var _pinCoords: ({})

    function clearPins() {
        for (var id in _pinItems) removePin(id)
    }

    function addPin(id, lat, lng, kind) {
        removePin(id)
        _pinCoords[id] = QtPositioning.coordinate(lat, lng)
        _pinItems[id] = pinComponent.createObject(root, {"kind": kind})
        _reposition()
    }

    function removePin(id) {
        var item = _pinItems[id]
        if (item) {
            item.destroy()
            delete _pinItems[id]
            delete _pinCoords[id]
        }
    }

    Component {
        id: pinComponent
        Pin {}
    }

    // --- My location -------------------------------------------------
    property bool _tracking: false
    property var _me: QtPositioning.coordinate()

    function setMyLocation(lat, lng, hasFix) {
        if (!hasFix) {
            _tracking = false
            meDot.visible = false
            return
        }
        root._me = QtPositioning.coordinate(lat, lng)
        _tracking = true
        _reposition()
    }

    LocationDot { id: meDot; z: 10 }

    // Overlays are plain items placed from the map's own projection, redone
    // whenever the camera or the size changes.
    function _reposition() {
        if (!map) return
        function place(item, coord, anchorBottom) {
            var pt = map.fromCoordinate(coord, false)
            var vis = !isNaN(pt.x) && pt.x >= -50 && pt.y >= -50 && pt.x <= map.width + 50 && pt.y <= map.height + 50
            item.visible = vis
            if (!vis) return
            item.x = pt.x - item.width / 2
            item.y = anchorBottom ? pt.y - item.height : pt.y - item.height / 2
        }
        if (_tracking) place(meDot, _me, false)
        for (var id in _pinItems)
            place(_pinItems[id], _pinCoords[id], true)
    }

    onWidthChanged: _reposition()
    onHeightChanged: _reposition()

    Loader {
        id: mapLoader
        anchors.fill: parent
        active: root._styleUrl !== "" && !root._rebuilding
        sourceComponent: mapComponent
    }

    Component {
        id: mapComponent
        Map {
            id: map
            property alias styleObj: mapStyle
            property bool gestureInProgress: dragger.active || pincher.active

            plugin: Plugin {
                name: "maplibre"
                PluginParameter { name: "maplibre.map.styles"; value: root._styleUrl }
            }
            minimumZoomLevel: 2
            maximumZoomLevel: 21.5
            maximumTilt: 60

            MapLibre.style: Style { id: mapStyle }

            onCenterChanged: { root._lat = center.latitude; root._lng = center.longitude; root._reposition() }
            onZoomLevelChanged: { root.zoomLevel = zoomLevel; root._reposition() }
            onBearingChanged: { root._bearing = bearing; root._reposition() }
            onTiltChanged: { root._tilt = tilt; root._reposition() }
            onGestureInProgressChanged: {
                if (gestureInProgress && root._following) {
                    root._following = false
                    root.followInterrupted()
                }
            }
            onMapReadyChanged: if (mapReady) { map.tilt = root._tilt; map.bearing = root._bearing }
            Component.onCompleted: {
                // Restore the camera (a style swap builds a fresh Map).
                map.center = QtPositioning.coordinate(root._lat, root._lng)
                map.zoomLevel = root.zoomLevel
                map.bearing = root._bearing
                map.tilt = root._tilt
                root.ready = true
                root._reposition()
                Qt.callLater(root._replay)
            }

            // The map's gestures must not take a touch over from the panel
            // beside it (list scrolling, buttons): only from other handlers.
            DragHandler {
                id: dragger
                target: null
                grabPermissions: PointerHandler.CanTakeOverFromHandlersOnly | PointerHandler.ApprovesTakeOverByAnything
                property point last: Qt.point(0, 0)
                onActiveChanged: last = Qt.point(0, 0)
                onTranslationChanged: {
                    map.pan(last.x - translation.x, last.y - translation.y)
                    last = Qt.point(translation.x, translation.y)
                }
            }
            PinchHandler {
                id: pincher
                target: null
                grabPermissions: PointerHandler.CanTakeOverFromHandlersOnly | PointerHandler.ApprovesTakeOverByAnything
                onScaleChanged: (delta) => map.zoomLevel += Math.log(delta) / Math.LN2
                onRotationChanged: (delta) => map.bearing -= delta
            }
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (event) => map.zoomLevel += event.angleDelta.y / 240
            }
            TapHandler {
                onTapped: (point) => root.mapClicked(map.toCoordinate(point.position, false))
                onLongPressed: root.mapLongPressed(map.toCoordinate(point.position, false))
            }
        }
    }
}
