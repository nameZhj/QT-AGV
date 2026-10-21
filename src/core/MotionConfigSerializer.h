/**
 * @file    MotionConfigSerializer.h
 * @layer   CoreScheduling
 * @brief   Settings persistence and JSON serializer for kinematic motion parameters.
 * @deps    MotionMetadataParser.h, QJsonObject, QJsonArray
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include "MotionMetadataParser.h"

#include <QJsonObject>
#include <QJsonArray>

struct MotionConfigData {
    double maxSpeed                  = 0.5;
    double maxAcceleration           = 0.4;
    double maxDeceleration           = 0.4;
    double maxAngularSpeedDeg        = 50.0;
    double maxAngularAccelerationDeg = 20.0;
    double arrivalDistanceTolerance  = 0.1;
    double arrivalAngleToleranceDeg  = 0.5;
    double routeWidth                = 4.0;

    double defaultObsStopLength      = 0.1;
    double defaultObsStopWidth       = 0.1;
    double defaultObsDecelLength     = 0.5;
    double defaultObsDecelWidth      = 0.3;
};

class MotionConfigSerializer final
{
public:
    MotionConfigSerializer() = delete;
    ~MotionConfigSerializer() = delete;

    static double clampByParamMeta(const QString &paramName,
                                   double val,
                                   const QMap<QString, MotionParamMeta> &metas);

    static void loadFromSettings(MotionConfigData &data,
                                 QMap<QString, MotionParamMeta> &metas);

    static void saveToSettings(const MotionConfigData &data);

    static bool updateFromConfigsArray(MotionConfigData &data,
                                       const QJsonArray &configsList,
                                       const QMap<QString, MotionParamMeta> &metas);

    static bool updateFromJsonObject(MotionConfigData &data,
                                     const QJsonObject &params,
                                     const QMap<QString, MotionParamMeta> &metas);

    static bool updateObstacleConfig(MotionConfigData &data,
                                     const QJsonObject &params);

    static QJsonObject toJsonObject(const MotionConfigData &data);
};
