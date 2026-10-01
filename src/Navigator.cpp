#include "Navigator.h"

#include "AppSettings.h"
#include "HereClient.h"
#include "HereUrls.h"
#include "ItineraryParser.h"
#include "NavGeo.h"

#include <QDateTime>
#include <QGeoCoordinate>
#include <QJsonObject>
#include <QtGlobal>

namespace {
// Staying more than this far from the route for this long triggers a reroute.
constexpr qint64 kOffRouteDelayMs = 8000;
// Reroute attempts are throttled so a noisy GPS fix can't burn through the
// HERE routing quota: at most once every 30s, at most 15 times per trip.
constexpr qint64 kRerouteCooldownMs = 30000;
constexpr int kRerouteMaxCount = 15;
// Within this radius of the final maneuver, the trip counts as arrived.
constexpr qreal kArrivalRadius = 30.0;
// Within this radius of a user-added stop, the stop counts as reached.
constexpr qreal kStopReachedRadius = 30.0;
// Two maneuvers closer together than this both show on screen at once
// ("then turn right"), so you don't miss a quick turn right after another.
constexpr qreal kThenPreviewGap = 150.0;
}

Navigator::Navigator(AppSettings *settings, HereClient *client, QObject *parent)
    : QObject(parent), m_settings(settings), m_client(client)
{
    m_tickTimer.setInterval(1000);
    connect(&m_tickTimer, &QTimer::timeout, this, [this]() {
        if (m_mode == QStringLiteral("transit") && m_state == QStringLiteral("guiding"))
            updateTransitTick(now());
    });
}

void Navigator::setClockForTesting(std::function<qint64()> clock) { m_clock = std::move(clock); }
void Navigator::setRerouteRequesterForTesting(std::function<void(double, double, int)> fn) { m_requester = std::move(fn); }

void Navigator::setSimulationClock(bool on, qreal speedFactor)
{
    m_simClockOn = on;
    m_simSpeedFactor = speedFactor > 0 ? speedFactor : 1.0;
    m_simWallStartMs = QDateTime::currentMSecsSinceEpoch();
    m_simBaseMs = !m_legs.isEmpty() && m_legs.first().fromExpectedMs > 0
            ? m_legs.first().fromExpectedMs : m_simWallStartMs;
}

qint64 Navigator::now() const
{
    if (m_clock)
        return m_clock();
    if (m_simClockOn) {
        const qint64 wallElapsed = QDateTime::currentMSecsSinceEpoch() - m_simWallStartMs;
        return m_simBaseMs + qint64(wallElapsed * m_simSpeedFactor);
    }
    return QDateTime::currentMSecsSinceEpoch();
}

QString Navigator::maneuverAction() const
{
    return (m_stepIndex >= 0 && m_stepIndex < m_maneuvers.size()) ? m_maneuvers[m_stepIndex].action : QString();
}

QString Navigator::maneuverDirection() const
{
    return (m_stepIndex >= 0 && m_stepIndex < m_maneuvers.size()) ? m_maneuvers[m_stepIndex].direction : QString();
}

QString Navigator::instruction() const
{
    return (m_stepIndex >= 0 && m_stepIndex < m_maneuvers.size()) ? m_maneuvers[m_stepIndex].instruction : QString();
}

QString Navigator::thenAction() const
{
    const int i = m_stepIndex + 1;
    return (m_hasThen && i < m_maneuvers.size()) ? m_maneuvers[i].action : QString();
}

QString Navigator::thenDirection() const
{
    const int i = m_stepIndex + 1;
    return (m_hasThen && i < m_maneuvers.size()) ? m_maneuvers[i].direction : QString();
}

QString Navigator::arrivalText() const
{
    if (m_mode == QStringLiteral("transit")) {
        if (m_legs.isEmpty() || m_legs.last().toExpectedMs <= 0)
            return QString();
        return QDateTime::fromMSecsSinceEpoch(m_legs.last().toExpectedMs).toString(QStringLiteral("HH:mm"));
    }
    return QDateTime::currentDateTime().addSecs(m_remainingSeconds).toString(QStringLiteral("HH:mm"));
}

