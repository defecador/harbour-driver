#include "ItineraryParser.h"

#include <QMap>
#include "FlexiblePolyline.h"

#include <QDateTime>
#include <QGeoCoordinate>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

namespace {

QVariantList decodeCoords(const QString &polyline)
{
    QVariantList out;
    const auto pts = FlexiblePolyline::decode(polyline);
    out.reserve(pts.size());
    for (const auto &p : pts)
        out.append(QVariant::fromValue(QGeoCoordinate(p.lat, p.lng)));
    return out;
}

qint64 secondsBetween(const QString &a, const QString &b)
{
    const QDateTime da = QDateTime::fromString(a, Qt::ISODate);
    const QDateTime db = QDateTime::fromString(b, Qt::ISODate);
    if (!da.isValid() || !db.isValid())
        return -1;
    return da.secsTo(db);
}

QVariantMap parseStop(const QJsonObject &ev)
{
    QVariantMap m;
    const QJsonObject place = ev.value(QStringLiteral("place")).toObject();
    const QJsonObject loc = place.value(QStringLiteral("location")).toObject();
    const QString time = ev.value(QStringLiteral("time")).toString();
    const QDateTime dt = QDateTime::fromString(time, Qt::ISODate);
    const qint64 timeMs = dt.isValid() ? dt.toMSecsSinceEpoch() : -1;
    m[QStringLiteral("name")] = place.value(QStringLiteral("name")).toString();
    m[QStringLiteral("time")] = time;
    m[QStringLiteral("timeText")] = ItineraryParser::timeText(time);
    m[QStringLiteral("delay")] = ev.contains(QStringLiteral("delay")) ? ev.value(QStringLiteral("delay")).toInt() : -1;
    // Epoch milliseconds, parsed once here so navigation doesn't re-parse ISO
    // strings every tick. HERE's `time` is taken to already be the expected
    // (real-time) time, with `delay` only saying how far that is from the
    // schedule — so expectedMs == timeMs. Still to be confirmed against a live
    // response; if `time` turns out to be the scheduled time instead, this is
    // the one line to change (add the delay).
    m[QStringLiteral("timeMs")] = timeMs;
    m[QStringLiteral("expectedMs")] = timeMs;
    m[QStringLiteral("status")] = ev.value(QStringLiteral("status")).toString();
    m[QStringLiteral("platform")] = place.value(QStringLiteral("platform")).toString();
    m[QStringLiteral("lat")] = loc.value(QStringLiteral("lat")).toDouble();
    m[QStringLiteral("lng")] = loc.value(QStringLiteral("lng")).toDouble();
    return m;
}

// baseOffset shifts each action's `offset` from being an index into the section's
// own polyline to an index into some larger, already-flattened coordinate list
// (parseRoutes uses this to make route-level actions index route-level coordinates;
// transit leg actions are left leg-local, baseOffset = 0, since Navigator walks
// transit legs one at a time).
QVariantList parseActions(const QJsonArray &arr, int baseOffset = 0)
{
    QVariantList out;
    for (const auto &v : arr) {
        const QJsonObject a = v.toObject();
        QVariantMap m;
        m[QStringLiteral("action")] = a.value(QStringLiteral("action")).toString();
        m[QStringLiteral("direction")] = a.value(QStringLiteral("direction")).toString();
        m[QStringLiteral("severity")] = a.value(QStringLiteral("severity")).toString();
        m[QStringLiteral("instruction")] = a.value(QStringLiteral("instruction")).toString();
        m[QStringLiteral("length")] = a.value(QStringLiteral("length")).toInt();
        m[QStringLiteral("duration")] = a.value(QStringLiteral("duration")).toInt();
        m[QStringLiteral("exit")] = a.value(QStringLiteral("exit")).toInt();
        m[QStringLiteral("offset")] = baseOffset + a.value(QStringLiteral("offset")).toInt();
        out.append(m);
    }
    return out;
}


// Google-style encoded polyline with 6 decimal places (Valhalla's shape format).
QVariantList decodePolyline6(const QString &enc)
{
    QVariantList out;
    int index = 0, lat = 0, lng = 0;
    const int n = enc.size();
    auto next = [&](int &acc) {
        int shift = 0, result = 0, b;
        do {
            if (index >= n)
                return false;
            b = enc.at(index++).unicode() - 63;
            result |= (b & 0x1f) << shift;
            shift += 5;
        } while (b >= 0x20);
        acc += (result & 1) ? ~(result >> 1) : (result >> 1);
        return true;
    };
    while (index < n) {
        if (!next(lat) || !next(lng))
            break;
        out.append(QVariant::fromValue(QGeoCoordinate(lat / 1e6, lng / 1e6)));
    }
    return out;
}

// Valhalla maneuver type -> HERE-style action/direction, which the UI icons use.
void valhallaAction(int type, QString &action, QString &direction)
{
    direction.clear();
    switch (type) {
    case 1: case 2: case 3: action = QStringLiteral("depart"); break;
    case 4: case 5: case 6: action = QStringLiteral("arrive"); break;
    case 9: action = QStringLiteral("turn"); direction = QStringLiteral("slightRight"); break;
    case 10: action = QStringLiteral("turn"); direction = QStringLiteral("right"); break;
    case 11: action = QStringLiteral("turn"); direction = QStringLiteral("sharpRight"); break;
    case 12: case 13: action = QStringLiteral("uTurn"); break;
    case 14: action = QStringLiteral("turn"); direction = QStringLiteral("sharpLeft"); break;
    case 15: action = QStringLiteral("turn"); direction = QStringLiteral("left"); break;
    case 16: action = QStringLiteral("turn"); direction = QStringLiteral("slightLeft"); break;
    case 17: case 18: case 19: action = QStringLiteral("rampProceed"); break;
    case 20: action = QStringLiteral("exit"); direction = QStringLiteral("right"); break;
    case 21: action = QStringLiteral("exit"); direction = QStringLiteral("left"); break;
    case 23: action = QStringLiteral("fork"); direction = QStringLiteral("right"); break;
    case 24: action = QStringLiteral("fork"); direction = QStringLiteral("left"); break;
    case 26: action = QStringLiteral("roundaboutEnter"); break;
    case 27: action = QStringLiteral("roundaboutExit"); break;
    default: action = QStringLiteral("continue"); break;
    }
}

}

