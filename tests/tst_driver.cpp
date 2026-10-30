#include <QtTest>
#include <QGeoCoordinate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QUrlQuery>
#include <cmath>

#include "FlexiblePolyline.h"
#include "HereUrls.h"
#include "ItineraryParser.h"
#include "NavGeo.h"
#include "Navigator.h"
#include "RoadAlerts.h"
#include "SignPacks.h"
#include "SpeedLimitStore.h"
#include <QTemporaryDir>

class TstDriver : public QObject
{
    Q_OBJECT

private slots:
    // --- FlexiblePolyline --------------------------------------------
    void polyline_decodesKnownEncoding();
    void polyline_rejectsBadVersion();
    void polyline_emptyInputGivesEmptyOutput();

    // --- ItineraryParser -----------------------------------------------
    void parser_routeActionOffsetsAreFlattenedAcrossSections();
    void parser_legActionOffsetsStayLocal();
    void parser_timeTextExtractsHHMM();
    void parser_carTrafficDelayIsDurationMinusBase();

    // --- HereUrls --------------------------------------------------
    void urls_routeOmitsCourseByDefault();
    void urls_routeAddsCourseWhenGiven();
    void urls_pedestrianRouteSendsDepartureTimeAny();
    void urls_carRouteOmitsDepartureTime();
    void urls_bicycleShortestSetsRoutingMode();
    void urls_viasAreAskedToPassThrough();
    void urls_valhallaRequestCarriesViasAndNoKey();
    void parser_valhallaRouteBecomesAnItinerary();
    void navigator_stopIsReachedByWalkingUpToIt();
    void urls_transitReturnIncludesActions();
    void urls_redactHidesApiKeyValue();

    // --- NavGeo ------------------------------------------------------
    void navgeo_metersBetweenMatchesKnownDistance();
    void navgeo_projectOntoSegmentFindsMidpoint();
    void navgeo_projectOntoSegmentClampsPastEndpoints();

    // --- Navigator -------------------------------------------------
    void navigator_tracksProgressAndRemainingTime();
    void navigator_firesArrivedNearFinalPoint();
    void navigator_thenPreviewOnlyWhenManeuversAreClose();
    void navigator_announcesEachTierExactlyOnce();
    void navigator_addStopIsWalkAndBikeOnly();
    void navigator_failedAddStopIsDropped();
    void navigator_stopIsReachedAndTripContinues();
    void navigator_offRouteWaitsBeforeRerouting();
    void navigator_rerouteRespectsCooldownAndPerTripLimit();
    void navigator_slightlyOffRouteWaitsLonger();
    void navigator_snapsFixesOntoTheRoute();

    // --- Navigator transit -------------------------------------------
    void parser_stopExpectedMsEqualsTime();
    void transit_walksWaitsRidesAlightsAndArrives();
    void transit_countsStopsByTimetableWithNoGpsFix();
    void transit_alightAnnouncementsFireExactlyOnce();
    void transit_missedConnectionSetsFlagWithoutRequest();
    void transit_replanTransitMakesExactlyOneRequest();
    void transit_delayRefreshUpdatesMatchingLegOnly();
    void transit_delayRefreshNoMatchKeepsOldPlan();
    void transit_hurryWhenWalkIsLongerThanTimeToDeparture();
    void transit_offRouteOnWalkLegReroutesPedestrianToLegEnd();

    // --- Sign packs and road alerts ------------------------------------------
    void navgeo_angleDiffWrapsAround();
    void navgeo_bearingDegreesCardinals();
    void signpacks_roundTripKeepsRecords();
    void signpacks_rejectsDamagedFiles();
    void signpacks_nearAndCovers();
    void roadalerts_routeAlertFiresOnceAndClearsAfterPassing();
    void roadalerts_routeIgnoresOppositeDirectionAndSideStreets();
    void roadalerts_camerasOnlyWhenEnabled();
    void roadalerts_freeDriveUsesHeadingCone();
    void roadalerts_lookaheadGrowsWithSpeed();
    void roadalerts_inactiveStaysSilentAndNoDataIsReported();
    void roadalerts_fixtureAlongRouteCar();
    void tolls_parserListsCountriesAndAvoidSegments();
    void tolls_urlAndBody();
    void speedlimit_parserReadsHereSpans();
    void speedlimit_storeEncodeLoadLookup();
    void speedlimit_storeCrossesTileBorders();
    void roadalerts_speedLimitFromRouteThenRoadData();
};

// Encoded with a from-scratch encoder that mirrors FlexiblePolyline::decode()
// (version=1, precision=5, no third dimension); verified in Python to decode
// back to these exact points before being pasted in here.
void TstDriver::polyline_decodesKnownEncoding()
{
    const auto pts = FlexiblePolyline::decode(QStringLiteral("BFoz5xJ67i1B1B7PzIha9L7Y"));
    QCOMPARE(pts.size(), 4);
    // Precision 5 means the encoder itself rounds each coordinate to 5 decimals
    // before encoding, so the decoded values are compared against that rounded
    // form, not the original unrounded input the string was generated from.
    QVERIFY(qAbs(pts[0].lat - 50.10228) < 1e-9);
    QVERIFY(qAbs(pts[0].lng - 8.69821) < 1e-9);
    QVERIFY(qAbs(pts[3].lat - 50.09872) < 1e-9);
    QVERIFY(qAbs(pts[3].lng - 8.68752) < 1e-9);
}

void TstDriver::polyline_rejectsBadVersion()
{
    // First varint (version) decodes to 2, which decode() must refuse.
    QVERIFY(FlexiblePolyline::decode(QStringLiteral("CFoz5xJ")).isEmpty());
}

void TstDriver::polyline_emptyInputGivesEmptyOutput()
{
    QVERIFY(FlexiblePolyline::decode(QString()).isEmpty());
}

namespace {

QJsonObject makeSection(const QString &type, double depLat, double depLng, double arrLat, double arrLng,
                        int duration, int length, const QJsonArray &actions = QJsonArray())
{
    QJsonObject dep, depPlace, depLoc;
    depLoc[QStringLiteral("lat")] = depLat;
    depLoc[QStringLiteral("lng")] = depLng;
    depPlace[QStringLiteral("location")] = depLoc;
    depPlace[QStringLiteral("name")] = QStringLiteral("A");
    dep[QStringLiteral("place")] = depPlace;
    dep[QStringLiteral("time")] = QStringLiteral("2026-01-01T10:00:00+01:00");

    QJsonObject arr, arrPlace, arrLoc;
    arrLoc[QStringLiteral("lat")] = arrLat;
    arrLoc[QStringLiteral("lng")] = arrLng;
    arrPlace[QStringLiteral("location")] = arrLoc;
    arrPlace[QStringLiteral("name")] = QStringLiteral("B");
    arr[QStringLiteral("place")] = arrPlace;
    arr[QStringLiteral("time")] = QStringLiteral("2026-01-01T10:10:00+01:00");

    QJsonObject sum;
    sum[QStringLiteral("duration")] = duration;
    sum[QStringLiteral("length")] = length;

    QJsonObject s;
    s[QStringLiteral("type")] = type;
    s[QStringLiteral("polyline")] = QString();
    s[QStringLiteral("departure")] = dep;
    s[QStringLiteral("arrival")] = arr;
    s[QStringLiteral("summary")] = sum;
    s[QStringLiteral("actions")] = actions;
    return s;
}

QJsonArray makeActions(const QVector<QPair<QString, int>> &actionOffsets)
{
    QJsonArray out;
    for (const auto &ao : actionOffsets) {
        QJsonObject a;
        a[QStringLiteral("action")] = ao.first;
        a[QStringLiteral("offset")] = ao.second;
        a[QStringLiteral("length")] = 0;
        a[QStringLiteral("duration")] = 0;
        out.append(a);
    }
    return out;
}

}

