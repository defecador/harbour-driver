#include "HereUrls.h"

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
                     const QString &lang, const QString &units, int alternatives, int courseDegrees)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("transportMode"), mode);
    // courseDegrees >= 0 means "reroute while moving": tells HERE which direction
    // we're already travelling in, so the new route starts ahead of us rather
    // than behind, e.g. sending us straight into a U-turn.
    q.addQueryItem(QStringLiteral("origin"), courseDegrees >= 0
                   ? coord(from) + QStringLiteral(";course=") + QString::number(courseDegrees)
                   : coord(from));
    q.addQueryItem(QStringLiteral("destination"), coord(to));
    q.addQueryItem(QStringLiteral("return"), QStringLiteral("polyline,summary,actions,instructions,travelSummary"));
    if (alternatives > 0)
        q.addQueryItem(QStringLiteral("alternatives"), QString::number(alternatives));
    if (mode != QLatin1String("car"))
        q.addQueryItem(QStringLiteral("departureTime"), QStringLiteral("any"));
    q.addQueryItem(QStringLiteral("lang"), lang);
    q.addQueryItem(QStringLiteral("units"), units);
    return make(QStringLiteral("https://router.hereapi.com/v8/routes"), q, key);
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

QString HereUrls::redact(const QString &text)
{
    QString out = text;
    out.replace(QRegularExpression(QStringLiteral("apiKey=[^&\\s\"]*")), QStringLiteral("apiKey=***"));
    return out;
}
