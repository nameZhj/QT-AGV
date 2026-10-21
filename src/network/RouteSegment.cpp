/**
 * @file    RouteSegment.cpp
 * @layer   DataComm
 * @brief   RouteSegment serialization and deserialization implementation.
 * @deps    RouteNetworkApi.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "RouteNetworkApi.h"
#include "MotionConfigManager.h"

QJsonObject RouteSegment::toJson() const
{
    QJsonObject obj;
    obj["id"]          = id;
    obj["name"]        = name;
    obj["type"]        = type;
    obj["sId"]         = startId;
    obj["eId"]         = endId;
    obj["startid"]     = startId;
    obj["endid"]       = endId;
    obj["direct"]      = direct;
    obj["direction"]   = direct;

    obj["x1"]          = startPos.x();
    obj["y1"]          = startPos.y();
    obj["x2"]          = endPos.x();
    obj["y2"]          = endPos.y();

    if (type == "curve" || !controlPts.isEmpty()) {
        QJsonArray ctrlArr;
        for (const auto &p : controlPts) {
            QJsonObject cp;
            cp["x"] = p.x();
            cp["y"] = p.y();
            ctrlArr.append(cp);
        }
        obj["controlPoints"] = ctrlArr;
        if (controlPts.size() == 3) {
            obj["cx"] = controlPts[1].x();
            obj["cy"] = controlPts[1].y();
        }
    }

    obj["maxSpeed"]                 = maxSpeed;
    obj["maxspeed"]                 = maxSpeed;
    obj["minSpeed"]                 = minSpeed;
    obj["minspeed"]                 = minSpeed;
    obj["max_acceleration"]         = maxAcc;
    obj["maxAcc"]                   = maxAcc;
    obj["maxacc"]                   = maxAcc;
    obj["max_deceleration"]         = maxDec;
    obj["maxDec"]                   = maxDec;
    obj["maxdec"]                   = maxDec;
    obj["max_angular_speed"]        = maxRotSpeed;
    obj["maxRotSpeed"]              = maxRotSpeed;
    obj["maxrot"]                   = maxRotSpeed;
    obj["max_angular_acceleration"] = maxRotAcc;
    obj["maxRotAcc"]                = maxRotAcc;
    obj["maxrotacc"]                = maxRotAcc;
    obj["reachDist"]   = reachDist;
    obj["reachAngle"]  = reachAngle;
    obj["roadWidth"]   = roadWidth;
    obj["width"]       = roadWidth;

    obj["moveStyle"]     = moveStyle;
    obj["locked"]        = locked;
    obj["weight"]        = weight;
    obj["priority"]      = priority;
    obj["spinTurn"]      = spinTurn;
    obj["avoidObstacle"] = avoidObstacle;

    obj["intrusion_stop_length"]         = intrusionStopLength;
    obj["intrusion_stop_width"]          = intrusionStopWidth;
    obj["intrusion_deceleration_length"] = intrusionDecelLength;
    obj["intrusion_deceleration_width"]  = intrusionDecelWidth;
    obj["is_custom_obstacle"]            = isCustomObstacle;

    return obj;
}

RouteSegment RouteSegment::fromJson(const QJsonObject &obj)
{
    RouteSegment seg;
    seg.id      = obj.value("id").toString();
    seg.name    = obj.value("name").toString();
    seg.type    = obj.value("type").toString(seg.id.startsWith("b_") ? "curve" : "line");
    seg.startId = obj.contains("sId") ? obj.value("sId").toString() : obj.value("startid").toString();
    seg.endId   = obj.contains("eId") ? obj.value("eId").toString() : obj.value("endid").toString();
    seg.direct  = obj.value("direct").toInt(obj.value("direction").toInt(10));

    seg.startPos = QPointF(obj.value("x1").toDouble(0.0), obj.value("y1").toDouble(0.0));
    seg.endPos   = QPointF(obj.value("x2").toDouble(0.0), obj.value("y2").toDouble(0.0));

    if (obj.contains("controlPoints")) {
        const QJsonArray arr = obj.value("controlPoints").toArray();
        for (const auto &v : arr) {
            const QJsonObject p = v.toObject();
            seg.controlPts.append(QPointF(p.value("x").toDouble(), p.value("y").toDouble()));
        }
    } else if (obj.contains("cx") && obj.contains("cy")) {
        seg.controlPts.append(seg.startPos);
        seg.controlPts.append(QPointF(obj.value("cx").toDouble(), obj.value("cy").toDouble()));
        seg.controlPts.append(seg.endPos);
    }

    seg.maxSpeed    = obj.contains("maxSpeed") ? obj.value("maxSpeed").toDouble(0.5) : obj.value("maxspeed").toDouble(0.5);
    seg.minSpeed    = obj.contains("min_speed") ? obj.value("min_speed").toDouble(0.0)
                    : obj.contains("minSpeed")  ? obj.value("minSpeed").toDouble(0.0)
                    : obj.value("minspeed").toDouble(0.0);
    seg.maxAcc      = obj.contains("max_acceleration") ? obj.value("max_acceleration").toDouble(0.3)
                    : obj.contains("maxAcc")           ? obj.value("maxAcc").toDouble(0.3)
                    : obj.value("maxacc").toDouble(0.3);
    seg.maxDec      = obj.contains("max_deceleration") ? obj.value("max_deceleration").toDouble(0.3)
                    : obj.contains("maxDec")           ? obj.value("maxDec").toDouble(0.3)
                    : obj.value("maxdec").toDouble(0.3);
    seg.maxRotSpeed = obj.contains("max_angular_speed") ? obj.value("max_angular_speed").toDouble(60.0)
                    : obj.contains("maxRotSpeed")       ? obj.value("maxRotSpeed").toDouble(60.0)
                    : obj.value("maxrot").toDouble(60.0);
    seg.maxRotAcc   = obj.contains("max_angular_acceleration") ? obj.value("max_angular_acceleration").toDouble(45.0)
                    : obj.contains("maxRotAcc")                ? obj.value("maxRotAcc").toDouble(45.0)
                    : obj.value("maxrotacc").toDouble(45.0);
    seg.reachDist   = obj.value("reachDist").toDouble(0.05);
    seg.reachAngle  = obj.value("reachAngle").toDouble(2.0);
    seg.roadWidth   = obj.contains("roadWidth") ? obj.value("roadWidth").toDouble(1.0) : obj.value("width").toDouble(1.0);

    seg.moveStyle     = obj.value("moveStyle").toInt(obj.value("movestyle").toInt(0));
    seg.locked        = obj.value("locked").toBool(false);
    seg.weight        = obj.value("weight").toDouble(1.0);
    seg.priority       = obj.value("priority").toInt(0);
    seg.spinTurn      = obj.value("spinTurn").toBool(true);
    seg.avoidObstacle = obj.value("avoidObstacle").toBool(true);

    seg.isCustomObstacle     = obj.value("is_custom_obstacle").toBool(obj.value("custom_obstacle").toBool(false));
    const auto &mc = MotionConfigManager::instance();
    seg.intrusionStopLength  = obj.value("intrusion_stop_length").toDouble(seg.isCustomObstacle ? 0.0 : mc.defaultObsStopLength());
    seg.intrusionStopWidth   = obj.value("intrusion_stop_width").toDouble(seg.isCustomObstacle ? 0.0 : mc.defaultObsStopWidth());
    seg.intrusionDecelLength = obj.value("intrusion_deceleration_length").toDouble(seg.isCustomObstacle ? 0.0 : mc.defaultObsDecelLength());
    seg.intrusionDecelWidth  = obj.value("intrusion_deceleration_width").toDouble(seg.isCustomObstacle ? 0.0 : mc.defaultObsDecelWidth());
    if (!seg.isCustomObstacle && !obj.contains("intrusion_stop_length") &&
        seg.intrusionStopLength <= 0.0 && seg.intrusionStopWidth <= 0.0 &&
        seg.intrusionDecelLength <= 0.0 && seg.intrusionDecelWidth <= 0.0) {
        seg.intrusionStopLength  = mc.defaultObsStopLength();
        seg.intrusionStopWidth   = mc.defaultObsStopWidth();
        seg.intrusionDecelLength = mc.defaultObsDecelLength();
        seg.intrusionDecelWidth  = mc.defaultObsDecelWidth();
    }

    return seg;
}
