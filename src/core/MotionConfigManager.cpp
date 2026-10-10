#include "MotionConfigManager.h"
#include "ApiService.h"
#include <QSettings>
#include <QtGlobal>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

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

void MotionConfigManager::updateFromConfigsArray(const QJsonArray &configsList)
{
    bool changed = false;
    for (const auto &item : configsList) {
        QJsonObject entry = item.toObject();
        QString name = entry.value("name").toString().trimmed().toLower();
        QString path = entry.value("file_path").toString().trimmed().toLower();
        QJsonObject params = entry.value("params").toObject();

        if (name.contains("motion_control") || path.contains("motion_control") || name == "运动控制") {
            if (params.contains("max_speed")) {
                double v = clampByParamMeta("max_speed", params.value("max_speed").toDouble());
                if (v > 0.0 && !qFuzzyCompare(v, m_maxSpeed)) { m_maxSpeed = v; changed = true; }
            }
            if (params.contains("max_acceleration")) {
                double v = clampByParamMeta("max_acceleration", params.value("max_acceleration").toDouble());
                if (v > 0.0 && !qFuzzyCompare(v, m_maxAcceleration)) { m_maxAcceleration = v; changed = true; }
            }
            if (params.contains("max_deceleration")) {
                double v = clampByParamMeta("max_deceleration", params.value("max_deceleration").toDouble());
                if (v > 0.0 && !qFuzzyCompare(v, m_maxDeceleration)) { m_maxDeceleration = v; changed = true; }
            }
            if (params.contains("max_angular_speed")) {
                double v = clampByParamMeta("max_angular_speed", params.value("max_angular_speed").toDouble());
                if (v > 0.0 && !qFuzzyCompare(v, m_maxAngularSpeedDeg)) { m_maxAngularSpeedDeg = v; changed = true; }
            }
            if (params.contains("max_angular_acceleration")) {
                double v = clampByParamMeta("max_angular_acceleration", params.value("max_angular_acceleration").toDouble());
                if (v > 0.0 && !qFuzzyCompare(v, m_maxAngularAccelerationDeg)) { m_maxAngularAccelerationDeg = v; changed = true; }
            }
            if (params.contains("arrival_distance_tolerance")) {
                double v = clampByParamMeta("arrival_distance_tolerance", params.value("arrival_distance_tolerance").toDouble());
                if (v > 0.0 && !qFuzzyCompare(v, m_arrivalDistanceTolerance)) { m_arrivalDistanceTolerance = v; changed = true; }
            }
            if (params.contains("arrival_angle_tolerance")) {
                double v = clampByParamMeta("arrival_angle_tolerance", params.value("arrival_angle_tolerance").toDouble());
                if (v > 0.0 && !qFuzzyCompare(v, m_arrivalAngleToleranceDeg)) { m_arrivalAngleToleranceDeg = v; changed = true; }
            }
            if (params.contains("route_width")) {
                double v = clampByParamMeta("route_width", params.value("route_width").toDouble());
                if (v > 0.0 && !qFuzzyCompare(v, m_routeWidth)) { m_routeWidth = v; changed = true; }
            }
        }

        if (params.contains("id_stop_range_threshold") || params.contains("id_deceleration_range_threshold")) {
            if (params.contains("id_stop_range_threshold")) {
                QJsonValue v = params.value("id_stop_range_threshold");
                if (v.isArray()) {
                    QJsonArray arr = v.toArray();
                    if (arr.size() >= 2) {
                        double dx = arr.at(0).toDouble();
                        double dy = arr.at(1).toDouble();
                        if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, m_defaultObsStopLength) || !qFuzzyCompare(dy, m_defaultObsStopWidth))) {
                            m_defaultObsStopLength = dx;
                            m_defaultObsStopWidth = dy;
                            changed = true;
                        }
                    }
                }
            }
            if (params.contains("id_deceleration_range_threshold")) {
                QJsonValue v = params.value("id_deceleration_range_threshold");
                if (v.isArray()) {
                    QJsonArray arr = v.toArray();
                    if (arr.size() >= 2) {
                        double dx = arr.at(0).toDouble();
                        double dy = arr.at(1).toDouble();
                        if (dx >= 0.0 && dy >= 0.0 && (!qFuzzyCompare(dx, m_defaultObsDecelLength) || !qFuzzyCompare(dy, m_defaultObsDecelWidth))) {
                            m_defaultObsDecelLength = dx;
                            m_defaultObsDecelWidth = dy;
                            changed = true;
                        }
                    }
                }
            }
        }
    }

    if (changed) {
        saveToSettings();
        qDebug() << "[MotionConfigManager] Synced from get_all_configs:"
                 << "max_speed=" << m_maxSpeed << "max_acc=" << m_maxAcceleration
                 << "max_rot=" << m_maxAngularSpeedDeg << "max_rot_acc=" << m_maxAngularAccelerationDeg
                 << "stopRange=[" << m_defaultObsStopLength << "," << m_defaultObsStopWidth << "]"
                 << "decelRange=[" << m_defaultObsDecelLength << "," << m_defaultObsDecelWidth << "]";
        emit configChanged();
    }
}

