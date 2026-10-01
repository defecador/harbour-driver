#include "FlexiblePolyline.h"

#include <cmath>

namespace {

int decodeChar(QChar c)
{
    const ushort u = c.unicode();
    if (u >= 'A' && u <= 'Z') return u - 'A';
    if (u >= 'a' && u <= 'z') return u - 'a' + 26;
    if (u >= '0' && u <= '9') return u - '0' + 52;
    if (u == '-') return 62;
    if (u == '_') return 63;
    return -1;
}

// Reads one unsigned varint starting at pos; advances pos. Returns false on error.
bool readUnsigned(const QString &s, int &pos, qint64 &out)
{
    qint64 result = 0;
    int shift = 0;
    while (pos < s.size()) {
        const int v = decodeChar(s.at(pos++));
        if (v < 0 || shift > 60)
            return false;
        result |= qint64(v & 0x1F) << shift;
        if ((v & 0x20) == 0) {
            out = result;
            return true;
        }
        shift += 5;
    }
    return false;
}

bool readSigned(const QString &s, int &pos, qint64 &out)
{
    qint64 v;
    if (!readUnsigned(s, pos, v))
        return false;
    if (v & 1)
        v = ~v;
    out = v >> 1;
    return true;
}

}

QVector<FlexiblePolyline::Point> FlexiblePolyline::decode(const QString &encoded, int *thirdDimOut)
{
    QVector<Point> points;
    int pos = 0;
    qint64 version, header;
    if (!readUnsigned(encoded, pos, version) || version != 1)
        return points;
    if (!readUnsigned(encoded, pos, header))
        return points;

    const int precision = int(header & 15);
    const int thirdDim = int((header >> 4) & 7);
    const int thirdPrecision = int((header >> 7) & 15);
    if (thirdDimOut)
        *thirdDimOut = thirdDim;

    const double mult = std::pow(10.0, precision);
    const double mult3 = std::pow(10.0, thirdPrecision);
    qint64 lat = 0, lng = 0, z = 0;

    while (pos < encoded.size()) {
        qint64 d;
        if (!readSigned(encoded, pos, d)) return QVector<Point>();
        lat += d;
        if (!readSigned(encoded, pos, d)) return QVector<Point>();
        lng += d;
        if (thirdDim) {
            if (!readSigned(encoded, pos, d)) return QVector<Point>();
            z += d;
        }
        points.append(Point{lat / mult, lng / mult, thirdDim ? z / mult3 : 0.0});
    }
    return points;
}
