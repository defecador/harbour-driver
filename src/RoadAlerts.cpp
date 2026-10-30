#include "RoadAlerts.h"

#include <QDateTime>
#include <QGeoCoordinate>
#include <algorithm>
#include <cmath>

#include "AppSettings.h"
#include "NavGeo.h"
#include "SignPacks.h"

namespace {
// Signs must lie this close to the route/heading line. Cameras sit further
// from the centre line (gantries, poles at the roadside).
const double kSignTolerance = 12.0;
const double kCameraTolerance = 25.0;
// Route and sign direction may differ by this much (HERE and OSM geometry
// disagree by a few degrees, and curves bend the road under the sign).
const double kRouteBearingTolerance = 35.0;
const double kConeBearingTolerance = 40.0;
// Further than this from the route we fall back to the heading cone.
const double kOffRouteMeters = 50.0;
// Free drive only warns when actually moving (about 11 km/h).
const double kConeMinSpeed = 3.0;
// A sign that already alerted stays quiet for this long (ms).
const qint64 kRealertMs = 10 * 60 * 1000;
const double kPassedBehind = -5.0;
}

RoadAlerts::RoadAlerts(AppSettings *settings, SignPacks *packs, QObject *parent)
    : QObject(parent), m_settings(settings), m_packs(packs)
{
    if (m_settings) {
        connect(m_settings, &AppSettings::warnSignsChanged, this, &RoadAlerts::readSettings);
        connect(m_settings, &AppSettings::speedCamerasChanged, this, &RoadAlerts::readSettings);
        readSettings();
    }
}

void RoadAlerts::readSettings()
{
    if (!m_settings)
        return;
    setKinds(m_settings->warnSigns(), m_settings->speedCameras());
}

void RoadAlerts::setKinds(bool signs, bool cameras)
{
    m_signsOn = signs;
    m_camerasOn = cameras;
    if (!m_signsOn && !m_camerasOn)
        setNoData(false);
    // Drop an alert for a kind that was just switched off.
    if ((m_kind == QLatin1String("camera") && !m_camerasOn)
        || ((m_kind == QLatin1String("stop") || m_kind == QLatin1String("giveWay")) && !m_signsOn))
        clearAlert();
}

void RoadAlerts::setActive(bool on)
{
    if (on == m_active)
        return;
    m_active = on;
    if (!on) {
        clearAlert();
        setNoData(false);
        setLimit(0);
        m_havePrev = false;
        m_haveRef = false;
    }
    emit activeChanged();
}

double RoadAlerts::lookahead(bool camera, double speedMps)
{
    return camera ? qBound(250.0, speedMps * 15.0, 800.0) : qBound(120.0, speedMps * 12.0, 300.0);
}

void RoadAlerts::setRoute(const QVariantList &coordinates)
{
    m_coords.clear();
    m_cum.clear();
    m_traveled = 0;
    for (const QVariant &v : coordinates) {
        const QGeoCoordinate c = v.value<QGeoCoordinate>();
        if (!c.isValid())
            continue;
        if (!m_coords.isEmpty()) {
            m_cum.append(m_cum.last()
                         + NavGeo::metersBetween(m_coords.last().first, m_coords.last().second,
                                                 c.latitude(), c.longitude()));
        } else {
            m_cum.append(0.0);
        }
        m_coords.append(qMakePair(c.latitude(), c.longitude()));
    }
    if (m_coords.size() < 2) {
        m_coords.clear();
        m_cum.clear();
    }
    m_limitSpans.clear();
    clearAlert();
}

void RoadAlerts::setRouteLimits(const QVariantList &limits)
{
    m_limitSpans.clear();
    for (const QVariant &v : limits) {
        const QVariantMap m = v.toMap();
        const int a = m.value(QStringLiteral("start")).toInt(), b = m.value(QStringLiteral("end")).toInt();
        const int kmh = m.value(QStringLiteral("kmh")).toInt();
        if (kmh > 0 && a >= 0 && b > a && b < m_cum.size())
            m_limitSpans.append({m_cum[a], m_cum[b], kmh});
    }
}

void RoadAlerts::clearRoute()
{
    m_limitSpans.clear();
    m_coords.clear();
    m_cum.clear();
    m_traveled = 0;
    clearAlert();
}

void RoadAlerts::clearAlert()
{
    m_misses = 0;
    m_curKey = 0;
    if (m_kind.isEmpty())
        return;
    m_kind.clear();
    m_distance = 0;
    m_maxspeed = 0;
    emit alertChanged();
}

