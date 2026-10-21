/**
 * @file    MapDataSerializer.cpp
 * @layer   GUI
 * @brief   Implementation of GeoJSON serialization and parsing for topological route features.
 * @deps    MapDataSerializer.h, MapEditDataModel.h, RouteNetworkValidator.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapDataSerializer.h"
#include "MapEditDataModel.h"
#include "core/RouteNetworkValidator.h"
#include "utils/MapPgmUtil.h"
#include <QJsonDocument>

QJsonObject MapDataSerializer::buildRouteGeoJson(const QString &mapId, const MapEditDataModel *model)
{
    if (!model) return QJsonObject();
    QJsonArray areas = model->highLevelAreas();
    return RouteNetworkValidator::buildRouteGeoJson(
        mapId, model->points(), model->lines(), model->curves(),
        model->charges(), model->highPrecisionPoints(), areas);
}

int MapDataSerializer::nextIdNum(const QJsonArray &arr, const QString &prefix)
{
    int maxN = 0;
    for (const auto &v : arr) {
        QString id = v.toObject().value("id").toString();
        if (id.startsWith(prefix)) {
            bool ok = false;
            int n = id.mid(prefix.length()).toInt(&ok);
            if (ok && n > maxN) maxN = n;
        }
    }
    return maxN + 1;
}

static void normalizeSpeedProps(QJsonObject &props)
{
    if (props.contains("max_speed")) {
        if (!props.contains("maxspeed")) props["maxspeed"] = props.value("max_speed");
        props.remove("max_speed");
    }
    if (props.contains("checked_props")) {
        QJsonArray cp = props.value("checked_props").toArray();
        QJsonArray newCp;
        for (const auto &v : cp) {
            QString s = v.toString();
            if (s == "max_speed") s = "maxspeed";
            newCp.append(s);
        }
        props["checked_props"] = newCp;
    }
}

bool MapDataSerializer::parseRouteGeoJson(const QJsonObject &root, MapEditDataModel *model)
{
    if (!model) return false;

    // 1. 解析高级区域
    QJsonArray highLevelAreas;
    auto parseAreas = [&](const QJsonValue &v) {
        if (v.isArray()) {
            highLevelAreas = v.toArray();
        } else if (v.isObject() && !v.toObject().isEmpty()) {
            highLevelAreas.append(v.toObject());
        }
    };

    if (root.contains("highLevelArea")) parseAreas(root.value("highLevelArea"));
    else if (root.contains("highLevel Area")) parseAreas(root.value("highLevel Area"));
    else if (root.contains("highLevelAreas")) parseAreas(root.value("highLevelAreas"));

    for (int i = 0; i < highLevelAreas.size(); ++i) {
        QJsonObject a = highLevelAreas[i].toObject();
        normalizeSpeedProps(a);
        if (a.value("id").toString().isEmpty())
            a["id"] = QString("highLevelArea_%1").arg(i + 1);
        if (a.value("name").toString().isEmpty())
            a["name"] = QString("AdvancedArea %1").arg(i + 1);

        double minX = 0, maxX = 0, minY = 0, maxY = 0;
        MapPgmUtil::extractAreaBounds(a, minX, maxX, minY, maxY);

        QJsonArray coords, pTL, pBR;
        pTL.append(minX); pTL.append(maxY);
        pBR.append(maxX); pBR.append(minY);
        coords.append(pTL); coords.append(pBR);
        a["coordinates"] = coords;

        a.remove("minX"); a.remove("maxX");
        a.remove("minY"); a.remove("maxY");
        highLevelAreas[i] = a;
    }
    model->setHighLevelAreas(highLevelAreas);

    // 2. 兼容旧版扁平结构: {points:[...], lines:[...], curves:[...], ...}
    if (!root.contains("features") && (root.contains("points") || root.contains("lines") || root.contains("curves"))) {
        model->setPoints(root.value("points").toArray());
        model->setLines(root.value("lines").toArray());
        model->setCurves(root.value("curves").toArray());
        model->setCharges(root.value("charges").toArray());
        model->setHighPrecisionPoints(root.value("highPrecisionPoints").toArray());
        return true;
    }

    // 3. 标准 GeoJSON FeatureCollection 解析
    QJsonArray features = root.value("features").toArray();
    QJsonArray pts, lns, cvs, chgs, hps;

    for (const auto &fv : features) {
        QJsonObject f = fv.toObject();
        QJsonObject props = f.value("properties").toObject();
        normalizeSpeedProps(props);

        QJsonObject geom = f.value("geometry").toObject();
        QString gtype = geom.value("type").toString();
        QString id = props.value("id").toString();
        QJsonArray coords = geom.value("coordinates").toArray();

        if (gtype == "Point") {
            if (coords.size() < 2) continue;
            QJsonObject obj = props;
            obj["x"] = coords[0].toDouble();
            obj["y"] = coords[1].toDouble();

            bool isCharge = id.startsWith("c_", Qt::CaseInsensitive)
                || props.value("typeName").toString() == "ChargePoint"
                || props.value("type").toString() == "charge"
                || props.value("name").toString().startsWith("CP", Qt::CaseInsensitive)
                || props.value("name").toString().contains("充电");

            bool isHp = id.startsWith("hp_", Qt::CaseInsensitive)
                || props.value("typeName").toString() == "ActionPoint"
                || props.value("type").toString() == "highPrecision"
                || props.value("name").toString().startsWith("AP", Qt::CaseInsensitive)
                || props.value("name").toString().startsWith("TL", Qt::CaseInsensitive)
                || MapPgmUtil::isSpecialWorkstationPoint(obj);

            if (isCharge) {
                MapPgmUtil::forceZeroObstacleParams(obj);
                chgs.append(obj);
            } else if (isHp) {
                MapPgmUtil::forceZeroObstacleParams(obj);
                hps.append(obj);
            } else {
                pts.append(obj);
            }
        } else if (gtype == "LineString") {
            QJsonObject obj = props;
            if (coords.size() >= 2) {
                QJsonArray c0 = coords.first().toArray();
                QJsonArray c1 = coords.last().toArray();
                obj["x1"] = c0[0].toDouble(); obj["y1"] = c0[1].toDouble();
                obj["x2"] = c1[0].toDouble(); obj["y2"] = c1[1].toDouble();
            }
            lns.append(obj);
        } else if (gtype == "MultiPoint" && coords.size() >= 3) {
            QJsonObject obj = props;
            QJsonArray ctrlArr;
            for (const auto &cv : coords) {
                QJsonArray c = cv.toArray();
                if (c.size() < 2) continue;
                QJsonObject pt;
                pt["x"] = c[0].toDouble(); pt["y"] = c[1].toDouble();
                ctrlArr.append(pt);
            }
            obj["controlPoints"] = ctrlArr;
            QJsonArray c0 = coords.first().toArray();
            QJsonArray cN = coords.last().toArray();
            obj["x1"] = c0[0].toDouble(); obj["y1"] = c0[1].toDouble();
            obj["x2"] = cN[0].toDouble(); obj["y2"] = cN[1].toDouble();
            if (coords.size() == 3) {
                QJsonArray c1 = coords[1].toArray();
                obj["cx"] = c1[0].toDouble(); obj["cy"] = c1[1].toDouble();
            }
            cvs.append(obj);
        }
    }

    model->setPoints(pts);
    model->setLines(lns);
    model->setCurves(cvs);
    model->setCharges(chgs);
    model->setHighPrecisionPoints(hps);
    return true;
}

QJsonArray MapDataSerializer::parseReflectorJson(const QJsonObject &data)
{
    QJsonValue dataVal = data.value("data");
    QJsonArray features;

    if (dataVal.isArray()) {
        features = dataVal.toArray();
    } else if (dataVal.isObject()) {
        QJsonObject geo = dataVal.toObject();
        features = geo.value("features").toArray();
        if (features.isEmpty() && geo.value("type").toString() == "Feature")
            features.append(geo);
    } else if (dataVal.isString()) {
        QJsonDocument doc = QJsonDocument::fromJson(dataVal.toString().toUtf8());
        if (doc.isArray()) {
            features = doc.array();
        } else if (doc.isObject()) {
            QJsonObject geo = doc.object();
            features = geo.value("features").toArray();
            if (features.isEmpty() && geo.value("type").toString() == "Feature")
                features.append(geo);
        }
    }

    if (features.isEmpty()) {
        features = data.value("features").toArray();
    }
    return features;
}
