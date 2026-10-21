/**
 * @file    RouteObstacleSync.cpp
 * @layer   CoreScheduling
 * @brief   Implementation of line and curve obstacle parameter synchronization.
 * @deps    RouteObstacleSync.h, MotionConfigManager.h, MapPgmUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */
#include "RouteObstacleSync.h"
#include "core/MotionConfigManager.h"
#include "utils/MapPgmUtil.h"
#include <QSet>
#include <QMap>
#include <QJsonObject>

namespace RouteObstacleSync {

void syncLineObstaclesToEndpoints(QJsonArray &points,
                                  QJsonArray &lines,
                                  QJsonArray &curves,
                                  QJsonArray &charges,
                                  QJsonArray &highPrecisionPoints)
{
    QSet<QString> specialIds;
    for (const auto &v : charges) {
        const QString id = v.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) specialIds.insert(id);
    }
    for (const auto &v : highPrecisionPoints) {
        const QString id = v.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) specialIds.insert(id);
    }
    for (const auto &v : points) {
        const QJsonObject o = v.toObject();
        if (MapPgmUtil::isSpecialWorkstationPoint(o)) {
            const QString id = o.value(QStringLiteral("id")).toString();
            if (!id.isEmpty()) specialIds.insert(id);
        }
    }

    struct ObstacleParam {
        double stopLen  = 0.0;
        double stopWid  = 0.0;
        double decelLen = 0.0;
        double decelWid = 0.0;
        bool isCustom   = false;
    };
    QMap<QString, ObstacleParam> normalStationObsMap;

    auto isNodeSpecial = [&](const QString &nodeId) -> bool {
        if (nodeId.isEmpty()) return false;
        if (specialIds.contains(nodeId)) return true;
        if (nodeId.startsWith(QStringLiteral("hp_"), Qt::CaseInsensitive) ||
            nodeId.startsWith(QStringLiteral("c_"), Qt::CaseInsensitive) ||
            nodeId.startsWith(QStringLiteral("ap_"), Qt::CaseInsensitive) ||
            nodeId.startsWith(QStringLiteral("cp_"), Qt::CaseInsensitive)) {
            return true;
        }
        return false;
    };

    const auto &mc = MotionConfigManager::instance();

    auto collectLineObstacle = [&](const QJsonArray &arr) {
        for (const auto &v : arr) {
            const QJsonObject ln = v.toObject();
            const bool lnCustom = ln.value(QStringLiteral("is_custom_obstacle")).toBool(ln.value(QStringLiteral("custom_obstacle")).toBool(false));
            double sl = ln.value(QStringLiteral("intrusion_stop_length")).toDouble(lnCustom ? 0.0 : mc.defaultObsStopLength());
            double sw = ln.value(QStringLiteral("intrusion_stop_width")).toDouble(lnCustom ? 0.0 : mc.defaultObsStopWidth());
            double dl = ln.value(QStringLiteral("intrusion_deceleration_length")).toDouble(lnCustom ? 0.0 : mc.defaultObsDecelLength());
            double dw = ln.value(QStringLiteral("intrusion_deceleration_width")).toDouble(lnCustom ? 0.0 : mc.defaultObsDecelWidth());
            if (!lnCustom && !ln.contains(QStringLiteral("intrusion_stop_length")) && !ln.contains(QStringLiteral("intrusion_stop_width")) &&
                !ln.contains(QStringLiteral("intrusion_deceleration_length")) && !ln.contains(QStringLiteral("intrusion_deceleration_width"))) {
                sl = mc.defaultObsStopLength();
                sw = mc.defaultObsStopWidth();
                dl = mc.defaultObsDecelLength();
                dw = mc.defaultObsDecelWidth();
            }

            QString sId = ln.contains(QStringLiteral("startid")) ? ln.value(QStringLiteral("startid")).toString() : ln.value(QStringLiteral("sId")).toString();
            if (sId.isEmpty()) sId = ln.value(QStringLiteral("startId")).toString();
            QString eId = ln.contains(QStringLiteral("endid")) ? ln.value(QStringLiteral("endid")).toString() : ln.value(QStringLiteral("eId")).toString();
            if (eId.isEmpty()) eId = ln.value(QStringLiteral("endId")).toString();

            const bool sIsSpecial = isNodeSpecial(sId);
            const bool eIsSpecial = isNodeSpecial(eId);

            auto updateStation = [&](const QString &nodeId) {
                if (nodeId.isEmpty() || isNodeSpecial(nodeId)) return;
                ObstacleParam &param = normalStationObsMap[nodeId];
                param.stopLen  = qMax(param.stopLen, sl);
                param.stopWid  = qMax(param.stopWid, sw);
                param.decelLen = qMax(param.decelLen, dl);
                param.decelWid = qMax(param.decelWid, dw);
                if (lnCustom) param.isCustom = true;
            };

            if (sIsSpecial && !eIsSpecial) {
                updateStation(eId);
            } else if (!sIsSpecial && eIsSpecial) {
                updateStation(sId);
            } else if (!sIsSpecial && !eIsSpecial) {
                updateStation(sId);
                updateStation(eId);
            }
        }
    };

    collectLineObstacle(lines);
    collectLineObstacle(curves);

    auto fillLineDefaults = [&](QJsonArray &arr) {
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject o = arr[i].toObject();
            const bool isCustom = o.value(QStringLiteral("is_custom_obstacle")).toBool(o.value(QStringLiteral("custom_obstacle")).toBool(false));
            if (!isCustom && !o.contains(QStringLiteral("intrusion_stop_length"))) {
                o[QStringLiteral("intrusion_stop_length")]         = mc.defaultObsStopLength();
                o[QStringLiteral("intrusion_stop_width")]          = mc.defaultObsStopWidth();
                o[QStringLiteral("intrusion_deceleration_length")] = mc.defaultObsDecelLength();
                o[QStringLiteral("intrusion_deceleration_width")]  = mc.defaultObsDecelWidth();
                o[QStringLiteral("is_custom_obstacle")]            = false;
                arr[i] = o;
            }
        }
    };
    fillLineDefaults(lines);
    fillLineDefaults(curves);

    for (int i = 0; i < points.size(); ++i) {
        QJsonObject pt = points[i].toObject();
        const QString ptId = pt.value(QStringLiteral("id")).toString();
        if (MapPgmUtil::isSpecialWorkstationPoint(pt) || specialIds.contains(ptId)) {
            MapPgmUtil::forceZeroObstacleParams(pt);
            points[i] = pt;
            continue;
        }

        const bool ptCustom = pt.value(QStringLiteral("is_custom_obstacle")).toBool(pt.value(QStringLiteral("custom_obstacle")).toBool(false));
        if (ptCustom) {
            points[i] = pt;
            continue;
        }

        double sl = mc.defaultObsStopLength(), sw = mc.defaultObsStopWidth(), dl = mc.defaultObsDecelLength(), dw = mc.defaultObsDecelWidth();
        bool isCustom = false;
        if (normalStationObsMap.contains(ptId)) {
            const auto &param = normalStationObsMap.value(ptId);
            sl = param.stopLen;
            sw = param.stopWid;
            dl = param.decelLen;
            dw = param.decelWid;
            isCustom = param.isCustom;
        } else if (pt.contains(QStringLiteral("intrusion_stop_length"))) {
            sl = pt.value(QStringLiteral("intrusion_stop_length")).toDouble(0.0);
            sw = pt.value(QStringLiteral("intrusion_stop_width")).toDouble(0.0);
            dl = pt.value(QStringLiteral("intrusion_deceleration_length")).toDouble(0.0);
            dw = pt.value(QStringLiteral("intrusion_deceleration_width")).toDouble(0.0);
        }

        pt[QStringLiteral("intrusion_stop_length")]         = sl;
        pt[QStringLiteral("intrusion_stop_width")]          = sw;
        pt[QStringLiteral("intrusion_deceleration_length")] = dl;
        pt[QStringLiteral("intrusion_deceleration_width")]  = dw;
        pt[QStringLiteral("is_custom_obstacle")]            = isCustom;

        QJsonArray actions = pt.value(QStringLiteral("actions")).toArray();
        bool foundAction = false;
        for (int j = 0; j < actions.size(); ++j) {
            QJsonObject act = actions[j].toObject();
            if (act.value(QStringLiteral("action_type")).toInt() == 21 || act.value(QStringLiteral("name")).toString() == QStringLiteral("实时避障范围")) {
                act[QStringLiteral("name")] = QStringLiteral("实时避障范围");
                act[QStringLiteral("action_type")] = 21;
                act[QStringLiteral("intrusion_stop_length")]         = sl;
                act[QStringLiteral("intrusion_stop_width")]          = sw;
                act[QStringLiteral("intrusion_deceleration_length")] = dl;
                act[QStringLiteral("intrusion_deceleration_width")]  = dw;
                act[QStringLiteral("is_custom_obstacle")]            = isCustom;
                actions[j] = act;
                foundAction = true;
                break;
            }
        }
        if (!foundAction) {
            QJsonObject act;
            act[QStringLiteral("name")] = QStringLiteral("实时避障范围");
            act[QStringLiteral("action_type")] = 21;
            act[QStringLiteral("intrusion_stop_length")]         = sl;
            act[QStringLiteral("intrusion_stop_width")]          = sw;
            act[QStringLiteral("intrusion_deceleration_length")] = dl;
            act[QStringLiteral("intrusion_deceleration_width")]  = dw;
            act[QStringLiteral("is_custom_obstacle")]            = isCustom;
            actions.append(act);
        }
        pt[QStringLiteral("actions")] = actions;
        points[i] = pt;
    }

    auto resetSpecialPoints = [](QJsonArray &arr) {
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject pt = arr[i].toObject();
            MapPgmUtil::forceZeroObstacleParams(pt);
            arr[i] = pt;
        }
    };
    resetSpecialPoints(charges);
    resetSpecialPoints(highPrecisionPoints);
}

} // namespace RouteObstacleSync