// Section 1 has 3 polyline points (so its actions' own offsets run 0..2);
// section 2's actions must come back shifted by 3 once flattened to route level.
void TstDriver::parser_routeActionOffsetsAreFlattenedAcrossSections()
{
    QJsonArray s1Actions = makeActions({{QStringLiteral("depart"), 0}, {QStringLiteral("turn"), 2}});
    QJsonArray s2Actions = makeActions({{QStringLiteral("arrive"), 0}});

    QJsonObject s1 = makeSection(QStringLiteral("vehicle"), 52.0, 13.0, 52.1, 13.1, 300, 1000, s1Actions);
    // Give section 1 a 3-point polyline so decodeCoords().size() == 3.
    s1[QStringLiteral("polyline")] = QStringLiteral("BFoz5xJ67i1B1B7PzIha");
    QJsonObject s2 = makeSection(QStringLiteral("vehicle"), 52.1, 13.1, 52.2, 13.2, 200, 500, s2Actions);

    QJsonObject route;
    route[QStringLiteral("sections")] = QJsonArray{s1, s2};
    QJsonObject root;
    root[QStringLiteral("routes")] = QJsonArray{route};

    const QVariantList items = ItineraryParser::parseRoutes(root, QStringLiteral("car"));
    QCOMPARE(items.size(), 1);
    const QVariantList actions = items.first().toMap().value(QStringLiteral("actions")).toList();
    QCOMPARE(actions.size(), 3);
    QCOMPARE(actions[0].toMap().value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(actions[1].toMap().value(QStringLiteral("offset")).toInt(), 2);
    // section 2's "arrive" had local offset 0; route-level it must land at 3
    // (section 1 contributed 3 coordinate points ahead of it).
    QCOMPARE(actions[2].toMap().value(QStringLiteral("offset")).toInt(), 3);

    const QVariantList legs = items.first().toMap().value(QStringLiteral("legs")).toList();
    QCOMPARE(legs.size(), 2);
    const QVariantList leg2Actions = legs[1].toMap().value(QStringLiteral("actions")).toList();
    // Per-leg actions stay leg-local: unlike the flattened list above, this must stay 0.
    QCOMPARE(leg2Actions[0].toMap().value(QStringLiteral("offset")).toInt(), 0);
}

void TstDriver::parser_legActionOffsetsStayLocal()
{
    QJsonArray actions = makeActions({{QStringLiteral("depart"), 0}, {QStringLiteral("arrive"), 5}});
    QJsonObject s = makeSection(QStringLiteral("pedestrian"), 52.0, 13.0, 52.01, 13.01, 300, 400, actions);
    QJsonObject route;
    route[QStringLiteral("sections")] = QJsonArray{s};
    QJsonObject root;
    root[QStringLiteral("routes")] = QJsonArray{route};

    const QVariantList items = ItineraryParser::parseRoutes(root, QStringLiteral("pedestrian"));
    const QVariantList legActions = items.first().toMap().value(QStringLiteral("legs")).toList()
            .first().toMap().value(QStringLiteral("actions")).toList();
    QCOMPARE(legActions[1].toMap().value(QStringLiteral("offset")).toInt(), 5);
}

void TstDriver::parser_timeTextExtractsHHMM()
{
    QCOMPARE(ItineraryParser::timeText(QStringLiteral("2026-09-28T10:07:00+02:00")), QStringLiteral("10:07"));
    QCOMPARE(ItineraryParser::timeText(QStringLiteral("not-a-date")), QString());
}

void TstDriver::parser_carTrafficDelayIsDurationMinusBase()
{
    QJsonObject s = makeSection(QStringLiteral("vehicle"), 52.0, 13.0, 52.1, 13.1, 900, 4000);
    QJsonObject sum = s[QStringLiteral("summary")].toObject();
    sum[QStringLiteral("baseDuration")] = 600;
    s[QStringLiteral("summary")] = sum;
    QJsonObject route;
    route[QStringLiteral("sections")] = QJsonArray{s};
    QJsonObject root;
    root[QStringLiteral("routes")] = QJsonArray{route};

    const QVariantList items = ItineraryParser::parseRoutes(root, QStringLiteral("car"));
    QCOMPARE(items.first().toMap().value(QStringLiteral("trafficDelay")).toInt(), 300);
}

void TstDriver::urls_routeOmitsCourseByDefault()
{
    const QUrl url = HereUrls::route(QStringLiteral("k"), QStringLiteral("car"), {52.0, 13.0}, {52.1, 13.1},
                                     QStringLiteral("en-GB"), QStringLiteral("metric"), 2);
    QVERIFY(!url.toString().contains(QStringLiteral("course=")));
}

void TstDriver::urls_routeAddsCourseWhenGiven()
{
    const QUrl url = HereUrls::route(QStringLiteral("k"), QStringLiteral("car"), {52.0, 13.0}, {52.1, 13.1},
                                     QStringLiteral("en-GB"), QStringLiteral("metric"), 0, 270);
    QVERIFY(url.toString().contains(QStringLiteral("course%3D270")) || url.toString().contains(QStringLiteral("course=270")));
}

void TstDriver::urls_pedestrianRouteSendsDepartureTimeAny()
{
    const QUrl url = HereUrls::route(QStringLiteral("k"), QStringLiteral("pedestrian"), {52.0, 13.0}, {52.1, 13.1},
                                     QStringLiteral("en-GB"), QStringLiteral("metric"), 0);
    QVERIFY(url.toString().contains(QStringLiteral("departureTime")));
}

void TstDriver::urls_carRouteOmitsDepartureTime()
{
    const QUrl url = HereUrls::route(QStringLiteral("k"), QStringLiteral("car"), {52.0, 13.0}, {52.1, 13.1},
                                     QStringLiteral("en-GB"), QStringLiteral("metric"), 0);
    QVERIFY(!url.toString().contains(QStringLiteral("departureTime")));
}

void TstDriver::urls_bicycleShortestSetsRoutingMode()
{
    const QUrl url = HereUrls::route(QStringLiteral("k"), QStringLiteral("bicycle"), {52.0, 13.0}, {52.1, 13.1},
                                     QStringLiteral("en-GB"), QStringLiteral("metric"), 0, -1, {}, QStringLiteral("short"));
    QVERIFY(url.toString().contains(QStringLiteral("routingMode=short")));
    const QUrl plain = HereUrls::route(QStringLiteral("k"), QStringLiteral("bicycle"), {52.0, 13.0}, {52.1, 13.1},
                                       QStringLiteral("en-GB"), QStringLiteral("metric"), 0);
    QVERIFY(!plain.toString().contains(QStringLiteral("routingMode")));
}

void TstDriver::urls_viasAreAskedToPassThrough()
{
    const QUrl url = HereUrls::route(QStringLiteral("k"), QStringLiteral("pedestrian"), {52.0, 13.0}, {52.1, 13.1},
                                     QStringLiteral("en-GB"), QStringLiteral("metric"), 0, -1, {{52.05, 13.05}});
    const QString s = QUrl::fromPercentEncoding(url.toString().toUtf8());
    QVERIFY(s.contains(QStringLiteral("via=52.050000,13.050000!passThrough=true")));
}

void TstDriver::urls_valhallaRequestCarriesViasAndNoKey()
{
    const QUrl url = HereUrls::valhallaBicycle({52.0, 13.0}, {52.1, 13.1}, {{52.05, 13.05}},
                                               QStringLiteral("en-US"), QStringLiteral("metric"));
    QCOMPARE(url.host(), QStringLiteral("valhalla1.openstreetmap.de"));
    QVERIFY(!url.toString().contains(QStringLiteral("apiKey")));
    const QJsonObject req = QJsonDocument::fromJson(QUrlQuery(url).queryItemValue(QStringLiteral("json"), QUrl::FullyDecoded).toUtf8()).object();
    const QJsonArray locs = req.value(QStringLiteral("locations")).toArray();
    QCOMPARE(locs.size(), 3);
    QCOMPARE(locs.at(1).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("through"));
    QCOMPARE(req.value(QStringLiteral("costing")).toString(), QStringLiteral("bicycle"));
}

void TstDriver::parser_valhallaRouteBecomesAnItinerary()
{
    // Shape: 5 points encoded with 6-decimal precision (52.5208,13.4094 ... 52.5163,13.3777).
    const QString json = QStringLiteral(
        "{\"trip\":{\"status\":0,\"summary\":{\"time\":1020,\"length\":4.6},\"legs\":[{"
        "\"shape\":\"_urdcBofmqXnoBnmMv|AntLn}@ntLnKvbS\",\"maneuvers\":["
        "{\"type\":1,\"instruction\":\"Bike west.\",\"time\":300,\"length\":1.3,\"begin_shape_index\":0},"
        "{\"type\":10,\"instruction\":\"Turn right.\",\"time\":420,\"length\":1.9,\"begin_shape_index\":2},"
        "{\"type\":4,\"instruction\":\"Arrived.\",\"time\":0,\"length\":0.0,\"begin_shape_index\":4}]}]}}");
    const QVariantList items = ItineraryParser::parseValhalla(QJsonDocument::fromJson(json.toUtf8()).object(), QStringLiteral("bicycle"));
    QCOMPARE(items.size(), 1);
    const QVariantMap it = items.first().toMap();
    QCOMPARE(it.value(QStringLiteral("length")).toInt(), 4600);
    QCOMPARE(it.value(QStringLiteral("duration")).toInt(), 1020);
    const QVariantList coords = it.value(QStringLiteral("coordinates")).toList();
    QCOMPARE(coords.size(), 5);
    QVERIFY(qAbs(coords.first().value<QGeoCoordinate>().latitude() - 52.5208) < 1e-4);
    QVERIFY(qAbs(coords.last().value<QGeoCoordinate>().longitude() - 13.3777) < 1e-4);
    const QVariantList actions = it.value(QStringLiteral("actions")).toList();
    QCOMPARE(actions.size(), 3);
    QCOMPARE(actions.at(0).toMap().value(QStringLiteral("action")).toString(), QStringLiteral("depart"));
    QCOMPARE(actions.at(1).toMap().value(QStringLiteral("action")).toString(), QStringLiteral("turn"));
    QCOMPARE(actions.at(1).toMap().value(QStringLiteral("direction")).toString(), QStringLiteral("right"));
    QCOMPARE(actions.at(1).toMap().value(QStringLiteral("offset")).toInt(), 2);
    QCOMPARE(actions.at(2).toMap().value(QStringLiteral("action")).toString(), QStringLiteral("arrive"));
    // A failed Valhalla reply (no "trip") yields no routes.
    QVERIFY(ItineraryParser::parseValhalla(QJsonObject(), QStringLiteral("bicycle")).isEmpty());
}

void TstDriver::urls_transitReturnIncludesActions()
{
    const QUrl url = HereUrls::transit(QStringLiteral("k"), {52.0, 13.0}, {52.1, 13.1},
                                       QStringLiteral("en-GB"), QStringLiteral("metric"), 3);
    QVERIFY(url.toString().contains(QStringLiteral("actions")));
}

void TstDriver::urls_redactHidesApiKeyValue()
{
    const QString redacted = HereUrls::redact(QStringLiteral("https://x/?apiKey=SECRET123&lang=en"));
    QVERIFY(!redacted.contains(QStringLiteral("SECRET123")));
    QVERIFY(redacted.contains(QStringLiteral("lang=en")));
}

void TstDriver::navgeo_metersBetweenMatchesKnownDistance()
{
    // One degree of longitude at the equator is ~111.19 km with NavGeo's own
    // Earth radius (6371 km); this checks the helper against that, not GPS
    // ground truth, since the flat-earth approximation is deliberately
    // simplified for city-scale use, not geodesic precision.
    const double d = NavGeo::metersBetween(0.0, 13.0, 0.0, 14.0);
    QVERIFY(qAbs(d - 111194.9) < 1.0);
}

void TstDriver::navgeo_projectOntoSegmentFindsMidpoint()
{
    // A point north of the segment's own midpoint longitude (13.005, halfway
    // between 13.0 and 13.01) should project onto the midpoint (t=0.5).
    const auto proj = NavGeo::projectOntoSegment(0.001, 13.005, 0.0, 13.0, 0.0, 13.01);
    QVERIFY(qAbs(proj.t - 0.5) < 0.01);
    QVERIFY(qAbs(proj.perpDistance - NavGeo::metersBetween(0.0, 13.005, 0.001, 13.005)) < 1.0);
}

void TstDriver::navgeo_projectOntoSegmentClampsPastEndpoints()
{
    // A point far beyond B must still project onto B itself (t clamped to 1),
    // never extrapolated past the segment.
    const auto proj = NavGeo::projectOntoSegment(0.0, 20.0, 0.0, 13.0, 0.0, 13.01);
    QCOMPARE(proj.t, 1.0);
}

namespace {

// A straight line running east along the equator, so every point on it is
// exactly `metersFromStart` metres from (0, 13.0) with no latitude component
// to complicate the maths. Kept separate from real routes/cities on purpose:
// these tests are about Navigator's own progress/timing logic, not geodesy.
QGeoCoordinate onTestRoute(double metersFromStart)
{
    constexpr double kEarthRadius = 6371000.0;
    constexpr double kDegPerMeterAtEquator = 180.0 / (M_PI * kEarthRadius);
    return QGeoCoordinate(0.0, 13.0 + metersFromStart * kDegPerMeterAtEquator);
}

// A point `metersNorth` north of the same route's start, for off-route tests.
QGeoCoordinate northOfRouteStart(double metersNorth)
{
    constexpr double kEarthRadius = 6371000.0;
    constexpr double kDegPerMeterAtEquator = 180.0 / (M_PI * kEarthRadius);
    return QGeoCoordinate(metersNorth * kDegPerMeterAtEquator, 13.0);
}

QVariantMap makeAction(const QString &action, const QString &direction, const QString &instruction,
                       int duration, int offset)
{
    QVariantMap m;
    m[QStringLiteral("action")] = action;
    m[QStringLiteral("direction")] = direction;
    m[QStringLiteral("instruction")] = instruction;
    m[QStringLiteral("duration")] = duration;
    m[QStringLiteral("offset")] = offset;
    return m;
}

QVariantMap makeItinerary(const QString &mode, const QVector<QGeoCoordinate> &points,
                          const QVector<QVariantMap> &actions)
{
    QVariantList coords;
    for (const auto &p : points)
        coords.append(QVariant::fromValue(p));
    QVariantList acts;
    for (const auto &a : actions)
        acts.append(a);
    QVariantMap it;
    it[QStringLiteral("mode")] = mode;
    it[QStringLiteral("coordinates")] = coords;
    it[QStringLiteral("actions")] = acts;
    return it;
}

QVariantMap destOf(const QGeoCoordinate &c)
{
    QVariantMap m;
    m[QStringLiteral("lat")] = c.latitude();
    m[QStringLiteral("lng")] = c.longitude();
    return m;
}

}

void TstDriver::navigator_tracksProgressAndRemainingTime()
{
    // depart(0m) --60s--> turn(200m) --40s--> arrive(400m)
    const auto itinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(200), onTestRoute(400)},
            {makeAction(QStringLiteral("depart"), QString(), QStringLiteral("Head east"), 60, 0),
             makeAction(QStringLiteral("turn"), QStringLiteral("left"), QStringLiteral("Turn left"), 40, 1),
             makeAction(QStringLiteral("arrive"), QString(), QStringLiteral("Arrive"), 0, 2)});

    Navigator nav(nullptr, nullptr);
    nav.start(itinerary, destOf(onTestRoute(400)));
    QCOMPARE(nav.state(), QStringLiteral("waitingForGps"));

    const auto p = onTestRoute(100);
    nav.updatePosition(p.latitude(), p.longitude(), 5.0, 10.0, 90.0, 1000);

    QCOMPARE(nav.state(), QStringLiteral("guiding"));
    QVERIFY(qAbs(nav.distanceToManeuver() - 100.0) < 2.0);
    QVERIFY(qAbs(nav.remainingDistance() - 300.0) < 2.0);
    // Half of the 60s "depart" leg still to run (30s) + the full 40s "turn" leg.
    QVERIFY(qAbs(nav.remainingSeconds() - 70) <= 1);
}

