#include "HereUrls.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QRegularExpression>
#include <QUrlQuery>

namespace {

QString coord(HereUrls::LatLng p)
{
    return QString::number(p.lat, 'f', 6) + QLatin1Char(',') + QString::number(p.lng, 'f', 6);
}

QUrl make(const QString &base, QUrlQuery q, const QString &key)
{
    q.addQueryItem(QStringLiteral("apiKey"), key);
    QUrl url(base);
    url.setQuery(q);
    return url;
}

}

QUrl HereUrls::autosuggest(const QString &key, const QString &text, LatLng at, const QString &lang, int limit)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), text);
    q.addQueryItem(QStringLiteral("at"), coord(at));
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    q.addQueryItem(QStringLiteral("lang"), lang);
    return make(QStringLiteral("https://autosuggest.search.hereapi.com/v1/autosuggest"), q, key);
}

QUrl HereUrls::discover(const QString &key, const QString &text, LatLng at, const QString &lang, int limit)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), text);
    q.addQueryItem(QStringLiteral("at"), coord(at));
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    q.addQueryItem(QStringLiteral("lang"), lang);
    return make(QStringLiteral("https://discover.search.hereapi.com/v1/discover"), q, key);
}

QUrl HereUrls::revgeocode(const QString &key, LatLng at, const QString &lang)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("at"), coord(at));
    q.addQueryItem(QStringLiteral("lang"), lang);
    return make(QStringLiteral("https://revgeocode.search.hereapi.com/v1/revgeocode"), q, key);
}

QUrl HereUrls::route(const QString &key, const QString &mode, LatLng from, LatLng to,
                     const QString &lang, const QString &units, int alternatives, int courseDegrees,
                     const QVector<LatLng> &vias, const QString &routingMode, bool withTolls)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("transportMode"), mode);
    // courseDegrees >= 0 means "reroute while moving": tells HERE which direction
    // we're already travelling in, so the new route starts ahead of us rather
    // than behind, e.g. sending us straight into a U-turn.
    q.addQueryItem(QStringLiteral("origin"), courseDegrees >= 0
                   ? coord(from) + QStringLiteral(";course=") + QString::number(courseDegrees)
                   : coord(from));
    // passThrough: the route must go through the point but not stop there, so
    // it takes the shortest way past it instead of detouring to "arrive".
    for (const LatLng &v : vias)
        q.addQueryItem(QStringLiteral("via"), coord(v) + QStringLiteral("!passThrough=true"));
    if (!routingMode.isEmpty())
        q.addQueryItem(QStringLiteral("routingMode"), routingMode);
    q.addQueryItem(QStringLiteral("destination"), coord(to));
    const bool car = mode == QLatin1String("car");
    q.addQueryItem(QStringLiteral("return"), QStringLiteral("polyline,summary,actions,instructions,travelSummary")
                   + (car && withTolls ? QStringLiteral(",tolls") : QString()));
    if (alternatives > 0)
        q.addQueryItem(QStringLiteral("alternatives"), QString::number(alternatives));
    // Per-span traffic speed, so the route line can show slow stretches.
    if (mode == QLatin1String("car"))
        q.addQueryItem(QStringLiteral("spans"), QStringLiteral("dynamicSpeedInfo,speedLimit")
                       + (withTolls ? QStringLiteral(",countryCode,tollSystems,segmentRef") : QString()));
    if (mode != QLatin1String("car"))
        q.addQueryItem(QStringLiteral("departureTime"), QStringLiteral("any"));
    q.addQueryItem(QStringLiteral("lang"), lang);
    q.addQueryItem(QStringLiteral("units"), units);
    return make(QStringLiteral("https://router.hereapi.com/v8/routes"), q, key);
}