void Navigator::rebuildFromItinerary(const QVariantMap &itinerary)
{
    m_itinerary = itinerary;
    m_mode = itinerary.value(QStringLiteral("mode")).toString();
    m_variant = itinerary.value(QStringLiteral("variant")).toString();

    m_coords.clear();
    m_cumDist.clear();
    m_maneuvers.clear();

    const QVariantList coordList = itinerary.value(QStringLiteral("coordinates")).toList();
    m_coords.reserve(coordList.size());
    for (const QVariant &v : coordList) {
        const QGeoCoordinate c = v.value<QGeoCoordinate>();
        m_coords.append(qMakePair(c.latitude(), c.longitude()));
    }

    m_cumDist.resize(m_coords.size());
    qreal acc = 0;
    for (int i = 0; i < m_coords.size(); ++i) {
        if (i > 0)
            acc += NavGeo::metersBetween(m_coords[i - 1].first, m_coords[i - 1].second,
                                         m_coords[i].first, m_coords[i].second);
        m_cumDist[i] = acc;
    }
    m_totalLength = m_cumDist.isEmpty() ? 0 : m_cumDist.last();

    const QVariantList actionList = itinerary.value(QStringLiteral("actions")).toList();
    m_maneuvers.reserve(actionList.size());
    for (const QVariant &v : actionList) {
        const QVariantMap a = v.toMap();
        Maneuver m;
        m.action = a.value(QStringLiteral("action")).toString();
        m.direction = a.value(QStringLiteral("direction")).toString();
        m.instruction = a.value(QStringLiteral("instruction")).toString();
        m.duration = a.value(QStringLiteral("duration")).toInt();
        int offset = a.value(QStringLiteral("offset")).toInt();
        if (!m_cumDist.isEmpty()) {
            offset = qBound(0, offset, m_cumDist.size() - 1);
            m.distanceAlong = m_cumDist[offset];
        }
        m_maneuvers.append(m);
    }

    // A route through user stops has one "arrive" per stop before the final
    // one; remember where, and name them after the stop instead of HERE's
    // generic "arrive" text.
    m_stopManeuvers.clear();
    for (int i = 0; i + 1 < m_maneuvers.size() && m_stopManeuvers.size() < m_stops.size(); ++i) {
        if (m_maneuvers[i].action == QStringLiteral("arrive")) {
            const QString title = m_stops.at(m_stopManeuvers.size()).value(QStringLiteral("title")).toString();
            if (!title.isEmpty())
                m_maneuvers[i].instruction = tr("Stop: %1").arg(title);
            m_stopManeuvers.append(i);
        }
    }

    resetProgressState();
}

void Navigator::resetProgressState()
{
    m_stepIndex = m_maneuvers.size() > 1 ? 1 : qMax(0, m_maneuvers.size() - 1);
    m_lastMatchPoint = 0;
    m_traveled = 0;
    m_distanceToManeuver = 0;
    m_hasThen = false;
    m_remainingDistance = m_totalLength;
    m_remainingSeconds = 0;
    m_offRoute = false;
    m_offRouteSinceMs = -1;
    m_rerouting = false;
    m_announcedFar.fill(false, m_maneuvers.size());
    m_announcedNear.fill(false, m_maneuvers.size());
    m_announcedNow.fill(false, m_maneuvers.size());
}

void Navigator::start(const QVariantMap &itinerary, const QVariantMap &destination)
{
    m_destination = destination;
    if (!m_stops.isEmpty()) {
        m_stops.clear();
        emit stopsChanged();
    }
    m_stopManeuvers.clear();
    m_addingStop = false;
    ++m_rerouteGeneration;
    ++m_transitRefreshGeneration;
    m_rerouteCount = 0;
    m_lastRerouteMs = -1;
    m_mode = itinerary.value(QStringLiteral("mode")).toString();

    if (m_mode == QStringLiteral("transit")) {
        rebuildTransitFromItinerary(itinerary);
        m_state = m_legs.isEmpty() ? QStringLiteral("error") : QStringLiteral("waitingForGps");
        emit stateChanged();
        emit maneuverChanged();
        emit transitChanged();
        m_tickTimer.start();
        return;
    }

    rebuildFromItinerary(itinerary);
    m_state = (m_maneuvers.isEmpty() || m_coords.size() < 2)
            ? QStringLiteral("error") : QStringLiteral("waitingForGps");
    emit stateChanged();
    emit maneuverChanged();
}