void TstDriver::navigator_firesArrivedNearFinalPoint()
{
    const auto itinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(400)},
            {makeAction(QStringLiteral("depart"), QString(), QStringLiteral("Head east"), 60, 0),
             makeAction(QStringLiteral("arrive"), QString(), QStringLiteral("Arrive"), 0, 1)});

    Navigator nav(nullptr, nullptr);
    nav.start(itinerary, destOf(onTestRoute(400)));
    QSignalSpy spy(&nav, &Navigator::arrived);

    const auto p = onTestRoute(390);
    nav.updatePosition(p.latitude(), p.longitude(), 5.0, 5.0, 90.0, 1000);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(nav.state(), QStringLiteral("arrived"));
}

void TstDriver::navigator_thenPreviewOnlyWhenManeuversAreClose()
{
    // Two maneuvers 80m apart (< 150m threshold): "then" should show.
    auto closeItinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(100), onTestRoute(180), onTestRoute(260)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 20, 0),
             makeAction(QStringLiteral("turn"), QStringLiteral("left"), QStringLiteral("Turn left"), 10, 1),
             makeAction(QStringLiteral("turn"), QStringLiteral("right"), QStringLiteral("Turn right"), 20, 2),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 3)});
    Navigator close(nullptr, nullptr);
    close.start(closeItinerary, destOf(onTestRoute(260)));
    auto p0 = onTestRoute(0);
    close.updatePosition(p0.latitude(), p0.longitude(), 5.0, 5.0, 90.0, 1000);
    QVERIFY(close.hasThen());
    QCOMPARE(close.thenAction(), QStringLiteral("turn"));
    QCOMPARE(close.thenDirection(), QStringLiteral("right"));

    // Same shape but 220m apart (>= 150m threshold): "then" should not show.
    auto farItinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(100), onTestRoute(320), onTestRoute(400)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 20, 0),
             makeAction(QStringLiteral("turn"), QStringLiteral("left"), QStringLiteral("Turn left"), 10, 1),
             makeAction(QStringLiteral("turn"), QStringLiteral("right"), QStringLiteral("Turn right"), 20, 2),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 3)});
    Navigator far(nullptr, nullptr);
    far.start(farItinerary, destOf(onTestRoute(400)));
    far.updatePosition(p0.latitude(), p0.longitude(), 5.0, 5.0, 90.0, 1000);
    QVERIFY(!far.hasThen());
}

void TstDriver::navigator_announcesEachTierExactlyOnce()
{
    // Pedestrian thresholds are far=60m, near=12m, now=5m.
    const auto itinerary = makeItinerary(QStringLiteral("pedestrian"),
            {onTestRoute(0), onTestRoute(100), onTestRoute(140)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 60, 0),
             makeAction(QStringLiteral("turn"), QStringLiteral("right"), QStringLiteral("Turn right"), 20, 1),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 2)});
    Navigator nav(nullptr, nullptr);
    nav.start(itinerary, destOf(onTestRoute(140)));
    QSignalSpy spy(&nav, &Navigator::announce);

    auto feedAt = [&](double metersFromStart, qint64 ts) {
        const auto p = onTestRoute(metersFromStart);
        nav.updatePosition(p.latitude(), p.longitude(), 5.0, 1.5, 90.0, ts);
    };

    feedAt(30, 1000);   // 70m to go: above the 60m "far" threshold, nothing yet.
    QCOMPARE(spy.count(), 0);
    feedAt(45, 2000);   // 55m to go: crosses "far".
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.last().at(1).toString(), QStringLiteral("far"));
    feedAt(85, 3000);   // 15m to go: not yet "near" (12m).
    QCOMPARE(spy.count(), 1);
    feedAt(89, 4000);   // 11m to go: crosses "near".
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.last().at(1).toString(), QStringLiteral("near"));
    feedAt(96, 5000);   // 4m to go: crosses "now".
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.last().at(1).toString(), QStringLiteral("now"));
    feedAt(96, 6000);   // Same position again: no repeat announcements.
    QCOMPARE(spy.count(), 3);
}

void TstDriver::navigator_addStopIsWalkAndBikeOnly()
{
    const auto itinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(400)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 60, 0),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 1)});
    Navigator nav(nullptr, nullptr);
    int requests = 0;
    nav.setRerouteRequesterForTesting([&](double, double, int) { ++requests; });
    nav.start(itinerary, destOf(onTestRoute(400)));
    const auto p = onTestRoute(10);
    nav.updatePosition(p.latitude(), p.longitude(), 5.0, 10.0, 0.0, 1000);
    QVariantMap place;
    place[QStringLiteral("title")] = QStringLiteral("Cafe");
    nav.addStop(place);
    QCOMPARE(nav.stopCount(), 0);
    QCOMPARE(requests, 0);
}

void TstDriver::navigator_failedAddStopIsDropped()
{
    const auto itinerary = makeItinerary(QStringLiteral("pedestrian"),
            {onTestRoute(0), onTestRoute(400)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 300, 0),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 1)});
    Navigator nav(nullptr, nullptr);
    int requests = 0;
    nav.setRerouteRequesterForTesting([&](double, double, int) { ++requests; });
    nav.start(itinerary, destOf(onTestRoute(400)));
    const auto p = onTestRoute(10);
    nav.updatePosition(p.latitude(), p.longitude(), 5.0, 1.4, 0.0, 1000);
    QSignalSpy announces(&nav, &Navigator::announce);

    QVariantMap place;
    place[QStringLiteral("title")] = QStringLiteral("Cafe");
    nav.addStop(place);

    // One routing attempt was made; with no way to carry it out the stop is dropped.
    QCOMPARE(requests, 1);
    QCOMPARE(nav.stopCount(), 0);
    bool sawFailure = false;
    for (const auto &a : announces)
        sawFailure = sawFailure || a.at(1).toString() == QStringLiteral("stopFailed");
    QVERIFY(sawFailure);
}

void TstDriver::navigator_stopIsReachedAndTripContinues()
{
    const auto firstRoute = makeItinerary(QStringLiteral("pedestrian"),
            {onTestRoute(0), onTestRoute(300)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 220, 0),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 1)});
    // The same walk re-planned through a stop at 150 m: HERE ends the first
    // section with an "arrive" and starts the second with a "depart".
    const auto viaRoute = makeItinerary(QStringLiteral("pedestrian"),
            {onTestRoute(0), onTestRoute(150), onTestRoute(300)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 110, 0),
             makeAction(QStringLiteral("arrive"), QString(), QStringLiteral("Arrive"), 0, 1),
             makeAction(QStringLiteral("depart"), QString(), QString(), 110, 1),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 2)});

    Navigator nav(nullptr, nullptr);
    nav.start(firstRoute, destOf(onTestRoute(300)));
    const auto p0 = onTestRoute(10);
    nav.updatePosition(p0.latitude(), p0.longitude(), 5.0, 1.4, 0.0, 1000);

    QVariantMap cafe;
    cafe[QStringLiteral("title")] = QStringLiteral("Cafe");
    nav.m_stops.append(cafe);
    nav.m_addingStop = true;
    nav.m_rerouting = true;
    nav.applyRerouteResult(true, QVariantList{viaRoute});

    QCOMPARE(nav.stopCount(), 1);
    QCOMPARE(nav.instruction(), QStringLiteral("Stop: Cafe"));

    QSignalSpy announces(&nav, &Navigator::announce);
    QSignalSpy arrivedSpy(&nav, &Navigator::arrived);
    auto feedAt = [&](double m, qint64 ts) {
        const auto p = onTestRoute(m);
        nav.updatePosition(p.latitude(), p.longitude(), 5.0, 1.4, 0.0, ts);
    };

    feedAt(100, 2000);
    QCOMPARE(nav.stopCount(), 1);
    feedAt(160, 3000);                       // past the stop
    QCOMPARE(nav.stopCount(), 0);
    QCOMPARE(nav.state(), QStringLiteral("guiding"));
    QCOMPARE(arrivedSpy.count(), 0);
    bool sawReached = false;
    for (const auto &a : announces)
        sawReached = sawReached || a.at(1).toString() == QStringLiteral("stopReached");
    QVERIFY(sawReached);

    feedAt(298, 4000);                       // the real destination
    QCOMPARE(nav.state(), QStringLiteral("arrived"));
    QCOMPARE(arrivedSpy.count(), 1);
}