void MotionConfigManager::loadFromSettings()
{
    QSettings s;
    // 1. 优先从本地缓存恢复元数据范围
    QByteArray cachedMeta = s.value(QStringLiteral("editConfig/cachedMetadata")).toByteArray();
    if (!cachedMeta.isEmpty()) {
        updateFromMetadata(cachedMeta);
    }

    // 2. 优先从本地缓存的配置列表 (get_all_configs) 中动态恢复各项参数值
    QByteArray cachedConfigs = s.value(QStringLiteral("editConfig/cachedConfigs")).toByteArray();
    if (!cachedConfigs.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(cachedConfigs);
        if (doc.isObject()) {
            QJsonObject root = doc.object();
            if (root.contains("data")) {
                QJsonValue dv = root.value("data");
                if (dv.isArray()) {
                    updateFromConfigsArray(dv.toArray());
                } else if (dv.isObject()) {
                    QJsonObject dataObj = dv.toObject();
                    if (dataObj.contains("data") && dataObj.value("data").isArray())
                        updateFromConfigsArray(dataObj.value("data").toArray());
                }
            }
        } else if (doc.isArray()) {
            updateFromConfigsArray(doc.array());
        }
    }

    // 3. 从单独持久化字段中恢复并经元数据范围校验
    m_maxSpeed                  = clampByParamMeta(QStringLiteral("max_speed"), s.value(QStringLiteral("motion/max_speed"), m_maxSpeed).toDouble());
    m_maxAcceleration           = clampByParamMeta(QStringLiteral("max_acceleration"), s.value(QStringLiteral("motion/max_acceleration"), m_maxAcceleration).toDouble());
    m_maxDeceleration           = clampByParamMeta(QStringLiteral("max_deceleration"), s.value(QStringLiteral("motion/max_deceleration"), m_maxDeceleration).toDouble());
    m_maxAngularSpeedDeg        = clampByParamMeta(QStringLiteral("max_angular_speed"), s.value(QStringLiteral("motion/max_angular_speed"), m_maxAngularSpeedDeg).toDouble());
    m_maxAngularAccelerationDeg = clampByParamMeta(QStringLiteral("max_angular_acceleration"), s.value(QStringLiteral("motion/max_angular_acceleration"), m_maxAngularAccelerationDeg).toDouble());
    m_arrivalDistanceTolerance  = clampByParamMeta(QStringLiteral("arrival_distance_tolerance"), s.value(QStringLiteral("motion/arrival_distance_tolerance"), m_arrivalDistanceTolerance).toDouble());
    m_arrivalAngleToleranceDeg  = clampByParamMeta(QStringLiteral("arrival_angle_tolerance"), s.value(QStringLiteral("motion/arrival_angle_tolerance"), m_arrivalAngleToleranceDeg).toDouble());
    m_routeWidth                = clampByParamMeta(QStringLiteral("route_width"), s.value(QStringLiteral("motion/route_width"), m_routeWidth).toDouble());

    m_defaultObsStopLength     = s.value(QStringLiteral("obstacle/id_stop_range_threshold_dx"), m_defaultObsStopLength).toDouble();
    m_defaultObsStopWidth      = s.value(QStringLiteral("obstacle/id_stop_range_threshold_dy"), m_defaultObsStopWidth).toDouble();
    m_defaultObsDecelLength    = s.value(QStringLiteral("obstacle/id_deceleration_range_threshold_dx"), m_defaultObsDecelLength).toDouble();
    m_defaultObsDecelWidth     = s.value(QStringLiteral("obstacle/id_deceleration_range_threshold_dy"), m_defaultObsDecelWidth).toDouble();
}

