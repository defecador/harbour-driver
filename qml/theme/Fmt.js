.pragma library

// Formatting helpers shared by the WeGo-style screens. Plain ES5 (Qt 5.6 JS engine).

// HERE's transit line colors sometimes come back already "#"-prefixed and
// sometimes bare, so a caller that always prepends "#" itself can end up with
// "##rrggbb", which QML's color parser rejects and silently renders as black.
function hexColor(hex, fallback) {
    if (!hex) return fallback;
    return hex.charAt(0) === "#" ? hex : ("#" + hex);
}

function duration(seconds) {
    seconds = seconds || 0;
    var mins = Math.round(seconds / 60);
    if (mins < 1) return "1 min";
    var h = Math.floor(mins / 60);
    var m = mins % 60;
    if (h <= 0) return m + " min";
    if (m === 0) return h + " h";
    return h + " h " + m + " min";
}

function distance(meters, units) {
    meters = meters || 0;
    if (units === "imperial") {
        var feet = meters * 3.28084;
        if (feet < 528) return Math.round(feet) + " ft";
        var miles = feet / 5280;
        return (miles < 10 ? miles.toFixed(1) : Math.round(miles)) + " mi";
    }
    if (meters < 1000) return Math.round(meters) + " m";
    var km = meters / 1000;
    return (km < 10 ? km.toFixed(1) : Math.round(km)) + " km";
}

function delayText(delaySeconds) {
    if (delaySeconds === undefined || delaySeconds === null || delaySeconds < 0)
        return "";
    if (delaySeconds < 60) return "on time";
    return "+" + Math.round(delaySeconds / 60) + " min";
}

function isLate(delaySeconds) {
    return delaySeconds !== undefined && delaySeconds !== null && delaySeconds >= 60;
}

function modeLabel(mode) {
    switch (mode) {
    case "car": return "Drive";
    case "transit": return "Transit";
    case "pedestrian": return "Walk";
    case "bicycle": return "Bike";
    default: return mode || "";
    }
}

function modeIcon(mode) {
    switch (mode) {
    case "car": return "directions_car";
    case "transit": return "directions_transit";
    case "pedestrian": return "directions_walk";
    case "bicycle": return "directions_bike";
    default: return "directions";
    }
}

function transitLabel(transportMode) {
    switch (transportMode) {
    case "highSpeedTrain":
    case "intercityTrain":
    case "regionalTrain":
    case "cityTrain":
    case "railShuttle": return "Train";
    case "lightRail": return "Tram";
    case "subway": return "Subway";
    case "bus":
    case "busRapid": return "Bus";
    case "ferry": return "Ferry";
    default: return "Transit";
    }
}

function transitIcon(transportMode) {
    switch (transportMode) {
    case "highSpeedTrain":
    case "intercityTrain":
    case "regionalTrain":
    case "cityTrain":
    case "railShuttle": return "train";
    case "lightRail": return "tram";
    case "subway": return "subway";
    case "bus":
    case "busRapid": return "directions_bus";
    case "ferry": return "directions_boat";
    default: return "directions_transit";
    }
}

function turnIcon(action, direction) {
    switch (action) {
    case "depart": return "trip_origin";
    case "arrive": return "flag";
    case "continue":
    case "roundaboutPass": return "straight";
    case "uTurn": return "u_turn_left";
    case "fork":
        return direction === "right" ? "fork_right" : "fork_left";
    case "roundaboutEnter":
    case "roundaboutExit": return "roundabout_right";
    case "rampProceed": return "ramp_right";
    case "turn":
    case "exit":
        switch (direction) {
        case "left": return "turn_left";
        case "right": return "turn_right";
        case "slightLeft": return "turn_slight_left";
        case "slightRight": return "turn_slight_right";
        case "sharpLeft": return "turn_sharp_left";
        case "sharpRight": return "turn_sharp_right";
        default: return "straight";
        }
    default: return "straight";
    }
}

function distanceMeters(o) {
    return o && o.distance !== undefined ? o.distance : -1;
}
