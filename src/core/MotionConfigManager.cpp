#include "MotionConfigManager.h"
#include <QSettings>
#include <QtGlobal>
#include <QDebug>
#include <QJsonArray>

MotionConfigManager &MotionConfigManager::instance()
{
    static MotionConfigManager inst;
    return inst;
}

MotionConfigManager::MotionConfigManager(QObject *parent)
    : QObject(parent)
{
    loadFromSettings();
}

void MotionConfigManager::loadFromSettings()
{
    QSettings s;
    m_maxSpeed                 = s.value(QStringLiteral("motion/max_speed"), 0.5).toDouble();
    m_maxAcceleration          = s.value(QStringLiteral("motion/max_acceleration"), 0.3).toDouble();
    m_maxDeceleration          = s.value(QStringLiteral("motion/max_deceleration"), 0.3).toDouble();
    m_maxAngularSpeedDeg       = s.value(QStringLiteral("motion/max_angular_speed"), 60.0).toDouble();
    m_arrivalDistanceTolerance = s.value(QStringLiteral("motion/arrival_distance_tolerance"), 0.1).toDouble();
    m_arrivalAngleToleranceDeg = s.value(QStringLiteral("motion/arrival_angle_tolerance"), 0.5).toDouble();
    m_routeWidth               = s.value(QStringLiteral("motion/route_width"), 4.0).toDouble();

    // 默认避障参数 (id_stop_range_threshold: [0.1, 0.1], id_deceleration_range_threshold: [0.5, 0.3])
    m_defaultObsStopLength     = s.value(QStringLiteral("obstacle/id_stop_range_threshold_dx"), 0.1).toDouble();
    m_defaultObsStopWidth      = s.value(QStringLiteral("obstacle/id_stop_range_threshold_dy"), 0.1).toDouble();
    m_defaultObsDecelLength    = s.value(QStringLiteral("obstacle/id_deceleration_range_threshold_dx"), 0.5).toDouble();
    m_defaultObsDecelWidth     = s.value(QStringLiteral("obstacle/id_deceleration_range_threshold_dy"), 0.3).toDouble();

    // 合理性校验
    if (m_maxSpeed <= 0.0)                 m_maxSpeed = 0.5;
    if (m_maxAcceleration <= 0.0)          m_maxAcceleration = 0.3;
    if (m_maxDeceleration <= 0.0)          m_maxDeceleration = 0.3;
    if (m_maxAngularSpeedDeg <= 0.0)       m_maxAngularSpeedDeg = 60.0;
    if (m_arrivalDistanceTolerance <= 0.0) m_arrivalDistanceTolerance = 0.1;
    if (m_arrivalAngleToleranceDeg <= 0.0) m_arrivalAngleToleranceDeg = 0.5;
    if (m_routeWidth <= 0.0)               m_routeWidth = 4.0;

    if (m_defaultObsStopLength < 0.0)      m_defaultObsStopLength = 0.1;
    if (m_defaultObsStopWidth < 0.0)       m_defaultObsStopWidth = 0.1;
    if (m_defaultObsDecelLength < 0.0)     m_defaultObsDecelLength = 0.5;
    if (m_defaultObsDecelWidth < 0.0)      m_defaultObsDecelWidth = 0.3;
}

void MotionConfigManager::saveToSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("motion/max_speed"), m_maxSpeed);
    s.setValue(QStringLiteral("motion/max_acceleration"), m_maxAcceleration);
    s.setValue(QStringLiteral("motion/max_deceleration"), m_maxDeceleration);
    s.setValue(QStringLiteral("motion/max_angular_speed"), m_maxAngularSpeedDeg);
    s.setValue(QStringLiteral("motion/arrival_distance_tolerance"), m_arrivalDistanceTolerance);
    s.setValue(QStringLiteral("motion/arrival_angle_tolerance"), m_arrivalAngleToleranceDeg);
    s.setValue(QStringLiteral("motion/route_width"), m_routeWidth);

    s.setValue(QStringLiteral("obstacle/id_stop_range_threshold_dx"), m_defaultObsStopLength);
    s.setValue(QStringLiteral("obstacle/id_stop_range_threshold_dy"), m_defaultObsStopWidth);
    s.setValue(QStringLiteral("obstacle/id_deceleration_range_threshold_dx"), m_defaultObsDecelLength);
    s.setValue(QStringLiteral("obstacle/id_deceleration_range_threshold_dy"), m_defaultObsDecelWidth);
}

