// Transit guidance: a leg-by-leg state machine (walk -> wait -> ride ->
// alight -> next leg / arrived), built as extra methods on the same
// Navigator class so QML keeps a single `Navigator` object regardless of
// mode. Kept in its own file purely to keep Navigator.cpp from growing
// unwieldy.
#include "Navigator.h"

#include "AppSettings.h"
#include "HereClient.h"
#include "HereUrls.h"
#include "ItineraryParser.h"
#include "NavGeo.h"

#include <QDateTime>
#include <QGeoCoordinate>
#include <QJsonObject>

namespace {
// Boarding/alighting stop proximity, in metres.
constexpr qreal kStopRadius = 40.0;
// A stop counts as "passed" once GPS progress is this far past it, so a
// fix taken exactly at the platform doesn't flicker the count.
constexpr qreal kStopPassedSlack = 30.0;
// Enter "alight" (show "Get off at the next stop") this far from the end.
constexpr qreal kAlightRadius = 300.0;
constexpr qint64 kAlightNowMs = 60000;
// Missed-connection: still at the stop this long after the (delayed)
// departure time before asking "Missed U2? Find next connection".
constexpr qint64 kMissedConnectionGraceMs = 90000;
// Refresh a transit leg's live delay once departure is this close.
constexpr qint64 kDelayRefreshWindowMs = 5 * 60 * 1000;
constexpr qreal kMissedConnectionRadius = 150.0;
}

