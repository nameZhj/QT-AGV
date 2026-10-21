/**
 * @file    RouteGeoJsonParser.cpp
 * @layer   DataComm
 * @brief   GeoJSON feature parser implementation for RouteNetwork.
 * @deps    RouteGeoJsonCodec.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RouteGeoJsonCodec.h"

void RouteGeoJsonCodec::parsePointFeature(
    const QJsonObject &properties,
    const QJsonArray &coordinates,
    RouteNetwork &outNetwork)
{
    const double x = (coordinates.size() > 0) ? coordinates[0].toDouble() : 0.0;
    const double y = (coordinates.size() > 1) ? coordinates[1].toDouble() : 0.0;
    QJsonObject fullObj = properties;
    fullObj[QStringLiteral("x")] = x;
    fullObj[QStringLiteral("y")] = y;
    const RoutePoint point = RoutePoint::fromJson(fullObj);
    const QString id = properties.value(QStringLiteral("id")).toString();

    if (id.startsWith(QStringLiteral("c_"), Qt::CaseInsensitive) ||
        point.type == QStringLiteral("charge") ||
        point.typeName == QStringLiteral("ChargePoint") ||
        point.name.startsWith(QStringLiteral("CP"), Qt::CaseInsensitive) ||
        point.name.contains(QStringLiteral("充电"))) {
        outNetwork.charges.append(point);
    } else if (id.startsWith(QStringLiteral("hp_"), Qt::CaseInsensitive) ||
               point.type == QStringLiteral("highPrecision") ||
               point.typeName == QStringLiteral("ActionPoint") ||
               point.name.startsWith(QStringLiteral("AP"), Qt::CaseInsensitive) ||
               point.name.startsWith(QStringLiteral("TL"), Qt::CaseInsensitive) ||
               point.isSpecial()) {
        outNetwork.highPrecisionPoints.append(point);
    } else {
        outNetwork.points.append(point);
    }
}

void RouteGeoJsonCodec::parseLineFeature(
    const QJsonObject &properties,
    const QJsonArray &coordinates,
    RouteNetwork &outNetwork)
{
    QJsonObject fullObj = properties;
    if (coordinates.size() >= 2) {
        const QJsonArray c0 = coordinates.first().toArray();
        const QJsonArray c1 = coordinates.last().toArray();
        fullObj[QStringLiteral("x1")] = c0[0].toDouble();
        fullObj[QStringLiteral("y1")] = c0[1].toDouble();
        fullObj[QStringLiteral("x2")] = c1[0].toDouble();
        fullObj[QStringLiteral("y2")] = c1[1].toDouble();
    }
    outNetwork.lines.append(RouteSegment::fromJson(fullObj));
}

void RouteGeoJsonCodec::parseCurveFeature(
    const QJsonObject &properties,
    const QJsonArray &coordinates,
    RouteNetwork &outNetwork)
{
    QJsonObject fullObj = properties;
    if (coordinates.size() >= 2) {
        QJsonArray ctrlArr;
        for (const auto &cv : coordinates) {
            const QJsonArray c = cv.toArray();
            if (c.size() < 2) continue;
            QJsonObject p;
            p[QStringLiteral("x")] = c[0].toDouble();
            p[QStringLiteral("y")] = c[1].toDouble();
            ctrlArr.append(p);
        }
        fullObj[QStringLiteral("controlPoints")] = ctrlArr;
        const QJsonArray c0 = coordinates.first().toArray();
        const QJsonArray cN = coordinates.last().toArray();
        fullObj[QStringLiteral("x1")] = c0[0].toDouble();
        fullObj[QStringLiteral("y1")] = c0[1].toDouble();
        fullObj[QStringLiteral("x2")] = cN[0].toDouble();
        fullObj[QStringLiteral("y2")] = cN[1].toDouble();
    }
    outNetwork.curves.append(RouteSegment::fromJson(fullObj));
}

RouteNetwork RouteGeoJsonCodec::fromGeoJson(const QJsonObject &root)
{
    RouteNetwork net;
    net.mapId   = root.value(QStringLiteral("mapId")).toString();
    net.mapName = root.value(QStringLiteral("mapName")).toString();

    if (!root.contains(QStringLiteral("features")) &&
        (root.contains(QStringLiteral("points")) || root.contains(QStringLiteral("lines")) || root.contains(QStringLiteral("curves")))) {
        for (const auto &v : root.value(QStringLiteral("points")).toArray())              net.points.append(RoutePoint::fromJson(v.toObject()));
        for (const auto &v : root.value(QStringLiteral("charges")).toArray())             net.charges.append(RoutePoint::fromJson(v.toObject()));
        for (const auto &v : root.value(QStringLiteral("highPrecisionPoints")).toArray()) net.highPrecisionPoints.append(RoutePoint::fromJson(v.toObject()));
        for (const auto &v : root.value(QStringLiteral("lines")).toArray())               net.lines.append(RouteSegment::fromJson(v.toObject()));
        for (const auto &v : root.value(QStringLiteral("curves")).toArray())              net.curves.append(RouteSegment::fromJson(v.toObject()));
        return net;
    }

    const QJsonArray features = root.value(QStringLiteral("features")).toArray();
    for (const auto &fv : features) {
        const QJsonObject f     = fv.toObject();
        const QJsonObject props = f.value(QStringLiteral("properties")).toObject();
        const QJsonObject geom  = f.value(QStringLiteral("geometry")).toObject();
        const QString gtype     = geom.value(QStringLiteral("type")).toString();
        const QJsonArray coords = geom.value(QStringLiteral("coordinates")).toArray();

        if (gtype == QStringLiteral("Point")) {
            parsePointFeature(props, coords, net);
        } else if (gtype == QStringLiteral("LineString")) {
            parseLineFeature(props, coords, net);
        } else if (gtype == QStringLiteral("MultiPoint")) {
            parseCurveFeature(props, coords, net);
        }
    }
    return net;
}
