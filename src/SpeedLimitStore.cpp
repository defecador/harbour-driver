#include "SpeedLimitStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "NavGeo.h"

namespace {
const int kHeaderSize = 32;
const int kDirEntry = 16;
// A road counts as "the one we are on" within this distance (GPS error plus
// the simplification of the stored geometry).
const double kMaxPerp = 18.0;
const double kAlignTolerance = 40.0;
const double kOnewayTolerance = 60.0;
const int kMaxCachedTiles = 24;

qint32 rd32(const uchar *p)
{
    return qint32(quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24));
}

quint32 readVarint(const uchar *&p, const uchar *end)
{
    quint32 v = 0;
    int shift = 0;
    while (p < end) {
        const uchar b = *p++;
        v |= quint32(b & 0x7f) << shift;
        if (!(b & 0x80))
            break;
        shift += 7;
        if (shift > 28)
            break;
    }
    return v;
}

qint32 unzigzag(quint32 v) { return qint32(v >> 1) ^ -qint32(v & 1); }

void putVarint(QByteArray *b, quint32 v)
{
    while (v >= 0x80) {
        b->append(char((v & 0x7f) | 0x80));
        v >>= 7;
    }
    b->append(char(v));
}

void put32(QByteArray *b, qint32 v)
{
    const quint32 u = quint32(v);
    for (int i = 0; i < 4; ++i)
        b->append(char((u >> (8 * i)) & 0xff));
}

qint32 e7(double deg) { return qint32(std::lround(deg * 1e7)); }
}

SpeedLimitStore::SpeedLimitStore() {}

SpeedLimitStore::~SpeedLimitStore() { clear(); }

void SpeedLimitStore::clear()
{
    for (File &f : m_files) {
        if (f.file) {
            if (f.data)
                f.file->unmap(const_cast<uchar *>(f.data));
            delete f.file;
        }
    }
    m_files.clear();
    m_cache.clear();
    m_loose = Tile();
}

void SpeedLimitStore::load(const QString &dir)
{
    clear();
    if (dir.isEmpty())
        return;
    const QStringList names = QDir(dir).entryList(QStringList() << QStringLiteral("*.drsl"), QDir::Files);
    for (const QString &name : names) {
        File f;
        f.file = new QFile(dir + QLatin1Char('/') + name);
        if (!f.file->open(QIODevice::ReadOnly)) {
            delete f.file;
            continue;
        }
        f.size = f.file->size();
        f.data = f.file->map(0, f.size);
        if (!f.data || f.size < kHeaderSize || memcmp(f.data, "DRSL", 4) != 0 || rd32(f.data + 4) != 1) {
            if (f.data)
                f.file->unmap(const_cast<uchar *>(f.data));
            delete f.file;
            continue;
        }
        const quint32 tiles = quint32(rd32(f.data + 8));
        if (qint64(kHeaderSize) + qint64(tiles) * kDirEntry > f.size) {
            f.file->unmap(const_cast<uchar *>(f.data));
            delete f.file;
            continue;
        }
        for (quint32 i = 0; i < tiles; ++i) {
            const uchar *e = f.data + kHeaderSize + qint64(i) * kDirEntry;
            const quint32 off = quint32(rd32(e + 8)), len = quint32(rd32(e + 12));
            if (qint64(off) + len <= f.size)
                f.dir.insert(tileKey(rd32(e), rd32(e + 4)), qMakePair(off, len));
        }
        m_files.append(f);
    }
}

bool SpeedLimitStore::loadJson(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject() || !doc.object().value(QStringLiteral("ways")).isArray())
        return false;
    clear();
    for (const QJsonValue &v : doc.object().value(QStringLiteral("ways")).toArray()) {
        const QJsonObject o = v.toObject();
        QVector<double> lat, lng;
        for (const QJsonValue &p : o.value(QStringLiteral("points")).toArray()) {
            lat.append(p.toArray().at(0).toDouble());
            lng.append(p.toArray().at(1).toDouble());
        }
        addWay(&m_loose, o.value(QStringLiteral("limit")).toInt(), o.value(QStringLiteral("flags")).toInt(), lat, lng);
    }
    return true;
}