void TstDriver::navigator_stopIsReachedByWalkingUpToIt()
{
    // Pass-through stops have no "arrive" maneuver: walking up to the stop is what counts.
    const auto route = makeItinerary(QStringLiteral("pedestrian"),
            {onTestRoute(0), onTestRoute(300)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 220, 0),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 1)});
    Navigator nav(nullptr, nullptr);
    nav.start(route, destOf(onTestRoute(300)));
    QVariantMap cafe;
    cafe[QStringLiteral("title")] = QStringLiteral("Cafe");
    cafe[QStringLiteral("lat")] = onTestRoute(150).latitude();
    cafe[QStringLiteral("lng")] = onTestRoute(150).longitude();
    nav.m_stops.append(cafe);

    auto feedAt = [&](double m, qint64 ts) {
        const auto p = onTestRoute(m);
        nav.updatePosition(p.latitude(), p.longitude(), 5.0, 1.4, 0.0, ts);
    };
    feedAt(60, 1000);
    QCOMPARE(nav.stopCount(), 1);
    feedAt(100, 2000);                       // 50 m short: not yet
    QCOMPARE(nav.stopCount(), 1);
    feedAt(130, 3000);                       // 20 m short: reached
    QCOMPARE(nav.stopCount(), 0);
    QCOMPARE(nav.state(), QStringLiteral("guiding"));
}

void TstDriver::navigator_offRouteWaitsBeforeRerouting()
{
    const auto itinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(1000)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 60, 0),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 1)});
    Navigator nav(nullptr, nullptr);
    nav.start(itinerary, destOf(onTestRoute(1000)));

    int rerouteCalls = 0;
    nav.setRerouteRequesterForTesting([&](double, double, int) { ++rerouteCalls; });

    // 100m off the route (car's off-route threshold is 25m, so this is far
    // off): off-route immediately, but one stray fix must not reroute; the
    // next one, half a second or more later, does.
    const auto off = northOfRouteStart(100);
    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 0);
    QVERIFY(nav.offRoute());
    QCOMPARE(rerouteCalls, 0);

    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 300);
    QCOMPARE(rerouteCalls, 0);

    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 1000);
    QCOMPARE(rerouteCalls, 1);
    QCOMPARE(nav.rerouteCount(), 1);
}

void TstDriver::navigator_slightlyOffRouteWaitsLonger()
{
    const auto itinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(100000)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 60, 0),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 1)});
    Navigator nav(nullptr, nullptr);
    nav.start(itinerary, destOf(onTestRoute(100000)));
    int rerouteCalls = 0;
    nav.setRerouteRequesterForTesting([&](double, double, int) { ++rerouteCalls; });

    // 40m off: beyond the 25m threshold but not "far", so 1.5s are needed.
    const auto off = northOfRouteStart(40);
    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 0);
    QVERIFY(nav.offRoute());
    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 1000);
    QCOMPARE(rerouteCalls, 0);
    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 1600);
    QCOMPARE(rerouteCalls, 1);
}

void TstDriver::navigator_snapsFixesOntoTheRoute()
{
    const auto itinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(100000)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 60, 0),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 1)});
    Navigator nav(nullptr, nullptr);
    nav.start(itinerary, destOf(onTestRoute(100000)));
    QVERIFY(!nav.snapped());

    // 10m beside the road: drawn on it, heading along it.
    const auto beside = northOfRouteStart(10);
    nav.updatePosition(beside.latitude(), beside.longitude(), 5.0, 14.0, 80.0, 0);
    QVERIFY(nav.snapped());
    QVERIFY(qAbs(nav.snappedLat()) < 1e-7);
    QVERIFY(qAbs(nav.snappedHeading() - 90.0) < 0.5);

    // Really off the road: shown where it is.
    const auto off = northOfRouteStart(60);
    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 14.0, 80.0, 1000);
    QVERIFY(!nav.snapped());
}

void TstDriver::navigator_rerouteRespectsCooldownAndPerTripLimit()
{
    const auto itinerary = makeItinerary(QStringLiteral("car"),
            {onTestRoute(0), onTestRoute(100000)},
            {makeAction(QStringLiteral("depart"), QString(), QString(), 60, 0),
             makeAction(QStringLiteral("arrive"), QString(), QString(), 0, 1)});
    Navigator nav(nullptr, nullptr);
    nav.start(itinerary, destOf(onTestRoute(100000)));

    int rerouteCalls = 0;
    nav.setRerouteRequesterForTesting([&](double, double, int) { ++rerouteCalls; });
    const auto off = northOfRouteStart(100);

    // First reroute fires once 0.5s far off-route have passed.
    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 0);
    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 1000);
    QCOMPARE(rerouteCalls, 1);

    // Still off-route 1s later: the 3s cooldown blocks a second attempt.
    nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, 2000);
    QCOMPARE(rerouteCalls, 1);

    // 30 attempts total is the per-trip cap; keep going every 4s past that
    // and confirm it stops incrementing at 30.
    qint64 ts = 1000;
    for (int i = 0; i < 40; ++i) {
        ts += 4000;
        nav.updatePosition(off.latitude(), off.longitude(), 5.0, 5.0, 90.0, ts);
    }
    QCOMPARE(rerouteCalls, 30);
    QCOMPARE(nav.rerouteCount(), 30);
}

// =======================================================================
// Navigator transit tests
// =======================================================================

void TstDriver::parser_stopExpectedMsEqualsTime()
{
    QJsonObject depPlace, depLoc;
    depLoc[QStringLiteral("lat")] = 52.0;
    depLoc[QStringLiteral("lng")] = 13.0;
    depPlace[QStringLiteral("location")] = depLoc;
    depPlace[QStringLiteral("name")] = QStringLiteral("Stop A");
    QJsonObject dep;
    dep[QStringLiteral("place")] = depPlace;
    dep[QStringLiteral("time")] = QStringLiteral("2026-09-28T10:07:00+02:00");
    dep[QStringLiteral("delay")] = 90;

    QJsonObject arr = dep;
    arr[QStringLiteral("time")] = QStringLiteral("2026-09-28T10:16:00+02:00");

    QJsonObject section;
    section[QStringLiteral("type")] = QStringLiteral("transit");
    section[QStringLiteral("transport")] = QJsonObject{{QStringLiteral("mode"), QStringLiteral("subway")}, {QStringLiteral("name"), QStringLiteral("U2")}};
    section[QStringLiteral("departure")] = dep;
    section[QStringLiteral("arrival")] = arr;
    section[QStringLiteral("travelSummary")] = QJsonObject{{QStringLiteral("duration"), 540}, {QStringLiteral("length"), 2600}};
    section[QStringLiteral("polyline")] = QString();

    QJsonObject route;
    route[QStringLiteral("sections")] = QJsonArray{section};
    QJsonObject root;
    root[QStringLiteral("routes")] = QJsonArray{route};

    const QVariantList items = ItineraryParser::parseTransit(root);
    const QVariantMap from = items.first().toMap().value(QStringLiteral("legs")).toList()
            .first().toMap().value(QStringLiteral("from")).toMap();
    const qint64 expectedMs = QDateTime::fromString(QStringLiteral("2026-09-28T10:07:00+02:00"), Qt::ISODate).toMSecsSinceEpoch();
    QCOMPARE(from.value(QStringLiteral("timeMs")).toLongLong(), expectedMs);
    // expectedMs == timeMs by design (HERE's `time` is already the real/expected
    // time; `delay` is only reported alongside it) — see ItineraryParser.cpp.
    QCOMPARE(from.value(QStringLiteral("expectedMs")).toLongLong(), expectedMs);
    QCOMPARE(from.value(QStringLiteral("delay")).toInt(), 90);
}

namespace {

// A straight east-running line at `baseLat`, so every point is exactly
// `metersFromStart` from (baseLat, 13.0) with no latitude component —
// mirrors onTestRoute() above but parameterized so transit legs at
// different "places" don't overlap each other's geometry.
QGeoCoordinate transitPointAt(double baseLat, double metersFromStart)
{
    constexpr double kEarthRadius = 6371000.0;
    constexpr double kDegPerMeterAtEquator = 180.0 / (M_PI * kEarthRadius);
    return QGeoCoordinate(baseLat, 13.0 + metersFromStart * kDegPerMeterAtEquator);
}

QVariantMap makeStop(const QString &name, const QGeoCoordinate &c, qint64 expectedMs, int delay = -1)
{
    QVariantMap m;
    m[QStringLiteral("name")] = name;
    m[QStringLiteral("lat")] = c.latitude();
    m[QStringLiteral("lng")] = c.longitude();
    m[QStringLiteral("expectedMs")] = expectedMs;
    m[QStringLiteral("delay")] = delay;
    m[QStringLiteral("platform")] = QStringLiteral("1");
    return m;
}

QVariantMap makeWalkLeg(const QVariantMap &from, const QVariantMap &to, const QVector<QGeoCoordinate> &points,
                        int durationSecs = 0)
{
    QVariantList coords;
    for (const auto &p : points)
        coords.append(QVariant::fromValue(p));
    QVariantMap leg;
    leg[QStringLiteral("type")] = QStringLiteral("pedestrian");
    leg[QStringLiteral("from")] = from;
    leg[QStringLiteral("to")] = to;
    leg[QStringLiteral("coordinates")] = coords;
    if (durationSecs > 0) {
        // An explicit "depart" maneuver carrying the leg's whole duration,
        // so remainingSeconds (used by the hurry check) isn't always zero
        // the way the synthesized no-actions "arrive" maneuver would be.
        QVariantMap depart;
        depart[QStringLiteral("action")] = QStringLiteral("depart");
        depart[QStringLiteral("instruction")] = QStringLiteral("Walk");
        depart[QStringLiteral("duration")] = durationSecs;
        depart[QStringLiteral("offset")] = 0;
        QVariantMap arrive;
        arrive[QStringLiteral("action")] = QStringLiteral("arrive");
        arrive[QStringLiteral("duration")] = 0;
        arrive[QStringLiteral("offset")] = qMax(0, points.size() - 1);
        leg[QStringLiteral("actions")] = QVariantList{depart, arrive};
    } else {
        leg[QStringLiteral("actions")] = QVariantList();   // exercises the synthesized "arrive" maneuver
    }
    leg[QStringLiteral("duration")] = durationSecs > 0 ? durationSecs : 60;
    return leg;
}

QVariantMap makeTransitLeg(const QVariantMap &from, const QVariantMap &to, const QVector<QGeoCoordinate> &points,
                           const QVariantList &stops, const QString &lineName)
{
    QVariantList coords;
    for (const auto &p : points)
        coords.append(QVariant::fromValue(p));
    QVariantMap leg;
    leg[QStringLiteral("type")] = QStringLiteral("transit");
    leg[QStringLiteral("transportMode")] = QStringLiteral("subway");
    leg[QStringLiteral("lineName")] = lineName;
    leg[QStringLiteral("headsign")] = QStringLiteral("Somewhere");
    leg[QStringLiteral("lineColor")] = QStringLiteral("e4592e");
    leg[QStringLiteral("lineTextColor")] = QStringLiteral("ffffff");
    leg[QStringLiteral("from")] = from;
    leg[QStringLiteral("to")] = to;
    leg[QStringLiteral("coordinates")] = coords;
    leg[QStringLiteral("stops")] = stops;
    leg[QStringLiteral("actions")] = QVariantList();
    return leg;
}

QVariantMap makeTransitItinerary(const QVariantList &legs)
{
    QVariantMap it;
    it[QStringLiteral("mode")] = QStringLiteral("transit");
    it[QStringLiteral("legs")] = legs;
    it[QStringLiteral("actions")] = QVariantList();
    return it;
}

}

