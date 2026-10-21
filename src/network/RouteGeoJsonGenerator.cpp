/**
 * @file    RouteGeoJsonGenerator.cpp
 * @layer   DataComm
 * @brief   GeoJSON FeatureCollection generation implementation for RouteNetwork.
 * @deps    RouteGeoJsonCodec.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RouteGeoJsonCodec.h"
#include "MotionConfigManager.h"

void RouteGeoJsonCodec::collectStationObstacleOverrides(
    const RouteNetwork &network,
    const QSet<QString> &specialIds,
    QMap<QString, ObstacleTuple> &stationObstacles)
{
    const auto &mc = MotionConfigManager::instance();

    auto collect = [&](const RouteSegment &seg) {
        const bool sIsSpecial = specialIds.contains(seg.startId) ||
            seg.startId.startsWith(QStringLiteral("hp_"), Qt::CaseInsensitive) ||
            seg.startId.startsWith(QStringLiteral("c_"), Qt::CaseInsensitive) ||
            seg.startId.startsWith(QStringLiteral("ap_"), Qt::CaseInsensitive) ||
            seg.startId.startsWith(QStringLiteral("cp_"), Qt::CaseInsensitive);

        const bool eIsSpecial = specialIds.contains(seg.endId) ||
            seg.endId.startsWith(QStringLiteral("hp_"), Qt::CaseInsensitive) ||
            seg.endId.startsWith(QStringLiteral("c_"), Qt::CaseInsensitive) ||
            seg.endId.startsWith(QStringLiteral("ap_"), Qt::CaseInsensitive) ||
            seg.endId.startsWith(QStringLiteral("cp_"), Qt::CaseInsensitive);

        double segStopLen  = seg.intrusionStopLength;
        double segStopWid  = seg.intrusionStopWidth;
        double segDecelLen = seg.intrusionDecelLength;
        double segDecelWid = seg.intrusionDecelWidth;

        if (!seg.isCustomObstacle && segStopLen <= 0.0 && segStopWid <= 0.0 && segDecelLen <= 0.0 && segDecelWid <= 0.0) {
            segStopLen  = mc.defaultObsStopLength();
            segStopWid  = mc.defaultObsStopWidth();
            segDecelLen = mc.defaultObsDecelLength();
            segDecelWid = mc.defaultObsDecelWidth();
        }

        auto updateStation = [&](const QString &id) {
            if (id.isEmpty() || specialIds.contains(id)) return;
            auto &t = stationObstacles[id];
            t.stopLength  = qMax(t.stopLength, segStopLen);
            t.stopWidth   = qMax(t.stopWidth, segStopWid);
            t.decelLength = qMax(t.decelLength, segDecelLen);
            t.decelWidth  = qMax(t.decelWidth, segDecelWid);
        };

        if (sIsSpecial && !eIsSpecial) {
            updateStation(seg.endId);
        } else if (!sIsSpecial && eIsSpecial) {
            updateStation(seg.startId);
        } else if (!sIsSpecial && !eIsSpecial) {
            updateStation(seg.startId);
            updateStation(seg.endId);
        }
    };

    for (const auto &ln : network.lines)  collect(ln);
    for (const auto &cv : network.curves) collect(cv);
}

QJsonObject RouteGeoJsonCodec::buildPointFeature(
    RoutePoint pt,
    bool isSpecial,
    const QSet<QString> &specialIds,
    const QMap<QString, ObstacleTuple> &stationObstacles)
{
    if (isSpecial || pt.isSpecial() || specialIds.contains(pt.id)) {
        isSpecial = true;
        pt.intrusionStopLength  = 0.0;
        pt.intrusionStopWidth   = 0.0;
        pt.intrusionDecelLength = 0.0;
        pt.intrusionDecelWidth  = 0.0;
        pt.isCustomObstacle     = false;
    } else if (pt.isCustomObstacle) {
        // preserve custom obstacle
    } else if (stationObstacles.contains(pt.id)) {
        const auto &obs = stationObstacles[pt.id];
        pt.intrusionStopLength  = obs.stopLength;
        pt.intrusionStopWidth   = obs.stopWidth;
        pt.intrusionDecelLength = obs.decelLength;
        pt.intrusionDecelWidth  = obs.decelWidth;
    } else {
        const auto &mc = MotionConfigManager::instance();
        if (pt.intrusionStopLength <= 0.0 && pt.intrusionStopWidth <= 0.0 &&
            pt.intrusionDecelLength <= 0.0 && pt.intrusionDecelWidth <= 0.0) {
            pt.intrusionStopLength  = mc.defaultObsStopLength();
            pt.intrusionStopWidth   = mc.defaultObsStopWidth();
            pt.intrusionDecelLength = mc.defaultObsDecelLength();
            pt.intrusionDecelWidth  = mc.defaultObsDecelWidth();
        }
    }

    QJsonArray coords;
    coords.append(pt.x);
    coords.append(pt.y);
    coords.append(kDefaultZCoordinate);

    QJsonObject geom;
    geom[QStringLiteral("type")] = QStringLiteral("Point");
    geom[QStringLiteral("coordinates")] = coords;

    QJsonObject props = pt.toJson();
    props.remove(QStringLiteral("x"));
    props.remove(QStringLiteral("y"));

    if (isSpecial) {
        props[QStringLiteral("intrusion_stop_length")]         = 0.0;
        props[QStringLiteral("intrusion_stop_width")]          = 0.0;
        props[QStringLiteral("intrusion_deceleration_length")] = 0.0;
        props[QStringLiteral("intrusion_deceleration_width")]  = 0.0;
        props[QStringLiteral("is_custom_obstacle")]            = false;
        if (props.contains(QStringLiteral("actions")) && props.value(QStringLiteral("actions")).isArray()) {
            QJsonArray acts = props.value(QStringLiteral("actions")).toArray();
            for (int i = 0; i < acts.size(); ++i) {
                QJsonObject act = acts[i].toObject();
                if (act.value(QStringLiteral("action_type")).toInt() == kActionTypeObstacle ||
                    act.value(QStringLiteral("name")).toString() == QStringLiteral("实时避障范围")) {
                    act[QStringLiteral("name")] = QStringLiteral("实时避障范围");
                    act[QStringLiteral("action_type")] = kActionTypeObstacle;
                    act[QStringLiteral("intrusion_stop_length")]         = 0.0;
                    act[QStringLiteral("intrusion_stop_width")]          = 0.0;
                    act[QStringLiteral("intrusion_deceleration_length")] = 0.0;
                    act[QStringLiteral("intrusion_deceleration_width")]  = 0.0;
                    act[QStringLiteral("is_custom_obstacle")]            = false;
                    acts[i] = act;
                }
            }
            props[QStringLiteral("actions")] = acts;
        }
    }

    QJsonObject feature;
    feature[QStringLiteral("type")] = QStringLiteral("Feature");
    feature[QStringLiteral("geometry")] = geom;
    feature[QStringLiteral("properties")] = props;
    return feature;
}

QJsonObject RouteGeoJsonCodec::buildLineFeature(RouteSegment line)
{
    if (!line.isCustomObstacle &&
        line.intrusionStopLength <= 0.0 && line.intrusionStopWidth <= 0.0 &&
        line.intrusionDecelLength <= 0.0 && line.intrusionDecelWidth <= 0.0) {
        const auto &mc = MotionConfigManager::instance();
        line.intrusionStopLength  = mc.defaultObsStopLength();
        line.intrusionStopWidth   = mc.defaultObsStopWidth();
        line.intrusionDecelLength = mc.defaultObsDecelLength();
        line.intrusionDecelWidth  = mc.defaultObsDecelWidth();
    }

    QJsonArray ca, cb;
    ca.append(line.startPos.x()); ca.append(line.startPos.y()); ca.append(kDefaultZCoordinate);
    cb.append(line.endPos.x());   cb.append(line.endPos.y());   cb.append(kDefaultZCoordinate);
    QJsonArray coords;
    coords.append(ca);
    coords.append(cb);

    QJsonObject geom;
    geom[QStringLiteral("type")] = QStringLiteral("LineString");
    geom[QStringLiteral("coordinates")] = coords;

    QJsonObject props = line.toJson();
    props.remove(QStringLiteral("x1"));
    props.remove(QStringLiteral("y1"));
    props.remove(QStringLiteral("x2"));
    props.remove(QStringLiteral("y2"));

    QJsonObject feature;
    feature[QStringLiteral("type")] = QStringLiteral("Feature");
    feature[QStringLiteral("geometry")] = geom;
    feature[QStringLiteral("properties")] = props;
    return feature;
}

QJsonObject RouteGeoJsonCodec::buildCurveFeature(RouteSegment curve)
{
    if (!curve.isCustomObstacle &&
        curve.intrusionStopLength <= 0.0 && curve.intrusionStopWidth <= 0.0 &&
        curve.intrusionDecelLength <= 0.0 && curve.intrusionDecelWidth <= 0.0) {
        const auto &mc = MotionConfigManager::instance();
        curve.intrusionStopLength  = mc.defaultObsStopLength();
        curve.intrusionStopWidth   = mc.defaultObsStopWidth();
        curve.intrusionDecelLength = mc.defaultObsDecelLength();
        curve.intrusionDecelWidth  = mc.defaultObsDecelWidth();
    }

    QJsonArray coords;
    if (!curve.controlPts.isEmpty()) {
        for (const auto &p : curve.controlPts) {
            QJsonArray c;
            c.append(p.x()); c.append(p.y()); c.append(kDefaultZCoordinate);
            coords.append(c);
        }
    } else {
        QJsonArray c0, c1;
        c0.append(curve.startPos.x()); c0.append(curve.startPos.y()); c0.append(kDefaultZCoordinate);
        c1.append(curve.endPos.x());   c1.append(curve.endPos.y());   c1.append(kDefaultZCoordinate);
        coords.append(c0); coords.append(c1);
    }

    QJsonObject geom;
    geom[QStringLiteral("type")] = QStringLiteral("MultiPoint");
    geom[QStringLiteral("coordinates")] = coords;

    QJsonObject props = curve.toJson();
    props.remove(QStringLiteral("x1"));
    props.remove(QStringLiteral("y1"));
    props.remove(QStringLiteral("x2"));
    props.remove(QStringLiteral("y2"));
    props.remove(QStringLiteral("controlPoints"));

    QJsonObject feature;
    feature[QStringLiteral("type")] = QStringLiteral("Feature");
    feature[QStringLiteral("geometry")] = geom;
    feature[QStringLiteral("properties")] = props;
    return feature;
}

QJsonObject RouteGeoJsonCodec::toGeoJson(const RouteNetwork &network)
{
    QSet<QString> specialIds;
    for (const auto &pt : network.charges)             specialIds.insert(pt.id);
    for (const auto &pt : network.highPrecisionPoints) specialIds.insert(pt.id);
    for (const auto &pt : network.points) {
        if (pt.isSpecial()) specialIds.insert(pt.id);
    }

    QMap<QString, ObstacleTuple> stationObstacles;
    collectStationObstacleOverrides(network, specialIds, stationObstacles);

    QJsonArray features;
    for (const auto &pt : network.points)              features.append(buildPointFeature(pt, pt.isSpecial(), specialIds, stationObstacles));
    for (const auto &pt : network.charges)             features.append(buildPointFeature(pt, true, specialIds, stationObstacles));
    for (const auto &pt : network.highPrecisionPoints) features.append(buildPointFeature(pt, true, specialIds, stationObstacles));

    for (const auto &line : network.lines)   features.append(buildLineFeature(line));
    for (const auto &curve : network.curves) features.append(buildCurveFeature(curve));

    QJsonObject root;
    root[QStringLiteral("type")]     = QStringLiteral("FeatureCollection");
    root[QStringLiteral("mapId")]    = network.mapId;
    root[QStringLiteral("mapName")]  = network.mapName;
    root[QStringLiteral("features")] = features;
    return root;
}
