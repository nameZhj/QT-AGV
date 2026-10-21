/**
 * @file    RouteGeoJsonBuilder.cpp
 * @layer   CoreScheduling
 * @brief   Implementation of GeoJSON FeatureCollection serialization for route network.
 * @deps    RouteGeoJsonBuilder.h, RouteNetworkValidator.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "RouteGeoJsonBuilder.h"
#include "RouteNetworkValidator.h"
#include "utils/MapPgmUtil.h"

namespace RouteGeoJsonBuilder {

QJsonObject buildRouteGeoJson(const QString &mapId,
                              const QJsonArray &points,
                              const QJsonArray &lines,
                              const QJsonArray &curves,
                              const QJsonArray &charges,
                              const QJsonArray &highPrecisionPoints,
                              QJsonArray &highLevelAreas)
{
    QJsonArray features;

    auto makePointFeat = [](const QJsonObject &obj) -> QJsonObject {
        QJsonObject props = obj;
        if (MapPgmUtil::isSpecialWorkstationPoint(props)) {
            MapPgmUtil::forceZeroObstacleParams(props);
        }
        if (props.contains(QStringLiteral("max_speed"))) {
            if (!props.contains(QStringLiteral("maxspeed"))) {
                props[QStringLiteral("maxspeed")] = props.value(QStringLiteral("max_speed"));
            }
            props.remove(QStringLiteral("max_speed"));
        }
        QJsonArray coords;
        coords.append(props.value(QStringLiteral("x")).toDouble());
        coords.append(props.value(QStringLiteral("y")).toDouble());
        coords.append(0.0);
        QJsonObject geom;
        geom[QStringLiteral("type")] = QStringLiteral("Point");
        geom[QStringLiteral("coordinates")] = coords;
        props.remove(QStringLiteral("x"));
        props.remove(QStringLiteral("y"));
        QJsonObject f;
        f[QStringLiteral("type")] = QStringLiteral("Feature");
        f[QStringLiteral("geometry")] = geom;
        f[QStringLiteral("properties")] = props;
        return f;
    };

    for (const auto &v : points)              features.append(makePointFeat(v.toObject()));
    for (const auto &v : charges)             features.append(makePointFeat(v.toObject()));
    for (const auto &v : highPrecisionPoints) features.append(makePointFeat(v.toObject()));

    for (const auto &v : lines) {
        const QJsonObject ln = v.toObject();
        QJsonArray ca, cb;
        ca.append(ln.value(QStringLiteral("x1")).toDouble());
        ca.append(ln.value(QStringLiteral("y1")).toDouble());
        cb.append(ln.value(QStringLiteral("x2")).toDouble());
        cb.append(ln.value(QStringLiteral("y2")).toDouble());
        QJsonArray coords;
        coords.append(ca);
        coords.append(cb);
        QJsonObject geom;
        geom[QStringLiteral("type")] = QStringLiteral("LineString");
        geom[QStringLiteral("coordinates")] = coords;
        QJsonObject props = ln;
        if (props.contains(QStringLiteral("max_speed"))) {
            if (!props.contains(QStringLiteral("maxspeed"))) {
                props[QStringLiteral("maxspeed")] = props.value(QStringLiteral("max_speed"));
            }
            props.remove(QStringLiteral("max_speed"));
        }
        props.remove(QStringLiteral("x1"));
        props.remove(QStringLiteral("y1"));
        props.remove(QStringLiteral("x2"));
        props.remove(QStringLiteral("y2"));
        QJsonObject f;
        f[QStringLiteral("type")] = QStringLiteral("Feature");
        f[QStringLiteral("geometry")] = geom;
        f[QStringLiteral("properties")] = props;
        features.append(f);
    }

    for (const auto &v : curves) {
        const QJsonObject cv = v.toObject();
        QJsonArray coords;
        const QJsonArray ctrlArr = cv.value(QStringLiteral("controlPoints")).toArray();
        if (!ctrlArr.isEmpty()) {
            for (const auto &pv : ctrlArr) {
                const QJsonObject pt = pv.toObject();
                QJsonArray c;
                c.append(pt.value(QStringLiteral("x")).toDouble());
                c.append(pt.value(QStringLiteral("y")).toDouble());
                coords.append(c);
            }
        } else {
            QJsonArray c0, c1, c2;
            c0.append(cv.value(QStringLiteral("x1")).toDouble());
            c0.append(cv.value(QStringLiteral("y1")).toDouble());
            c1.append(cv.value(QStringLiteral("cx")).toDouble());
            c1.append(cv.value(QStringLiteral("cy")).toDouble());
            c2.append(cv.value(QStringLiteral("x2")).toDouble());
            c2.append(cv.value(QStringLiteral("y2")).toDouble());
            coords.append(c0);
            coords.append(c1);
            coords.append(c2);
        }

        QJsonObject geom;
        geom[QStringLiteral("type")] = QStringLiteral("MultiPoint");
        geom[QStringLiteral("coordinates")] = coords;
        QJsonObject props = cv;
        if (props.contains(QStringLiteral("max_speed"))) {
            if (!props.contains(QStringLiteral("maxspeed"))) {
                props[QStringLiteral("maxspeed")] = props.value(QStringLiteral("max_speed"));
            }
            props.remove(QStringLiteral("max_speed"));
        }
        props.remove(QStringLiteral("x1"));
        props.remove(QStringLiteral("y1"));
        props.remove(QStringLiteral("cx"));
        props.remove(QStringLiteral("cy"));
        props.remove(QStringLiteral("x2"));
        props.remove(QStringLiteral("y2"));
        props.remove(QStringLiteral("controlPoints"));
        QJsonObject f;
        f[QStringLiteral("type")] = QStringLiteral("Feature");
        f[QStringLiteral("geometry")] = geom;
        f[QStringLiteral("properties")] = props;
        features.append(f);
    }

    QJsonObject geojson;
    geojson[QStringLiteral("type")]     = QStringLiteral("FeatureCollection");
    geojson[QStringLiteral("features")] = features;
    geojson[QStringLiteral("mapId")]    = mapId;

    QJsonArray saveHighLevelAreas;
    for (int i = 0; i < highLevelAreas.size(); ++i) {
        QJsonObject areaObj = highLevelAreas[i].toObject();
        if (areaObj.contains(QStringLiteral("max_speed"))) {
            if (!areaObj.contains(QStringLiteral("maxspeed"))) {
                areaObj[QStringLiteral("maxspeed")] = areaObj.value(QStringLiteral("max_speed"));
            }
            areaObj.remove(QStringLiteral("max_speed"));
        }
        if (areaObj.contains(QStringLiteral("checked_props"))) {
            const QJsonArray cp = areaObj.value(QStringLiteral("checked_props")).toArray();
            QJsonArray newCp;
            for (const auto &v : cp) {
                QString s = v.toString();
                if (s == QStringLiteral("max_speed")) s = QStringLiteral("maxspeed");
                newCp.append(s);
            }
            areaObj[QStringLiteral("checked_props")] = newCp;
        }
        double minX = 0, maxX = 0, minY = 0, maxY = 0;
        RouteNetworkValidator::extractAreaBounds(areaObj, minX, maxX, minY, maxY);

        QJsonArray coords;
        QJsonArray pTL, pBR;
        pTL.append(minX); pTL.append(maxY);
        pBR.append(maxX); pBR.append(minY);
        coords.append(pTL); coords.append(pBR);
        areaObj[QStringLiteral("coordinates")] = coords;

        areaObj.remove(QStringLiteral("minX"));
        areaObj.remove(QStringLiteral("maxX"));
        areaObj.remove(QStringLiteral("minY"));
        areaObj.remove(QStringLiteral("maxY"));

        saveHighLevelAreas.append(areaObj);
    }
    geojson[QStringLiteral("highLevelArea")] = saveHighLevelAreas;

    auto cleanMaxSpeed = [](auto &self, QJsonValue val) -> QJsonValue {
        if (val.isObject()) {
            QJsonObject obj = val.toObject();
            if (obj.contains(QStringLiteral("max_speed"))) {
                if (!obj.contains(QStringLiteral("maxspeed"))) {
                    obj[QStringLiteral("maxspeed")] = obj.value(QStringLiteral("max_speed"));
                }
                obj.remove(QStringLiteral("max_speed"));
            }
            for (const QString &k : obj.keys()) {
                obj[k] = self(self, obj.value(k));
            }
            return obj;
        } else if (val.isArray()) {
            QJsonArray arr = val.toArray();
            for (int i = 0; i < arr.size(); ++i) {
                if (arr[i].isString() && arr[i].toString() == QStringLiteral("max_speed")) {
                    arr[i] = QStringLiteral("maxspeed");
                } else {
                    arr[i] = self(self, arr[i]);
                }
            }
            return arr;
        }
        return val;
    };

    geojson = cleanMaxSpeed(cleanMaxSpeed, geojson).toObject();
    highLevelAreas = geojson.value(QStringLiteral("highLevelArea")).toArray();
    return geojson;
}

} // namespace RouteGeoJsonBuilder
