#ifndef ROADALERTS_H
#define ROADALERTS_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

class AppSettings;
class SignPacks;

// Warns about stop signs, give-way signs and (optionally) speed cameras ahead,
// using the offline sign data in SignPacks. Works along the navigation route
// when there is one, and otherwise (free drive) along the direction the car is
// heading. QML feeds it every usable GPS fix and shows `kind`/`distance`.
class RoadAlerts : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    // "" | "stop" | "giveWay" | "camera"
    Q_PROPERTY(QString kind READ kind NOTIFY alertChanged)
    Q_PROPERTY(int distance READ distance NOTIFY alertChanged)   // metres to the sign
    Q_PROPERTY(int maxspeed READ maxspeed NOTIFY alertChanged)   // km/h, cameras, 0 = unknown
    // Warnings are on but no downloaded country covers the current position.
    Q_PROPERTY(bool noData READ noData NOTIFY noDataChanged)
    // Posted limit (km/h) of the road being driven, 0 = unknown. From the HERE
    // route while navigating, else from the downloaded OpenStreetMap limits.
    Q_PROPERTY(int speedLimit READ speedLimit NOTIFY speedLimitChanged)

public:
    RoadAlerts(AppSettings *settings, SignPacks *packs, QObject *parent = nullptr);

    bool active() const { return m_active; }
    void setActive(bool on);
    QString kind() const { return m_kind; }
    int distance() const { return m_distance; }
    int maxspeed() const { return m_maxspeed; }
    bool noData() const { return m_noData; }
    int speedLimit() const { return m_limit; }

    Q_INVOKABLE void setRoute(const QVariantList &coordinates);
    // [{start, end, kmh}] point ranges of the route just set (HERE speed limits).
    Q_INVOKABLE void setRouteLimits(const QVariantList &limits);
    Q_INVOKABLE void clearRoute();
    // Pass speedMps/headingDeg < 0 when unknown; they are derived from the
    // previous fix. timestampMs < 0 means "now".
    Q_INVOKABLE void updatePosition(double lat, double lng, double speedMps, double headingDeg,
                                    qint64 timestampMs = -1);

    // Overrides what the settings say (tests).
    void setKinds(bool signs, bool cameras);

    // How far ahead (m) a sign or camera is announced at the given speed (m/s).
    static double lookahead(bool camera, double speedMps);

signals:
    void activeChanged();
    void alertChanged();
    void noDataChanged();
    void speedLimitChanged();
    // Fired once per sign, when it first becomes the current alert.
    void alertStarted(const QString &kind);

private:
    void readSettings();
    void clearAlert();
    void setNoData(bool v);
    void setLimit(int kmh);
    void updateLimit(double lat, double lng, bool onRoute, qint64 ts);
    double matchRoute(double lat, double lng);
    bool alongRoute(double lat, double lng, double tolerance, double maxAhead, double signBearing,
                    double *distanceAhead) const;

    AppSettings *m_settings;
    SignPacks *m_packs;
    bool m_active = false;
    bool m_signsOn = true;
    bool m_camerasOn = false;
    bool m_noData = false;

    QVector<QPair<double, double>> m_coords;
    QVector<double> m_cum;
    struct LimitSpan { double from, to; int kmh; };
    QVector<LimitSpan> m_limitSpans;
    int m_limit = 0;
    qint64 m_limitTs = 0;
    double m_traveled = 0;

    bool m_havePrev = false;
    double m_prevLat = 0, m_prevLng = 0;
    qint64 m_prevTs = 0;
    bool m_haveRef = false;
    double m_refLat = 0, m_refLng = 0;
    double m_heading = -1;

    QString m_kind;
    int m_distance = 0;
    int m_maxspeed = 0;
    qint64 m_curKey = 0;
    int m_misses = 0;
    QHash<qint64, qint64> m_alerted;   // sign key -> time of its last alert
};

#endif