Navigator::TransitLeg Navigator::parseTransitLeg(const QVariantMap &legMap) const
{
    TransitLeg leg;
    leg.type = legMap.value(QStringLiteral("type")).toString();
    leg.transportMode = legMap.value(QStringLiteral("transportMode")).toString();
    leg.lineName = legMap.value(QStringLiteral("lineName")).toString();
    leg.headsign = legMap.value(QStringLiteral("headsign")).toString();
    leg.lineColor = legMap.value(QStringLiteral("lineColor")).toString();
    leg.lineTextColor = legMap.value(QStringLiteral("lineTextColor")).toString();
    leg.agency = legMap.value(QStringLiteral("agency")).toString();

    const QVariantMap from = legMap.value(QStringLiteral("from")).toMap();
    const QVariantMap to = legMap.value(QStringLiteral("to")).toMap();
    leg.fromName = from.value(QStringLiteral("name")).toString();
    leg.toName = to.value(QStringLiteral("name")).toString();
    leg.fromExpectedMs = from.value(QStringLiteral("expectedMs")).toLongLong();
    leg.toExpectedMs = to.value(QStringLiteral("expectedMs")).toLongLong();
    leg.fromDelay = from.value(QStringLiteral("delay"), -1).toInt();
    leg.toDelay = to.value(QStringLiteral("delay"), -1).toInt();
    leg.fromPlatform = from.value(QStringLiteral("platform")).toString();
    leg.fromLat = from.value(QStringLiteral("lat")).toDouble();
    leg.fromLng = from.value(QStringLiteral("lng")).toDouble();
    leg.toLat = to.value(QStringLiteral("lat")).toDouble();
    leg.toLng = to.value(QStringLiteral("lng")).toDouble();

    const QVariantList coordList = legMap.value(QStringLiteral("coordinates")).toList();
    leg.coords.reserve(coordList.size());
    for (const QVariant &v : coordList) {
        const QGeoCoordinate c = v.value<QGeoCoordinate>();
        leg.coords.append(qMakePair(c.latitude(), c.longitude()));
    }
    leg.cumDist.resize(leg.coords.size());
    qreal acc = 0;
    for (int i = 0; i < leg.coords.size(); ++i) {
        if (i > 0)
            acc += NavGeo::metersBetween(leg.coords[i - 1].first, leg.coords[i - 1].second,
                                         leg.coords[i].first, leg.coords[i].second);
        leg.cumDist[i] = acc;
    }
    leg.totalLength = leg.cumDist.isEmpty() ? 0 : leg.cumDist.last();

    if (leg.type == QStringLiteral("pedestrian")) {
        QVariantList actionList = legMap.value(QStringLiteral("actions")).toList();
        if (actionList.isEmpty()) {
            // Some walking legs (e.g. the sample fixtures) carry no actions
            // at all: synthesize a single "arrive" maneuver at the leg's end
            // so there's still something to match progress/arrival against.
            QVariantMap synth;
            synth[QStringLiteral("action")] = QStringLiteral("arrive");
            synth[QStringLiteral("instruction")] = leg.toName.isEmpty()
                    ? QObject::tr("Arrive") : QObject::tr("Arrive at %1").arg(leg.toName);
            synth[QStringLiteral("duration")] = 0;
            synth[QStringLiteral("offset")] = qMax(0, leg.coords.size() - 1);
            actionList.append(synth);
        }
        leg.maneuvers.reserve(actionList.size());
        for (const QVariant &v : actionList) {
            const QVariantMap a = v.toMap();
            Maneuver m;
            m.action = a.value(QStringLiteral("action")).toString();
            m.direction = a.value(QStringLiteral("direction")).toString();
            m.instruction = a.value(QStringLiteral("instruction")).toString();
            m.duration = a.value(QStringLiteral("duration")).toInt();
            int offset = a.value(QStringLiteral("offset")).toInt();
            if (!leg.cumDist.isEmpty()) {
                offset = qBound(0, offset, leg.cumDist.size() - 1);
                m.distanceAlong = leg.cumDist[offset];
            }
            leg.maneuvers.append(m);
        }
    } else {
        const QVariantList stopList = legMap.value(QStringLiteral("stops")).toList();
        leg.stops.reserve(stopList.size());
        for (const QVariant &v : stopList) {
            const QVariantMap s = v.toMap();
            TransitStop ts;
            ts.name = s.value(QStringLiteral("name")).toString();
            ts.lat = s.value(QStringLiteral("lat")).toDouble();
            ts.lng = s.value(QStringLiteral("lng")).toDouble();
            ts.expectedMs = s.value(QStringLiteral("expectedMs")).toLongLong();
            // Match each stop to the leg polyline once, so GPS-based counting
            // doesn't need to re-project every stop on every fix.
            qreal bestPerp = -1, bestDist = 0;
            for (int i = 0; i + 1 < leg.coords.size(); ++i) {
                const auto proj = NavGeo::projectOntoSegment(ts.lat, ts.lng,
                        leg.coords[i].first, leg.coords[i].second,
                        leg.coords[i + 1].first, leg.coords[i + 1].second);
                if (bestPerp < 0 || proj.perpDistance < bestPerp) {
                    bestPerp = proj.perpDistance;
                    bestDist = leg.cumDist[i] + proj.distanceFromStart;
                }
            }
            ts.distanceAlong = bestDist;
            leg.stops.append(ts);
        }
    }
    return leg;
}

void Navigator::rebuildTransitFromItinerary(const QVariantMap &itinerary)
{
    m_itinerary = itinerary;
    m_legs.clear();
    const QVariantList legList = itinerary.value(QStringLiteral("legs")).toList();
    m_legs.reserve(legList.size());
    for (const QVariant &v : legList)
        m_legs.append(parseTransitLeg(v.toMap()));

    m_legIndex = -1;
    m_offRoute = false;
    m_rerouting = false;
    m_rerouteCount = 0;
    if (!m_legs.isEmpty())
        enterLeg(0);
}

