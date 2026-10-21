/**
 * @file    MotionClampHelper.cpp
 * @layer   CoreScheduling
 * @brief   Implementation of kinematic limit clamping utility.
 * @deps    MotionClampHelper.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MotionClampHelper.h"

#include <cmath>

double MotionClampHelper::clampLinearSpeed(double spd, double minVal, const QMap<QString, MotionParamMeta> &metas)
{
    const double minLimit = MotionMetadataParser::getParamMin(QStringLiteral("max_speed"), metas, minVal);
    if (spd < minLimit) return minLimit;
    const double maxLimit = MotionMetadataParser::getParamMax(QStringLiteral("max_speed"), metas, kFallbackMaxLinearSpeed);
    if (maxLimit > 0.0 && spd > maxLimit) return maxLimit;
    return spd;
}

double MotionClampHelper::clampSignedLinearSpeed(double spd, const QMap<QString, MotionParamMeta> &metas)
{
    const double maxLimit = MotionMetadataParser::getParamMax(QStringLiteral("max_speed"), metas, kFallbackMaxLinearSpeed);
    if (maxLimit > 0.0) {
        if (spd > maxLimit)  return maxLimit;
        if (spd < -maxLimit) return -maxLimit;
    }
    return spd;
}

double MotionClampHelper::clampAngularSpeedDeg(double deg, double minVal, const QMap<QString, MotionParamMeta> &metas)
{
    const double minLimit = MotionMetadataParser::getParamMin(QStringLiteral("max_angular_speed"), metas, minVal);
    if (deg < minLimit) return minLimit;
    const double maxLimit = MotionMetadataParser::getParamMax(QStringLiteral("max_angular_speed"), metas, kFallbackMaxAngularSpeedDeg);
    if (maxLimit > 0.0 && deg > maxLimit) return maxLimit;
    return deg;
}

double MotionClampHelper::clampAngularSpeedRad(double rad, double minVal, const QMap<QString, MotionParamMeta> &metas)
{
    double minLimit = minVal;
    if (metas.contains(QStringLiteral("max_angular_speed")) && metas[QStringLiteral("max_angular_speed")].hasMin) {
        minLimit = metas[QStringLiteral("max_angular_speed")].minVal * M_PI / 180.0;
    }
    if (rad < minLimit) return minLimit;
    const double maxDeg = MotionMetadataParser::getParamMax(QStringLiteral("max_angular_speed"), metas, kFallbackMaxAngularSpeedDeg);
    const double maxRad = maxDeg * M_PI / 180.0;
    if (maxRad > 0.0 && rad > maxRad) return maxRad;
    return rad;
}

double MotionClampHelper::clampSignedAngularSpeedDeg(double deg, const QMap<QString, MotionParamMeta> &metas)
{
    const double maxLimit = MotionMetadataParser::getParamMax(QStringLiteral("max_angular_speed"), metas, kFallbackMaxAngularSpeedDeg);
    if (maxLimit > 0.0) {
        if (deg > maxLimit)  return maxLimit;
        if (deg < -maxLimit) return -maxLimit;
    }
    return deg;
}

double MotionClampHelper::clampSignedAngularSpeedRad(double rad, const QMap<QString, MotionParamMeta> &metas)
{
    const double maxDeg = MotionMetadataParser::getParamMax(QStringLiteral("max_angular_speed"), metas, kFallbackMaxAngularSpeedDeg);
    const double maxRad = maxDeg * M_PI / 180.0;
    if (maxRad > 0.0) {
        if (rad > maxRad)  return maxRad;
        if (rad < -maxRad) return -maxRad;
    }
    return rad;
}

double MotionClampHelper::clampAcceleration(double acc, const QMap<QString, MotionParamMeta> &metas)
{
    const double minAcc = MotionMetadataParser::getParamMin(QStringLiteral("max_acceleration"), metas, 0.0);
    const double maxAcc = MotionMetadataParser::getParamMax(QStringLiteral("max_acceleration"), metas, kFallbackMaxLinearAcceleration);
    if (minAcc > 0.0 && acc < minAcc) return minAcc;
    if (acc < 0.0) return 0.0;
    if (maxAcc > 0.0 && acc > maxAcc) return maxAcc;
    return acc;
}

double MotionClampHelper::clampDeceleration(double dec, const QMap<QString, MotionParamMeta> &metas)
{
    const double minDec = MotionMetadataParser::getParamMin(QStringLiteral("max_deceleration"), metas, 0.0);
    const double maxDec = MotionMetadataParser::getParamMax(QStringLiteral("max_deceleration"), metas, kFallbackMaxLinearAcceleration);
    if (minDec > 0.0 && dec < minDec) return minDec;
    if (dec < 0.0) return 0.0;
    if (maxDec > 0.0 && dec > maxDec) return maxDec;
    return dec;
}

double MotionClampHelper::clampAngularAccelerationDeg(double acc, const QMap<QString, MotionParamMeta> &metas)
{
    const double minAcc = MotionMetadataParser::getParamMin(QStringLiteral("max_angular_acceleration"), metas, 0.0);
    const double maxAcc = MotionMetadataParser::getParamMax(QStringLiteral("max_angular_acceleration"), metas, kFallbackMaxAngularAcceleration);
    if (minAcc > 0.0 && acc < minAcc) return minAcc;
    if (acc < 0.0) return 0.0;
    if (maxAcc > 0.0 && acc > maxAcc) return maxAcc;
    return acc;
}
