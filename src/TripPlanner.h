#ifndef TRIPPLANNER_H
#define TRIPPLANNER_H

#include <QNetworkReply>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

class AppSettings;
class HereClient;

// Plans one A→B trip in all four modes at once (4 HERE requests) and keeps the results.
class TripPlanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(QVariantMap to READ to WRITE setTo NOTIFY toChanged)
    Q_PROPERTY(QVariantMap results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap status READ status NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap errors READ errors NOTIFY resultsChanged)
    Q_PROPERTY(QString selectedMode READ selectedMode WRITE setSelectedMode NOTIFY selectionChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY selectionChanged)
    // Countries with toll roads on the planned car route, with whether the
    // user chose to avoid their tolls: [{code, avoid}].
    Q_PROPERTY(QVariantList tollCountries READ tollCountries NOTIFY tollsChanged)

public:
    TripPlanner(AppSettings *settings, HereClient *client, QObject *parent = nullptr);

    QVariantMap from() const { return m_from; }
    void setFrom(const QVariantMap &p) { m_from = p; resetTolls(); emit fromChanged(); }
    QVariantMap to() const { return m_to; }
    void setTo(const QVariantMap &p) { m_to = p; resetTolls(); emit toChanged(); }
    QVariantMap results() const { return m_results; }
    QVariantMap status() const { return m_status; }
    QVariantMap errors() const { return m_errors; }
    QString selectedMode() const { return m_mode; }
    void setSelectedMode(const QString &m);
    int selectedIndex() const { return m_index; }
    void setSelectedIndex(int i);
    QVariantMap selected() const;

    QVariantList tollCountries() const;
    // Plan the car route again, avoiding (or no longer avoiding) the toll
    // roads of one country (ISO 3166 alpha-3 code, as HERE reports it).
    Q_INVOKABLE void setAvoidToll(const QString &country, bool avoid);
    Q_INVOKABLE void plan();
    Q_INVOKABLE void swap();
    Q_INVOKABLE void clear();

signals:
    void fromChanged();
    void toChanged();
    void resultsChanged();
    void selectionChanged();
    void tollsChanged();

private:
    void request(const QString &mode);
    void requestCar(int gen, int carGen, int round);
    void resetTolls();
    void noteTolls(const QVariantList &items);
    void requestBicycle();
    void bicyclePartDone(int gen, const QString &variant, bool ok, const QVariantList &items, const QString &error);
    void finish(const QString &mode, bool ok, const QVariantList &items, const QString &error);

    AppSettings *m_settings;
    HereClient *m_client;
    QVariantMap m_from, m_to, m_results, m_status, m_errors;
    QString m_mode = QStringLiteral("car");
    int m_index = 0;
    int m_generation = 0;
    int m_carGen = 0;
    QStringList m_tollOrder;                 // countries with tolls on the first plan
    QStringList m_avoidTolls;                // countries whose tolls to avoid
    QHash<QString, QStringList> m_tollSegments;   // country -> segments to avoid
    QList<QPointer<QNetworkReply>> m_pending;
    QHash<QString, QVariantMap> m_bikeParts;   // variant -> itinerary, as each arrives
    int m_bikeOutstanding = 0;
    QString m_bikeError;
};

#endif