// Loads a pedestrian leg's own polyline/maneuvers into the generic
// m_coords/m_cumDist/m_maneuvers members so the existing single-route
// matching code (matchPositionToRoute, advanceStepPointer,
// updateDerivedProperties, checkAnnouncements) can drive it unchanged.
void Navigator::loadWalkLegIntoRouteState(const TransitLeg &leg)
{
    m_coords = leg.coords;
    m_cumDist = leg.cumDist;
    m_maneuvers = leg.maneuvers;
    m_totalLength = leg.totalLength;
    m_stepIndex = m_maneuvers.size() > 1 ? 1 : qMax(0, m_maneuvers.size() - 1);
    m_lastMatchPoint = 0;
    m_traveled = 0;
    m_distanceToManeuver = 0;
    m_hasThen = false;
    m_offRoute = false;
    m_offRouteSinceMs = -1;
    m_announcedFar.fill(false, m_maneuvers.size());
    m_announcedNear.fill(false, m_maneuvers.size());
    m_announcedNow.fill(false, m_maneuvers.size());
}

void Navigator::enterLeg(int index)
{
    m_legIndex = index;
    m_missedConnection = false;
    m_hurry = false;
    m_refreshedThisLeg = false;
    m_announcedAlight = false;
    m_announcedAlightNow = false;
    m_hasTransitFix = false;
    m_curLegDistanceAlong = 0;

    if (index < 0 || index >= m_legs.size()) {
        emitTransitArrived();
        return;
    }

    TransitLeg &leg = m_legs[index];
    if (leg.type == QStringLiteral("pedestrian")) {
        m_transitPhase = QStringLiteral("walk");
        loadWalkLegIntoRouteState(leg);
    } else {
        m_transitPhase = QStringLiteral("wait");
        for (TransitStop &s : leg.stops)
            s.passed = false;
        m_stopsRemaining = leg.stops.size() + 1;
    }
    emit maneuverChanged();
    emit transitChanged();
}

void Navigator::emitTransitArrived()
{
    m_state = QStringLiteral("arrived");
    m_rerouting = false;
    m_tickTimer.stop();
    emit stateChanged();
    emit announce(tr("You have arrived"), QStringLiteral("arrived"));
    emit arrived();
}

