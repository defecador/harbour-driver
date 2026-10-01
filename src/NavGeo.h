#ifndef NAVGEO_H
#define NAVGEO_H

// Small, pure geometry helpers used by Navigator, split out so they can be
// unit tested without constructing a full Navigator (which needs AppSettings
// and HereClient). Flat-earth (equirectangular) approximation throughout:
// accurate to well under 1% at city scale, which is all navigation needs.
namespace NavGeo {

// Distance in metres between two WGS84 points.
double metersBetween(double lat1, double lng1, double lat2, double lng2);

// Bearing in degrees [0,360) from point 1 to point 2, 0 = north.
double bearingDegrees(double lat1, double lng1, double lat2, double lng2);

struct Projection {
    double lat = 0;
    double lng = 0;
    double t = 0;                  // 0..1 fraction along the segment A->B
    double distanceFromStart = 0;  // metres from A to the projected point
    double perpDistance = 0;       // metres from (lat,lng) to the projected point
};

// Projects (lat,lng) onto the segment A->B, clamping t to [0,1] (i.e. this
// never extrapolates past either endpoint).
Projection projectOntoSegment(double lat, double lng,
                              double aLat, double aLng, double bLat, double bLng);

}

#endif