void Navigator::stop()
{
    ++m_rerouteGeneration;
    ++m_transitRefreshGeneration;
    m_state = QStringLiteral("idle");
    m_rerouting = false;
    m_tickTimer.stop();
    emit stateChanged();
}

void Navigator::advanceStepPointer()
{
    while (m_stepIndex < m_maneuvers.size() - 1 && m_traveled >= m_maneuvers[m_stepIndex].distanceAlong)
        ++m_stepIndex;
}

void Navigator::checkStopsReached(double lat, double lng)
{
    // A stop is reached by passing its arrival maneuver, or just by walking
    // up to it (routes through "pass-through" stops have no such maneuver).
    while (!m_stops.isEmpty()
           && (m_stopManeuvers.isEmpty() || m_stepIndex <= m_stopManeuvers.first())
           && m_stops.first().contains(QStringLiteral("lat"))
           && NavGeo::metersBetween(lat, lng, m_stops.first().value(QStringLiteral("lat")).toDouble(),
                                    m_stops.first().value(QStringLiteral("lng")).toDouble()) <= kStopReachedRadius) {
        const QString title = m_stops.first().value(QStringLiteral("title")).toString();
        if (!m_stopManeuvers.isEmpty())
            m_stopManeuvers.removeFirst();
        m_stops.removeFirst();
        emit announce(title.isEmpty() ? tr("Stop reached") : tr("Stop reached: %1").arg(title),
                      QStringLiteral("stopReached"));
        emit stopsChanged();
    }
    bool changed = false;
    while (!m_stopManeuvers.isEmpty() && m_stepIndex > m_stopManeuvers.first()) {
        const QString title = m_stops.isEmpty() ? QString() : m_stops.first().value(QStringLiteral("title")).toString();
        m_stopManeuvers.removeFirst();
        if (!m_stops.isEmpty())
            m_stops.removeFirst();
        emit announce(title.isEmpty() ? tr("Stop reached") : tr("Stop reached: %1").arg(title),
                      QStringLiteral("stopReached"));
        changed = true;
    }
    if (changed)
        emit stopsChanged();
}

QVariantList Navigator::stops() const
{
    QVariantList out;
    for (const QVariantMap &m : m_stops)
        out.append(m);
    return out;
}

void Navigator::addStop(const QVariantMap &place)
{
    if ((m_mode != QStringLiteral("pedestrian") && m_mode != QStringLiteral("bicycle"))
            || m_state != QStringLiteral("guiding") || m_rerouting)
        return;
    // Insert where it adds the least walking (straight-line estimate) to the
    // trip: current position -> stops -> destination.
    auto pt = [](const QVariantMap &m) {
        return qMakePair(m.value(QStringLiteral("navLat"), m.value(QStringLiteral("lat"))).toDouble(),
                         m.value(QStringLiteral("navLng"), m.value(QStringLiteral("lng"))).toDouble());
    };
    int bestIndex = 0;
    qreal bestLen = -1;
    for (int at = 0; at <= m_stops.size(); ++at) {
        QVector<QPair<double, double>> path{qMakePair(m_lastLat, m_lastLng)};
        for (int i = 0; i <= m_stops.size(); ++i) {
            if (i == at)
                path.append(pt(place));
            if (i < m_stops.size())
                path.append(pt(m_stops.at(i)));
        }
        path.append(pt(m_destination));
        qreal len = 0;
        for (int i = 1; i < path.size(); ++i)
            len += NavGeo::metersBetween(path[i - 1].first, path[i - 1].second, path[i].first, path[i].second);
        if (bestLen < 0 || len < bestLen) {
            bestLen = len;
            bestIndex = at;
        }
    }
    m_addedStopIndex = bestIndex;
    m_stops.insert(bestIndex, place);
    m_addingStop = true;
    emit stopsChanged();
    maybeReroute(m_lastLat, m_lastLng, m_lastHeading, now(), /*manual=*/true);
}