void Navigator::updateTransitPosition(double lat, double lng, double accuracyM,
                                      double speedMps, qint64 tsMs)
{
    Q_UNUSED(accuracyM)
    if (m_legIndex < 0 || m_legIndex >= m_legs.size())
        return;
    m_hasTransitFix = true;
    m_lastTransitLat = lat;
    m_lastTransitLng = lng;

    if (m_state == QStringLiteral("waitingForGps")) {
        m_state = QStringLiteral("guiding");
        emit stateChanged();
    }

    TransitLeg &leg = m_legs[m_legIndex];

    if (m_transitPhase == QStringLiteral("walk")) {
        const qreal perp = matchPositionToRoute(lat, lng);
        advanceStepPointer();
        updateDerivedProperties();
        m_curLegDistanceAlong = m_traveled;

        if (m_stepIndex == m_maneuvers.size() - 1 && m_distanceToManeuver <= kStopRadius) {
            // Reached the end of this walking leg (a stop, or the final
            // destination) — move on instead of signalling trip arrival.
            enterLeg(m_legIndex + 1);
            return;
        }
        checkAnnouncements(tsMs);
        checkOffRoute(perp, tsMs);
        if (m_offRoute && !m_rerouting && m_offRouteSinceMs >= 0 && tsMs - m_offRouteSinceMs >= 4000)
            maybeRerouteWalkLeg(lat, lng, tsMs);
        return;
    }

    if (m_transitPhase == QStringLiteral("wait")) {
        // Boarding is normally driven by the 1s QTimer tick (so it still
        // works with no fix at all, e.g. underground) — but a GPS fix that
        // already arrives at/after the scheduled departure can promote the
        // phase here too, rather than waiting for the next tick.
        if (!m_missedConnection && leg.fromExpectedMs > 0 && tsMs >= leg.fromExpectedMs) {
            m_transitPhase = QStringLiteral("ride");
            emit announce(tr("Boarded %1").arg(leg.lineName), QStringLiteral("boarded"));
            emit transitChanged();
        }
        return;
    }

    if (m_transitPhase == QStringLiteral("ride") || m_transitPhase == QStringLiteral("alight")) {
        qreal bestPerp = -1, bestDist = m_curLegDistanceAlong;
        for (int i = 0; i + 1 < leg.coords.size(); ++i) {
            const auto proj = NavGeo::projectOntoSegment(lat, lng,
                    leg.coords[i].first, leg.coords[i].second,
                    leg.coords[i + 1].first, leg.coords[i + 1].second);
            if (bestPerp < 0 || proj.perpDistance < bestPerp) {
                bestPerp = proj.perpDistance;
                bestDist = leg.cumDist[i] + proj.distanceFromStart;
            }
        }
        m_curLegDistanceAlong = qMax(m_curLegDistanceAlong, bestDist);

        for (TransitStop &s : leg.stops) {
            if (!s.passed && s.distanceAlong >= 0 && m_curLegDistanceAlong >= s.distanceAlong - kStopPassedSlack)
                s.passed = true;
        }
        int remaining = 1;
        for (const TransitStop &s : leg.stops)
            if (!s.passed)
                ++remaining;
        m_stopsRemaining = qMin(m_stopsRemaining, remaining);

        const qreal distToEnd = leg.totalLength - m_curLegDistanceAlong;
        const qint64 timeToEnd = leg.toExpectedMs > 0 ? leg.toExpectedMs - tsMs : -1;
        // Even a leg with no tracked intermediate stops (stopsRemaining == 1
        // from the moment it's boarded) must not jump straight to "alight":
        // that still needs to actually be near the end, by distance or time.
        const bool nearEnd = distToEnd <= kAlightRadius || (timeToEnd >= 0 && timeToEnd <= kAlightNowMs * 2);
        if (m_transitPhase == QStringLiteral("ride") && m_stopsRemaining <= 1 && nearEnd) {
            m_transitPhase = QStringLiteral("alight");
            if (!m_announcedAlight) {
                m_announcedAlight = true;
                emit announce(tr("Get off at the next stop"), QStringLiteral("alight"));
            }
        }
        if (m_transitPhase == QStringLiteral("alight") && !m_announcedAlightNow
                && (distToEnd <= 60.0 || (timeToEnd >= 0 && timeToEnd <= kAlightNowMs))) {
            m_announcedAlightNow = true;
            emit announce(tr("Get off now"), QStringLiteral("alightNow"));
        }
        // GPS reaching the alighting stop completes the leg outright, the
        // same way a walking leg's arrival does — this is what lets the
        // trip advance promptly above ground; updateTransitTick's own
        // timetable-based completion is what keeps it moving underground
        // when no fix like this ever arrives.
        if (m_transitPhase == QStringLiteral("alight") && distToEnd <= kStopPassedSlack) {
            enterLeg(m_legIndex + 1);
            return;
        }
        emit transitChanged();
    }
}