void SpeedLimitStore::addWay(Tile *t, int limit, int flags, const QVector<double> &lat, const QVector<double> &lng)
{
    if (lat.size() < 2 || limit <= 0 || limit > 250)
        return;
    Way w;
    w.limit = quint8(limit);
    w.flags = quint8(flags);
    w.first = t->pts.size() / 2;
    w.count = lat.size();
    w.minLat = w.maxLat = float(lat[0]);
    w.minLng = w.maxLng = float(lng[0]);
    for (int i = 0; i < lat.size(); ++i) {
        t->pts.append(e7(lat[i]));
        t->pts.append(e7(lng[i]));
        w.minLat = qMin(w.minLat, float(lat[i]));
        w.maxLat = qMax(w.maxLat, float(lat[i]));
        w.minLng = qMin(w.minLng, float(lng[i]));
        w.maxLng = qMax(w.maxLng, float(lng[i]));
    }
    t->ways.append(w);
}

void SpeedLimitStore::decodeTile(const uchar *p, quint32 length, Tile *out)
{
    const uchar *end = p + length;
    while (p + 3 <= end) {
        const int limit = *p++;
        const int flags = *p++;
        const quint32 n = readVarint(p, end);
        if (n < 2 || p + 8 > end)
            break;
        QVector<double> lat, lng;
        qint32 la = rd32(p), lo = rd32(p + 4);
        p += 8;
        lat.append(la / 1e7);
        lng.append(lo / 1e7);
        for (quint32 i = 1; i < n && p < end; ++i) {
            la += unzigzag(readVarint(p, end));
            lo += unzigzag(readVarint(p, end));
            lat.append(la / 1e7);
            lng.append(lo / 1e7);
        }
        addWay(out, limit, flags, lat, lng);
    }
}

const SpeedLimitStore::Tile *SpeedLimitStore::tile(int tileLat, int tileLng)
{
    const qint64 key = tileKey(tileLat, tileLng);
    const auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd())
        return &it.value();
    Tile t;
    bool found = false;
    for (const File &f : m_files) {
        const auto d = f.dir.constFind(key);
        if (d == f.dir.constEnd())
            continue;
        decodeTile(f.data + d.value().first, d.value().second, &t);
        found = true;
    }
    if (!found)
        return nullptr;
    if (m_cache.size() >= kMaxCachedTiles)
        m_cache.clear();
    return &m_cache.insert(key, t).value();
}

int SpeedLimitStore::search(const Tile &t, double lat, double lng, double headingDeg, double *bestPerp) const
{
    int limit = 0;
    const double pad = 0.0003;   // ~30 m
    for (const Way &w : t.ways) {
        if (lat < w.minLat - pad || lat > w.maxLat + pad || lng < w.minLng - pad || lng > w.maxLng + pad)
            continue;
        for (int i = 0; i + 1 < w.count; ++i) {
            const qint32 *a = &t.pts[(w.first + i) * 2];
            const qint32 *b = &t.pts[(w.first + i + 1) * 2];
            const double aLat = a[0] / 1e7, aLng = a[1] / 1e7, bLat = b[0] / 1e7, bLng = b[1] / 1e7;
            const NavGeo::Projection p = NavGeo::projectOntoSegment(lat, lng, aLat, aLng, bLat, bLng);
            if (p.perpDistance > kMaxPerp || p.perpDistance >= *bestPerp)
                continue;
            if (headingDeg >= 0) {
                const double seg = NavGeo::bearingDegrees(aLat, aLng, bLat, bLng);
                const double fwd = NavGeo::angleDiff(headingDeg, seg);
                const double bwd = 180.0 - fwd;
                if (w.flags == 1 ? fwd > kOnewayTolerance
                    : w.flags == 2 ? bwd > kOnewayTolerance
                    : (fwd > kAlignTolerance && bwd > kAlignTolerance))
                    continue;
            }
            *bestPerp = p.perpDistance;
            limit = w.limit;
        }
    }
    return limit;
}

