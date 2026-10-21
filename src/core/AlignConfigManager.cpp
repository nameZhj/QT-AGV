/**
 * @file    AlignConfigManager.cpp
 * @module  AGVModel
 * @brief   Implementation of AGV secondary precision docking and shelf alignment parameters.
 * @deps    AlignConfigManager.h, ApiService.h, JsonUtil.h
 * @note    Refactored for AI readability; runtime behavior & public API invariant.
 */

#include "AlignConfigManager.h"
#include <QDebug>

namespace {
constexpr auto kKeyAntiAlignDepth   = "alignConfig/anti_align_depth";
constexpr auto kKeyCylinderLength   = "alignConfig/cylinder_length";
constexpr auto kParamAntiAlignDepth = "anti_align_depth";
constexpr auto kParamCylinderLength = "cylinder_length";
constexpr auto kParamParams         = "params";
}

// === [Section: Singleton & Lifecycle] ===

AlignConfigManager &AlignConfigManager::instance()
{
    static AlignConfigManager s_inst;
    return s_inst;
}

AlignConfigManager::AlignConfigManager(QObject *parent)
    : QObject(parent)
{
    loadSettings();
}

// === [Section: Settings Persistence] ===

void AlignConfigManager::loadSettings()
{
    QSettings settings;
    m_antiAlignDepth = settings.value(QString::fromLatin1(kKeyAntiAlignDepth), kDefaultAntiAlignDepth).toDouble();
    m_cylinderLength = settings.value(QString::fromLatin1(kKeyCylinderLength), kDefaultCylinderLength).toDouble();
}

void AlignConfigManager::saveSettings()
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(kKeyAntiAlignDepth), m_antiAlignDepth);
    settings.setValue(QString::fromLatin1(kKeyCylinderLength), m_cylinderLength);
}

// === [Section: Property Accessors & Mutators] ===

void AlignConfigManager::setAntiAlignDepth(double val)
{
    if (qFuzzyCompare(m_antiAlignDepth, val)) return;
    m_antiAlignDepth = val;
    saveSettings();
    emit configChanged();
}

void AlignConfigManager::setCylinderLength(double val)
{
    if (qFuzzyCompare(m_cylinderLength, val)) return;
    m_cylinderLength = val;
    saveSettings();
    emit configChanged();
}

// === [Section: JSON Synchronization] ===

void AlignConfigManager::updateFromJsonObject(const QJsonObject &params)
{
    bool changed = false;
    if (params.contains(QLatin1String(kParamAntiAlignDepth))) {
        const double d = params.value(QLatin1String(kParamAntiAlignDepth)).toDouble(m_antiAlignDepth);
        if (!qFuzzyCompare(m_antiAlignDepth, d)) {
            m_antiAlignDepth = d;
            changed = true;
        }
    }
    if (params.contains(QLatin1String(kParamCylinderLength))) {
        const double cl = params.value(QLatin1String(kParamCylinderLength)).toDouble(m_cylinderLength);
        if (!qFuzzyCompare(m_cylinderLength, cl)) {
            m_cylinderLength = cl;
            changed = true;
        }
    }
    if (!changed) return;

    saveSettings();
    emit configChanged();
}

void AlignConfigManager::updateFromConfigs(const QJsonArray &configs)
{
    for (const auto &v : configs) {
        const QJsonObject params = v.toObject().value(QLatin1String(kParamParams)).toObject();
        if (params.contains(QLatin1String(kParamCylinderLength)) || params.contains(QLatin1String(kParamAntiAlignDepth))) {
            updateFromJsonObject(params);
            return;
        }
    }
}