void RoadAlerts::setLimit(int kmh)
{
    if (kmh == m_limit)
        return;
    m_limit = kmh;
    emit speedLimitChanged();
}

// The limit comes from the route when it has one for this stretch, otherwise
// from the downloaded road data. It is held for a few seconds when the data
// has a gap (a junction, a missing tag) so the sign doesn't flicker.
void RoadAlerts::updateLimit(double lat, double lng, bool onRoute, qint64 ts)
{
    int kmh = 0;
    if (onRoute) {
        for (const LimitSpan &sp : m_limitSpans) {
            if (m_traveled >= sp.from && m_traveled < sp.to) {
                kmh = sp.kmh;
                break;
            }
        }
    }
    if (kmh == 0 && m_packs)
        kmh = m_packs->speedLimitAt(lat, lng, m_heading);
    if (kmh > 0) {
        m_limitTs = ts;
        setLimit(kmh);
    } else if (m_limit > 0 && ts - m_limitTs > 8000) {
        setLimit(0);
    }
}

void RoadAlerts::setNoData(bool v)
{
    if (v == m_noData)
        return;
    m_noData = v;
    emit noDataChanged();
}

// Projects the position onto the route near the progress made so far, advances
// the progress, and returns the distance from the route (huge when no route).
double RoadAlerts::matchRoute(double lat, double lng)
{
    if (m_cum.size() < 2)
        return 1e9;
    const int n = m_cum.size();
    int i = int(std::upper_bound(m_cum.begin(), m_cum.end(), m_traveled - 50.0) - m_cum.begin()) - 1;
    i = qMax(0, i);
    double bestPerp = 1e9, bestAlong = m_traveled;
    for (; i < n - 1 && m_cum[i] <= m_traveled + 600.0; ++i) {
        const NavGeo::Projection p = NavGeo::projectOntoSegment(
            lat, lng, m_coords[i].first, m_coords[i].second, m_coords[i + 1].first, m_coords[i + 1].second);
        if (p.perpDistance < bestPerp) {
            bestPerp = p.perpDistance;
            bestAlong = m_cum[i] + p.distanceFromStart;
        }
    }
    if (bestPerp <= kOffRouteMeters)
        m_traveled = qMax(m_traveled, bestAlong);
    return bestPerp;
}

// Distance along the route (from the current progress) to the closest spot of
// the route that passes within `tolerance` of the sign with a compatible
// direction. False when the sign isn't on the route within maxAhead.
bool RoadAlerts::alongRoute(double lat, double lng, double tolerance, double maxAhead,
                            double signBearing, double *distanceAhead) const
{
    const int n = m_cum.size();
    int i = int(std::upper_bound(m_cum.begin(), m_cum.end(), m_traveled - 30.0) - m_cum.begin()) - 1;
    i = qMax(0, i);
    bool found = false;
    double bestPerp = 1e9;
    for (; i < n - 1 && m_cum[i] <= m_traveled + maxAhead + 40.0; ++i) {
        const NavGeo::Projection p = NavGeo::projectOntoSegment(
            lat, lng, m_coords[i].first, m_coords[i].second, m_coords[i + 1].first, m_coords[i + 1].second);
        if (p.perpDistance > tolerance || p.perpDistance >= bestPerp)
            continue;
        if (signBearing >= 0) {
            const double segBearing = NavGeo::bearingDegrees(m_coords[i].first, m_coords[i].second,
                                                             m_coords[i + 1].first, m_coords[i + 1].second);
            if (NavGeo::angleDiff(segBearing, signBearing) > kRouteBearingTolerance)
                continue;
        }
        const double ahead = m_cum[i] + p.distanceFromStart - m_traveled;
        if (ahead < kPassedBehind || ahead > maxAhead)
            continue;
        bestPerp = p.perpDistance;
        *distanceAhead = ahead;
        found = true;
    }
    return found;
}

