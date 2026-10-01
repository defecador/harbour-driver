#ifndef ITINERARYPARSER_H
#define ITINERARYPARSER_H

#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>

// Converts HERE JSON responses into provider-neutral maps that QML consumes directly.
//
// Itinerary: { mode, duration, baseDuration, trafficDelay, length, departureTime, arrivalTime,
//              departureText, arrivalText, transfers, walkDuration, coordinates, legs[],
//              attributions[] }
// Leg:       { type, transportMode, lineName, headsign, lineColor, lineTextColor, agency,
//              from{name,time,timeText,timeMs,expectedMs,delay,platform,lat,lng}, to{...}, duration, length,
//              coordinates, stops[], actions[] }
namespace ItineraryParser {

QVariantList parseRoutes(const QJsonObject &root, const QString &mode);
QVariantList parseTransit(const QJsonObject &root);
QVariantList parsePlaces(const QJsonObject &root);   // autosuggest / discover / revgeocode
QString errorMessage(const QJsonObject &root);

QString timeText(const QString &iso);                // "09:47" in the place's own offset

}

#endif
