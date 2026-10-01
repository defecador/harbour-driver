#include "ItineraryParser.h"
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

}

QString ItineraryParser::timeText(const QString &iso)
{
    static const QRegularExpression re(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}T(\\d{2}:\\d{2})"));
    const auto m = re.match(iso);
    return m.hasMatch() ? m.captured(1) : QString();
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
        QVariantList coords, legs, actions;
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
    return msg;
}