void Navigator::updateDerivedProperties()
{
    if (m_maneuvers.isEmpty())
        return;

    const qreal curDist = m_maneuvers[m_stepIndex].distanceAlong;
    m_distanceToManeuver = qMax(0.0, curDist - m_traveled);

    m_hasThen = m_stepIndex + 1 < m_maneuvers.size()
            && (m_maneuvers[m_stepIndex + 1].distanceAlong - curDist) < kThenPreviewGap;

    const qreal prevDist = m_stepIndex > 0 ? m_maneuvers[m_stepIndex - 1].distanceAlong : 0.0;
    const qreal segLen = curDist - prevDist;
    const qreal fraction = segLen > 1e-6 ? qBound(0.0, (m_traveled - prevDist) / segLen, 1.0) : 1.0;
    const int prevDuration = m_stepIndex > 0 ? m_maneuvers[m_stepIndex - 1].duration : 0;
    qreal remaining = prevDuration * (1.0 - fraction);
    for (int k = m_stepIndex; k < m_maneuvers.size() - 1; ++k)
        remaining += m_maneuvers[k].duration;
    m_remainingSeconds = int(remaining + 0.5);
    m_remainingDistance = qMax(0.0, m_totalLength - m_traveled);

    emit maneuverChanged();
}

void Navigator::checkAnnouncements(qint64 /*tsMs*/)
{
    // The final maneuver ("arrive") is announced through arrived() instead.
    if (m_stepIndex < 0 || m_stepIndex >= m_maneuvers.size() - 1)
        return;

    qreal farM, nearM, nowM;
    if (m_mode == QStringLiteral("car")) {
        if (m_lastSpeed * 3.6 > 80.0) {
            farM = 1500; nearM = 400; nowM = qMax(30.0, 5.0 * m_lastSpeed);
        } else {
            farM = 300; nearM = 100; nowM = 30;
        }
    } else {
        farM = 60; nearM = 12; nowM = 5;
    }

    const QString text = m_maneuvers[m_stepIndex].instruction;
    if (!m_announcedFar[m_stepIndex] && m_distanceToManeuver <= farM) {
        m_announcedFar[m_stepIndex] = true;
        emit announce(text, QStringLiteral("far"));
    }
    if (!m_announcedNear[m_stepIndex] && m_distanceToManeuver <= nearM) {
        m_announcedNear[m_stepIndex] = true;
        emit announce(text, QStringLiteral("near"));
    }
    if (!m_announcedNow[m_stepIndex] && m_distanceToManeuver <= nowM) {
        m_announcedNow[m_stepIndex] = true;
        emit announce(text, QStringLiteral("now"));
    }
}

qreal Navigator::offRouteThreshold(qreal accuracyM) const
{
    const qreal base = m_mode == QStringLiteral("car") ? 35.0 : 25.0;
    return qMax(base, 1.5 * accuracyM);
}

void Navigator::checkOffRoute(qreal perpDistance, qint64 tsMs)
{
    const bool was = m_offRoute;
    m_offRoute = perpDistance > offRouteThreshold(m_lastAccuracy);
    if (m_offRoute) {
        if (m_offRouteSinceMs < 0)
            m_offRouteSinceMs = tsMs;
    } else {
        m_offRouteSinceMs = -1;
    }
    if (was != m_offRoute)
        emit stateChanged();
}

