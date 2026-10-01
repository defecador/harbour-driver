#ifndef NAVIGATOR_H
#define NAVIGATOR_H

#include <QObject>
#include <QPair>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <functional>

class AppSettings;
class HereClient;

// Turn-by-turn guidance for one active trip: matches GPS fixes to the planned
// route, tracks progress and the upcoming maneuver, fires announcements, and
// reroutes when the position strays off the route for too long.
//
// Car, walk and bike only (the modes that have a single, flat list of
// "actions" with a length/duration each). Transit is a separate leg-by-leg
// state machine (see the Milestone 2 plan) and is not handled here.
//
// All distances are in metres, computed with a flat-earth (equirectangular)
// approximation, which the project's plan calls accurate enough at city
// scale. Times are in seconds unless the name says milliseconds.
class Navigator : public QObject
{
    Q_OBJECT
    // Lets tst_matka.cpp drive the normally-timer-only tick and the
    // normally-network-only delay-refresh callback directly, with fake
    // timestamps/results, instead of needing a real QTimer/HereClient.
    friend class TstMatka;
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY stateChanged)

    Q_PROPERTY(QString maneuverAction READ maneuverAction NOTIFY maneuverChanged)
    Q_PROPERTY(QString maneuverDirection READ maneuverDirection NOTIFY maneuverChanged)
    Q_PROPERTY(QString instruction READ instruction NOTIFY maneuverChanged)
    Q_PROPERTY(qreal distanceToManeuver READ distanceToManeuver NOTIFY maneuverChanged)

    Q_PROPERTY(bool hasThen READ hasThen NOTIFY maneuverChanged)
    Q_PROPERTY(QString thenAction READ thenAction NOTIFY maneuverChanged)
    Q_PROPERTY(QString thenDirection READ thenDirection NOTIFY maneuverChanged)

    Q_PROPERTY(qreal remainingDistance READ remainingDistance NOTIFY maneuverChanged)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY maneuverChanged)
    Q_PROPERTY(QString arrivalText READ arrivalText NOTIFY maneuverChanged)
    // 0..1 how far along the total route length we've travelled; QML uses
    // this only to approximate which prefix of `coordinates` to grey out,
    // not for anything timing- or distance-critical.
    Q_PROPERTY(qreal traveledFraction READ traveledFraction NOTIFY maneuverChanged)

    Q_PROPERTY(bool offRoute READ offRoute NOTIFY stateChanged)
    Q_PROPERTY(bool rerouting READ rerouting NOTIFY stateChanged)
    Q_PROPERTY(int rerouteCount READ rerouteCount NOTIFY stateChanged)

    // --- Transit (mode == "transit" only) ------------------------------
    // transitPhase: "walk" | "wait" | "ride" | "alight" — which leg-local
    // state the UI should show. Empty when mode isn't "transit".
    Q_PROPERTY(QString transitPhase READ transitPhase NOTIFY transitChanged)
    Q_PROPERTY(int legIndex READ legIndex NOTIFY transitChanged)
    Q_PROPERTY(int legCount READ legCount NOTIFY transitChanged)
    Q_PROPERTY(QString transportMode READ transportMode NOTIFY transitChanged)
    Q_PROPERTY(QString lineName READ lineName NOTIFY transitChanged)
    Q_PROPERTY(QString headsign READ headsign NOTIFY transitChanged)
    Q_PROPERTY(QString lineColor READ lineColor NOTIFY transitChanged)
    Q_PROPERTY(QString lineTextColor READ lineTextColor NOTIFY transitChanged)
    Q_PROPERTY(QString boardStopName READ boardStopName NOTIFY transitChanged)
    Q_PROPERTY(QString alightStopName READ alightStopName NOTIFY transitChanged)
    Q_PROPERTY(QString platform READ platform NOTIFY transitChanged)
    Q_PROPERTY(QString departureText READ departureText NOTIFY transitChanged)
    Q_PROPERTY(int departureDelay READ departureDelay NOTIFY transitChanged)
    Q_PROPERTY(int secondsToDeparture READ secondsToDeparture NOTIFY transitChanged)
    Q_PROPERTY(int stopsRemaining READ stopsRemaining NOTIFY transitChanged)
    Q_PROPERTY(QString nextStopName READ nextStopName NOTIFY transitChanged)
    Q_PROPERTY(bool missedConnection READ missedConnection NOTIFY transitChanged)
    Q_PROPERTY(bool hurry READ hurry NOTIFY transitChanged)
    // Walk/bike only: stops the user added along the way, nearest first. Each
    // is a place map (title, lat, lng, ...).
    Q_PROPERTY(QVariantList stops READ stops NOTIFY stopsChanged)
    Q_PROPERTY(int stopCount READ stopCount NOTIFY stopsChanged)