int SpeedLimitStore::lookup(double lat, double lng, double headingDeg)
{
    double best = 1e9;
    int limit = search(m_loose, lat, lng, headingDeg, &best);
    if (m_files.isEmpty())
        return limit;
    const int tLat = int(std::floor(lat * 10.0)), tLng = int(std::floor(lng * 10.0));
    // Ways are filed under the tile of their first point, so a road starting
    // in the next tile can still pass through this one: look at neighbours
    // when close to a border.
    const double fLat = lat * 10.0 - tLat, fLng = lng * 10.0 - tLng;
    for (int dLat = -1; dLat <= 1; ++dLat) {
        for (int dLng = -1; dLng <= 1; ++dLng) {
            if ((dLat < 0 && fLat > 0.02) || (dLat > 0 && fLat < 0.98)
                || (dLng < 0 && fLng > 0.02) || (dLng > 0 && fLng < 0.98))
                continue;
            if (const Tile *t = tile(tLat + dLat, tLng + dLng)) {
                const int l = search(*t, lat, lng, headingDeg, &best);
                if (l > 0)
                    limit = l;
            }
        }
    }
    return limit;
}

QByteArray SpeedLimitStore::encode(const QVector<LimitWay> &ways, quint32 date)
{
    QHash<qint64, QByteArray> tiles;
    QVector<qint64> order;
    qint32 minLat = 0, minLng = 0, maxLat = 0, maxLng = 0;
    bool first = true;
    for (const LimitWay &w : ways) {
        if (w.lat.size() < 2)
            continue;
        const qint64 key = tileKey(int(std::floor(w.lat[0] * 10.0)), int(std::floor(w.lng[0] * 10.0)));
        if (!tiles.contains(key))
            order.append(key);
        QByteArray &b = tiles[key];
        b.append(char(w.limit));
        b.append(char(w.flags));
        putVarint(&b, quint32(w.lat.size()));
        qint32 la = e7(w.lat[0]), lo = e7(w.lng[0]);
        put32(&b, la);
        put32(&b, lo);
        for (int i = 1; i < w.lat.size(); ++i) {
            const qint32 nla = e7(w.lat[i]), nlo = e7(w.lng[i]);
            const qint32 dla = nla - la, dlo = nlo - lo;
            putVarint(&b, quint32((dla << 1) ^ (dla >> 31)));
            putVarint(&b, quint32((dlo << 1) ^ (dlo >> 31)));
            la = nla;
            lo = nlo;
        }
        for (int i = 0; i < w.lat.size(); ++i) {
            const qint32 a = e7(w.lat[i]), b2 = e7(w.lng[i]);
            if (first) { minLat = maxLat = a; minLng = maxLng = b2; first = false; }
            minLat = qMin(minLat, a); maxLat = qMax(maxLat, a);
            minLng = qMin(minLng, b2); maxLng = qMax(maxLng, b2);
        }
    }
    QByteArray out("DRSL", 4);
    put32(&out, 1);
    put32(&out, qint32(order.size()));
    put32(&out, qint32(date));
    put32(&out, minLat);
    put32(&out, minLng);
    put32(&out, maxLat);
    put32(&out, maxLng);
    quint32 offset = quint32(kHeaderSize + order.size() * kDirEntry);
    for (qint64 key : order) {
        put32(&out, qint32(key / 4000 - 900));
        put32(&out, qint32(key % 4000 - 1800));
        put32(&out, qint32(offset));
        put32(&out, qint32(tiles[key].size()));
        offset += quint32(tiles[key].size());
    }
    for (qint64 key : order)
        out.append(tiles[key]);
    return out;
}