void Navigator::updateTransitTick(qint64 tsMs)
{
    if (m_legIndex < 0 || m_legIndex >= m_legs.size())
        return;
    TransitLeg &leg = m_legs[m_legIndex];

    if (m_transitPhase == QStringLiteral("wait")) {
        checkMissedConnection(tsMs);
        if (!m_missedConnection)
            maybeRefreshDelay(m_lastTransitLat, m_lastTransitLng, tsMs);
        if (!m_missedConnection && leg.fromExpectedMs > 0 && tsMs >= leg.fromExpectedMs) {
            m_transitPhase = QStringLiteral("ride");
            emit announce(tr("Boarded %1").arg(leg.lineName), QStringLiteral("boarded"));
        }
        // Hurry hint: only meaningful while still walking towards this stop,
        // handled in the walk phase of the previous leg — nothing to do here.
    } else if (m_transitPhase == QStringLiteral("ride") || m_transitPhase == QStringLiteral("alight")) {
        // Timetable fallback for stop counting: a stop counts as passed once
        // its scheduled (delay-adjusted) time has passed, even with no GPS
        // fix at all — this is what keeps counting working underground.
        for (TransitStop &s : leg.stops) {
            if (!s.passed && s.expectedMs > 0 && tsMs >= s.expectedMs)
                s.passed = true;
        }
        int remaining = 1;
        for (const TransitStop &s : leg.stops)
            if (!s.passed)
                ++remaining;
        m_stopsRemaining = qMin(m_stopsRemaining, remaining);
        // Timetable-only "nearEnd": no GPS distance to go on, so use time to
        // the scheduled arrival instead (same window as the GPS path uses).
        const qint64 timeToEnd = leg.toExpectedMs > 0 ? leg.toExpectedMs - tsMs : -1;
        const bool nearEnd = timeToEnd >= 0 && timeToEnd <= kAlightNowMs * 2;
        if (m_stopsRemaining <= 1 && nearEnd && m_transitPhase == QStringLiteral("ride")) {
            m_transitPhase = QStringLiteral("alight");
            if (!m_announcedAlight) {
                m_announcedAlight = true;
                emit announce(tr("Get off at the next stop"), QStringLiteral("alight"));
            }
        }
        if (leg.toExpectedMs > 0 && tsMs >= leg.toExpectedMs) {
            enterLeg(m_legIndex + 1);
            return;
        }
    } else if (m_transitPhase == QStringLiteral("walk")) {
        // Hurry: only relevant when the *next* leg is a ride we could miss.
        m_hurry = false;
        if (m_legIndex + 1 < m_legs.size()) {
            const TransitLeg &nextLeg = m_legs[m_legIndex + 1];
            if (nextLeg.type != QStringLiteral("pedestrian") && nextLeg.fromExpectedMs > 0) {
                const qint64 secsToDeparture = (nextLeg.fromExpectedMs - tsMs) / 1000;
                m_hurry = m_remainingSeconds > secsToDeparture;
            }
        }
    }
    emit transitChanged();
}

void Navigator::checkMissedConnection(qint64 tsMs)
{
    if (m_legIndex < 0 || m_legIndex >= m_legs.size())
        return;
    const TransitLeg &leg = m_legs[m_legIndex];
    if (leg.fromExpectedMs <= 0 || m_missedConnection)
        return;
    const qint64 delayMs = leg.fromDelay > 0 ? qint64(leg.fromDelay) * 1000 : 0;
    if (tsMs < leg.fromExpectedMs + delayMs + kMissedConnectionGraceMs)
        return;
    // Without a fix we can't disprove still being at the stop, so err on the
    // side of asking rather than silently replanning (the user's choice:
    // "ask first", never spend a HERE call without a tap).
    const bool stillNear = !m_hasTransitFix
            || NavGeo::metersBetween(m_lastTransitLat, m_lastTransitLng, leg.fromLat, leg.fromLng) <= kMissedConnectionRadius;
    if (stillNear) {
        m_missedConnection = true;
        emit announce(tr("Missed %1? Find the next connection.").arg(leg.lineName), QStringLiteral("missed"));
    }
}

void Navigator::maybeRefreshDelay(double lat, double lng, qint64 tsMs)
{
    if (m_legIndex < 0 || m_legIndex >= m_legs.size() || m_refreshedThisLeg)
        return;
    TransitLeg &leg = m_legs[m_legIndex];
    if (leg.fromExpectedMs <= 0 || tsMs < leg.fromExpectedMs - kDelayRefreshWindowMs)
        return;
    m_refreshedThisLeg = true;
    if (!m_client || !m_settings)
        return;
    if (!m_client->fixtureMode() && !m_settings->hasHereKey())
        return;

    const double fromLat = (lat != 0 || lng != 0) ? lat : leg.fromLat;
    const double fromLng = (lat != 0 || lng != 0) ? lng : leg.fromLng;
    const HereUrls::LatLng from{fromLat, fromLng};
    const HereUrls::LatLng to{
        m_destination.value(QStringLiteral("navLat"), m_destination.value(QStringLiteral("lat"))).toDouble(),
        m_destination.value(QStringLiteral("navLng"), m_destination.value(QStringLiteral("lng"))).toDouble()
    };
    const int gen = ++m_transitRefreshGeneration;
    const QUrl url = HereUrls::transit(m_settings->hereApiKey(), from, to, m_settings->lang(), m_settings->units(), 0);
    m_client->get(url, QStringLiteral("transit_refresh"), [this, gen](bool ok, const QJsonObject &json, const QString &) {
        if (gen != m_transitRefreshGeneration)
            return;
        const QVariantList items = ok ? ItineraryParser::parseTransit(json) : QVariantList();
        applyDelayRefresh(ok && !items.isEmpty(), items);
    });
}

