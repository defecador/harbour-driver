#include "SignPacks.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimer>
#include <algorithm>
#include <cmath>

#include "NavGeo.h"

namespace {
const char kBaseUrl[] = "https://github.com/defecador/harbour-driver/releases/download/signpacks/";
const char kMagic[] = "DRSP";
const int kHeaderSize = 32;
const int kRecordSize = 12;
// A pack covers its sign bounding box plus this margin (degrees), so roads at
// the edge of a country still count as covered.
const double kCoverMargin = 0.05;

void put32(QByteArray *b, qint32 v)
{
    const quint32 u = quint32(v);
    b->append(char(u & 0xff));
    b->append(char((u >> 8) & 0xff));
    b->append(char((u >> 16) & 0xff));
    b->append(char((u >> 24) & 0xff));
}

qint32 get32(const char *p)
{
    const unsigned char *u = reinterpret_cast<const unsigned char *>(p);
    return qint32(quint32(u[0]) | (quint32(u[1]) << 8) | (quint32(u[2]) << 16) | (quint32(u[3]) << 24));
}

quint8 kindFromString(const QString &s)
{
    if (s == QLatin1String("stop")) return Sign::Stop;
    if (s == QLatin1String("giveWay")) return Sign::GiveWay;
    if (s == QLatin1String("camera")) return Sign::Camera;
    return 0;
}

void fillBounds(const QVector<Sign> &signs, SignPacks::Header *h)
{
    h->count = quint32(signs.size());
    if (signs.isEmpty())
        return;
    h->minLat = h->maxLat = signs.first().latE7;
    h->minLng = h->maxLng = signs.first().lngE7;
    for (const Sign &s : signs) {
        h->minLat = qMin(h->minLat, s.latE7);
        h->maxLat = qMax(h->maxLat, s.latE7);
        h->minLng = qMin(h->minLng, s.lngE7);
        h->maxLng = qMax(h->maxLng, s.lngE7);
    }
}
}

SignPacks::SignPacks(const QString &dir, QObject *parent)
    : QObject(parent), m_dir(dir)
{
    loadInstalled();
}

bool SignPacks::parsePack(const QByteArray &data, Header *header, QVector<Sign> *signs)
{
    if (data.size() < kHeaderSize || !data.startsWith(kMagic))
        return false;
    const char *p = data.constData();
    if (get32(p + 4) != 1)
        return false;
    Header h;
    h.count = quint32(get32(p + 8));
    h.date = quint32(get32(p + 12));
    h.minLat = get32(p + 16);
    h.minLng = get32(p + 20);
    h.maxLat = get32(p + 24);
    h.maxLng = get32(p + 28);
    if (qint64(data.size()) != qint64(kHeaderSize) + qint64(h.count) * kRecordSize)
        return false;
    if (signs) {
        signs->clear();
        signs->reserve(int(h.count));
        const char *r = p + kHeaderSize;
        for (quint32 i = 0; i < h.count; ++i, r += kRecordSize) {
            Sign s;
            s.latE7 = get32(r);
            s.lngE7 = get32(r + 4);
            s.kind = quint8(r[8]);
            const quint8 b = quint8(r[9]);
            s.bearing = b == 255 ? qint16(-1) : qint16(b * 2);
            s.maxspeed = quint8(r[10]);
            if (s.kind >= Sign::Stop && s.kind <= Sign::Camera)
                signs->append(s);
        }
    }
    if (header)
        *header = h;
    return true;
}

QByteArray SignPacks::buildPack(const QVector<Sign> &signs, quint32 date)
{
    Header h;
    fillBounds(signs, &h);
    QByteArray out;
    out.reserve(kHeaderSize + signs.size() * kRecordSize);
    out.append(kMagic, 4);
    put32(&out, 1);
    put32(&out, qint32(h.count));
    put32(&out, qint32(date));
    put32(&out, h.minLat);
    put32(&out, h.minLng);
    put32(&out, h.maxLat);
    put32(&out, h.maxLng);
    for (const Sign &s : signs) {
        put32(&out, s.latE7);
        put32(&out, s.lngE7);
        out.append(char(s.kind));
        out.append(char(s.bearing < 0 ? 255 : (s.bearing % 360) / 2));
        out.append(char(s.maxspeed));
        out.append(char(0));
    }
    return out;
}