QVariantList ItineraryParser::parseValhalla(const QJsonObject &root, const QString &mode)
{
    const QJsonObject trip = root.value(QStringLiteral("trip")).toObject();
    const QJsonArray legsJson = trip.value(QStringLiteral("legs")).toArray();
    if (trip.value(QStringLiteral("status")).toInt(-1) != 0 || legsJson.isEmpty())
        return QVariantList();

    QVariantList coords, actions;
    int pointOffset = 0;
    for (const auto &lv : legsJson) {
        const QJsonObject lj = lv.toObject();
        QVariantList c = decodePolyline6(lj.value(QStringLiteral("shape")).toString());
        // Consecutive legs share their joint point: keep it once.
        const int skip = coords.isEmpty() ? 0 : 1;
        if (skip && !c.isEmpty())
            c.removeFirst();
        for (const auto &mv : lj.value(QStringLiteral("maneuvers")).toArray()) {
            const QJsonObject m = mv.toObject();
            QVariantMap a;
            QString action, direction;
            valhallaAction(m.value(QStringLiteral("type")).toInt(), action, direction);
            a[QStringLiteral("action")] = action;
            a[QStringLiteral("direction")] = direction;
            a[QStringLiteral("instruction")] = m.value(QStringLiteral("instruction")).toString();
            a[QStringLiteral("length")] = int(m.value(QStringLiteral("length")).toDouble() * 1000.0 + 0.5);
            a[QStringLiteral("duration")] = int(m.value(QStringLiteral("time")).toDouble() + 0.5);
            const int begin = qMax(0, m.value(QStringLiteral("begin_shape_index")).toInt() - skip);
            a[QStringLiteral("offset")] = pointOffset + begin;
            actions.append(a);
        }
        coords.append(c);
        pointOffset += c.size();
    }

    const QJsonObject sum = trip.value(QStringLiteral("summary")).toObject();
    const int duration = int(sum.value(QStringLiteral("time")).toDouble() + 0.5);
    const int length = int(sum.value(QStringLiteral("length")).toDouble() * 1000.0 + 0.5);
    const QDateTime depart = QDateTime::currentDateTime();
    const QString dep = depart.toString(Qt::ISODate);
    const QString arr = depart.addSecs(duration).toString(Qt::ISODate);

    QVariantMap leg;
    leg[QStringLiteral("type")] = mode;
    leg[QStringLiteral("duration")] = duration;
    leg[QStringLiteral("length")] = length;
    leg[QStringLiteral("coordinates")] = coords;
    leg[QStringLiteral("actions")] = actions;

    QVariantMap attribution;
    attribution[QStringLiteral("text")] = QStringLiteral("Routing: Valhalla (FOSSGIS) · Map data © OpenStreetMap contributors");

    QVariantMap it;
    it[QStringLiteral("mode")] = mode;
    it[QStringLiteral("duration")] = duration;
    it[QStringLiteral("baseDuration")] = duration;
    it[QStringLiteral("trafficDelay")] = 0;
    it[QStringLiteral("length")] = length;
    it[QStringLiteral("departureTime")] = dep;
    it[QStringLiteral("arrivalTime")] = arr;
    it[QStringLiteral("departureText")] = timeText(dep);
    it[QStringLiteral("arrivalText")] = timeText(arr);
    it[QStringLiteral("transfers")] = 0;
    it[QStringLiteral("walkDuration")] = 0;
    it[QStringLiteral("coordinates")] = coords;
    it[QStringLiteral("legs")] = QVariantList{leg};
    it[QStringLiteral("actions")] = actions;
    it[QStringLiteral("attributions")] = QVariantList{attribution};
    return QVariantList{it};
}

