/**
 * @file    MotionClampHelper.h
 * @layer   CoreScheduling
 * @brief   Kinematic limit clamping utility for linear and angular velocities.
 * @deps    MotionMetadataParser.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include "MotionMetadataParser.h"

class MotionClampHelper final
{
public:
    MotionClampHelper() = delete;
    ~MotionClampHelper() = delete;

    static constexpr double kFallbackMaxLinearSpeed         = 1.0;
    static constexpr double kFallbackMaxAngularSpeedDeg     = 60.0;
    static constexpr double kFallbackMaxLinearAcceleration  = 0.5;
    static constexpr double kFallbackMaxAngularAcceleration = 30.0;

    static double clampLinearSpeed(double spd, double minVal, const QMap<QString, MotionParamMeta> &metas);
    static double clampSignedLinearSpeed(double spd, const QMap<QString, MotionParamMeta> &metas);
    static double clampAngularSpeedDeg(double deg, double minVal, const QMap<QString, MotionParamMeta> &metas);
    static double clampAngularSpeedRad(double rad, double minVal, const QMap<QString, MotionParamMeta> &metas);
    static double clampSignedAngularSpeedDeg(double deg, const QMap<QString, MotionParamMeta> &metas);
    static double clampSignedAngularSpeedRad(double rad, const QMap<QString, MotionParamMeta> &metas);
    static double clampAcceleration(double acc, const QMap<QString, MotionParamMeta> &metas);
    static double clampDeceleration(double dec, const QMap<QString, MotionParamMeta> &metas);
    static double clampAngularAccelerationDeg(double acc, const QMap<QString, MotionParamMeta> &metas);
};