bool SignPacks::parseJsonPack(const QByteArray &json, Header *header, QVector<Sign> *signs)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject() || !doc.object().value(QStringLiteral("signs")).isArray())
        return false;
    QVector<Sign> out;
    const QJsonArray arr = doc.object().value(QStringLiteral("signs")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Sign s;
        s.kind = kindFromString(o.value(QStringLiteral("kind")).toString());
        if (!s.kind)
            continue;
        s.latE7 = qint32(std::lround(o.value(QStringLiteral("lat")).toDouble() * 1e7));
        s.lngE7 = qint32(std::lround(o.value(QStringLiteral("lng")).toDouble() * 1e7));
        s.bearing = o.contains(QStringLiteral("bearing"))
                        ? qint16(((o.value(QStringLiteral("bearing")).toInt() % 360) + 360) % 360)
                        : qint16(-1);
        s.maxspeed = quint8(o.value(QStringLiteral("maxspeed")).toInt());
        out.append(s);
    }
    if (header) {
        fillBounds(out, header);
        header->date = 20000101;
    }
    if (signs)
        *signs = out;
    return true;
}

void SignPacks::setFixtureDir(const QString &dir)
{
    m_fixtureDir = dir;
    if (dir.isEmpty())
        return;
    // Fixture mode never touches the real pack folder.
    m_dir.clear();
    m_installed.clear();
    QFile f(dir + QStringLiteral("/signpack_test.json"));
    if (f.open(QIODevice::ReadOnly)) {
        Installed inst;
        if (parseJsonPack(f.readAll(), &inst.header, &inst.signs))
            m_installed.insert(QStringLiteral("test"), inst);
    }
    QFile lim(dir + QStringLiteral("/speedlimits_test.json"));
    if (lim.open(QIODevice::ReadOnly))
        m_limits.loadJson(lim.readAll());
    rebuildGrid();
    emit changed();
    emit dataChanged();
}

bool SignPacks::hasLimits(const QString &id) const
{
    return !m_dir.isEmpty() && QFile::exists(m_dir + QLatin1Char('/') + id + QStringLiteral(".drsl"));
}

void SignPacks::loadInstalled()
{
    if (m_dir.isEmpty())
        return;
    const QStringList files = QDir(m_dir).entryList(QStringList() << QStringLiteral("*.drsp"), QDir::Files);
    for (const QString &name : files) {
        QFile f(m_dir + QLatin1Char('/') + name);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        Installed inst;
        if (parsePack(f.readAll(), &inst.header, &inst.signs))
            m_installed.insert(name.left(name.size() - 5), inst);
    }
    rebuildGrid();
    m_limits.load(m_dir);
}

qint64 SignPacks::cellKey(double lat, double lng)
{
    const qint64 ila = qint64(std::floor(lat * 100.0)) + 9000;
    const qint64 iln = qint64(std::floor(lng * 100.0)) + 18000;
    return ila * 40000 + iln;
}

void SignPacks::rebuildGrid()
{
    m_all.clear();
    m_grid.clear();
    for (auto it = m_installed.constBegin(); it != m_installed.constEnd(); ++it)
        m_all += it.value().signs;
    for (int i = 0; i < m_all.size(); ++i)
        m_grid[cellKey(m_all[i].lat(), m_all[i].lng())].append(i);
}

bool SignPacks::covers(double lat, double lng) const
{
    for (auto it = m_installed.constBegin(); it != m_installed.constEnd(); ++it) {
        const Header &h = it.value().header;
        if (lat >= h.minLat / 1e7 - kCoverMargin && lat <= h.maxLat / 1e7 + kCoverMargin
            && lng >= h.minLng / 1e7 - kCoverMargin && lng <= h.maxLng / 1e7 + kCoverMargin)
            return true;
    }
    return false;
}

QVector<Sign> SignPacks::near(double lat, double lng, double radiusM) const
{
    QVector<Sign> out;
    if (m_all.isEmpty())
        return out;
    const double dLat = radiusM / 111320.0;
    const double dLng = radiusM / (111320.0 * std::max(0.2, std::cos(lat * M_PI / 180.0)));
    const qint64 la0 = qint64(std::floor((lat - dLat) * 100.0)), la1 = qint64(std::floor((lat + dLat) * 100.0));
    const qint64 ln0 = qint64(std::floor((lng - dLng) * 100.0)), ln1 = qint64(std::floor((lng + dLng) * 100.0));
    for (qint64 la = la0; la <= la1; ++la) {
        for (qint64 ln = ln0; ln <= ln1; ++ln) {
            const auto it = m_grid.constFind((la + 9000) * 40000 + (ln + 18000));
            if (it == m_grid.constEnd())
                continue;
            for (int idx : it.value()) {
                const Sign &s = m_all.at(idx);
                if (NavGeo::metersBetween(lat, lng, s.lat(), s.lng()) <= radiusM)
                    out.append(s);
            }
        }
    }
    return out;
}

bool SignPacks::install(const QString &id, const QByteArray &data, bool json)
{
    Installed inst;
    const bool ok = json ? parseJsonPack(data, &inst.header, &inst.signs)
                         : parsePack(data, &inst.header, &inst.signs);
    if (!ok)
        return false;
    if (!m_dir.isEmpty() && !json) {
        QDir().mkpath(m_dir);
        QSaveFile f(m_dir + QLatin1Char('/') + id + QStringLiteral(".drsp"));
        if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
            return false;
    }
    m_installed.insert(id, inst);
    rebuildGrid();
    emit changed();
    emit dataChanged();
    return true;
}

