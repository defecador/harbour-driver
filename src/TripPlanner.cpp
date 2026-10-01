#include "TripPlanner.h"
#include "AppSettings.h"
#include "HereClient.h"
#include "HereUrls.h"
#include "ItineraryParser.h"

namespace {

HereUrls::LatLng navPoint(const QVariantMap &p)
{
    const double lat = p.value(QStringLiteral("navLat"), p.value(QStringLiteral("lat"))).toDouble();
    const double lng = p.value(QStringLiteral("navLng"), p.value(QStringLiteral("lng"))).toDouble();
    return HereUrls::LatLng{lat, lng};
}

const QStringList kModes = {QStringLiteral("car"), QStringLiteral("transit"),
                            QStringLiteral("pedestrian"), QStringLiteral("bicycle")};

}

TripPlanner::TripPlanner(AppSettings *settings, HereClient *client, QObject *parent)
    : QObject(parent), m_settings(settings), m_client(client)
{
}

void TripPlanner::setSelectedMode(const QString &m)
{
    if (m == m_mode) return;
    m_mode = m;
    m_index = 0;
    emit selectionChanged();
}

void TripPlanner::setSelectedIndex(int i)
{
    if (i == m_index) return;
    m_index = i;
    emit selectionChanged();
}

QVariantMap TripPlanner::selected() const
{
    const QVariantList list = m_results.value(m_mode).toList();
    return (m_index >= 0 && m_index < list.size()) ? list.at(m_index).toMap() : QVariantMap();
}

void TripPlanner::clear()
{
    ++m_generation;
    for (auto &r : m_pending)
        if (r) r->abort();
    m_pending.clear();
    m_results.clear();
    m_status.clear();
    m_errors.clear();
    m_index = 0;
    emit resultsChanged();
    emit selectionChanged();
}

void TripPlanner::swap()
{
    std::swap(m_from, m_to);
    emit fromChanged();
    emit toChanged();
    plan();
}

void TripPlanner::plan()
{
    clear();
    if (m_from.isEmpty() || m_to.isEmpty())
        return;
    for (const QString &mode : kModes) {
        if (!m_client->fixtureMode() && !m_settings->hasHereKey()) {
            m_status[mode] = QStringLiteral("error");
            m_errors[mode] = tr("Add your HERE API key in Settings to plan routes.");
            continue;
        }
        m_status[mode] = QStringLiteral("loading");
        request(mode);
    }
    emit resultsChanged();
}

void TripPlanner::request(const QString &mode)
{
    if (mode == QLatin1String("bicycle")) {
        requestBicycle();
        return;
    }
    const int gen = m_generation;
    const QString key = m_settings->hereApiKey();
    const QString lang = m_settings->lang();
    const QString units = m_settings->units();
    const auto from = navPoint(m_from), to = navPoint(m_to);

    QUrl url;
    QString fixture;
    if (mode == QLatin1String("transit")) {
        url = HereUrls::transit(key, from, to, lang, units, 3);
        fixture = QStringLiteral("transit");
    } else {
        url = HereUrls::route(key, mode, from, to, lang, units, mode == QLatin1String("car") ? 2 : 0);
        fixture = QStringLiteral("route_") + mode;
    }
    m_pending.append(m_client->get(url, fixture, [this, gen, mode](bool ok, const QJsonObject &json, const QString &err) {
        if (gen != m_generation)
            return;
        const QVariantList items = !ok ? QVariantList()
                : mode == QLatin1String("transit") ? ItineraryParser::parseTransit(json)
                                                   : ItineraryParser::parseRoutes(json, mode);
        finish(mode, ok, items, err);
    }));
}

// Cycling offers three routes: fastest and shortest from HERE, greenest from
// Valhalla (OpenStreetMap), which can prefer bike paths and quiet streets.
// They arrive independently and the list fills in as they do.
void TripPlanner::requestBicycle()
{
    const int gen = m_generation;
    const QString key = m_settings->hereApiKey();
    const QString lang = m_settings->lang();
    const QString units = m_settings->units();
    const auto from = navPoint(m_from), to = navPoint(m_to);
    m_bikeParts.clear();
    m_bikeError.clear();
    m_bikeOutstanding = 3;

    struct Variant { const char *name; QUrl url; const char *fixture; bool valhalla; };
    const QVector<Variant> variants = {
        {"fast", HereUrls::route(key, QStringLiteral("bicycle"), from, to, lang, units, 0, -1, {}, QStringLiteral("fast")),
         "route_bicycle", false},
        {"short", HereUrls::route(key, QStringLiteral("bicycle"), from, to, lang, units, 0, -1, {}, QStringLiteral("short")),
         "route_bicycle_short", false},
        {"green", HereUrls::valhallaBicycle(from, to, {}, lang, units), "route_bicycle_green", true},
    };
    for (const Variant &v : variants) {
        const QString name = QString::fromLatin1(v.name);
        const bool valhalla = v.valhalla;
        m_pending.append(m_client->get(v.url, QString::fromLatin1(v.fixture),
                [this, gen, name, valhalla](bool ok, const QJsonObject &json, const QString &err) {
            if (gen != m_generation)
                return;
            QVariantList items;
            if (ok) {
                items = valhalla ? ItineraryParser::parseValhalla(json, QStringLiteral("bicycle"))
                                 : ItineraryParser::parseRoutes(json, QStringLiteral("bicycle"));
            }
            bicyclePartDone(gen, name, ok, items, err);
        }));
    }
}

void TripPlanner::bicyclePartDone(int gen, const QString &variant, bool ok, const QVariantList &items, const QString &error)
{
    if (gen != m_generation)
        return;
    --m_bikeOutstanding;
    if (ok && !items.isEmpty()) {
        QVariantMap it = items.first().toMap();
        it[QStringLiteral("variant")] = variant;
        m_bikeParts[variant] = it;
    } else if (m_bikeError.isEmpty()) {
        m_bikeError = error;
    }

    // Fixed order: fastest first (the mode tab shows its time), then shortest, then greenest.
    QVariantList list;
    for (const QString &v : {QStringLiteral("fast"), QStringLiteral("short"), QStringLiteral("green")}) {
        if (!m_bikeParts.contains(v))
            continue;
        const QVariantMap it = m_bikeParts.value(v);
        // Skip a route identical to one already listed.
        bool duplicate = false;
        for (const QVariant &e : list) {
            const QVariantMap o = e.toMap();
            duplicate = duplicate || (o.value(QStringLiteral("length")) == it.value(QStringLiteral("length"))
                                      && o.value(QStringLiteral("duration")) == it.value(QStringLiteral("duration")));
        }
        if (!duplicate)
            list.append(it);
    }

    if (!list.isEmpty())
        finish(QStringLiteral("bicycle"), true, list, QString());
    else if (m_bikeOutstanding == 0)
        finish(QStringLiteral("bicycle"), false, QVariantList(), m_bikeError);
}

void TripPlanner::finish(const QString &mode, bool ok, const QVariantList &items, const QString &error)
{
    m_results[mode] = items;
    if (!ok) {
        m_status[mode] = QStringLiteral("error");
        m_errors[mode] = error;
    } else {
        m_status[mode] = items.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ok");
    }
    emit resultsChanged();
    if (mode == m_mode)
        emit selectionChanged();
}