void MotionConfigManager::saveToSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("motion/max_speed"), maxSpeed());
    s.setValue(QStringLiteral("motion/max_acceleration"), maxAcceleration());
    s.setValue(QStringLiteral("motion/max_deceleration"), maxDeceleration());
    s.setValue(QStringLiteral("motion/max_angular_speed"), maxAngularSpeedDeg());
    s.setValue(QStringLiteral("motion/max_angular_acceleration"), maxAngularAccelerationDeg());
    s.setValue(QStringLiteral("motion/arrival_distance_tolerance"), m_arrivalDistanceTolerance);
    s.setValue(QStringLiteral("motion/arrival_angle_tolerance"), m_arrivalAngleToleranceDeg);
    s.setValue(QStringLiteral("motion/route_width"), m_routeWidth);

    s.setValue(QStringLiteral("obstacle/id_stop_range_threshold_dx"), m_defaultObsStopLength);
    s.setValue(QStringLiteral("obstacle/id_stop_range_threshold_dy"), m_defaultObsStopWidth);
    s.setValue(QStringLiteral("obstacle/id_deceleration_range_threshold_dx"), m_defaultObsDecelLength);
    s.setValue(QStringLiteral("obstacle/id_deceleration_range_threshold_dy"), m_defaultObsDecelWidth);
}

double MotionConfigManager::clampByParamMeta(const QString &paramName, double val) const
{
    if (m_paramMetas.contains(paramName)) {
        const auto &meta = m_paramMetas[paramName];
        if (meta.hasMin && val < meta.minVal) val = meta.minVal;
        if (meta.hasMax && val > meta.maxVal) val = meta.maxVal;
    }
    return val;
}