void SignPacks::remove(const QString &id)
{
    if (!m_installed.contains(id))
        return;
    m_installed.remove(id);
    if (!m_dir.isEmpty())
        QFile::remove(m_dir + QLatin1Char('/') + id + QStringLiteral(".drsp"));
    if (!m_dir.isEmpty()) {
        QFile::remove(m_dir + QLatin1Char('/') + id + QStringLiteral(".drsl"));
        m_limits.load(m_dir);
    }
    rebuildGrid();
    emit changed();
    emit dataChanged();
}

QVariantList SignPacks::packs() const
{
    QHash<QString, QVariantMap> byId;
    for (const IndexEntry &e : m_index) {
        QVariantMap m;
        m[QStringLiteral("id")] = e.id;
        m[QStringLiteral("name")] = e.name;
        m[QStringLiteral("continent")] = e.continent;
        m[QStringLiteral("bytes")] = e.bytes + e.limitsBytes;
        m[QStringLiteral("count")] = e.count;
        m[QStringLiteral("date")] = e.date;
        byId.insert(e.id, m);
    }
    for (auto it = m_installed.constBegin(); it != m_installed.constEnd(); ++it) {
        QVariantMap m = byId.value(it.key());
        if (m.isEmpty()) {
            m[QStringLiteral("id")] = it.key();
            m[QStringLiteral("name")] = it.key();
            m[QStringLiteral("bytes")] = qint64(kHeaderSize) + qint64(it.value().header.count) * kRecordSize;
            m[QStringLiteral("count")] = it.value().header.count;
            m[QStringLiteral("date")] = it.value().header.date;
        }
        m[QStringLiteral("installed")] = true;
        bool wantsLimits = false;
        for (const IndexEntry &e : m_index) {
            if (e.id == it.key())
                wantsLimits = e.limitsBytes > 0;
        }
        m[QStringLiteral("updateAvailable")] = m.value(QStringLiteral("date")).toUInt() > it.value().header.date
                                               || (wantsLimits && !hasLimits(it.key()));
        byId.insert(it.key(), m);
    }
    QVariantList out;
    for (auto it = byId.constBegin(); it != byId.constEnd(); ++it) {
        QVariantMap m = it.value();
        if (!m.contains(QStringLiteral("installed"))) {
            m[QStringLiteral("installed")] = false;
            m[QStringLiteral("updateAvailable")] = false;
        }
        m[QStringLiteral("progress")] = m_progress.value(it.key(), -1.0);
        out.append(m);
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    return out;
}

void SignPacks::setStatus(const QString &s)
{
    m_status = s;
    emit changed();
}

QNetworkRequest SignPacks::request(const QString &file) const
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kBaseUrl) + file));
    req.setRawHeader("User-Agent", QByteArrayLiteral("harbour-driver sign packs"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    return req;
}

void SignPacks::refreshIndex()
{
    if (m_refreshing)
        return;
    auto apply = [this](const QByteArray &body) {
        m_index.clear();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        const QJsonArray arr = doc.object().value(QStringLiteral("packs")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            IndexEntry e;
            e.id = o.value(QStringLiteral("id")).toString();
            if (e.id.isEmpty() || e.id.contains(QLatin1Char('/')) || e.id.contains(QLatin1Char('.')))
                continue;
            e.name = o.value(QStringLiteral("name")).toString(e.id);
            e.continent = o.value(QStringLiteral("continent")).toString();
            e.bytes = qint64(o.value(QStringLiteral("bytes")).toDouble());
            e.count = quint32(o.value(QStringLiteral("count")).toDouble());
            e.date = quint32(o.value(QStringLiteral("date")).toDouble());
            e.sha256 = o.value(QStringLiteral("sha256")).toString();
            e.limitsBytes = qint64(o.value(QStringLiteral("limitsBytes")).toDouble());
            e.limitsSha256 = o.value(QStringLiteral("limitsSha256")).toString();
            m_index.append(e);
        }
        m_status = m_index.isEmpty() ? tr("No sign data has been published yet") : QString();
    };

    if (fixtureMode()) {
        QFile f(m_fixtureDir + QStringLiteral("/signpacks_index.json"));
        if (f.open(QIODevice::ReadOnly))
            apply(f.readAll());
        emit changed();
        return;
    }

    m_refreshing = true;
    setStatus(QString());
    QNetworkReply *reply = m_nam.get(request(QStringLiteral("index.json")));
    QTimer::singleShot(20000, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [this, reply, apply]() {
        reply->deleteLater();
        m_refreshing = false;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && code >= 200 && code < 300) {
            apply(reply->readAll());
            emit changed();
        } else if (code == 404) {
            m_index.clear();
            setStatus(tr("No sign data has been published yet"));
        } else {
            setStatus(tr("Couldn't load the country list. Check your connection."));
        }
    });
}

