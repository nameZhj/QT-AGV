/**
 * @file    MotionConfigManager.cpp
 * @layer   CoreScheduling
 * @brief   Implementation of kinematic motion parameter manager and constraint profiles.
 * @deps    MotionConfigManager.h, MotionConfigSerializer.h, MotionClampHelper.h, MotionMetadataParser.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "MotionConfigManager.h"
#include "MotionConfigSerializer.h"
#include "MotionClampHelper.h"
#include "MotionMetadataParser.h"

#include <QtGlobal>
#include <QDebug>

namespace {

MotionConfigData toData(const MotionConfigManager &m)
{
    MotionConfigData d;
    d.maxSpeed                  = m.maxSpeed();
    d.maxAcceleration           = m.maxAcceleration();
    d.maxDeceleration           = m.maxDeceleration();
    d.maxAngularSpeedDeg        = m.maxAngularSpeedDeg();
    d.maxAngularAccelerationDeg = m.maxAngularAccelerationDeg();
    d.arrivalDistanceTolerance  = m.arrivalDistanceTolerance();
    d.arrivalAngleToleranceDeg  = m.arrivalAngleToleranceDeg();
    d.routeWidth                = m.routeWidth();
    d.defaultObsStopLength      = m.defaultObsStopLength();
    d.defaultObsStopWidth       = m.defaultObsStopWidth();
    d.defaultObsDecelLength     = m.defaultObsDecelLength();
    d.defaultObsDecelWidth      = m.defaultObsDecelWidth();
    return d;
}

} // anonymous namespace

// === [Section: Singleton Access & Lifecycle] ===

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

// === [Section: Settings Persistence & JSON Serialization] ===

void MotionConfigManager::loadFromSettings()
{
    MotionConfigData d = toData(*this);
    MotionConfigSerializer::loadFromSettings(d, m_paramMetas);
    applyConfigData(d);
}

void MotionConfigManager::saveToSettings()
{
    MotionConfigSerializer::saveToSettings(toData(*this));
}

void MotionConfigManager::updateFromConfigsArray(const QJsonArray &configsList)
{
    MotionConfigData d = toData(*this);
    if (!MotionConfigSerializer::updateFromConfigsArray(d, configsList, m_paramMetas)) {
        return;
    }
    applyConfigData(d);
    saveToSettings();
    emit configChanged();
}

void MotionConfigManager::updateFromJsonObject(const QJsonObject &params)
{
    MotionConfigData d = toData(*this);
    if (!MotionConfigSerializer::updateFromJsonObject(d, params, m_paramMetas)) {
        return;
    }
    applyConfigData(d);
    saveToSettings();
    emit configChanged();
}

void MotionConfigManager::updateObstacleConfig(const QJsonObject &params)
{
    MotionConfigData d = toData(*this);
    if (!MotionConfigSerializer::updateObstacleConfig(d, params)) {
        return;
    }
    m_defaultObsStopLength  = d.defaultObsStopLength;
    m_defaultObsStopWidth   = d.defaultObsStopWidth;
    m_defaultObsDecelLength = d.defaultObsDecelLength;
    m_defaultObsDecelWidth  = d.defaultObsDecelWidth;
    saveToSettings();
    emit configChanged();
}

QJsonObject MotionConfigManager::toJsonObject() const
{
    return MotionConfigSerializer::toJsonObject(toData(*this));
}

// === [Section: Property Mutators] ===

void MotionConfigManager::setMaxSpeed(double val)
{
    const double clamped = clampByParamMeta(QStringLiteral("max_speed"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxSpeed)) {
        m_maxSpeed = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxAcceleration(double val)
{
    const double clamped = clampByParamMeta(QStringLiteral("max_acceleration"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxAcceleration)) {
        m_maxAcceleration = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxDeceleration(double val)
{
    const double clamped = clampByParamMeta(QStringLiteral("max_deceleration"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxDeceleration)) {
        m_maxDeceleration = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxAngularSpeedDeg(double val)
{
    const double clamped = clampByParamMeta(QStringLiteral("max_angular_speed"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxAngularSpeedDeg)) {
        m_maxAngularSpeedDeg = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setMaxAngularAccelerationDeg(double val)
{
    const double clamped = clampByParamMeta(QStringLiteral("max_angular_acceleration"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_maxAngularAccelerationDeg)) {
        m_maxAngularAccelerationDeg = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setArrivalDistanceTolerance(double val)
{
    const double clamped = clampByParamMeta(QStringLiteral("arrival_distance_tolerance"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_arrivalDistanceTolerance)) {
        m_arrivalDistanceTolerance = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setArrivalAngleToleranceDeg(double val)
{
    const double clamped = clampByParamMeta(QStringLiteral("arrival_angle_tolerance"), val);
    if (clamped > 0.0 && !qFuzzyCompare(clamped, m_arrivalAngleToleranceDeg)) {
        m_arrivalAngleToleranceDeg = clamped;
        saveToSettings();
        emit configChanged();
    }
}

void MotionConfigManager::setRouteWidth(double val)
{
    const double clamped = clampByParamMeta(QStringLiteral("route_width"), val);
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

// === [Section: Metadata Parsing & Synchronization] ===

void MotionConfigManager::updateFromMetadata(const QByteArray &rawJson)
{
    QJsonArray list;
    MotionMetadataParser::parseMetadataJson(rawJson, list);
    if (!list.isEmpty()) {
        updateFromMetadataArray(list);
    }
}

void MotionConfigManager::updateFromMetadataArray(const QJsonArray &metaList)
{
    const bool metaUpdated = MotionMetadataParser::parseMetadataArray(
        metaList, m_paramMetas,
        m_defaultObsStopLength, m_defaultObsStopWidth,
        m_defaultObsDecelLength, m_defaultObsDecelWidth);

    if (!metaUpdated) return;

    bool changed = false;
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

    if (changed) {
        saveToSettings();
    }
    emit configChanged();
}
