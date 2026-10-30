#ifndef SIGNPACKS_H
#define SIGNPACKS_H

#include <QByteArray>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVector>

#include "SpeedLimitStore.h"

class QNetworkReply;
class QSaveFile;
class QTimer;

// One stop sign, give-way sign or speed camera, as stored in a sign pack
// (tools/signpacks/build_pack.py). A sign that applies to several approaches
// is stored once per approach; `bearing` is the direction of travel (degrees,
// 0 = north) the record applies to, or -1 for "any direction".
struct Sign {
    enum Kind { Stop = 1, GiveWay = 2, Camera = 3 };

    qint32 latE7 = 0;
    qint32 lngE7 = 0;
    qint16 bearing = -1;
    quint8 kind = 0;
    quint8 maxspeed = 0;   // km/h, cameras only, 0 = unknown

    double lat() const { return latE7 / 1e7; }
    double lng() const { return lngE7 / 1e7; }
    // Identifies the physical sign (all approach records share it).
    qint64 id() const { return (qint64(latE7) << 32) | quint32(lngE7); }
};

// The offline sign data the user has downloaded, one pack per country.
// Packs are plain files (see parsePack for the layout) hosted on a GitHub
// release; this class lists them, downloads and removes them, and answers
// "which signs are near here" for RoadAlerts.
class SignPacks : public QObject
{
    Q_OBJECT
    // [{id, name, continent, bytes, count, date, installed, updateAvailable, progress}]
    // progress is -1 when idle, 0..1 while downloading.
    Q_PROPERTY(QVariantList packs READ packs NOTIFY changed)
    Q_PROPERTY(int installedCount READ installedCount NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY changed)

public:
    // `dir` is where packs are stored; empty keeps everything in memory (tests).
    explicit SignPacks(const QString &dir = QString(), QObject *parent = nullptr);

    struct Header {
        quint32 count = 0;
        quint32 date = 0;   // yyyymmdd
        qint32 minLat = 0, minLng = 0, maxLat = 0, maxLng = 0;   // e7
    };

    // Pack layout (little endian): 32-byte header ("DRSP", u32 version = 1,
    // u32 count, u32 date, i32 minLat, minLng, maxLat, maxLng in 1e-7 degrees)
    // followed by `count` 12-byte records: i32 lat, i32 lng, u8 kind,
    // u8 bearing/2 (255 = any), u8 maxspeed, u8 flags.
    static bool parsePack(const QByteArray &data, Header *header, QVector<Sign> *signs);
    static QByteArray buildPack(const QVector<Sign> &signs, quint32 date);

    // Dev/test packs are JSON: {"signs":[{lat,lng,kind,bearing,maxspeed}]}
    // with kind "stop" | "giveWay" | "camera" and bearing omitted for "any".
    static bool parseJsonPack(const QByteArray &json, Header *header, QVector<Sign> *signs);

    void setFixtureDir(const QString &dir);
    bool fixtureMode() const { return !m_fixtureDir.isEmpty(); }

    QVariantList packs() const;
    int installedCount() const { return m_installed.size(); }
    QString status() const { return m_status; }
    bool refreshing() const { return m_refreshing; }

    Q_INVOKABLE void refreshIndex();
    Q_INVOKABLE void download(const QString &id);
    Q_INVOKABLE void remove(const QString &id);

    // Posted limit (km/h) of the road at the position when heading that way
    // (< 0: unknown), from the downloaded limit files; 0 when unknown.
    int speedLimitAt(double lat, double lng, double headingDeg) { return m_limits.lookup(lat, lng, headingDeg); }
    SpeedLimitStore *limitStore() { return &m_limits; }

    // True when an installed pack's area contains the point.
    bool covers(double lat, double lng) const;
    // Signs within radiusM of the point.
    QVector<Sign> near(double lat, double lng, double radiusM) const;

    // Installs pack bytes under `id` (writing the file when a directory is set).
    bool install(const QString &id, const QByteArray &data, bool json = false);

signals:
    void changed();
    void dataChanged();
    void error(const QString &message);

private:
    struct IndexEntry {
        QString id, name, continent;
        qint64 bytes = 0;
        quint32 count = 0, date = 0;
        QString sha256;
        qint64 limitsBytes = 0;   // companion speed limit file, 0 = none
        QString limitsSha256;
    };
    struct Installed {
        Header header;
        QVector<Sign> signs;
    };

    void loadInstalled();
    void rebuildGrid();
    void setStatus(const QString &s);
    void startFile(const QString &id, const QString &suffix);
    void finishDownload(const QString &id, const QString &suffix, QNetworkReply *reply, QSaveFile *file);
    bool hasLimits(const QString &id) const;
    QNetworkRequest request(const QString &file) const;
    static qint64 cellKey(double lat, double lng);

    QString m_dir;
    QString m_fixtureDir;
    QString m_status;
    bool m_refreshing = false;
    QNetworkAccessManager m_nam;
    QVector<IndexEntry> m_index;
    QHash<QString, Installed> m_installed;
    QHash<QString, double> m_progress;
    QHash<QString, QPointer<QNetworkReply>> m_replies;

    SpeedLimitStore m_limits;
    QVector<Sign> m_all;
    QHash<qint64, QVector<int>> m_grid;
};

#endif