// walk(0->40m, base 0.0) -> transit(U2, base 0.001, 0->600m, 1 intermediate
// stop at 300m) -> walk(base 0.002, 0->40m) -> arrived. Driven purely by
// simulated GPS fixes at the fake clock's timestamps, mirroring a real trip.
void TstDriver::transit_walksWaitsRidesAlightsAndArrives()
{
    const qint64 t0 = 1000000;
    // Walk legs are well longer than kStopRadius (40m) so the first fix, at
    // the leg's own start, isn't already within arrival distance of its end.
    const auto a0 = transitPointAt(0.0, 0), a1 = transitPointAt(0.0, 200);
    const auto b0 = transitPointAt(0.001, 0), bMid = transitPointAt(0.001, 300), b1 = transitPointAt(0.001, 600);
    const auto c0 = transitPointAt(0.002, 0), c1 = transitPointAt(0.002, 200);

    const QVariantMap stopA = makeStop(QStringLiteral("Stop A"), a1, t0);
    const QVariantMap stopB = makeStop(QStringLiteral("Stop B"), b0, t0 + 60000);
    const QVariantMap stopC = makeStop(QStringLiteral("Stop C"), b1, t0 + 360000);
    const QVariantMap stopD = makeStop(QStringLiteral("Stop D"), c1, t0 + 420000);

    const QVariantList midStops = {makeStop(QStringLiteral("Mid"), bMid, t0 + 210000)};

    const QVariantList legs = {
        makeWalkLeg(stopA, stopB, {a0, a1}),
        makeTransitLeg(stopB, stopC, {b0, bMid, b1}, midStops, QStringLiteral("U2")),
        makeWalkLeg(stopC, stopD, {c0, c1})
    };
    const QVariantMap itinerary = makeTransitItinerary(legs);

    Navigator nav(nullptr, nullptr);
    nav.setClockForTesting([&]() { return t0; });   // overridden per-call below via timestampMs
    nav.start(itinerary, QVariantMap{{QStringLiteral("lat"), c1.latitude()}, {QStringLiteral("lng"), c1.longitude()}});
    QCOMPARE(nav.transitPhase(), QStringLiteral("walk"));

    // Walk leg 1: reach stop B.
    nav.updatePosition(a0.latitude(), a0.longitude(), 5, 1.4, 90, t0);
    QCOMPARE(nav.transitPhase(), QStringLiteral("walk"));
    nav.updatePosition(a1.latitude(), a1.longitude(), 5, 1.4, 90, t0 + 30000);
    QCOMPARE(nav.transitPhase(), QStringLiteral("wait"));
    QCOMPARE(nav.boardStopName(), QStringLiteral("Stop B"));
    QCOMPARE(nav.lineName(), QStringLiteral("U2"));

    // Tick past the fake "now" to drive boarding (tick uses now(), not the
    // updatePosition timestamp, so point the fake clock at departure time).
    nav.setClockForTesting([&]() { return t0 + 60000; });
    nav.updatePosition(b0.latitude(), b0.longitude(), 5, 0, 90, t0 + 60000);
    // updateTransitTick only runs off the internal QTimer in production; call
    // the same transition path it would reach by feeding a position at/after
    // departure, which the ride-phase matching below also exercises.
    QVERIFY(nav.transitPhase() == QStringLiteral("wait") || nav.transitPhase() == QStringLiteral("ride"));

    // Ride: pass the intermediate stop, then approach the alighting stop.
    nav.setClockForTesting([&]() { return t0 + 60000; });
    nav.updatePosition(b0.latitude(), b0.longitude(), 5, 10, 90, t0 + 60000);
    nav.updatePosition(bMid.latitude(), bMid.longitude(), 5, 10, 90, t0 + 210000);
    QCOMPARE(nav.stopsRemaining(), 1);
    // 50m short of the alighting stop: within kAlightRadius (300m) so
    // "alight" has begun, but outside kStopPassedSlack (30m) so the leg
    // hasn't completed yet — that needs its own, later fix.
    const auto bNearEnd = transitPointAt(0.001, 550);
    nav.updatePosition(bNearEnd.latitude(), bNearEnd.longitude(), 5, 2, 90, t0 + 355000);
    QCOMPARE(nav.transitPhase(), QStringLiteral("alight"));
    nav.updatePosition(b1.latitude(), b1.longitude(), 5, 0, 90, t0 + 360000);
    QCOMPARE(nav.transitPhase(), QStringLiteral("walk"));
    QCOMPARE(nav.legIndex(), 2);

    // Final walk leg to the destination.
    nav.updatePosition(c0.latitude(), c0.longitude(), 5, 1.4, 90, t0 + 361000);
    QSignalSpy arrivedSpy(&nav, &Navigator::arrived);
    nav.updatePosition(c1.latitude(), c1.longitude(), 5, 1.4, 90, t0 + 390000);
    QCOMPARE(arrivedSpy.count(), 1);
    QCOMPARE(nav.state(), QStringLiteral("arrived"));
}

// With no GPS fix at all during the ride, stops still advance from the
// timetable alone once their (delay-adjusted) time has passed.
void TstDriver::transit_countsStopsByTimetableWithNoGpsFix()
{
    const qint64 t0 = 2000000;
    const auto b0 = transitPointAt(0.01, 0), bMid = transitPointAt(0.01, 300), b1 = transitPointAt(0.01, 600);
    const QVariantMap stopB = makeStop(QStringLiteral("Board"), b0, t0);
    const QVariantMap stopC = makeStop(QStringLiteral("Alight"), b1, t0 + 300000);
    const QVariantList midStops = {makeStop(QStringLiteral("Mid"), bMid, t0 + 150000)};
    const QVariantList legs = {makeTransitLeg(stopB, stopC, {b0, bMid, b1}, midStops, QStringLiteral("Bus"))};
    Navigator nav(nullptr, nullptr);
    qint64 fakeNow = t0;
    nav.setClockForTesting([&]() { return fakeNow; });
    nav.start(makeTransitItinerary(legs), QVariantMap{{QStringLiteral("lat"), b1.latitude()}, {QStringLiteral("lng"), b1.longitude()}});
    QCOMPARE(nav.transitPhase(), QStringLiteral("wait"));

    // One GPS fix at the very start establishes "guiding", then nothing
    // else arrives (as if underground) — only the clock advances.
    nav.updatePosition(b0.latitude(), b0.longitude(), 5, 0, 90, t0);

    fakeNow = t0 + 10000;   // past departure: board.
    nav.updateTransitTick(fakeNow);
    QVERIFY(nav.transitPhase() != QStringLiteral("wait"));

    fakeNow = t0 + 150000;   // past the intermediate stop's time.
    nav.updateTransitTick(fakeNow);
    QCOMPARE(nav.stopsRemaining(), 1);

    fakeNow = t0 + 300000;   // past arrival: the leg (and trip) ends.
    nav.updateTransitTick(fakeNow);
    QCOMPARE(nav.state(), QStringLiteral("arrived"));
}

void TstDriver::transit_alightAnnouncementsFireExactlyOnce()
{
    const qint64 t0 = 3000000;
    const auto b0 = transitPointAt(0.02, 0), b1 = transitPointAt(0.02, 400);
    const QVariantMap stopB = makeStop(QStringLiteral("Board"), b0, t0);
    const QVariantMap stopC = makeStop(QStringLiteral("Alight"), b1, t0 + 200000);
    const QVariantList legs = {makeTransitLeg(stopB, stopC, {b0, b1}, QVariantList(), QStringLiteral("Tram"))};
    Navigator nav(nullptr, nullptr);
    nav.setClockForTesting([&]() { return t0; });
    nav.start(makeTransitItinerary(legs), QVariantMap{{QStringLiteral("lat"), b1.latitude()}, {QStringLiteral("lng"), b1.longitude()}});
    QSignalSpy spy(&nav, &Navigator::announce);

    nav.updatePosition(b0.latitude(), b0.longitude(), 5, 0, 90, t0);
    nav.updateTransitTick(t0);   // past departure (fromExpectedMs == t0): board, phase -> "ride".
    QCOMPARE(nav.transitPhase(), QStringLiteral("ride"));
    // Only one stop total (the alighting one): "alight" fires as soon as the
    // ride begins (stopsRemaining starts at 1), repeated fixes must not repeat it.
    nav.updatePosition(transitPointAt(0.02, 100).latitude(), transitPointAt(0.02, 100).longitude(), 5, 10, 90, t0 + 10000);
    nav.updatePosition(transitPointAt(0.02, 101).latitude(), transitPointAt(0.02, 101).longitude(), 5, 10, 90, t0 + 11000);
    int alightCount = 0, alightNowCount = 0;
    for (int i = 0; i < spy.count(); ++i) {
        const QString kind = spy.at(i).at(1).toString();
        if (kind == QStringLiteral("alight")) ++alightCount;
        if (kind == QStringLiteral("alightNow")) ++alightNowCount;
    }
    QCOMPARE(alightCount, 1);

    nav.updatePosition(b1.latitude(), b1.longitude(), 5, 0, 90, t0 + 20000);
    nav.updatePosition(b1.latitude(), b1.longitude(), 5, 0, 90, t0 + 21000);
    alightNowCount = 0;
    for (int i = 0; i < spy.count(); ++i)
        if (spy.at(i).at(1).toString() == QStringLiteral("alightNow"))
            ++alightNowCount;
    QCOMPARE(alightNowCount, 1);
}