// Projects (lat,lng) onto the current m_coords/m_cumDist polyline, updating
// m_lastMatchPoint and m_traveled, and returns the perpendicular distance to
// the matched segment. Shared by plain car/walk/bike guidance and by a
// transit walking leg (which loads its own coords into these same members
// for the duration of that leg — see loadWalkLegIntoRouteState).
qreal Navigator::matchPositionToRoute(double lat, double lng)
{
    // Windowed search: only project against segments within 50 m behind and
    // 600 m ahead of the last match, so a long route stays cheap to match
    // against and a stale match can't jump to a far-away, coincidentally
    // close bit of the route (e.g. a road the route crosses twice).
    const qreal lastDist = m_cumDist.isEmpty() ? 0 : m_cumDist[qBound(0, m_lastMatchPoint, m_cumDist.size() - 1)];
    const qreal winLo = qMax(0.0, lastDist - 50.0);
    const qreal winHi = lastDist + 600.0;

    int loIdx = 0, hiIdx = m_coords.size() - 2;
    for (int i = 0; i < m_coords.size() - 1; ++i) {
        loIdx = i;
        if (m_cumDist[i + 1] >= winLo)
            break;
    }
    for (int i = m_coords.size() - 2; i >= 0; --i) {
        hiIdx = i;
        if (m_cumDist[i] <= winHi)
            break;
    }
    if (hiIdx < loIdx)
        hiIdx = loIdx;

    qreal bestPerp = -1, bestDistAlong = m_traveled;
    int bestSeg = m_lastMatchPoint;
    for (int i = loIdx; i <= hiIdx && i < m_coords.size() - 1; ++i) {
        const auto proj = NavGeo::projectOntoSegment(lat, lng,
                m_coords[i].first, m_coords[i].second, m_coords[i + 1].first, m_coords[i + 1].second);
        const qreal distAlong = m_cumDist[i] + proj.distanceFromStart;
        if (bestPerp < 0 || proj.perpDistance < bestPerp) {
            bestPerp = proj.perpDistance;
            bestDistAlong = distAlong;
            bestSeg = i;
        }
    }
    m_lastMatchPoint = bestSeg;
    // Progress never runs backwards: GPS jitter can project slightly behind
    // the last match, but the trip itself only moves forward.
    m_traveled = qMin(m_totalLength, qMax(m_traveled, bestDistAlong));
    return bestPerp;
}

void Navigator::updatePosition(double lat, double lng, double accuracyM,
                               double speedMps, double headingDeg, qint64 timestampMs)
{
    if (m_state == QStringLiteral("idle") || m_state == QStringLiteral("arrived")
            || m_state == QStringLiteral("error"))
        return;

    const qint64 ts = timestampMs >= 0 ? timestampMs : now();
    m_lastLat = lat; m_lastLng = lng; m_lastHeading = headingDeg;
    m_lastSpeed = speedMps; m_lastAccuracy = accuracyM;

    if (m_mode == QStringLiteral("transit")) {
        updateTransitPosition(lat, lng, accuracyM, speedMps, ts);
        return;
    }
    if (m_coords.size() < 2)
        return;

    const qreal bestPerp = matchPositionToRoute(lat, lng);
    advanceStepPointer();
    checkStopsReached(lat, lng);
    updateDerivedProperties();

    if (m_state == QStringLiteral("waitingForGps")) {
        m_state = QStringLiteral("guiding");
        emit stateChanged();
    }

    if (m_stepIndex == m_maneuvers.size() - 1 && m_distanceToManeuver <= kArrivalRadius) {
        m_state = QStringLiteral("arrived");
        m_rerouting = false;
        emit stateChanged();
        emit announce(tr("You have arrived"), QStringLiteral("arrived"));
        emit arrived();
        return;
    }

    checkAnnouncements(ts);
    checkOffRoute(bestPerp, ts);
    if (m_offRoute && !m_rerouting && m_offRouteSinceMs >= 0 && ts - m_offRouteSinceMs >= kOffRouteDelayMs)
        maybeReroute(lat, lng, headingDeg, ts);
}

