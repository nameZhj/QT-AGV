/**
 * @file    RouteNetworkApi.cpp
 * @layer   DataComm
 * @brief   Implementation of route network topology client, validation, and directed graph.
 * @deps    RouteNetworkApi.h, RouteGeoJsonCodec.h, RouteSmapConverter.h, ApiService.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RouteNetworkApi.h"
#include "RouteGeoJsonCodec.h"
#include "RouteSmapConverter.h"
#include "ApiService.h"
#include "MapPgmUtil.h"

#include <cmath>
#include <QJsonDocument>
#include <QSet>

// ==============================================================================
// === [Section: GeoJSON & SMAP Forwarding] ===
// ==============================================================================

RouteNetwork RouteNetworkApi::fromGeoJson(const QJsonObject &root)
{
    return RouteGeoJsonCodec::fromGeoJson(root);
}

QJsonObject RouteNetworkApi::toGeoJson(const RouteNetwork &network)
{
    return RouteGeoJsonCodec::toGeoJson(network);
}

bool RouteNetworkApi::parseSmap(const QJsonObject &smapObj, RouteNetwork &outNetwork)
{
    return RouteSmapConverter::parseSmap(smapObj, outNetwork);
}

QJsonObject RouteNetworkApi::exportSmap(const RouteNetwork &network)
{
    return RouteSmapConverter::exportSmap(network);
}

// ==============================================================================
// === [Section: Directed Graph Expansion] ===
// ==============================================================================

QList<DirectedEdge> RouteNetworkApi::toDirectedGraph(const RouteNetwork &network)
{
    QList<DirectedEdge> graph;
    constexpr int kBezierSampleCount = 30;

    auto processSegment = [&](const RouteSegment &seg) {
        if (seg.startId.isEmpty() || seg.endId.isEmpty()) return;

        double len = 0.0;
        QList<QPointF> forwardPts;
        if (seg.type == QStringLiteral("curve") && seg.controlPts.size() >= 2) {
            QPointF prev = seg.controlPts.first();
            forwardPts.append(prev);
            for (int i = 1; i <= kBezierSampleCount; ++i) {
                const double t = double(i) / kBezierSampleCount;
                const QPointF curr = MapPgmUtil::evaluateBezier(seg.controlPts, t);
                len += std::hypot(curr.x() - prev.x(), curr.y() - prev.y());
                forwardPts.append(curr);
                prev = curr;
            }
        } else {
            len = std::hypot(seg.endPos.x() - seg.startPos.x(), seg.endPos.y() - seg.startPos.y());
            forwardPts = {seg.startPos, seg.endPos};
        }

        const bool hasForward  = (seg.direct == 0 || seg.direct == 10 || seg.direct == 2);
        const bool hasBackward = (seg.direct == 1 || seg.direct == 10 || seg.direct == 2);

        if (hasForward) {
            DirectedEdge ef;
            ef.edgeId         = seg.id + QStringLiteral("_fwd");
            ef.sourcePointId  = seg.startId;
            ef.targetPointId  = seg.endId;
            ef.lengthMeters   = len;
            ef.maxSpeed       = seg.maxSpeed;
            ef.weight         = seg.weight;
            ef.isReverse      = false;
            ef.pathPoints     = forwardPts;
            graph.append(ef);
        }

        if (hasBackward) {
            DirectedEdge eb;
            eb.edgeId         = seg.id + QStringLiteral("_rev");
            eb.sourcePointId  = seg.endId;
            eb.targetPointId  = seg.startId;
            eb.lengthMeters   = len;
            eb.maxSpeed       = seg.maxSpeed;
            eb.weight         = seg.weight;
            eb.isReverse      = true;
            for (int i = forwardPts.size() - 1; i >= 0; --i) {
                eb.pathPoints.append(forwardPts[i]);
            }
            graph.append(eb);
        }
    };

    for (const auto &line : network.lines)   processSegment(line);
    for (const auto &curve : network.curves) processSegment(curve);

    return graph;
}

// ==============================================================================
// === [Section: Validation & Asynchronous Network IO] ===
// ==============================================================================

bool RouteNetworkApi::validate(const RouteNetwork &network, QStringList &warnings, QStringList &errors)
{
    QSet<QString> pointIds;
    for (const auto &p : network.points)              pointIds.insert(p.id);
    for (const auto &p : network.charges)             pointIds.insert(p.id);
    for (const auto &p : network.highPrecisionPoints) pointIds.insert(p.id);

    if (pointIds.isEmpty()) {
        errors.append(QStringLiteral("路网中无任何有效站点 (Point/Charge/HighPrecision)"));
    }

    auto checkSeg = [&](const RouteSegment &seg) {
        if (seg.startId.isEmpty() || seg.endId.isEmpty()) {
            errors.append(QStringLiteral("线路 %1 未指定起终点").arg(seg.id));
        } else {
            if (seg.startId == seg.endId) {
                warnings.append(QStringLiteral("线路 %1 存在自环 (起终点均为 %2)").arg(seg.id, seg.startId));
            }
            if (!pointIds.contains(seg.startId)) {
                warnings.append(QStringLiteral("线路 %1 的起点 %2 不在站点列表中").arg(seg.id, seg.startId));
            }
            if (!pointIds.contains(seg.endId)) {
                warnings.append(QStringLiteral("线路 %1 的终点 %2 不在站点列表中").arg(seg.id, seg.endId));
            }
        }
        if (seg.maxSpeed <= 0.0) {
            warnings.append(QStringLiteral("线路 %1 最大速度为 0 或负数").arg(seg.id));
        }
    };

    for (const auto &line : network.lines)   checkSeg(line);
    for (const auto &curve : network.curves) checkSeg(curve);

    return errors.isEmpty();
}

void RouteNetworkApi::loadNetwork(const QString &mapId, LoadCallback cb)
{
    Q_UNUSED(mapId);
    ApiService::instance().loadEditedMap([cb](int code, const QJsonObject &resp) {
        if (code != 200) {
            if (cb) cb(false, RouteNetwork(), QStringLiteral("HTTP 请求失败或接口异常"));
            return;
        }
        const QString jsonStr = resp.value(QStringLiteral("json")).toString();
        const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (!doc.isObject()) {
            if (cb) cb(false, RouteNetwork(), QStringLiteral("解析返回路网 JSON 数据失败"));
            return;
        }
        const RouteNetwork net = fromGeoJson(doc.object());
        if (cb) cb(true, net, QString());
    });
}

void RouteNetworkApi::saveNetwork(const QString &mapId, const QString &mapName, const RouteNetwork &network, SaveCallback cb)
{
    RouteNetwork net = network;
    net.mapId   = mapId;
    net.mapName = mapName;

    const QJsonObject geojson = toGeoJson(net);
    ApiService::instance().saveEditedMap(geojson, [cb](int code, const QJsonObject &resp) {
        if (code == 200) {
            if (cb) cb(true, QStringLiteral("路网保存成功"));
        } else {
            const QString msg = resp.value(QStringLiteral("msg")).toString(QStringLiteral("保存路网失败"));
            if (cb) cb(false, msg);
        }
    });
}
