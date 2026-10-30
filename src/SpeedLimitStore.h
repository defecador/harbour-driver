#ifndef SPEEDLIMITSTORE_H
#define SPEEDLIMITSTORE_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

class QFile;

// Posted speed limits of roads, from the per-country ".drsl" files
// (tools/signpacks/build_limits.py), for free drive where there is no route
// to take them from. Files are memory mapped and decoded one 0.1 degree tile
// at a time.
//
// File layout (little endian): 32-byte header ("DRSL", u32 version = 1, u32
// tile count, u32 date, i32 minLat, minLng, maxLat, maxLng in 1e-7 degrees),
// a directory of 16-byte entries (i32 tileLat, i32 tileLng, u32 offset, u32
// length; tile = floor(degrees * 10)), then the tiles. A tile is a run of
// ways: u8 limit km/h, u8 flags (1 = forward travel only, 2 = backward only),
// varint point count, the first point as two i32 (1e-7 degrees), then each
// further point as zigzag varint deltas.
class SpeedLimitStore
{
public:
    struct LimitWay {
        int limit = 0;
        int flags = 0;
        QVector<double> lat, lng;
    };

    SpeedLimitStore();
    ~SpeedLimitStore();

    // Loads every .drsl file in the folder (replacing what was loaded).
    void load(const QString &dir);
    // Dev/test data: {"ways":[{"limit":50,"flags":0,"points":[[lat,lng],...]}]}
    bool loadJson(const QByteArray &json);
    bool isEmpty() const { return m_files.isEmpty() && m_loose.ways.isEmpty(); }

    // Limit in km/h of the road at the position when travelling towards
    // headingDeg (< 0: unknown), or 0 when there is none.
    int lookup(double lat, double lng, double headingDeg);

    // Writes a file holding the ways (ways are filed under the tile of their
    // first point, so they should not cross tile borders).
    static QByteArray encode(const QVector<LimitWay> &ways, quint32 date);

private:
    struct Way {
        quint8 limit = 0;
        quint8 flags = 0;
        float minLat = 0, minLng = 0, maxLat = 0, maxLng = 0;
        int first = 0, count = 0;   // into Tile::pts
    };
    struct Tile {
        QVector<Way> ways;
        QVector<qint32> pts;   // lat, lng pairs in 1e-7 degrees
    };
    struct File {
        QFile *file = nullptr;
        const uchar *data = nullptr;
        qint64 size = 0;
        QHash<qint64, QPair<quint32, quint32>> dir;   // tile key -> offset, length
    };

    static qint64 tileKey(int tileLat, int tileLng) { return qint64(tileLat + 900) * 4000 + (tileLng + 1800); }
    const Tile *tile(int tileLat, int tileLng);
    static void decodeTile(const uchar *p, quint32 length, Tile *out);
    static void addWay(Tile *t, int limit, int flags, const QVector<double> &lat, const QVector<double> &lng);
    int search(const Tile &t, double lat, double lng, double headingDeg, double *bestPerp) const;
    void clear();

    QVector<File> m_files;
    QHash<qint64, Tile> m_cache;
    Tile m_loose;
};

#endif