void Navigator::applyDelayRefresh(bool ok, const QVariantList &items)
{
    if (!ok || m_legIndex < 0 || m_legIndex >= m_legs.size())
        return;
    TransitLeg &leg = m_legs[m_legIndex];
    const QVariantMap newItin = items.first().toMap();
    for (const QVariant &lv : newItin.value(QStringLiteral("legs")).toList()) {
        const QVariantMap l = lv.toMap();
        if (l.value(QStringLiteral("lineName")).toString() != leg.lineName)
            continue;
        const QVariantMap from = l.value(QStringLiteral("from")).toMap();
        if (from.value(QStringLiteral("name")).toString() != leg.fromName)
            continue;
        const QVariantMap to = l.value(QStringLiteral("to")).toMap();
        leg.fromExpectedMs = from.value(QStringLiteral("expectedMs")).toLongLong();
        leg.fromDelay = from.value(QStringLiteral("delay"), -1).toInt();
        leg.toExpectedMs = to.value(QStringLiteral("expectedMs")).toLongLong();
        leg.toDelay = to.value(QStringLiteral("delay"), -1).toInt();
        emit transitChanged();
        return;
    }
    // No matching leg in the fresh plan: keep what we had rather than guess.
}

// Off-route handling for a transit walking leg: a plain pedestrian reroute
// to this leg's own end stop, independent of the car/bike maybeReroute()
// (which routes to the trip's overall destination) but sharing its
// cooldown/per-trip counters so a flaky fix can't burn through the quota
// any faster than car navigation can.
void Navigator::maybeRerouteWalkLeg(double lat, double lng, qint64 tsMs)
{
    if (m_rerouteCount >= 15)
        return;
    if (m_lastRerouteMs >= 0 && tsMs - m_lastRerouteMs < 10000)
        return;
    if (m_legIndex < 0 || m_legIndex >= m_legs.size())
        return;
    m_lastRerouteMs = tsMs;
    ++m_rerouteCount;
    m_rerouting = true;
    emit stateChanged();
    emit announce(tr("Recalculating route…"), QStringLiteral("rerouting"));

    if (m_requester)
        m_requester(lat, lng, -1);

    if (!m_client || !m_settings)
        return;
    if (!m_client->fixtureMode() && !m_settings->hasHereKey()) {
        m_rerouting = false;
        emit stateChanged();
        return;
    }

    const TransitLeg &leg = m_legs[m_legIndex];
    const int gen = ++m_rerouteGeneration;
    const int legIndex = m_legIndex;
    const HereUrls::LatLng from{lat, lng};
    const HereUrls::LatLng to{leg.toLat, leg.toLng};
    const QUrl url = HereUrls::route(m_settings->hereApiKey(), QStringLiteral("pedestrian"), from, to,
                                     m_settings->lang(), m_settings->units(), 0);
    m_client->get(url, QStringLiteral("route_pedestrian"), [this, gen, legIndex](bool ok, const QJsonObject &json, const QString &) {
        if (gen != m_rerouteGeneration || legIndex != m_legIndex)
            return;
        m_rerouting = false;
        const QVariantList items = ok ? ItineraryParser::parseRoutes(json, QStringLiteral("pedestrian")) : QVariantList();
        if (ok && !items.isEmpty()) {
            const TransitLeg newLeg = parseTransitLeg(items.first().toMap());
            m_legs[legIndex].coords = newLeg.coords;
            m_legs[legIndex].cumDist = newLeg.cumDist;
            m_legs[legIndex].maneuvers = newLeg.maneuvers;
            m_legs[legIndex].totalLength = newLeg.totalLength;
            loadWalkLegIntoRouteState(m_legs[legIndex]);
            emit announce(tr("Route updated"), QStringLiteral("rerouted"));
            emit maneuverChanged();
        }
        emit stateChanged();
    });
}

