/**
 * @file    MotionConfigSerializer.cpp
 * @layer   CoreScheduling
 * @brief   Implementation of settings persistence and JSON serializer for motion parameters.
 * @deps    MotionConfigSerializer.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MotionConfigSerializer.h"

#include <QSettings>
#include <QJsonDocument>
#include <QtGlobal>
#include <QDebug>

double MotionConfigSerializer::clampByParamMeta(const QString &paramName,
                                               double val,
                                               const QMap<QString, MotionParamMeta> &metas)
{
    if (metas.contains(paramName)) {
        const auto &meta = metas[paramName];
        if (meta.hasMin && val < meta.minVal) val = meta.minVal;
        if (meta.hasMax && val > meta.maxVal) val = meta.maxVal;
    }
    return val;
}

void MotionConfigSerializer::loadFromSettings(MotionConfigData &data,
                                             QMap<QString, MotionParamMeta> &metas)
{
    QSettings s;
    const QByteArray cachedMeta = s.value(QStringLiteral("editConfig/cachedMetadata")).toByteArray();
    if (!cachedMeta.isEmpty()) {
        QJsonArray list;
        MotionMetadataParser::parseMetadataJson(cachedMeta, list);
        if (!list.isEmpty()) {
            MotionMetadataParser::parseMetadataArray(
                list, metas,
                data.defaultObsStopLength, data.defaultObsStopWidth,
                data.defaultObsDecelLength, data.defaultObsDecelWidth);
        }
    }

    const QByteArray cachedConfigs = s.value(QStringLiteral("editConfig/cachedConfigs")).toByteArray();
    if (!cachedConfigs.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(cachedConfigs);
        if (doc.isObject()) {
            const QJsonObject root = doc.object();
            if (root.contains("data")) {
                const QJsonValue dv = root.value("data");
                if (dv.isArray()) {
                    updateFromConfigsArray(data, dv.toArray(), metas);
                } else if (dv.isObject()) {
                    const QJsonObject dataObj = dv.toObject();
                    if (dataObj.contains("data") && dataObj.value("data").isArray()) {
                        updateFromConfigsArray(data, dataObj.value("data").toArray(), metas);
                    }
                }
            }
        } else if (doc.isArray()) {
            updateFromConfigsArray(data, doc.array(), metas);
        }
    }

    data.maxSpeed                  = clampByParamMeta(QStringLiteral("max_speed"), s.value(QStringLiteral("motion/max_speed"), data.maxSpeed).toDouble(), metas);
    data.maxAcceleration           = clampByParamMeta(QStringLiteral("max_acceleration"), s.value(QStringLiteral("motion/max_acceleration"), data.maxAcceleration).toDouble(), metas);
    data.maxDeceleration           = clampByParamMeta(QStringLiteral("max_deceleration"), s.value(QStringLiteral("motion/max_deceleration"), data.maxDeceleration).toDouble(), metas);
    data.maxAngularSpeedDeg        = clampByParamMeta(QStringLiteral("max_angular_speed"), s.value(QStringLiteral("motion/max_angular_speed"), data.maxAngularSpeedDeg).toDouble(), metas);
    data.maxAngularAccelerationDeg = clampByParamMeta(QStringLiteral("max_angular_acceleration"), s.value(QStringLiteral("motion/max_angular_acceleration"), data.maxAngularAccelerationDeg).toDouble(), metas);
    data.arrivalDistanceTolerance  = clampByParamMeta(QStringLiteral("arrival_distance_tolerance"), s.value(QStringLiteral("motion/arrival_distance_tolerance"), data.arrivalDistanceTolerance).toDouble(), metas);
    data.arrivalAngleToleranceDeg  = clampByParamMeta(QStringLiteral("arrival_angle_tolerance"), s.value(QStringLiteral("motion/arrival_angle_tolerance"), data.arrivalAngleToleranceDeg).toDouble(), metas);
    data.routeWidth                = clampByParamMeta(QStringLiteral("route_width"), s.value(QStringLiteral("motion/route_width"), data.routeWidth).toDouble(), metas);

    data.defaultObsStopLength     = s.value(QStringLiteral("obstacle/id_stop_range_threshold_dx"), data.defaultObsStopLength).toDouble();
    data.defaultObsStopWidth      = s.value(QStringLiteral("obstacle/id_stop_range_threshold_dy"), data.defaultObsStopWidth).toDouble();
    data.defaultObsDecelLength    = s.value(QStringLiteral("obstacle/id_deceleration_range_threshold_dx"), data.defaultObsDecelLength).toDouble();
    data.defaultObsDecelWidth     = s.value(QStringLiteral("obstacle/id_deceleration_range_threshold_dy"), data.defaultObsDecelWidth).toDouble();
}

void MotionConfigSerializer::saveToSettings(const MotionConfigData &data)
{
    QSettings s;
    s.setValue(QStringLiteral("motion/max_speed"), data.maxSpeed);
    s.setValue(QStringLiteral("motion/max_acceleration"), data.maxAcceleration);
    s.setValue(QStringLiteral("motion/max_deceleration"), data.maxDeceleration);
    s.setValue(QStringLiteral("motion/max_angular_speed"), data.maxAngularSpeedDeg);
    s.setValue(QStringLiteral("motion/max_angular_acceleration"), data.maxAngularAccelerationDeg);
    s.setValue(QStringLiteral("motion/arrival_distance_tolerance"), data.arrivalDistanceTolerance);
    s.setValue(QStringLiteral("motion/arrival_angle_tolerance"), data.arrivalAngleToleranceDeg);
    s.setValue(QStringLiteral("motion/route_width"), data.routeWidth);

    s.setValue(QStringLiteral("obstacle/id_stop_range_threshold_dx"), data.defaultObsStopLength);
    s.setValue(QStringLiteral("obstacle/id_stop_range_threshold_dy"), data.defaultObsStopWidth);
    s.setValue(QStringLiteral("obstacle/id_deceleration_range_threshold_dx"), data.defaultObsDecelLength);
    s.setValue(QStringLiteral("obstacle/id_deceleration_range_threshold_dy"), data.defaultObsDecelWidth);
}

bool MotionConfigSerializer::updateFromConfigsArray(MotionConfigData &data,
                                                   const QJsonArray &configsList,
                                                   const QMap<QString, MotionParamMeta> &metas)
{
    bool changed = false;
    for (const auto &item : configsList) {
        const QJsonObject entry = item.toObject();
        const QString name = entry.value("name").toString().trimmed().toLower();
        const QString path = entry.value("file_path").toString().trimmed().toLower();
        const QJsonObject params = entry.value("params").toObject();

        if (name.contains("motion_control") || path.contains("motion_control") || name == "运动控制") {
            struct MotionBinding {
                const char *key;
                double &target;
            };
            const MotionBinding bindings[] = {
                {"max_speed", data.maxSpeed},
                {"max_acceleration", data.maxAcceleration},
                {"max_deceleration", data.maxDeceleration},
                {"max_angular_speed", data.maxAngularSpeedDeg},
                {"max_angular_acceleration", data.maxAngularAccelerationDeg},
                {"arrival_distance_tolerance", data.arrivalDistanceTolerance},
                {"arrival_angle_tolerance", data.arrivalAngleToleranceDeg},
                {"route_width", data.routeWidth}
            };
            for (const auto &b : bindings) {
                if (params.contains(b.key)) {
                    const double v = clampByParamMeta(b.key, params.value(b.key).toDouble(), metas);
                    if (v > 0.0 && !qFuzzyCompare(v, b.target)) {
                        b.target = v;
                        changed = true;
                    }
                }
            }
        }

        auto parseRangeArray = [](const QJsonObject &p, const char *key, double &outDx, double &outDy) -> bool {
            if (!p.contains(key)) return false;
            const QJsonValue v = p.value(key);
            if (!v.isArray()) return false;
            const QJsonArray arr = v.toArray();
            if (arr.size() < 2) return false;
            const double dx = arr.at(0).toDouble();
            const double dy = arr.at(1).toDouble();
            if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, outDx) || !qFuzzyCompare(dy, outDy))) {
                outDx = dx;
                outDy = dy;
                return true;
            }
            return false;
        };

        if (parseRangeArray(params, "id_stop_range_threshold", data.defaultObsStopLength, data.defaultObsStopWidth)) {
            changed = true;
        }
        if (parseRangeArray(params, "id_deceleration_range_threshold", data.defaultObsDecelLength, data.defaultObsDecelWidth)) {
            changed = true;
        }
    }
    return changed;
}

bool MotionConfigSerializer::updateObstacleConfig(MotionConfigData &data,
                                                 const QJsonObject &params)
{
    bool changed = false;
    if (params.contains(QStringLiteral("id_stop_range_threshold"))) {
        const QJsonValue val = params.value(QStringLiteral("id_stop_range_threshold"));
        if (val.isArray()) {
            const QJsonArray arr = val.toArray();
            if (arr.size() >= 2) {
                const double dx = arr.at(0).toDouble(data.defaultObsStopLength);
                const double dy = arr.at(1).toDouble(data.defaultObsStopWidth);
                if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, data.defaultObsStopLength) || !qFuzzyCompare(dy, data.defaultObsStopWidth))) {
                    data.defaultObsStopLength = dx;
                    data.defaultObsStopWidth  = dy;
                    changed = true;
                }
            }
        }
    }
    if (params.contains(QStringLiteral("id_deceleration_range_threshold"))) {
        const QJsonValue val = params.value(QStringLiteral("id_deceleration_range_threshold"));
        if (val.isArray()) {
            const QJsonArray arr = val.toArray();
            if (arr.size() >= 2) {
                const double dx = arr.at(0).toDouble(data.defaultObsDecelLength);
                const double dy = arr.at(1).toDouble(data.defaultObsDecelWidth);
                if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, data.defaultObsDecelLength) || !qFuzzyCompare(dy, data.defaultObsDecelWidth))) {
                    data.defaultObsDecelLength = dx;
                    data.defaultObsDecelWidth  = dy;
                    changed = true;
                }
            }
        }
    }
    return changed;
}

bool MotionConfigSerializer::updateFromJsonObject(MotionConfigData &data,
                                                 const QJsonObject &params,
                                                 const QMap<QString, MotionParamMeta> &metas)
{
    struct MotionBinding {
        const char *key;
        double &target;
    };
    const MotionBinding bindings[] = {
        {"max_speed", data.maxSpeed},
        {"max_acceleration", data.maxAcceleration},
        {"max_deceleration", data.maxDeceleration},
        {"max_angular_speed", data.maxAngularSpeedDeg},
        {"max_angular_acceleration", data.maxAngularAccelerationDeg},
        {"arrival_distance_tolerance", data.arrivalDistanceTolerance},
        {"arrival_angle_tolerance", data.arrivalAngleToleranceDeg},
        {"route_width", data.routeWidth}
    };

    bool changed = false;
    for (const auto &b : bindings) {
        if (params.contains(b.key)) {
            const double v = clampByParamMeta(b.key, params.value(b.key).toDouble(), metas);
            if (v > 0.0 && !qFuzzyCompare(v, b.target)) {
                b.target = v;
                changed = true;
            }
        }
    }

    if (updateObstacleConfig(data, params)) {
        changed = true;
    }
    return changed;
}

QJsonObject MotionConfigSerializer::toJsonObject(const MotionConfigData &data)
{
    QJsonObject obj;
    obj[QStringLiteral("max_speed")]                  = data.maxSpeed;
    obj[QStringLiteral("max_acceleration")]           = data.maxAcceleration;
    obj[QStringLiteral("max_deceleration")]           = data.maxDeceleration;
    obj[QStringLiteral("max_angular_speed")]          = data.maxAngularSpeedDeg;
    obj[QStringLiteral("max_angular_acceleration")]   = data.maxAngularAccelerationDeg;
    obj[QStringLiteral("arrival_distance_tolerance")] = data.arrivalDistanceTolerance;
    obj[QStringLiteral("arrival_angle_tolerance")]    = data.arrivalAngleToleranceDeg;
    obj[QStringLiteral("route_width")]                = data.routeWidth;
    return obj;
}