void TstDriver::transit_missedConnectionSetsFlagWithoutRequest()
{
    const qint64 t0 = 4000000;
    const auto b0 = transitPointAt(0.03, 0), b1 = transitPointAt(0.03, 400);
    // Departure 5s after t0, so the first fix (at t0) is still "wait", not
    // already past departure (which would promote straight to "ride").
    const QVariantMap stopB = makeStop(QStringLiteral("Board"), b0, t0 + 5000, /*delay=*/0);
    const QVariantMap stopC = makeStop(QStringLiteral("Alight"), b1, t0 + 200000);
    const QVariantList legs = {makeTransitLeg(stopB, stopC, {b0, b1}, QVariantList(), QStringLiteral("U2"))};
    Navigator nav(nullptr, nullptr);
    qint64 fakeNow = t0;
    nav.setClockForTesting([&]() { return fakeNow; });
    int requests = 0;
    nav.setRerouteRequesterForTesting([&](double, double, int) { ++requests; });
    nav.start(makeTransitItinerary(legs), QVariantMap{{QStringLiteral("lat"), b1.latitude()}, {QStringLiteral("lng"), b1.longitude()}});
    nav.updatePosition(b0.latitude(), b0.longitude(), 5, 0, 90, t0);
    QVERIFY(!nav.missedConnection());

    // 90s grace period after departure, still parked at the stop.
    fakeNow = t0 + 5000 + 95000;
    nav.updateTransitTick(fakeNow);
    QVERIFY(nav.missedConnection());
    QCOMPARE(requests, 0);   // detecting a miss never spends a HERE call by itself.
}

void TstDriver::transit_replanTransitMakesExactlyOneRequest()
{
    const qint64 t0 = 5000000;
    const auto b0 = transitPointAt(0.04, 0), b1 = transitPointAt(0.04, 400);
    const QVariantMap stopB = makeStop(QStringLiteral("Board"), b0, t0, 0);
    const QVariantMap stopC = makeStop(QStringLiteral("Alight"), b1, t0 + 200000);
    const QVariantList legs = {makeTransitLeg(stopB, stopC, {b0, b1}, QVariantList(), QStringLiteral("U2"))};
    Navigator nav(nullptr, nullptr);
    nav.setClockForTesting([&]() { return t0; });
    int requests = 0;
    nav.setRerouteRequesterForTesting([&](double, double, int) { ++requests; });
    nav.start(makeTransitItinerary(legs), QVariantMap{{QStringLiteral("lat"), b1.latitude()}, {QStringLiteral("lng"), b1.longitude()}});
    nav.updatePosition(b0.latitude(), b0.longitude(), 5, 0, 90, t0);

    nav.replanTransit();
    QCOMPARE(requests, 1);
    // No real HereClient/AppSettings were given to this Navigator, so the
    // request stops right there (same "test-only" early-out maybeReroute()
    // uses) — rerouteCount still reflects exactly one attempt being made.
    QCOMPARE(nav.rerouteCount(), 1);
}

void TstDriver::transit_delayRefreshUpdatesMatchingLegOnly()
{
    // This exercises applyDelayRefresh()'s matching logic directly (same
    // shape HereClient's callback would hand it), since driving the real
    // network path needs a live HereClient/AppSettings pair that the other
    // Navigator unit tests also avoid constructing.
    const qint64 t0 = 6000000;
    const auto b0 = transitPointAt(0.05, 0), b1 = transitPointAt(0.05, 400);
    const QVariantMap stopB = makeStop(QStringLiteral("Board"), b0, t0, 0);
    const QVariantMap stopC = makeStop(QStringLiteral("Alight"), b1, t0 + 200000);
    const QVariantList legs = {makeTransitLeg(stopB, stopC, {b0, b1}, QVariantList(), QStringLiteral("U2"))};
    Navigator nav(nullptr, nullptr);
    nav.setClockForTesting([&]() { return t0; });
    nav.start(makeTransitItinerary(legs), QVariantMap{{QStringLiteral("lat"), b1.latitude()}, {QStringLiteral("lng"), b1.longitude()}});

    const QVariantMap updatedStopB = makeStop(QStringLiteral("Board"), b0, t0 + 180000, 180);
    const QVariantMap updatedStopC = makeStop(QStringLiteral("Alight"), b1, t0 + 380000, 180);
    const QVariantList refreshedLegs = {makeTransitLeg(updatedStopB, updatedStopC, {b0, b1}, QVariantList(), QStringLiteral("U2"))};
    const QVariantList refreshedItems = {makeTransitItinerary(refreshedLegs)};
    nav.applyDelayRefresh(true, refreshedItems);
    QCOMPARE(nav.departureDelay(), 180);
}

void TstDriver::transit_delayRefreshNoMatchKeepsOldPlan()
{
    const qint64 t0 = 7000000;
    const auto b0 = transitPointAt(0.06, 0), b1 = transitPointAt(0.06, 400);
    const QVariantMap stopB = makeStop(QStringLiteral("Board"), b0, t0, 0);
    const QVariantMap stopC = makeStop(QStringLiteral("Alight"), b1, t0 + 200000);
    const QVariantList legs = {makeTransitLeg(stopB, stopC, {b0, b1}, QVariantList(), QStringLiteral("U2"))};
    Navigator nav(nullptr, nullptr);
    nav.setClockForTesting([&]() { return t0; });
    nav.start(makeTransitItinerary(legs), QVariantMap{{QStringLiteral("lat"), b1.latitude()}, {QStringLiteral("lng"), b1.longitude()}});

    // A refresh whose line name doesn't match ("M48" vs "U2") must not
    // touch the existing plan.
    const QVariantMap otherStopB = makeStop(QStringLiteral("Board"), b0, t0 + 999000, 999);
    const QVariantList otherLegs = {makeTransitLeg(otherStopB, stopC, {b0, b1}, QVariantList(), QStringLiteral("M48"))};
    const QVariantList refreshedItems = {makeTransitItinerary(otherLegs)};
    nav.applyDelayRefresh(true, refreshedItems);
    QCOMPARE(nav.departureDelay(), 0);
}

void TstDriver::transit_hurryWhenWalkIsLongerThanTimeToDeparture()
{
    const qint64 t0 = 8000000;
    const auto a0 = transitPointAt(0.07, 0), a1 = transitPointAt(0.07, 400);   // long walk: 400m at 1.4m/s ~ 286s
    const auto b0 = transitPointAt(0.071, 0), b1 = transitPointAt(0.071, 400);
    const QVariantMap stopA = makeStop(QStringLiteral("A"), a1, t0);
    // Departure only 30s after the walk starts: can't possibly make it on time.
    const QVariantMap stopB = makeStop(QStringLiteral("B"), b0, t0 + 30000);
    const QVariantMap stopC = makeStop(QStringLiteral("C"), b1, t0 + 230000);
    const QVariantList legs = {
        // An explicit 286s duration: the synthesized no-actions "arrive"
        // maneuver (what makeWalkLeg gives with no durationSecs) always
        // carries duration 0, which would make remainingSeconds 0 too and
        // never exceed secondsToDeparture.
        makeWalkLeg(stopA, stopB, {a0, a1}, 286),
        makeTransitLeg(stopB, stopC, {b0, b1}, QVariantList(), QStringLiteral("U2"))
    };
    Navigator nav(nullptr, nullptr);
    nav.setClockForTesting([&]() { return t0; });
    nav.start(makeTransitItinerary(legs), QVariantMap{{QStringLiteral("lat"), b1.latitude()}, {QStringLiteral("lng"), b1.longitude()}});
    nav.updatePosition(a0.latitude(), a0.longitude(), 5, 1.4, 90, t0);

    nav.updateTransitTick(t0);
    QVERIFY(nav.hurry());
}

void TstDriver::transit_offRouteOnWalkLegReroutesPedestrianToLegEnd()
{
    const qint64 t0 = 9000000;
    const auto a0 = transitPointAt(0.08, 0), a1 = transitPointAt(0.08, 1000);
    const QVariantMap stopA = makeStop(QStringLiteral("A"), a1, t0);
    const QVariantMap stopB = makeStop(QStringLiteral("B"), transitPointAt(0.081, 0), t0 + 100000);
    const QVariantList legs = {makeWalkLeg(stopA, stopB, {a0, a1})};
    Navigator nav(nullptr, nullptr);
    nav.setClockForTesting([&]() { return t0; });
    int requests = 0;
    nav.setRerouteRequesterForTesting([&](double, double, int) { ++requests; });
    nav.start(makeTransitItinerary(legs), QVariantMap{{QStringLiteral("lat"), stopB.value(QStringLiteral("lat")).toDouble()}});

    // 60m off a pedestrian leg (threshold 25m): off-route immediately, then
    // a reroute after 8 continuous seconds, exactly like car/bike off-route.
    constexpr double kEarthRadius = 6371000.0;
    constexpr double kDegPerMeterAtEquator = 180.0 / (M_PI * kEarthRadius);
    const QGeoCoordinate off(0.08 + 60 * kDegPerMeterAtEquator, 13.0);
    nav.updatePosition(off.latitude(), off.longitude(), 5, 1.4, 90, t0);
    QVERIFY(nav.offRoute());
    QCOMPARE(requests, 0);
    nav.updatePosition(off.latitude(), off.longitude(), 5, 1.4, 90, t0 + 9000);
    QCOMPARE(requests, 1);
}


namespace {

Sign mkSign(const QGeoCoordinate &c, int kind, int bearing, int maxspeed = 0)
{
    Sign s;
    s.latE7 = qint32(std::lround(c.latitude() * 1e7));
    s.lngE7 = qint32(std::lround(c.longitude() * 1e7));
    s.kind = quint8(kind);
    s.bearing = qint16(bearing);
    s.maxspeed = quint8(maxspeed);
    return s;
}

// Straight eastbound route along the equator (see onTestRoute), a point every 100 m.
QVariantList eastRoute(double lengthM)
{
    QVariantList l;
    for (double m = 0; m <= lengthM; m += 100)
        l.append(QVariant::fromValue(onTestRoute(m)));
    return l;
}

SignPacks *packsWith(const QVector<Sign> &signs)
{
    SignPacks *p = new SignPacks();
    if (!p->install(QStringLiteral("t"), SignPacks::buildPack(signs, 20250101))) {
        delete p;
        return nullptr;
    }
    return p;
}

// Drives east from `fromM` to `toM` in 10 m steps at 14 m/s, returning the
// "alertStarted" kinds in order.
QStringList driveEast(RoadAlerts &ra, double fromM, double toM, double headingDeg, qint64 t0 = 1000000)
{
    QStringList kinds;
    const QMetaObject::Connection conn =
        QObject::connect(&ra, &RoadAlerts::alertStarted, [&kinds](const QString &k) { kinds.append(k); });
    qint64 t = t0;
    for (double m = fromM; m <= toM; m += 10, t += 714) {
        const QGeoCoordinate c = onTestRoute(m);
        ra.updatePosition(c.latitude(), c.longitude(), 14.0, headingDeg, t);
    }
    QObject::disconnect(conn);
    return kinds;
}

}

void TstDriver::navgeo_angleDiffWrapsAround()
{
    QCOMPARE(NavGeo::angleDiff(350, 10), 20.0);
    QCOMPARE(NavGeo::angleDiff(10, 350), 20.0);
    QCOMPARE(NavGeo::angleDiff(90, 270), 180.0);
    QCOMPARE(NavGeo::angleDiff(45, 45), 0.0);
}