void MotionConfigManager::setMaxSpeed(double val)
{
    double clamped = clampByParamMeta(QStringLiteral("max_speed"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxSpeed)) {
        m_maxSpeed = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxAcceleration(double val)
{
    double clamped = clampByParamMeta(QStringLiteral("max_acceleration"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxAcceleration)) {
        m_maxAcceleration = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxDeceleration(double val)
{
    double clamped = clampByParamMeta(QStringLiteral("max_deceleration"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxDeceleration)) {
        m_maxDeceleration = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxAngularSpeedDeg(double val)
{
    double clamped = clampByParamMeta(QStringLiteral("max_angular_speed"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxAngularSpeedDeg)) {
        m_maxAngularSpeedDeg = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxAngularAccelerationDeg(double val)
{
    double clamped = clampByParamMeta(QStringLiteral("max_angular_acceleration"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxAngularAccelerationDeg)) {
        m_maxAngularAccelerationDeg = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setArrivalDistanceTolerance(double val)
{
    double clamped = clampByParamMeta(QStringLiteral("arrival_distance_tolerance"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_arrivalDistanceTolerance)) {
        m_arrivalDistanceTolerance = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setArrivalAngleToleranceDeg(double val)
{
    double clamped = clampByParamMeta(QStringLiteral("arrival_angle_tolerance"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_arrivalAngleToleranceDeg)) {
        m_arrivalAngleToleranceDeg = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setRouteWidth(double val)
{
    double clamped = clampByParamMeta(QStringLiteral("route_width"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_routeWidth)) {
        m_routeWidth = clamped;
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
        double v = clampByParamMeta(QStringLiteral("max_speed"), params.value(QStringLiteral("max_speed")).toDouble());
        if (v > 0.0 && !qFuzzyCompare(v, m_maxSpeed)) { m_maxSpeed = v; changed = true; }
    }
    if (params.contains(QStringLiteral("max_acceleration"))) {
        double v = clampByParamMeta(QStringLiteral("max_acceleration"), params.value(QStringLiteral("max_acceleration")).toDouble());
        if (v > 0.0 && !qFuzzyCompare(v, m_maxAcceleration)) { m_maxAcceleration = v; changed = true; }
    }
    if (params.contains(QStringLiteral("max_deceleration"))) {
        double v = clampByParamMeta(QStringLiteral("max_deceleration"), params.value(QStringLiteral("max_deceleration")).toDouble());
        if (v > 0.0 && !qFuzzyCompare(v, m_maxDeceleration)) { m_maxDeceleration = v; changed = true; }
    }
    if (params.contains(QStringLiteral("max_angular_speed"))) {
        double v = clampByParamMeta(QStringLiteral("max_angular_speed"), params.value(QStringLiteral("max_angular_speed")).toDouble());
        if (v > 0.0 && !qFuzzyCompare(v, m_maxAngularSpeedDeg)) { m_maxAngularSpeedDeg = v; changed = true; }
    }
    if (params.contains(QStringLiteral("max_angular_acceleration"))) {
        double v = clampByParamMeta(QStringLiteral("max_angular_acceleration"), params.value(QStringLiteral("max_angular_acceleration")).toDouble());
        if (v > 0.0 && !qFuzzyCompare(v, m_maxAngularAccelerationDeg)) { m_maxAngularAccelerationDeg = v; changed = true; }
    }
    if (params.contains(QStringLiteral("arrival_distance_tolerance"))) {
        double v = clampByParamMeta(QStringLiteral("arrival_distance_tolerance"), params.value(QStringLiteral("arrival_distance_tolerance")).toDouble());
        if (v > 0.0 && !qFuzzyCompare(v, m_arrivalDistanceTolerance)) { m_arrivalDistanceTolerance = v; changed = true; }
    }
    if (params.contains(QStringLiteral("arrival_angle_tolerance"))) {
        double v = clampByParamMeta(QStringLiteral("arrival_angle_tolerance"), params.value(QStringLiteral("arrival_angle_tolerance")).toDouble());
        if (v > 0.0 && !qFuzzyCompare(v, m_arrivalAngleToleranceDeg)) { m_arrivalAngleToleranceDeg = v; changed = true; }
    }
    if (params.contains(QStringLiteral("route_width"))) {
        double v = clampByParamMeta(QStringLiteral("route_width"), params.value(QStringLiteral("route_width")).toDouble());
        if (v > 0.0 && !qFuzzyCompare(v, m_routeWidth)) { m_routeWidth = v; changed = true; }
    }

    if (changed) {
        saveToSettings();
        qDebug() << "[MotionConfigManager] Updated motion parameters from config:"
                 << "max_speed=" << m_maxSpeed
                 << "max_acceleration=" << m_maxAcceleration
                 << "max_angular_speed=" << m_maxAngularSpeedDeg
                 << "max_angular_acceleration=" << m_maxAngularAccelerationDeg;
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
    obj[QStringLiteral("max_angular_acceleration")]   = m_maxAngularAccelerationDeg;
    obj[QStringLiteral("arrival_distance_tolerance")] = m_arrivalDistanceTolerance;
    obj[QStringLiteral("arrival_angle_tolerance")]    = m_arrivalAngleToleranceDeg;
    obj[QStringLiteral("route_width")]                = m_routeWidth;
    return obj;
}

double MotionConfigManager::clampLinearSpeed(double spd, double minVal) const
{
    double minLimit = paramMin(QStringLiteral("max_speed"), minVal);
    if (spd < minLimit) return minLimit;
    double maxLimit = paramMax(QStringLiteral("max_speed"), 1.0);
    if (maxLimit > 0.0 && spd > maxLimit) return maxLimit;
    return spd;
}

double MotionConfigManager::clampSignedLinearSpeed(double spd) const
{
    double maxLimit = paramMax(QStringLiteral("max_speed"), 1.0);
    if (maxLimit > 0.0) {
        if (spd > maxLimit)  return maxLimit;
        if (spd < -maxLimit) return -maxLimit;
    }
    return spd;
}

double MotionConfigManager::clampAngularSpeedDeg(double deg, double minVal) const
{
    double minLimit = paramMin(QStringLiteral("max_angular_speed"), minVal);
    if (deg < minLimit) return minLimit;
    double maxLimit = paramMax(QStringLiteral("max_angular_speed"), 60.0);
    if (maxLimit > 0.0 && deg > maxLimit) return maxLimit;
    return deg;
}

double MotionConfigManager::clampAngularSpeedRad(double rad, double minVal) const
{
    double minLimit = minVal;
    if (m_paramMetas.contains(QStringLiteral("max_angular_speed")) && m_paramMetas[QStringLiteral("max_angular_speed")].hasMin) {
        minLimit = m_paramMetas[QStringLiteral("max_angular_speed")].minVal * M_PI / 180.0;
    }
    if (rad < minLimit) return minLimit;
    double maxDeg = paramMax(QStringLiteral("max_angular_speed"), 60.0);
    double maxRad = maxDeg * M_PI / 180.0;
    if (maxRad > 0.0 && rad > maxRad) return maxRad;
    return rad;
}

double MotionConfigManager::clampSignedAngularSpeedDeg(double deg) const
{
    double maxLimit = paramMax(QStringLiteral("max_angular_speed"), 60.0);
    if (maxLimit > 0.0) {
        if (deg > maxLimit)  return maxLimit;
        if (deg < -maxLimit) return -maxLimit;
    }
    return deg;
}

double MotionConfigManager::clampSignedAngularSpeedRad(double rad) const
{
    double maxDeg = paramMax(QStringLiteral("max_angular_speed"), 60.0);
    double maxRad = maxDeg * M_PI / 180.0;
    if (maxRad > 0.0) {
        if (rad > maxRad)  return maxRad;
        if (rad < -maxRad) return -maxRad;
    }
    return rad;
}

double MotionConfigManager::clampAcceleration(double acc) const
{
    double minAcc = paramMin(QStringLiteral("max_acceleration"), 0.0);
    double maxAcc = paramMax(QStringLiteral("max_acceleration"), 0.5);
    if (minAcc > 0.0 && acc < minAcc) return minAcc;
    if (acc < 0.0) return 0.0;
    if (maxAcc > 0.0 && acc > maxAcc) return maxAcc;
    return acc;
}

double MotionConfigManager::clampDeceleration(double dec) const
{
    double minDec = paramMin(QStringLiteral("max_deceleration"), 0.0);
    double maxDec = paramMax(QStringLiteral("max_deceleration"), 0.5);
    if (minDec > 0.0 && dec < minDec) return minDec;
    if (dec < 0.0) return 0.0;
    if (maxDec > 0.0 && dec > maxDec) return maxDec;
    return dec;
}

double MotionConfigManager::clampAngularAccelerationDeg(double acc) const
{
    double minAcc = paramMin(QStringLiteral("max_angular_acceleration"), 0.0);
    double maxAcc = paramMax(QStringLiteral("max_angular_acceleration"), 30.0);
    if (minAcc > 0.0 && acc < minAcc) return minAcc;
    if (acc < 0.0) return 0.0;
    if (maxAcc > 0.0 && acc > maxAcc) return maxAcc;
    return acc;
}


void MotionConfigManager::updateFromMetadata(const QByteArray &rawJson)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(rawJson, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }
    QJsonObject root = doc.object();
    QJsonArray list;
    if (root.contains("data")) {
        QJsonValue dv = root.value("data");
        if (dv.isArray()) {
            list = dv.toArray();
        } else if (dv.isObject()) {
            QJsonObject dataObj = dv.toObject();
            if (dataObj.contains("data") && dataObj.value("data").isArray()) {
                list = dataObj.value("data").toArray();
            } else {
                for (const auto &k : dataObj.keys()) {
                    if (dataObj.value(k).isArray()) {
                        list = dataObj.value(k).toArray();
                        break;
                    }
                }
            }
        }
    }
    if (!list.isEmpty()) {
        updateFromMetadataArray(list);
    }
}

void MotionConfigManager::updateFromMetadataArray(const QJsonArray &metaList)
{
    bool metaUpdated = false;
    for (const auto &item : metaList) {
        QJsonObject obj = item.toObject();
        QJsonObject params = obj.value("params").toObject();
        for (auto it = params.begin(); it != params.end(); ++it) {
            QString paramName = it.key();
            QJsonObject pObj = it.value().toObject();
            MotionParamMeta m;
            if (pObj.contains("default") && !pObj.value("default").isNull()) {
                m.defaultValue = pObj.value("default").toDouble();
                m.hasDefault = true;
            }
            if (pObj.contains("min") && !pObj.value("min").isNull()) {
                m.minVal = pObj.value("min").toDouble();
                m.hasMin = true;
            }
            if (pObj.contains("max") && !pObj.value("max").isNull()) {
                m.maxVal = pObj.value("max").toDouble();
                m.hasMax = true;
            }
            m_paramMetas[paramName] = m;
            metaUpdated = true;
        }

        // 默认避障参数解析 (若接口元数据或当前参数包含对应配置)
        if (params.contains(QStringLiteral("id_stop_range_threshold"))) {
            QJsonValue stopVal = params.value(QStringLiteral("id_stop_range_threshold"));
            if (stopVal.isObject()) {
                QJsonObject stopObj = stopVal.toObject();
                if (stopObj.contains("default") && stopObj.value("default").isArray()) {
                    QJsonArray defArr = stopObj.value("default").toArray();
                    if (defArr.size() >= 2) {
                        m_defaultObsStopLength = defArr.at(0).toDouble();
                        m_defaultObsStopWidth  = defArr.at(1).toDouble();
                    }
                }
            } else if (stopVal.isArray()) {
                QJsonArray defArr = stopVal.toArray();
                if (defArr.size() >= 2) {
                    m_defaultObsStopLength = defArr.at(0).toDouble();
                    m_defaultObsStopWidth  = defArr.at(1).toDouble();
                }
            }
        }
        if (params.contains(QStringLiteral("id_deceleration_range_threshold"))) {
            QJsonValue decelVal = params.value(QStringLiteral("id_deceleration_range_threshold"));
            if (decelVal.isObject()) {
                QJsonObject decelObj = decelVal.toObject();
                if (decelObj.contains("default") && decelObj.value("default").isArray()) {
                    QJsonArray defArr = decelObj.value("default").toArray();
                    if (defArr.size() >= 2) {
                        m_defaultObsDecelLength = defArr.at(0).toDouble();
                        m_defaultObsDecelWidth  = defArr.at(1).toDouble();
                    }
                }
            } else if (decelVal.isArray()) {
                QJsonArray defArr = decelVal.toArray();
                if (defArr.size() >= 2) {
                    m_defaultObsDecelLength = defArr.at(0).toDouble();
                    m_defaultObsDecelWidth  = defArr.at(1).toDouble();
                }
            }
        }
    }

    if (!metaUpdated) return;

    QSettings s;
    bool changed = false;

    // 元数据接口 (/rbrainrobot/get_config_metadata) 提供的是最大/最小设定范围。
    // 当前全局参数的值保持来自“参数”的设定值，仅在超出元数据的 min~max 允许范围时进行裁剪约束。
    auto constrainParamByMeta = [&](const QString &key, double &currentVal) {
        if (!m_paramMetas.contains(key)) return;
        const auto &meta = m_paramMetas[key];
        if (meta.hasMin && currentVal < meta.minVal) {
            currentVal = meta.minVal;
            changed = true;
        }
        if (meta.hasMax && currentVal > meta.maxVal) {
            currentVal = meta.maxVal;
            changed = true;
        }
    };

    constrainParamByMeta(QStringLiteral("max_speed"), m_maxSpeed);
    constrainParamByMeta(QStringLiteral("max_acceleration"), m_maxAcceleration);
    constrainParamByMeta(QStringLiteral("max_deceleration"), m_maxDeceleration);
    constrainParamByMeta(QStringLiteral("max_angular_speed"), m_maxAngularSpeedDeg);
    constrainParamByMeta(QStringLiteral("max_angular_acceleration"), m_maxAngularAccelerationDeg);
    constrainParamByMeta(QStringLiteral("arrival_distance_tolerance"), m_arrivalDistanceTolerance);
    constrainParamByMeta(QStringLiteral("arrival_angle_tolerance"), m_arrivalAngleToleranceDeg);
    constrainParamByMeta(QStringLiteral("route_width"), m_routeWidth);

    qDebug() << "[MotionConfigManager] Synced metadata bounds from get_config_metadata for motion_control.yaml:"
             << "max_speed current=" << m_maxSpeed << "bounds=[" << paramMin("max_speed") << "," << paramMax("max_speed") << "]"
             << "max_acceleration current=" << m_maxAcceleration << "bounds=[" << paramMin("max_acceleration") << "," << paramMax("max_acceleration") << "]"
             << "max_angular_speed current=" << m_maxAngularSpeedDeg << "bounds=[" << paramMin("max_angular_speed") << "," << paramMax("max_angular_speed") << "]"
             << "max_angular_acceleration current=" << m_maxAngularAccelerationDeg << "bounds=[" << paramMin("max_angular_acceleration") << "," << paramMax("max_angular_acceleration") << "]";

    if (changed) {
        saveToSettings();
    }
    emit configChanged();
}

bool MotionConfigManager::hasParamMeta(const QString &paramName) const
{
    return m_paramMetas.contains(paramName);
}

MotionConfigManager::MotionParamMeta MotionConfigManager::paramMeta(const QString &paramName) const
{
    return m_paramMetas.value(paramName);
}

double MotionConfigManager::paramMin(const QString &paramName, double fallback) const
{
    if (m_paramMetas.contains(paramName) && m_paramMetas[paramName].hasMin) {
        return m_paramMetas[paramName].minVal;
    }
    return fallback;
}

double MotionConfigManager::paramMax(const QString &paramName, double fallback) const
{
    if (m_paramMetas.contains(paramName) && m_paramMetas[paramName].hasMax) {
        return m_paramMetas[paramName].maxVal;
    }
    return fallback;
}

double MotionConfigManager::paramDefault(const QString &paramName, double fallback) const
{
    if (m_paramMetas.contains(paramName) && m_paramMetas[paramName].hasDefault) {
        return m_paramMetas[paramName].defaultValue;
    }
    return fallback;
}

