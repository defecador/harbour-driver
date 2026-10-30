#ifndef HERECLIENT_H
#define HERECLIENT_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <functional>

class QNetworkReply;

// Thin HTTP layer for HERE. In fixture mode it answers from local JSON files instead,
// so the UI can be developed and screenshotted without an API key.
class HereClient : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(bool ok, const QJsonObject &json, const QString &error)>;

    explicit HereClient(QObject *parent = nullptr);

    void setFixtureDir(const QString &dir) { m_fixtureDir = dir; }
    bool fixtureMode() const { return !m_fixtureDir.isEmpty(); }

    // Returns the reply so callers can abort it; null in fixture mode.
    QPointer<QNetworkReply> get(const QUrl &url, const QString &fixture, Callback cb);
    // Same with a JSON body (long avoid lists don't fit in a URL).
    QPointer<QNetworkReply> post(const QUrl &url, const QByteArray &json, const QString &fixture, Callback cb);

private:
    QPointer<QNetworkReply> send(const QUrl &url, const QByteArray &body, bool post, const QString &fixture, Callback cb);

    QNetworkAccessManager m_nam;
    QString m_fixtureDir;
};

#endif
