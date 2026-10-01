#ifndef FLEXIBLEPOLYLINE_H
#define FLEXIBLEPOLYLINE_H

#include <QString>
#include <QVector>
#include <QPointF>

// Decoder for HERE's Flexible Polyline encoding.
// Spec and reference implementations (MIT): https://github.com/heremaps/flexible-polyline
namespace FlexiblePolyline {

struct Point {
    double lat;
    double lng;
    double z;
};

// Returns an empty vector if the input is malformed.
QVector<Point> decode(const QString &encoded, int *thirdDim = nullptr);

}

#endif