public:
    Navigator(AppSettings *settings, HereClient *client, QObject *parent = nullptr);

    // Test seam: overrides the wall clock updatePosition() uses when no
    // explicit timestampMs is given, and the network request a reroute makes.
    // Production code (main.cpp) never calls these; the defaults are real.
    void setClockForTesting(std::function<qint64()> clock);
    void setRerouteRequesterForTesting(std::function<void(double lat, double lng, int courseDeg)> fn);

    QString state() const { return m_state; }
    QString mode() const { return m_mode; }

    QString maneuverAction() const;
    QString maneuverDirection() const;
    QString instruction() const;
    qreal distanceToManeuver() const { return m_distanceToManeuver; }

    bool hasThen() const { return m_hasThen; }
    QString thenAction() const;
    QString thenDirection() const;

    qreal remainingDistance() const { return m_remainingDistance; }
    int remainingSeconds() const
    {
        if (m_mode == QStringLiteral("transit")) {
            if (m_legs.isEmpty() || m_legs.last().toExpectedMs <= 0)
                return 0;
            return int(qMax(qint64(0), (m_legs.last().toExpectedMs - now()) / 1000));
        }
        return m_remainingSeconds;
    }
    QString arrivalText() const;
    qreal traveledFraction() const { return m_totalLength > 0 ? qBound(0.0, m_traveled / m_totalLength, 1.0) : 0.0; }

    bool offRoute() const { return m_offRoute; }
    bool rerouting() const { return m_rerouting; }
    int rerouteCount() const { return m_rerouteCount; }

    Q_INVOKABLE void start(const QVariantMap &itinerary, const QVariantMap &destination);
    Q_INVOKABLE void stop();

    // timestampMs < 0 (the default) means "now"; tests pass explicit values
    // so off-route/announcement timing doesn't depend on real wall-clock time.
    Q_INVOKABLE void updatePosition(double lat, double lng, double accuracyM,
                                     double speedMps, double headingDeg, qint64 timestampMs = -1);

    // Manual retry once the per-trip reroute limit is hit.
    Q_INVOKABLE void retryReroute();

    // Transit-only: requests one fresh transit plan from the current position
    // to the destination and restarts guidance from its first leg. Shares the
    // reroute counter/limit with car/walk/bike rerouting, but has no cooldown
    // since it's only ever user-triggered (the "Find next connection" button).
    Q_INVOKABLE void replanTransit();

    // Walk/bike only: adds `place` as the next stop and reroutes through it
    // from the current position (one routing request). Does nothing in other
    // modes or while not guiding. On failure the stop is dropped again.
    Q_INVOKABLE void addStop(const QVariantMap &place);
    QVariantList stops() const;
    int stopCount() const { return m_stops.size(); }

    // Test/dev seam: when `on`, now() maps real elapsed wall-clock time (sped
    // up by `speedFactor`) onto the itinerary's own first-leg departure time,
    // so a fixture trip whose timestamps are in the past (or far in the
    // future) still drives wait/ride countdowns sensibly, and a long real
    // trip can be played back quickly. Off by default (real wall clock).
    Q_INVOKABLE void setSimulationClock(bool on, qreal speedFactor = 4.0);

    QString transitPhase() const { return m_transitPhase; }
    int legIndex() const { return m_legIndex; }
    int legCount() const { return m_legs.size(); }
    QString transportMode() const;
    QString lineName() const;
    QString headsign() const;
    QString lineColor() const;
    QString lineTextColor() const;
    QString boardStopName() const;
    QString alightStopName() const;
    QString platform() const;
    QString departureText() const;
    int departureDelay() const;
    int secondsToDeparture() const;
    int stopsRemaining() const { return m_stopsRemaining; }
    QString nextStopName() const;
    bool missedConnection() const { return m_missedConnection; }
    bool hurry() const { return m_hurry; }

signals:
    void stateChanged();
    void maneuverChanged();
    void transitChanged();
    void stopsChanged();
    // kind: "far" | "near" | "now" | "offroute" | "rerouting" | "rerouted" | "arrived"
    // | "alight" | "alightNow" | "missed" | "boarded" | "stopAdded" | "stopFailed" | "stopReached"
    void announce(const QString &text, const QString &kind);
    void arrived();
    void rerouted(const QVariantMap &newItinerary);

