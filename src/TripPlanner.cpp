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