void Navigator::maybeReroute(double lat, double lng, double headingDeg, qint64 tsMs, bool manual)
{
    if (!manual) {
        if (m_rerouteCount >= kRerouteMaxCount)
            return;
        if (m_lastRerouteMs >= 0 && tsMs - m_lastRerouteMs < kRerouteCooldownMs)
            return;
    }
    m_lastRerouteMs = tsMs;
    ++m_rerouteCount;
    m_rerouting = true;
    emit stateChanged();
    emit announce(tr("Recalculating route…"), QStringLiteral("rerouting"));

    const int courseArg = (m_mode == QStringLiteral("car") && headingDeg >= 0) ? int(headingDeg) : -1;
    if (m_requester)
        m_requester(lat, lng, courseArg);

    if (!m_client || !m_settings) {
        // Test-only Navigators built without real collaborators: the spy
        // above already observed the attempt, nothing else to do.
        m_rerouting = false;
        dropFailedStop();
        emit stateChanged();
        return;
    }
    if (!m_client->fixtureMode() && !m_settings->hasHereKey()) {
        m_rerouting = false;
        dropFailedStop();
        emit stateChanged();
        return;
    }

    const int gen = ++m_rerouteGeneration;
    const HereUrls::LatLng from{lat, lng};
    const HereUrls::LatLng to{
        m_destination.value(QStringLiteral("navLat"), m_destination.value(QStringLiteral("lat"))).toDouble(),
        m_destination.value(QStringLiteral("navLng"), m_destination.value(QStringLiteral("lng"))).toDouble()
    };
    const QString mode = m_mode;
    QVector<HereUrls::LatLng> vias;
    for (const QVariantMap &stop : m_stops) {
        vias.append({stop.value(QStringLiteral("navLat"), stop.value(QStringLiteral("lat"))).toDouble(),
                     stop.value(QStringLiteral("navLng"), stop.value(QStringLiteral("lng"))).toDouble()});
    }
    const QString variant = m_variant;
    const bool valhalla = mode == QStringLiteral("bicycle") && variant == QStringLiteral("green");
    const QUrl url = valhalla
            ? HereUrls::valhallaBicycle(from, to, vias, m_settings->lang(), m_settings->units())
            : HereUrls::route(m_settings->hereApiKey(), mode, from, to, m_settings->lang(), m_settings->units(),
                              0, courseArg, vias,
                              mode == QStringLiteral("bicycle") && variant == QStringLiteral("short")
                                      ? QStringLiteral("short") : QString());
    const QString fixture = mode == QStringLiteral("bicycle") && !variant.isEmpty() && variant != QStringLiteral("fast")
            ? QStringLiteral("route_bicycle_") + variant : QStringLiteral("route_") + mode;
    m_client->get(url, fixture, [this, gen, mode, variant, valhalla](bool ok, const QJsonObject &json, const QString &) {
        if (gen != m_rerouteGeneration)
            return;
        QVariantList items = !ok ? QVariantList()
                : valhalla ? ItineraryParser::parseValhalla(json, mode) : ItineraryParser::parseRoutes(json, mode);
        if (!items.isEmpty()) {
            QVariantMap first = items.first().toMap();
            first[QStringLiteral("variant")] = variant;
            items[0] = first;
        }
        applyRerouteResult(ok && !items.isEmpty(), items);
    });
}

void Navigator::retryReroute()
{
    if (m_state == QStringLiteral("idle") || m_state == QStringLiteral("error"))
        return;
    maybeReroute(m_lastLat, m_lastLng, m_lastHeading, now(), /*manual=*/true);
}

void Navigator::dropFailedStop()
{
    if (!m_addingStop)
        return;
    m_addingStop = false;
    if (m_addedStopIndex >= 0 && m_addedStopIndex < m_stops.size())
        m_stops.removeAt(m_addedStopIndex);
    emit stopsChanged();
    emit announce(tr("Couldn't add the stop"), QStringLiteral("stopFailed"));
}

void Navigator::applyRerouteResult(bool ok, const QVariantList &items)
{
    m_rerouting = false;
    if (!ok)
        dropFailedStop();
    if (ok) {
        const QVariantMap newItinerary = items.first().toMap();
        const bool addedStop = m_addingStop;
        m_addingStop = false;
        rebuildFromItinerary(newItinerary);
        m_state = QStringLiteral("guiding");
        emit announce(addedStop ? tr("Stop added") : tr("Route updated"),
                      addedStop ? QStringLiteral("stopAdded") : QStringLiteral("rerouted"));
        emit rerouted(newItinerary);
        emit maneuverChanged();
    }
    emit stateChanged();
}
