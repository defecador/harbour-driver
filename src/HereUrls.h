#ifndef HEREURLS_H
#define HEREURLS_H

#include <QString>
#include <QUrl>
#include <QVector>

// Pure URL builders for the HERE REST APIs, kept separate so they can be unit tested.
namespace HereUrls {

struct LatLng {
    double lat;
    double lng;
};

QUrl autosuggest(const QString &key, const QString &q, LatLng at, const QString &lang, int limit = 8);
QUrl discover(const QString &key, const QString &q, LatLng at, const QString &lang, int limit = 20);
QUrl revgeocode(const QString &key, LatLng at, const QString &lang);

// mode: car | pedestrian | bicycle. Car omits departureTime so live traffic is used;
// pedestrian/bicycle send departureTime=any to stay in the cheaper billing tier.
// courseDegrees: heading in degrees (0-359) to anchor the origin while moving, e.g.
// when rerouting off-route during navigation; -1 (default) omits it.
// vias: stopovers to pass through, in order, between from and to (walk/bike "add a stop").
QUrl route(const QString &key, const QString &mode, LatLng from, LatLng to,
           const QString &lang, const QString &units, int alternatives, int courseDegrees = -1,
           const QVector<LatLng> &vias = QVector<LatLng>(), const QString &routingMode = QString());

// Greenest-route alternative for cycling: the public Valhalla (OpenStreetMap)
// server, which can prefer bike paths and quiet streets (HERE cannot).
// vias are passed through without stopping. No key needed.
QUrl valhallaBicycle(LatLng from, LatLng to, const QVector<LatLng> &vias, const QString &lang, const QString &units);
QUrl transit(const QString &key, LatLng from, LatLng to, const QString &lang,
             const QString &units, int alternatives);

// Raster tile templates for MapLibre ({z}/{x}/{y} placeholders kept).
QString baseTiles(const QString &key, const QString &style, const QString &lang);
QString trafficTiles(const QString &key, bool night);

// Replaces any apiKey=... value so keys never reach logs.
QString redact(const QString &text);

}

#endif