QString ItineraryParser::timeText(const QString &iso)
{
    static const QRegularExpression re(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}T(\\d{2}:\\d{2})"));
    const auto m = re.match(iso);
    return m.hasMatch() ? m.captured(1) : QString();
}

// Slow stretches of a car section, as {start, end, level} indices into the
// section's coordinates (level 1 = moderate, 2 = heavy), from the ratio of
// the current to the free-flow speed in each span. Neighbouring spans of the
// same level are merged.
static QVariantList trafficSegments(const QJsonObject &section, int pointCount, int baseOffset)
{
    QVariantList out;
    const QJsonArray spans = section.value(QStringLiteral("spans")).toArray();
    int curLevel = 0, curStart = 0;
    auto flush = [&](int end) {
        if (curLevel > 0 && end > curStart) {
            QVariantMap m;
            m[QStringLiteral("start")] = baseOffset + curStart;
            m[QStringLiteral("end")] = baseOffset + end;
            m[QStringLiteral("level")] = curLevel;
            out.append(m);
        }
    };
    for (int i = 0; i < spans.size(); ++i) {
        const QJsonObject sp = spans[i].toObject();
        const QJsonObject dyn = sp.value(QStringLiteral("dynamicSpeedInfo")).toObject();
        const double base = dyn.value(QStringLiteral("baseSpeed")).toDouble();
        const double now = dyn.value(QStringLiteral("trafficSpeed")).toDouble();
        int level = 0;
        if (base > 0) {
            const double ratio = now / base;
            level = ratio < 0.4 ? 2 : (ratio < 0.7 ? 1 : 0);
        }
        const int start = qBound(0, sp.value(QStringLiteral("offset")).toInt(), pointCount - 1);
        if (level != curLevel) {
            flush(start);
            curLevel = level;
            curStart = start;
        }
    }
    flush(pointCount - 1);
    return out;
}

// Legal speed limit per stretch of the section (spans=speedLimit, metres per
// second) as {start, end, kmh} point ranges; stretches with no limit are left out.
static QVariantList speedLimitSegments(const QJsonObject &section, int pointCount, int baseOffset)
{
    QVariantList out;
    const QJsonArray spans = section.value(QStringLiteral("spans")).toArray();
    int curKmh = 0, curStart = 0;
    auto flush = [&](int end) {
        if (curKmh > 0 && end > curStart) {
            QVariantMap m;
            m[QStringLiteral("start")] = baseOffset + curStart;
            m[QStringLiteral("end")] = baseOffset + end;
            m[QStringLiteral("kmh")] = curKmh;
            out.append(m);
        }
    };
    for (int i = 0; i < spans.size(); ++i) {
        const QJsonObject sp = spans[i].toObject();
        const QJsonValue v = sp.value(QStringLiteral("speedLimit"));
        const double mps = v.isObject() ? v.toObject().value(QStringLiteral("speedLimit")).toDouble() : v.toDouble();
        const int kmh = mps > 0 ? int(mps * 3.6 + 0.5) : 0;
        const int start = qBound(0, sp.value(QStringLiteral("offset")).toInt(), pointCount - 1);
        if (kmh != curKmh) {
            flush(start);
            curKmh = kmh;
            curStart = start;
        }
    }
    flush(pointCount - 1);
    return out;
}