QUrl HereUrls::valhallaBicycle(LatLng from, LatLng to, const QVector<LatLng> &vias, const QString &lang, const QString &units)
{
    auto loc = [](LatLng p, const char *type) {
        QJsonObject o;
        o[QStringLiteral("lat")] = p.lat;
        o[QStringLiteral("lon")] = p.lng;
        o[QStringLiteral("type")] = QString::fromLatin1(type);
        return o;
    };
    QJsonArray locations{loc(from, "break")};
    for (const LatLng &v : vias)
        locations.append(loc(v, "through"));
    locations.append(loc(to, "break"));

    // use_roads low = prefer cycle paths and quiet streets over busy roads.
    QJsonObject bike;
    bike[QStringLiteral("bicycle_type")] = QStringLiteral("Hybrid");
    bike[QStringLiteral("use_roads")] = 0.0;
    bike[QStringLiteral("use_hills")] = 0.4;
    bike[QStringLiteral("avoid_bad_surfaces")] = 0.5;
    QJsonObject costing;
    costing[QStringLiteral("bicycle")] = bike;
    QJsonObject dir;
    dir[QStringLiteral("units")] = units == QLatin1String("imperial") ? QStringLiteral("miles") : QStringLiteral("kilometers");
    dir[QStringLiteral("language")] = lang;

    QJsonObject req;
    req[QStringLiteral("locations")] = locations;
    req[QStringLiteral("costing")] = QStringLiteral("bicycle");
    req[QStringLiteral("costing_options")] = costing;
    req[QStringLiteral("directions_options")] = dir;

    QUrl url(QStringLiteral("https://valhalla1.openstreetmap.de/route"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("json"), QString::fromUtf8(QJsonDocument(req).toJson(QJsonDocument::Compact)));
    url.setQuery(q);
    return url;
}

QUrl HereUrls::transit(const QString &key, LatLng from, LatLng to, const QString &lang,
                       const QString &units, int alternatives)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("origin"), coord(from));
    q.addQueryItem(QStringLiteral("destination"), coord(to));
    // "actions" costs nothing extra here and gives walking legs turn-by-turn
    // instructions, which transit guidance needs for the walk-to/from-stop parts.
    q.addQueryItem(QStringLiteral("return"), QStringLiteral("polyline,travelSummary,intermediate,actions"));
    q.addQueryItem(QStringLiteral("alternatives"), QString::number(alternatives));
    q.addQueryItem(QStringLiteral("lang"), lang);
    q.addQueryItem(QStringLiteral("units"), units);
    return make(QStringLiteral("https://transit.router.hereapi.com/v8/routes"), q, key);
}

QString HereUrls::baseTiles(const QString &key, const QString &style, const QString &lang)
{
    // lang is intentionally omitted: HERE renders each tile in its country's
    // default language when unset, matching Pure Maps' proven-working config,
    // and it avoids depending on the tile API's actual supported-language list
    // (which is stricter than the lang query param's own format regex).
    Q_UNUSED(lang)
    const bool sat = style.contains(QLatin1String("satellite"));
    return QStringLiteral("https://maps.hereapi.com/v3/base/mc/{z}/{x}/{y}/%1?style=%2&size=512&ppi=400%3&apiKey=%4")
            .arg(sat ? QStringLiteral("jpeg") : QStringLiteral("png8"), style,
                 sat ? QString() : QStringLiteral("&features=pois:all"), key);
}

QString HereUrls::trafficTiles(const QString &key, bool night)
{
    return QStringLiteral("https://traffic.maps.hereapi.com/v3/flow/mc/{z}/{x}/{y}/png8?style=%1&size=512&ppi=400&apiKey=%2")
            .arg(night ? QStringLiteral("explore.night") : QStringLiteral("explore.day"), key);
}

QByteArray HereUrls::avoidSegmentsBody(const QStringList &segments)
{
    QJsonArray arr;
    for (const QString &s : segments)
        arr.append(s);
    QJsonObject avoid;
    avoid[QStringLiteral("segments")] = arr;
    QJsonObject root;
    root[QStringLiteral("avoid")] = avoid;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString HereUrls::redact(const QString &text)
{
    QString out = text;
    out.replace(QRegularExpression(QStringLiteral("apiKey=[^&\\s\"]*")), QStringLiteral("apiKey=***"));
    return out;
}
