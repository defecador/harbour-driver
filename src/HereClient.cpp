#include "HereClient.h"
#include "HereUrls.h"
#include "ItineraryParser.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTimer>

HereClient::HereClient(QObject *parent)
    : QObject(parent)
{
}

QPointer<QNetworkReply> HereClient::get(const QUrl &url, const QString &fixture, Callback cb)
{
    if (fixtureMode()) {
        const QString path = m_fixtureDir + QLatin1Char('/') + fixture + QStringLiteral(".json");
        QTimer::singleShot(350, this, [path, cb]() {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) {
                cb(false, QJsonObject(), QStringLiteral("Missing fixture %1").arg(path));
                return;
            }
            cb(true, QJsonDocument::fromJson(f.readAll()).object(), QString());
        });
        return QPointer<QNetworkReply>();
    }

    // Third-party routing URLs carry the trip's coordinates: log the host only.
    if (url.host().endsWith(QLatin1String("hereapi.com")))
        qInfo("HERE GET %s", qPrintable(HereUrls::redact(url.toString())));
    else
        qInfo("GET %s", qPrintable(url.host()));
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", QByteArray("harbour-matka/") + QCoreApplication::applicationVersion().toUtf8()
                     + " (Sailfish OS)");
    QNetworkReply *reply = m_nam.get(req);

    QTimer *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout->start(20000);

    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            cb(false, json, QCoreApplication::translate("HereClient", "Request timed out"));
            return;
        }
        if (status == 401 || status == 403) {
            cb(false, json, QCoreApplication::translate("HereClient",
                "HERE rejected the API key. Check it in Settings and that it has access to this service."));
            return;
        }
        if (status == 429) {
            cb(false, json, QCoreApplication::translate("HereClient", "HERE rate limit reached. Try again shortly."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            QString msg = ItineraryParser::errorMessage(json);
            if (msg.isEmpty())
                msg = status == 0 ? QCoreApplication::translate("HereClient", "No connection")
                                  : HereUrls::redact(reply->errorString());
            qWarning("HERE error %d: %s", status, qPrintable(HereUrls::redact(msg)));
            cb(false, json, msg);
            return;
        }
        cb(true, json, QString());
    });
    return reply;
}
