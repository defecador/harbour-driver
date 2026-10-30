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
    case "ramp":
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

// HERE's actions carry no road class, so motorway manoeuvres are recognised
// from the action type (ramp / exit) and from the road named in the text
// ("... onto A100 (Stadtring)", "... take exit 10 toward Potsdam onto A115").
// Returns {highway, ref, exitNo, toward}.
function roadSign(action, instruction) {
    var text = (instruction || "").split(/\.\s/)[0]
    var ref = ""
    var m = /\b(?:onto|on)\s+((?:[AEM]\s?\d{1,3}|I-\d{1,3}|BAB\s?\d{1,3})(?:-[A-Z0-9]+)?)\b/.exec(text)
    if (m)
        ref = m[1].replace(/\s+/, "").replace(/^BAB/, "A")
    var named = /\b(motorway|freeway|highway|interstate|autobahn|expressway|autoroute|autostrada|autopista|autovia)\b/i.test(text)
    var exitNo = ""
    m = /\bexit\s+([0-9]+[A-Za-z]?)\b/i.exec(text)
    if (m && action === "exit")
        exitNo = m[1]
    var toward = ""
    m = /\btoward\s+(.+?)(?:\s+onto\s|$)/.exec(text)
    if (m)
        toward = m[1].split("/").slice(0, 2).join(" · ")
    return {
        highway: action === "ramp" || action === "exit" || ref.length > 0 || named,
        ref: ref, exitNo: exitNo, toward: toward
    }
}

function distanceMeters(o) {
    return o && o.distance !== undefined ? o.distance : -1;
}

// Country name for an ISO 3166 alpha-3 code as HERE reports it (the codes
// seen on European toll routes); unknown codes are shown as they are.
var COUNTRIES = {
    ALB: "Albania", AND: "Andorra", AUT: "Austria", BEL: "Belgium", BGR: "Bulgaria", BIH: "Bosnia and Herzegovina",
    BLR: "Belarus", CHE: "Switzerland", CYP: "Cyprus", CZE: "Czechia", DEU: "Germany", DNK: "Denmark",
    ESP: "Spain", EST: "Estonia", FIN: "Finland", FRA: "France", GBR: "United Kingdom", GEO: "Georgia",
    GRC: "Greece", HRV: "Croatia", HUN: "Hungary", IRL: "Ireland", ISL: "Iceland", ITA: "Italy",
    LIE: "Liechtenstein", LTU: "Lithuania", LUX: "Luxembourg", LVA: "Latvia", MCO: "Monaco", MDA: "Moldova",
    MKD: "North Macedonia", MLT: "Malta", MNE: "Montenegro", NLD: "Netherlands", NOR: "Norway", POL: "Poland",
    PRT: "Portugal", ROU: "Romania", RUS: "Russia", SMR: "San Marino", SRB: "Serbia", SVK: "Slovakia",
    SVN: "Slovenia", SWE: "Sweden", TUR: "Turkey", UKR: "Ukraine", XKX: "Kosovo"
};

function countryName(code) {
    return COUNTRIES[code] || code;
}