void Navigator::replanTransit()
{
    if (m_state == QStringLiteral("idle") || m_state == QStringLiteral("error"))
        return;
    if (m_rerouteCount >= 15)
        return;
    ++m_rerouteCount;
    m_rerouting = true;
    emit stateChanged();

    if (m_requester)
        m_requester(m_lastTransitLat, m_lastTransitLng, -1);

    if (!m_client || !m_settings)
        return;
    if (!m_client->fixtureMode() && !m_settings->hasHereKey()) {
        m_rerouting = false;
        emit stateChanged();
        return;
    }

    const int gen = ++m_rerouteGeneration;
    const HereUrls::LatLng from{m_lastTransitLat, m_lastTransitLng};
    const HereUrls::LatLng to{
        m_destination.value(QStringLiteral("navLat"), m_destination.value(QStringLiteral("lat"))).toDouble(),
        m_destination.value(QStringLiteral("navLng"), m_destination.value(QStringLiteral("lng"))).toDouble()
    };
    const QUrl url = HereUrls::transit(m_settings->hereApiKey(), from, to, m_settings->lang(), m_settings->units(), 0);
    m_client->get(url, QStringLiteral("transit"), [this, gen](bool ok, const QJsonObject &json, const QString &) {
        if (gen != m_rerouteGeneration)
            return;
        m_rerouting = false;
        const QVariantList items = ok ? ItineraryParser::parseTransit(json) : QVariantList();
        if (ok && !items.isEmpty()) {
            const QVariantMap newItinerary = items.first().toMap();
            rebuildTransitFromItinerary(newItinerary);
            m_state = QStringLiteral("guiding");
            m_tickTimer.start();
            emit announce(tr("Route updated"), QStringLiteral("rerouted"));
            emit rerouted(newItinerary);
            emit maneuverChanged();
        }
        emit stateChanged();
    });
}

QString Navigator::transportMode() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].transportMode : QString();
}
QString Navigator::lineName() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].lineName : QString();
}
QString Navigator::headsign() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].headsign : QString();
}
QString Navigator::lineColor() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].lineColor : QString();
}
QString Navigator::lineTextColor() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].lineTextColor : QString();
}
QString Navigator::boardStopName() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].fromName : QString();
}
QString Navigator::alightStopName() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].toName : QString();
}
QString Navigator::platform() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].fromPlatform : QString();
}
QString Navigator::departureText() const
{
    if (m_legIndex < 0 || m_legIndex >= m_legs.size() || m_legs[m_legIndex].fromExpectedMs <= 0)
        return QString();
    return QDateTime::fromMSecsSinceEpoch(m_legs[m_legIndex].fromExpectedMs).toString(QStringLiteral("HH:mm"));
}
int Navigator::departureDelay() const
{
    return (m_legIndex >= 0 && m_legIndex < m_legs.size()) ? m_legs[m_legIndex].fromDelay : -1;
}
int Navigator::secondsToDeparture() const
{
    if (m_legIndex < 0 || m_legIndex >= m_legs.size() || m_legs[m_legIndex].fromExpectedMs <= 0)
        return 0;
    return qMax(qint64(0), (m_legs[m_legIndex].fromExpectedMs - now()) / 1000);
}
QString Navigator::nextStopName() const
{
    if (m_legIndex < 0 || m_legIndex >= m_legs.size())
        return QString();
    const TransitLeg &leg = m_legs[m_legIndex];
    for (const TransitStop &s : leg.stops)
        if (!s.passed)
            return s.name;
    return leg.toName;
}