private:
    struct Maneuver {
        QString action;
        QString direction;
        QString instruction;
        int duration = 0;      // seconds of the segment that FOLLOWS this maneuver
        qreal distanceAlong = 0;  // metres from the route start to this maneuver
    };

    void rebuildFromItinerary(const QVariantMap &itinerary);
    void resetProgressState();
    qint64 now() const;
    void advanceStepPointer();
    void checkStopsReached(double lat, double lng);
    void dropFailedStop();
    void updateDerivedProperties();
    void checkAnnouncements(qint64 tsMs);
    void checkOffRoute(qreal perpDistance, qint64 tsMs);
    qreal matchPositionToRoute(double lat, double lng);
    void maybeReroute(double lat, double lng, double headingDeg, qint64 tsMs, bool manual = false);
    void applyRerouteResult(bool ok, const QVariantList &items);

    qreal offRouteThreshold(qreal accuracyM) const;

    // --- Transit (NavigatorTransit.cpp) --------------------------------
    struct TransitStop {
        QString name;
        double lat = 0, lng = 0;
        qint64 expectedMs = -1;
        qreal distanceAlong = -1;   // set once the leg's coords are loaded; -1 until matched
        bool passed = false;
    };
    struct TransitLeg {
        QString type;               // "pedestrian" | "transit"
        QString transportMode, lineName, headsign, lineColor, lineTextColor, agency;
        QString fromName, toName;
        qint64 fromExpectedMs = -1, toExpectedMs = -1;
        int fromDelay = -1, toDelay = -1;
        QString fromPlatform;
        double fromLat = 0, fromLng = 0, toLat = 0, toLng = 0;
        QVector<QPair<double, double>> coords;
        QVector<qreal> cumDist;
        QVector<Maneuver> maneuvers;   // walking legs only, leg-local offsets
        QVector<TransitStop> stops;    // transit legs only, intermediate stops
        qreal totalLength = 0;
    };

    void rebuildTransitFromItinerary(const QVariantMap &itinerary);
    TransitLeg parseTransitLeg(const QVariantMap &legMap) const;
    void loadWalkLegIntoRouteState(const TransitLeg &leg);
    void enterLeg(int index);
    void updateTransitPosition(double lat, double lng, double accuracyM, double speedMps, qint64 tsMs);
    void updateTransitTick(qint64 tsMs);
    void checkMissedConnection(qint64 tsMs);
    void maybeRefreshDelay(double lat, double lng, qint64 tsMs);
    void applyDelayRefresh(bool ok, const QVariantList &items);
    void maybeRerouteWalkLeg(double lat, double lng, qint64 tsMs);
    void emitTransitArrived();

    QVector<TransitLeg> m_legs;
    int m_legIndex = 0;
    QString m_transitPhase;
    int m_stopsRemaining = 0;
    bool m_missedConnection = false;
    bool m_hurry = false;
    bool m_refreshedThisLeg = false;
    bool m_announcedAlight = false;
    bool m_announcedAlightNow = false;
    qreal m_curLegDistanceAlong = 0;   // progress (m) into the current leg's own polyline
    int m_transitRefreshGeneration = 0;
    double m_lastTransitLat = 0, m_lastTransitLng = 0;
    bool m_hasTransitFix = false;

    // Simulation clock: when active, now() is m_simBaseMs + (wall-elapsed * factor).
    bool m_simClockOn = false;
    qreal m_simSpeedFactor = 4.0;
    qint64 m_simBaseMs = 0;
    qint64 m_simWallStartMs = 0;

    // Drives wait/ride countdowns and timetable-based stop counting forward
    // even when no GPS fix arrives (e.g. underground). Only active while
    // mode == "transit" and state == "guiding".
    QTimer m_tickTimer;

    AppSettings *m_settings;
    HereClient *m_client;
    std::function<qint64()> m_clock;
    std::function<void(double, double, int)> m_requester;

    QString m_state = QStringLiteral("idle");
    QString m_mode;
    QVariantMap m_destination;
    QVariantMap m_itinerary;
    QVector<QVariantMap> m_stops;       // pending user stops, in visiting order
    QString m_variant;                  // bicycle: "fast" | "short" | "green" (route flavour to keep on reroutes)
    QVector<int> m_stopManeuvers;       // maneuver index of each pending stop's arrival
    int m_addedStopIndex = 0;           // where the stop being added sits in m_stops
    bool m_addingStop = false;          // the reroute in flight is for a newly added stop

    QVector<QPair<double, double>> m_coords;   // lat,lng
    QVector<qreal> m_cumDist;                  // cumulative metres per point, same size as m_coords
    QVector<Maneuver> m_maneuvers;              // one per "action", in route order
    qreal m_totalLength = 0;

    int m_stepIndex = 1;        // index into m_maneuvers of the upcoming maneuver
    int m_lastMatchPoint = 0;   // last matched segment start index, for windowed search
    qreal m_traveled = 0;       // metres along the route

    double m_lastLat = 0, m_lastLng = 0, m_lastHeading = 0, m_lastSpeed = 0, m_lastAccuracy = 0;

    qreal m_distanceToManeuver = 0;
    bool m_hasThen = false;
    qreal m_remainingDistance = 0;
    int m_remainingSeconds = 0;

    bool m_offRoute = false;
    qint64 m_offRouteSinceMs = -1;
    bool m_rerouting = false;
    int m_rerouteCount = 0;
    qint64 m_lastRerouteMs = -1;
    int m_rerouteGeneration = 0;

    QVector<bool> m_announcedFar, m_announcedNear, m_announcedNow;
};

#endif
