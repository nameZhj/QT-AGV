/**
 * @file    MotionConfigManager.h
 * @module  AGVModel
 * @brief   Kinematic motion parameter manager and velocity/acceleration constraint profile model.
 * @deps    QObject, QJsonObject, ApiService
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#pragma once

#include <QObject>
#include <QJsonObject>
#include <cmath>
#include "MotionMetadataParser.h"
#include "MotionClampHelper.h"
#include "MotionConfigSerializer.h"

class MotionConfigManager : public QObject
{
    Q_OBJECT

public:
    // === [Section: Singleton Access] ===
    static MotionConfigManager &instance();

    // === [Section: Physical & Default Constants] ===
    static constexpr double kPi                       = 3.14159265358979323846;
    static constexpr double kDegToRad                 = kPi / 180.0;

    static constexpr double kDefaultMaxSpeed                  = 0.5;   ///< 最大线速度 (m/s)
    static constexpr double kDefaultMaxAcceleration           = 0.4;   ///< 最大线加速度 (m/s²)
    static constexpr double kDefaultMaxDeceleration           = 0.4;   ///< 最大线减速度 (m/s²)
    static constexpr double kDefaultMaxAngularSpeedDeg        = 50.0;  ///< 最大角速度 (°/s)
    static constexpr double kDefaultMaxAngularAccelerationDeg = 20.0;  ///< 最大角加速度 (°/s²)
    static constexpr double kDefaultArrivalDistanceTolerance  = 0.1;   ///< 到达距离误差 (m)
    static constexpr double kDefaultArrivalAngleToleranceDeg  = 0.5;   ///< 到达角度误差 (°)
    static constexpr double kDefaultRouteWidth                = 4.0;   ///< 路线宽度 (m)

    static constexpr double kDefaultObsStopLength             = 0.1;   ///< 纵向停车扩展量 dx (m)
    static constexpr double kDefaultObsStopWidth              = 0.1;   ///< 横向停车扩展量 dy (m)
    static constexpr double kDefaultObsDecelLength            = 0.5;   ///< 纵向减速扩展量 dx (m)
    static constexpr double kDefaultObsDecelWidth             = 0.3;   ///< 横向减速扩展量 dy (m)

    // === [Section: Property Accessors] ===
    double maxSpeed() const { return m_maxSpeed; }
    double maxAcceleration() const { return m_maxAcceleration; }
    double maxDeceleration() const { return m_maxDeceleration; }
    double maxAngularSpeedDeg() const { return m_maxAngularSpeedDeg; }
    double maxAngularSpeedRad() const { return m_maxAngularSpeedDeg * kDegToRad; }
    double maxAngularAccelerationDeg() const { return m_maxAngularAccelerationDeg; }
    double maxAngularAccelerationRad() const { return m_maxAngularAccelerationDeg * kDegToRad; }
    double arrivalDistanceTolerance() const { return m_arrivalDistanceTolerance; }
    double arrivalAngleToleranceDeg() const { return m_arrivalAngleToleranceDeg; }
    double arrivalAngleToleranceRad() const { return m_arrivalAngleToleranceDeg * kDegToRad; }
    double routeWidth() const { return m_routeWidth; }

    double defaultObsStopLength() const { return m_defaultObsStopLength; }
    double defaultObsStopWidth() const { return m_defaultObsStopWidth; }
    double defaultObsDecelLength() const { return m_defaultObsDecelLength; }
    double defaultObsDecelWidth() const { return m_defaultObsDecelWidth; }

    // === [Section: Metadata & Schema Introspection] ===
    using MotionParamMeta = ::MotionParamMeta;
    using ParamRange      = ::MotionParamRange;

    void updateFromMetadata(const QByteArray &rawJson);
    void updateFromMetadataArray(const QJsonArray &metaList);
    void updateFromConfigsArray(const QJsonArray &configsList);

    bool hasParamMeta(const QString &paramName) const {
        return m_paramMetas.contains(paramName);
    }
    MotionParamMeta paramMeta(const QString &paramName) const {
        return m_paramMetas.value(MotionMetadataParser::canonicalKey(paramName, m_paramMetas));
    }
    double paramMin(const QString &paramName, double fallback = 0.0) const {
        return MotionMetadataParser::getParamMin(paramName, m_paramMetas, fallback);
    }
    double paramMax(const QString &paramName, double fallback = 0.0) const {
        return MotionMetadataParser::getParamMax(paramName, m_paramMetas, fallback);
    }
    double paramDefault(const QString &paramName, double fallback = 0.0) const {
        return MotionMetadataParser::getParamDefault(paramName, m_paramMetas, fallback);
    }
    double clampByParamMeta(const QString &paramName, double val) const {
        return MotionConfigSerializer::clampByParamMeta(paramName, val, m_paramMetas);
    }

    ParamRange paramRange(const QString &paramName) const {
        return MotionMetadataParser::getParamRange(paramName, m_paramMetas);
    }
    const QMap<QString, MotionParamMeta> &allParamMetas() const { return m_paramMetas; }

    // === [Section: Property Mutators & Synchronization] ===
    void setMaxSpeed(double val);
    void setMaxAcceleration(double val);
    void setMaxDeceleration(double val);
    void setMaxAngularSpeedDeg(double val);
    void setMaxAngularAccelerationDeg(double val);
    void setArrivalDistanceTolerance(double val);
    void setArrivalAngleToleranceDeg(double val);
    void setRouteWidth(double val);
    void setDefaultObsStopRange(double dx, double dy);
    void setDefaultObsDecelRange(double dx, double dy);

    void updateFromJsonObject(const QJsonObject &params);
    void updateObstacleConfig(const QJsonObject &params);
    QJsonObject toJsonObject() const;

    // === [Section: Value Clamping Helpers] ===
    double clampLinearSpeed(double spd, double minVal = 0.0) const {
        return MotionClampHelper::clampLinearSpeed(spd, minVal, m_paramMetas);
    }
    double clampSignedLinearSpeed(double spd) const {
        return MotionClampHelper::clampSignedLinearSpeed(spd, m_paramMetas);
    }
    double clampAngularSpeedDeg(double deg, double minVal = 0.0) const {
        return MotionClampHelper::clampAngularSpeedDeg(deg, minVal, m_paramMetas);
    }
    double clampAngularSpeedRad(double rad, double minVal = 0.0) const {
        return MotionClampHelper::clampAngularSpeedRad(rad, minVal, m_paramMetas);
    }
    double clampSignedAngularSpeedDeg(double deg) const {
        return MotionClampHelper::clampSignedAngularSpeedDeg(deg, m_paramMetas);
    }
    double clampSignedAngularSpeedRad(double rad) const {
        return MotionClampHelper::clampSignedAngularSpeedRad(rad, m_paramMetas);
    }
    double clampAcceleration(double acc) const {
        return MotionClampHelper::clampAcceleration(acc, m_paramMetas);
    }
    double clampDeceleration(double dec) const {
        return MotionClampHelper::clampDeceleration(dec, m_paramMetas);
    }
    double clampAngularAccelerationDeg(double acc) const {
        return MotionClampHelper::clampAngularAccelerationDeg(acc, m_paramMetas);
    }

signals:
    void configChanged();

private:
    // === [Section: Lifecycle & Internal State] ===
    explicit MotionConfigManager(QObject *parent = nullptr);
    ~MotionConfigManager() override = default;
    MotionConfigManager(const MotionConfigManager &) = delete;
    MotionConfigManager &operator=(const MotionConfigManager &) = delete;

    void loadFromSettings();
    void saveToSettings();

    void applyConfigData(const MotionConfigData &d) {
        m_maxSpeed                  = d.maxSpeed;
        m_maxAcceleration           = d.maxAcceleration;
        m_maxDeceleration           = d.maxDeceleration;
        m_maxAngularSpeedDeg        = d.maxAngularSpeedDeg;
        m_maxAngularAccelerationDeg = d.maxAngularAccelerationDeg;
        m_arrivalDistanceTolerance  = d.arrivalDistanceTolerance;
        m_arrivalAngleToleranceDeg  = d.arrivalAngleToleranceDeg;
        m_routeWidth                = d.routeWidth;
        m_defaultObsStopLength      = d.defaultObsStopLength;
        m_defaultObsStopWidth       = d.defaultObsStopWidth;
        m_defaultObsDecelLength     = d.defaultObsDecelLength;
        m_defaultObsDecelWidth      = d.defaultObsDecelWidth;
    }

    double m_maxSpeed                  = kDefaultMaxSpeed;
    double m_maxAcceleration           = kDefaultMaxAcceleration;
    double m_maxDeceleration           = kDefaultMaxDeceleration;
    double m_maxAngularSpeedDeg        = kDefaultMaxAngularSpeedDeg;
    double m_maxAngularAccelerationDeg = kDefaultMaxAngularAccelerationDeg;
    double m_arrivalDistanceTolerance  = kDefaultArrivalDistanceTolerance;
    double m_arrivalAngleToleranceDeg  = kDefaultArrivalAngleToleranceDeg;
    double m_routeWidth                = kDefaultRouteWidth;

    double m_defaultObsStopLength      = kDefaultObsStopLength;
    double m_defaultObsStopWidth       = kDefaultObsStopWidth;
    double m_defaultObsDecelLength     = kDefaultObsDecelLength;
    double m_defaultObsDecelWidth      = kDefaultObsDecelWidth;

    QMap<QString, MotionParamMeta> m_paramMetas;
};