void RoadAlerts::updatePosition(double lat, double lng, double speedMps, double headingDeg, qint64 ts)
{
    if (ts < 0)
        ts = QDateTime::currentMSecsSinceEpoch();

    if (speedMps < 0 && m_havePrev) {
        const double dt = (ts - m_prevTs) / 1000.0;
        if (dt > 0.05)
            speedMps = NavGeo::metersBetween(m_prevLat, m_prevLng, lat, lng) / dt;
    }
    if (speedMps < 0)
        speedMps = 0;
    m_havePrev = true;
    m_prevLat = lat;
    m_prevLng = lng;
    m_prevTs = ts;

    // Keep the last good heading: GPS reports none while standing still.
    if (headingDeg >= 0) {
        m_heading = headingDeg;
        m_refLat = lat;
        m_refLng = lng;
        m_haveRef = true;
    } else if (!m_haveRef) {
        m_refLat = lat;
        m_refLng = lng;
        m_haveRef = true;
    } else if (NavGeo::metersBetween(m_refLat, m_refLng, lat, lng) >= 8.0) {
        m_heading = NavGeo::bearingDegrees(m_refLat, m_refLng, lat, lng);
        m_refLat = lat;
        m_refLng = lng;
    }

    if (!m_active) {
        clearAlert();
        setLimit(0);
        return;
    }
    const double routePerp = matchRoute(lat, lng);
    const bool onRoute = routePerp <= kOffRouteMeters;
    updateLimit(lat, lng, onRoute, ts);
    if ((!m_signsOn && !m_camerasOn) || !m_packs) {
        clearAlert();
        return;
    }
    setNoData(!m_packs->covers(lat, lng));

    const double signsAhead = lookahead(false, speedMps);
    const double camsAhead = lookahead(true, speedMps);
    const QVector<Sign> candidates = m_packs->near(lat, lng, (m_camerasOn ? camsAhead : signsAhead) + 60.0);

    const bool cone = !onRoute && m_heading >= 0 && speedMps >= kConeMinSpeed;

    bool have = false;
    double bestDist = 1e9;
    Sign best;
    for (const Sign &s : candidates) {
        const bool camera = s.kind == Sign::Camera;
        if (camera ? !m_camerasOn : !m_signsOn)
            continue;
        const double maxAhead = camera ? camsAhead : signsAhead;
        double dist = 0;
        if (onRoute) {
            if (!alongRoute(s.lat(), s.lng(), camera ? kCameraTolerance : kSignTolerance, maxAhead,
                            s.bearing, &dist))
                continue;
        } else if (cone) {
            const double d = NavGeo::metersBetween(lat, lng, s.lat(), s.lng());
            if (d > maxAhead * 1.5 || d < 1.0)
                continue;
            const double toSign = NavGeo::bearingDegrees(lat, lng, s.lat(), s.lng());
            const double delta = NavGeo::angleDiff(m_heading, toSign) * M_PI / 180.0;
            const double fwd = d * std::cos(delta);
            const double lateral = d * std::sin(delta);
            if (fwd <= 0 || fwd > maxAhead)
                continue;
            if (lateral > qMin(30.0, (camera ? 15.0 : 10.0) + 0.08 * fwd))
                continue;
            if (s.bearing >= 0 && NavGeo::angleDiff(m_heading, s.bearing) > kConeBearingTolerance)
                continue;
            dist = fwd;
        } else {
            continue;
        }
        if (dist < bestDist) {
            bestDist = dist;
            best = s;
            have = true;
        }
    }

    if (!have) {
        if (m_kind.isEmpty())
            return;
        // Passed it (it was close) or lost it for a few fixes.
        if (m_distance < 50 || ++m_misses >= 3)
            clearAlert();
        return;
    }

    m_misses = 0;
    const QString kind = best.kind == Sign::Stop ? QStringLiteral("stop")
                       : best.kind == Sign::GiveWay ? QStringLiteral("giveWay")
                       : QStringLiteral("camera");
    const qint64 key = best.id() * 4 + best.kind;
    const bool isNew = key != m_curKey;
    const int dist = int(std::lround(qMax(0.0, bestDist)));
    const bool changed = isNew || dist != m_distance || kind != m_kind || best.maxspeed != m_maxspeed;
    m_curKey = key;
    m_kind = kind;
    m_distance = dist;
    m_maxspeed = best.maxspeed;
    if (changed)
        emit alertChanged();
    if (isNew) {
        const qint64 last = m_alerted.value(key, -1);
        if (last < 0 || ts - last > kRealertMs) {
            if (m_alerted.size() > 400) {
                for (auto it = m_alerted.begin(); it != m_alerted.end();) {
                    if (ts - it.value() > kRealertMs)
                        it = m_alerted.erase(it);
                    else
                        ++it;
                }
            }
            m_alerted.insert(key, ts);
            emit alertStarted(kind);
        }
    }
}
