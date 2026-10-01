#ifndef SEARCHCONTROLLER_H
#define SEARCHCONTROLLER_H

#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>

class AppSettings;
class HereClient;

class SearchController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY resultsChanged)

public:
    SearchController(AppSettings *settings, HereClient *client, QObject *parent = nullptr);

    QString query() const { return m_query; }
    void setQuery(const QString &q);
    QVariantList results() const { return m_results; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }

    Q_INVOKABLE void setCenter(double lat, double lng) { m_lat = lat; m_lng = lng; }
    Q_INVOKABLE void submit();
    Q_INVOKABLE void reverse(double lat, double lng);
    Q_INVOKABLE void testKey();

signals:
    void queryChanged();
    void resultsChanged();
    void busyChanged();
    void reverseFinished(const QVariantMap &place, const QString &error);
    void keyTested(bool ok, const QString &message);

private:
    void run(bool discover);
    void setBusy(bool b) { if (b != m_busy) { m_busy = b; emit busyChanged(); } }

    AppSettings *m_settings;
    HereClient *m_client;
    QString m_query, m_error;
    QVariantList m_results;
    bool m_busy = false;
    double m_lat = 52.52, m_lng = 13.405;
    QTimer m_debounce;
    QPointer<QNetworkReply> m_reply;
    int m_generation = 0;
};

#endif
