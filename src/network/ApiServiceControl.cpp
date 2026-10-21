/**
 * @file    ApiServiceControl.cpp
 * @layer   ProtocolComm
 * @brief   AGV motion, pose initialization, speed configuration, and controller dispatch APIs.
 * @deps    ApiService.h, AppConfig.h, MotionConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ApiService.h"
#include "AppConfig.h"
#include "MotionConfigManager.h"

#include <QDateTime>
#include <QJsonArray>
#include <QUrlQuery>

// ========== 导航 / 位姿接口 (端口 8048) ==========

void ApiService::initPose(const QString &carId, const QJsonObject &pose, const Callback &cb)
{
    QJsonObject data = pose;
    data["carId"] = carId;
    post("/robot/navigation/initPose", data, cb);
}

void ApiService::navigateTo(const QString &locationId, const Callback &cb)
{
    QUrlQuery q;
    if (!locationId.isEmpty())
        q.addQueryItem("locationId", locationId);
    QString url = "/robot/navigation/navigate/to";
    if (!q.isEmpty()) url += "?" + q.toString();
    post(url, QJsonObject(), cb);
}

void ApiService::robotNavigationMove(const QUrlQuery &query, const Callback &cb)
{
    QString url = "/robot/navigation/move";
    if (!query.isEmpty()) {
        url += "?" + query.toString(QUrl::FullyEncoded);
    }
    post(url, QJsonObject(), cb, 8048);
}

void ApiService::robotNavigationMoveLinear(double vx, double vy, double distance, const Callback &cb)
{
    QUrlQuery q;
    q.addQueryItem("actionType", "6");
    q.addQueryItem("vx", QString::number(vx));
    q.addQueryItem("vy", QString::number(vy));
    q.addQueryItem("distance", QString::number(distance));
    robotNavigationMove(q, cb);
}

void ApiService::robotNavigationMoveRotate(double rotateAngle, double rotateSpeed, const Callback &cb)
{
    QUrlQuery q;
    q.addQueryItem("actionType", "12");
    q.addQueryItem("rotateAngle", QString::number(rotateAngle));
    q.addQueryItem("rotateSpeed", QString::number(rotateSpeed));
    robotNavigationMove(q, cb);
}

// ========== 控制器接口 (端口 8061, 参考控制器APP规范) ==========

static inline int resolveActionId(int actionId)
{
    if (actionId <= 0) {
        return static_cast<int>(QDateTime::currentMSecsSinceEpoch() % 1000000 + 1);
    }
    return actionId;
}

static void clampPathsSpeed(QJsonObject &root)
{
    QJsonObject d = root.value("data").toObject();
    if (!d.contains("robot_paths")) return;
    QJsonArray paths = d.value("robot_paths").toArray();
    for (int i = 0; i < paths.size(); ++i) {
        QJsonObject p = paths[i].toObject();
        if (p.contains("path_speed")) {
            p["path_speed"] = MotionConfigManager::instance().clampLinearSpeed(p.value("path_speed").toDouble(0.5));
            paths[i] = p;
        }
    }
    d["robot_paths"] = paths;
    root["data"] = d;
}

void ApiService::setActionControl(const QJsonObject &data, const Callback &cb)
{
    QJsonObject root = data.contains("data") ? data : QJsonObject{{"data", data}};
    post("/rbrainrobot/set_action_control", root, cb, kControllerPort, false);
}

void ApiService::setLinearMotion(double vx, double vy, double distance, int actionId, const Callback &cb)
{
    actionId = resolveActionId(actionId);
    vx = MotionConfigManager::instance().clampSignedLinearSpeed(vx);
    vy = MotionConfigManager::instance().clampSignedLinearSpeed(vy);

    QJsonObject actionObj{
        {"action_id", actionId},
        {"action_type", 6},
        {"vx", vx},
        {"vy", vy},
        {"distance", distance}
    };
    setActionControl(actionObj, cb);
}

void ApiService::setRotateMotion(double rotateAngleRad, double rotateSpeed, int actionId, const Callback &cb)
{
    actionId = resolveActionId(actionId);
    rotateSpeed = MotionConfigManager::instance().clampSignedAngularSpeedRad(rotateSpeed);

    QJsonObject actionObj{
        {"action_id", actionId},
        {"action_type", 12},
        {"rotate_angle", std::abs(rotateAngleRad)},
        {"rotate_speed", rotateSpeed}
    };
    setActionControl(actionObj, cb);
}

void ApiService::setJackControl(double liftHeight, double liftLinear, int actionId, const Callback &cb)
{
    actionId = resolveActionId(actionId);
    QJsonObject actionObj{
        {"action_id", actionId},
        {"action_type", 3},
        {"lift_height", liftHeight},
        {"lift_linear", liftLinear}
    };
    setActionControl(actionObj, cb);
}

void ApiService::setRobotFixPath(const QJsonObject &data, const Callback &cb)
{
    QJsonObject root = data.contains("data") ? data : QJsonObject{{"data", data}};
    clampPathsSpeed(root);
    post("/rbrainrobot/set_robot_fix_path", root, cb, kControllerPort, false);
}

void ApiService::setThirdFixPath(const QJsonObject &data, const Callback &cb)
{
    QJsonObject root = data.contains("data") ? data : QJsonObject{{"data", data}};
    clampPathsSpeed(root);
    post("/rbrainrobot/set_third_fix_path", root, cb, kControllerPort, false);
}

void ApiService::navigateToStation(const QString &targetStation, const QString &mapCode, double targetX, double targetY, double speed, int actionId, const Callback &cb)
{
    Q_UNUSED(targetX)
    Q_UNUSED(targetY)
    actionId = resolveActionId(actionId);
    speed = (speed <= 0.0) ? MotionConfigManager::instance().maxSpeed() : MotionConfigManager::instance().clampLinearSpeed(speed);

    QString actualMapCode = mapCode.trimmed().isEmpty() ? AppConfig::instance().currentDevice().mapName : mapCode.trimmed();
    if (actualMapCode.isEmpty()) actualMapCode = "test_map";

    bool ok = false;
    const int goalNodeInt = targetStation.toInt(&ok);
    QJsonObject pathObj{
        {"start_node", ok ? QJsonValue(1) : QJsonValue("start")},
        {"goal_node", ok ? QJsonValue(goalNodeInt) : QJsonValue(targetStation)},
        {"index", 1},
        {"robot_first_path", true},
        {"path_speed", speed},
        {"reverse", false}
    };

    QJsonObject d{
        {"action_id", actionId},
        {"mapcode", actualMapCode},
        {"robot_paths", QJsonArray{pathObj}}
    };
    QJsonObject root{{"data", d}};
    post("/rbrainrobot/set_robot_fix_path", root, cb, kControllerPort, false);
}

void ApiService::navigateToPose(double startX, double startY, double goalX, double goalY, double speed, int actionId, const Callback &cb)
{
    actionId = resolveActionId(actionId);
    speed = (speed <= 0.0) ? MotionConfigManager::instance().maxSpeed() : MotionConfigManager::instance().clampLinearSpeed(speed);

    QJsonObject pathObj{
        {"start_node", "start"},
        {"goal_node", "goal"},
        {"start_pose", QJsonObject{{"x", startX}, {"y", startY}}},
        {"goal_pose", QJsonObject{{"x", goalX}, {"y", goalY}}},
        {"index", 1},
        {"robot_first_path", true},
        {"path_speed", speed},
        {"reverse", false}
    };

    QJsonObject d{
        {"action_id", actionId},
        {"robot_paths", QJsonArray{pathObj}}
    };
    QJsonObject root{{"data", d}};
    post("/rbrainrobot/set_third_fix_path", root, cb, kControllerPort, false);
}

void ApiService::getRobotRunStatus(const Callback &cb)
{
    get("/rbrainrobot/get_robot_run_status", cb, kControllerPort, false);
}

void ApiService::queryActionIdStatus(int actionId, const Callback &cb)
{
    QJsonObject root{{"data", QJsonObject{{"action_id", actionId}}}};
    post("/rbrainrobot/query_action_id_status", root, cb, kControllerPort, false);
}

void ApiService::setInitPose8061(double x, double y, double angleRad, const Callback &cb)
{
    QJsonObject root{{"data", QJsonObject{{"x", x}, {"y", y}, {"angle", angleRad}}}};
    post("/rbrainrobot/set_init_pose", root, cb, kControllerPort, false);
}

void ApiService::setRobotSpeed(double linearSpeed, double angularSpeed, const Callback &cb)
{
    linearSpeed  = MotionConfigManager::instance().clampLinearSpeed(linearSpeed);
    angularSpeed = MotionConfigManager::instance().clampAngularSpeedRad(angularSpeed);

    QJsonObject d{
        {"linear_speed", linearSpeed},
        {"angular_speed", angularSpeed},
        {"max_linear_speed", linearSpeed},
        {"max_angular_speed", angularSpeed}
    };
    QJsonObject root{{"data", d}};
    post("/rbrainrobot/set_speed", root, cb, kControllerPort, false);
}

void ApiService::modifyPathSpeed(const QString &mapName, double pathSpeed, double speedProportion, const Callback &cb)
{
    pathSpeed = MotionConfigManager::instance().clampLinearSpeed(pathSpeed);

    QJsonObject pathItem{{"path_speed", pathSpeed}};
    QJsonObject d{
        {"map_name", mapName},
        {"speed_proportion", speedProportion},
        {"path_info", QJsonArray{pathItem}}
    };
    QJsonObject root{{"data", d}};
    post("/rbrainrobot/modify_path_property", root, cb, kControllerPort, false);
}