void TstDriver::navgeo_bearingDegreesCardinals()
{
    QVERIFY(qAbs(NavGeo::bearingDegrees(52, 13, 52.01, 13) - 0.0) < 0.5);
    QVERIFY(qAbs(NavGeo::bearingDegrees(52, 13, 52, 13.01) - 90.0) < 0.5);
    QVERIFY(qAbs(NavGeo::bearingDegrees(52, 13, 51.99, 13) - 180.0) < 0.5);
    QVERIFY(qAbs(NavGeo::bearingDegrees(52, 13, 52, 12.99) - 270.0) < 0.5);
}

void TstDriver::signpacks_roundTripKeepsRecords()
{
    QVector<Sign> in;
    in << mkSign(QGeoCoordinate(52.5, 13.4), Sign::Stop, 256)
       << mkSign(QGeoCoordinate(52.6, 13.5), Sign::Camera, -1, 50)
       << mkSign(QGeoCoordinate(-33.9, -70.7), Sign::GiveWay, 90);
    const QByteArray bytes = SignPacks::buildPack(in, 20250315);
    QCOMPARE(bytes.size(), 32 + 3 * 12);

    SignPacks::Header h;
    QVector<Sign> out;
    QVERIFY(SignPacks::parsePack(bytes, &h, &out));
    QCOMPARE(h.count, 3u);
    QCOMPARE(h.date, 20250315u);
    QCOMPARE(h.minLat, in[2].latE7);   // southern hemisphere handled
    QCOMPARE(h.maxLng, in[1].lngE7);
    QCOMPARE(out.size(), 3);
    QCOMPARE(out[0].latE7, in[0].latE7);
    QCOMPARE(out[0].bearing, qint16(256));
    QCOMPARE(out[0].kind, quint8(Sign::Stop));
    QCOMPARE(out[1].bearing, qint16(-1));
    QCOMPARE(out[1].maxspeed, quint8(50));
    QCOMPARE(out[2].lngE7, in[2].lngE7);   // negative longitude survives
    QCOMPARE(out[2].bearing, qint16(90));
}

void TstDriver::signpacks_rejectsDamagedFiles()
{
    QVector<Sign> in;
    in << mkSign(QGeoCoordinate(52.5, 13.4), Sign::Stop, 90);
    QByteArray good = SignPacks::buildPack(in, 20250101);
    QVERIFY(SignPacks::parsePack(good, nullptr, nullptr));
    QVERIFY(!SignPacks::parsePack(good.left(good.size() - 1), nullptr, nullptr));   // truncated
    QByteArray badMagic = good;
    badMagic[0] = 'X';
    QVERIFY(!SignPacks::parsePack(badMagic, nullptr, nullptr));
    QVERIFY(!SignPacks::parsePack(QByteArray("DRSP"), nullptr, nullptr));
    QVERIFY(!SignPacks::parsePack(QByteArray(), nullptr, nullptr));
}

void TstDriver::signpacks_nearAndCovers()
{
    QVector<Sign> in;
    in << mkSign(QGeoCoordinate(52.5000, 13.4000), Sign::Stop, 90)
       << mkSign(QGeoCoordinate(52.5030, 13.4000), Sign::GiveWay, 90)   // ~334 m north
       << mkSign(QGeoCoordinate(52.9000, 13.4000), Sign::Stop, 90);     // far away
    QScopedPointer<SignPacks> packs(packsWith(in));
    QVERIFY(!packs.isNull());
    QCOMPARE(packs->installedCount(), 1);
    QCOMPARE(packs->near(52.5, 13.4, 100).size(), 1);
    QCOMPARE(packs->near(52.5, 13.4, 400).size(), 2);
    QCOMPARE(packs->near(52.5, 13.4, 50000).size(), 3);
    QCOMPARE(packs->near(10.0, 10.0, 1000).size(), 0);
    QVERIFY(packs->covers(52.6, 13.4));
    QVERIFY(!packs->covers(40.0, -3.7));

    packs->remove(QStringLiteral("t"));
    QCOMPARE(packs->installedCount(), 0);
    QCOMPARE(packs->near(52.5, 13.4, 400).size(), 0);
}

void TstDriver::roadalerts_routeAlertFiresOnceAndClearsAfterPassing()
{
    QVector<Sign> in;
    in << mkSign(onTestRoute(1000), Sign::Stop, 90);
    QScopedPointer<SignPacks> packs(packsWith(in));
    RoadAlerts ra(nullptr, packs.data());
    ra.setKinds(true, false);
    ra.setActive(true);
    ra.setRoute(eastRoute(2000));

    QStringList started;
    QObject::connect(&ra, &RoadAlerts::alertStarted, [&started](const QString &k) { started.append(k); });
    qint64 t = 1000000;
    int lastDistance = -1;
    bool sawAlert = false;
    for (double m = 0; m <= 1300; m += 10, t += 714) {
        const QGeoCoordinate c = onTestRoute(m);
        ra.updatePosition(c.latitude(), c.longitude(), 14.0, 90.0, t);
        if (m < 700)
            QVERIFY2(ra.kind().isEmpty(), "must stay silent outside the lookahead");
        if (!ra.kind().isEmpty()) {
            sawAlert = true;
            QCOMPARE(ra.kind(), QStringLiteral("stop"));
            QVERIFY(lastDistance < 0 || ra.distance() <= lastDistance);   // counts down
            lastDistance = ra.distance();
        }
        if (m > 1020)
            QVERIFY2(ra.kind().isEmpty(), "must clear once the sign is passed");
    }
    QVERIFY(sawAlert);
    QCOMPARE(started, QStringList() << QStringLiteral("stop"));   // once, not per fix
}

void TstDriver::roadalerts_routeIgnoresOppositeDirectionAndSideStreets()
{
    constexpr double kDegPerM = 180.0 / (M_PI * 6371000.0);
    QVector<Sign> in;
    in << mkSign(onTestRoute(500), Sign::Stop, 270)                                  // other direction of travel
       << mkSign(QGeoCoordinate(25 * kDegPerM, onTestRoute(900).longitude()), Sign::Stop, 90)   // 25 m off the route
       << mkSign(onTestRoute(1300), Sign::GiveWay, 0)                                // cross street
       << mkSign(onTestRoute(1700), Sign::GiveWay, 90);                              // the real one
    QScopedPointer<SignPacks> packs(packsWith(in));
    RoadAlerts ra(nullptr, packs.data());
    ra.setKinds(true, true);
    ra.setActive(true);
    ra.setRoute(eastRoute(2000));
    const QStringList kinds = driveEast(ra, 0, 1900, 90);
    QCOMPARE(kinds, QStringList() << QStringLiteral("giveWay"));
}

void TstDriver::roadalerts_camerasOnlyWhenEnabled()
{
    QVector<Sign> in;
    in << mkSign(onTestRoute(800), Sign::Camera, -1, 50);
    QScopedPointer<SignPacks> packs(packsWith(in));
    RoadAlerts ra(nullptr, packs.data());
    ra.setActive(true);
    ra.setRoute(eastRoute(2000));

    ra.setKinds(true, false);
    QVERIFY(driveEast(ra, 0, 1000, 90).isEmpty());

    ra.setRoute(eastRoute(2000));
    ra.setKinds(true, true);
    QStringList kinds = driveEast(ra, 0, 1000, 90, 9000000);
    QCOMPARE(kinds, QStringList() << QStringLiteral("camera"));
    ra.setKinds(true, true);

    // The camera's speed limit is reported.
    ra.setRoute(eastRoute(2000));
    const QGeoCoordinate near = onTestRoute(600);
    ra.updatePosition(near.latitude(), near.longitude(), 14.0, 90.0, 20000000);
    QCOMPARE(ra.kind(), QStringLiteral("camera"));
    QCOMPARE(ra.maxspeed(), 50);
    ra.setKinds(true, false);   // switching it off drops the alert at once
    QVERIFY(ra.kind().isEmpty());
}

void TstDriver::roadalerts_freeDriveUsesHeadingCone()
{
    constexpr double kDegPerM = 180.0 / (M_PI * 6371000.0);
    QVector<Sign> in;
    in << mkSign(onTestRoute(600), Sign::Stop, 90)                                         // ahead, on the line
       << mkSign(QGeoCoordinate(40 * kDegPerM, onTestRoute(900).longitude()), Sign::Stop, 90)   // 40 m to the side
       << mkSign(onTestRoute(-300), Sign::GiveWay, 90)                                       // behind
       << mkSign(onTestRoute(1200), Sign::GiveWay, 270);                                     // ahead but for the other direction
    QScopedPointer<SignPacks> packs(packsWith(in));
    RoadAlerts ra(nullptr, packs.data());
    ra.setKinds(true, false);
    ra.setActive(true);   // no route set: heading cone
    // Heading unknown (-1): derived from the movement itself.
    const QStringList kinds = driveEast(ra, -250, 1400, -1);
    QCOMPARE(kinds, QStringList() << QStringLiteral("stop"));
}

void TstDriver::roadalerts_lookaheadGrowsWithSpeed()
{
    QVERIFY(RoadAlerts::lookahead(false, 5) == 120.0);       // floor
    QVERIFY(RoadAlerts::lookahead(false, 20) == 240.0);
    QVERIFY(RoadAlerts::lookahead(false, 50) == 300.0);      // cap
    QVERIFY(RoadAlerts::lookahead(true, 5) == 250.0);
    QVERIFY(RoadAlerts::lookahead(true, 30) == 450.0);
    QVERIFY(RoadAlerts::lookahead(true, 80) == 800.0);
    QVERIFY(RoadAlerts::lookahead(true, 30) > RoadAlerts::lookahead(false, 30));
}

void TstDriver::roadalerts_inactiveStaysSilentAndNoDataIsReported()
{
    QVector<Sign> in;
    in << mkSign(onTestRoute(500), Sign::Stop, 90);
    QScopedPointer<SignPacks> packs(packsWith(in));
    RoadAlerts ra(nullptr, packs.data());
    ra.setKinds(true, false);
    ra.setRoute(eastRoute(2000));
    QVERIFY(driveEast(ra, 0, 800, 90).isEmpty());   // not active

    ra.setActive(true);
    QSignalSpy noData(&ra, &RoadAlerts::noDataChanged);
    ra.updatePosition(40.0, -3.7, 14.0, 90.0, 5000);   // Madrid, no pack installed there
    QVERIFY(ra.noData());
    QCOMPARE(noData.count(), 1);
    ra.setActive(false);
    QVERIFY(!ra.noData());
}