void MotionConfigManager::setMaxSpeed(double val)
{
    if (val > 0.0 && !qFuzzyCompare(val, m_maxSpeed)) {
        m_maxSpeed = val;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxAcceleration(double val)
{
    if (val > 0.0 && !qFuzzyCompare(val, m_maxAcceleration)) {
        m_maxAcceleration = val;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxDeceleration(double val)
{
    if (val > 0.0 && !qFuzzyCompare(val, m_maxDeceleration)) {
        m_maxDeceleration = val;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxAngularSpeedDeg(double val)
{
    if (val > 0.0 && !qFuzzyCompare(val, m_maxAngularSpeedDeg)) {
        m_maxAngularSpeedDeg = val;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setArrivalDistanceTolerance(double val)
{
    if (val > 0.0 && !qFuzzyCompare(val, m_arrivalDistanceTolerance)) {
        m_arrivalDistanceTolerance = val;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setArrivalAngleToleranceDeg(double val)
{
    if (val > 0.0 && !qFuzzyCompare(val, m_arrivalAngleToleranceDeg)) {
        m_arrivalAngleToleranceDeg = val;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setRouteWidth(double val)
{
    if (val > 0.0 && !qFuzzyCompare(val, m_routeWidth)) {
        m_routeWidth = val;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setDefaultObsStopRange(double dx, double dy)
{
    if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, m_defaultObsStopLength) || !qFuzzyCompare(dy, m_defaultObsStopWidth))) {
        m_defaultObsStopLength = dx;
        m_defaultObsStopWidth = dy;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setDefaultObsDecelRange(double dx, double dy)
{
    if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, m_defaultObsDecelLength) || !qFuzzyCompare(dy, m_defaultObsDecelWidth))) {
        m_defaultObsDecelLength = dx;
        m_defaultObsDecelWidth = dy;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::updateObstacleConfig(const QJsonObject &params)
{
    bool changed = false;
    if (params.contains(QStringLiteral("id_stop_range_threshold"))) {
        QJsonValue val = params.value(QStringLiteral("id_stop_range_threshold"));
        if (val.isArray()) {
            QJsonArray arr = val.toArray();
            if (arr.size() >= 2) {
                double dx = arr.at(0).toDouble(m_defaultObsStopLength);
                double dy = arr.at(1).toDouble(m_defaultObsStopWidth);
                if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, m_defaultObsStopLength) || !qFuzzyCompare(dy, m_defaultObsStopWidth))) {
                    m_defaultObsStopLength = dx;
                    m_defaultObsStopWidth  = dy;
                    changed = true;
                }
            }
        }
    }
    if (params.contains(QStringLiteral("id_deceleration_range_threshold"))) {
        QJsonValue val = params.value(QStringLiteral("id_deceleration_range_threshold"));
        if (val.isArray()) {
            QJsonArray arr = val.toArray();
            if (arr.size() >= 2) {
                double dx = arr.at(0).toDouble(m_defaultObsDecelLength);
                double dy = arr.at(1).toDouble(m_defaultObsDecelWidth);
                if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, m_defaultObsDecelLength) || !qFuzzyCompare(dy, m_defaultObsDecelWidth))) {
                    m_defaultObsDecelLength = dx;
                    m_defaultObsDecelWidth  = dy;
                    changed = true;
                }
            }
        }
    }

    if (changed) {
        saveToSettings();
        qDebug() << "[MotionConfigManager] Updated obstacle parameters: stopRange=[" << m_defaultObsStopLength << "," << m_defaultObsStopWidth
                 << "] decelRange=[" << m_defaultObsDecelLength << "," << m_defaultObsDecelWidth << "]";
        emit configChanged();
    }
}

void MotionConfigManager::updateFromJsonObject(const QJsonObject &params)
{
    bool changed = false;
    if (params.contains(QStringLiteral("max_speed"))) {
        double v = params.value(QStringLiteral("max_speed")).toDouble();
        if (v > 0.0 && !qFuzzyCompare(v, m_maxSpeed)) { m_maxSpeed = v; changed = true; }
    }
    if (params.contains(QStringLiteral("max_acceleration"))) {
        double v = params.value(QStringLiteral("max_acceleration")).toDouble();
        if (v > 0.0 && !qFuzzyCompare(v, m_maxAcceleration)) { m_maxAcceleration = v; changed = true; }
    }
    if (params.contains(QStringLiteral("max_deceleration"))) {
        double v = params.value(QStringLiteral("max_deceleration")).toDouble();
        if (v > 0.0 && !qFuzzyCompare(v, m_maxDeceleration)) { m_maxDeceleration = v; changed = true; }
    }
    if (params.contains(QStringLiteral("max_angular_speed"))) {
        double v = params.value(QStringLiteral("max_angular_speed")).toDouble();
        if (v > 0.0 && !qFuzzyCompare(v, m_maxAngularSpeedDeg)) { m_maxAngularSpeedDeg = v; changed = true; }
    }
    if (params.contains(QStringLiteral("arrival_distance_tolerance"))) {
        double v = params.value(QStringLiteral("arrival_distance_tolerance")).toDouble();
        if (v > 0.0 && !qFuzzyCompare(v, m_arrivalDistanceTolerance)) { m_arrivalDistanceTolerance = v; changed = true; }
    }
    if (params.contains(QStringLiteral("arrival_angle_tolerance"))) {
        double v = params.value(QStringLiteral("arrival_angle_tolerance")).toDouble();
        if (v > 0.0 && !qFuzzyCompare(v, m_arrivalAngleToleranceDeg)) { m_arrivalAngleToleranceDeg = v; changed = true; }
    }
    if (params.contains(QStringLiteral("route_width"))) {
        double v = params.value(QStringLiteral("route_width")).toDouble();
        if (v > 0.0 && !qFuzzyCompare(v, m_routeWidth)) { m_routeWidth = v; changed = true; }
    }

    if (changed) {
        saveToSettings();
        qDebug() << "[MotionConfigManager] Updated motion parameters: max_speed=" << m_maxSpeed
                 << "max_angular_speed=" << m_maxAngularSpeedDeg;
        emit configChanged();
    }

    updateObstacleConfig(params);
}

QJsonObject MotionConfigManager::toJsonObject() const
{
    QJsonObject obj;
    obj[QStringLiteral("max_speed")]                  = m_maxSpeed;
    obj[QStringLiteral("max_acceleration")]           = m_maxAcceleration;
    obj[QStringLiteral("max_deceleration")]           = m_maxDeceleration;
    obj[QStringLiteral("max_angular_speed")]          = m_maxAngularSpeedDeg;
    obj[QStringLiteral("arrival_distance_tolerance")] = m_arrivalDistanceTolerance;
    obj[QStringLiteral("arrival_angle_tolerance")]    = m_arrivalAngleToleranceDeg;
    obj[QStringLiteral("route_width")]                = m_routeWidth;
    return obj;
}

double MotionConfigManager::clampLinearSpeed(double spd, double minVal) const
{
    if (spd < minVal) return minVal;
    if (spd > m_maxSpeed) return m_maxSpeed;
    return spd;
}

double MotionConfigManager::clampSignedLinearSpeed(double spd) const
{
    if (spd > m_maxSpeed)  return m_maxSpeed;
    if (spd < -m_maxSpeed) return -m_maxSpeed;
    return spd;
}

double MotionConfigManager::clampAngularSpeedDeg(double deg, double minVal) const
{
    if (deg < minVal) return minVal;
    if (deg > m_maxAngularSpeedDeg) return m_maxAngularSpeedDeg;
    return deg;
}

double MotionConfigManager::clampAngularSpeedRad(double rad, double minVal) const
{
    double maxRad = maxAngularSpeedRad();
    if (rad < minVal) return minVal;
    if (rad > maxRad) return maxRad;
    return rad;
}

double MotionConfigManager::clampSignedAngularSpeedDeg(double deg) const
{
    if (deg > m_maxAngularSpeedDeg)  return m_maxAngularSpeedDeg;
    if (deg < -m_maxAngularSpeedDeg) return -m_maxAngularSpeedDeg;
    return deg;
}

double MotionConfigManager::clampSignedAngularSpeedRad(double rad) const
{
    double maxRad = maxAngularSpeedRad();
    if (rad > maxRad)  return maxRad;
    if (rad < -maxRad) return -maxRad;
    return rad;
}

double MotionConfigManager::clampAcceleration(double acc) const
{
    if (acc < 0.01) return 0.01;
    if (acc > m_maxAcceleration) return m_maxAcceleration;
    return acc;
}

double MotionConfigManager::clampDeceleration(double dec) const
{
    if (dec < 0.01) return 0.01;
    if (dec > m_maxDeceleration) return m_maxDeceleration;
    return dec;
}