// Toll stretches of a section (spans=tollSystems + countryCode + segmentRef):
// the countries that charge, in route order, and the road segments to feed
// back as "avoid" to route around them.
static void tollStretches(const QJsonObject &section, QStringList *order, QMap<QString, QStringList> *segments)
{
    for (const QJsonValue &v : section.value(QStringLiteral("spans")).toArray()) {
        const QJsonObject sp = v.toObject();
        if (sp.value(QStringLiteral("tollSystems")).toArray().isEmpty())
            continue;
        const QString country = sp.value(QStringLiteral("countryCode")).toString();
        const QString ref = sp.value(QStringLiteral("segmentRef")).toString();
        if (country.isEmpty() || ref.isEmpty())
            continue;
        if (!order->contains(country))
            order->append(country);
        // The route reports "$0::377526182:$1:155956282#-0..1" (segment id,
        // direction travelled, range); avoiding wants "here:cm:segment:<id>#-".
        static const QRegularExpression refRe(QStringLiteral("^\\$\\d+::\\d+:\\$\\d+:(\\d+)#([+-]?)"));
        const QRegularExpressionMatch m = refRe.match(ref);
        if (!m.hasMatch())
            continue;
        const QString avoid = QStringLiteral("here:cm:segment:") + m.captured(1)
                              + (m.captured(2).isEmpty() ? QString() : QStringLiteral("#") + m.captured(2));
        QStringList &list = (*segments)[country];
        if (!list.contains(avoid))
            list.append(avoid);
    }
}

QVariantList ItineraryParser::parseRoutes(const QJsonObject &root, const QString &mode)
{
    QVariantList out;
    for (const auto &rv : root.value(QStringLiteral("routes")).toArray()) {
        const QJsonArray sections = rv.toObject().value(QStringLiteral("sections")).toArray();
        if (sections.isEmpty())
            continue;
        int duration = 0, base = 0, length = 0;
        // Running count of coordinate *points* seen so far, not sections: `coords`
        // below is a list-of-per-section-lists (each append() adds one nested
        // QVariantList), so coords.size() would only count sections.
        int pointOffset = 0;
        QVariantList coords, legs, actions, traffic, speedLimits;
        QStringList tollOrder;
        QMap<QString, QStringList> tollSegs;
        for (const auto &sv : sections) {
            const QJsonObject s = sv.toObject();
            QJsonObject sum = s.value(QStringLiteral("summary")).toObject();
            if (sum.isEmpty())
                sum = s.value(QStringLiteral("travelSummary")).toObject();
            const int d = sum.value(QStringLiteral("duration")).toInt();
            duration += d;
            base += sum.contains(QStringLiteral("baseDuration")) ? sum.value(QStringLiteral("baseDuration")).toInt() : d;
            length += sum.value(QStringLiteral("length")).toInt();
            const QVariantList c = decodeCoords(s.value(QStringLiteral("polyline")).toString());
            const QVariantList aLocal = parseActions(s.value(QStringLiteral("actions")).toArray());
            const QVariantList a = parseActions(s.value(QStringLiteral("actions")).toArray(), pointOffset);
            if (mode == QLatin1String("car")) {
                traffic.append(trafficSegments(s, c.size(), pointOffset));
                speedLimits.append(speedLimitSegments(s, c.size(), pointOffset));
                tollStretches(s, &tollOrder, &tollSegs);
            }
            coords.append(c);
            pointOffset += c.size();
            actions.append(a);
            QVariantMap leg;
            leg[QStringLiteral("type")] = mode;
            leg[QStringLiteral("duration")] = d;
            leg[QStringLiteral("length")] = sum.value(QStringLiteral("length")).toInt();
            leg[QStringLiteral("coordinates")] = c;
            leg[QStringLiteral("actions")] = aLocal;
            leg[QStringLiteral("from")] = parseStop(s.value(QStringLiteral("departure")).toObject());
            leg[QStringLiteral("to")] = parseStop(s.value(QStringLiteral("arrival")).toObject());
            legs.append(leg);
        }
        const QString dep = sections.first().toObject().value(QStringLiteral("departure")).toObject().value(QStringLiteral("time")).toString();
        const QString arr = sections.last().toObject().value(QStringLiteral("arrival")).toObject().value(QStringLiteral("time")).toString();
        QVariantMap it;
        it[QStringLiteral("mode")] = mode;
        it[QStringLiteral("duration")] = duration;
        it[QStringLiteral("baseDuration")] = base;
        it[QStringLiteral("trafficDelay")] = mode == QLatin1String("car") ? qMax(0, duration - base) : 0;
        it[QStringLiteral("length")] = length;
        it[QStringLiteral("departureTime")] = dep;
        it[QStringLiteral("arrivalTime")] = arr;
        it[QStringLiteral("departureText")] = timeText(dep);
        it[QStringLiteral("arrivalText")] = timeText(arr);
        it[QStringLiteral("transfers")] = 0;
        it[QStringLiteral("walkDuration")] = mode == QLatin1String("pedestrian") ? duration : 0;
        it[QStringLiteral("coordinates")] = coords;
        it[QStringLiteral("legs")] = legs;
        it[QStringLiteral("actions")] = actions;
        it[QStringLiteral("traffic")] = traffic;
        it[QStringLiteral("speedLimits")] = speedLimits;
        // Countries with toll roads on this route, and per country the
        // segments to avoid to get around them.
        QVariantMap tollMap;
        for (auto t = tollSegs.constBegin(); t != tollSegs.constEnd(); ++t)
            tollMap[t.key()] = t.value();
        it[QStringLiteral("tollCountries")] = tollOrder;
        it[QStringLiteral("tollSegments")] = tollMap;
        it[QStringLiteral("attributions")] = QVariantList();
        out.append(it);
    }
    return out;
}

