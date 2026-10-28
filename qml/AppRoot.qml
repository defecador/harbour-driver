import QtQuick 2.6
import QtPositioning 5.3
import "theme"
import "components"
import "map"
import "screens"

// Screen-state machine for the whole app: a full-screen map with a draggable
// Home sheet, and a right-sliding ScreenStack for everything else (search,
// place cards, the trip planner, route/transit details, settings...).
Item {
    id: root
    anchors.fill: parent

    // Wide windows (a tablet in landscape) lay the screens out as a left
    // panel beside the map; narrow ones (phone, or half of a split screen)
    // keep the bottom sheet and full-screen pages.
    readonly property bool wide: width >= 840 * M.dp
    readonly property real panelW: Math.min(440 * M.dp, width * 0.42)
    readonly property real mapLeftInset: wide && !navItem ? panelW : 0
    onMapLeftInsetChanged: mapView.setMargins(mapLeftInset, 0, 0, 0)

    function meAsPlace() {
        return {
            "title": qsTr("Your location"),
            "address": "",
            "lat": mapView.myLat, "lng": mapView.myLng,
            "navLat": mapView.myLat, "navLng": mapView.myLng
        }
    }

    function openSearch() {
        var item = stack.push(Qt.resolvedUrl("screens/SearchScreen.qml"), {})
        if (item) {
            item.placeChosen.connect(function(place) {
                stack.pop()
                AppSettings.addRecent(place)
                root.openPlaceCard(place)
            })
            item.closed.connect(function() { stack.pop() })
        }
    }

    function openPlaceCard(place, kind) {
        mapView.clearPins()
        mapView.showPin("dest", place, kind || "destination")
        mapView.fitCoordinates([placeCoord(place)], false)
        var item = stack.push(Qt.resolvedUrl("screens/PlaceCard.qml"), {"place": place})
        if (item) {
            item.directionsClicked.connect(function(p) {
                root.startPlanning(p)
            })
            item.closed.connect(function() {
                stack.pop()
                mapView.clearPins()
            })
        }
    }

    function placeCoord(p) {
        var lat = p.navLat !== undefined ? p.navLat : p.lat
        var lng = p.navLng !== undefined ? p.navLng : p.lng
        return QtPositioning.coordinate(lat, lng)
    }

    function startPlanning(toPlace) {
        TripPlanner.from = root.meAsPlace()
        TripPlanner.to = toPlace
        TripPlanner.plan()
        stack.clear()
        var item = stack.push(Qt.resolvedUrl("screens/PlannerScreen.qml"), {})
        if (item) {
            item.openDetails.connect(function() {
                root.openDetails()
            })
            item.closed.connect(function() {
                stack.clear()
                mapView.clearRoutes()
                mapView.clearPins()
            })
            item.previewFrame.connect(function(x, y, w, h) {
                var coords = TripPlanner.selected.coordinates || []
                mapView.fitInRect(coords, x, y, w, h)
            })
            item.editField.connect(function(field) {
                var search = stack.push(Qt.resolvedUrl("screens/SearchScreen.qml"), {})
                if (search) {
                    search.placeChosen.connect(function(place) {
                        stack.pop()
                        AppSettings.addRecent(place)
                        if (field === "from") TripPlanner.from = place
                        else TripPlanner.to = place
                        TripPlanner.plan()
                    })
                    search.closed.connect(function() { stack.pop() })
                }
            })
        }
    }

    function openDetails() {
        var mode = TripPlanner.selectedMode
        var url = mode === "transit" ? Qt.resolvedUrl("screens/TransitDetailsScreen.qml") : Qt.resolvedUrl("screens/RouteDetailsScreen.qml")
        var item = stack.push(url, {})
        if (item) {
            item.closed.connect(function() { stack.pop() })
            item.startNavigation.connect(function() { root.startNavigation(false) })
        }
    }

    property var navItem: null
    property int navTilt: 0
    // The itinerary currently being followed (replaced when a reroute or a
    // newly added stop produces a new one).
    property var navItinerary: null
    property var stopCandidate: null
    property bool stopLookup: false
    property int stopPinCount: 0
    // Distance of the driving camera, chosen with the slider; lasts for the
    // app session. 18.3 is the default street-level view.
    property real povZoom: 18.3

    // simulate: true plays back the planned route's own coordinates as fake
    // GPS instead of waiting for a real fix — used by the `navigate` dev
    // screen and (once wired to a Settings toggle, Milestone 2 step 6) by
    // testers without driving anywhere.
    function startNavigation(simulate) {
        var itin = TripPlanner.selected
        // Same emptiness check as updateRoutePreview() below: a QVariantMap
        // exposed to QML doesn't reliably support Object.keys(), so check a
        // concrete field instead of the map's own "shape".
        if (!itin || !itin.coordinates || itin.coordinates.length < 2) {
            // Not planned yet (e.g. a dev screen that just called
            // startPlanning()): wait for that fixture/network response.
            var conn = function() {
                TripPlanner.resultsChanged.disconnect(conn)
                root.startNavigation(simulate)
            }
            TripPlanner.resultsChanged.connect(conn)
            return
        }
        root.navItinerary = itin
        Navigator.start(itin, TripPlanner.to)
        if (itin.mode === "transit" && simulate)
            Navigator.setSimulationClock(true, 6.0)
        // Car gets a pitched, heading-up 3D view; walk/bike/transit stay
        // flat and north-up.
        root.navTilt = itin.mode === "car" && AppSettings.view3d ? 60 : 0
        root._lastOverlayFraction = -1
        mapView.setPovZoom(root.povZoom)
        mapView.startFollowMode(root.navTilt, simulate)
        stack.clear()
        var item = stack.push(Qt.resolvedUrl("screens/NavigationScreen.qml"),
                              {"simulate": simulate === true, "povControls": root.navTilt > 0, "povZoom": root.povZoom})
        root.navItem = item
        if (item) {
            item.closed.connect(function() { root.stopNavigation() })
            item.positionSimulated.connect(function(lat, lng, heading) {
                mapView.updateFollowPosition(lat, lng, heading)
            })
            item.povZoomChanged.connect(function() {
                root.povZoom = item.povZoom
                mapView.setPovZoom(item.povZoom)
            })
            item.addStopRequested.connect(function() {
                if (root.stopCandidate)
                    Navigator.addStop(root.stopCandidate)
                root.clearStopCandidate()
            })
            item.stopCandidateDismissed.connect(function() { root.clearStopCandidate() })
            item.followToggled.connect(function(want) {
                mapView.setFollowActive(want, root.navTilt)
            })
        }
    }

    // Tapping (or long-pressing) the map while walking or cycling offers to
    // add that spot as a stop. Returns true when the tap was used for that.
    function mapTappedDuringNav(coordinate) {
        if (!root.navItem || Navigator.state !== "guiding"
                || (Navigator.mode !== "pedestrian" && Navigator.mode !== "bicycle"))
            return false
        var lat = coordinate.latitude, lng = coordinate.longitude
        root.stopCandidate = {
            "title": qsTr("Dropped pin"), "address": "",
            "lat": lat, "lng": lng, "navLat": lat, "navLng": lng
        }
        mapView.removePin("stopcand")
        mapView.showPin("stopcand", root.stopCandidate, "place")
        root.navItem.stopCandidate = root.stopCandidate
        root.stopLookup = true
        SearchController.reverse(lat, lng)
        return true
    }

    function clearStopCandidate() {
        root.stopCandidate = null
        root.stopLookup = false
        mapView.removePin("stopcand")
        if (root.navItem)
            root.navItem.stopCandidate = null
    }

    function redrawStopPins() {
        for (var i = 0; i < root.stopPinCount; i++)
            mapView.removePin("stop" + i)
        var stops = Navigator.stops
        for (var j = 0; j < stops.length; j++)
            mapView.showPin("stop" + j, stops[j], "place")
        root.stopPinCount = stops.length
    }

    Connections {
        target: Navigator
        onStopsChanged: if (root.navItem) root.redrawStopPins()
        onRerouted: {
            if (!root.navItem)
                return
            root.navItinerary = newItinerary
            mapView.clearRoutes()
            mapView.showRoutes([newItinerary], 0)
            root._lastOverlayFraction = -1
        }
    }

    function stopNavigation() {
        root.clearStopCandidate()
        for (var i = 0; i < root.stopPinCount; i++)
            mapView.removePin("stop" + i)
        root.stopPinCount = 0
        root.navItinerary = null
        Navigator.stop()
        Navigator.setSimulationClock(false)
        mapView.stopFollowMode()
        mapView.clearRoutes()
        mapView.clearPins()
        stack.clear()
        root.navItem = null
    }

    Connections {
        target: mapView
        onFollowInterrupted: if (root.navItem) root.navItem.following = false
    }

    // Greys out the part of the selected route already covered, using
    // Navigator's traveledFraction as an index into the same coordinate
    // list the route was drawn from. Approximate (points aren't evenly
    // spaced by distance) and throttled to ~20m of travel, since it's a
    // cosmetic overlay, not something guidance itself depends on.
    property real _lastOverlayFraction: -1
    function updateTraveledOverlay() {
        if (!root.navItem || Navigator.mode === "transit")
            return
        var frac = Navigator.traveledFraction
        var itin = root.navItinerary || TripPlanner.selected
        var coords = itin.coordinates || []
        if (coords.length < 2)
            return
        var totalLen = itin.length || 0
        var minStep = totalLen > 0 ? 20 / totalLen : 0.02
        if (frac < root._lastOverlayFraction + minStep && frac < 0.999)
            return
        root._lastOverlayFraction = frac
        var count = Math.max(2, Math.min(coords.length, Math.round(coords.length * frac) + 1))
        mapView.setTraveledCoordinates(coords.slice(0, count))
    }

    Connections {
        target: Navigator
        onManeuverChanged: root.updateTraveledOverlay()
    }

    function openLayers() {
        var item = stack.push(Qt.resolvedUrl("screens/LayersSheet.qml"), {})
        if (item) item.closed.connect(function() { stack.pop() })
    }

    function openSettings() {
        var item = stack.push(Qt.resolvedUrl("screens/SettingsScreen.qml"), {})
        if (item) item.closed.connect(function() { stack.pop() })
    }

    function openAbout() {
        var item = stack.push(Qt.resolvedUrl("screens/AboutScreen.qml"), {})
        if (item) item.closed.connect(function() { stack.pop() })
    }

    MapView {
        id: mapView
        anchors.fill: parent
        onMapClicked: root.mapTappedDuringNav(coordinate)
        onMapLongPressed: {
            if (root.mapTappedDuringNav(coordinate))
                return
            var c = coordinate
            SearchController.reverse(c.latitude, c.longitude)
        }
    }

    Connections {
        target: SearchController
        onReverseFinished: {
            if (root.stopLookup) {
                root.stopLookup = false
                if (error.length === 0 && root.stopCandidate && root.navItem) {
                    var c = root.stopCandidate
                    c.title = place.title || c.title
                    c.address = place.address || ""
                    root.stopCandidate = c
                    root.navItem.stopCandidate = c
                }
                return
            }
            if (error.length === 0)
                root.openPlaceCard(place)
        }
    }

    function updateRoutePreview() {
        if (stack.depth === 0) return
        var list = TripPlanner.results[TripPlanner.selectedMode] || []
        mapView.showRoutes(list, TripPlanner.selectedIndex)
        if (list.length > 0) {
            var it = list[TripPlanner.selectedIndex] || list[0]
            if (it.coordinates && it.coordinates.length > 0)
                mapView.fitCoordinates(it.coordinates, false)
        }
    }

    Connections {
        target: TripPlanner
        onResultsChanged: root.updateRoutePreview()
        onSelectionChanged: root.updateRoutePreview()
    }

    MapControls {
        id: controls
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: M.spacingM
        anchors.rightMargin: M.spacingM
        z: 20
        visible: (root.wide || sheet.sheetState !== "full") && stack.depth === 0
        locating: mapView.hasFix
        onLayersClicked: root.openLayers()
        onLocateClicked: mapView.centerOnMe()
    }

    BottomSheet {
        id: sheet
        z: 10
        sidePanel: root.wide
        panelWidth: root.panelW
        // Room for the saved-places row when there is one.
        peekHeight: (172 + (AppSettings.savedPlaces.length > 0 ? 52 : 0)) * M.dp
        visible: stack.depth === 0

        HomeSheet {
            onSearchTapped: root.openSearch()
            onPlaceTapped: root.openPlaceCard(place)
            onSettingsTapped: root.openSettings()
            onAboutTapped: root.openAbout()
        }
    }

    ScreenStack {
        id: stack
        z: 30
        panelWidth: root.mapLeftInset
    }

    Snackbar {
        id: snackbar
        z: 40
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: M.spacingM
        anchors.leftMargin: root.mapLeftInset + M.spacingM
        anchors.bottomMargin: (stack.depth === 0 && !root.wide ? sheet.peekHeight : 0) + M.spacingM
    }

    Connections {
        target: AppSettings
        onTermsAcceptedChanged: if (AppSettings.termsAccepted && stack.depth > 0) stack.pop()
    }

    function devSamplePlace() {
        return {
            "title": "Brandenburg Gate",
            "address": "Brandenburg Gate, Pariser Platz, 10117 Berlin, Germany",
            "lat": 52.51628, "lng": 13.37770,
            "navLat": 52.51628, "navLng": 13.37770
        }
    }

    function applyDevScreen(name) {
        AppSettings.termsAccepted = true
        if (name === "terms") {
            AppSettings.termsAccepted = false
            stack.push(Qt.resolvedUrl("screens/FirstRunTerms.qml"), {})
        } else if (name === "search") {
            root.openSearch()
        } else if (name === "place") {
            root.openPlaceCard(root.devSamplePlace())
        } else if (name === "planner") {
            root.startPlanning(root.devSamplePlace())
        } else if (name === "plannerpreview") {
            root.startPlanning(root.devSamplePlace())
            TripPlanner.selectedMode = "car"
        } else if (name === "plannerbike") {
            root.startPlanning(root.devSamplePlace())
            TripPlanner.selectedMode = "bicycle"
        } else if (name === "routedetails") {
            root.startPlanning(root.devSamplePlace())
            TripPlanner.selectedMode = "car"
            root.openDetails()
        } else if (name === "transitdetails") {
            root.startPlanning(root.devSamplePlace())
            TripPlanner.selectedMode = "transit"
            root.openDetails()
        } else if (name === "navigate") {
            // Simulated car navigation over the sample fixture route, for
            // testing/screenshots without a real drive (--fixtures required
            // for the coordinates to be non-empty).
            root.startPlanning(root.devSamplePlace())
            TripPlanner.selectedMode = "car"
            root.startNavigation(true)
        } else if (name === "homeset") {
            // Home and Work saved with long addresses (use a blank config dir).
            AppSettings.home = {"title": "Karl-Liebknecht-Strasse 1, 10178 Berlin, Germany", "address": "", "lat": 52.52, "lng": 13.40}
            AppSettings.work = {"title": "Unter den Linden 77, 10117 Berlin, Germany", "address": "", "lat": 52.516, "lng": 13.38}
            AppSettings.addSavedPlace({"title": "Tempelhofer Feld, Berlin, Germany", "address": "", "lat": 52.47, "lng": 13.40})
            AppSettings.addSavedPlace({"title": "Cafe Einstein Stammhaus, Kurfuerstenstrasse 58", "address": "", "lat": 52.50, "lng": 13.36})
            AppSettings.addSavedPlace({"title": "Mauerpark, Berlin", "address": "", "lat": 52.54, "lng": 13.40})
            AppSettings.addSavedPlace({"title": "Treptower Park, Berlin", "address": "", "lat": 52.49, "lng": 13.46})
        } else if (name === "navwalkstop") {
            // Simulated walk with a tapped-map "add stop" candidate showing.
            root.startPlanning(root.devSamplePlace())
            TripPlanner.selectedMode = "pedestrian"
            root.startNavigation(true)
            devStopTap.start()
        } else if (name === "navtransit") {
            // Simulated transit navigation (walk -> wait -> ride -> alight
            // -> walk -> arrived) over the sample fixture trip.
            root.startPlanning(root.devSamplePlace())
            TripPlanner.selectedMode = "transit"
            root.startNavigation(true)
        } else if (name === "layers") {
            root.openLayers()
        } else if (name === "settings") {
            root.openSettings()
        } else if (name === "about") {
            root.openAbout()
        } else if (name === "locate") {
            locateRetry.start()
        } else if (name === "osmtest") {
            mapView.debugSetStyle(AppTools.debugOsmStyleJson())
        }
    }

    Timer {
        id: devStopTap
        interval: 2500
        onTriggered: {
            var coords = (root.navItinerary || {}).coordinates || []
            if (coords.length > 2)
                root.mapTappedDuringNav(coords[Math.floor(coords.length * 0.6)])
        }
    }

    Timer {
        id: locateRetry
        interval: 500
        repeat: true
        property int tries: 0
        onTriggered: {
            tries += 1
            if (mapView.hasFix) {
                mapView.centerOnMe()
                stop()
            } else if (tries > 20) {
                stop()
            }
        }
    }

    // Android back key: closes the top screen like its own close button.
    // Returns false at the root, where the key should leave the app.
    function goBack() {
        if (stack.depth === 0)
            return false
        var top = stack._stack[stack._stack.length - 1]
        if (top && top.closed)
            top.closed()
        else
            stack.pop()
        return true
    }

    Component.onCompleted: {
        if (AppTools.devScreen.length > 0) {
            root.applyDevScreen(AppTools.devScreen)
        } else if (!AppSettings.termsAccepted) {
            stack.push(Qt.resolvedUrl("screens/FirstRunTerms.qml"), {})
        }
    }
}