void TstDriver::roadalerts_fixtureAlongRouteCar()
{
    QFile f(QStringLiteral(FIXTURE_DIR "/route_car.json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const QVariantList routes = ItineraryParser::parseRoutes(root, QStringLiteral("car"));
    QVERIFY(!routes.isEmpty());
    const QVariantList coords = routes.first().toMap().value(QStringLiteral("coordinates")).toList();
    QVERIFY(coords.size() >= 2);

    // Samples every 10 m along the polyline.
    QVector<QGeoCoordinate> track;
    for (int i = 0; i + 1 < coords.size(); ++i) {
        const QGeoCoordinate a = coords[i].value<QGeoCoordinate>(), b = coords[i + 1].value<QGeoCoordinate>();
        const double len = a.distanceTo(b);
        for (double d = 0; d < len; d += 10)
            track.append(a.atDistanceAndAzimuth(d, a.azimuthTo(b)));
    }
    QVERIFY(track.size() > 200);

    SignPacks packs;
    packs.setFixtureDir(QStringLiteral(FIXTURE_DIR));
    QCOMPARE(packs.installedCount(), 1);

    for (int withRoute = 0; withRoute < 2; ++withRoute) {
        for (int cameras = 0; cameras < 2; ++cameras) {
            RoadAlerts ra(nullptr, &packs);
            ra.setKinds(true, cameras == 1);
            ra.setActive(true);
            if (withRoute)
                ra.setRoute(coords);
            QStringList kinds;
            QObject::connect(&ra, &RoadAlerts::alertStarted, [&kinds](const QString &k) { kinds.append(k); });
            qint64 t = 1000000;
            for (const QGeoCoordinate &c : track) {
                ra.updatePosition(c.latitude(), c.longitude(), 14.0, withRoute ? 256.0 : -1.0, t);
                t += 714;
            }
            QStringList expected;
            expected << QStringLiteral("stop") << QStringLiteral("giveWay");
            if (cameras)
                expected << QStringLiteral("camera");
            QVERIFY2(kinds == expected, qPrintable(QStringLiteral("route=%1 cameras=%2 got %3")
                                                       .arg(withRoute).arg(cameras).arg(kinds.join(','))));
        }
    }
}


void TstDriver::tolls_parserListsCountriesAndAvoidSegments()
{
    QFile f(QStringLiteral(FIXTURE_DIR "/route_car.json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QVariantList routes = ItineraryParser::parseRoutes(QJsonDocument::fromJson(f.readAll()).object(), QStringLiteral("car"));
    const QVariantMap first = routes.first().toMap();
    QCOMPARE(first.value(QStringLiteral("tollCountries")).toStringList(),
             QStringList() << QStringLiteral("DEU") << QStringLiteral("POL"));
    const QVariantMap segs = first.value(QStringLiteral("tollSegments")).toMap();
    // The direction travelled is kept, the range is dropped, and a stretch
    // without a toll system (segment 1003) is not listed.
    QCOMPARE(segs.value(QStringLiteral("DEU")).toStringList(),
             QStringList() << QStringLiteral("here:cm:segment:1001#+") << QStringLiteral("here:cm:segment:1002#+"));
    QCOMPARE(segs.value(QStringLiteral("POL")).toStringList(), QStringList() << QStringLiteral("here:cm:segment:2001#-"));

    // The avoided variant has none.
    QFile g(QStringLiteral(FIXTURE_DIR "/route_car_avoid.json"));
    QVERIFY(g.open(QIODevice::ReadOnly));
    const QVariantList avoided = ItineraryParser::parseRoutes(QJsonDocument::fromJson(g.readAll()).object(), QStringLiteral("car"));
    QVERIFY(avoided.first().toMap().value(QStringLiteral("tollCountries")).toStringList().isEmpty());
}

void TstDriver::tolls_urlAndBody()
{
    const HereUrls::LatLng a{52.5, 13.4}, b{48.1, 11.5};
    const QUrl plain = HereUrls::route(QStringLiteral("K"), QStringLiteral("car"), a, b, QStringLiteral("en"),
                                       QStringLiteral("metric"), 2);
    QVERIFY(!plain.toString().contains(QStringLiteral("tolls")));
    QVERIFY(!plain.toString().contains(QStringLiteral("segmentRef")));
    const QUrl tolls = HereUrls::route(QStringLiteral("K"), QStringLiteral("car"), a, b, QStringLiteral("en"),
                                       QStringLiteral("metric"), 2, -1, {}, QString(), true);
    const QUrlQuery q(tolls);
    QVERIFY(q.queryItemValue(QStringLiteral("return")).endsWith(QStringLiteral(",tolls")));
    QVERIFY(q.queryItemValue(QStringLiteral("spans")).contains(QStringLiteral("tollSystems")));
    // Not for walking.
    const QUrl walk = HereUrls::route(QStringLiteral("K"), QStringLiteral("pedestrian"), a, b, QStringLiteral("en"),
                                      QStringLiteral("metric"), 0, -1, {}, QString(), true);
    QVERIFY(!walk.toString().contains(QStringLiteral("tolls")));

    const QJsonObject body = QJsonDocument::fromJson(
        HereUrls::avoidSegmentsBody(QStringList() << QStringLiteral("here:cm:segment:1#+") << QStringLiteral("here:cm:segment:2#-"))).object();
    QCOMPARE(body.value(QStringLiteral("avoid")).toObject().value(QStringLiteral("segments")).toArray().size(), 2);
}

void TstDriver::speedlimit_parserReadsHereSpans()
{
    QFile f(QStringLiteral(FIXTURE_DIR "/route_car.json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QVariantList routes = ItineraryParser::parseRoutes(QJsonDocument::fromJson(f.readAll()).object(), QStringLiteral("car"));
    const QVariantList limits = routes.first().toMap().value(QStringLiteral("speedLimits")).toList();
    QCOMPARE(limits.size(), 2);   // 50 km/h then 30 km/h
    QCOMPARE(limits[0].toMap().value(QStringLiteral("kmh")).toInt(), 50);
    QCOMPARE(limits[0].toMap().value(QStringLiteral("start")).toInt(), 0);
    QCOMPARE(limits[1].toMap().value(QStringLiteral("kmh")).toInt(), 30);
    QCOMPARE(limits[0].toMap().value(QStringLiteral("end")).toInt(), limits[1].toMap().value(QStringLiteral("start")).toInt());
}

void TstDriver::speedlimit_storeEncodeLoadLookup()
{
    SpeedLimitStore::LimitWay a, oneway;
    a.limit = 50;
    a.lat << 52.5000 << 52.5000;
    a.lng << 13.4010 << 13.4100;   // eastbound road
    oneway.limit = 30;
    oneway.flags = 1;               // forward travel only
    oneway.lat << 52.5100 << 52.5100;
    oneway.lng << 13.4010 << 13.4100;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile out(dir.path() + QStringLiteral("/t.drsl"));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(SpeedLimitStore::encode(QVector<SpeedLimitStore::LimitWay>() << a << oneway, 20250101));
    out.close();

    SpeedLimitStore store;
    store.load(dir.path());
    QVERIFY(!store.isEmpty());
    QCOMPARE(store.lookup(52.5000, 13.4050, 90), 50);
    QCOMPARE(store.lookup(52.5000, 13.4050, 270), 50);    // either way along a two-way road
    QCOMPARE(store.lookup(52.5000, 13.4050, -1), 50);     // heading unknown
    QCOMPARE(store.lookup(52.5000, 13.4050, 0), 0);       // across the road
    QCOMPARE(store.lookup(52.50040, 13.4050, 90), 0);     // ~45 m off the road
    QCOMPARE(store.lookup(52.5100, 13.4050, 90), 30);
    QCOMPARE(store.lookup(52.5100, 13.4050, 270), 0);     // against a one-way road
    QCOMPARE(store.lookup(48.0, 2.0, 90), 0);             // no data there
}

void TstDriver::speedlimit_storeCrossesTileBorders()
{
    // Filed under the tile of its first point (13.3 - 13.4 degrees) but also
    // passing through the next one.
    SpeedLimitStore::LimitWay w;
    w.limit = 70;
    w.lat << 52.5000 << 52.5000;
    w.lng << 13.3990 << 13.4100;
    QTemporaryDir dir;
    QFile out(dir.path() + QStringLiteral("/t.drsl"));
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(SpeedLimitStore::encode(QVector<SpeedLimitStore::LimitWay>() << w, 20250101));
    out.close();
    SpeedLimitStore store;
    store.load(dir.path());
    QCOMPARE(store.lookup(52.5000, 13.3995, 90), 70);   // in the first tile
    QCOMPARE(store.lookup(52.5000, 13.4005, 90), 70);   // just past the border
}

void TstDriver::roadalerts_speedLimitFromRouteThenRoadData()
{
    QScopedPointer<SignPacks> packs(packsWith(QVector<Sign>() << mkSign(onTestRoute(5000), Sign::Stop, 90)));
    QVERIFY(packs->limitStore()->loadJson(QByteArray(
        R"({"ways":[{"limit":80,"flags":0,"points":[[0.0, 13.0],[0.0, 13.05]]}]})")));

    RoadAlerts ra(nullptr, packs.data());
    ra.setKinds(false, false);   // limits don't depend on the warning settings
    ra.setActive(true);
    QSignalSpy changed(&ra, &RoadAlerts::speedLimitChanged);

    // On a route: HERE's limits, in point ranges (every 100 m here).
    ra.setRoute(eastRoute(2000));
    ra.setRouteLimits(QVariantList() << QVariantMap{{"start", 0}, {"end", 10}, {"kmh", 50}}
                                     << QVariantMap{{"start", 10}, {"end", 20}, {"kmh", 30}});
    qint64 t = 1000000;
    auto at = [&](double m) {
        const QGeoCoordinate c = onTestRoute(m);
        ra.updatePosition(c.latitude(), c.longitude(), 14.0, 90.0, t);
        t += 1000;
    };
    at(300);
    QCOMPARE(ra.speedLimit(), 50);
    at(800);
    at(1300);
    at(1500);
    QCOMPARE(ra.speedLimit(), 30);

    // Off the route (free drive): the downloaded road data, held for a few
    // seconds when it has a gap, then dropped.
    ra.clearRoute();
    at(1600);
    QCOMPARE(ra.speedLimit(), 80);
    const QGeoCoordinate far = onTestRoute(9000000);   // nowhere near the way
    ra.updatePosition(far.latitude(), far.longitude(), 14.0, 90.0, t + 2000);
    QCOMPARE(ra.speedLimit(), 80);
    ra.updatePosition(far.latitude(), far.longitude(), 14.0, 90.0, t + 12000);
    QCOMPARE(ra.speedLimit(), 0);
    ra.setActive(false);
    QVERIFY(changed.count() >= 3);
}

QTEST_GUILESS_MAIN(TstDriver)
#include "tst_driver.moc"