QVariantList ItineraryParser::parseTransit(const QJsonObject &root)
{
    QVariantList out;
    for (const auto &rv : root.value(QStringLiteral("routes")).toArray()) {
        const QJsonArray sections = rv.toObject().value(QStringLiteral("sections")).toArray();
        if (sections.isEmpty())
            continue;
        QVariantList legs, coords, attributions;
        QSet<QString> seenAttr;
        int transitLegs = 0, walk = 0, length = 0, sumDur = 0;
        for (const auto &sv : sections) {
            const QJsonObject s = sv.toObject();
            const QJsonObject tr = s.value(QStringLiteral("transport")).toObject();
            const QJsonObject ts = s.value(QStringLiteral("travelSummary")).toObject();
            const QVariantMap from = parseStop(s.value(QStringLiteral("departure")).toObject());
            const QVariantMap to = parseStop(s.value(QStringLiteral("arrival")).toObject());
            int d = ts.value(QStringLiteral("duration")).toInt();
            if (d <= 0) {
                const qint64 secs = secondsBetween(from.value(QStringLiteral("time")).toString(), to.value(QStringLiteral("time")).toString());
                d = secs > 0 ? int(secs) : 0;
            }
            const QString type = s.value(QStringLiteral("type")).toString();
            QVariantMap leg;
            leg[QStringLiteral("type")] = type;
            leg[QStringLiteral("transportMode")] = tr.value(QStringLiteral("mode")).toString();
            leg[QStringLiteral("lineName")] = tr.value(QStringLiteral("name")).toString();
            leg[QStringLiteral("headsign")] = tr.value(QStringLiteral("headsign")).toString();
            leg[QStringLiteral("lineColor")] = tr.value(QStringLiteral("color")).toString();
            leg[QStringLiteral("lineTextColor")] = tr.value(QStringLiteral("textColor")).toString();
            leg[QStringLiteral("agency")] = s.value(QStringLiteral("agency")).toObject().value(QStringLiteral("name")).toString();
            leg[QStringLiteral("from")] = from;
            leg[QStringLiteral("to")] = to;
            leg[QStringLiteral("duration")] = d;
            leg[QStringLiteral("length")] = ts.value(QStringLiteral("length")).toInt();
            const QVariantList c = decodeCoords(s.value(QStringLiteral("polyline")).toString());
            leg[QStringLiteral("coordinates")] = c;
            coords.append(c);
            QVariantList stops;
            for (const auto &iv : s.value(QStringLiteral("intermediateStops")).toArray())
                stops.append(parseStop(iv.toObject().value(QStringLiteral("departure")).toObject()));
            leg[QStringLiteral("stops")] = stops;
            leg[QStringLiteral("actions")] = parseActions(s.value(QStringLiteral("actions")).toArray());
            legs.append(leg);

            if (type == QLatin1String("transit")) ++transitLegs;
            else if (type == QLatin1String("pedestrian")) walk += d;
            length += ts.value(QStringLiteral("length")).toInt();
            sumDur += d;

            for (const auto &av : s.value(QStringLiteral("attributions")).toArray()) {
                const QJsonObject a = av.toObject();
                const QString text = a.value(QStringLiteral("text")).toString();
                if (text.isEmpty() || seenAttr.contains(text))
                    continue;
                seenAttr.insert(text);
                QVariantMap am;
                am[QStringLiteral("text")] = text;
                am[QStringLiteral("href")] = a.value(QStringLiteral("href")).toString();
                am[QStringLiteral("type")] = a.value(QStringLiteral("type")).toString();
                attributions.append(am);
            }
        }
        const QVariantMap firstFrom = legs.first().toMap().value(QStringLiteral("from")).toMap();
        const QVariantMap lastTo = legs.last().toMap().value(QStringLiteral("to")).toMap();
        const QString dep = firstFrom.value(QStringLiteral("time")).toString();
        const QString arr = lastTo.value(QStringLiteral("time")).toString();
        const qint64 total = secondsBetween(dep, arr);

        QVariantMap it;
        it[QStringLiteral("mode")] = QStringLiteral("transit");
        it[QStringLiteral("duration")] = total > 0 ? int(total) : sumDur;
        it[QStringLiteral("baseDuration")] = it.value(QStringLiteral("duration"));
        it[QStringLiteral("trafficDelay")] = 0;
        it[QStringLiteral("length")] = length;
        it[QStringLiteral("departureTime")] = dep;
        it[QStringLiteral("arrivalTime")] = arr;
        it[QStringLiteral("departureText")] = timeText(dep);
        it[QStringLiteral("arrivalText")] = timeText(arr);
        it[QStringLiteral("transfers")] = qMax(0, transitLegs - 1);
        it[QStringLiteral("walkDuration")] = walk;
        it[QStringLiteral("coordinates")] = coords;
        it[QStringLiteral("legs")] = legs;
        it[QStringLiteral("actions")] = QVariantList();
        it[QStringLiteral("attributions")] = attributions;
        out.append(it);
    }
    return out;
}

