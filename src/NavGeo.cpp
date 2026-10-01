#include "NavGeo.h"

#include <cmath>

namespace {
constexpr double kEarthRadius = 6371000.0;
constexpr double kDegToRad = M_PI / 180.0;

// Local flat (x=east, y=north) metres of `p` relative to origin `o`, valid
// only for p close to o (city-scale distances), which is always true here
// since callers only ever project against nearby route segments.
void toLocalXY(double oLat, double oLng, double lat, double lng, double *x, double *y)
{
    const double latRad = oLat * kDegToRad;
    *x = (lng - oLng) * kDegToRad * std::cos(latRad) * kEarthRadius;
    *y = (lat - oLat) * kDegToRad * kEarthRadius;
}

void fromLocalXY(double oLat, double oLng, double x, double y, double *lat, double *lng)
{
    const double latRad = oLat * kDegToRad;
    *lat = oLat + (y / kEarthRadius) / kDegToRad;
    *lng = oLng + (x / (kEarthRadius * std::cos(latRad))) / kDegToRad;
}

}

double NavGeo::metersBetween(double lat1, double lng1, double lat2, double lng2)
{
    double x, y;
    toLocalXY(lat1, lng1, lat2, lng2, &x, &y);
    return std::sqrt(x * x + y * y);
}

double NavGeo::bearingDegrees(double lat1, double lng1, double lat2, double lng2)
{
    double x, y;
    toLocalXY(lat1, lng1, lat2, lng2, &x, &y);
    if (x == 0.0 && y == 0.0)
        return 0.0;
    double deg = std::atan2(x, y) / kDegToRad;
    if (deg < 0) deg += 360.0;
    return deg;
}

NavGeo::Projection NavGeo::projectOntoSegment(double lat, double lng,
                                              double aLat, double aLng, double bLat, double bLng)
{
    double px, py, bx, by;
    toLocalXY(aLat, aLng, lat, lng, &px, &py);
    toLocalXY(aLat, aLng, bLat, bLng, &bx, &by);

    const double segLenSq = bx * bx + by * by;
    double t = segLenSq > 1e-9 ? (px * bx + py * by) / segLenSq : 0.0;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    const double cx = bx * t;
    const double cy = by * t;

    Projection result;
    fromLocalXY(aLat, aLng, cx, cy, &result.lat, &result.lng);
    result.t = t;
    result.distanceFromStart = std::sqrt(cx * cx + cy * cy);
    const double dx = px - cx, dy = py - cy;
    result.perpDistance = std::sqrt(dx * dx + dy * dy);
    return result;
}