void SignPacks::download(const QString &id)
{
    if (m_replies.value(id) || m_progress.contains(id))
        return;
    const IndexEntry *entry = nullptr;
    for (const IndexEntry &e : m_index) {
        if (e.id == id)
            entry = &e;
    }
    if (!entry)
        return;

    if (fixtureMode()) {
        // No network in fixture mode: animate a short progress run, then mark
        // the pack installed (empty, in memory only).
        m_progress[id] = 0.0;
        emit changed();
        QTimer *t = new QTimer(this);
        t->setInterval(200);
        connect(t, &QTimer::timeout, this, [this, id, t]() {
            const double p = m_progress.value(id) + 0.25;
            if (p < 1.0) {
                m_progress[id] = p;
                emit changed();
                return;
            }
            t->deleteLater();
            m_progress.remove(id);
            install(id, buildPack(QVector<Sign>(), 20250101));
        });
        t->start();
        return;
    }

    if (m_dir.isEmpty())
        return;
    m_progress[id] = 0.0;
    emit changed();
    startFile(id, QStringLiteral(".drsp"));
}

void SignPacks::startFile(const QString &id, const QString &suffix)
{
    QDir().mkpath(m_dir);
    QSaveFile *file = new QSaveFile(m_dir + QLatin1Char('/') + id + suffix, this);
    if (!file->open(QIODevice::WriteOnly)) {
        file->deleteLater();
        m_progress.remove(id);
        emit changed();
        emit error(tr("Couldn't save the file."));
        return;
    }
    bool second = suffix == QLatin1String(".drsl");
    bool twoParts = second;
    for (const IndexEntry &e : m_index) {
        if (e.id == id && e.limitsBytes > 0)
            twoParts = true;
    }
    QNetworkReply *reply = m_nam.get(request(id + suffix));
    m_replies[id] = reply;

    // Large files are fine, a stalled connection is not.
    QTimer *idle = new QTimer(reply);
    idle->setSingleShot(true);
    idle->setInterval(30000);
    connect(idle, &QTimer::timeout, reply, &QNetworkReply::abort);
    idle->start();

    connect(reply, &QNetworkReply::readyRead, this, [file, reply, idle]() {
        file->write(reply->readAll());
        idle->start();
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, id, idle, second, twoParts](qint64 got, qint64 total) {
        idle->start();
        double p = total > 0 ? double(got) / double(total) : 0.0;
        if (twoParts)
            p = second ? 0.5 + p / 2 : p / 2;
        if (p - m_progress.value(id) >= 0.01) {
            m_progress[id] = p;
            emit changed();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, id, suffix, reply, file]() {
        finishDownload(id, suffix, reply, file);
    });
}

void SignPacks::finishDownload(const QString &id, const QString &suffix, QNetworkReply *reply, QSaveFile *file)
{
    reply->deleteLater();
    m_replies.remove(id);
    const bool limitsFile = suffix == QLatin1String(".drsl");
    auto fail = [this](const QString &msg) {
        m_status = msg;
        emit changed();
        emit error(msg);
    };
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || code < 200 || code >= 300) {
        file->cancelWriting();
        file->deleteLater();
        m_progress.remove(id);
        fail(tr("Download failed. Check your connection."));
        return;
    }
    file->write(reply->readAll());
    const bool saved = file->commit();
    const QString path = file->fileName();
    file->deleteLater();

    QByteArray data;
    if (saved) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            data = f.readAll();
    }
    QString sha;
    bool wantsLimits = false;
    for (const IndexEntry &e : m_index) {
        if (e.id != id)
            continue;
        sha = limitsFile ? e.limitsSha256 : e.sha256;
        wantsLimits = e.limitsBytes > 0;
    }
    const bool hashOk = sha.isEmpty()
        || QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex() == sha.toLatin1();
    Installed inst;
    const bool valid = limitsFile ? data.startsWith("DRSL") : parsePack(data, &inst.header, &inst.signs);
    if (!saved || !hashOk || !valid) {
        QFile::remove(path);
        m_progress.remove(id);
        fail(tr("The downloaded file was damaged. Try again."));
        return;
    }
    if (limitsFile) {
        m_progress.remove(id);
        m_limits.load(m_dir);
        emit changed();
        emit dataChanged();
        return;
    }
    m_installed.insert(id, inst);
    rebuildGrid();
    emit dataChanged();
    if (wantsLimits) {
        startFile(id, QStringLiteral(".drsl"));
        return;
    }
    m_progress.remove(id);
    emit changed();
}
