#include "SearchController.h"
#include "AppSettings.h"
#include "HereClient.h"
#include "HereUrls.h"
#include "ItineraryParser.h"

SearchController::SearchController(AppSettings *settings, HereClient *client, QObject *parent)
    : QObject(parent), m_settings(settings), m_client(client)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, [this]() { run(false); });
}

void SearchController::setQuery(const QString &q)
{
    if (q == m_query) return;
    m_query = q;
    emit queryChanged();
    if (m_reply) m_reply->abort();
    ++m_generation;
    if (m_query.trimmed().size() < 3) {
        m_debounce.stop();
        if (!m_results.isEmpty()) {
            m_results.clear();
            m_error.clear();
            emit resultsChanged();
        }
        setBusy(false);
        return;
    }
    m_debounce.start();
}

void SearchController::submit()
{
    m_debounce.stop();
    if (m_reply) m_reply->abort();
    ++m_generation;
    if (m_query.trimmed().isEmpty())
        return;
    run(true);
}

void SearchController::run(bool discover)
{
    if (!m_client->fixtureMode() && !m_settings->hasHereKey()) {
        m_results.clear();
        m_error = tr("Add your HERE API key in Settings to search.");
        emit resultsChanged();
        return;
    }
    const int gen = m_generation;
    const HereUrls::LatLng at{m_lat, m_lng};
    const QUrl url = discover
            ? HereUrls::discover(m_settings->hereApiKey(), m_query, at, m_settings->lang())
            : HereUrls::autosuggest(m_settings->hereApiKey(), m_query, at, m_settings->lang());
    const QString fixture = discover ? QStringLiteral("discover") : QStringLiteral("autosuggest");

    setBusy(true);
    m_reply = m_client->get(url, fixture, [this, gen](bool ok, const QJsonObject &json, const QString &err) {
        if (gen != m_generation)
            return;
        setBusy(false);
        if (!ok) {
            m_results.clear();
            m_error = err;
        } else {
            m_results = ItineraryParser::parsePlaces(json);
            m_error.clear();
        }
        emit resultsChanged();
    });
}

void SearchController::reverse(double lat, double lng)
{
    if (!m_client->fixtureMode() && !m_settings->hasHereKey()) {
        emit reverseFinished(QVariantMap(), tr("Add your HERE API key in Settings to look up places."));
        return;
    }
    const HereUrls::LatLng at{lat, lng};
    const QUrl url = HereUrls::revgeocode(m_settings->hereApiKey(), at, m_settings->lang());
    m_client->get(url, QStringLiteral("revgeocode"), [this, lat, lng](bool ok, const QJsonObject &json, const QString &err) {
        if (!ok) {
            emit reverseFinished(QVariantMap(), err);
            return;
        }
        const QVariantList places = ItineraryParser::parsePlaces(json);
        QVariantMap pinned = places.isEmpty() ? QVariantMap() : places.first().toMap();
        if (pinned.isEmpty()) {
            pinned.insert(QStringLiteral("title"), tr("Dropped pin"));
        }
        pinned.insert(QStringLiteral("lat"), lat);
        pinned.insert(QStringLiteral("lng"), lng);
        pinned.insert(QStringLiteral("navLat"), lat);
        pinned.insert(QStringLiteral("navLng"), lng);
        emit reverseFinished(pinned, QString());
    });
}

void SearchController::testKey()
{
    if (m_settings->hereApiKey().isEmpty()) {
        emit keyTested(false, tr("Enter a key first."));
        return;
    }
    const HereUrls::LatLng at{m_lat, m_lng};
    const QUrl url = HereUrls::revgeocode(m_settings->hereApiKey(), at, m_settings->lang());
    m_client->get(url, QStringLiteral("revgeocode"), [this](bool ok, const QJsonObject &, const QString &err) {
        emit keyTested(ok, ok ? tr("Key works.") : err);
    });
}