QVariantList ItineraryParser::parsePlaces(const QJsonObject &root)
{
    QVariantList out;
    for (const auto &v : root.value(QStringLiteral("items")).toArray()) {
        const QJsonObject o = v.toObject();
        QJsonObject pos = o.value(QStringLiteral("position")).toObject();
        if (pos.isEmpty())
            continue;   // query/category suggestions without a location
        const QJsonArray access = o.value(QStringLiteral("access")).toArray();
        const QJsonObject nav = access.isEmpty() ? pos : access.first().toObject();
        const QJsonObject addr = o.value(QStringLiteral("address")).toObject();
        const QJsonArray cats = o.value(QStringLiteral("categories")).toArray();
        QVariantMap m;
        m[QStringLiteral("id")] = o.value(QStringLiteral("id")).toString();
        m[QStringLiteral("title")] = o.value(QStringLiteral("title")).toString();
        QString label = addr.value(QStringLiteral("label")).toString();
        if (label.startsWith(m.value(QStringLiteral("title")).toString() + QStringLiteral(", ")))
            label = label.mid(m.value(QStringLiteral("title")).toString().size() + 2);
        m[QStringLiteral("address")] = label;
        m[QStringLiteral("resultType")] = o.value(QStringLiteral("resultType")).toString();
        m[QStringLiteral("category")] = cats.isEmpty() ? QString() : cats.first().toObject().value(QStringLiteral("name")).toString();
        m[QStringLiteral("distance")] = o.contains(QStringLiteral("distance")) ? o.value(QStringLiteral("distance")).toInt() : -1;
        m[QStringLiteral("lat")] = pos.value(QStringLiteral("lat")).toDouble();
        m[QStringLiteral("lng")] = pos.value(QStringLiteral("lng")).toDouble();
        m[QStringLiteral("navLat")] = nav.value(QStringLiteral("lat")).toDouble();
        m[QStringLiteral("navLng")] = nav.value(QStringLiteral("lng")).toDouble();
        out.append(m);
    }
    return out;
}

QString ItineraryParser::errorMessage(const QJsonObject &root)
{
    QString msg = root.value(QStringLiteral("title")).toString();
    const QString cause = root.value(QStringLiteral("cause")).toString();
    if (!cause.isEmpty())
        msg += (msg.isEmpty() ? QString() : QStringLiteral(": ")) + cause;
    if (msg.isEmpty())
        msg = root.value(QStringLiteral("error_description")).toString();
    if (msg.isEmpty())
        msg = root.value(QStringLiteral("error")).toString();   // Valhalla
    return msg;
}
