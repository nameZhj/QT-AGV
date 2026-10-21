/**
 * @file    ConfigDefaultCompleter.cpp
 * @layer   GUILayer
 * @brief   Implementation of fallback default config completion.
 * @deps    ConfigDefaultCompleter.h, MotionConfigManager.h, AlignConfigManager.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "ConfigDefaultCompleter.h"
#include "core/MotionConfigManager.h"
#include "core/AlignConfigManager.h"

#include <QJsonObject>

void ConfigDefaultCompleter::ensureMotionControlConfig(QJsonArray &configs)
{
    bool found = false;
    auto &mc = MotionConfigManager::instance();
    for (int i = 0; i < configs.size(); ++i) {
        QJsonObject entry = configs[i].toObject();
        QString name = entry.value("name").toString().toLower();
        QString path = entry.value("file_path").toString().toLower();
        if (name == "motion_control" || name.contains("motion_control") ||
            path.contains("motion_control") || name == "运动控制") {
            found = true;
            QJsonObject params = entry.value("params").toObject();
            if (!params.contains("max_speed")) params["max_speed"] = mc.maxSpeed();
            if (!params.contains("max_acceleration")) params["max_acceleration"] = mc.maxAcceleration();
            if (!params.contains("max_deceleration")) params["max_deceleration"] = mc.maxDeceleration();
            if (!params.contains("max_angular_speed")) params["max_angular_speed"] = mc.maxAngularSpeedDeg();
            if (!params.contains("max_angular_acceleration")) params["max_angular_acceleration"] = mc.maxAngularAccelerationDeg();
            if (!params.contains("arrival_distance_tolerance")) params["arrival_distance_tolerance"] = mc.arrivalDistanceTolerance();
            if (!params.contains("arrival_angle_tolerance")) params["arrival_angle_tolerance"] = mc.arrivalAngleToleranceDeg();
            if (!params.contains("route_width")) params["route_width"] = mc.routeWidth();
            entry["params"] = params;
            configs[i] = entry;
            mc.updateFromJsonObject(params);
            break;
        }
    }

    if (!found) {
        QJsonObject motionEntry;
        motionEntry["name"] = "motion_control";
        motionEntry["file_path"] = "/home/ubuntu/xinje_agv/src/robot_bringup/config/motion_control.yaml";
        motionEntry["status"] = true;
        motionEntry["params"] = mc.toJsonObject();
        configs.prepend(motionEntry);
    }
}

void ConfigDefaultCompleter::ensureAlignConfig(QJsonArray &configs)
{
    auto &ac = AlignConfigManager::instance();
    for (int i = 0; i < configs.size(); ++i) {
        QJsonObject entry = configs[i].toObject();
        QString path = entry.value("file_path").toString().toLower();
        QJsonObject params = entry.value("params").toObject();
        if (params.contains("cylinder_length") || path.contains("secondary_localization_reflector")) {
            if (params.contains("cylinder_length")) {
                ac.setCylinderLength(params.value("cylinder_length").toDouble(ac.cylinderLength()));
            }
            if (params.contains("anti_align_depth")) {
                ac.setAntiAlignDepth(params.value("anti_align_depth").toDouble(ac.antiAlignDepth()));
            } else {
                params["anti_align_depth"] = ac.antiAlignDepth();
                entry["params"] = params;
                configs[i] = entry;
            }
            break;
        }
    }
}

void ConfigDefaultCompleter::ensureObstacleConfig(QJsonArray &configs)
{
    auto &mc = MotionConfigManager::instance();
    for (int i = 0; i < configs.size(); ++i) {
        QJsonObject entry = configs[i].toObject();
        QString name = entry.value("name").toString().toLower();
        QString path = entry.value("file_path").toString().toLower();
        QJsonObject params = entry.value("params").toObject();
        if (params.contains("id_stop_range_threshold") || params.contains("id_deceleration_range_threshold") ||
            name.contains("obstacle") || path.contains("obstacle") || name.contains("config") || path.contains("config.yaml")) {
            bool modified = false;
            if (!params.contains("id_stop_range_threshold")) {
                QJsonArray stopArr;
                stopArr.append(mc.defaultObsStopLength());
                stopArr.append(mc.defaultObsStopWidth());
                params["id_stop_range_threshold"] = stopArr;
                modified = true;
            }
            if (!params.contains("id_deceleration_range_threshold")) {
                QJsonArray decelArr;
                decelArr.append(mc.defaultObsDecelLength());
                decelArr.append(mc.defaultObsDecelWidth());
                params["id_deceleration_range_threshold"] = decelArr;
                modified = true;
            }
            if (modified) {
                entry["params"] = params;
                configs[i] = entry;
            }
            mc.updateObstacleConfig(params);
            break;
        }
    }
}
