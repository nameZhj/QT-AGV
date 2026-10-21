/**
 * @file    MapTopologyUtil.cpp
 * @layer   Utils
 * @brief   Implementation of AABB boundary extraction and workstation point classification.
 * @deps    MapTopologyUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MapTopologyUtil.h"

#include <QtGlobal>

void MapTopologyUtil::extractAreaBounds(const QJsonObject &obj, double &minX, double &maxX, double &minY, double &maxY)
{
    if (obj.contains(QStringLiteral("coordinates"))) {
        const QJsonArray coords = obj.value(QStringLiteral("coordinates")).toArray();
        if (coords.size() >= 2) {
            const QJsonArray pTL = coords[0].toArray();
            const QJsonArray pBR = coords[1].toArray();
            if (pTL.size() >= 2 && pBR.size() >= 2) {
                const double x1 = pTL[0].toDouble();
                const double y1 = pTL[1].toDouble();
                const double x2 = pBR[0].toDouble();
                const double y2 = pBR[1].toDouble();
                minX = qMin(x1, x2);
                maxX = qMax(x1, x2);
                minY = qMin(y1, y2);
                maxY = qMax(y1, y2);
                return;
            }
        }
    }
    minX = obj.value(QStringLiteral("minX")).toDouble();
    maxX = obj.value(QStringLiteral("maxX")).toDouble();
    minY = obj.value(QStringLiteral("minY")).toDouble();
    maxY = obj.value(QStringLiteral("maxY")).toDouble();
}

bool MapTopologyUtil::isSpecialWorkstationPoint(const QJsonObject &obj)
{
    const QString id = obj.value(QStringLiteral("id")).toString();
    const QString name = obj.value(QStringLiteral("name")).toString();
    const QString typeName = obj.value(QStringLiteral("typeName")).toString();
    const QString type = obj.value(QStringLiteral("type")).toString();
    const QString label = obj.value(QStringLiteral("label")).toString();

    if (id.startsWith(QStringLiteral("hp_"), Qt::CaseInsensitive) ||
        id.startsWith(QStringLiteral("c_"), Qt::CaseInsensitive) ||
        id.startsWith(QStringLiteral("ap_"), Qt::CaseInsensitive) ||
        id.startsWith(QStringLiteral("cp_"), Qt::CaseInsensitive)) {
        return true;
    }
    if (typeName.compare(QStringLiteral("ActionPoint"), Qt::CaseInsensitive) == 0 ||
        typeName.compare(QStringLiteral("ChargePoint"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (type.compare(QStringLiteral("highPrecision"), Qt::CaseInsensitive) == 0 ||
        type.compare(QStringLiteral("charge"), Qt::CaseInsensitive) == 0 ||
        type.compare(QStringLiteral("highPrecisionEpc"), Qt::CaseInsensitive) == 0 ||
        type.compare(QStringLiteral("chargeEpc"), Qt::CaseInsensitive) == 0 ||
        type.compare(QStringLiteral("ActionPoint"), Qt::CaseInsensitive) == 0 ||
        type.compare(QStringLiteral("ChargePoint"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (name.startsWith(QStringLiteral("AP"), Qt::CaseInsensitive) ||
        name.startsWith(QStringLiteral("CP"), Qt::CaseInsensitive) ||
        name.startsWith(QStringLiteral("TL"), Qt::CaseInsensitive)) {
        return true;
    }
    if (name.contains(QStringLiteral("工作站")) || name.contains(QStringLiteral("充电")) ||
        label.contains(QStringLiteral("工作站")) || label.contains(QStringLiteral("充电"))) {
        return true;
    }
    if (obj.contains(QStringLiteral("epc_x")) || obj.contains(QStringLiteral("epcX")) ||
        obj.contains(QStringLiteral("epc_y")) || obj.contains(QStringLiteral("epcY")) ||
        obj.contains(QStringLiteral("epc_theta")) || obj.contains(QStringLiteral("epcTheta")) ||
        obj.contains(QStringLiteral("epc_Yaw")) || obj.contains(QStringLiteral("epc_yaw"))) {
        return true;
    }
    return false;
}

void MapTopologyUtil::forceZeroObstacleParams(QJsonObject &obj)
{
    constexpr int kActionTypeObstacle = 21;

    obj[QStringLiteral("intrusion_stop_length")]         = 0.0;
    obj[QStringLiteral("intrusion_stop_width")]          = 0.0;
    obj[QStringLiteral("intrusion_deceleration_length")] = 0.0;
    obj[QStringLiteral("intrusion_deceleration_width")]  = 0.0;
    obj[QStringLiteral("is_custom_obstacle")]            = false;

    if (!obj.contains(QStringLiteral("actions")) || !obj.value(QStringLiteral("actions")).isArray()) {
        return;
    }

    QJsonArray acts = obj.value(QStringLiteral("actions")).toArray();
    bool modified = false;
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
            modified = true;
        }
    }
    if (modified) {
        obj[QStringLiteral("actions")] = acts;
    }
}
