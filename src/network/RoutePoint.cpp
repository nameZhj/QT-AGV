/**
 * @file    RoutePoint.cpp
 * @layer   DataComm
 * @brief   RoutePoint serialization and deserialization implementation.
 * @deps    RouteNetworkApi.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RouteNetworkApi.h"
#include "MotionConfigManager.h"

#include <cmath>

bool RoutePoint::isSpecial() const
{
    if (id.startsWith("hp_", Qt::CaseInsensitive) || id.startsWith("c_", Qt::CaseInsensitive) ||
        id.startsWith("ap_", Qt::CaseInsensitive) || id.startsWith("cp_", Qt::CaseInsensitive)) {
        return true;
    }
    if (typeName.compare("ActionPoint", Qt::CaseInsensitive) == 0 ||
        typeName.compare("ChargePoint", Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (type.compare("highPrecision", Qt::CaseInsensitive) == 0 ||
        type.compare("charge", Qt::CaseInsensitive) == 0 ||
        type.compare("highPrecisionEpc", Qt::CaseInsensitive) == 0 ||
        type.compare("chargeEpc", Qt::CaseInsensitive) == 0 ||
        type.compare("ActionPoint", Qt::CaseInsensitive) == 0 ||
        type.compare("ChargePoint", Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (name.startsWith("AP", Qt::CaseInsensitive) ||
        name.startsWith("CP", Qt::CaseInsensitive) ||
        name.startsWith("TL", Qt::CaseInsensitive)) {
        return true;
    }
    if (name.contains("工作站") || name.contains("充电") ||
        label.contains("工作站") || label.contains("充电")) {
        return true;
    }
    if (epc_x != 0.0 || epc_y != 0.0 || epc_theta != 0.0) {
        return true;
    }
    return false;
}

QJsonObject RoutePoint::toJson() const
{
    QJsonObject obj;
    obj["id"]          = id;
    obj["name"]        = name;
    obj["type"]        = type;
    obj["x"]           = x;
    obj["y"]           = y;
    obj["yaw"]         = yaw;
    obj["theta"]       = yaw * 180.0 / M_PI;
    obj["robotYaw"]    = 0.0;
    obj["rdsDockable"] = rdsDockable;
    obj["allowRotate"] = allowRotate;
    obj["useDownPGV"]  = useDownPGV;
    obj["epc"]         = epc;
    obj["isyawfix"]    = isyawfix;
    obj["taskid"]      = taskid;

    if (ignoreDir) obj["ignoreDir"] = true;
    if (!label.isEmpty()) obj["label"] = label;
    if (waitTimeout > 0.0) obj["waitTimeout"] = waitTimeout;

    if (epc_x != 0.0 || epc_y != 0.0 || epc_theta != 0.0 || type == "charge" || type == "highPrecision" || isSpecial()) {
        obj["epc_x"]     = epc_x;
        obj["epc_y"]     = epc_y;
        obj["epc_theta"] = epc_theta;
        obj["epcX"]      = epc_x;
        obj["epcY"]      = epc_y;
        obj["epcTheta"]  = epc_theta;
        obj["epc_Yaw"]   = epc_theta;
        obj["epc_yaw"]   = epc_theta;
    }
    if (!typeName.isEmpty()) obj["typeName"] = typeName;

    const bool special = isSpecial();
    const double sl = special ? 0.0 : intrusionStopLength;
    const double sw = special ? 0.0 : intrusionStopWidth;
    const double dl = special ? 0.0 : intrusionDecelLength;
    const double dw = special ? 0.0 : intrusionDecelWidth;

    obj["intrusion_stop_length"]         = sl;
    obj["intrusion_stop_width"]          = sw;
    obj["intrusion_deceleration_length"] = dl;
    obj["intrusion_deceleration_width"]  = dw;
    obj["is_custom_obstacle"]            = special ? false : isCustomObstacle;

    QJsonArray outActions = actions;
    bool foundAction = false;
    for (int i = 0; i < outActions.size(); ++i) {
        QJsonObject act = outActions[i].toObject();
        if (act.value("action_type").toInt() == 21 || act.value("name").toString() == "实时避障范围") {
            act["name"] = "实时避障范围";
            act["action_type"] = 21;
            act["intrusion_stop_length"]         = sl;
            act["intrusion_stop_width"]          = sw;
            act["intrusion_deceleration_length"] = dl;
            act["intrusion_deceleration_width"]  = dw;
            act["is_custom_obstacle"]            = special ? false : isCustomObstacle;
            outActions[i] = act;
            foundAction = true;
            break;
        }
    }
    if (!foundAction && !special) {
        QJsonObject act;
        act["name"] = "实时避障范围";
        act["action_type"] = 21;
        act["intrusion_stop_length"]         = sl;
        act["intrusion_stop_width"]          = sw;
        act["intrusion_deceleration_length"] = dl;
        act["intrusion_deceleration_width"]  = dw;
        act["is_custom_obstacle"]            = isCustomObstacle;
        outActions.append(act);
    }
    if (!outActions.isEmpty()) {
        obj["actions"] = outActions;
    }

    return obj;
}

RoutePoint RoutePoint::fromJson(const QJsonObject &obj)
{
    RoutePoint pt;
    pt.id          = obj.value("id").toString();
    pt.name        = obj.value("name").toString();
    pt.type        = obj.value("type").toString("point");
    pt.x           = obj.value("x").toDouble(0.0);
    pt.y           = obj.value("y").toDouble(0.0);
    pt.yaw         = obj.value("yaw").toDouble(0.0);
    pt.rdsDockable = obj.value("rdsDockable").toBool(false);
    pt.allowRotate = obj.value("allowRotate").toBool(false);
    pt.useDownPGV  = obj.value("useDownPGV").toBool(false);
    pt.epc         = obj.value("epc").toString();
    pt.isyawfix    = obj.value("isyawfix").toBool(false);
    pt.taskid      = obj.value("taskid").toInt(0);

    pt.epc_x     = obj.contains("epc_x") ? obj.value("epc_x").toDouble(0.0) : obj.value("epcX").toDouble(0.0);
    pt.epc_y     = obj.contains("epc_y") ? obj.value("epc_y").toDouble(0.0) : obj.value("epcY").toDouble(0.0);
    if (obj.contains("epc_theta")) pt.epc_theta = obj.value("epc_theta").toDouble(0.0);
    else if (obj.contains("epcTheta")) pt.epc_theta = obj.value("epcTheta").toDouble(0.0);
    else if (obj.contains("epc_Yaw")) pt.epc_theta = obj.value("epc_Yaw").toDouble(0.0);
    else if (obj.contains("epc_yaw")) pt.epc_theta = obj.value("epc_yaw").toDouble(0.0);
    else pt.epc_theta = 0.0;
    pt.typeName  = obj.value("typeName").toString();

    pt.ignoreDir   = obj.value("ignoreDir").toBool(false);
    pt.label       = obj.value("label").toString();
    pt.waitTimeout = obj.value("waitTimeout").toDouble(0.0);

    pt.intrusionStopLength  = obj.value("intrusion_stop_length").toDouble(0.0);
    pt.intrusionStopWidth   = obj.value("intrusion_stop_width").toDouble(0.0);
    pt.intrusionDecelLength = obj.value("intrusion_deceleration_length").toDouble(0.0);
    pt.intrusionDecelWidth  = obj.value("intrusion_deceleration_width").toDouble(0.0);
    pt.isCustomObstacle     = obj.value("is_custom_obstacle").toBool(obj.value("custom_obstacle").toBool(false));
    pt.actions              = obj.value("actions").toArray();
    if (pt.intrusionStopLength == 0.0 && pt.intrusionStopWidth == 0.0 &&
        pt.intrusionDecelLength == 0.0 && pt.intrusionDecelWidth == 0.0) {
        for (const auto &av : pt.actions) {
            const QJsonObject act = av.toObject();
            if (act.value("action_type").toInt() == 21 || act.value("name").toString() == "实时避障范围") {
                pt.intrusionStopLength  = act.value("intrusion_stop_length").toDouble(0.0);
                pt.intrusionStopWidth   = act.value("intrusion_stop_width").toDouble(0.0);
                pt.intrusionDecelLength = act.value("intrusion_deceleration_length").toDouble(0.0);
                pt.intrusionDecelWidth  = act.value("intrusion_deceleration_width").toDouble(0.0);
                if (act.contains("is_custom_obstacle")) {
                    pt.isCustomObstacle = act.value("is_custom_obstacle").toBool(false);
                }
                break;
            }
        }
    }
    if (pt.isSpecial()) {
        pt.intrusionStopLength  = 0.0;
        pt.intrusionStopWidth   = 0.0;
        pt.intrusionDecelLength = 0.0;
        pt.intrusionDecelWidth  = 0.0;
        pt.isCustomObstacle     = false;
    } else {
        if (!pt.isCustomObstacle && !obj.contains("intrusion_stop_length") &&
            pt.intrusionStopLength <= 0.0 && pt.intrusionStopWidth <= 0.0 &&
            pt.intrusionDecelLength <= 0.0 && pt.intrusionDecelWidth <= 0.0) {
            const auto &mc = MotionConfigManager::instance();
            pt.intrusionStopLength  = mc.defaultObsStopLength();
            pt.intrusionStopWidth   = mc.defaultObsStopWidth();
            pt.intrusionDecelLength = mc.defaultObsDecelLength();
            pt.intrusionDecelWidth  = mc.defaultObsDecelWidth();
        }
    }

    return pt;
}
